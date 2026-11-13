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
        // A gear is offered only when some battlefield is not the one it is
        // on. "Some gear can move" is not enough: the chosen gear needs its
        // own destination (CR 355.8).
        auto destinations = [&](GameObjectId g) {
            std::vector<BattlefieldId> out;
            if (!ctx.state.objectExists(g) || !ctx.state.getObject(g).location.has_value()) return out;
            std::optional<BattlefieldId> here;
            if (const auto* bf = std::get_if<BattlefieldLocation>(&*ctx.state.getObject(g).location))
                here = bf->id;
            for (const auto& slot : ctx.state.battlefields) {
                if (here.has_value() && *here == slot.id) continue;
                out.push_back(slot.id);
            }
            return out;
        };
        std::vector<GameObjectId> movable;
        for (auto g : gears)
            if (!destinations(g).empty()) movable.push_back(g);
        uint32_t modes = 2u;
        if (!movable.empty()) modes |= 1u;
        int mode = pickMode(ctx, "Command: Attack", 2,
                            {"Move a friendly Deploy gear", "Look at the top 5"}, modes);
        if (mode < 0) return;
        GameObjectId gear = kInvalidId;
        int locked = 0;
        if (ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_data.size() > 18)
            locked = ctx.state.chain.resuming->resume_data[18];
        if (mode == 0) {
            gear = pickTarget(ctx, "Command: Attack: a Deploy gear", movable);
            if (gear == kInvalidId) return;
            // The answer is the battlefield's id, not its place in the list
            // built from where the gear is now. A response can move the gear,
            // and that list is then a different battlefield.
            if (locked == 0) {
                const auto dests = destinations(gear);
                std::vector<std::string> labels;
                for (auto id : dests) {
                    const BattlefieldState* slot = nullptr;
                    for (const auto& bf : ctx.state.battlefields)
                        if (bf.id == id) slot = &bf;
                    labels.push_back(slot ? battlefieldLabel(ctx.state, *slot)
                                          : "a battlefield");
                }
                if (labels.empty()) {
                    if (finalizingChoices(ctx)) return;
                    return;
                }
                auto w = pickValueAs(ctx, ctx.controller, "Command: Attack: a battlefield",
                                     labels, PromptKind::Location, 16);
                if (!w) return;
                if (*w < 0 || *w >= static_cast<int>(dests.size())) return;
                locked = dests[static_cast<std::size_t>(*w)].raw() + 1;
                if (ctx.state.chain.resuming.has_value()) {
                    auto& rd = ctx.state.chain.resuming->resume_data;
                    while (rd.size() <= 18) rd.push_back(0);
                    rd[18] = locked;
                }
            }
        }
        if (finalizingChoices(ctx)) return;
        if (mode == 0) {
            if (gear != kInvalidId && locked > 0 && ctx.state.objectExists(gear)) {
                const BattlefieldId dest = BattlefieldId::fromRaw(locked - 1);
                bool already = false;
                if (ctx.state.getObject(gear).location.has_value())
                    if (const auto* bf = std::get_if<BattlefieldLocation>(
                            &*ctx.state.getObject(gear).location))
                        already = bf->id == dest;
                if (!already) ctx.executor.moveToBattlefield(gear, dest);
            }
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
