#include "training/trainer_v3.h"

#include "openspiel/action_vocab.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <limits>

namespace riftbound::training {

namespace {

torch::Tensor stackFlatStates(const std::vector<ReplaySample>& batch, int input_dim) {
    const int B = static_cast<int>(batch.size());
    auto out = torch::empty({B, input_dim}, torch::TensorOptions().dtype(torch::kFloat));
    auto out_acc = out.accessor<float, 2>();
    for (int i = 0; i < B; ++i) {
        const auto& fs = batch[i].flat_state;
        const int n = std::min<int>(input_dim, static_cast<int>(fs.size()));
        for (int j = 0; j < n; ++j) out_acc[i][j] = fs[j];
        for (int j = n; j < input_dim; ++j) out_acc[i][j] = 0.0f;
    }
    return out;
}

}  // namespace

TrainerV3::TrainerV3(::riftbound::ml::V3Model model,
                      const TrainerConfig& cfg,
                      torch::Device device)
    : model_(std::move(model)), cfg_(cfg), device_(device) {
    model_->to(device_);
    optimizer_ = std::make_unique<torch::optim::AdamW>(
        model_->parameters(),
        torch::optim::AdamWOptions(cfg_.learning_rate)
            .weight_decay(cfg_.weight_decay));
}

std::tuple<float, float, float>
TrainerV3::step(const std::vector<ReplaySample>& batch) {
    if (batch.empty()) return {0.0f, 0.0f, 0.0f};

    const auto& cfg = model_->config();
    const int B = static_cast<int>(batch.size());
    const int slots = cfg.num_action_slots;

    auto x = stackFlatStates(batch, cfg.input_dim).to(device_);

    int max_legal = 1;
    for (const auto& sample : batch) {
        max_legal = std::max(max_legal, static_cast<int>(sample.legal_actions.size()));
    }

    // Logit of each legal action, gathered from its description. A padded
    // column is -inf so it drops out of the softmax. A row with no legal
    // action keeps a single finite logit and a zero target, so the row
    // adds nothing and log_softmax stays defined.
    auto index = torch::zeros({B, max_legal}, torch::TensorOptions().dtype(torch::kLong));
    auto target = torch::zeros({B, max_legal}, torch::TensorOptions().dtype(torch::kFloat));
    auto valid = torch::zeros({B, max_legal}, torch::TensorOptions().dtype(torch::kBool));
    {
        auto ix = index.accessor<int64_t, 2>();
        auto tg = target.accessor<float, 2>();
        auto ok = valid.accessor<bool, 2>();
        for (int i = 0; i < B; ++i) {
            const auto& legal = batch[i].legal_actions;
            const auto& policy = batch[i].policy_over_legal;
            bool any = false;
            for (size_t j = 0; j < legal.size(); ++j) {
                const int32_t action = legal[j];
                if (action < 0) continue;
                const int slot = ::riftbound::openspiel::actionIdSlot(action);
                if (slot < 0 || slot >= slots) continue;
                ix[i][static_cast<int>(j)] = slot;
                ok[i][static_cast<int>(j)] = true;
                if (j < policy.size()) tg[i][static_cast<int>(j)] = policy[j];
                any = true;
            }
            if (!any) ok[i][0] = true;
        }
    }

    auto value_target = torch::empty({B, 1}, torch::TensorOptions().dtype(torch::kFloat));
    {
        float* vd = value_target.data_ptr<float>();
        for (int i = 0; i < B; ++i) vd[i] = batch[i].value;
    }

    index = index.to(device_);
    target = target.to(device_);
    valid = valid.to(device_);
    value_target = value_target.to(device_);

    model_->train();
    auto out = model_->forward(x);

    auto gathered = out.flat_policy_logits.gather(/*dim=*/1, index);
    auto neg_inf = torch::full_like(gathered, -std::numeric_limits<float>::infinity());
    auto masked = torch::where(valid, gathered, neg_inf);

    auto log_probs = torch::log_softmax(masked, /*dim=*/1);
    auto contributions = torch::where(target > 0, target * log_probs,
                                       torch::zeros_like(log_probs));
    auto policy_loss = -contributions.sum(/*dim=*/1).mean();

    torch::Tensor entropy_term;
    if (cfg_.entropy_coef != 0.0) {
        auto safe_log = log_probs.masked_fill(~valid, 0.0);
        auto probs = torch::exp(log_probs).masked_fill(~valid, 0.0);
        auto model_entropy = -(probs * safe_log).sum(/*dim=*/1).mean();
        entropy_term = -static_cast<float>(cfg_.entropy_coef) * model_entropy;
    } else {
        entropy_term = torch::zeros({}, masked.options());
    }

    auto value_loss = torch::mse_loss(out.value, value_target);
    auto value_term = cfg_.freeze_value_head ? value_loss.detach() : value_loss;
    auto total_loss = policy_loss + entropy_term + value_term;

    optimizer_->zero_grad();
    total_loss.backward();
    if (cfg_.grad_clip > 0.0) {
        torch::nn::utils::clip_grad_norm_(model_->parameters(), cfg_.grad_clip);
    }
    optimizer_->step();

    return {policy_loss.item<float>(),
            value_loss.item<float>(),
            total_loss.item<float>()};
}

void TrainerV3::save(const std::string& path) {
    torch::save(model_, path);
    nlohmann::json j;
    j["kind"] = "v3";
    const auto& c = model_->config();
    j["input_dim"] = c.input_dim;
    j["trunk_hidden"] = c.trunk_hidden;
    j["trunk_blocks"] = c.trunk_blocks;
    j["policy_hidden"] = c.policy_hidden;
    j["value_hidden"] = c.value_hidden;
    j["num_action_slots"] = c.num_action_slots;
    std::ofstream(path + ".config.json") << j.dump(2);
}

void TrainerV3::load(const std::string& path) {
    torch::load(model_, path);
}

}  // namespace riftbound::training
