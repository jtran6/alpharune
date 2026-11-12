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

class PitilessStalker_1033 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller != ctx.controller; });
        auto t = pickTarget(ctx, "Pitiless Stalker: stun an enemy", units);
        if (t != kInvalidId) ctx.executor.stunUnit(t);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1033;
        d.def_id = R"RB(rad-104-167)RB";
        d.name = R"RB(Pitiless Stalker)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-104/167)RB";
        d.collector_number = 104;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB([E]: [Stun] an enemy unit. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fc61b244fb3ed01d61617c3c2633a71424604130-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1033(CardRegistry& r) {
    r.registerCard(1033, std::make_unique<PitilessStalker_1033>());
}

}  // namespace riftbound
