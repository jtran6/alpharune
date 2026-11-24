#include "agents/battlefield_assignment.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_object.h"
#include "core/game_state.h"
#include "engine/resource_payment.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace riftbound {
namespace {

/// The signed Might in the "defends alone" sentence, the number before [M].
/// "it gets +2 [M]." is +2. No such sentence is 0.
int printedDefendAloneMight(std::string_view text) {
    constexpr std::string_view kMark = "defends alone";
    const auto at = text.find(kMark);
    if (at == std::string_view::npos) return 0;
    const auto stop = text.find('.', at);
    const auto sentence = text.substr(
        at, stop == std::string_view::npos ? text.size() - at : stop - at);
    const auto em = sentence.rfind("[M]");
    if (em == std::string_view::npos || em == 0) return 0;
    size_t end = em;
    while (end > 0 && sentence[end - 1] == ' ') --end;
    size_t begin = end;
    while (begin > 0 &&
           std::isdigit(static_cast<unsigned char>(sentence[begin - 1])))
        --begin;
    if (begin == end) return 0;
    int sign = 1;
    if (begin > 0 && sentence[begin - 1] == '-') sign = -1;
    int value = 0;
    for (size_t i = begin; i < end; ++i)
        value = value * 10 + (sentence[i] - '0');
    return sign * value;
}

int unitMight(const GameObject& obj) { return std::max(0, obj.current_might); }

int sideMightAt(const GameState& state, PlayerId who, BattlefieldId bf) {
    int sum = 0;
    for (const auto& [id, obj] : state.objects.onBoard()) {
        (void)id;
        if (!obj.isUnit() || !obj.isAlive() || obj.attached_to) continue;
        if (obj.controller != who || obj.battlefieldId() != bf) continue;
        sum += unitMight(obj);
    }
    return sum;
}

std::optional<BattlefieldId> battlefieldOf(const LocationId& loc) {
    if (const auto* bf = std::get_if<BattlefieldLocation>(&loc)) return bf->id;
    return std::nullopt;
}

struct Placement {
    GameObjectId unit = kInvalidId;
    std::optional<BattlefieldId> battlefield;
};

std::optional<Placement> placementOf(const GameState& state, const Intent& in) {
    const bool play = in.type == IntentType::PlayCard ||
                      in.type == IntentType::PlayActionCard ||
                      in.type == IntentType::PlayReaction;
    if (play && in.play_location && state.objectExists(in.card)) {
        const GameObject& obj = state.getObject(in.card);
        if (!obj.isUnit() || obj.attached_to) return std::nullopt;
        return Placement{in.card, battlefieldOf(*in.play_location)};
    }
    if (in.type == IntentType::StandardMove && in.move_destination &&
        in.units_to_move.size() == 1) {
        return Placement{in.units_to_move.front(),
                         battlefieldOf(*in.move_destination)};
    }
    // A unit joining a standard move (the destination is stamped on the
    // choice). "Move no more units" names nobody.
    if (in.type == IntentType::MakeChoice && in.move_destination &&
        in.chosen_objects.size() == 1) {
        return Placement{in.chosen_objects.front(),
                         battlefieldOf(*in.move_destination)};
    }
    return std::nullopt;
}

struct Body {
    GameObjectId id = kInvalidId;
    int might = 0;
    std::optional<BattlefieldId> origin;
    std::optional<BattlefieldId> at;
    bool free = false;
    bool needed = false;
};

bool mightDesc(const std::vector<Body>& bodies, int a, int b) {
    if (bodies[a].might != bodies[b].might) return bodies[a].might > bodies[b].might;
    return bodies[a].id < bodies[b].id;
}

/// One defender on a battlefield this player defends, plus whatever extras
/// the +bonus does not already replace. Units already there are the anchor
/// before a unit that would have to move in.
void coverDefense(std::vector<Body>& bodies, BattlefieldId bf, int enemy,
                  int bonus) {
    std::vector<int> here;
    std::vector<int> free;
    for (int i = 0; i < static_cast<int>(bodies.size()); ++i) {
        if (!bodies[i].free && bodies[i].at == bf) here.push_back(i);
        else if (bodies[i].free) free.push_back(i);
    }
    std::sort(here.begin(), here.end(),
              [&](int a, int b) { return mightDesc(bodies, a, b); });
    std::sort(free.begin(), free.end(),
              [&](int a, int b) { return mightDesc(bodies, a, b); });

    // Nobody here and nothing to fight: a spare unit may hold it later.
    if (here.empty() && enemy <= 0) return;

    int anchor = -1;
    if (!here.empty()) anchor = here.front();
    else if (!free.empty()) anchor = free.front();
    else return;

    auto take = [&](int idx, bool needed) {
        bodies[idx].free = false;
        bodies[idx].at = bf;
        bodies[idx].needed = needed;
    };

    const int lone = bodies[anchor].might + bonus;
    // The bonus covers the fight, including a tie: the enemy does not live,
    // so a second body adds nothing the +bonus does not already replace.
    if (enemy <= 0 || lone >= enemy) {
        take(anchor, false);
        for (int idx : here) {
            if (idx == anchor) continue;
            bodies[idx].free = true;
            bodies[idx].at = std::nullopt;
            bodies[idx].needed = false;
        }
        return;
    }

    int sum = bodies[anchor].might;
    take(anchor, false);
    std::vector<int> order;
    for (int idx : here)
        if (idx != anchor) order.push_back(idx);
    for (int idx : free)
        if (idx != anchor) order.push_back(idx);
    for (int idx : order) {
        if (sum >= enemy) break;
        sum += bodies[idx].might;
        take(idx, true);
    }
    if (!extrasRequiredToSurvive(lone, sum, enemy)) {
        // The whole group still leaves the enemy alive. Do not spend the
        // extra units on a fight the bonus cannot win either.
        for (auto& b : bodies) {
            if (b.id == bodies[anchor].id) {
                b.at = bf;
                b.free = false;
                b.needed = false;
                continue;
            }
            if (b.origin == bf || b.at == bf) {
                b.at = std::nullopt;
                b.free = true;
                b.needed = false;
            }
        }
        return;
    }
    for (int idx : here) {
        if (idx == anchor || bodies[idx].needed) continue;
        bodies[idx].free = true;
        bodies[idx].at = std::nullopt;
        bodies[idx].needed = false;
    }
}

struct Fight {
    BattlefieldId bf = kInvalidId;
    int enemy = 0;
    int present = 0;
};

int mightOn(const std::vector<Body>& bodies, BattlefieldId bf) {
    int sum = 0;
    for (const auto& b : bodies)
        if (!b.free && b.at == bf) sum += b.might;
    return sum;
}

bool alreadyScored(const GameState& state, PlayerId player, BattlefieldId bf) {
    return state.player(player).battlefields_scored_this_turn.count(bf) != 0;
}

/// Control gained of a battlefield not yet scored this turn (CR 471.1, 472).
/// One this player already controls is a hold or a defense, not a conquer.
bool conquerTarget(const GameState& state, PlayerId player, BattlefieldId bf) {
    if (!bf.valid() || bf.raw() >= state.battlefields.size()) return false;
    if (state.battlefields[bf].controller == player) return false;
    return !alreadyScored(state, player, bf);
}

void place(Body& b, BattlefieldId bf) {
    b.free = false;
    b.at = bf;
    b.needed = false;
}

/// Smallest free unit whose Might is strictly greater than `gap`.
int soloCover(const std::vector<Body>& bodies, int gap) {
    int pick = -1;
    for (int i = 0; i < static_cast<int>(bodies.size()); ++i) {
        if (!bodies[i].free || bodies[i].might <= gap) continue;
        if (pick < 0 || bodies[i].might < bodies[pick].might ||
            (bodies[i].might == bodies[pick].might && bodies[i].id < bodies[pick].id))
            pick = i;
    }
    return pick;
}

int largestFree(const std::vector<Body>& bodies) {
    int pick = -1;
    for (int i = 0; i < static_cast<int>(bodies.size()); ++i) {
        if (!bodies[i].free) continue;
        if (pick < 0 || bodies[i].might > bodies[pick].might ||
            (bodies[i].might == bodies[pick].might && bodies[i].id < bodies[pick].id))
            pick = i;
    }
    return pick;
}

int freeMight(const std::vector<Body>& bodies) {
    int sum = 0;
    for (const auto& b : bodies)
        if (b.free) sum += b.might;
    return sum;
}

/// One unit covers a fight when the Might there is strictly greater than the
/// enemy's. A tie leaves nobody, and that is not a conquer (CR 468.3.d,
/// 468.5.b). Spares take those conquers one battlefield at a time, easiest
/// first. Units that cannot add a conquer then stack on the first fight.
void sendSparesToConquers(std::vector<Body>& bodies, const GameState& state,
                          PlayerId player, int bonus) {
    std::vector<Fight> fights;
    for (const auto& bf : state.battlefields) {
        if (bonus > 0 && defendsBattlefield(state, player, bf.id)) continue;
        const int enemy = sideMightAt(state, opponent(player), bf.id);
        if (enemy <= 0) continue;
        fights.push_back(Fight{bf.id, enemy, mightOn(bodies, bf.id)});
    }

    auto easier = [&](int a, int b) {
        const int ga = fights[a].enemy - fights[a].present;
        const int gb = fights[b].enemy - fights[b].present;
        if (ga != gb) return ga < gb;
        return fights[a].bf < fights[b].bf;
    };

    std::vector<int> open;
    for (int i = 0; i < static_cast<int>(fights.size()); ++i) {
        if (fights[i].present > fights[i].enemy) continue;
        if (!conquerTarget(state, player, fights[i].bf)) continue;
        open.push_back(i);
    }
    std::stable_sort(open.begin(), open.end(), easier);
    for (int fi : open) {
        const int pick = soloCover(bodies, fights[fi].enemy - fights[fi].present);
        if (pick < 0) continue;
        place(bodies[pick], fights[fi].bf);
        fights[fi].present += bodies[pick].might;
    }

    // A fight one unit could not take, that the units still free can take
    // together. Only until it is covered, so the rest can conquer elsewhere.
    for (int guard = static_cast<int>(bodies.size()); guard > 0; --guard) {
        int best = -1;
        for (int i = 0; i < static_cast<int>(fights.size()); ++i) {
            if (fights[i].present > fights[i].enemy) continue;
            if (!conquerTarget(state, player, fights[i].bf)) continue;
            if (fights[i].present + freeMight(bodies) <= fights[i].enemy) continue;
            if (best < 0 || easier(i, best)) best = i;
        }
        if (best < 0) break;
        int steps = static_cast<int>(bodies.size());
        while (fights[best].present <= fights[best].enemy && steps-- > 0) {
            const int pick = largestFree(bodies);
            if (pick < 0 || bodies[pick].might <= 0) break;
            place(bodies[pick], fights[best].bf);
            fights[best].present += bodies[pick].might;
        }
    }

    for (const auto& f : fights) {
        bool joined = false;
        for (auto& b : bodies) {
            if (!b.free) continue;
            place(b, f.bf);
            joined = true;
        }
        if (joined) break;
    }
}

/// Might of the units `assigned` put at `bf`, other than `except`. Strictly
/// greater than the enemy's Might is a fight one unit already covers.
bool coveredWithout(const GameState& state, PlayerId player,
                    const Assignment& assigned, BattlefieldId bf,
                    GameObjectId except) {
    int ours = 0;
    bool any = false;
    for (const auto& post : assigned.posts) {
        if (post.id == except || post.battlefield != bf) continue;
        if (!state.objectExists(post.id)) continue;
        any = true;
        ours += unitMight(state.getObject(post.id));
    }
    if (!any) return false;
    return ours > sideMightAt(state, opponent(player), bf);
}

/// A card in hand or the Champion Zone conquers only if it is played. The
/// Main Phase omits the play when the printed cost and an alternative cost
/// both fail. A discount that exists only at one battlefield is not asked
/// here: a card we cannot see a way to pay for is not posted, and its play
/// stays in the legal list.
bool canPlayUnitNow(const GameState& state, PlayerId player, const GameObject& obj,
                    const CardRegistry* cards) {
    if (!cards || obj.card_def_id == kInvalidId) return false;
    if (state.player(player).cant_play_cards_this_turn) return false;
    const Card* card = cards->get(obj.card_def_id);
    if (!card || !card->canBePlayed(state, player)) return false;

    const CardDef& def = card->def();
    const auto& ps = state.player(player);
    const bool ignore_energy = ps.transient_ignore_energy ||
        obj.card_counters.find("__ignore_energy") != obj.card_counters.end();
    const bool ignore_power = ps.transient_ignore_power;
    int energy = ps.transient_replace_energy >= 0 ? ps.transient_replace_energy
                                                  : def.energy_cost;
    int power = ps.transient_replace_power >= 0 ? ps.transient_replace_power
                                                : def.power_cost;
    if (ignore_energy) energy = 0;
    if (ignore_power) power = 0;
    energy = std::max(0, energy - card->selfCostReduction(state, player));
    power = std::max(0, power - card->selfPowerCostReduction(state, player));
    if (resource::canPayIn(state, player, energy, power, resource::maskOf(def.domains),
                           SpendFor::PlayUnit))
        return true;

    const auto alt = card->alternativePlayCost(state, player);
    if (!alt.valid) return false;
    const auto mask = alt.any_domain ? resource::kAnyDomain
                                     : resource::maskOf(alt.power_domain);
    return resource::canPayIn(state, player, alt.energy, alt.power, mask,
                              SpendFor::PlayUnit);
}

}  // namespace

int defendAloneBonus(const GameState& state, PlayerId player,
                     const CardRegistry* cards) {
    const GameObjectId id = state.player(player).legend_zone;
    if (!id.valid() || !state.objectExists(id)) return 0;
    const GameObject& legend = state.getObject(id);
    if (legend.controller != player || legend.zone.get() != ZoneType::LegendZone)
        return 0;
    if (cards) {
        const Card* card = cards->get(legend.card_def_id);
        if (!card) return 0;
        const int printed = printedDefendAloneMight(card->def().ability_text);
        return printed > 0 ? printed : 0;
    }
    // Wuju Bladesman - Starter. "While a friendly unit defends alone, it gets +2 [M]."
    if (legend.card_def_id == 317) return 2;
    return 0;
}

bool defendsBattlefield(const GameState& state, PlayerId player,
                        BattlefieldId bf) {
    if (!bf.valid()) return false;
    if (bf.raw() >= state.battlefields.size()) return false;
    const BattlefieldState& field = state.battlefields[bf];
    if (field.combat_in_progress) return field.defender == player;
    return field.controller == player;
}

Assignment assignUnits(const GameState& state, PlayerId player,
                       const CardRegistry* cards) {
    Assignment out;
    out.bonus = defendAloneBonus(state, player, cards);

    std::vector<Body> bodies;
    for (const auto& [id, obj] : state.objects) {
        if (obj.controller != player || !obj.isUnit() || !obj.isAlive()) continue;
        if (obj.attached_to) continue;
        const auto at = obj.battlefieldId();
        if (at) {
            bodies.push_back(Body{id, unitMight(obj), at, at, false, false});
            continue;
        }
        const ZoneType zone = obj.zone.get();
        const bool in_hand = zone == ZoneType::Hand || zone == ZoneType::ChampionZone;
        const bool at_base = obj.isAtBase() && !obj.is_exhausted && !obj.cant_move_this_turn;
        if (in_hand) {
            if (!canPlayUnitNow(state, player, obj, cards)) continue;
        } else if (!at_base) {
            continue;
        }
        bodies.push_back(Body{id, unitMight(obj), std::nullopt, std::nullopt, true, false});
    }

    if (out.bonus > 0) {
        for (const auto& bf : state.battlefields) {
            if (!defendsBattlefield(state, player, bf.id)) continue;
            coverDefense(bodies, bf.id, sideMightAt(state, opponent(player), bf.id),
                         out.bonus);
        }
    }

    // A spare unit holds a battlefield we already control, then conquers an
    // empty one. One already scored this turn is not a point (CR 472), so
    // it does not take the unit that could conquer somewhere else. The fight
    // above is settled first, so a unit required there is not the one sent away.
    std::vector<BattlefieldId> empty;
    for (const auto& bf : state.battlefields) {
        if (sideMightAt(state, opponent(player), bf.id) > 0) continue;
        if (alreadyScored(state, player, bf.id)) continue;
        bool ours = false;
        for (const auto& b : bodies)
            if (b.at == bf.id) ours = true;
        if (!ours) empty.push_back(bf.id);
    }
    std::stable_sort(empty.begin(), empty.end(), [&](BattlefieldId a, BattlefieldId b) {
        const bool ac = state.battlefields[a].controller == player;
        const bool bc = state.battlefields[b].controller == player;
        if (ac != bc) return ac;
        return a < b;
    });
    for (BattlefieldId bf : empty) {
        int pick = -1;
        for (int i = 0; i < static_cast<int>(bodies.size()); ++i) {
            if (!bodies[i].free) continue;
            if (pick < 0 || bodies[i].might < bodies[pick].might ||
                (bodies[i].might == bodies[pick].might && bodies[i].id < bodies[pick].id))
                pick = i;
        }
        if (pick < 0) break;
        bodies[pick].free = false;
        bodies[pick].at = bf;
        bodies[pick].needed = false;
    }

    // A second unit stacks on a fight only when it cannot conquer elsewhere.
    sendSparesToConquers(bodies, state, player, out.bonus);

    out.posts.reserve(bodies.size());
    for (const auto& b : bodies)
        out.posts.push_back(UnitPost{b.id, b.at, b.needed});
    return out;
}

std::vector<GameObjectId> assignedUnits(const Assignment& assignment,
                                        BattlefieldId bf) {
    std::vector<GameObjectId> ids;
    for (const auto& post : assignment.posts)
        if (post.battlefield == bf) ids.push_back(post.id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<Intent> keepBattlefieldAssignment(const GameState& state,
                                              const std::vector<Intent>& legal,
                                              const CardRegistry& cards) {
    if (legal.empty()) return legal;
    const PlayerId player = legal.front().player;
    if (player != PlayerId::Player1 && player != PlayerId::Player2) return legal;

    const int bonus = defendAloneBonus(state, player, &cards);
    const Assignment assigned = assignUnits(state, player, &cards);
    std::vector<Intent> kept;
    kept.reserve(legal.size());
    for (const Intent& in : legal) {
        const auto placed = placementOf(state, in);
        if (!placed) {
            kept.push_back(in);
            continue;
        }
        const UnitPost* post = nullptr;
        for (const auto& candidate : assigned.posts)
            if (candidate.id == placed->unit) post = &candidate;
        if (!post) {
            kept.push_back(in);
            continue;
        }
        if (bonus > 0) {
            const bool onto_defense =
                placed->battlefield &&
                defendsBattlefield(state, player, *placed->battlefield);
            // A second body on a battlefield the bonus already covers.
            if (onto_defense && post->battlefield != placed->battlefield) continue;
            // The unit is the one that keeps the enemy from living.
            if (post->needed && post->battlefield &&
                placed->battlefield != post->battlefield)
                continue;
        }
        // Stacking onto a battlefield already conquered, or that one assigned
        // unit already covers, when this unit was sent to conquer another.
        if (placed->battlefield && post->battlefield &&
            *placed->battlefield != *post->battlefield &&
            (alreadyScored(state, player, *placed->battlefield) ||
             coveredWithout(state, player, assigned, *placed->battlefield, placed->unit)))
            continue;
        kept.push_back(in);
    }
    return kept.empty() ? legal : kept;
}

}  // namespace riftbound
