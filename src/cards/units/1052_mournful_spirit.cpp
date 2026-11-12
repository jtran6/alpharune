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

class MournfulSpirit_1052 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        LocationId there{BaseLocation{ctx.controller}};
        if (ctx.state.objectExists(ctx.source) && ctx.state.getObject(ctx.source).last_location.has_value())
            there = *ctx.state.getObject(ctx.source).last_location;
        auto opts = boardObjects(ctx.state, [&](const GameObject& o) {
            return (o.isUnit() || o.isGear()) && o.location.has_value() && *o.location == there && o.id != ctx.source;
        });
        int go = confirmOptional(ctx, "Mournful Spirit: kill a unit or gear here", [ok=!opts.empty()]{ return ok; });
        if (go < 0) return;
        if (finalizingChoices(ctx)) return;
        if (go != 1) return;
        auto t = pickTarget(ctx, "Mournful Spirit: kill", opts);
        if (t != kInvalidId) ctx.executor.killObject(t);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1052;
        d.def_id = R"RB(rad-126-167)RB";
        d.name = R"RB(Mournful Spirit)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-126/167)RB";
        d.collector_number = 126;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Spirit)RB"};
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell][>] You may kill a unit or gear here. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6d8500ac7efc5382985f566212d88ac8f41c135d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1052(CardRegistry& r) {
    r.registerCard(1052, std::make_unique<MournfulSpirit_1052>());
}

}  // namespace riftbound
