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

class HearteningSoul_984 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "Ready a friendly legend" targets that legend and is chosen as the
    // trigger is finalized. "Ready your legend" does not, and is Hall of
    // Legends. The [E] is granted to legends, so its unit is chosen as that
    // ability is finalized.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger | kFinalizeActivate; }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility a;
        a.cost.exhaust = true;
        return {a};
    }
    // The ability is the legends', not this unit's. The generator offers a
    // card's own abilities on the unit and the granted copy on each legend.
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (!UnitCard::canActivateAbilityAt(state, controller, source, ability_index)) return false;
        return state.objectExists(source) && state.getObject(source).zone == ZoneType::LegendZone;
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        return !chooseableUnits(state, player, [&](const GameObject& u) {
            return u.controller == player && u.isUnit();
        }).empty();
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto legends = friendlyLegends(ctx.state, ctx.controller);
        auto which = pickTarget(ctx, "Heartening Soul: ready a friendly legend", legends);
        if (ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;
        if (finalizingChoices(ctx)) return;
        if (which != kInvalidId) ctx.executor.readyObject(which);
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.controller == ctx.controller && u.isUnit();
        });
        auto t = pickTarget(ctx, "Heartening Soul: +3 [M] this turn", units);
        if (ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;
        if (finalizingChoices(ctx)) return;
        if (t != kInvalidId) ctx.executor.giveTemporaryMight(t, 3);
    }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject&) {
            for (auto legend : friendlyLegends(state, controller))
                state.getObject(legend).granted_abilities.push_back({cardDefId(), 0});
        });
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 984;
        d.def_id = R"RB(rad-040-167)RB";
        d.name = R"RB(Heartening Soul)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-040/167)RB";
        d.collector_number = 40;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 7;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(When you play me, ready a friendly legend.
Friendly legends have "[E]: Give a unit +3 [M] this turn.")RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0a5f9ab3216462356f6ddf8f172284dca6be21e9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_984(CardRegistry& r) {
    r.registerCard(984, std::make_unique<HearteningSoul_984>());
}

}  // namespace riftbound
