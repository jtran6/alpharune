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

class RuinedMonastery_1083 : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        int go = confirmOptional(ctx, "Ruined Monastery: discard 1 and channel 1 exhausted", []{ return true; });
        if (go < 0) return;
        if (finalizingChoices(ctx)) return;
        if (go == 1) {
            ctx.executor.discardCards(ctx.controller, 1);
            ctx.executor.channelRunes(ctx.controller, 1, true);
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1083;
        d.def_id = R"RB(rad-166-167)RB";
        d.name = R"RB(Ruined Monastery)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-166/167)RB";
        d.collector_number = 166;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(When you conquer here, you may discard 1 and channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2d756c5198dafb151f51cceca4f11824575960cd-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1083(CardRegistry& r) {
    r.registerCard(1083, std::make_unique<RuinedMonastery_1083>());
}

}  // namespace riftbound
