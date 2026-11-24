#include "config/config.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace riftbound::config {

namespace {

namespace fs = std::filesystem;

[[noreturn]] void fail(const std::string& msg) { throw ConfigError(msg); }

int needInt(const nlohmann::json& j, const char* key, const std::string& where) {
    if (!j.contains(key) || !j[key].is_number_integer()) {
        fail(where + " missing integer \"" + key + "\"");
    }
    return j[key].get<int>();
}

double optDouble(const nlohmann::json& j, const char* key, double fallback) {
    if (!j.contains(key)) return fallback;
    if (!j[key].is_number()) fail(std::string(key) + " must be a number");
    return j[key].get<double>();
}

int optInt(const nlohmann::json& j, const char* key, int fallback) {
    if (!j.contains(key)) return fallback;
    if (!j[key].is_number_integer()) fail(std::string(key) + " must be an integer");
    return j[key].get<int>();
}

std::string resolve(const fs::path& base, const std::string& path, const char* what) {
    if (path.empty()) fail(std::string(what) + " is empty");
    fs::path p(path);
    if (p.is_relative()) p = base / p;
    p = p.lexically_normal();
    if (!fs::exists(p)) fail(std::string(what) + " not found: " + p.string());
    return p.string();
}

std::string resolveMaybeMissing(const fs::path& base, const std::string& path) {
    if (path.empty()) return {};
    fs::path p(path);
    if (p.is_relative()) p = base / p;
    return p.lexically_normal().string();
}

}  // namespace

Config parseConfig(const std::string& config_path) {
    std::ifstream in(config_path);
    if (!in) fail("cannot open config: " + config_path);
    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        fail(std::string("JSON parse: ") + e.what());
    }
    if (!j.is_object()) fail("config root must be an object");

    const std::string mode = j.value("mode", std::string());
    if (mode != "train") {
        fail("mode must be \"train\" (this build restores V3 training only)");
    }
    if (!j.contains("training") || !j["training"].is_object()) {
        fail("missing object \"training\"");
    }
    const auto& t = j["training"];

    fs::path base = fs::current_path();
    if (j.contains("root_dir") && j["root_dir"].is_string() &&
        !j["root_dir"].get<std::string>().empty()) {
        base = j["root_dir"].get<std::string>();
        if (base.is_relative()) base = fs::current_path() / base;
        if (!fs::exists(base)) fail("root_dir not found: " + base.string());
    }

    Config cfg;
    cfg.mode = Mode::Train;
    cfg.seed = optInt(j, "seed", 0);
    cfg.root_dir = base.string();
    if (j.contains("registry") && j["registry"].is_string()) {
        cfg.registry = resolveMaybeMissing(base, j["registry"].get<std::string>());
    }

    TrainingConfig train;
    train.legend = t.value("legend", std::string());
    train.iterations = needInt(t, "iterations", "training");
    train.games_per_iter = needInt(t, "games_per_iter", "training");
    train.train_steps_per_iter = needInt(t, "train_steps_per_iter", "training");
    train.batch_size = needInt(t, "batch_size", "training");
    train.replay_capacity = needInt(t, "replay_capacity", "training");
    train.mirror_ratio = optDouble(t, "mirror_ratio", 0.5);
    if (!t.contains("checkpoint_dir") || !t["checkpoint_dir"].is_string()) {
        fail("training.checkpoint_dir is required");
    }
    train.checkpoint_dir =
        resolveMaybeMissing(base, t["checkpoint_dir"].get<std::string>());
    train.checkpoint_every_iter = optInt(t, "checkpoint_every_iter", 1);
    train.keep_last_n = optInt(t, "keep_last_n", 0);
    train.num_self_play_workers = optInt(t, "num_self_play_workers", 1);
    train.max_decisions_per_game = optInt(t, "max_decisions_per_game", 600);
    train.bootstrap_iters = optInt(t, "bootstrap_iters", 0);
    train.bootstrap_mcts_sims = optInt(t, "bootstrap_mcts_sims", 40);
    train.bootstrap_iters_min = optInt(t, "bootstrap_iters_min", 0);
    train.bootstrap_value_loss_threshold =
        optDouble(t, "bootstrap_value_loss_threshold", 0.0);
    train.start_iter = optInt(t, "start_iter", 0);
    if (t.contains("resume_checkpoint") && t["resume_checkpoint"].is_string() &&
        !t["resume_checkpoint"].get<std::string>().empty()) {
        train.resume_checkpoint =
            resolve(base, t["resume_checkpoint"].get<std::string>(),
                    "training.resume_checkpoint");
    }

    if (t.contains("self_play_agent") && t["self_play_agent"].is_object()) {
        const auto& a = t["self_play_agent"];
        train.self_play_agent.kind = a.value("kind", std::string("mcts"));
        if (train.self_play_agent.kind != "mcts") {
            fail("training.self_play_agent.kind must be \"mcts\" (V3 self-play "
                 "is OpenSpiel MCTSBot)");
        }
        if (a.contains("sims")) train.self_play_agent.sims = needInt(a, "sims", "self_play_agent");
    }

    if (!t.contains("deck_pools") || !t["deck_pools"].is_object()) {
        fail("training.deck_pools is required");
    }
    const auto& pools = t["deck_pools"];
    auto takePool = [&](const char* key, std::vector<std::string>& dst) {
        if (!pools.contains(key) || !pools[key].is_array() || pools[key].empty()) {
            fail(std::string("training.deck_pools.") + key + " must be a non-empty array");
        }
        for (const auto& item : pools[key]) {
            if (!item.is_string()) fail(std::string("deck path in ") + key + " must be a string");
            dst.push_back(resolve(base, item.get<std::string>(), key));
        }
    };
    takePool("self", train.deck_pools.self);
    takePool("opponent", train.deck_pools.opponent);

    if (t.contains("devices")) {
        if (!t["devices"].is_array() || t["devices"].empty()) {
            fail("training.devices must be a non-empty array");
        }
        train.devices.clear();
        for (const auto& d : t["devices"]) {
            if (!d.is_string()) fail("training.devices entries must be strings");
            train.devices.push_back(d.get<std::string>());
        }
    }
    train.train_device = t.value("train_device", train.devices.front());

    if (t.contains("optimizer") && t["optimizer"].is_object()) {
        const auto& o = t["optimizer"];
        train.optimizer.lr = optDouble(o, "lr", train.optimizer.lr);
        train.optimizer.weight_decay =
            optDouble(o, "weight_decay", train.optimizer.weight_decay);
        train.optimizer.grad_clip = optDouble(o, "grad_clip", train.optimizer.grad_clip);
    }
    if (t.contains("mcts") && t["mcts"].is_object()) {
        const auto& m = t["mcts"];
        train.mcts.uct_c = optDouble(m, "uct_c", train.mcts.uct_c);
        train.mcts.dirichlet_alpha =
            optDouble(m, "dirichlet_alpha", train.mcts.dirichlet_alpha);
        train.mcts.dirichlet_fraction =
            optDouble(m, "dirichlet_fraction", train.mcts.dirichlet_fraction);
        train.mcts.temperature = optDouble(m, "temperature", train.mcts.temperature);
        train.mcts.temperature_drop_at_decision =
            optInt(m, "temperature_drop_at_decision",
                   train.mcts.temperature_drop_at_decision);
    }
    if (!t.contains("model") || !t["model"].is_object()) fail("training.model is required");
    const auto& model = t["model"];
    train.model.kind = model.value("kind", std::string());
    if (train.model.kind != "v3") {
        fail("training.model.kind must be \"v3\" (this build trains V3 only, got \"" +
             train.model.kind + "\")");
    }
    train.model.trunk_hidden = optInt(model, "trunk_hidden", train.model.trunk_hidden);
    train.model.trunk_blocks = optInt(model, "trunk_blocks", train.model.trunk_blocks);
    train.model.policy_hidden = optInt(model, "policy_hidden", train.model.policy_hidden);
    train.model.value_hidden = optInt(model, "value_hidden", train.model.value_hidden);

    if (train.iterations < 1) fail("training.iterations must be >= 1");
    if (train.games_per_iter < 1) fail("training.games_per_iter must be >= 1");
    if (train.batch_size < 1) fail("training.batch_size must be >= 1");
    if (train.replay_capacity < 1) fail("training.replay_capacity must be >= 1");
    if (train.num_self_play_workers < 1) fail("training.num_self_play_workers must be >= 1");
    if (train.max_decisions_per_game < 1) fail("training.max_decisions_per_game must be >= 1");
    if (train.model.trunk_hidden < 1 || train.model.trunk_blocks < 1 ||
        train.model.policy_hidden < 1 || train.model.value_hidden < 1) {
        fail("V3 widths and trunk_blocks must be >= 1");
    }
    if (train.mirror_ratio < 0.0 || train.mirror_ratio > 1.0) {
        fail("training.mirror_ratio must be in [0, 1]");
    }

    cfg.training = std::move(train);
    return cfg;
}

}  // namespace riftbound::config
