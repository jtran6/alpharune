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

class MechanicalBandit_1040 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        struct Top { PlayerId who; GameObjectId id; int energy; };
        std::vector<Top> tops;
        for (auto p : {PlayerId::Player1, PlayerId::Player2}) {
            auto& deck = ctx.state.player(p).main_deck;
            if (deck.empty()) continue;
            auto id = deck.back();
            int energy = 99;
            auto def = ctx.state.getObject(id).card_def_id;
            if (def != kInvalidId && ctx.registry && ctx.registry->get(def))
                energy = ctx.registry->get(def)->def().energy_cost;
            ctx.events.emit(CardRevealedEvent{id, def, p, true});
            tops.push_back({p, id, energy});
        }
        int best = 99;
        for (auto& t : tops) best = std::min(best, t.energy);
        for (auto& t : tops) {
            if (t.who == ctx.controller && t.energy == best) ctx.executor.drawHeldCard(t.who, t.id, true);
            else if (ctx.state.objectExists(t.id) && ctx.state.getObject(t.id).zone == ZoneType::MainDeck)
                ctx.executor.recycleCards(t.who, {t.id});
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1040;
        d.def_id = R"RB(rad-111-167)RB";
        d.name = R"RB(Mechanical Bandit)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-111/167)RB";
        d.collector_number = 111;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Device)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(At the start of your Beginning Phase, each player reveals the top card of their Main Deck. If you revealed a card with the lowest Energy cost, draw it. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cb2a419b61b6f1e6d138c3c3b07b393a4037237b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1040(CardRegistry& r) {
    r.registerCard(1040, std::make_unique<MechanicalBandit_1040>());
}

}  // namespace riftbound
