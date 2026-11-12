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

class PiltovanGuidebot_996 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayACard; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        int energy = 0;
        if (ctx.state.chain.resuming) {
            auto sub = ctx.state.chain.resuming->triggering_subject;
            if (sub != kInvalidId && ctx.state.objectExists(sub) && ctx.registry) {
                auto id = ctx.state.getObject(sub).card_def_id;
                if (id != kInvalidId && ctx.registry->get(id)) energy = ctx.registry->get(id)->def().energy_cost;
            }
        }
        int go = confirmOptional(ctx, "Piltovan Guidebot: my base Might becomes that card's Energy cost this turn", []{ return true; });
        if (go < 0) return;
        if (finalizingChoices(ctx)) return;
        if (go == 1 && ctx.state.objectExists(ctx.source)) {
            ctx.state.getObject(ctx.source).base_might_this_turn = energy;
            ctx.state.getObject(ctx.source).recomputeMight();
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 996;
        d.def_id = R"RB(rad-057-167)RB";
        d.name = R"RB(Piltovan Guidebot)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-057/167)RB";
        d.collector_number = 57;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 3;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(When you play a card, you may have my base Might become equal to that card's Energy cost this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9558ced7593f5a26911eb6e1393aae3289842e20-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_996(CardRegistry& r) {
    r.registerCard(996, std::make_unique<PiltovanGuidebot_996>());
}

}  // namespace riftbound
