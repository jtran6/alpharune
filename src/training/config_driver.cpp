#include "training/config_driver.h"

#include "ml/feature_extractor.h"
#include "ml/v3_model.h"
#include "openspiel/action_vocab.h"
#include "training/replay_buffer.h"
#include "training/self_play.h"
#include "training/trainer_v3.h"

#include "open_spiel/spiel.h"

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace riftbound::training {

namespace {

struct DeckPair {
    std::string deck1;
    std::string deck2;
};

DeckPair samplePair(const ::riftbound::config::DeckPools& pools,
                     double mirror_ratio,
                     std::mt19937& rng) {
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::uniform_int_distribution<size_t> self_d(0, pools.self.size() - 1);
    const bool mirror = pools.opponent.empty() || unit(rng) < mirror_ratio;
    DeckPair pair;
    pair.deck1 = pools.self[self_d(rng)];
    if (mirror) {
        pair.deck2 = pools.self[self_d(rng)];
    } else {
        std::uniform_int_distribution<size_t> opp_d(0, pools.opponent.size() - 1);
        pair.deck2 = pools.opponent[opp_d(rng)];
    }
    return pair;
}

int gameSeed(int config_seed, int iter, int game) {
    uint32_t x = static_cast<uint32_t>(config_seed) * 1000003u ^
                 static_cast<uint32_t>(iter) * 9176u ^
                 static_cast<uint32_t>(game) * 31337u;
    x %= 1000000000u;
    if (x == 0) x = 1;
    return static_cast<int>(x);
}

std::string gameString(const std::string& registry, const DeckPair& pair, int seed) {
    // No game_mode: the default is duel, which draws each setup battlefield
    // at random. Match would search those two picks and write them into the
    // replay buffer. The registry parameter is accepted and ignored.
    return "riftbound(deck1=" + pair.deck1 +
           ",deck2=" + pair.deck2 +
           ",registry=" + registry +
           ",seed=" + std::to_string(seed) + ")";
}

::riftbound::ml::V3ModelConfig buildV3ModelConfig(
    const ::riftbound::config::ModelConfig& m) {
    ::riftbound::ml::V3ModelConfig out;
    out.input_dim = ::riftbound::ml::kStateFeatureDim;
    out.trunk_hidden = m.trunk_hidden;
    out.trunk_blocks = m.trunk_blocks;
    out.policy_hidden = m.policy_hidden;
    out.value_hidden = m.value_hidden;
    // Per-description head. Not kNumActions (slot * twin plane).
    out.num_action_slots = ::riftbound::openspiel::kNumSlots;
    return out;
}

torch::Device parseDevice(const std::string& s) {
    if (s == "cpu") return torch::kCPU;
    if (s == "cuda" || s == "cuda:0") {
        if (!torch::cuda::is_available()) {
            throw std::runtime_error(
                "training.train_device='" + s +
                "' but this LibTorch build has no CUDA");
        }
        return torch::kCUDA;
    }
    if (s.rfind("cuda:", 0) == 0) {
        if (!torch::cuda::is_available()) {
            throw std::runtime_error("CUDA device requested but LibTorch has no CUDA");
        }
        const int idx = std::stoi(s.substr(5));
        return torch::Device(torch::kCUDA, static_cast<torch::DeviceIndex>(idx));
    }
    throw std::runtime_error("unknown device string: '" + s + "'");
}

void pruneCheckpoints(const std::string& dir, int keep_last_n) {
    if (keep_last_n <= 0) return;
    namespace fs = std::filesystem;
    struct Item { int iter; fs::path path; };
    std::vector<Item> ckpts;
    for (const auto& entry : fs::directory_iterator(dir)) {
        const auto name = entry.path().filename().string();
        if (entry.path().extension() == ".pt" && name.rfind("iter_", 0) == 0) {
            ckpts.push_back({std::stoi(entry.path().stem().string().substr(5)),
                             entry.path()});
        }
    }
    std::sort(ckpts.begin(), ckpts.end(),
              [](const Item& a, const Item& b) { return a.iter < b.iter; });
    while (static_cast<int>(ckpts.size()) > keep_last_n) {
        fs::remove(ckpts.front().path);
        fs::remove(ckpts.front().path.string() + ".config.json");
        ckpts.erase(ckpts.begin());
    }
}

const char* exitTag(int reason) {
    switch (reason) {
        case 0: return "term";
        case 1: return "cap";
        case 2: return "stuck";
        default: return "?";
    }
}

}  // namespace

int runConfigTraining(const ::riftbound::config::Config& cfg) {
    if (!cfg.training.has_value()) {
        throw std::runtime_error("runConfigTraining called without a training block");
    }
    const auto& t = *cfg.training;
    if (t.model.kind != "v3") {
        throw std::runtime_error("V3 is the only model this binary trains (kind=" +
                                 t.model.kind + ")");
    }

    std::filesystem::create_directories(t.checkpoint_dir);
    auto device = parseDevice(t.train_device);

    TrainerConfig train_cfg;
    train_cfg.learning_rate = t.optimizer.lr;
    train_cfg.weight_decay = t.optimizer.weight_decay;
    train_cfg.grad_clip = t.optimizer.grad_clip;

    auto v3_cfg = buildV3ModelConfig(t.model);
    ::riftbound::ml::V3Model model(v3_cfg);
    if (t.resume_checkpoint.has_value()) {
        std::cout << "Resuming V3 from " << *t.resume_checkpoint << "\n";
        torch::load(model, *t.resume_checkpoint);
    }
    TrainerV3 trainer(model, train_cfg, device);
    std::cout << "Model architecture: V3 (residual MLP on flat features, "
              << "input_dim=" << v3_cfg.input_dim
              << "  trunk=" << v3_cfg.trunk_blocks << "x" << v3_cfg.trunk_hidden
              << "  policy_slots=" << v3_cfg.num_action_slots
              << "  device=" << t.train_device << ")\n";

    ReplayBuffer buffer(static_cast<size_t>(t.replay_capacity));
    std::mt19937 rng(static_cast<uint32_t>(cfg.seed));

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Training legend='" << t.legend
              << "'  self_pool=" << t.deck_pools.self.size()
              << "  opp_pool=" << t.deck_pools.opponent.size()
              << "  mirror_ratio=" << t.mirror_ratio << "\n";
    if (t.start_iter > 0) {
        std::cout << "Resuming at iter " << t.start_iter << "\n";
    }

    std::optional<int> bootstrap_ended_at_iter;
    const bool gate_enabled = t.bootstrap_value_loss_threshold > 0.0;
    if (gate_enabled) {
        std::cout << "Bootstrap exit gate: value_loss < "
                  << t.bootstrap_value_loss_threshold
                  << " and iter >= " << t.bootstrap_iters_min
                  << " (cap at iter " << t.bootstrap_iters << ")\n";
    }

    const auto t_total_start = std::chrono::steady_clock::now();
    int completed = 0;
    for (int iter = t.start_iter; iter < t.iterations; ++iter) {
        std::cout << "\n=== iter " << iter << " / " << t.iterations << " ===\n";

        const int bootstrap_cap = bootstrap_ended_at_iter.value_or(t.bootstrap_iters);
        const bool is_bootstrap = iter < bootstrap_cap;
        // Entropy decays for 10 iterations after bootstrap. The value head
        // is never frozen for V3.
        constexpr int kEntropyDecayIters = 10;
        double entropy_coef = 0.0;
        if (is_bootstrap) {
            entropy_coef = 0.02;
        } else {
            const int post = iter - t.bootstrap_iters;
            if (post < kEntropyDecayIters) {
                entropy_coef = 0.02 * (1.0 - static_cast<double>(post) / kEntropyDecayIters);
            }
        }
        trainer.setEntropyCoef(entropy_coef);
        trainer.setFreezeValueHead(false);

        SelfPlayConfig sp_cfg;
        sp_cfg.num_games = 1;
        sp_cfg.mcts_sims = is_bootstrap ? t.bootstrap_mcts_sims
                                        : t.self_play_agent.sims.value_or(50);
        sp_cfg.max_decisions = t.max_decisions_per_game;
        sp_cfg.uct_c = t.mcts.uct_c;
        sp_cfg.use_model = !is_bootstrap;
        sp_cfg.dirichlet_alpha = t.mcts.dirichlet_alpha;
        sp_cfg.dirichlet_fraction = t.mcts.dirichlet_fraction;
        sp_cfg.temperature = t.mcts.temperature;
        sp_cfg.temperature_drop_at_decision = t.mcts.temperature_drop_at_decision;

        if (is_bootstrap) {
            std::cout << "  [bootstrap] iter " << iter << " < " << t.bootstrap_iters
                      << " — heuristic MCTS (sims=" << sp_cfg.mcts_sims
                      << "), V3 is not in the search\n";
        }

        struct Dispatch {
            DeckPair pair;
            int seed = 1;
            std::shared_ptr<const ::open_spiel::Game> game;
        };
        std::vector<Dispatch> dispatches;
        dispatches.reserve(static_cast<size_t>(t.games_per_iter));
        for (int g = 0; g < t.games_per_iter; ++g) {
            Dispatch d;
            d.pair = samplePair(t.deck_pools, t.mirror_ratio, rng);
            d.seed = gameSeed(cfg.seed, iter, g);
            d.game = ::open_spiel::LoadGame(gameString(cfg.registry, d.pair, d.seed));
            if (!d.game) {
                throw std::runtime_error("LoadGame(\"riftbound\") returned null");
            }
            dispatches.push_back(std::move(d));
        }

        SelfPlayStats agg;
        std::mutex stats_mu;
        std::exception_ptr error;
        const auto t_sp_start = std::chrono::steady_clock::now();
        {
            boost::asio::thread_pool pool(static_cast<std::size_t>(t.num_self_play_workers));
            for (int g = 0; g < t.games_per_iter; ++g) {
                boost::asio::post(pool, [&, g] {
                    try {
                        std::mt19937 worker_rng(static_cast<uint32_t>(dispatches[g].seed));
                        const auto t_game = std::chrono::steady_clock::now();
                        auto s = runSelfPlayV3(*dispatches[g].game, model, buffer, sp_cfg,
                                                worker_rng, iter, g);
                        const auto game_secs = std::chrono::duration<double>(
                                                    std::chrono::steady_clock::now() - t_game)
                                                    .count();
                        std::lock_guard<std::mutex> lk(stats_mu);
                        agg.games_played += s.games_played;
                        agg.p1_wins += s.p1_wins;
                        agg.p2_wins += s.p2_wins;
                        agg.draws += s.draws;
                        agg.tuples_added += s.tuples_added;
                        agg.total_decisions += s.total_decisions;
                        agg.exit_terminal += s.exit_terminal;
                        agg.exit_cap += s.exit_cap;
                        agg.exit_stuck += s.exit_stuck;
                        const char* outcome = s.p1_wins ? "P1" : s.p2_wins ? "P2"
                                                              : s.draws     ? "draw"
                                                                            : "?";
                        std::cout << "  [iter " << iter << "] game " << (g + 1) << "/"
                                  << t.games_per_iter
                                  << "  dec=" << s.total_decisions
                                  << "  result=" << outcome
                                  << "  exit=" << exitTag(s.last_exit_reason)
                                  << "  iter_WLD=" << agg.p1_wins << "/" << agg.p2_wins
                                  << "/" << agg.draws
                                  << "  buf=" << buffer.size()
                                  << "  (" << game_secs << "s)\n"
                                  << std::flush;
                    } catch (...) {
                        std::lock_guard<std::mutex> lk(stats_mu);
                        if (!error) error = std::current_exception();
                    }
                });
            }
            pool.join();
        }
        if (error) std::rethrow_exception(error);

        const auto sp_secs = std::chrono::duration<double>(
                                 std::chrono::steady_clock::now() - t_sp_start)
                                 .count();
        std::cout << "  self-play: " << agg.games_played << " games, "
                  << agg.tuples_added << " tuples, W/L/D="
                  << agg.p1_wins << "/" << agg.p2_wins << "/" << agg.draws
                  << "  exit_term/cap/stuck="
                  << agg.exit_terminal << "/" << agg.exit_cap << "/" << agg.exit_stuck
                  << "  avg_dec="
                  << (agg.games_played > 0 ? agg.total_decisions / agg.games_played : 0)
                  << "  (" << sp_secs << "s)\n";

        if (buffer.size() < static_cast<size_t>(t.batch_size)) {
            std::cout << "  buffer too small (" << buffer.size()
                      << " < batch_size " << t.batch_size << ") — skipping train\n";
            ++completed;
            continue;
        }

        const auto t_tr_start = std::chrono::steady_clock::now();
        double sum_pl = 0, sum_vl = 0, sum_tl = 0;
        for (int s = 0; s < t.train_steps_per_iter; ++s) {
            auto batch = buffer.sample(static_cast<size_t>(t.batch_size), rng);
            auto [pl, vl, tl] = trainer.step(batch);
            sum_pl += pl;
            sum_vl += vl;
            sum_tl += tl;
        }
        const auto tr_secs = std::chrono::duration<double>(
                                 std::chrono::steady_clock::now() - t_tr_start)
                                 .count();
        if (t.train_steps_per_iter > 0) {
            const double n = static_cast<double>(t.train_steps_per_iter);
            std::cout << "  train:     " << t.train_steps_per_iter << " steps, "
                      << "loss policy=" << (sum_pl / n)
                      << " value=" << (sum_vl / n)
                      << " total=" << (sum_tl / n)
                      << "  (" << tr_secs << "s)\n";
        }

        if (gate_enabled && !bootstrap_ended_at_iter.has_value() && is_bootstrap &&
            t.train_steps_per_iter > 0) {
            const double last_value_loss = sum_vl / t.train_steps_per_iter;
            if (iter + 1 >= t.bootstrap_iters_min &&
                last_value_loss < t.bootstrap_value_loss_threshold) {
                bootstrap_ended_at_iter = iter + 1;
                std::cout << "  [gate] value_loss " << last_value_loss
                          << " < " << t.bootstrap_value_loss_threshold
                          << " — bootstrap ends at iter " << (iter + 1) << "\n";
            }
        }

        if (t.checkpoint_every_iter > 0 &&
            (iter % t.checkpoint_every_iter == 0 || iter == t.iterations - 1)) {
            const auto path = t.checkpoint_dir + "/iter_" + std::to_string(iter) + ".pt";
            trainer.save(path);
            std::cout << "  saved " << path << "\n";
            pruneCheckpoints(t.checkpoint_dir, t.keep_last_n);
        }
        ++completed;
    }

    const auto total_secs = std::chrono::duration<double>(
                                std::chrono::steady_clock::now() - t_total_start)
                                .count();
    std::cout << "\nDone. " << completed << " iterations in " << total_secs << "s\n";
    return completed;
}

}  // namespace riftbound::training
