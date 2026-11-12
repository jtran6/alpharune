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

class EvelynnInControl_1038 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source) || !ctx.state.getObject(ctx.source).location.has_value()) return;
        auto* bf = std::get_if<BattlefieldLocation>(&*ctx.state.getObject(ctx.source).location);
        if (!bf) return;
        for (auto id : ctx.state.battlefields[bf->id.raw()].facedown) {
            if (ctx.state.objectExists(id) && ctx.state.getObject(id).controller == ctx.controller) {
                ctx.executor.scorePoints(ctx.controller, 1);
                return;
            }
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1038;
        d.def_id = R"RB(rad-109-167)RB";
        d.name = R"RB(Evelynn, In Control)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-109/167)RB";
        d.collector_number = 109;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Evelynn)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Disarm);
        d.disarm_value = 1;
        d.ability_text = R"RB([Disarm] (When I attack, give an enemy unit here -1 [M] this turn.)
When I hold, if you control a facedown card here, score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8c17158e6153eddb4b56abd8a09cbbd20600b049-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1038(CardRegistry& r) {
    r.registerCard(1038, std::make_unique<EvelynnInControl_1038>());
}

}  // namespace riftbound
