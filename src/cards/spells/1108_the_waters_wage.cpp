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

class TheWatersWage_1108 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        bool facedown = playedFromFacedown(ctx.state, ctx.source);
        int n = (facedown && ctx.state.turn.turn_player == ctx.controller) ? 3 : 1;
        for (int i = 0; i < n; ++i) {
            auto id = createGoldExhausted(ctx);
            if (n == 3 && id != kInvalidId) ctx.executor.readyObject(id);
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1108;
        d.def_id = R"RB(rad-100-167)RB";
        d.name = R"RB(The Waters' Wage)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-100/167)RB";
        d.collector_number = 100;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 1;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
Play a Gold gear token exhausted. If it's your turn and you played this from face down, play three ready Gold gear tokens instead. (Gold tokens have "[Reaction][>] Kill this, [E]: [Add] [A]."))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0552932562db1dd48f14c74779999a645d62cc0d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1108(CardRegistry& r) {
    r.registerCard(1108, std::make_unique<TheWatersWage_1108>());
}

}  // namespace riftbound
