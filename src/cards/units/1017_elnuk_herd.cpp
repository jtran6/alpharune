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

class ElnukHerd_1017 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.controller == ctx.controller && u.id != ctx.source;
        });
        auto t = pickTarget(ctx, "Elnuk Herd: +3 [M] to another friendly unit", units);
        if (finalizingChoices(ctx)) return;
        if (t != kInvalidId) ctx.executor.giveTemporaryMight(t, 3);
        if (playedFromFacedown(ctx.state, ctx.source)) ctx.executor.recycleCards(ctx.controller, {ctx.source});
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1017;
        d.def_id = R"RB(rad-081-167)RB";
        d.name = R"RB(Elnuk Herd)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-081/167)RB";
        d.collector_number = 81;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.energy_cost = 6;
        d.power_cost = 0;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
When you play me, give another friendly unit +3 [M] this turn, then recycle me if I was played from face down.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1799cd4c46fcf0243b4022de2d427b15e67a15ad-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1017(CardRegistry& r) {
    r.registerCard(1017, std::make_unique<ElnukHerd_1017>());
}

}  // namespace riftbound
