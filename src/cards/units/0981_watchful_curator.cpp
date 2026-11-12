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

class WatchfulCurator_981 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId self, GameObject&) {
            for (auto& [id, obj] : state.objects.onBoard()) {
                if (obj.controller == controller && obj.isGear()) auraKeyword(obj, self, Keyword::Deflect, 1);
            }
        });
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 981;
        d.def_id = R"RB(rad-037-167)RB";
        d.name = R"RB(Watchful Curator)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-037/167)RB";
        d.collector_number = 37;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Uncommon;

        d.deflect_value = 1;
        d.ability_text = R"RB(Your gear have [Deflect]. (Opponents must pay [A] to choose each with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5081f81341bb02dfd26c970c09651fcf64a18a04-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_981(CardRegistry& r) {
    r.registerCard(981, std::make_unique<WatchfulCurator_981>());
}

}  // namespace riftbound
