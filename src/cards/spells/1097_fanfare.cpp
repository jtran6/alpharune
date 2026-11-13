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

class Fanfare_1097 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.state.player(ctx.controller).fanfare_draws += 3;
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1097;
        d.def_id = R"RB(rad-047-167)RB";
        d.name = R"RB(Fanfare)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-047/167)RB";
        d.collector_number = 47;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Common;

        d.ability_text = R"RB(The next three times you play a unit this turn, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/75a3d8fbfd8caeabfd5be38513a6624c61f5230f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1097(CardRegistry& r) {
    r.registerCard(1097, std::make_unique<Fanfare_1097>());
}

}  // namespace riftbound
