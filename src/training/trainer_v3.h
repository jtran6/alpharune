#pragma once
/// @file trainer_v3.h
/// One AdamW step on V3: policy cross-entropy against MCTS visit counts,
/// value MSE, optional entropy bonus.
///
/// Visit counts are stored on action ids. The head has one logit per
/// description, so the logit of an action is the logit of its slot
/// (`actionIdSlot`). The softmax is over the decision's legal actions,
/// not over the whole head: illegal descriptions stay out of the loss,
/// and two legal actions that share a slot share a logit.

#include "ml/v3_model.h"
#include "training/replay_buffer.h"
#include "training/trainer_config.h"

#include <torch/torch.h>

#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace riftbound::training {

class TrainerV3 {
public:
    TrainerV3(::riftbound::ml::V3Model model,
              const TrainerConfig& cfg,
              torch::Device device = torch::kCPU);

    /// Returns (policy_loss, value_loss, total_loss).
    std::tuple<float, float, float> step(const std::vector<ReplaySample>& batch);

    void save(const std::string& path);
    void load(const std::string& path);

    ::riftbound::ml::V3Model& model() { return model_; }

    void setEntropyCoef(double c) { cfg_.entropy_coef = c; }
    void setFreezeValueHead(bool freeze) { cfg_.freeze_value_head = freeze; }

private:
    ::riftbound::ml::V3Model model_;
    TrainerConfig cfg_;
    torch::Device device_;
    std::unique_ptr<torch::optim::AdamW> optimizer_;
};

}  // namespace riftbound::training
