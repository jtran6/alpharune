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

class HuntersCircle_1080 : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // A battlefield card has no location, so it is not in inPlay() and
        // forEachSelfOnce never sees it. The engine calls this once per slot.
        if (controller == PlayerId::None) return;
        for (const auto& bf : state.battlefields) {
            if (bf.controller != controller || !state.objectExists(bf.card_object_id)) continue;
            auto& self = state.getObject(bf.card_object_id);
            if (self.card_def_id != cardDefId()) continue;
            bool seen = false;
            for (const auto& ae : self.aura_effects)
                if (ae.source == bf.card_object_id && ae.keyword == Keyword::Count &&
                    ae.keyword_value == kAuraOnceMarker) { seen = true; break; }
            if (seen) continue;
            GameObject::AuraEffect marker;
            marker.source = bf.card_object_id;
            marker.keyword = Keyword::Count;
            marker.keyword_value = kAuraOnceMarker;
            self.aura_effects.push_back(marker);
            const int slot = static_cast<int>(bf.id.raw());
            PlayerState::CostModifier unit;
            unit.source = bf.card_object_id;
            unit.energy_reduction = 1;
            unit.units_only = true;
            unit.next_unit_only = true;
            unit.from_aura = true;
            unit.only_at_battlefield = slot;
            PlayerState::CostModifier gear = unit;
            gear.units_only = false;
            gear.next_unit_only = false;
            gear.gear_only = true;
            gear.first_gear_per_turn = true;
            state.player(controller).cost_modifiers.push_back(unit);
            state.player(controller).cost_modifiers.push_back(gear);
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1080;
        d.def_id = R"RB(rad-163-167)RB";
        d.name = R"RB(Hunters' Circle)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-163/167)RB";
        d.collector_number = 163;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(For each player, the first unit or gear they play here on their turn costs [1] less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e7e25d90506d805f1996e7f751cb5b6baa8ec305-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1080(CardRegistry& r) {
    r.registerCard(1080, std::make_unique<HuntersCircle_1080>());
}

}  // namespace riftbound
