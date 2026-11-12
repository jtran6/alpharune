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

class EkkoRevolutionary_971 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool entersReadyOnPlay() const override { return true; }
    bool hasActivatedAbility() const override { return true; }
    bool isActionAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c; c.discard = true; c.energy = 1; c.power = 1; c.power_domain = Domain::Fury;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.banishObject(ctx.source);
        ctx.executor.playIgnoringCost(ctx.controller, ctx.source, BaseLocation{ctx.controller});
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 971;
        d.def_id = R"RB(rad-021-167)RB";
        d.name = R"RB(Ekko, Revolutionary)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-021/167)RB";
        d.collector_number = 21;
        d.artist = R"RB()RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Ekko)RB", R"RB(Zaun)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Assault);
        d.assault_value = 1;
        d.ability_text = R"RB(I enter ready.
[Assault] (+1 [M] while I'm an attacker.)
[Action][>] Discard 1, [1][R]: Banish me, then play me to your base, ignoring my cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d13964c46506f9f36430dfe91ee02d258a304908-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_971(CardRegistry& r) {
    r.registerCard(971, std::make_unique<EkkoRevolutionary_971>());
}

}  // namespace riftbound
