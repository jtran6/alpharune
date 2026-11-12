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

class SeraphineInspiring_1003 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouExhaustThreeOrMore; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.state.objectExists(ctx.source) &&
            ctx.state.getObject(ctx.source).card_counters["__sera_drew"] == ctx.state.turn.turn_number)
            { if (finalizingChoices(ctx)) return; return; }
        int go = confirmOptional(ctx, "Seraphine, Inspiring: draw 1", []{ return true; });
        if (go < 0) return;
        if (finalizingChoices(ctx)) return;
        if (go == 1) {
            ctx.executor.drawCards(ctx.controller, 1);
            if (ctx.state.objectExists(ctx.source))
                ctx.state.getObject(ctx.source).card_counters["__sera_drew"] = ctx.state.turn.turn_number;
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1003;
        d.def_id = R"RB(rad-065-167)RB";
        d.name = R"RB(Seraphine, Inspiring)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-065/167)RB";
        d.collector_number = 65;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Piltover)RB", R"RB(Seraphine)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Disarm);
        d.disarm_value = 2;
        d.ability_text = R"RB([Disarm 2] (When I attack, give an enemy unit here -2 [M] this turn.)
Once each turn, when you exhaust three or more friendly units at the same time, you may draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/60e0711c64958cc3a59a3bd3bf4c013758b1b92b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1003(CardRegistry& r) {
    r.registerCard(1003, std::make_unique<SeraphineInspiring_1003>());
}

}  // namespace riftbound
