#include "training/self_play.h"

#include "ml/feature_extractor.h"
#include "ml/libtorch_evaluator_with_model.h"
#include "openspiel/riftbound_game.h"
#include "openspiel/riftbound_state.h"
#include "training/heuristic_evaluator.h"
#include "training/replay_buffer.h"

#include "open_spiel/algorithms/mcts.h"
#include "open_spiel/spiel.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

namespace riftbound::training {

namespace {

enum class ExitReason : int { Terminal = 0, DecisionCap = 1, LegalEmpty = 2 };

struct PendingTuple {
    std::vector<float> flat_state;
    std::vector<int32_t> legal_actions;
    std::vector<float> policy_over_legal;
    int acting_player = 0;
};

struct GameResult {
    int decisions = 0;
    std::vector<double> returns;
    ExitReason exit_reason = ExitReason::Terminal;
};

GameResult playOneGame(const ::open_spiel::Game& game,
                        std::shared_ptr<::open_spiel::algorithms::Evaluator> evaluator,
                        const SelfPlayConfig& cfg,
                        std::mt19937& rng,
                        ReplayBuffer& buffer) {
    auto state = game.NewInitialState();
    // PUCT so the evaluator prior (V3, or uniform while bootstrapping)
    // enters selection. Dirichlet noise is mixed into the root prior.
    ::open_spiel::algorithms::MCTSBot bot(
        game, evaluator,
        cfg.uct_c, cfg.mcts_sims,
        /*max_memory_mb=*/256, /*solve=*/false,
        /*seed=*/static_cast<int>(rng()),
        /*verbose=*/false,
        ::open_spiel::algorithms::ChildSelectionPolicy::PUCT,
        /*dirichlet_alpha=*/cfg.dirichlet_alpha,
        /*dirichlet_epsilon=*/cfg.dirichlet_fraction);

    std::vector<PendingTuple> pending;
    pending.reserve(static_cast<size_t>(std::max(cfg.max_decisions, 0)));

    const auto& rb_game =
        dynamic_cast<const ::riftbound::openspiel::RiftboundGame&>(game);

    int decisions = 0;
    ExitReason exit_reason = ExitReason::Terminal;
    while (true) {
        if (state->IsTerminal()) {
            exit_reason = ExitReason::Terminal;
            break;
        }
        if (decisions >= cfg.max_decisions) {
            exit_reason = ExitReason::DecisionCap;
            break;
        }
        auto legal = state->LegalActions();
        if (legal.empty()) {
            exit_reason = ExitReason::LegalEmpty;
            break;
        }
        const auto cp = state->CurrentPlayer();
        if (cp != 0 && cp != 1) {
            exit_reason = ExitReason::LegalEmpty;
            break;
        }

        PendingTuple tuple;
        tuple.acting_player = static_cast<int>(cp);
        const auto& rb_state =
            dynamic_cast<const ::riftbound::openspiel::RiftboundState&>(*state);
        const auto pid = (cp == 0) ? ::riftbound::PlayerId::Player1
                                    : ::riftbound::PlayerId::Player2;
        tuple.flat_state = ::riftbound::ml::extractStateFeatures(
            rb_state.engineState(), pid, rb_game.cardDb());

        auto root = bot.MCTSearch(*state);
        int64_t total_visits = 0;
        for (const auto& child : root->children) total_visits += child.explore_count;

        tuple.legal_actions.reserve(legal.size());
        tuple.policy_over_legal.reserve(legal.size());
        for (auto action : legal) {
            tuple.legal_actions.push_back(static_cast<int32_t>(action));
            float p = 0.0f;
            for (const auto& child : root->children) {
                if (child.action == action) {
                    p = (total_visits > 0)
                            ? static_cast<float>(child.explore_count) /
                                  static_cast<float>(total_visits)
                            : 0.0f;
                    break;
                }
            }
            tuple.policy_over_legal.push_back(p);
        }
        pending.push_back(std::move(tuple));

        const double tau = (decisions < cfg.temperature_drop_at_decision)
                               ? cfg.temperature
                               : 0.0;
        // One simulation evaluates the root and does not expand it, so the
        // child list can be empty. Fall back to the first legal action.
        ::open_spiel::Action act = legal.front();
        if (!root->children.empty() &&
            (tau <= 0.0 || total_visits <= 0)) {
            act = root->BestChild().action;
        } else if (tau > 0.0 && total_visits > 0 && !root->children.empty()) {
            std::vector<double> weights;
            weights.reserve(root->children.size());
            double sum = 0.0;
            for (const auto& child : root->children) {
                const double w = std::pow(static_cast<double>(child.explore_count),
                                           1.0 / tau);
                weights.push_back(w);
                sum += w;
            }
            if (sum > 0.0) {
                std::uniform_real_distribution<double> u(0.0, sum);
                double pick = u(rng);
                double cum = 0.0;
                act = root->children.front().action;
                for (size_t i = 0; i < root->children.size(); ++i) {
                    cum += weights[i];
                    if (pick <= cum) {
                        act = root->children[i].action;
                        break;
                    }
                }
            }
        }
        state->ApplyAction(act);
        ++decisions;
    }

    GameResult out;
    out.decisions = decisions;
    out.returns = state->Returns();
    out.exit_reason = exit_reason;

    for (auto& tuple : pending) {
        ReplaySample sample;
        sample.flat_state = std::move(tuple.flat_state);
        sample.legal_actions = std::move(tuple.legal_actions);
        sample.policy_over_legal = std::move(tuple.policy_over_legal);
        const int seat = tuple.acting_player;
        sample.value = (seat >= 0 && static_cast<size_t>(seat) < out.returns.size())
                           ? static_cast<float>(out.returns[static_cast<size_t>(seat)])
                           : 0.0f;
        buffer.push(std::move(sample));
    }
    return out;
}

}  // namespace

SelfPlayStats runSelfPlayV3(const ::open_spiel::Game& game,
                             ::riftbound::ml::V3Model model,
                             ReplayBuffer& buffer,
                             const SelfPlayConfig& cfg,
                             std::mt19937& rng,
                             int /*iter*/,
                             int /*game_idx*/) {
    std::shared_ptr<::open_spiel::algorithms::Evaluator> evaluator;
    if (cfg.use_model) {
        evaluator = ::riftbound::ml::makeEvaluatorFromModel(model);
    } else {
        evaluator = std::make_shared<HeuristicValueEvaluator>();
    }

    SelfPlayStats stats;
    const int64_t prior_pushes = buffer.total_pushes();
    const auto t_start = std::chrono::steady_clock::now();
    for (int g = 0; g < cfg.num_games; ++g) {
        auto result = playOneGame(game, evaluator, cfg, rng, buffer);
        ++stats.games_played;
        stats.total_decisions += result.decisions;
        if (result.returns.size() >= 2) {
            if (result.returns[0] > 0.0) ++stats.p1_wins;
            else if (result.returns[1] > 0.0) ++stats.p2_wins;
            else ++stats.draws;
        }
        switch (result.exit_reason) {
            case ExitReason::Terminal: ++stats.exit_terminal; break;
            case ExitReason::DecisionCap: ++stats.exit_cap; break;
            case ExitReason::LegalEmpty: ++stats.exit_stuck; break;
        }
        stats.last_exit_reason = static_cast<int>(result.exit_reason);
        if (cfg.verbose) {
            const auto elapsed = std::chrono::duration<double>(
                                     std::chrono::steady_clock::now() - t_start)
                                     .count();
            std::cerr << "  self-play[V3] game " << (g + 1) << "/" << cfg.num_games
                      << "  decisions=" << result.decisions
                      << "  W/L/D=" << stats.p1_wins << "/" << stats.p2_wins
                      << "/" << stats.draws
                      << "  buffer=" << buffer.size()
                      << "  elapsed=" << static_cast<int>(elapsed) << "s\n";
        }
    }
    stats.tuples_added = buffer.total_pushes() - prior_pushes;
    return stats;
}

}  // namespace riftbound::training
