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

class TrinketPeddler_994 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // Printed rules are keywords and reminder text.

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 994;
        d.def_id = R"RB(rad-054-167)RB";
        d.name = R"RB(Trinket Peddler)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-054/167)RB";
        d.collector_number = 54;
        d.artist = R"RB(Alessandro Poli)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Disarm);
        d.disarm_value = 1;
        d.ability_text = R"RB([Disarm] (When I attack, give an enemy unit here -1 [M] this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/78106b7cea441b13d108e532adc5ce063f448165-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_994(CardRegistry& r) {
    r.registerCard(994, std::make_unique<TrinketPeddler_994>());
}

}  // namespace riftbound
