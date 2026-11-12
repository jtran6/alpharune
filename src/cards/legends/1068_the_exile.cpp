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

class TheExile_1068 : public LegendCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject&) {
            state.player(controller).units_enter_ready_at_your_bf = true;
        });
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1068;
        d.def_id = R"RB(rad-149-167)RB";
        d.name = R"RB(The Exile)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-149/167)RB";
        d.collector_number = 149;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm, Domain::Chaos};
        d.tags = {R"RB(Riven)RB"};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(Units you play to battlefields you control enter ready.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/342befcaab98642a2348bd2f6fdb92c68563b6d7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1068(CardRegistry& r) {
    r.registerCard(1068, std::make_unique<TheExile_1068>());
}

}  // namespace riftbound
