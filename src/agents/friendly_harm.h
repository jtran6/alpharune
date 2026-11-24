#pragma once

/// Search-only prune for empty friendly-harm lines.
///
/// The rules engine still generates those plays. MCTS and ISMCTS call
/// searchCandidates and score only what it returns, so a line that deals
/// damage, reduces Might, or bounces a friendly unit is not taken when that
/// result does not enable a printed death trigger or a kill that pays a
/// cost. A non-killing result is dropped on its own. A kill of a friendly
/// with no death trigger is dropped whether or not an enemy is also a legal
/// target: an empty target set is not an enemy, and that set itself stays
/// because it harms nobody. A friendly target the printed card wants (it
/// says "friendly" / "unit you control", or must_be_friendly) stays, and so
/// does a banish that plays the unit again. A draw or a Predict stays when
/// no enemy is a legal target. Playing a unit stays: the body is the line,
/// even when an optional or enemy-only line of text parses as harm. A spell
/// that says "Give a unit +N [M]" is scored the other way. The printed card
/// does not say the unit is his, so an opponent's unit stays legal and stays
/// in the search when it is the only unit. While a friendly unit is also a
/// legal target, that is the line that is scored. A dropped list is not put
/// back when nothing else remains.

#include "core/intent.h"

#include <vector>

namespace riftbound {

class GameState;
class CardRegistry;

std::vector<Intent> searchCandidates(const GameState& state,
                                     const std::vector<Intent>& legal,
                                     const CardRegistry& cards);

}  // namespace riftbound
