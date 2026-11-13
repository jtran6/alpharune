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

class SkystrikeSquadron_1012 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB&, PlayerId player, GameObjectId self) const override {
        AdditionalPlayCost c; c.exists = true;
        c.best.energy_reduction = 2;
        c.payable = !boardObjects(state, [&](const GameObject& o) {
            return o.isGear() && o.controller == player && o.id != self;
        }).empty();
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(CardContext& ctx, const std::function<bool(const PlayCostAdjust&)>& affordable) override {
        auto gears = boardObjects(ctx.state, [&](const GameObject& o) {
            return o.isGear() && o.controller == ctx.controller && o.id != ctx.source;
        });
        PlayCostAdjust off; off.energy_reduction = 2;
        if (gears.empty()) return {};
        if (affordable && affordable({}) && !ctx.executor.confirmNow(ctx.controller, "Skystrike Squadron: return a gear to cost [2] less"))
            return {};
        auto id = ctx.executor.chooseObjectNow(ctx.controller, gears, "Skystrike Squadron: return");
        if (id == kInvalidId) return {};
        ctx.executor.bounceToHand(id);
        return off;
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1012;
        d.def_id = R"RB(rad-075-167)RB";
        d.name = R"RB(Skystrike Squadron)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-075/167)RB";
        d.collector_number = 75;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {};
        d.energy_cost = 5;
        d.power_cost = 0;
        d.might = 4;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Deflect);
        d.deflect_value = 1;
        d.ability_text = R"RB(You may return a friendly gear to its owner's hand as an additional cost to play me. If you do, I cost [2] less.
[Deflect] (Opponents must pay [A] to choose me with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/694a362579d3f2e9805cac0da4814ea911423033-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1012(CardRegistry& r) {
    r.registerCard(1012, std::make_unique<SkystrikeSquadron_1012>());
}

}  // namespace riftbound
