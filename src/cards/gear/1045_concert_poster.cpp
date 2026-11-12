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

class ConcertPoster_1045 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override { return {TriggerType::WhenYouPlayMe, TriggerType::WhenYouConquerHere}; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        int go = confirmOptional(ctx, "Concert Poster: pay [1] to play a Recruit", []{ return true; });
        if (go < 0) return;
        if (finalizingChoices(ctx)) return;
        if (go == 1 && ctx.executor.payResources(ctx.controller, 1, 0, resource::kAnyDomain, SpendFor::Other))
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1, {"Recruit"}, {}, BaseLocation{ctx.controller});
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1045;
        d.def_id = R"RB(rad-116-167)RB";
        d.name = R"RB(Concert Poster)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-116/167)RB";
        d.collector_number = 116;
        d.artist = R"RB()RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Device)RB"};
        d.energy_cost = 1;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Deploy);
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
When you play this or when you conquer here, you may pay [1] to play a 1 [M] Recruit unit token to your base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0e80322dac6065e3ee726f5f9a2650a88195a44c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1045(CardRegistry& r) {
    r.registerCard(1045, std::make_unique<ConcertPoster_1045>());
}

}  // namespace riftbound
