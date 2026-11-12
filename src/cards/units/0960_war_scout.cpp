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

class WarScout_960 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; });
        const GameObjectId t = pickTarget(ctx, "War Scout: +2 [M] this turn", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        ctx.executor.giveTemporaryMight(t, 2);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 960;
        d.def_id = R"RB(rad-008-167)RB";
        d.name = R"RB(War Scout)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-008/167)RB";
        d.collector_number = 8;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 2;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][R] as an additional cost to have me enter ready.)
When you play me, give a unit +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3adae96f4731752074cf2145507a175dee800411-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_960(CardRegistry& r) {
    r.registerCard(960, std::make_unique<WarScout_960>());
}

}  // namespace riftbound
