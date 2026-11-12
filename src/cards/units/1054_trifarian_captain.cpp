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

class TrifarianCaptain_1054 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& deck = ctx.state.player(ctx.controller).main_deck;
        int n = std::min(4, static_cast<int>(deck.size()));
        std::vector<GameObjectId> top(n ? deck.end() - n : deck.end(), deck.end());
        // A private look, re-recorded on every entry so the draw offer names
        // cards this player has been shown. The recycle waits until the draw
        // has been chosen: doing it while that prompt is still out takes the
        // cards back before the decision is answered.
        for (auto id : top)
            knowledge::shown(ctx.state, id, knowledge::only(ctx.controller));
        std::vector<GameObjectId> ok;
        for (auto id : top) {
            auto def = ctx.state.getObject(id).card_def_id;
            if (def == kInvalidId || !ctx.registry || !ctx.registry->get(def)) continue;
            const auto& d = ctx.registry->get(def)->def();
            if ((d.card_type == CardType::Unit || d.card_type == CardType::Gear) && d.energy_cost <= 3 && d.power_cost <= 1)
                ok.push_back(id);
        }
        int go = confirmOptional(ctx, "Trifarian Captain: reveal a cheap unit or gear and draw it", [legal=!ok.empty()]{ return legal; });
        if (go < 0) return;
        if (finalizingChoices(ctx)) return;
        GameObjectId pick = kInvalidId;
        if (go == 1) {
            pick = pickUnseenCard(ctx, "Trifarian Captain: draw", ok);
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

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1054;
        d.def_id = R"RB(rad-129-167)RB";
        d.name = R"RB(Trifarian Captain)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-129/167)RB";
        d.collector_number = 129;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 1;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(When you play me, look at the top 4 cards of your Main Deck. You may reveal from among them a unit or gear with Energy cost no more than [3] and with Power cost no more than [A] and draw it. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/914ced10400826a878ed5cf7294207a4e7b542b1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1054(CardRegistry& r) {
    r.registerCard(1054, std::make_unique<TrifarianCaptain_1054>());
}

}  // namespace riftbound
