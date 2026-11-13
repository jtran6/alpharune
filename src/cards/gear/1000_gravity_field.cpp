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

class GravityField_1000 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject& me) {
            if (!me.location.has_value()) return;
            if (auto* bf = std::get_if<BattlefieldLocation>(&*me.location)) {
                auto& slot = state.battlefields[bf->id.raw()];
                slot.arrival_energy = 1;
                slot.arrival_power = 1;
                slot.arrival_exempt = static_cast<int>(controller);
            }
        });
    }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 1);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1000;
        d.def_id = R"RB(rad-062-167)RB";
        d.name = R"RB(Gravity Field)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-062/167)RB";
        d.collector_number = 62;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Device)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Deploy);
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
Opponents must pay [1][A] to move one or more units to this battlefield.
[Deathknell][>] Draw 1. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/51f589e5ed0f7316b6cb64ebc93c4ef3e75b7f46-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1000(CardRegistry& r) {
    r.registerCard(1000, std::make_unique<GravityField_1000>());
}

}  // namespace riftbound
