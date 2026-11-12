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

class PrideOfNazumah_1067 : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.energy = 1; c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        auto t = pickTarget(ctx, "Pride of Nazumah: Assault equal to Shield", units);
        if (t == kInvalidId) return;
        int shield = std::max(1, ctx.state.getObject(t).shield_value);
        ctx.executor.giveTemporaryKeyword(t, Keyword::Assault, shield);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1067;
        d.def_id = R"RB(rad-147-167)RB";
        d.name = R"RB(Pride of Nazumah)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-147/167)RB";
        d.collector_number = 147;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm, Domain::Body};
        d.tags = {R"RB(K'Sante)RB"};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Shield);
        d.shield_value = 1;
        d.ability_text = R"RB([1], [E]: Give a friendly unit "I have [Assault] equal to my [Shield]" this turn. (+1 [M] for each instance of Assault while it's an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/190a27bf844fa21105824efc19de4e71c3137df6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1067(CardRegistry& r) {
    r.registerCard(1067, std::make_unique<PrideOfNazumah_1067>());
}

}  // namespace riftbound
