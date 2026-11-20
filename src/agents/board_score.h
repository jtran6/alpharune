#pragma once

/// Leaf value for ISMCTS. Replaces a random rollout: the rules already know
/// the score, who controls each battlefield, and the Might of the units that
/// would fight there. One point toward the victory score outweighs the whole
/// of that board picture, so a line that scores beats a line that spends a
/// card to shrink a friendly unit.
///
/// A fight one side has already won is that whole combat term. Extra Might
/// piled past the win does not outrank a second battlefield that is also
/// won, which is a second conquer.
///
/// A legend that pays Might while a unit defends alone (Wuju Bladesman -
/// Starter: "While a friendly unit defends alone, it gets +2 [M].") is
/// counted on that one defender. An extra body that the bonus already
/// replaces is a small penalty, so the leaf agrees with leaving it off.

namespace riftbound {

class GameState;
class CardRegistry;

/// From player 1's seat, in [-1, +1]. Player 2's value is the negation.
/// A finished game is not passed through here: the search uses the result.
/// The one-argument form reads Wuju Bladesman - Starter by its card id. The
/// overload reads the legend's printed text from `cards`.
double boardScore(const GameState& state);
double boardScore(const GameState& state, const CardRegistry& cards);

}  // namespace riftbound
