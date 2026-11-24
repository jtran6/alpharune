#pragma once
/// @file v3_model.h
/// V3 model — residual MLP on the flat state vector from extractStateFeatures.
///
/// Inputs:
///   state_features : FloatTensor (B, kStateFeatureDim)
///
/// Outputs:
///   flat_policy_logits : (B, num_action_slots) — bare logits. Callers mask
///                        to the legal actions and apply cross-entropy.
///   value              : (B, 1) — tanh in [-1, +1].
///
/// The policy head is one logit per action *description* (openspiel::kNumSlots).
/// It is not sized by NumDistinctActions() (kNumSlots * kTwinPlanes, about
/// 130 million): docs/checkpoint-breaking-2026-10.md, section 7. A legal
/// action's logit is the logit of its slot (`actionIdSlot`). Two intents that
/// share a description therefore share a logit; the twin number only tells
/// them apart in the action id.

#include "ml/feature_extractor.h"
#include "openspiel/action_vocab.h"

#include <torch/torch.h>

#include <vector>

namespace riftbound::ml {

struct V3ModelConfig {
    /// Flat observation width. Tracks kStateFeatureDim unless a checkpoint
    /// sidecar says otherwise.
    int input_dim = kStateFeatureDim;

    /// Trunk width. 512 matches the width the V3 configs request.
    int trunk_hidden = 512;

    /// Residual blocks. Each block is Linear → ReLU → Linear, plus a skip
    /// and a ReLU. No LayerNorm.
    int trunk_blocks = 6;

    int policy_hidden = 256;
    int value_hidden = 128;

    /// Policy head width: one logit per description. The training driver
    /// sets this from openspiel::kNumSlots. The forward test overrides it.
    int num_action_slots = ::riftbound::openspiel::kNumSlots;
};

class V3ModelImpl : public torch::nn::Module {
public:
    explicit V3ModelImpl(const V3ModelConfig& cfg = {});

    struct Output {
        torch::Tensor flat_policy_logits;   // (B, num_action_slots)
        torch::Tensor value;                // (B, 1)
    };

    /// `state_features` is (B, input_dim), float.
    Output forward(torch::Tensor state_features);

    const V3ModelConfig& config() const { return cfg_; }

private:
    V3ModelConfig cfg_;

    torch::nn::Linear stem_{nullptr};
    std::vector<torch::nn::Linear> block_a_;
    std::vector<torch::nn::Linear> block_b_;
    torch::nn::Linear policy_hidden_{nullptr};
    torch::nn::Linear policy_out_{nullptr};
    torch::nn::Linear value_hidden_{nullptr};
    torch::nn::Linear value_out_{nullptr};
};
TORCH_MODULE(V3Model);

}  // namespace riftbound::ml
