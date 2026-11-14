#pragma once

/// Search-only prune for empty friendly-harm lines.
///
/// The rules engine still generates those plays. MCTS and ISMCTS call
/// searchCandidates and score only what it returns, so a line that deals
/// damage, reduces Might, bounces, or banishes a friendly unit is not
/// taken when that result does not kill the unit and does not enable a
/// printed death trigger or a kill that pays a cost. A friendly target the
/// printed card wants (it says "friendly" / "unit you control", or
/// must_be_friendly) stays. When the same harmful spell also has an enemy
/// unit as a legal target, the friendly candidate is dropped unless the
/// friendly result is one the card wants: a death trigger, a buff, or a
/// kill that pays.

#include "core/intent.h"

#include <vector>

namespace riftbound {

class GameState;
class CardRegistry;

std::vector<Intent> searchCandidates(const GameState& state,
                                     const std::vector<Intent>& legal,
                                     const CardRegistry& cards);

}  // namespace riftbound
