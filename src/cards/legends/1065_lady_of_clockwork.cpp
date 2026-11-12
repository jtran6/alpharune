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

class LadyOfClockwork_1065 : public LegendCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::AtStartOfMain; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto gears = boardObjects(ctx.state, [&](const GameObject& o) { return o.isGear() && o.controller == ctx.controller; });
        const bool can_pay = canPayEnergy(ctx, ctx.controller, 1);
        int go = confirmOptional(ctx, "Lady of Clockwork: pay [1] to make a gear a unit",
                                 [ok=!gears.empty() && can_pay]{ return ok; });
        if (go < 0) return;
        GameObjectId t = kInvalidId;
        if (go == 1) {
            t = pickTarget(ctx, "Lady of Clockwork: a friendly gear", gears);
            if (ctx.state.chain.resuming && ctx.state.chain.resuming->resume_point == 7) return;
            if (costDueNow(ctx)) {
                if (t == kInvalidId ||
                    !ctx.executor.payResources(ctx.controller, 1, 0, resource::kAnyDomain, SpendFor::Other)) {
                    markCostUnpaid(ctx);
                } else if (ctx.state.objectExists(ctx.source)) {
                    int prev = ctx.state.getObject(ctx.source).card_counters["__clock_gear"];
                    if (prev > 0) {
                        GameObjectId old = GameObjectId::fromRaw(prev);
                        if (ctx.state.objectExists(old) && ctx.state.getObject(old).is_unit_gear) {
                            auto& was = ctx.state.getObject(old);
                            was.is_unit_gear = false;
                            if (was.card_def_id != kInvalidId && ctx.registry &&
                                ctx.registry->get(was.card_def_id))
                                was.base_might = ctx.registry->get(was.card_def_id)->def().might;
                            was.recomputeMight();
                        }
                    }
                }
            }
        }
        if (finalizingChoices(ctx)) return;
        if (go != 1 || costUnpaid(ctx) || t == kInvalidId || !ctx.state.objectExists(t)) return;
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).card_counters["__clock_gear"] = t.raw();
        auto& gear = ctx.state.getObject(t);
        if (gear.attached_to.has_value()) ctx.executor.unattachGear(t);
        int energy = 0;
        if (gear.card_def_id != kInvalidId && ctx.registry && ctx.registry->get(gear.card_def_id))
            energy = ctx.registry->get(gear.card_def_id)->def().energy_cost;
        gear.is_unit_gear = true;
        gear.base_might = energy;
        gear.recomputeMight();
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1065;
        d.def_id = R"RB(rad-145-167)RB";
        d.name = R"RB(Lady of Clockwork)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-145/167)RB";
        d.collector_number = 145;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm, Domain::Mind};
        d.tags = {R"RB(Orianna)RB"};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(At the start of your Main Phase, you may pay [1] to choose a friendly gear. It becomes a unit gear with Might equal to its Energy cost until you choose another gear with me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d1f940425d4d913d8f56c017254343bc898f4892-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1065(CardRegistry& r) {
    r.registerCard(1065, std::make_unique<LadyOfClockwork_1065>());
}

}  // namespace riftbound
