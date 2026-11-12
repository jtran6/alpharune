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

class BattlefieldArmorer_1055 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyGearDies; }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1055;
        d.def_id = R"RB(rad-130-167)RB";
        d.name = R"RB(Battlefield Armorer)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-130/167)RB";
        d.collector_number = 130;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(When a friendly gear at a battlefield dies, you may pay [1] to return it to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/eb04c39763de8fb8e56153d1f0e4f799e15dced9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1055(CardRegistry& r) {
    r.registerCard(1055, std::make_unique<BattlefieldArmorer_1055>());
}

}  // namespace riftbound
