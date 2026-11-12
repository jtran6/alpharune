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

class ImprovisedProjectile_1100 : public SpellCard {
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
        const bool gear = !boardObjects(state, [&](const GameObject& o) {
            return o.isGear() && o.controller == player;
        }).empty();
        const bool unit = !chooseableUnits(state, player, [](const GameObject& u) {
            return u.isAtBattlefield();
        }).empty();
        return gear && unit;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto gears = boardObjects(ctx.state, [&](const GameObject& o) { return o.isGear() && o.controller == ctx.controller; });
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.location.has_value() && std::holds_alternative<BattlefieldLocation>(*u.location);
        });
        auto [gear, t] = pickTargetPair(ctx, "Improvised Projectile", gears,
                                        [&](GameObjectId) { return units; });
        if (finalizingChoices(ctx)) return;
        const bool waiting = (gear == kInvalidId || t == kInvalidId) &&
                             ctx.state.chain.resuming.has_value() &&
                             (ctx.state.chain.resuming->resume_point == 10 ||
                              ctx.state.chain.resuming->resume_point == 12);
        if (waiting) return;
        if (gear == kInvalidId || t == kInvalidId) return;
        int energy = 0;
        auto def = ctx.state.getObject(gear).card_def_id;
        if (def != kInvalidId && ctx.registry && ctx.registry->get(def)) energy = ctx.registry->get(def)->def().energy_cost;
        ctx.executor.dealDamage(t, energy, ctx.source);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1100;
        d.def_id = R"RB(rad-056-167)RB";
        d.name = R"RB(Improvised Projectile)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-056/167)RB";
        d.collector_number = 56;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose a friendly gear. Deal damage equal to its Energy cost to a unit at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ecbb970f955b8e12369921f30ad08cab04e59a3f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1100(CardRegistry& r) {
    r.registerCard(1100, std::make_unique<ImprovisedProjectile_1100>());
}

}  // namespace riftbound
