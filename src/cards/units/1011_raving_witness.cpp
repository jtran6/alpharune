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

class RavingWitness_1011 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& deck = ctx.state.player(ctx.controller).main_deck;
        if (deck.empty()) return;
        auto top = deck.back();
        ctx.events.emit(CardRevealedEvent{top, ctx.state.getObject(top).card_def_id, ctx.controller, true});
        int might = 0;
        auto def = ctx.state.getObject(top).card_def_id;
        if (def != kInvalidId && ctx.registry && ctx.registry->get(def))
            might = ctx.registry->get(def)->def().might;
        if (might >= 5) ctx.executor.drawHeldCard(ctx.controller, top, true);
        else ctx.executor.recycleCards(ctx.controller, {top});
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1011;
        d.def_id = R"RB(rad-074-167)RB";
        d.name = R"RB(Raving Witness)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-074/167)RB";
        d.collector_number = 74;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.rarity = Rarity::Common;

        d.ability_text = R"RB(When I move, reveal the top card of your Main Deck. If it's [Mighty], draw it. Otherwise, recycle it. (A unit is Mighty while it has 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/778e70b353d7f28eae9b54b1a37af3853238ba04-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1011(CardRegistry& r) {
    r.registerCard(1011, std::make_unique<RavingWitness_1011>());
}

}  // namespace riftbound
