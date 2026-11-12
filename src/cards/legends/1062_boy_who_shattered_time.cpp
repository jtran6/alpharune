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
        ctx.state.getObject(ctx.source).card_counters["__boy_kill"] = 1;
    }
    std::vector<TriggerType> triggerTypes() const override { return {TriggerType::WhenYouPlayAUnit, TriggerType::AtEndOfTurn}; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.state.chain.resuming && ctx.state.chain.resuming->fired_trigger == TriggerType::AtEndOfTurn) {
            auto sub = ctx.state.chain.resuming->triggering_subject;
            if (sub != kInvalidId) ctx.executor.killObject(sub);
            return;
        }
        if (!ctx.state.objectExists(ctx.source) || ctx.state.getObject(ctx.source).card_counters["__boy_kill"] != 1) return;
        ctx.state.getObject(ctx.source).card_counters["__boy_kill"] = 0;
        if (!ctx.state.chain.resuming) return;
        auto sub = ctx.state.chain.resuming->triggering_subject;
        if (sub != kInvalidId) atEndOfTurnAbout(ctx, cardDefId(), sub, 0);
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
