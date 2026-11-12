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

class NtofoStrikes_1120 : public SpellCard {
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
        auto [f, e] = pickTargetPair(ctx, "Ntofo Strikes", friends,
                                     [&](GameObjectId) { return enemies; });
        if (finalizingChoices(ctx)) return;
        const bool waiting = (f == kInvalidId || e == kInvalidId) &&
                             ctx.state.chain.resuming.has_value() &&
                             (ctx.state.chain.resuming->resume_point == 10 ||
                              ctx.state.chain.resuming->resume_point == 12);
        if (waiting) return;
        if (f != kInvalidId) ctx.executor.giveTemporaryKeyword(f, Keyword::Shield, 2);
        if (f != kInvalidId && e != kInvalidId)
            ctx.executor.dealDamage(e, std::max(1, ctx.state.getObject(f).shield_value), ctx.source);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1120;
        d.def_id = R"RB(rad-148-167)RB";
        d.name = R"RB(Ntofo Strikes)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-148/167)RB";
        d.collector_number = 148;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Body};
        d.tags = {R"RB(K'Sante)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Action);
        d.shield_value = 2;
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Give a friendly unit [Shield 2] this turn, then deal damage equal to its [Shield] to an enemy unit. (A unit with Shield has +1 [M] for each instance of Shield while it's a defender.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ab5aa7d29bd49efe2c9da3b62a8065295c189991-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1120(CardRegistry& r) {
    r.registerCard(1120, std::make_unique<NtofoStrikes_1120>());
}

}  // namespace riftbound
