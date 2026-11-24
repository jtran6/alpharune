#pragma once
/// @file trainer_config.h
/// Optimizer settings shared by the V3 trainer. This is the piece of the
/// old V2 Trainer header that V3 still uses.

namespace riftbound::training {

struct TrainerConfig {
    double learning_rate = 1e-3;
    double weight_decay  = 1e-4;
    double grad_clip     = 1.0;
    /// Added as `-entropy_coef * H(policy)` so a peaked policy is penalised.
    /// 0 leaves the loss as policy cross-entropy plus value MSE.
    double entropy_coef = 0.0;
    /// When set, value MSE is reported and not back-propagated. The V3
    /// loop leaves this false: the value head trains from the first step.
    bool freeze_value_head = false;
};

}  // namespace riftbound::training
