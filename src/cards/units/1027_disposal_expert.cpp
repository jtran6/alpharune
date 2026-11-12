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

class DisposalExpert_1027 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool entersReadyOnPlay(const GameState& state, PlayerId controller, const GameObject& self) const override {
        for (auto id : state.allUnitsControlledBy(controller)) if (id != self.id) return false;
        return true;
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1027;
        d.def_id = R"RB(rad-093-167)RB";
        d.name = R"RB(Disposal Expert)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-093/167)RB";
        d.collector_number = 93;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 3;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Disarm);
        d.disarm_value = 2;
        d.ability_text = R"RB(I enter ready if you control no other units.
[Disarm 2] (When I attack, give an enemy unit here -2 [M] this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/79ec89753be773e7ea93b2116ea29d21c81fcef9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1027(CardRegistry& r) {
    r.registerCard(1027, std::make_unique<DisposalExpert_1027>());
}

}  // namespace riftbound
