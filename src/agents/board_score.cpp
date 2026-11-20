#include "agents/board_score.h"

#include "agents/battlefield_assignment.h"
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
// Smaller than the combat swing of turning a loss into a kill, and smaller
// than one battlefield. Large enough that a fight the defend-alone bonus
// already covers prefers the single defender over the same Might in two bodies.
constexpr double kUselessStack = 0.02;

int bakedFromLegend(const GameObject& unit, GameObjectId legend) {
    if (!legend.valid()) return 0;
    int baked = 0;
    for (const auto& aura : unit.aura_effects)
        if (aura.source == legend) baked += aura.might_bonus;
    return baked;
}

double scoreBoard(const GameState& state, const CardRegistry* cards) {
    const int victory = std::max(1, state.effectiveVictoryScore());
    const double points = static_cast<double>(state.player(PlayerId::Player1).score -
                                              state.player(PlayerId::Player2).score);
    const PlayerId seat[2] = {PlayerId::Player1, PlayerId::Player2};
    int bonus[2] = {0, 0};
    GameObjectId legend[2] = {kInvalidId, kInvalidId};
    for (int s = 0; s < 2; ++s) {
        bonus[s] = defendAloneBonus(state, seat[s], cards);
        legend[s] = state.player(seat[s]).legend_zone;
    }

    double board = 0.0;
    for (const auto& bf : state.battlefields) {
        if (bf.controller == PlayerId::Player1) board += kBattlefield;
        else if (bf.controller == PlayerId::Player2) board -= kBattlefield;

        int might[2] = {0, 0};
        int count[2] = {0, 0};
        int best[2] = {0, 0};
        int baked[2] = {0, 0};
        GameObjectId best_id[2] = {kInvalidId, kInvalidId};
        for (const auto& [id, obj] : state.objects.onBoard()) {
            if (!obj.isUnit() || !obj.isAlive() || obj.attached_to) continue;
            const auto at = obj.battlefieldId();
            if (!at || *at != bf.id) continue;
            if (obj.controller != PlayerId::Player1 && obj.controller != PlayerId::Player2)
                continue;
            const int s = playerIndex(obj.controller);
            const int m = std::max(0, obj.current_might);
            might[s] += m;
            ++count[s];
            const bool higher =
                m > best[s] ||
                (m == best[s] && (best_id[s] == kInvalidId || id < best_id[s]));
            if (higher) {
                best[s] = m;
                best_id[s] = id;
                baked[s] = bakedFromLegend(obj, legend[s]);
            }
        }

        bool defends[2] = {false, false};
        int eff[2] = {might[0], might[1]};
        for (int s = 0; s < 2; ++s) {
            defends[s] = defendsBattlefield(state, seat[s], bf.id);
            if (count[s] == 1 && defends[s] && bonus[s] > 0)
                eff[s] += std::max(0, bonus[s] - baked[s]);
        }
        if (eff[0] > 0 && eff[1] > 0) {
            const double edge = static_cast<double>(eff[0] - eff[1]) /
                                static_cast<double>(eff[0] + eff[1]);
            board += kCombat * edge;
        }
        for (int s = 0; s < 2; ++s) {
            if (count[s] <= 1 || bonus[s] <= 0 || !defends[s]) continue;
            const int lone = best[s] + bonus[s];
            if (extrasRequiredToSurvive(lone, might[s], eff[1 - s])) continue;
            const double pen = kUselessStack * static_cast<double>(count[s] - 1);
            board += (s == 0) ? -pen : pen;
        }
    }

    double value = (points + board) / static_cast<double>(victory);
    value = std::clamp(value, -1.0, 1.0);
    return value;
}

}  // namespace

double boardScore(const GameState& state) { return scoreBoard(state, nullptr); }

double boardScore(const GameState& state, const CardRegistry& cards) {
    return scoreBoard(state, &cards);
}

}  // namespace riftbound
