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

class OriannaMechanicalMarvel_1006 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId self, GameObject& me) {
            if (!me.location.has_value()) return;
            PlayerState::CostModifier mod;
            mod.source = self;
            mod.energy_reduction = 1;
            mod.power_reduction = 1;
            mod.gear_only = true;
            mod.first_gear_per_turn = true;
            state.player(controller).cost_modifiers.push_back(mod);
        });
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1006;
        d.def_id = R"RB(rad-068-167)RB";
        d.name = R"RB(Orianna, Mechanical Marvel)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-068/167)RB";
        d.collector_number = 68;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Piltover)RB", R"RB(Orianna)RB"};
        d.energy_cost = 5;
        d.power_cost = 0;
        d.might = 5;
        d.rarity = Rarity::Epic;

        d.ability_text = R"RB(Once each turn, while I'm at a battlefield, you may pay [1][A] less to play a gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/00f5a081a085e9c3851daf79d9e39bd378f71fe3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1006(CardRegistry& r) {
    r.registerCard(1006, std::make_unique<OriannaMechanicalMarvel_1006>());
}

}  // namespace riftbound
