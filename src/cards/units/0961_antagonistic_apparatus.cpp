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

class AntagonisticApparatus_961 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool entersReadyOnPlay() const override { return true; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source) || !ctx.state.getObject(ctx.source).location.has_value()) return;
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Mech", 3, {"Mech"}, {},
                                 *ctx.state.getObject(ctx.source).location, true);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 961;
        d.def_id = R"RB(rad-009-167)RB";
        d.name = R"RB(Antagonistic Apparatus)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-009/167)RB";
        d.collector_number = 9;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(I enter ready.
When you play me, play a ready 3 [M] Mech unit token here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5be775464c8b897501317c6d32fd60c8b962b5c2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_961(CardRegistry& r) {
    r.registerCard(961, std::make_unique<AntagonisticApparatus_961>());
}

}  // namespace riftbound
