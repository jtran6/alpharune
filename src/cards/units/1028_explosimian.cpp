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

class Explosimian_1028 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        playBombToken(*this, ctx, "Explosimian: a battlefield for three Bombs");
        if (finalizingChoices(ctx)) return;
        playBombToken(*this, ctx, "Explosimian: a battlefield for three Bombs");
        playBombToken(*this, ctx, "Explosimian: a battlefield for three Bombs");
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1028;
        d.def_id = R"RB(rad-094-167)RB";
        d.name = R"RB(Explosimian)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-094/167)RB";
        d.collector_number = 94;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 6;
        d.power_cost = 0;
        d.might = 6;
        d.rarity = Rarity::Common;

        d.ability_text = R"RB(When I hold, play three Bomb gear tokens to a single battlefield. (They have [Deploy]. When each dies, deal 2 to an enemy there and kill your other Bombs there.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f2590617229f3efc078aa2429f65b85101303200-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1028(CardRegistry& r) {
    r.registerCard(1028, std::make_unique<Explosimian_1028>());
}

}  // namespace riftbound
