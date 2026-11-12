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
        int go = confirmOptional(ctx, "Exemplar of Demacia: exhaust me and pay [A] to move a unit there", [ok=!units.empty()]{ return ok; });
        if (go < 0) return;
        GameObjectId t = kInvalidId;
        BattlefieldId dest = kInvalidId;
        if (go == 1) {
            t = pickTarget(ctx, "Exemplar: move a friendly unit", units);
            if (ctx.state.chain.resuming && ctx.state.chain.resuming->resume_point == 7) return;
            if (ctx.state.objectExists(sub) && ctx.state.getObject(sub).location.has_value()) {
                if (auto* bf = std::get_if<BattlefieldLocation>(&*ctx.state.getObject(sub).location))
                    dest = bf->id;
            }
            if (costDueNow(ctx)) {
                const bool paid = t != kInvalidId && dest.valid() &&
                    ctx.executor.payResources(ctx.controller, 0, 1, resource::kAnyDomain, SpendFor::Other);
                if (!paid) markCostUnpaid(ctx);
                else ctx.executor.exhaustObject(ctx.source);
            }
        }
        if (finalizingChoices(ctx)) return;
        if (go != 1 || costUnpaid(ctx) || t == kInvalidId || !dest.valid()) return;
        ctx.executor.moveToBattlefield(t, dest);
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
