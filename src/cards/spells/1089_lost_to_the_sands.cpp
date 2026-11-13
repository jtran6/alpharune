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

class LostToTheSands_1089 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId player,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, player, object, [](const GameObject& u) {
            return u.isAtBattlefield();
        });
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        const bool unit = !chooseableUnits(state, player, [](const GameObject& u) {
            return u.isAtBattlefield();
        }).empty();
        const bool gear = !boardObjects(state, [](const GameObject& o) { return o.isGear(); }).empty();
        return unit || gear;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.location.has_value() && std::holds_alternative<BattlefieldLocation>(*u.location);
        });
        auto gears = boardObjects(ctx.state, [](const GameObject& o) { return o.isGear(); });
        uint32_t mask = 0;
        if (!units.empty()) mask |= 1u;
        if (!gears.empty()) mask |= 2u;
        int mode = pickMode(ctx, "Lost to the Sands", 2,
                            {"Deal 4 to a unit at a battlefield", "Kill a gear"}, mask);
        if (mode < 0) return;
        if (mode == 0) {
            auto t = pickTarget(ctx, "Lost to the Sands: deal 4", units);
            if (finalizingChoices(ctx)) return;
            if (t != kInvalidId) ctx.executor.dealDamage(t, 4, ctx.source);
        } else {
            auto t = pickTarget(ctx, "Lost to the Sands: kill a gear", gears);
            if (finalizingChoices(ctx)) return;
            if (t != kInvalidId) ctx.executor.killObject(t);
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1089;
        d.def_id = R"RB(rad-013-167)RB";
        d.name = R"RB(Lost to the Sands)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-013/167)RB";
        d.collector_number = 13;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose one —
Deal 4 to a unit at a battlefield.
Kill a gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/92071a0f4d495f5f7acdf78d060133b98d1cebae-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1089(CardRegistry& r) {
    r.registerCard(1089, std::make_unique<LostToTheSands_1089>());
}

}  // namespace riftbound
