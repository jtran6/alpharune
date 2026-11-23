#pragma once
/// @file replay_buffer.h
/// Rolling buffer of self-play decisions for the V3 trainer.
///
/// One sample is one decision: the acting player's flat state vector,
/// the legal action ids, the MCTS visit distribution over those actions,
/// and the game outcome from that player's seat (+1 / -1 / 0).

#include <cstdint>
#include <mutex>
#include <random>
#include <vector>

namespace riftbound::training {

struct ReplaySample {
    std::vector<float> flat_state;
    std::vector<int32_t> legal_actions;
    /// Visit distribution, aligned with `legal_actions`. Sums to 1 when
    /// every legal action was a child of the MCTS root.
    std::vector<float> policy_over_legal;
    float value = 0.0f;
};

class ReplayBuffer {
public:
    explicit ReplayBuffer(size_t capacity);

    void push(ReplaySample sample);
    std::vector<ReplaySample> sample(size_t k, std::mt19937& rng) const;

    size_t size() const;
    /// Push count, including slots overwritten once the ring is full.
    int64_t total_pushes() const;
    size_t capacity() const { return capacity_; }

private:
    mutable std::mutex mu_;
    std::vector<ReplaySample> buf_;
    size_t capacity_;
    size_t next_idx_ = 0;
    int64_t total_pushes_ = 0;
};

}  // namespace riftbound::training
