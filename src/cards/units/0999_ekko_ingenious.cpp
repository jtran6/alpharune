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

class EkkoIngenious_999 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB&, PlayerId player, GameObjectId self) const override {
        AdditionalPlayCost c; c.exists = true; c.mandatory = true;
        c.payable = !chooseableUnits(state, player, [&](const GameObject& u) {
            return u.controller == player && u.id != self;
        }).empty();
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(CardContext& ctx, const std::function<bool(const PlayCostAdjust&)>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.controller == ctx.controller && u.id != ctx.source;
        });
        auto id = ctx.executor.chooseObjectNow(ctx.controller, units, "Ekko: return a friendly unit");
        if (id != kInvalidId) ctx.executor.bounceToHand(id);
        return {};
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 999;
        d.def_id = R"RB(rad-061-167)RB";
        d.name = R"RB(Ekko, Ingenious)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-061/167)RB";
        d.collector_number = 61;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Ekko)RB", R"RB(Zaun)RB"};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB(As an additional cost to play me, return a friendly unit to its owner's hand.
[Ganking] (I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3129469742d7aa5c6921ebae841ccee1923bb279-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_999(CardRegistry& r) {
    r.registerCard(999, std::make_unique<EkkoIngenious_999>());
}

}  // namespace riftbound
