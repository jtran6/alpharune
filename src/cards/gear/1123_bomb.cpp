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

// CR 187.12: a domainless gear token with [Deploy] and [Deathknell] — "Deal 2
// to an enemy unit here and kill all other Bombs you control here."
class Bomb_1123 : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        LocationId there{BaseLocation{ctx.controller}};
        if (ctx.state.objectExists(ctx.source)) {
            const auto& self = ctx.state.getObject(ctx.source);
            if (self.last_location.has_value()) there = *self.last_location;
            else if (self.location.has_value()) there = *self.location;
        }
        const auto enemies = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.controller != ctx.controller && u.location.has_value() && *u.location == there;
        });
        const GameObjectId enemy = pickTarget(ctx, "Bomb: deal 2 to an enemy unit here", enemies);
        if (finalizingChoices(ctx)) return;
        if (enemy != kInvalidId) ctx.executor.dealDamage(enemy, 2, ctx.source);
        std::vector<GameObjectId> others;
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (id == ctx.source || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value() || *obj.location != there) continue;
            if (obj.card_def_id.raw() == 1123 ||
                std::find(obj.tags.begin(), obj.tags.end(), "Bomb") != obj.tags.end())
                others.push_back(id);
        }
        for (auto id : others) ctx.executor.killObject(id);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1123;
        d.def_id = R"RB(rad-token-bomb)RB";
        d.name = R"RB(Bomb)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::Token;
        d.energy_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.tags = {"Bomb"};
        d.keywords.set(Keyword::Deploy);
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
[Deathknell] — Deal 2 to an enemy unit here and kill all other Bombs you control here.)RB";
        return d;
    }();
};

}  // namespace

void register_card_1123(CardRegistry& r) {
    r.registerCard(1123, std::make_unique<Bomb_1123>());
}

}  // namespace riftbound
