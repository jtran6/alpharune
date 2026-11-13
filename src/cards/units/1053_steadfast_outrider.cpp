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

class SteadfastOutrider_1053 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool hasMightDependentAura() const override { return true; }
    void applyMightDependentAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId self, GameObject& me) {
            if (!me.location.has_value()) return;
            int n = 0;
            for (auto& [id, obj] : state.objects.atLocation(*me.location))
                if (id != self && obj.controller == controller && (obj.isUnit() || obj.isGear())) ++n;
            if (n >= 2) auraMight(me, self, 2);
        });
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1053;
        d.def_id = R"RB(rad-128-167)RB";
        d.name = R"RB(Steadfast Outrider)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-128/167)RB";
        d.collector_number = 128;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Tank] (I must be assigned combat damage first.)
While there are 2 or more other friendly units and/or gear at my battlefield, I have +2 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/efa8d58e65b1b1f804d37bb51bb67ea52e0b3205-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1053(CardRegistry& r) {
    r.registerCard(1053, std::make_unique<SteadfastOutrider_1053>());
}

}  // namespace riftbound
