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

class IridescentAvian_976 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; });
        auto t = pickTarget(ctx, "Iridescent Avian: buff a unit", units);
        if (t == kInvalidId) return;
        if (ctx.state.getObject(t).buff_count > 0) ctx.executor.giveTemporaryMight(t, 2);
        else ctx.executor.buffUnit(t);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 976;
        d.def_id = R"RB(rad-027-167)RB";
        d.name = R"RB(Iridescent Avian)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-027/167)RB";
        d.collector_number = 27;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.rarity = Rarity::Common;

        d.ability_text = R"RB([E]: [Buff] a unit. If it's already buffed, give it +2 [M] this turn instead. (To buff a unit, give it a +1 [M] buff if it doesn't have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/73fde9189b222313dcf8c2abe79b38082c76961e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_976(CardRegistry& r) {
    r.registerCard(976, std::make_unique<IridescentAvian_976>());
}

}  // namespace riftbound
