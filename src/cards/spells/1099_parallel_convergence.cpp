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

class ParallelConvergence_1099 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB&, PlayerId player, GameObjectId) const override {
        AdditionalPlayCost c; c.exists = true; c.mandatory = true;
        c.payable = !chooseableUnits(state, player, [&](const GameObject& u) { return u.controller == player; }).empty();
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(CardContext& ctx, const std::function<bool(const PlayCostAdjust&)>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        auto id = ctx.executor.chooseObjectNow(ctx.controller, units, "Parallel Convergence: return a friendly unit");
        if (id != kInvalidId) ctx.executor.bounceToHand(id);
        return {};
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
        const GameObjectId t = pickTarget(ctx, "Parallel Convergence: deal 5 to a unit at a battlefield", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        ctx.executor.dealDamage(t, 5, ctx.source);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1099;
        d.def_id = R"RB(rad-051-167)RB";
        d.name = R"RB(Parallel Convergence)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-051/167)RB";
        d.collector_number = 51;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Common;

        d.ability_text = R"RB(As an additional cost to play this, return a friendly unit to its owner's hand.
Deal 5 to a unit at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/55e382346c6fa890936a1cf14abaf57b8ee0b8f2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1099(CardRegistry& r) {
    r.registerCard(1099, std::make_unique<ParallelConvergence_1099>());
}

}  // namespace riftbound
