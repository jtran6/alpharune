#pragma once
/// @file self_play.h
/// Self-play for V3. OpenSpiel MCTSBot searches each decision. The visit
/// counts on the root's children are the policy target; the game's return
/// from the acting seat is the value target. Both go into the replay buffer.
///
/// Child selection is PUCT. OpenSpiel's UCT formula does not read the
/// evaluator prior, so a UCT search would train V3 on visits the net did
/// not influence. PUCT is the AlphaZero selection rule: the V3 prior is
/// part of the search that produces the visit counts.

#include "ml/v3_model.h"

#include <cstdint>
#include <random>

namespace open_spiel { class Game; }

namespace riftbound::training {

class ReplayBuffer;

struct SelfPlayConfig {
    int num_games = 1;
    int mcts_sims = 50;
    int max_decisions = 600;
    double uct_c = 1.4;
    double dirichlet_alpha = 0.3;
    double dirichlet_fraction = 0.25;
    double temperature = 1.0;
    int temperature_drop_at_decision = 30;
    /// False: heuristic value, uniform prior (bootstrap). True: V3.
    bool use_model = true;
    bool verbose = false;
};

struct SelfPlayStats {
    int games_played = 0;
    int p1_wins = 0;
    int p2_wins = 0;
    int draws = 0;
    int64_t tuples_added = 0;
    int64_t total_decisions = 0;
    int exit_terminal = 0;
    int exit_cap = 0;
    int exit_stuck = 0;
    /// 0 terminal, 1 decision cap, 2 no legal actions. The last game.
    int last_exit_reason = 0;
};

SelfPlayStats runSelfPlayV3(const ::open_spiel::Game& game,
                             ::riftbound::ml::V3Model model,
                             ReplayBuffer& buffer,
                             const SelfPlayConfig& cfg,
                             std::mt19937& rng,
                             int iter = -1,
                             int game_idx = -1);

}  // namespace riftbound::training
