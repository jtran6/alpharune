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

class SpiritOfTheForge_1004 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB&, PlayerId player, GameObjectId self) const override {
        AdditionalPlayCost c; c.exists = true;
        c.payable = !boardObjects(state, [&](const GameObject& o) {
            return o.isGear() && o.controller == player && o.id != self;
        }).empty();
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(CardContext& ctx, const std::function<bool(const PlayCostAdjust&)>&) override {
        auto gears = boardObjects(ctx.state, [&](const GameObject& o) {
            return o.isGear() && o.controller == ctx.controller && o.id != ctx.source;
        });
        ctx.state.getObject(ctx.source).card_counters[kAdditionalCostPaid] = 0;
        if (gears.empty() || !ctx.executor.confirmNow(ctx.controller, "Spirit of the Forge: return a friendly gear"))
            return {};
        auto id = ctx.executor.chooseObjectNow(ctx.controller, gears, "Spirit of the Forge: return");
        if (id == kInvalidId) return {};
        int energy = 0;
        auto def = ctx.state.getObject(id).card_def_id;
        if (def != kInvalidId && ctx.registry && ctx.registry->get(def))
            energy = ctx.registry->get(def)->def().energy_cost;
        ctx.executor.bounceToHand(id);
        ctx.state.getObject(ctx.source).card_counters[kAdditionalCostPaid] = 1;
        ctx.state.getObject(ctx.source).card_counters["__forge_energy"] = energy;
        return {};
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& me = ctx.state.getObject(ctx.source);
        if (me.card_counters[kAdditionalCostPaid] != 1) return;
        ctx.executor.readyObject(ctx.source);
        ctx.executor.giveTemporaryMight(ctx.source, me.card_counters["__forge_energy"]);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1004;
        d.def_id = R"RB(rad-066-167)RB";
        d.name = R"RB(Spirit of the Forge)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-066/167)RB";
        d.collector_number = 66;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(You may return a friendly gear to its owner's hand as an additional cost to play me.
When you play me, if you paid the additional cost, ready me and give me +[M] this turn equal to the Energy cost of the gear you returned.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7a1f527e5910fbc18f7cb3cddf2aa59b483393f4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1004(CardRegistry& r) {
    r.registerCard(1004, std::make_unique<SpiritOfTheForge_1004>());
}

}  // namespace riftbound
