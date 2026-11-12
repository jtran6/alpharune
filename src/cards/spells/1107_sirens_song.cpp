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

class SirenSSong_1107 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId, GameObjectId) const override {
        return FinalizeChoice::No;
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        return !chooseableUnits(state, player, [&](const GameObject& u) {
            return u.controller != player && u.moves_this_turn > 0;
        }).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.controller != ctx.controller && u.moves_this_turn > 0;
        });
        const GameObjectId t = pickTarget(ctx, "Siren's Song: an enemy that moved this turn", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        if (ctx.state.getObject(t).location.has_value() && std::holds_alternative<BattlefieldLocation>(*ctx.state.getObject(t).location))
            ctx.executor.moveToBase(t);
        else if (!ctx.state.battlefields.empty()) ctx.executor.moveToBattlefield(t, ctx.state.battlefields[0].id);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1107;
        d.def_id = R"RB(rad-098-167)RB";
        d.name = R"RB(Siren's Song)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-098/167)RB";
        d.collector_number = 98;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Move an enemy unit that moved this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/742967b3e85f51b242fc04b8db4c4e3cd7b88672-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1107(CardRegistry& r) {
    r.registerCard(1107, std::make_unique<SirenSSong_1107>());
}

}  // namespace riftbound
