#include "agents/board_score.h"

#include "core/game_object.h"
#include "core/game_state.h"

#include <algorithm>

namespace riftbound {
namespace {

// In points, not yet divided by the victory score. A scored point is 1.
// Two battlefields, each controlled and each a won fight, is 0.8: still
// less than the point those fights have not scored yet.
constexpr double kBattlefield = 0.2;
constexpr double kCombat = 0.2;

}  // namespace

double boardScore(const GameState& state) {
    const int victory = std::max(1, state.effectiveVictoryScore());
    const double points = static_cast<double>(state.player(PlayerId::Player1).score -
                                              state.player(PlayerId::Player2).score);

    double board = 0.0;
    for (const auto& bf : state.battlefields) {
        if (bf.controller == PlayerId::Player1) board += kBattlefield;
        else if (bf.controller == PlayerId::Player2) board -= kBattlefield;

        int might[2] = {0, 0};
        for (const auto& [id, obj] : state.objects.onBoard()) {
            (void)id;
            if (!obj.isUnit() || !obj.isAlive()) continue;
            const auto at = obj.battlefieldId();
            if (!at || *at != bf.id) continue;
            if (obj.controller != PlayerId::Player1 && obj.controller != PlayerId::Player2)
                continue;
            might[playerIndex(obj.controller)] += std::max(0, obj.current_might);
        }
        if (might[0] > 0 && might[1] > 0) {
            const double edge = static_cast<double>(might[0] - might[1]) /
                                static_cast<double>(might[0] + might[1]);
            board += kCombat * edge;
        }
    }

    double value = (points + board) / static_cast<double>(victory);
    value = std::clamp(value, -1.0, 1.0);
    return value;
}

}  // namespace riftbound
