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

class Brawl_1085 : public SpellCard {
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
        const GameObjectId t = pickTarget(ctx, "Brawl: a unit", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        int might = ctx.state.getObject(t).current_might;
        ctx.executor.giveTemporaryKeyword(t, Keyword::Assault, std::max(1, might));
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1085;
        d.def_id = R"RB(rad-004-167)RB";
        d.name = R"RB(Brawl)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-004/167)RB";
        d.collector_number = 4;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Action);
        d.assault_value = 1;
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Give a unit [Assault] this turn equal to its Might. (+1 [M] for each instance of Assault while it's an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/38fd13c7522bb47dcc5ce1596aeb6a638087ee09-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1085(CardRegistry& r) {
    r.registerCard(1085, std::make_unique<Brawl_1085>());
}

}  // namespace riftbound
