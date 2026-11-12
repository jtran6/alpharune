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

class ScrutinizingScope_1002 : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.energy = 1; c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto guess = pickValueAs(ctx, opponent(ctx.controller),
                                 "Scrutinizing Scope: guess a card type",
                                 {"Unit", "Gear", "Spell"}, PromptKind::Mode, /*slot=*/7);
        if (!guess) return;
        const int said = *guess;
        if (said < 0) return;
        auto& deck = ctx.state.player(ctx.controller).main_deck;
        if (deck.empty()) return;
        auto top = deck.back();
        ctx.events.emit(CardRevealedEvent{top, ctx.state.getObject(top).card_def_id, ctx.controller, true});
        CardType want = said == 0 ? CardType::Unit : said == 1 ? CardType::Gear : CardType::Spell;
        if (ctx.state.getObject(top).card_type == want) ctx.executor.recycleCards(ctx.controller, {top});
        else ctx.executor.drawHeldCard(ctx.controller, top, true);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1002;
        d.def_id = R"RB(rad-064-167)RB";
        d.name = R"RB(Scrutinizing Scope)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-064/167)RB";
        d.collector_number = 64;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB([1], [E]: Choose an opponent. They guess a card type. Reveal the top card of your Main Deck. If it has that type, recycle it. Otherwise, draw it. (Unit, gear, and spell are types.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6a028fc372c14dda73864aba3d3e84063f6da98d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1002(CardRegistry& r) {
    r.registerCard(1002, std::make_unique<ScrutinizingScope_1002>());
}

}  // namespace riftbound
