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

class Transpositionist_1044 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.power = 1; c.power_domain = Domain::Chaos; c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        auto t = pickTarget(ctx, "Transpositionist: return a friendly unit", units);
        if (t == kInvalidId) return;
        int might = ctx.state.getObject(t).current_might;
        ctx.executor.bounceToHand(t);
        auto hand = ctx.state.player(ctx.controller).hand;
        std::vector<GameObjectId> cheaper;
        for (auto id : hand) {
            if (!ctx.state.objectExists(id) || !ctx.state.getObject(id).isUnit()) continue;
            auto def = ctx.state.getObject(id).card_def_id;
            int m = def != kInvalidId && ctx.registry && ctx.registry->get(def) ? ctx.registry->get(def)->def().might : 0;
            if (m < might) cheaper.push_back(id);
        }
        int go = confirmOptional(ctx, "Transpositionist: play a unit with less Might, ignoring its cost", [ok=!cheaper.empty()]{ return ok; });
        if (go == 1) {
            auto play = pickUnseenCard(ctx, "Transpositionist: play", cheaper);
            if (play != kInvalidId) ctx.executor.playIgnoringCost(ctx.controller, play, LocationId{BaseLocation{ctx.controller}});
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1044;
        d.def_id = R"RB(rad-115-167)RB";
        d.name = R"RB(Transpositionist)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-115/167)RB";
        d.collector_number = 115;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 6;
        d.power_cost = 0;
        d.might = 4;
        d.rarity = Rarity::Epic;

        d.ability_text = R"RB([P], [E]: Return a friendly unit to its owner's hand. You may play a unit with less Might than that unit had, ignoring its cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/60cf9dbb9345d44f0a9a78b60d5fb71411b50d69-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1044(CardRegistry& r) {
    r.registerCard(1044, std::make_unique<Transpositionist_1044>());
}

}  // namespace riftbound
