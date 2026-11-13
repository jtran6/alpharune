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

class PillagedArmory_1010 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenYouConquerHere};
    }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        auto t = pickTarget(ctx, "Pillaged Armory: buff a friendly unit", units);
        if (finalizingChoices(ctx)) return;
        if (t != kInvalidId) ctx.executor.buffUnit(t);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1010;
        d.def_id = R"RB(rad-073-167)RB";
        d.name = R"RB(Pillaged Armory)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-073/167)RB";
        d.collector_number = 73;
        d.artist = R"RB()RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Device)RB"};
        d.energy_cost = 1;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Deploy);
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
When you play this or when you conquer here, [Buff] a friendly unit. (Give it a +1 [M] buff if it doesn't have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/374248eb65ca1e90a434652073dbbd3a12a11eda-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1010(CardRegistry& r) {
    r.registerCard(1010, std::make_unique<PillagedArmory_1010>());
}

}  // namespace riftbound
