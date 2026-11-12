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

class ExemplarOfDemacia_1072 : public LegendCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAGear; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.resuming) { if (finalizingChoices(ctx)) return; return; }
        auto sub = ctx.state.chain.resuming->triggering_subject;
        if (sub == kInvalidId || !ctx.state.objectExists(sub) || !ctx.state.getObject(sub).hasKeyword(Keyword::Deploy)) {
            if (finalizingChoices(ctx)) return;
            return;
        }
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        const bool ready = ctx.state.objectExists(ctx.source) &&
                           !ctx.state.getObject(ctx.source).is_exhausted;
        const bool can_pay = ready && ctx.executor.canPayAnyPower(ctx.controller, 1);
        int go = confirmOptional(ctx, "Exemplar of Demacia: exhaust me and pay [A] to move a unit there",
                                 [ok=!units.empty() && can_pay]{ return ok; });
        if (go < 0) return;
        GameObjectId t = kInvalidId;
        if (go == 1) {
            t = pickTarget(ctx, "Exemplar: move a friendly unit", units);
            if (ctx.state.chain.resuming && ctx.state.chain.resuming->resume_point == 7) return;
            if (costDueNow(ctx)) {
                BattlefieldId there = kInvalidId;
                if (ctx.state.objectExists(sub) && ctx.state.getObject(sub).location.has_value()) {
                    if (auto* bf = std::get_if<BattlefieldLocation>(&*ctx.state.getObject(sub).location))
                        there = bf->id;
                }
                auto& ri = *ctx.state.chain.resuming;
                while (ri.resume_data.size() <= 17) ri.resume_data.push_back(0);
                ri.resume_data[16] = there.valid() ? there.raw() + 1 : 0;
                const bool legend_ready = ctx.state.objectExists(ctx.source) &&
                                          !ctx.state.getObject(ctx.source).is_exhausted;
                const bool paid = t != kInvalidId && there.valid() && legend_ready &&
                    ctx.executor.payResources(ctx.controller, 0, 1, resource::kAnyDomain, SpendFor::Other);
                if (!paid) markCostUnpaid(ctx);
                else ctx.executor.exhaustObject(ctx.source);
            }
        }
        if (finalizingChoices(ctx)) return;
        int locked = 0;
        if (ctx.state.chain.resuming && ctx.state.chain.resuming->resume_data.size() > 16)
            locked = ctx.state.chain.resuming->resume_data[16];
        if (go != 1 || costUnpaid(ctx) || t == kInvalidId || locked <= 0) return;
        ctx.executor.moveToBattlefield(t, BattlefieldId::fromRaw(locked - 1));
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1072;
        d.def_id = R"RB(rad-155-167)RB";
        d.name = R"RB(Exemplar of Demacia)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-155/167)RB";
        d.collector_number = 155;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Jarvan IV)RB"};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play a gear with [Deploy], you may exhaust me and pay [A] to move a friendly unit to that battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e85c005f938e44655f926bb406a47359ae51ecc1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1072(CardRegistry& r) {
    r.registerCard(1072, std::make_unique<ExemplarOfDemacia_1072>());
}

}  // namespace riftbound
