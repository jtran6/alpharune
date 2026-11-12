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

class CommandAttack_1094 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId, GameObjectId) const override {
        return FinalizeChoice::No;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto gears = boardObjects(ctx.state, [&](const GameObject& o) {
            return o.isGear() && o.controller == ctx.controller && o.hasKeyword(Keyword::Deploy);
        });
        // Mode 0 needs a Deploy gear and a battlefield. With neither, only
        // the look is offered, so the spell is not played into a required
        // choice that has no option.
        uint32_t modes = 2u;
        if (!gears.empty() && !ctx.state.battlefields.empty()) modes |= 1u;
        int mode = pickMode(ctx, "Command: Attack", 2,
                            {"Move a friendly Deploy gear", "Look at the top 5"}, modes);
        if (mode < 0) return;
        GameObjectId gear = kInvalidId;
        int where = -1;
        if (mode == 0) {
            gear = pickTarget(ctx, "Command: Attack: a Deploy gear", gears);
            if (gear == kInvalidId) return;
            std::vector<std::string> labels;
            for (const auto& bf : ctx.state.battlefields)
                labels.push_back(battlefieldLabel(ctx.state, bf));
            auto w = pickValueAs(ctx, ctx.controller, "Command: Attack: a battlefield",
                                 labels, PromptKind::Location, 16);
            if (!w) return;
            where = *w;
        }
        if (finalizingChoices(ctx)) return;
        if (mode == 0) {
            if (gear != kInvalidId && where >= 0 &&
                where < static_cast<int>(ctx.state.battlefields.size()))
                ctx.executor.moveToBattlefield(gear, ctx.state.battlefields[where].id);
        } else {
            auto& deck = ctx.state.player(ctx.controller).main_deck;
            int n = std::min(5, static_cast<int>(deck.size()));
            std::vector<GameObjectId> top(deck.end() - n, deck.end());
            for (auto id : top)
                knowledge::shown(ctx.state, id, knowledge::only(ctx.controller));
            std::vector<GameObjectId> gears;
            for (auto id : top) if (ctx.state.getObject(id).isGear()) gears.push_back(id);
            int go = confirmOptional(ctx, "Command: Attack: reveal a gear and draw it", [ok=!gears.empty()]{ return ok; });
            if (go < 0) return;
            GameObjectId pick = kInvalidId;
            if (go == 1) {
                pick = pickUnseenCard(ctx, "Command: Attack: a gear", gears);
                if (pick == kInvalidId && ctx.state.chain.resuming.has_value() &&
                    ctx.state.chain.resuming->resume_point == 7)
                    return;
                if (pick != kInvalidId) ctx.executor.drawHeldCard(ctx.controller, pick, true);
            }
            std::vector<GameObjectId> rest;
            for (auto id : top)
                if (id != pick && ctx.state.objectExists(id) &&
                    ctx.state.getObject(id).zone == ZoneType::MainDeck)
                    rest.push_back(id);
            if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1094;
        d.def_id = R"RB(rad-033-167)RB";
        d.name = R"RB(Command: Attack)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-033/167)RB";
        d.collector_number = 33;
        d.artist = R"RB()RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Choose one —
Move a friendly gear with [Deploy] to a battlefield.
Look at the top 5 cards of your Main Deck. You may reveal a gear from among them and draw it. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b025756992e22af480902e13b01e25acbc747221-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1094(CardRegistry& r) {
    r.registerCard(1094, std::make_unique<CommandAttack_1094>());
}

}  // namespace riftbound
