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

class EphemeralWard_983 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId self, GameObject&) {
            for (auto id : state.allUnitsControlledBy(controller))
                auraMight(state.getObject(id), self, 2);
        });
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.killObject(ctx.source);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 983;
        d.def_id = R"RB(rad-039-167)RB";
        d.name = R"RB(Ephemeral Ward)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-039/167)RB";
        d.collector_number = 39;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.energy_cost = 4;
        d.power_cost = 2;
        d.might = 0;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(Your units have +2 [M].
When you play a unit, kill this.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d2e562a1b69a42ebd9c8f95a9420750f45aca5dc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_983(CardRegistry& r) {
    r.registerCard(983, std::make_unique<EphemeralWard_983>());
}

}  // namespace riftbound
