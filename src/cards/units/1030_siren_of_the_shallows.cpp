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

class SirenOfTheShallows_1030 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Disarm] and [Ganking] are printed keywords.

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1030;
        d.def_id = R"RB(rad-097-167)RB";
        d.name = R"RB(Siren of the Shallows)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-097/167)RB";
        d.collector_number = 97;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 1;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Disarm);
        d.keywords.set(Keyword::Ganking);
        d.disarm_value = 1;
        d.ability_text = R"RB([Disarm] (When I attack, give an enemy unit here -1 [M] this turn.)
[Ganking] (I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e666029dcebc507cc80435eee17db435305d1361-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1030(CardRegistry& r) {
    r.registerCard(1030, std::make_unique<SirenOfTheShallows_1030>());
}

}  // namespace riftbound
