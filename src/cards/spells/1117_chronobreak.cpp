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

class Chronobreak_1117 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId, GameObjectId) const override {
        return FinalizeChoice::No;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        int x = pickXAmount(ctx, "Chronobreak: pay any amount of Power", 0, 5);
        if (x < 0) return;
        if (finalizingChoices(ctx)) return;
        if (!ctx.executor.payResources(ctx.controller, 0, x, resource::kAnyDomain, SpendFor::Spell)) return;
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        int n = std::min(x, static_cast<int>(units.size()));
        std::vector<std::pair<GameObjectId, LocationId>> back;
        for (int i = 0; i < n; ++i) {
            LocationId loc = ctx.state.getObject(units[i]).location.value_or(LocationId{BaseLocation{ctx.controller}});
            back.push_back({units[i], loc});
            ctx.executor.banishObject(units[i]);
        }
        for (auto& [id, loc] : back) ctx.executor.playIgnoringCost(ctx.state.getObject(id).owner, id, loc);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1117;
        d.def_id = R"RB(rad-140-167)RB";
        d.name = R"RB(Chronobreak)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-140/167)RB";
        d.collector_number = 140;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Mind};
        d.tags = {R"RB(Ekko)RB"};
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Epic;

        d.ability_text = R"RB(As an additional cost to play this, pay any amount of Power.
Banish that many units you control, then for each, its owner plays that unit back to the same location or to their base, ignoring its cost. (You choose the order.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/af2123a99f3e3e7e6c9b101c5fc2c9b0ff56e096-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1117(CardRegistry& r) {
    r.registerCard(1117, std::make_unique<Chronobreak_1117>());
}

}  // namespace riftbound
