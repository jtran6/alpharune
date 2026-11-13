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

class ClangingSentry_995 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId self, GameObject&) {
            for (auto id : state.allUnitsControlledBy(controller)) {
                auto& obj = state.getObject(id);
                bool mech = obj.name == "Mech";
                if (!mech) for (const auto& tag : obj.tags) if (tag == "Mech") mech = true;
                if (mech) auraKeyword(obj, self, Keyword::Disarm, 1);
            }
        });
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 995;
        d.def_id = R"RB(rad-055-167)RB";
        d.name = R"RB(Clanging Sentry)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-055/167)RB";
        d.collector_number = 55;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 2;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(Your Mechs have [Disarm]. (When each of us attacks, give an enemy unit there -1 [M] this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5d4c22ef863e041887a00d34b2dae1f3ce658326-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_995(CardRegistry& r) {
    r.registerCard(995, std::make_unique<ClangingSentry_995>());
}

}  // namespace riftbound
