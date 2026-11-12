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

class OneTwoPunch_1112 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId player,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, player, object);
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        const bool mine = !chooseableUnits(state, player, [&](const GameObject& u) {
            return u.controller == player;
        }).empty();
        const bool theirs = !chooseableUnits(state, player, [&](const GameObject& u) {
            return u.controller != player;
        }).empty();
        return mine && theirs;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto friends = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        auto enemies = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller != ctx.controller; });
        // Two targets share one pickTarget slot, so the second call would
        // answer itself. The pair keeps each choice until both are in.
        auto [a, b] = pickTargetPair(ctx, "One-Two Punch", friends,
                                     [&](GameObjectId) { return enemies; });
        if (finalizingChoices(ctx)) return;
        const bool waiting = (a == kInvalidId || b == kInvalidId) &&
                             ctx.state.chain.resuming.has_value() &&
                             (ctx.state.chain.resuming->resume_point == 10 ||
                              ctx.state.chain.resuming->resume_point == 12);
        if (waiting) return;
        if (a != kInvalidId) ctx.executor.buffUnit(a);
        if (b != kInvalidId) ctx.executor.stunUnit(b);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1112;
        d.def_id = R"RB(rad-121-167)RB";
        d.name = R"RB(One-Two Punch)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-121/167)RB";
        d.collector_number = 121;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
[Buff] a friendly unit and [Stun] an enemy unit. (Give the friendly unit a +1 [M] buff if it doesn't have one. The enemy unit doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1f246286c658d44dc4e7d51c158b2f822da39825-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1112(CardRegistry& r) {
    r.registerCard(1112, std::make_unique<OneTwoPunch_1112>());
}

}  // namespace riftbound
