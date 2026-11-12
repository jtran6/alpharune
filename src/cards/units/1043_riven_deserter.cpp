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

class RivenDeserter_1043 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyUnitBecomesExhausted; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.resuming || ctx.state.chain.resuming->triggering_subject == ctx.source) return;
        ctx.executor.giveTemporaryMight(ctx.source, 1);
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { ActivationCost c; return c; }
    // The engine stamps __ability_used_turn_0 when the activation is paid.
    // "Exhaust a friendly unit" is the cost: with nobody ready the ability is
    // not legal, and a second activation the same turn is not offered.
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (!UnitCard::canActivateAbilityAt(state, controller, source, ability_index)) return false;
        if (!state.objectExists(source)) return false;
        const auto& self = state.getObject(source);
        auto used = self.card_counters.find("__ability_used_turn_0");
        if (used != self.card_counters.end() && used->second == state.turn.turn_number + 1) return false;
        return !chooseableUnits(state, controller, [&](const GameObject& u) {
            return u.controller == controller && !u.is_exhausted;
        }).empty();
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.state.objectExists(ctx.source) &&
            ctx.state.getObject(ctx.source).card_counters["__riven_d"] == ctx.state.turn.turn_number) return;
        auto friends = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.controller == ctx.controller && !u.is_exhausted;
        });
        auto any = chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; });
        auto [cost, t] = pickTargetPair(ctx, "Riven, Deserter", friends,
                                        [&](GameObjectId) { return any; });
        const bool waiting = (cost == kInvalidId || t == kInvalidId) &&
                             ctx.state.chain.resuming.has_value() &&
                             (ctx.state.chain.resuming->resume_point == 10 ||
                              ctx.state.chain.resuming->resume_point == 12);
        if (waiting) return;
        if (cost == kInvalidId) return;
        ctx.executor.exhaustObject(cost);
        if (t != kInvalidId) ctx.executor.readyObject(t);
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).card_counters["__riven_d"] = ctx.state.turn.turn_number;
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1043;
        d.def_id = R"RB(rad-114-167)RB";
        d.name = R"RB(Riven, Deserter)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-114/167)RB";
        d.collector_number = 114;
        d.artist = R"RB()RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Riven)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Epic;

        d.ability_text = R"RB(When another friendly unit becomes exhausted, give me +1 [M] this turn.
Exhaust a friendly unit: Ready a unit. Use this ability only once each turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/59c080321a697164c9eeb6f0ad297ff0d0b85808-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1043(CardRegistry& r) {
    r.registerCard(1043, std::make_unique<RivenDeserter_1043>());
}

}  // namespace riftbound
