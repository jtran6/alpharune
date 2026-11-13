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

class HexplosivesExpert_1063 : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.energy = 1; c.power = 1; c.power_any_domain = true; c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        playBombToken(*this, ctx, "Hexplosives Expert: play a Bomb");
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1063;
        d.def_id = R"RB(rad-141-167)RB";
        d.name = R"RB(Hexplosives Expert)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-141/167)RB";
        d.collector_number = 141;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury, Domain::Chaos};
        d.tags = {R"RB(Yordle)RB", R"RB(Ziggs)RB"};
        d.champion_tag = R"RB(Ziggs)RB";
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB([1][A], [E]: Play a Bomb gear token to a battlefield. (It has [Deploy]. When it dies, deal 2 to an enemy there and kill your other Bombs there.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/69899f4d5e05ff0a060b9f38894cd77f9e17d195-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1063(CardRegistry& r) {
    r.registerCard(1063, std::make_unique<HexplosivesExpert_1063>());
}

}  // namespace riftbound
