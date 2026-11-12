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

class ConsultTheHeavens_1090 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId player,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, player, object);
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        return !chooseableUnits(state, player, [](const GameObject&) { return true; }).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; });
        const GameObjectId t = pickTarget(ctx, "Consult the Heavens: Shield 2 this turn", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        ctx.executor.giveTemporaryKeyword(t, Keyword::Shield, 2);
        ctx.executor.predict(ctx.controller, 1);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1090;
        d.def_id = R"RB(rad-025-167)RB";
        d.name = R"RB(Consult the Heavens)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-025/167)RB";
        d.collector_number = 25;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.energy_cost = 1;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Reaction);
        d.shield_value = 2;
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Give a unit [Shield 2] this turn. (+2 [M] while it's a defender.)
[Predict]. (Look at the top card of your Main Deck. You may recycle it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/120b20ee26d221cea31f510090cef037e74c023f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1090(CardRegistry& r) {
    r.registerCard(1090, std::make_unique<ConsultTheHeavens_1090>());
}

}  // namespace riftbound
