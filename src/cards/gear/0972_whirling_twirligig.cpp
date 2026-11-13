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

class WhirlingTwirligig_972 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility a, b, c;
        a.cost.exhaust = true;
        b.cost.exhaust = true;
        c.cost.exhaust = true;
        return {a, b, c};
    }
    void onActivate(CardContext& ctx, int ability_index, const std::vector<GameObjectId>&) override {
        if (ability_index == 0) { createGoldExhausted(ctx); return; }
        auto pred = [&](const GameObject& o) {
            return o.controller == ctx.controller && o.id != ctx.source &&
                   (ability_index == 1 ? o.isGear() : o.isUnit());
        };
        auto opts = boardObjects(ctx.state, pred);
        auto id = pickCostObject(ctx, ability_index == 1 ? "Twirl: kill a friendly gear" : "Twirl: kill a friendly unit", opts);
        if (id == kInvalidId || !killAsCost(ctx, id)) return;
        if (ability_index == 1)
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1, {"Recruit"}, {}, BaseLocation{ctx.controller});
        else
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Mech", 3, {"Mech"}, {}, BaseLocation{ctx.controller});
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 972;
        d.def_id = R"RB(rad-022-167)RB";
        d.name = R"RB(Whirling Twirligig)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-022/167)RB";
        d.collector_number = 22;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Epic;

        d.ability_text = R"RB([E]: Play a Gold gear token exhausted. (It has "[Reaction][>] Kill this, [E]: [Add] [A].")
Kill a friendly gear, [E]: Play a 1 [M] Recruit unit token.
Kill a friendly unit, [E]: Play a 3 [M] Mech unit token.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a05ff95e5bb933664e09656973c72f4d39d8e701-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_972(CardRegistry& r) {
    r.registerCard(972, std::make_unique<WhirlingTwirligig_972>());
}

}  // namespace riftbound
