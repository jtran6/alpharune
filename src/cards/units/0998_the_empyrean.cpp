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

class TheEmpyrean_998 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "A card" is a Main Deck card (CR 052). Tokens are not cards (CR 185),
    // and a rune is not one either. The Legend Zone stays out: a legend is
    // not a card this instruction shows off.
    bool matchesShowOff(const GameState&, const GameObject& obj) const override {
        if (obj.isToken() || obj.isRune()) return false;
        if (obj.zone == ZoneType::LegendZone) return false;
        if (obj.card_def_id == kInvalidId) return false;
        return true;
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source) || !playedWithShowOff(ctx.state.getObject(ctx.source))) return;
        int power = ctx.state.getObject(ctx.source).card_counters[kShowOffPower];
        if (power > 0) ctx.executor.predict(ctx.controller, power);
        ctx.executor.drawCards(ctx.controller, 2);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 998;
        d.def_id = R"RB(rad-060-167)RB";
        d.name = R"RB(The Empyrean)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-060/167)RB";
        d.collector_number = 60;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Dragon)RB"};
        d.energy_cost = 8;
        d.power_cost = 0;
        d.might = 8;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::ShowOff);
        d.ability_text = R"RB([Show Off] a card. (As you play me, you may reveal a card from hand or pick another friendly one.)
When you play me, if you showed off a card, [Predict] equal to its Power cost, then draw 2. (Look at that many cards from your Main Deck. Recycle any of them and put the rest back in any order.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b7cd2e475834863bf4e66994118a835ba4377d16-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_998(CardRegistry& r) {
    r.registerCard(998, std::make_unique<TheEmpyrean_998>());
}

}  // namespace riftbound
