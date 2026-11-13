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

class NazumanNtofo_1008 : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasEquipAbility() const override { return true; }
    EquipCost equipCost(const GameState&, PlayerId, GameObjectId, GameObjectId) const override {
        EquipCost c;
        c.valid = true;
        c.energy = 1;
        c.power = 0;
        return c;
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1008;
        d.def_id = R"RB(rad-071-167)RB";
        d.name = R"RB(Nazuman Ntofo)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-071/167)RB";
        d.collector_number = 71;
        d.artist = R"RB()RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};
        d.energy_cost = 1;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Common;

        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB(Equip [1] ([1]: Attach this to a unit you control.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9d518a6db3ae174096b09cf80cece2997f685ede-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1008(CardRegistry& r) {
    r.registerCard(1008, std::make_unique<NazumanNtofo_1008>());
}

}  // namespace riftbound
