#pragma once

/// Search-only prune for empty friendly-harm lines.
///
/// The rules engine still generates those plays. MCTS and ISMCTS call
/// searchCandidates and score only what it returns, so a line that deals
/// damage, reduces Might, or bounces a friendly unit is not taken when that
/// result does not kill the unit and does not enable a printed death trigger
/// or a kill that pays a cost. A friendly target the printed card wants (it
/// says "friendly" / "unit you control", or must_be_friendly) stays, and so
/// does a banish that plays the unit again. A draw or a Predict that still
/// happens on a friendly debuff stays when no enemy is a legal target.
/// Playing a unit stays: the body is the line, even when an optional or
/// enemy-only line of text parses as harm. When the same harmful spell also
/// has an enemy unit as a legal target, the friendly candidate is dropped
/// unless the friendly result is one the card wants. A dropped list is not
/// put back when nothing else remains.

#include "core/intent.h"

#include <vector>

namespace riftbound {

class GameState;
class CardRegistry;

std::vector<Intent> searchCandidates(const GameState& state,
                                     const std::vector<Intent>& legal,
                                     const CardRegistry& cards);

}  // namespace riftbound
