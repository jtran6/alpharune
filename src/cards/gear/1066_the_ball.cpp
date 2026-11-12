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

class TheBall_1066 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override { return {TriggerType::WhenYouPlayMe, TriggerType::WhenIMove}; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        int mode = pickMode(ctx, "The Ball", 2, {"Draw 1", "Deal 2 to enemy units here"});
        if (mode < 0) return;
        if (finalizingChoices(ctx)) return;
        if (mode == 0) ctx.executor.drawCards(ctx.controller, 1);
        else if (ctx.state.objectExists(ctx.source) && ctx.state.getObject(ctx.source).location.has_value()) {
            LocationId here = *ctx.state.getObject(ctx.source).location;
            std::vector<GameObjectId> enemies;
            for (auto& [id, obj] : ctx.state.objects.atLocation(here))
                if (obj.isUnit() && obj.controller != ctx.controller) enemies.push_back(id);
            for (auto id : enemies) ctx.executor.dealDamage(id, 2, ctx.source);
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1066;
        d.def_id = R"RB(rad-146-167)RB";
        d.name = R"RB(The Ball)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-146/167)RB";
        d.collector_number = 146;
        d.artist = R"RB()RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Mind};
        d.tags = {R"RB(Device)RB", R"RB(Orianna)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Deploy);
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
[Ganking] (While this is a unit, it can move from battlefield to battlefield.)
When you play or move this, choose one —
Draw 1.
Deal 2 to enemy units here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4b11f36bd5ce2b08677959664cda3906a04d234c-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1066(CardRegistry& r) {
    r.registerCard(1066, std::make_unique<TheBall_1066>());
}

}  // namespace riftbound
