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

class BushwhackTrap_1037 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject& me) {
            if (!me.location.has_value()) return;
            if (auto* bf = std::get_if<BattlefieldLocation>(&*me.location)) {
                auto& slot = state.battlefields[bf->id.raw()];
                if (slot.controller.has_value() && *slot.controller == controller) slot.extra_facedown += 1;
            }
        });
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1037;
        d.def_id = R"RB(rad-108-167)RB";
        d.name = R"RB(Bushwhack Trap)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-108/167)RB";
        d.collector_number = 108;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Device)RB"};
        d.energy_cost = 1;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Deploy);
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
While you or an ally control this battlefield, that player may hide an additional card here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/120762e23a599ee14d3f88856fcf707154338fb0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1037(CardRegistry& r) {
    r.registerCard(1037, std::make_unique<BushwhackTrap_1037>());
}

}  // namespace riftbound
