#include "ml/v3_model.h"

#include <string>

namespace riftbound::ml {

V3ModelImpl::V3ModelImpl(const V3ModelConfig& cfg)
    : cfg_(cfg),
      stem_(torch::nn::LinearOptions(cfg.input_dim, cfg.trunk_hidden)),
      policy_hidden_(torch::nn::LinearOptions(cfg.trunk_hidden, cfg.policy_hidden)),
      policy_out_(torch::nn::LinearOptions(cfg.policy_hidden, cfg.num_action_slots)),
      value_hidden_(torch::nn::LinearOptions(cfg.trunk_hidden, cfg.value_hidden)),
      value_out_(torch::nn::LinearOptions(cfg.value_hidden, 1)) {
    register_module("stem", stem_);

    // Skip around the second linear, then ReLU. No LayerNorm: that was the
    // V2 trunk's failure mode, and V3 does not repeat it.
    block_a_.reserve(cfg.trunk_blocks);
    block_b_.reserve(cfg.trunk_blocks);
    for (int i = 0; i < cfg.trunk_blocks; ++i) {
        auto a = torch::nn::Linear(
            torch::nn::LinearOptions(cfg.trunk_hidden, cfg.trunk_hidden));
        auto b = torch::nn::Linear(
            torch::nn::LinearOptions(cfg.trunk_hidden, cfg.trunk_hidden));
        register_module("block_a_" + std::to_string(i), a);
        register_module("block_b_" + std::to_string(i), b);
        block_a_.push_back(a);
        block_b_.push_back(b);
    }

    register_module("policy_hidden", policy_hidden_);
    register_module("policy_out", policy_out_);
    register_module("value_hidden", value_hidden_);
    register_module("value_out", value_out_);
}

V3ModelImpl::Output V3ModelImpl::forward(torch::Tensor state_features) {
    auto x = torch::relu(stem_->forward(state_features));

    for (size_t i = 0; i < block_a_.size(); ++i) {
        auto h = torch::relu(block_a_[i]->forward(x));
        h = block_b_[i]->forward(h);
        x = torch::relu(x + h);
    }

    auto p_hidden = torch::relu(policy_hidden_->forward(x));
    auto flat_policy_logits = policy_out_->forward(p_hidden);

    auto v_hidden = torch::relu(value_hidden_->forward(x));
    auto value = torch::tanh(value_out_->forward(v_hidden));

    return Output{
        /*.flat_policy_logits =*/ flat_policy_logits,
        /*.value              =*/ value,
    };
}

}  // namespace riftbound::ml
