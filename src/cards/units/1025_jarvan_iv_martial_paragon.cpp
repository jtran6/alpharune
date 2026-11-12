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

class JarvanIVMartialParagon_1025 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject&) {
            state.player(controller).deploy_gear_have_action = true;
        });
    }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source) || !ctx.state.getObject(ctx.source).location.has_value()) return;
        LocationId here = *ctx.state.getObject(ctx.source).location;
        for (auto& [id, obj] : ctx.state.objects.atLocation(here)) {
            if (obj.isGear() && obj.controller == ctx.controller && obj.hasKeyword(Keyword::Deploy)) {
                ctx.executor.channelRunes(ctx.controller, 1, true);
                return;
            }
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1025;
        d.def_id = R"RB(rad-091-167)RB";
        d.name = R"RB(Jarvan IV, Martial Paragon)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-091/167)RB";
        d.collector_number = 91;
        d.artist = R"RB(JunHuan)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Demacia)RB", R"RB(Jarvan IV)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Your gear with [Deploy] have [Action]. (They can be played on your turn or in showdowns.)
When I conquer, if there is a friendly gear with [Deploy] here, channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8edacba51db193506e13e1afb6a8509256cdf55c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1025(CardRegistry& r) {
    r.registerCard(1025, std::make_unique<JarvanIVMartialParagon_1025>());
}

}  // namespace riftbound
