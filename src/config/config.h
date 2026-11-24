#pragma once
/// @file config.h
/// Training config for the V3 self-play loop. Parsed from the JSON files
/// under configs/. Play mode is not part of this restoration.

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace riftbound::config {

enum class Mode { Train };

struct OptimizerConfig {
    double lr = 0.001;
    double weight_decay = 0.0;
    double grad_clip = 0.0;
};

struct MctsConfig {
    double uct_c = 1.4;
    double dirichlet_alpha = 0.3;
    double dirichlet_fraction = 0.25;
    double temperature = 1.0;
    int temperature_drop_at_decision = 30;
};

struct ModelConfig {
    std::string kind = "v3";
    int trunk_hidden = 512;
    int trunk_blocks = 6;
    int policy_hidden = 256;
    int value_hidden = 128;
};

struct DeckPools {
    std::vector<std::string> self;
    std::vector<std::string> opponent;
};

struct AgentSpec {
    std::string kind = "mcts";
    std::optional<int> sims;
};

struct TrainingConfig {
    std::string legend;
    int iterations = 1;
    int games_per_iter = 1;
    int train_steps_per_iter = 0;
    int batch_size = 1;
    int replay_capacity = 1;
    double mirror_ratio = 0.5;
    std::string checkpoint_dir;
    int checkpoint_every_iter = 1;
    int keep_last_n = 0;
    int num_self_play_workers = 1;
    int max_decisions_per_game = 600;
    int bootstrap_iters = 0;
    int bootstrap_mcts_sims = 40;
    int bootstrap_iters_min = 0;
    double bootstrap_value_loss_threshold = 0.0;
    int start_iter = 0;
    std::optional<std::string> resume_checkpoint;
    AgentSpec self_play_agent;
    DeckPools deck_pools;
    std::vector<std::string> devices = {"cpu"};
    std::string train_device = "cpu";
    OptimizerConfig optimizer;
    MctsConfig mcts;
    ModelConfig model;
};

struct Config {
    Mode mode = Mode::Train;
    int seed = 0;
    std::optional<std::string> root_dir;
    /// Accepted and ignored. Card data is compiled into the engine.
    std::string registry;
    std::optional<TrainingConfig> training;
};

class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

Config parseConfig(const std::string& config_path);

}  // namespace riftbound::config
