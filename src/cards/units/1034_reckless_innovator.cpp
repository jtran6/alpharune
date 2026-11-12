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

class RecklessInnovator_1034 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenIBecomeExhausted; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        int mode = pickMode(ctx, "Reckless Innovator", 2, {"Play an exhausted Gold", "Pay [P] to draw 1"});
        if (mode < 0) return;
        if (finalizingChoices(ctx)) return;
        if (mode == 0) createGoldExhausted(ctx);
        else if (ctx.executor.payResources(ctx.controller, 0, 1, resource::maskOf(Domain::Chaos), SpendFor::Other))
            ctx.executor.drawCards(ctx.controller, 1);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1034;
        d.def_id = R"RB(rad-105-167)RB";
        d.name = R"RB(Reckless Innovator)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-105/167)RB";
        d.collector_number = 105;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][P] as an additional cost to have me enter ready.)
When I become exhausted, choose one —
Play a Gold gear token exhausted. (It has "[Reaction][>] Kill this, [E]: [Add] [A].")Pay [P] to draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b7ff5684309bcd346e6b3534e0d1f38ec1a38726-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1034(CardRegistry& r) {
    r.registerCard(1034, std::make_unique<RecklessInnovator_1034>());
}

}  // namespace riftbound
