#pragma once

/// Leaf value for ISMCTS. Replaces a random rollout: the rules already know
/// the score, who controls each battlefield, and the Might of the units that
/// would fight there. One point toward the victory score outweighs the whole
/// of that board picture, so a line that scores beats a line that spends a
/// card to shrink a friendly unit.

namespace riftbound {

class GameState;

/// From player 1's seat, in [-1, +1]. Player 2's value is the negation.
/// A finished game is not passed through here: the search uses the result.
double boardScore(const GameState& state);

}  // namespace riftbound
