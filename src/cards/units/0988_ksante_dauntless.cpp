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

class KSanteDauntless_988 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override { return {TriggerType::WhenIAttack, TriggerType::WhenIDefend}; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            if (u.controller != ctx.controller || !u.location.has_value()) return false;
            if (!ctx.state.objectExists(ctx.source) || !ctx.state.getObject(ctx.source).location.has_value()) return false;
            return *u.location == *ctx.state.getObject(ctx.source).location && u.hasKeyword(Keyword::Shield);
        });
        auto t = pickTarget(ctx, "K'Sante: double a unit's Shield this turn", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        int shield = std::max(1, ctx.state.getObject(t).shield_value);
        ctx.executor.giveTemporaryKeyword(t, Keyword::Shield, shield);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 988;
        d.def_id = R"RB(rad-045-167)RB";
        d.name = R"RB(K'Sante, Dauntless)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-045/167)RB";
        d.collector_number = 45;
        d.artist = R"RB()RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Shurima)RB", R"RB(K'Sante)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Shield);
        d.shield_value = 1;
        d.ability_text = R"RB([Shield] (+1 [M] while I'm a defender.)
When I attack or defend, double the [Shield] of a unit you control here this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f56e9f9db199f915e32ea264b7dab70202f97957-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_988(CardRegistry& r) {
    r.registerCard(988, std::make_unique<KSanteDauntless_988>());
}

}  // namespace riftbound
