#pragma once
/// @file libtorch_evaluator_with_model.h
/// Evaluator that keeps the caller's V3Model, so a later AdamW step is
/// visible to the next self-play game without a checkpoint round-trip.

#include "ml/libtorch_evaluator.h"
#include "ml/v3_model.h"

namespace riftbound::ml {

std::shared_ptr<::open_spiel::algorithms::Evaluator>
makeEvaluatorFromModel(V3Model model);

}  // namespace riftbound::ml
