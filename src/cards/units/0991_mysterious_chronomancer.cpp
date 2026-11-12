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

class MysteriousChronomancer_991 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.predict(ctx.controller, 2);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 991;
        d.def_id = R"RB(rad-050-167)RB";
        d.name = R"RB(Mysterious Chronomancer)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-050/167)RB";
        d.collector_number = 50;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.energy_cost = 5;
        d.power_cost = 0;
        d.might = 4;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Disarm);
        d.disarm_value = 2;
        d.ability_text = R"RB(When you play me, [Predict 2]. (Look at the top 2 cards of your Main Deck. Recycle any of them and put the rest back in any order.)
[Disarm 2] (When I attack, give an enemy unit here -2 [M] this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/72828b0fe019b2e03069510f781dfa8f5759ac12-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_991(CardRegistry& r) {
    r.registerCard(991, std::make_unique<MysteriousChronomancer_991>());
}

}  // namespace riftbound
