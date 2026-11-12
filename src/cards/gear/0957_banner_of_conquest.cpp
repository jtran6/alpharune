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

class BannerOfConquest_957 : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId self, GameObject& me) {
            if (!me.location.has_value()) return;
            for (auto& [id, obj] : state.objects.atLocation(*me.location)) {
                if (!(obj.isUnit() && obj.controller == controller)) continue;
                auraKeyword(obj, self, Keyword::Assault, 1);
            }
        });
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 957;
        d.def_id = R"RB(rad-002-167)RB";
        d.name = R"RB(Banner of Conquest)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-002/167)RB";
        d.collector_number = 2;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Device)RB"};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Deploy);
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
Friendly units here have [Assault]. (+1 [M] while they're attackers.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1e2a4e0a8cf533c82e9c3f4d507dd7eb63ece943-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_957(CardRegistry& r) {
    r.registerCard(957, std::make_unique<BannerOfConquest_957>());
}

}  // namespace riftbound
