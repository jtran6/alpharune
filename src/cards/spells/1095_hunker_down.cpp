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

class HunkerDown_1095 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId player,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, player, object);
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        return !chooseableUnits(state, player, [&](const GameObject& u) {
            return u.controller == player;
        }).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        const GameObjectId t = pickTarget(ctx, "Hunker Down: ready a friendly unit", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        ctx.executor.readyObject(t);
        ctx.state.getObject(t).cant_move_this_turn = true;
        ctx.executor.drawCards(ctx.controller, 1);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1095;
        d.def_id = R"RB(rad-034-167)RB";
        d.name = R"RB(Hunker Down)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-034/167)RB";
        d.collector_number = 34;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.energy_cost = 1;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Ready a friendly unit. It can't move this turn. Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/111c518590b17919e5b851ef6c713c7eb7504ae2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1095(CardRegistry& r) {
    r.registerCard(1095, std::make_unique<HunkerDown_1095>());
}

}  // namespace riftbound
