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

class OriannaExquisite_985 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool hasMightDependentAura() const override { return true; }
    void applyMightDependentAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject& me) {
            bool gear = false;
            if (me.location.has_value()) {
                for (auto& [id, obj] : state.objects.atLocation(*me.location))
                    if (obj.isGear() && obj.controller == controller) gear = true;
            }
            if (!gear) me.aura_no_combat_damage = true;
        });
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 985;
        d.def_id = R"RB(rad-042-167)RB";
        d.name = R"RB(Orianna, Exquisite)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-042/167)RB";
        d.collector_number = 42;
        d.artist = R"RB()RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Piltover)RB", R"RB(Orianna)RB"};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Shield);
        d.shield_value = 1;
        d.ability_text = R"RB([Shield] (+1 [M] while I'm a defender.)
I don't deal combat damage unless there's a friendly gear here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3e608c51698b6fb0cc6a00335dbd1bca84f57c73-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_985(CardRegistry& r) {
    r.registerCard(985, std::make_unique<OriannaExquisite_985>());
}

}  // namespace riftbound
