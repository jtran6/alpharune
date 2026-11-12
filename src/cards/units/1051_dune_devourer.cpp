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

class DuneDevourer_1051 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB&, PlayerId player, GameObjectId self) const override {
        AdditionalPlayCost c; c.exists = true; c.best.energy_reduction = 2;
        c.payable = !chooseableUnits(state, player, [&](const GameObject& u) {
            return u.controller == player && u.id != self;
        }).empty();
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(CardContext& ctx, const std::function<bool(const PlayCostAdjust&)>& affordable) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.controller == ctx.controller && u.id != ctx.source;
        });
        if (units.empty()) return {};
        if (affordable && affordable({}) && !ctx.executor.confirmNow(ctx.controller, "Dune Devourer: kill a friendly unit to cost [2] less"))
            return {};
        auto id = ctx.executor.chooseObjectNow(ctx.controller, units, "Dune Devourer: kill");
        if (id == kInvalidId || !killAsCost(ctx, id)) return {};
        PlayCostAdjust off; off.energy_reduction = 2; return off;
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1051;
        d.def_id = R"RB(rad-125-167)RB";
        d.name = R"RB(Dune Devourer)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-125/167)RB";
        d.collector_number = 125;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 7;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Deflect);
        d.deflect_value = 1;
        d.ability_text = R"RB(You may kill a friendly unit as an additional cost to play me. If you do, I cost [2] less.
[Deflect] (Opponents must pay [A] to choose me with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4f49bf38f0d3abb71e148783ebec97f446db8166-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1051(CardRegistry& r) {
    r.registerCard(1051, std::make_unique<DuneDevourer_1051>());
}

}  // namespace riftbound
