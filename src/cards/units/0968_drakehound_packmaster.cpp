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

class DrakehoundPackmaster_968 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<GameObjectId> tops;
        for (auto p : {PlayerId::Player1, PlayerId::Player2}) {
            if (p == ctx.controller) continue;
            auto& deck = ctx.state.player(p).main_deck;
            if (deck.empty()) continue;
            tops.push_back(deck.back());
            ctx.events.emit(CardRevealedEvent{deck.back(), ctx.state.getObject(deck.back()).card_def_id,
                                              p, true});
        }
        auto chosen = pickUnseenCard(ctx, "Drakehound Packmaster: banish and play", tops);
        if (ctx.state.chain.resuming && ctx.state.chain.resuming->resume_point == 7) return;
        if (finalizingChoices(ctx)) return;
        for (auto id : tops) {
            if (id == chosen) {
                ctx.executor.banishObject(id);
                ctx.executor.playIgnoringCost(ctx.controller, id, LocationId{BaseLocation{ctx.controller}});
            } else {
                ctx.executor.recycleCards(ctx.state.getObject(id).owner, {id});
            }
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 968;
        d.def_id = R"RB(rad-018-167)RB";
        d.name = R"RB(Drakehound Packmaster)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-018/167)RB";
        d.collector_number = 18;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.energy_cost = 9;
        d.power_cost = 2;
        d.might = 9;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(When you play me, reveal the top card of each opponent's Main Deck. Choose one and banish it, then play it, ignoring its cost. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9ddc2759cf8b039bc1ee77e32326c5475cc001e8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_968(CardRegistry& r) {
    r.registerCard(968, std::make_unique<DrakehoundPackmaster_968>());
}

}  // namespace riftbound
