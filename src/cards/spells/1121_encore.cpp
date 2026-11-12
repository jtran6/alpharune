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
        // pickMode keeps one answer per chain item, so a second call repeated
        // the first mode. Each mode is its own prompt, and the stored value is
        // the mode id, so the list can omit a mode that has no legal target
        // (CR 355.8) without shifting a stored answer. Damage and buff targets
        // are chosen before the marker; where a Recruit is played and which
        // looked-at card is drawn are resolution choices.
        auto units = chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; });
        auto askMode = [&](const char* label, size_t slot, bool allow_deal) -> std::optional<int> {
            if (!ctx.state.chain.resuming.has_value()) return allow_deal ? 0 : 1;
            auto& ri = *ctx.state.chain.resuming;
            while (ri.resume_data.size() < slot + 2) ri.resume_data.push_back(0);
            if (ri.resume_data[slot] == 2) return ri.resume_data[slot + 1];
            if (ri.resume_data[slot] == 1) {
                auto picked = ctx.executor.takeChoice();
                int v = (picked && picked->chosen_value) ? *picked->chosen_value : 1;
                if (v < 0 || v > 3 || (v == 0 && !allow_deal)) v = 1;
                ri.resume_data[slot] = 2;
                ri.resume_data[slot + 1] = v;
                return v;
            }
            std::vector<Intent> choices;
            auto add = [&](int id, const char* text) {
                Intent o;
                o.type = IntentType::MakeChoice;
                o.player = ctx.controller;
                o.chosen_value = id;
                o.choice_label = text;
                choices.push_back(std::move(o));
            };
            if (allow_deal) add(0, "Deal 2 to a unit");
            add(1, "Play two Recruits");
            add(2, "Give up to two units +1");
            add(3, "Look at the top 2");
            ctx.executor.requestChoice(ctx.controller, std::move(choices), label, PromptKind::Mode);
            ri.resume_data[slot] = 1;
            return std::nullopt;
        };
        // Deal is offered only when some unit's Deflect can be paid with the
        // spell. Both modes may deal to the same unit (4 damage).
        auto payable = offerableChoices(ctx, units);
        const bool allow_deal = !payable.empty();
        auto first = askMode("Encore: first mode", 20, allow_deal);
        if (!first) return;
        auto second = askMode("Encore: second mode", 22, allow_deal);
        if (!second) return;
        const int a = *first;
        const int b = *second;

        const int deals = (a == 0 ? 1 : 0) + (b == 0 ? 1 : 0);
        GameObjectId deal_a = kInvalidId, deal_b = kInvalidId;
        if (deals == 2) {
            auto [x, y] = pickTargetPair(ctx, "Encore: deal 2", payable,
                                         [&](GameObjectId) { return payable; });
            const bool waiting = (x == kInvalidId || y == kInvalidId) &&
                                 ctx.state.chain.resuming.has_value() &&
                                 (ctx.state.chain.resuming->resume_point == 10 ||
                                  ctx.state.chain.resuming->resume_point == 12);
            if (waiting) return;
            deal_a = x;
            deal_b = y;
        } else if (deals == 1) {
            auto t = pickTarget(ctx, "Encore: deal 2 to a unit", payable);
            if (t == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7)
                return;
            if (a == 0) deal_a = t;
            else deal_b = t;
        }
        std::optional<std::vector<GameObjectId>> buff_a, buff_b;
        if (a == 2) {
            buff_a = selectSeveralTargets(*this, ctx, "Encore: +1 [M] to up to two units",
                                         units, 2, /*first_slot=*/24);
            if (!buff_a) return;
        }
        if (b == 2) {
            buff_b = selectSeveralTargets(*this, ctx, "Encore: +1 [M] to up to two units, again",
                                         units, 2, /*first_slot=*/28);
            if (!buff_b) return;
        }
        if (finalizingChoices(ctx)) return;
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        while (ri.resume_data.size() < 32) ri.resume_data.push_back(0);

        auto lookOnce = [&](int look_i) -> bool {
            auto& deck = ctx.state.player(ctx.controller).main_deck;
            const int base = 90 + 6 * look_i;
            while (static_cast<int>(ri.resume_data.size()) < base + 4) ri.resume_data.push_back(0);
            if (ri.resume_data[base] == 0) {
                const int n = std::min(2, static_cast<int>(deck.size()));
                ri.resume_data[base] = n + 1;
                for (int i = 0; i < n; ++i) {
                    const GameObjectId id = deck[deck.size() - 1 - static_cast<std::size_t>(i)];
                    ri.resume_data[base + 1 + i] = static_cast<int32_t>(id.raw());
                    knowledge::shown(ctx.state, id, knowledge::only(ctx.controller));
                }
            }
            const int n = ri.resume_data[base] - 1;
            std::vector<GameObjectId> top;
            for (int i = 0; i < n; ++i) {
                const GameObjectId id = GameObjectId::fromRaw(ri.resume_data[base + 1 + i]);
                if (std::find(deck.begin(), deck.end(), id) != deck.end()) top.push_back(id);
            }
            if (top.empty()) return true;
            auto drawn = selectObjectAs(ctx, ctx.controller, "Encore: draw one of the top cards",
                                       top, static_cast<std::size_t>(70 + 4 * look_i));
            if (!drawn) return false;
            GameObjectId keep = *drawn == kInvalidId ? top.front() : *drawn;
            if (std::find(top.begin(), top.end(), keep) == top.end()) keep = top.front();
            if (std::find(deck.begin(), deck.end(), keep) != deck.end())
                ctx.executor.drawHeldCard(ctx.controller, keep, false);
            std::vector<GameObjectId> rest;
            for (auto id : top)
                if (id != keep && std::find(deck.begin(), deck.end(), id) != deck.end())
                    rest.push_back(id);
            if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
            return true;
        };
        auto apply = [&](int mode, int which, GameObjectId deal,
                         const std::optional<std::vector<GameObjectId>>& buff, int look_i) -> bool {
            if (mode == 0 && deal != kInvalidId)
                ctx.executor.dealDamage(deal, 2, ctx.source);
            if (mode == 2 && buff)
                for (auto id : *buff) ctx.executor.giveTemporaryMight(id, 1);
            if (mode == 1) {
                if (ri.resume_data[30 + which] == 0) {
                    if (ri.resume_data.size() > kTokenPlaySlot) ri.resume_data[kTokenPlaySlot] = 0;
                    ri.resume_data[30 + which] = 1;
                }
                std::vector<GameObjectId> made;
                if (!playTokensChoosingLocations(ctx, "Encore: play a Recruit", 2,
                        [&](LocationId loc) {
                            return ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1,
                                                           {"Recruit"}, {}, loc);
                        }, made))
                    return false;
            }
            if (mode == 3 && !lookOnce(look_i)) return false;
            return true;
        };
        // Modes resolve in the order they were chosen, each once.
        if (ri.resume_data[18] == 0) {
            if (!apply(a, 0, deal_a, buff_a, 0)) return;
            ri.resume_data[18] = 1;
        }
        if (ri.resume_data[18] == 1) {
            const int look_i = a == 3 ? 1 : 0;
            if (!apply(b, 1, deal_b, buff_b, look_i)) return;
            ri.resume_data[18] = 2;
        }
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
