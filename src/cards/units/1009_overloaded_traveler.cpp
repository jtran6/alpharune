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

class OverloadedTraveler_1009 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool hasMightDependentAura() const override { return true; }
    void applyMightDependentAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId self, GameObject& me) {
            if (!me.location.has_value()) return;
            for (auto& [id, obj] : state.objects.atLocation(*me.location)) {
                if (obj.isGear() && obj.controller == controller && obj.hasKeyword(Keyword::Deploy)) {
                    auraMight(me, self, 1);
                    return;
                }
            }
        });
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1009;
        d.def_id = R"RB(rad-072-167)RB";
        d.name = R"RB(Overloaded Traveler)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-072/167)RB";
        d.collector_number = 72;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {};
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 2;
        d.rarity = Rarity::Common;
        d.ability_text = R"RB(While there's a friendly gear with [Deploy] at my battlefield, I have +1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6db2839d8702b15ab62413028c0321c75e87801c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1009(CardRegistry& r) {
    r.registerCard(1009, std::make_unique<OverloadedTraveler_1009>());
}

}  // namespace riftbound
