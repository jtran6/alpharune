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

class CosmicVista_1077 : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }

    // "When you hold here": the battlefield's hold, with the holder as its
    // controller (TriggerManager::onScore). WhenIHold is a unit's own hold.
    // "If your Rune Deck is empty" is the trigger condition (CR 383.2.a.1):
    // judged when the hold happens. A rune that arrives or leaves in response
    // does not change whether the ability was placed.
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
    bool triggerCondition(const GameState& state, GameObjectId, PlayerId controller,
                          TriggerType, GameObjectId) const override {
        return controller != PlayerId::None && state.player(controller).rune_deck.empty();
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.scorePoints(ctx.controller, 1);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1077;
        d.def_id = R"RB(rad-160-167)RB";
        d.name = R"RB(Cosmic Vista)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-160/167)RB";
        d.collector_number = 160;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(When you hold here, if your Rune Deck is empty, you score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0efb6db0a41350e9cfc116b235eeb92bb05ed5bf-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1077(CardRegistry& r) {
    r.registerCard(1077, std::make_unique<CosmicVista_1077>());
}

}  // namespace riftbound
