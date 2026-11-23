#include "ml/libtorch_evaluator_with_model.h"

#include "ml/feature_extractor.h"
#include "openspiel/action_vocab.h"
#include "openspiel/riftbound_game.h"
#include "openspiel/riftbound_state.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace riftbound::ml {

namespace {

torch::Tensor flatFeatures(const ::open_spiel::State& state, int perspective) {
    const auto& rb_state =
        dynamic_cast<const ::riftbound::openspiel::RiftboundState&>(state);
    const auto& rb_game =
        dynamic_cast<const ::riftbound::openspiel::RiftboundGame&>(*state.GetGame());
    const auto pid = (perspective == 0) ? ::riftbound::PlayerId::Player1
                                         : ::riftbound::PlayerId::Player2;
    auto feats = extractStateFeatures(rb_state.engineState(), pid, rb_game.cardDb());
    return torch::from_blob(feats.data(),
                             {1, static_cast<long>(feats.size())},
                             torch::TensorOptions().dtype(torch::kFloat))
        .clone();
}

V3ModelConfig configFromSidecar(const std::string& checkpoint_path) {
    const auto side = checkpoint_path + ".config.json";
    if (!std::filesystem::exists(side)) {
        throw std::runtime_error(
            "V3 checkpoint has no sidecar (expected " + side + ")");
    }
    nlohmann::json j;
    std::ifstream(side) >> j;
    if (j.value("kind", std::string("v2")) != "v3") {
        throw std::runtime_error("checkpoint sidecar is not kind \"v3\": " + side);
    }
    V3ModelConfig cfg;
    auto take = [&](const char* key, int& dst) {
        if (j.contains(key)) dst = j[key].get<int>();
    };
    take("input_dim", cfg.input_dim);
    take("trunk_hidden", cfg.trunk_hidden);
    take("trunk_blocks", cfg.trunk_blocks);
    take("policy_hidden", cfg.policy_hidden);
    take("value_hidden", cfg.value_hidden);
    take("num_action_slots", cfg.num_action_slots);
    return cfg;
}

class LibTorchEvaluatorV3 : public ::open_spiel::algorithms::Evaluator {
public:
    explicit LibTorchEvaluatorV3(V3Model model) : model_(std::move(model)) {}

    LibTorchEvaluatorV3(const std::string& checkpoint_path, const V3ModelConfig& cfg)
        : model_(cfg) {
        torch::load(model_, checkpoint_path, torch::kCPU);
        model_->eval();
    }

    std::vector<double> Evaluate(const ::open_spiel::State& state) override {
        if (state.IsTerminal()) return state.Returns();
        const double v = forward(state).value.item<double>();
        const auto cp = state.CurrentPlayer();
        if (cp == 0) return {v, -v};
        if (cp == 1) return {-v, v};
        return {0.0, 0.0};
    }

    ::open_spiel::ActionsAndProbs Prior(const ::open_spiel::State& state) override {
        auto legal = state.LegalActions();
        if (legal.empty()) return {};

        auto logits = forward(state).flat_policy_logits.view({-1});
        const int slots = model_->config().num_action_slots;
        std::vector<int64_t> index;
        index.reserve(legal.size());
        for (auto action : legal) {
            int slot = 0;
            if (action >= 0 && action <= static_cast<::open_spiel::Action>(
                                              std::numeric_limits<int>::max())) {
                slot = ::riftbound::openspiel::actionIdSlot(static_cast<int>(action));
            }
            if (slot < 0 || slot >= slots) slot = 0;
            index.push_back(slot);
        }
        auto picked = logits.index_select(
            0, torch::tensor(index, torch::TensorOptions()
                                        .dtype(torch::kLong)
                                        .device(logits.device())));
        picked = picked.to(torch::kCPU).contiguous();

        double max_logit = -std::numeric_limits<double>::infinity();
        std::vector<double> weights(legal.size(), 0.0);
        const float* data = picked.data_ptr<float>();
        for (size_t i = 0; i < legal.size(); ++i) {
            weights[i] = static_cast<double>(data[i]);
            max_logit = std::max(max_logit, weights[i]);
        }
        double sum = 0.0;
        for (double& w : weights) {
            w = std::exp(w - max_logit);
            sum += w;
        }
        if (sum <= 0.0) sum = 1.0;

        ::open_spiel::ActionsAndProbs out;
        out.reserve(legal.size());
        for (size_t i = 0; i < legal.size(); ++i) {
            out.emplace_back(legal[i], weights[i] / sum);
        }
        return out;
    }

private:
    V3ModelImpl::Output forward(const ::open_spiel::State& state) {
        // Each self-play worker builds its own evaluator around the same
        // module. One lock covers every forward of every evaluator.
        static std::mutex forward_mu;
        std::lock_guard<std::mutex> lock(forward_mu);
        auto cp = state.CurrentPlayer();
        if (cp != 0 && cp != 1) cp = 0;
        auto x = flatFeatures(state, static_cast<int>(cp));
        auto device = model_->parameters().begin()->device();
        if (x.device() != device) x = x.to(device);
        torch::NoGradGuard no_grad;
        model_->eval();
        return model_->forward(x);
    }

    V3Model model_;
};

}  // namespace

std::shared_ptr<::open_spiel::algorithms::Evaluator>
makeEvaluatorFromModel(V3Model model) {
    return std::make_shared<LibTorchEvaluatorV3>(std::move(model));
}

std::shared_ptr<::open_spiel::algorithms::Evaluator>
makeLibTorchEvaluator(const std::string& checkpoint_path) {
    if (checkpoint_path.empty()) {
        throw std::runtime_error("makeLibTorchEvaluator: empty checkpoint path");
    }
    auto cfg = configFromSidecar(checkpoint_path);
    return std::make_shared<LibTorchEvaluatorV3>(checkpoint_path, cfg);
}

}  // namespace riftbound::ml
