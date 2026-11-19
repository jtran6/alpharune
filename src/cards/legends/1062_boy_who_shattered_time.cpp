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

class BoyWhoShatteredTime_1062 : public LegendCard {
public:
    const CardDef& def() const override { return def_; }

    EmpowerSpec empowerSpec() const override {
        EmpowerSpec s; s.valid = true; s.cost.energy = 1; s.cost.exhaust = true; return s;
    }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        std::vector<ActivatedAbility> out;
        appendEmpowerAbilities(out);
        ActivatedAbility kill;
        kill.cost.exhaust = true;
        kill.cost.disempower_self = true;
        out.push_back(kill);
        return out;
    }
    void onActivate(CardContext& ctx, int index, const std::vector<GameObjectId>&) override {
        if (index == 0) return;
        PlayerState::CostModifier mod;
        mod.source = ctx.source;
        mod.energy_reduction = 2;
        mod.power_reduction = 1;
        mod.units_only = true;
        mod.next_unit_only = true;
        ctx.state.player(ctx.controller).cost_modifiers.push_back(mod);
        // The discount is this turn (CostModifier::this_turn_only). The kill
        // is too: the stamp is this turn's number, and it is spent on the
        // next unit. A later turn's unit is not this one.
        ctx.state.getObject(ctx.source).card_counters["__boy_kill"] =
            ctx.state.turn.turn_number + 1;   // +1 so 0 = never
    }
    // Only the unit play. "At the end of your turn" with no unit to kill is
    // not an ability: the kill is the delayed ability queued when the unit
    // is played, and nothing is queued when it is not.
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayAUnit};
    }
    // "The next unit you play this turn." A unit on a later turn, or a
    // second unit while this one is still on the chain, does not trigger.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId,
                          TriggerType trigger, GameObjectId) const override {
        if (trigger != TriggerType::WhenYouPlayAUnit) return true;
        if (!state.objectExists(source)) return false;
        const auto& c = state.getObject(source).card_counters;
        const auto it = c.find("__boy_kill");
        if (it == c.end() || it->second != state.turn.turn_number + 1) return false;
        return !triggeredAbilityOnChain(state, source, trigger);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // The delayed "Kill me at end of turn" (atEndOfTurnAbout). It is the
        // only AtEndOfTurn this card resolves: the legend itself does not
        // trigger then.
        if (ctx.firing_trigger == TriggerType::AtEndOfTurn) {
            if (!ctx.state.chain.resuming) return;
            const auto sub = ctx.state.chain.resuming->triggering_subject;
            if (sub != kInvalidId && ctx.state.objectExists(sub)) ctx.executor.killObject(sub);
            return;
        }
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.state.getObject(ctx.source).card_counters["__boy_kill"] = 0;
        if (!ctx.state.chain.resuming) return;
        const auto sub = ctx.state.chain.resuming->triggering_subject;
        // No unit: do not queue an empty end-of-turn ability.
        if (sub == kInvalidId || !ctx.state.objectExists(sub)) return;
        atEndOfTurnAbout(ctx, cardDefId(), sub, 0);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1062;
        d.def_id = R"RB(rad-139-167)RB";
        d.name = R"RB(Boy Who Shattered Time)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-139/167)RB";
        d.collector_number = 139;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury, Domain::Mind};
        d.tags = {R"RB(Ekko)RB"};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] — [1], [E] (Use only if not Empowered.)
Disempower me, [E]: Reduce the cost of the next unit you play this turn by [2][A] and give it "Kill me at end of turn.")RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8ca3ef446631784ce1d261e30f6a163843ffcb2b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1062(CardRegistry& r) {
    r.registerCard(1062, std::make_unique<BoyWhoShatteredTime_1062>());
}

}  // namespace riftbound
