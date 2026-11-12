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

class PackedAmphitheater_1081 : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId self, GameObject& me) {
            if (!me.location.has_value()) return;
            for (auto& [id, obj] : state.objects.atLocation(*me.location))
                if (obj.isUnit() && obj.isChampion()) auraMight(obj, self, 1);
        });
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1081;
        d.def_id = R"RB(rad-164-167)RB";
        d.name = R"RB(Packed Amphitheater)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-164/167)RB";
        d.collector_number = 164;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(Champion units here have +1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f4b396780dd7a5aa1be43f763265f308ee0fe665-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1081(CardRegistry& r) {
    r.registerCard(1081, std::make_unique<PackedAmphitheater_1081>());
}

}  // namespace riftbound
