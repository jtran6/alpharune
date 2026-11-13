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

class ResourceExtractor_992 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::AtEndOfTurn; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.state.turn.turn_player != ctx.controller) return;
        if (!ctx.state.objectExists(ctx.source) || !ctx.state.getObject(ctx.source).location.has_value()) return;
        auto* bf = std::get_if<BattlefieldLocation>(&*ctx.state.getObject(ctx.source).location);
        if (!bf) return;
        const auto& slot = ctx.state.battlefields[bf->id.raw()];
        if (!slot.controller.has_value() || *slot.controller != ctx.controller) return;
        createGoldExhausted(ctx);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 992;
        d.def_id = R"RB(rad-052-167)RB";
        d.name = R"RB(Resource Extractor)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-052/167)RB";
        d.collector_number = 52;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Device)RB"};
        d.energy_cost = 1;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Deploy);
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
At the end of your turn, if you control this battlefield, play a Gold gear token exhausted. (It has "[Reaction][>] Kill this, [E]: [Add] [A]."))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d5cc2f85febe2260e51b88bddfeab6ce700c85dd-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_992(CardRegistry& r) {
    r.registerCard(992, std::make_unique<ResourceExtractor_992>());
}

}  // namespace riftbound
