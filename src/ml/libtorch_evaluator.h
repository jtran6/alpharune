#pragma once
/// @file libtorch_evaluator.h
/// OpenSpiel MCTS evaluator backed by an in-memory V3 model, or by a
/// checkpoint written by TrainerV3 (weights plus a `.config.json` sidecar).

#include "open_spiel/algorithms/mcts.h"

#include <memory>
#include <string>

namespace riftbound::ml {

std::shared_ptr<::open_spiel::algorithms::Evaluator>
makeLibTorchEvaluator(const std::string& checkpoint_path);

}  // namespace riftbound::ml
