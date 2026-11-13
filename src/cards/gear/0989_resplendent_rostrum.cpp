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

class ResplendentRostrum_989 : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.executor.channelRunes(ctx.controller, 1, true);
    }

    TriggerType triggerType() const override { return TriggerType::WhenIBecomeExhausted; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        auto t = pickTarget(ctx, "Resplendent Rostrum: +3 [M]", units);
        if (finalizingChoices(ctx)) return;
        if (t != kInvalidId) ctx.executor.giveTemporaryMight(t, 3);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 989;
        d.def_id = R"RB(rad-046-167)RB";
        d.name = R"RB(Resplendent Rostrum)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-046/167)RB";
        d.collector_number = 46;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.energy_cost = 8;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Epic;

        d.ability_text = R"RB([E]: Draw 1 and channel 1 rune exhausted.
When I become exhausted, give a friendly unit +3 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/df1c12e65cc3c63c96fb6c96b2071795f37f30a6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_989(CardRegistry& r) {
    r.registerCard(989, std::make_unique<ResplendentRostrum_989>());
}

}  // namespace riftbound
