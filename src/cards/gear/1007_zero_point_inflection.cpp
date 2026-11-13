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

class ZeroPointInflection_1007 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayACard, TriggerType::WhenYouDrawACard};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.resuming) return;
        if (ctx.state.chain.resuming->fired_trigger == TriggerType::WhenYouDrawACard) {
            if (ctx.state.player(ctx.controller).hand.size() >= 10) {
                ctx.executor.discardCards(ctx.controller, static_cast<int>(ctx.state.player(ctx.controller).hand.size()));
                ctx.executor.killObject(ctx.source);
                ctx.executor.scorePoints(opponent(ctx.controller), 1);
            }
            return;
        }
        ctx.executor.drawCards(ctx.controller, 2);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1007;
        d.def_id = R"RB(rad-069-167)RB";
        d.name = R"RB(Zero Point Inflection)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-069/167)RB";
        d.collector_number = 69;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.energy_cost = 9;
        d.power_cost = 2;
        d.might = 0;
        d.rarity = Rarity::Epic;

        d.ability_text = R"RB(When you play a card, draw 2.
When you draw 1 or more cards with 9 or more other cards in your hand, discard your hand, kill this, and choose an opponent. They score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5e0646270d27f9fa78f4c3a637695e5cdeb8ee67-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1007(CardRegistry& r) {
    r.registerCard(1007, std::make_unique<ZeroPointInflection_1007>());
}

}  // namespace riftbound
