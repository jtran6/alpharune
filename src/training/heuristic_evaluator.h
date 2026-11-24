#pragma once
/// @file heuristic_evaluator.h
/// MCTS value for the bootstrap iterations, before V3 is in the search.
/// Score difference over the effective victory score, clamped to [-1, +1],
/// and a uniform prior so the visit counts are not pulled by action-id order.

#include "open_spiel/algorithms/mcts.h"

#include <vector>

namespace riftbound::training {

class HeuristicValueEvaluator : public ::open_spiel::algorithms::Evaluator {
public:
    std::vector<double> Evaluate(const ::open_spiel::State& state) override;
    ::open_spiel::ActionsAndProbs Prior(const ::open_spiel::State& state) override;
};

}  // namespace riftbound::training
