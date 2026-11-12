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

class TitanicShrine_1023 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.resuming) return;
        auto sub = ctx.state.chain.resuming->triggering_subject;
        if (sub == kInvalidId || !ctx.state.objectExists(sub)) return;
        int mine = ctx.state.getObject(sub).current_might;
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (!obj.isUnit() || id == sub) continue;
            if (obj.current_might >= mine) return;
        }
        if (ctx.state.objectExists(ctx.source) && !ctx.state.getObject(ctx.source).is_exhausted) {
            ctx.executor.exhaustObject(ctx.source);
            ctx.executor.drawCards(ctx.controller, 1);
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1023;
        d.def_id = R"RB(rad-089-167)RB";
        d.name = R"RB(Titanic Shrine)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-089/167)RB";
        d.collector_number = 89;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Device)RB"};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(When you play a unit with Might greater than each other unit on the board, exhaust me to draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7d027f4e30df4f7141799709f9fb785835adefed-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1023(CardRegistry& r) {
    r.registerCard(1023, std::make_unique<TitanicShrine_1023>());
}

}  // namespace riftbound
