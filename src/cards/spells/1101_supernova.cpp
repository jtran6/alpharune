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

class Supernova_1101 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
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
        const GameObjectId t = pickTarget(ctx, "Supernova: deal 10", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        ctx.executor.dealDamage(t, 10, ctx.source);
        ctx.executor.drawCards(ctx.controller, 2);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1101;
        d.def_id = R"RB(rad-059-167)RB";
        d.name = R"RB(Supernova)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-059/167)RB";
        d.collector_number = 59;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};
        d.energy_cost = 9;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(Deal 10 to a unit. Draw 2.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bea8e402f99b0afb5af1b19c0ee8ea34052e736f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1101(CardRegistry& r) {
    r.registerCard(1101, std::make_unique<Supernova_1101>());
}

}  // namespace riftbound
