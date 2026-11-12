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

class BattlePreparations_1093 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    bool matchesShowOff(const GameState&, const GameObject& obj) const override { return obj.isGear(); }
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
        auto t = pickTarget(ctx, "Battle Preparations: +2 [M]", units);
        if (finalizingChoices(ctx)) return;
        if (t != kInvalidId) ctx.executor.giveTemporaryMight(t, 2);
        if (auto shown = spellShowOff(ctx)) {
            int n = 0;
            for (auto id : ctx.state.runesInBase(ctx.controller)) {
                if (n == 2) break;
                ctx.executor.readyObject(id);
                ++n;
            }
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1093;
        d.def_id = R"RB(rad-032-167)RB";
        d.name = R"RB(Battle Preparations)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-032/167)RB";
        d.collector_number = 32;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::ShowOff);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
[Show Off] a gear. (As you play this, you may reveal a gear from hand or pick a friendly gear.)
Give a unit +2 [M] this turn. If you showed off, ready up to 2 runes.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9422caf2fcd4cfb346cf0dce48245ef13a77180f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1093(CardRegistry& r) {
    r.registerCard(1093, std::make_unique<BattlePreparations_1093>());
}

}  // namespace riftbound
