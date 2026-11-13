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

class KiBurst_1091 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB&, PlayerId player, GameObjectId) const override {
        AdditionalPlayCost c; c.exists = true; c.mandatory = true;
        c.payable = !chooseableUnits(state, player, [&](const GameObject& u) {
            return u.controller == player && !u.is_exhausted;
        }).empty();
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(CardContext& ctx, const std::function<bool(const PlayCostAdjust&)>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.controller == ctx.controller && !u.is_exhausted;
        });
        auto id = ctx.executor.chooseObjectNow(ctx.controller, units, "Ki Burst: exhaust a friendly unit");
        if (id != kInvalidId) ctx.executor.exhaustObject(id);
        return {};
    }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId player,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, player, object);
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        return !chooseableUnits(state, player, [](const GameObject&) { return true; }).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; });
        const GameObjectId t = pickTarget(ctx, "Ki Burst: stun a unit", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        ctx.executor.stunUnit(t);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1091;
        d.def_id = R"RB(rad-028-167)RB";
        d.name = R"RB(Ki Burst)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-028/167)RB";
        d.collector_number = 28;
        d.artist = R"RB()RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.energy_cost = 1;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
As an additional cost to play this, exhaust a friendly unit.
[Stun] a unit. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/33af916b50007c01b33e78aef3dc99b757edc3f2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1091(CardRegistry& r) {
    r.registerCard(1091, std::make_unique<KiBurst_1091>());
}

}  // namespace riftbound
