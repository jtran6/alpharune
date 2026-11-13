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

class IncorrigibleConstruct_962 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool matchesShowOff(const GameState&, const GameObject& obj) const override {
        if (obj.name == "Mech") return true;
        for (const auto& tag : obj.tags) if (tag == "Mech") return true;
        return false;
    }
    bool entersReadyOnPlay(const GameState&, PlayerId, const GameObject& self) const override {
        return playedWithShowOff(self);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 962;
        d.def_id = R"RB(rad-011-167)RB";
        d.name = R"RB(Incorrigible Construct)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-011/167)RB";
        d.collector_number = 11;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::ShowOff);
        d.ability_text = R"RB([Show Off] a Mech. (As you play me, you may reveal a Mech from hand or pick another friendly Mech.)
I enter ready if you showed off.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e95470d5c2fbc9f7f8ab0b1c19f32fd704918c83-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_962(CardRegistry& r) {
    r.registerCard(962, std::make_unique<IncorrigibleConstruct_962>());
}

}  // namespace riftbound
