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

class BlackMarket_1075 : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.power = 2; c.power_any_domain = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.state.objectExists(ctx.source) &&
            ctx.state.getObject(ctx.source).card_counters["__market"] == ctx.state.turn.turn_number) return;
        ctx.executor.drawCards(ctx.controller, 1);
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).card_counters["__market"] = ctx.state.turn.turn_number;
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1075;
        d.def_id = R"RB(rad-158-167)RB";
        d.name = R"RB(Black Market)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-158/167)RB";
        d.collector_number = 158;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB([A][A]: Draw 1. Each player may use this ability once on their turn (even if they don't control this battlefield).)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9a02ae108f32a8b88be47a94b5f0d39b5a06a6a1-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1075(CardRegistry& r) {
    r.registerCard(1075, std::make_unique<BlackMarket_1075>());
}

}  // namespace riftbound
