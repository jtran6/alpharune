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

class ShimmeringSwimmer_964 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB&, PlayerId player, GameObjectId) const override {
        AdditionalPlayCost c; c.exists = true;
        c.payable = !boardObjects(state, [&](const GameObject& o) {
            return o.controller == player && (o.isUnit() || o.isGear());
        }).empty();
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(CardContext& ctx, const std::function<bool(const PlayCostAdjust&)>&) override {
        auto opts = boardObjects(ctx.state, [&](const GameObject& o) {
            return o.controller == ctx.controller && o.id != ctx.source && (o.isUnit() || o.isGear());
        });
        auto& me = ctx.state.getObject(ctx.source);
        me.card_counters[kAdditionalCostPaid] = 0;
        if (opts.empty() || !ctx.executor.confirmNow(ctx.controller, "Shimmering Swimmer: kill a friendly unit or gear"))
            return {};
        auto id = ctx.executor.chooseObjectNow(ctx.controller, opts, "Shimmering Swimmer: kill");
        if (id == kInvalidId || !killAsCost(ctx, id)) return {};
        me.card_counters[kAdditionalCostPaid] = 1;
        return {};
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; });
        const GameObjectId t = pickTarget(ctx, "Shimmering Swimmer: deal 2", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        if (!ctx.state.objectExists(ctx.source)) return;
        auto it = ctx.state.getObject(ctx.source).card_counters.find(kAdditionalCostPaid);
        if (it == ctx.state.getObject(ctx.source).card_counters.end() || it->second != 1) return;
        ctx.executor.dealDamage(t, 2, ctx.source);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 964;
        d.def_id = R"RB(rad-014-167)RB";
        d.name = R"RB(Shimmering Swimmer)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-014/167)RB";
        d.collector_number = 14;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(You may kill a friendly unit or gear as an additional cost to play me.
When you play me, if you paid the additional cost, deal 2 to a unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/02ef2a5c93cf60048bb932a709d3a5b17b93bb1d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_964(CardRegistry& r) {
    r.registerCard(964, std::make_unique<ShimmeringSwimmer_964>());
}

}  // namespace riftbound
