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

class DemacianStandard_1073 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override { return {TriggerType::WhenFriendlyUnitsAttackHere, TriggerType::WhenIHold}; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.state.chain.resuming && ctx.state.chain.resuming->fired_trigger == TriggerType::WhenIHold) {
            int go = confirmOptional(ctx, "Demacian Standard: return this to its owner's hand", []{ return true; });
            if (go < 0) return;
            if (finalizingChoices(ctx)) return;
            if (go == 1) ctx.executor.bounceToHand(ctx.source);
            return;
        }
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            if (u.controller != ctx.controller || !u.location.has_value()) return false;
            if (!ctx.state.objectExists(ctx.source) || !ctx.state.getObject(ctx.source).location.has_value()) return false;
            return *u.location == *ctx.state.getObject(ctx.source).location;
        });
        auto t = pickTarget(ctx, "Demacian Standard: +2 [M]", units);
        if (finalizingChoices(ctx)) return;
        if (t != kInvalidId) ctx.executor.giveTemporaryMight(t, 2);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1073;
        d.def_id = R"RB(rad-156-167)RB";
        d.name = R"RB(Demacian Standard)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-156/167)RB";
        d.collector_number = 156;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Device)RB", R"RB(Jarvan IV)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Deploy);
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
When one or more friendly units attack here, give one of them +2 [M] this turn.
When you hold here, you may return this to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/315b0f7901a83df38b9765f12b1edfdc7de566a3-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1073(CardRegistry& r) {
    r.registerCard(1073, std::make_unique<DemacianStandard_1073>());
}

}  // namespace riftbound
