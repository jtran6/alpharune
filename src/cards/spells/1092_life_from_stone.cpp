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

class LifeFromStone_1092 : public SpellCard {
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
        auto id = ctx.executor.chooseObjectNow(ctx.controller, units, "Life from Stone: exhaust a friendly unit");
        if (id != kInvalidId) ctx.executor.exhaustObject(id);
        return {};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.executor.channelRunes(ctx.controller, 1, true);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1092;
        d.def_id = R"RB(rad-029-167)RB";
        d.name = R"RB(Life from Stone)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-029/167)RB";
        d.collector_number = 29;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.energy_cost = 1;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
As an additional cost to play this, exhaust a friendly unit.
Draw 1 and channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6a849c0425f0fecb8d00e1b9494e975c4dbbba75-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1092(CardRegistry& r) {
    r.registerCard(1092, std::make_unique<LifeFromStone_1092>());
}

}  // namespace riftbound
