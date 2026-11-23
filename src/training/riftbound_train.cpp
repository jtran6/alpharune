/// @file riftbound_train.cpp
/// V3 self-play training. One config file, optional overrides for a short run.
///
///   riftbound_train --config configs/smoke_miss_fortune_v3.json --dry-run
///   riftbound_train --config configs/smoke_miss_fortune_v3.json
///       --device cpu --iterations 1 --games-per-iter 1 --workers 1
///       --bootstrap-iters 0 --mcts-sims 2 --max-decisions 2
///       --train-steps 1 --batch-size 1

#include "config/config.h"
#include "training/config_driver.h"

#include <boost/program_options.hpp>

#include <cstdio>
#include <iostream>
#include <string>

namespace po = boost::program_options;

namespace {

void printSummary(const riftbound::config::Config& cfg) {
    const auto& t = *cfg.training;
    std::cout << "Config validated.\n";
    std::cout << "  mode                        : train\n";
    std::cout << "  seed                        : " << cfg.seed << "\n";
    std::cout << "  root_dir                    : " << cfg.root_dir.value_or("(cwd)") << "\n";
    std::cout << "  registry                    : "
              << (cfg.registry.empty() ? "(ignored)" : cfg.registry) << "\n";
    std::cout << "  training.legend             : " << t.legend << "\n";
    std::cout << "  training.model.kind         : " << t.model.kind << "\n";
    std::cout << "  training.iterations         : " << t.iterations << "\n";
    std::cout << "  training.games_per_iter     : " << t.games_per_iter << "\n";
    std::cout << "  training.train_steps        : " << t.train_steps_per_iter << "\n";
    std::cout << "  training.batch_size         : " << t.batch_size << "\n";
    std::cout << "  training.workers            : " << t.num_self_play_workers << "\n";
    std::cout << "  training.bootstrap_iters    : " << t.bootstrap_iters << "\n";
    std::cout << "  training.max_decisions      : " << t.max_decisions_per_game << "\n";
    std::cout << "  training.mcts_sims          : "
              << t.self_play_agent.sims.value_or(50) << "\n";
    std::cout << "  training.train_device       : " << t.train_device << "\n";
    std::cout << "  training.checkpoint_dir     : " << t.checkpoint_dir << "\n";
    std::cout << "  training.deck_pools.self    : " << t.deck_pools.self.size() << "\n";
    std::cout << "  training.deck_pools.opponent: " << t.deck_pools.opponent.size() << "\n";
    std::cout << "  training.model.trunk        : " << t.model.trunk_blocks << "x"
              << t.model.trunk_hidden << "\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::setvbuf(stderr, nullptr, _IOLBF, 0);

    po::options_description desc("Riftbound V3 trainer");
    desc.add_options()
        ("help,h", "Show help")
        ("config,c", po::value<std::string>()->required(), "Path to a training JSON config")
        ("dry-run,d", po::bool_switch()->default_value(false),
         "Parse the config and exit")
        ("iterations", po::value<int>(), "Override training.iterations")
        ("games-per-iter", po::value<int>(), "Override training.games_per_iter")
        ("train-steps", po::value<int>(), "Override training.train_steps_per_iter")
        ("batch-size", po::value<int>(), "Override training.batch_size")
        ("max-decisions", po::value<int>(), "Override training.max_decisions_per_game")
        ("mcts-sims", po::value<int>(), "Override self_play_agent.sims")
        ("bootstrap-iters", po::value<int>(), "Override training.bootstrap_iters")
        ("workers", po::value<int>(), "Override training.num_self_play_workers")
        ("device", po::value<std::string>(), "Override training.train_device (cpu or cuda:N)");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }
        po::notify(vm);
    } catch (const std::exception& e) {
        std::cerr << "Argument error: " << e.what() << "\n\n" << desc << "\n";
        return 2;
    }

    riftbound::config::Config cfg;
    try {
        cfg = riftbound::config::parseConfig(vm["config"].as<std::string>());
    } catch (const riftbound::config::ConfigError& e) {
        std::cerr << "Config error: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error parsing config: " << e.what() << "\n";
        return 1;
    }

    auto& t = *cfg.training;
    if (vm.count("iterations")) t.iterations = vm["iterations"].as<int>();
    if (vm.count("games-per-iter")) t.games_per_iter = vm["games-per-iter"].as<int>();
    if (vm.count("train-steps")) t.train_steps_per_iter = vm["train-steps"].as<int>();
    if (vm.count("batch-size")) t.batch_size = vm["batch-size"].as<int>();
    if (vm.count("max-decisions")) t.max_decisions_per_game = vm["max-decisions"].as<int>();
    if (vm.count("mcts-sims")) t.self_play_agent.sims = vm["mcts-sims"].as<int>();
    if (vm.count("bootstrap-iters")) t.bootstrap_iters = vm["bootstrap-iters"].as<int>();
    if (vm.count("workers")) t.num_self_play_workers = vm["workers"].as<int>();
    if (vm.count("device")) t.train_device = vm["device"].as<std::string>();

    if (vm["dry-run"].as<bool>()) {
        printSummary(cfg);
        return 0;
    }

    try {
        riftbound::training::runConfigTraining(cfg);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Training error: " << e.what() << "\n";
        return 1;
    }
}
