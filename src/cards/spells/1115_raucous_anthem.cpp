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

class RaucousAnthem_1115 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto mine = ctx.state.allUnitsControlledBy(ctx.controller);
        for (auto id : mine) {
            ctx.executor.readyObject(id);
            ctx.executor.giveTemporaryMight(id, 3);
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1115;
        d.def_id = R"RB(rad-134-167)RB";
        d.name = R"RB(Raucous Anthem)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-134/167)RB";
        d.collector_number = 134;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(Ready your units and give them +3 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0c307d5f4785efc4f0a4c8e486b53735f8cf04aa-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1115(CardRegistry& r) {
    r.registerCard(1115, std::make_unique<RaucousAnthem_1115>());
}

}  // namespace riftbound
