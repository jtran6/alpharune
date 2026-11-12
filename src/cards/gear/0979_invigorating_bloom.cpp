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

class InvigoratingBloom_979 : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        const GameObjectId t = pickTarget(ctx, "Invigorating Bloom: +3 [M]", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        ctx.executor.giveTemporaryMight(t, 3);
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        c.exhaust_friendly_unit = true;
        return c;
    }
    std::vector<GameObjectId> activationExhaustTargets(const GameState& state, PlayerId player,
                                                       GameObjectId) const override {
        return chooseableUnits(state, player, [&](const GameObject& u) {
            return u.controller == player && !u.is_exhausted && u.current_might >= 5;
        });
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 1);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 979;
        d.def_id = R"RB(rad-035-167)RB";
        d.name = R"RB(Invigorating Bloom)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-035/167)RB";
        d.collector_number = 35;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(When you play this, give a friendly unit +3 [M] this turn.
Exhaust a friendly [Mighty] unit, [E]: Draw 1. (A unit is Mighty while it has 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/66d4436ef5d7a7a81c60ee9b1b3ebae06162b7d2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_979(CardRegistry& r) {
    r.registerCard(979, std::make_unique<InvigoratingBloom_979>());
}

}  // namespace riftbound
