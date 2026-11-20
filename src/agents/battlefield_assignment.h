#pragma once

/// Where a player's units should stand, for the search.
///
/// Master Yi, Wuju Bladesman (Wuju Bladesman - Starter) prints:
/// "While a friendly unit defends alone, it gets +2 [M]."
/// That bonus is the reason to leave one defender on a battlefield. A second
/// unit still goes there when it does something the +2 does not replace: it
/// conquers, it holds another battlefield, it is required to survive a combat
/// the single unit would lose, or the printed bonus does not apply. Stacking
/// with none of those reasons is dropped from the lines the search expands.
///
/// Offense is the same choice in the other direction. A conquer is gaining
/// control of a battlefield this player has not scored this turn (CR 471.1,
/// 472), and one unit covers a fight when its Might is strictly greater than
/// the enemy's — a tie leaves nobody, which is not a conquer (CR 468.3.d,
/// 468.5.b). When two battlefields can be conquered this turn, one unit goes
/// to each. Stacking a second unit on a battlefield that is already conquered,
/// or that one unit already covers, is dropped when that unit could conquer
/// somewhere else. A unit that cannot conquer anywhere else still stacks.
/// The rules engine's legal list is unchanged.

#include "core/intent.h"

#include <optional>
#include <vector>

namespace riftbound {

class GameState;
class CardRegistry;

/// The printed line the assignment follows.
inline constexpr const char* kWujuBladesmanDefendAlone =
    "While a friendly unit defends alone, it gets +2 [M].";

/// Might from that player's legend text ("defends alone" ... "+N [M]").
/// 0 when the legend does not say it. With no registry, Wuju Bladesman -
/// Starter (card 317) is that line and every other legend is 0.
int defendAloneBonus(const GameState& state, PlayerId player,
                     const CardRegistry* cards);

/// True when `player` is the one who defends `bf`: the defender of a combat
/// there, otherwise the battlefield's controller. An attacker does not.
bool defendsBattlefield(const GameState& state, PlayerId player,
                        BattlefieldId bf);

/// `lone` is the best single unit plus the defend-alone bonus. `group` is
/// every unit with that bonus off (a second body ends it). The extras are
/// required when the single unit would leave the enemy alive and the group
/// would not.
inline bool extrasRequiredToSurvive(int lone, int group, int enemy) {
    return lone < enemy && group >= enemy;
}

/// One unit's chosen post. `battlefield` is empty when it stays at base (or
/// in hand). `needed` is set when that post is what keeps a fight from being
/// lost.
struct UnitPost {
    GameObjectId id = kInvalidId;
    std::optional<BattlefieldId> battlefield;
    bool needed = false;
};

struct Assignment {
    std::vector<UnitPost> posts;
    int bonus = 0;
};

/// The chosen posts for `player`'s units. A unit already on a battlefield
/// stays unless it is the extra body of a defend-alone stack the bonus
/// already covers.
Assignment assignUnits(const GameState& state, PlayerId player,
                       const CardRegistry* cards);

/// Units `assignUnits` put at `bf`, in ascending id order.
std::vector<GameObjectId> assignedUnits(const Assignment& assignment,
                                        BattlefieldId bf);

/// Plays and standard moves that put a unit where `assignUnits` does not.
/// Dropped: a second body on a battlefield the defend-alone bonus already
/// covers, a move off a post the assignment marked needed, and a move onto
/// a battlefield that is already conquered or that one assigned unit already
/// covers when this unit was sent to conquer another. A join of a standard
/// move is the same placement. Spells, passes and other choices stay. An
/// empty result is put back: a pass is not withheld.
std::vector<Intent> keepBattlefieldAssignment(const GameState& state,
                                              const std::vector<Intent>& legal,
                                              const CardRegistry& cards);

}  // namespace riftbound
