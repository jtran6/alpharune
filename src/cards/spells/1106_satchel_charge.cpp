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

class SatchelCharge_1106 : public SpellCard {
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
        auto t = pickTarget(ctx, "Satchel Charge: move a friendly unit", units);
        if (finalizingChoices(ctx)) return;
        // The Bomb's battlefield is chosen as that token is played, after
        // this marker (CR 420.3.b.2). That prompt re-enters this callback,
        // so the move is stamped the first time and not run again.
        bool already = false;
        if (ctx.state.chain.resuming.has_value()) {
            auto& rd = ctx.state.chain.resuming->resume_data;
            while (rd.size() <= 20) rd.push_back(0);
            already = rd[20] != 0;
            rd[20] = 1;
        }
        if (!already && t != kInvalidId && ctx.state.objectExists(t) &&
            ctx.state.getObject(t).location.has_value()) {
            if (std::holds_alternative<BaseLocation>(*ctx.state.getObject(t).location)) {
                if (!ctx.state.battlefields.empty()) ctx.executor.moveToBattlefield(t, ctx.state.battlefields[0].id);
            } else ctx.executor.moveToBase(t);
        }
        playBombToken(*this, ctx, "Satchel Charge: play a Bomb");
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1106;
        d.def_id = R"RB(rad-096-167)RB";
        d.name = R"RB(Satchel Charge)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-096/167)RB";
        d.collector_number = 96;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Move a friendly unit, then play a Bomb gear token to a battlefield. (It has [Deploy]. When it dies, deal 2 to an enemy there and kill your other Bombs there.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b07c26438f600b87c92e4ed778a1c8b073b47f5c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1106(CardRegistry& r) {
    r.registerCard(1106, std::make_unique<SatchelCharge_1106>());
}

}  // namespace riftbound
