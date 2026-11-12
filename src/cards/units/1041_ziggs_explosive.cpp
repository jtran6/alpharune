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

class ZiggsExplosive_1041 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.energy = 1; c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        playBombToken(*this, ctx, "Ziggs, Explosive: play a Bomb");
    }

    TriggerType triggerType() const override { return TriggerType::WhenIDefend; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto gears = boardObjects(ctx.state, [&](const GameObject& o) { return o.isGear() && o.controller == ctx.controller; });
        int go = confirmOptional(ctx, "Ziggs: kill a friendly gear to get +2 [M]", [ok=!gears.empty()]{ return ok; });
        if (go < 0) return;
        if (finalizingChoices(ctx)) return;
        if (go != 1) return;
        auto t = pickTarget(ctx, "Ziggs: kill a gear", gears);
        if (t != kInvalidId && killAsCost(ctx, t)) ctx.executor.giveTemporaryMight(ctx.source, 2);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1041;
        d.def_id = R"RB(rad-112-167)RB";
        d.name = R"RB(Ziggs, Explosive)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-112/167)RB";
        d.collector_number = 112;
        d.artist = R"RB()RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bandle City)RB", R"RB(Ziggs)RB"};
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 4;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB([1], [E]: Play a Bomb gear token to a battlefield. (It has [Deploy]. When it dies, deal 2 to an enemy there and kill your other Bombs there.)
When I defend, you may kill a friendly gear to give me +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f0c1f1f3b69b31a69f9637ece841b5ee1029cdd3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1041(CardRegistry& r) {
    r.registerCard(1041, std::make_unique<ZiggsExplosive_1041>());
}

}  // namespace riftbound
