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

class RealmOfDeath_1119 : public SpellCard {
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
        auto enemies = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller != ctx.controller; });
        auto friends = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        auto [e, f] = pickTargetPair(ctx, "Realm of Death", enemies,
                                     [&](GameObjectId) { return friends; });
        if (finalizingChoices(ctx)) return;
        const bool waiting = (e == kInvalidId || f == kInvalidId) &&
                             ctx.state.chain.resuming.has_value() &&
                             (ctx.state.chain.resuming->resume_point == 10 ||
                              ctx.state.chain.resuming->resume_point == 12);
        if (waiting) return;
        if (e != kInvalidId) ctx.executor.giveTemporaryMight(e, -2);
        if (f != kInvalidId) ctx.executor.giveTemporaryMight(f, 2);
        if (e != kInvalidId && f != kInvalidId && ctx.state.objectExists(e) && ctx.state.objectExists(f)) {
            int em = ctx.state.getObject(e).current_might;
            int fm = ctx.state.getObject(f).current_might;
            ctx.executor.dealDamage(f, em, e);
            ctx.executor.dealDamage(e, fm, f);
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1119;
        d.def_id = R"RB(rad-144-167)RB";
        d.name = R"RB(Realm of Death)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-144/167)RB";
        d.collector_number = 144;
        d.artist = R"RB()RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Order};
        d.tags = {R"RB(Mordekaiser)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Epic;

        d.ability_text = R"RB(Give an enemy unit -2 [M] this turn and a friendly unit +2 [M] this turn. They deal damage equal to their Mights to each other.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fd3b4c6939b6a80b33956da7e3a9102eba9c32a8-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1119(CardRegistry& r) {
    r.registerCard(1119, std::make_unique<RealmOfDeath_1119>());
}

}  // namespace riftbound
