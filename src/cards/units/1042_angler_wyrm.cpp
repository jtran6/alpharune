#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class AnglerWyrm_1042 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    // "My battlefield" does not exist while this is at a base. Offering the
    // exhaust there spends it on a choice that cannot move anything.
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (!UnitCard::canActivateAbilityAt(state, controller, source, ability_index)) return false;
        if (!state.objectExists(source) || !state.getObject(source).location.has_value()) return false;
        return std::holds_alternative<BattlefieldLocation>(*state.getObject(source).location);
    }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId them = opponent(ctx.controller);
        BattlefieldId here = kInvalidId;
        if (ctx.state.objectExists(ctx.source) && ctx.state.getObject(ctx.source).location.has_value()) {
            if (auto* bf = std::get_if<BattlefieldLocation>(&*ctx.state.getObject(ctx.source).location))
                here = bf->id;
        }
        // The opponent performs the move (CR 412). A unit they cannot move
        // is not a legal choice while any unit can; if none can, the
        // instruction does nothing.
        auto units = chooseableUnits(ctx.state, them, [&](const GameObject& u) {
            if (u.controller != them) return false;
            if (u.cant_move_this_turn || u.cant_be_moved_by_enemy) return false;
            if (!here.valid() || !u.location.has_value()) return true;
            if (const auto* bf = std::get_if<BattlefieldLocation>(&*u.location))
                return bf->id != here;
            return true;
        });
        if (units.empty()) return;
        auto t = selectObjectAs(ctx, them, "Angler Wyrm: move a unit you control here", units, /*slot=*/7);
        if (!t) return;
        // "My battlefield" is where I am as this resolves. A unit that died
        // in response is not in the list the opponent was just shown.
        here = kInvalidId;
        if (ctx.state.objectExists(ctx.source) && ctx.state.getObject(ctx.source).location.has_value()) {
            if (auto* bf = std::get_if<BattlefieldLocation>(&*ctx.state.getObject(ctx.source).location))
                here = bf->id;
        }
        if (*t == kInvalidId || !here.valid() || !ctx.state.objectExists(*t) ||
            !ctx.state.getObject(*t).location.has_value())
            return;
        if (const auto* at = std::get_if<BattlefieldLocation>(&*ctx.state.getObject(*t).location))
            if (at->id == here) return;
        const auto& moving = ctx.state.getObject(*t);
        if (moving.cant_move_this_turn || moving.cant_be_moved_by_enemy) return;
        // They pay an arrival cost at my battlefield, or the unit stays.
        // Declining is allowed (CR 204.4.c).
        const auto raw = static_cast<size_t>(here.raw());
        if (raw < ctx.state.battlefields.size()) {
            const auto& slot = ctx.state.battlefields[static_cast<size_t>(raw)];
            const bool owed = (slot.arrival_energy > 0 || slot.arrival_power > 0) &&
                              static_cast<int>(them) != slot.arrival_exempt;
            if (owed) {
                if (!resource::canPayIn(ctx.state, them, slot.arrival_energy, slot.arrival_power,
                                        resource::kAnyDomain))
                    return;
                if (!ctx.executor.confirmNow(them,
                        "Angler Wyrm: pay [1][A] to move that unit here?"))
                    return;
            }
        }
        if (!payArrival(ctx, them, here)) return;
        ctx.executor.moveToBattlefield(*t, here);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1042;
        d.def_id = R"RB(rad-113-167)RB";
        d.name = R"RB(Angler Wyrm)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-113/167)RB";
        d.collector_number = 113;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 8;
        d.power_cost = 0;
        d.might = 8;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Deflect);
        d.deflect_value = 1;
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
[E]: Choose an opponent. They must move a unit they control to my battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e805eba9add03fd3e1b1601b15c12a46fd403282-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1042(CardRegistry& r) {
    r.registerCard(1042, std::make_unique<AnglerWyrm_1042>());
}

}  // namespace riftbound
