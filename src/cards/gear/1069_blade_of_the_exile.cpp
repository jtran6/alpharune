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

class BladeOfTheExile_1069 : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasEquipAbility() const override { return true; }
    EquipCost equipCost(const GameState&, PlayerId, GameObjectId, GameObjectId) const override {
        EquipCost c;
        c.valid = true;
        c.energy = 0;
        c.power = 0;
        return c;
    }
    bool canPayEquipExtraCost(const GameState& state, PlayerId controller,
                              GameObjectId, GameObjectId) const override {
        if (resource::canPayIn(state, controller, 0, 1, resource::kAnyDomain, SpendFor::Other))
            return true;
        return !chooseableUnits(state, controller, [&](const GameObject& u) {
            return u.controller == controller && !u.is_exhausted;
        }).empty();
    }
    bool payEquipExtraCost(CardContext& ctx, GameObjectId) override {
        const bool power = resource::canPayIn(ctx.state, ctx.controller, 0, 1, resource::kAnyDomain,
                                              SpendFor::Other);
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.controller == ctx.controller && !u.is_exhausted;
        });
        if (power && units.empty())
            return ctx.executor.payResources(ctx.controller, 0, 1, resource::kAnyDomain, SpendFor::Other);
        if (!power && !units.empty()) {
            const GameObjectId u = ctx.executor.chooseObjectNow(
                ctx.controller, units, "Blade of the Exile: exhaust a friendly unit");
            if (u == kInvalidId) return false;
            ctx.executor.exhaustObject(u);
            return true;
        }
        if (!power) return false;
        const int which = ctx.executor.chooseValueNow(
            ctx.controller, {"Pay [A]", "Exhaust a friendly unit"},
            "Blade of the Exile: [A] or exhaust a friendly unit", PromptKind::Mode);
        if (which == 0)
            return ctx.executor.payResources(ctx.controller, 0, 1, resource::kAnyDomain, SpendFor::Other);
        const GameObjectId u = ctx.executor.chooseObjectNow(
            ctx.controller, units, "Blade of the Exile: exhaust a friendly unit");
        if (u == kInvalidId) return false;
        ctx.executor.exhaustObject(u);
        return true;
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1069;
        d.def_id = R"RB(rad-150-167)RB";
        d.name = R"RB(Blade of the Exile)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-150/167)RB";
        d.collector_number = 150;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Chaos};
        d.tags = {R"RB(Equipment)RB", R"RB(Riven)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] — [A] or exhaust a friendly unit. (Pay the cost: Attach this to a unit you control.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/04aaded01b23641e496e39f51882ccfe89715492-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1069(CardRegistry& r) {
    r.registerCard(1069, std::make_unique<BladeOfTheExile_1069>());
}

}  // namespace riftbound
