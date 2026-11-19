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

class AgonySEmbrace_1071 : public LegendCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayACard; }
    // "From face down" is the trigger condition (CR 383.2.a.1): a play from
    // hand does not put the ability on the chain. "Once each turn"
    // (CR 383.3.e.1): performed once, or still on the chain, a later
    // facedown play does not trigger it.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId,
                          TriggerType trigger, GameObjectId subject) const override {
        if (!playedFromFacedown(state, subject)) return false;
        if (!state.objectExists(source)) return false;
        const auto& c = state.getObject(source).card_counters;
        const auto it = c.find("__agony");
        if (it != c.end() && it->second == state.turn.turn_number + 1) return false;
        return !triggeredAbilityOnChain(state, source, trigger);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.resuming) return;
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).card_counters["__agony"] =
                ctx.state.turn.turn_number + 1;   // +1 so 0 = never
        ctx.executor.drawCards(ctx.controller, 1);
        if (ctx.state.turn.turn_player != ctx.controller) ctx.executor.discardCards(ctx.controller, 1);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1071;
        d.def_id = R"RB(rad-153-167)RB";
        d.name = R"RB(Agony's Embrace)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-153/167)RB";
        d.collector_number = 153;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body, Domain::Chaos};
        d.tags = {R"RB(Evelynn)RB"};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(Once each turn, when you play a card from face down, draw 1. Then, if it's not your turn, discard 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ca217b57d0e8d2e04e7a35105deaa6be17e854f0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1071(CardRegistry& r) {
    r.registerCard(1071, std::make_unique<AgonySEmbrace_1071>());
}

}  // namespace riftbound
