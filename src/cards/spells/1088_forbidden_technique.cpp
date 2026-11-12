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

class ForbiddenTechnique_1088 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId player,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, player, object);
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        return !chooseableUnits(state, player, [](const GameObject&) { return true; }).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; });
        const GameObjectId t = pickTarget(ctx, "Forbidden Technique: ready a unit", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        ctx.executor.readyObject(t);
        if (ctx.state.objectExists(ctx.source) &&
            ctx.state.getObject(ctx.source).card_counters["__ft_banish"] != 0)
            ctx.executor.banishObject(ctx.source);
    }

    bool triggersWhileInTrash(TriggerType t) const override {
        return t == TriggerType::WhenYouPlayACard;
    }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayACard};
    }
    bool triggerConditionOn(const GameState& state, const TriggerEvent& event) const override {
        if (event.trigger != TriggerType::WhenYouPlayACard) return false;
        if (!event.subject_card || event.subject_card->energy_cost < 7) return false;
        return state.objectExists(event.source) &&
               state.getObject(event.source).zone == ZoneType::Trash;
    }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve | kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        int go = confirmOptional(ctx, "Forbidden Technique: play this from your trash for [1] and 2 Fury?",
                                 [&] {
                                     if (!ctx.state.objectExists(ctx.source) ||
                                         ctx.state.getObject(ctx.source).zone != ZoneType::Trash)
                                         return false;
                                     if (!hasLegalTargets(ctx.state, ctx.controller)) return false;
                                     return ctx.executor.canPayWithAdds(
                                         ctx.controller, 1, 2, resource::maskOf(Domain::Fury), SpendFor::Spell);
                                 });
        if (go < 0) return;
        if (go == 1 && costDueNow(ctx)) {
            const bool paid = ctx.executor.payResources(
                ctx.controller, 1, 2, resource::maskOf(Domain::Fury), SpendFor::Spell);
            if (!paid) markCostUnpaid(ctx);
        }
        if (finalizingChoices(ctx)) return;
        if (go != 1 || costUnpaid(ctx)) return;
        if (!ctx.executor.playIgnoringCost(ctx.controller, ctx.source, std::nullopt)) return;
        // playIgnoringCost forgets per-play stamps first. The banish belongs
        // to this play, so it is stamped after that, and cleared once the
        // spell resolves.
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).card_counters["__ft_banish"] = 1;
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1088;
        d.def_id = R"RB(rad-010-167)RB";
        d.name = R"RB(Forbidden Technique)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-010/167)RB";
        d.collector_number = 10;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(Ready a unit.
When you play a card with Energy cost [7] or more, you may play this from your trash for [1][R][R]. Then banish this.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/293518dcf6c2f9e3f59a2e259826fd813436b3a5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1088(CardRegistry& r) {
    r.registerCard(1088, std::make_unique<ForbiddenTechnique_1088>());
}

}  // namespace riftbound
