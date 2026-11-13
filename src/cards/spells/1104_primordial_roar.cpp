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

class PrimordialRoar_1104 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    bool matchesShowOff(const GameState&, const GameObject& obj) const override { return obj.isUnit(); }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId player,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, player, object);
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        return !chooseableUnits(state, player, [](const GameObject&) { return true; }).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto shown = spellShowOff(ctx);
        auto units = chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; });
        auto t = pickTarget(ctx, "Primordial Roar: deal the shown unit's Might", units);
        if (finalizingChoices(ctx)) return;
        if (shown && shown->did && t != kInvalidId) ctx.executor.dealDamage(t, shown->might, ctx.source);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1104;
        d.def_id = R"RB(rad-082-167)RB";
        d.name = R"RB(Primordial Roar)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-082/167)RB";
        d.collector_number = 82;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {};
        d.energy_cost = 4;
        d.power_cost = 2;
        d.might = 0;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::ShowOff);
        d.ability_text = R"RB([Show Off] a unit. (As you play this, you may reveal a unit from hand or pick a friendly unit.)
If you showed off a unit, deal damage equal to that unit's Might to a unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/945633b0c2ed7f303245c917ffbcdb80dcfb524e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1104(CardRegistry& r) {
    r.registerCard(1104, std::make_unique<PrimordialRoar_1104>());
}

}  // namespace riftbound
