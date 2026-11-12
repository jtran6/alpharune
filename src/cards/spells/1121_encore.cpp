#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class Encore_1121 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId, GameObjectId) const override {
        return FinalizeChoice::No;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        int a = pickMode(ctx, "Encore: first", 4, {"Deal 2 to a unit", "Play two Recruits", "Give up to two units +1", "Look at the top 2"});
        if (a < 0) return;
        int b = pickMode(ctx, "Encore: second", 4, {"Deal 2 to a unit", "Play two Recruits", "Give up to two units +1", "Look at the top 2"});
        if (b < 0) return;
        if (finalizingChoices(ctx)) return;
        // Both modes are already chosen. Their targets are asked here, each
        // with its own slot, and nothing is done until every one of them has
        // an answer — a second pickTarget would share the first's slot, and a
        // look that ran while a later mode was still asking would draw twice.
        auto units = chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; });
        const int deals = (a == 0 ? 1 : 0) + (b == 0 ? 1 : 0);
        GameObjectId deal_a = kInvalidId, deal_b = kInvalidId;
        if (deals == 2) {
            auto [x, y] = pickTargetPair(ctx, "Encore: deal 2", units,
                                         [&](GameObjectId) { return units; });
            const bool waiting = (x == kInvalidId || y == kInvalidId) &&
                                 ctx.state.chain.resuming.has_value() &&
                                 (ctx.state.chain.resuming->resume_point == 10 ||
                                  ctx.state.chain.resuming->resume_point == 12);
            if (waiting) return;
            deal_a = x;
            deal_b = y;
        } else if (deals == 1) {
            auto t = pickTarget(ctx, "Encore: deal 2", units);
            if (t == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7)
                return;
            if (a == 0) deal_a = t;
            else deal_b = t;
        }
        std::optional<std::vector<GameObjectId>> buff_a, buff_b;
        auto takeBuffs = [&](size_t slot) -> std::optional<std::vector<GameObjectId>> {
            return selectSeveralTargets(*this, ctx, "Encore: +1", units, 2, slot);
        };
        if (a == 2) {
            buff_a = takeBuffs(7);
            if (!buff_a) return;
        }
        if (b == 2) {
            buff_b = takeBuffs(13);
            if (!buff_b) return;
        }
        auto apply = [&](int mode, GameObjectId dealt,
                         const std::optional<std::vector<GameObjectId>>& buffs) {
            if (mode == 0) {
                if (dealt != kInvalidId) ctx.executor.dealDamage(dealt, 2, ctx.source);
            } else if (mode == 1) {
                for (int i = 0; i < 2; ++i)
                    ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1,
                                             {"Recruit"}, {}, BaseLocation{ctx.controller});
            } else if (mode == 2) {
                if (buffs) for (auto id : *buffs) ctx.executor.giveTemporaryMight(id, 1);
            } else {
                auto& deck = ctx.state.player(ctx.controller).main_deck;
                int n = std::min(2, static_cast<int>(deck.size()));
                if (n == 0) return;
                std::vector<GameObjectId> top(deck.end() - n, deck.end());
                for (auto id : top)
                    knowledge::shown(ctx.state, id, knowledge::only(ctx.controller));
                ctx.executor.drawHeldCard(ctx.controller, top.back(), false);
                if (top.size() == 2) ctx.executor.recycleCards(ctx.controller, {top.front()});
            }
        };
        apply(a, deal_a, buff_a);
        apply(b, deal_b, buff_b);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1121;
        d.def_id = R"RB(rad-152-167)RB";
        d.name = R"RB(Encore)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-152/167)RB";
        d.collector_number = 152;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Order};
        d.tags = {R"RB(Seraphine)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Epic;

        d.ability_text = R"RB(Choose two —
Deal 2 to a unit.
Play two 1 [M] Recruit unit tokens.
Give up to two units each +1 [M] this turn.
Look at the top 2 cards of your Main Deck, draw 1 and recycle the other.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0200aeca5d0a1cca4b8ab5b3f3910207118ee4e4-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1121(CardRegistry& r) {
    r.registerCard(1121, std::make_unique<Encore_1121>());
}

}  // namespace riftbound
