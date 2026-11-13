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

class HoldEm_1102 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    int selfCostReduction(const GameState& state, PlayerId player) const override {
        return state.player(player).played_to_battlefield_this_turn > 0 ? 2 : 0;
    }
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
        const GameObjectId t = pickTarget(ctx, "Hold 'Em: +3 [M]", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        ctx.executor.giveTemporaryMight(t, 3);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1102;
        d.def_id = R"RB(rad-070-167)RB";
        d.name = R"RB(Hold 'Em)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-070/167)RB";
        d.collector_number = 70;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
This costs [2] less if you've played a unit or gear to a battlefield this turn.
Give a unit +3 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0d9af22d38d32b0ec2a6584d25abe201730f9e11-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1102(CardRegistry& r) {
    r.registerCard(1102, std::make_unique<HoldEm_1102>());
}

}  // namespace riftbound
