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

class FirebrandRaider_969 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool entersReadyOnPlay() const override { return true; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId self, GameObject&) {
            state.player(controller).units_enter_ready_by.push_back(self);
            state.player(controller).units_skip_awaken_ready = true;
            state.player(opponent(controller)).units_skip_awaken_ready = true;
        });
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<GameObjectId> enemies;
        for (auto id : ctx.state.allUnitsControlledBy(opponent(ctx.controller))) enemies.push_back(id);
        for (auto id : enemies) ctx.executor.readyObject(id);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 969;
        d.def_id = R"RB(rad-019-167)RB";
        d.name = R"RB(Firebrand Raider)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-019/167)RB";
        d.collector_number = 19;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(I enter ready.
When you play me, ready all enemy units.
Other units enter ready.
Units don't ready during the Awaken Phase.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/922ef5a266378fa8ebd7e75bf83435bacd1a75d8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_969(CardRegistry& r) {
    r.registerCard(969, std::make_unique<FirebrandRaider_969>());
}

}  // namespace riftbound
