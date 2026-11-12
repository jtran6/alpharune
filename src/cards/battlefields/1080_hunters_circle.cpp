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

class HuntersCircle_1080 : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId self, GameObject&) {
            PlayerState::CostModifier unit; unit.source = self; unit.energy_reduction = 1; unit.units_only = true; unit.next_unit_only = true; unit.from_aura = true;
            PlayerState::CostModifier gear; gear.source = self; gear.energy_reduction = 1; gear.gear_only = true; gear.first_gear_per_turn = true; gear.from_aura = true;
            state.player(controller).cost_modifiers.push_back(unit);
            state.player(controller).cost_modifiers.push_back(gear);
        });
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1080;
        d.def_id = R"RB(rad-163-167)RB";
        d.name = R"RB(Hunters' Circle)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-163/167)RB";
        d.collector_number = 163;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(For each player, the first unit or gear they play here on their turn costs [1] less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e7e25d90506d805f1996e7f751cb5b6baa8ec305-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1080(CardRegistry& r) {
    r.registerCard(1080, std::make_unique<HuntersCircle_1080>());
}

}  // namespace riftbound
