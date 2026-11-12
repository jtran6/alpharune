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

class ProtectorSShield_1050 : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // Printed rules are keywords and reminder text.

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1050;
        d.def_id = R"RB(rad-123-167)RB";
        d.name = R"RB(Protector's Shield)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-123/167)RB";
        d.collector_number = 123;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might = 1;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::QuickDraw);
        d.keywords.set(Keyword::Reaction);
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Quick-Draw] (This has [Reaction]. When you play it, attach it to a unit you control.)
[Equip] [1] ([1]: Attach this to a unit you control.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/97618bc30e9b65a2e5223e6f94db58d7098d30a6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1050(CardRegistry& r) {
    r.registerCard(1050, std::make_unique<ProtectorSShield_1050>());
}

}  // namespace riftbound
