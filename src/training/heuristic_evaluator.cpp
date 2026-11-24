#include "training/heuristic_evaluator.h"

#include "openspiel/riftbound_state.h"

#include <algorithm>

namespace riftbound::training {

std::vector<double> HeuristicValueEvaluator::Evaluate(const ::open_spiel::State& state) {
    if (state.IsTerminal()) return state.Returns();

    const auto& rb = dynamic_cast<const ::riftbound::openspiel::RiftboundState&>(state);
    const auto& gs = rb.engineState();
    const double victory = std::max(1, gs.effectiveVictoryScore());
    double diff = static_cast<double>(gs.player(::riftbound::PlayerId::Player1).score -
                                       gs.player(::riftbound::PlayerId::Player2).score) /
                  victory;
    diff = std::clamp(diff, -1.0, 1.0);
    return {diff, -diff};
}

::open_spiel::ActionsAndProbs HeuristicValueEvaluator::Prior(
    const ::open_spiel::State& state) {
    auto legal = state.LegalActions();
    ::open_spiel::ActionsAndProbs out;
    out.reserve(legal.size());
    if (legal.empty()) return out;
    const double p = 1.0 / static_cast<double>(legal.size());
    for (auto action : legal) out.emplace_back(action, p);
    return out;
}

}  // namespace riftbound::training
