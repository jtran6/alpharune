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

class Rakelstake_1082 : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }

    // "For each player, the first time they play a unit or gear here each turn,
    // they Predict." One permission per player, shared by a unit and a gear.
    // The stamp is written as the ability is queued (acceptTrigger), so the
    // next play this turn is not a second first, and a countered ability still
    // counts (CR 383.1). A gear play reaches this card through
    // TriggerManager's battlefield walk: the card has no location, so the
    // in-play walk for WhenYouPlayAGear never sees it.
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayAUnit, TriggerType::WhenYouPlayAGear};
    }
    static std::string key(PlayerId p) {
        return std::string("__rakel_p") + std::to_string(static_cast<int>(p));
    }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType, GameObjectId subject) const override {
        if (subject == kInvalidId || !state.objectExists(subject)) return false;
        const auto here = battlefieldOfCard(state, source);
        if (!here) return false;
        const auto at = state.getObject(subject).battlefieldId();
        if (!at || *at != *here) return false;
        const auto stamped = state.getObject(source).card_counters.find(key(controller));
        return stamped == state.getObject(source).card_counters.end() ||
               stamped->second != state.turn.turn_number + 1;
    }
    void acceptTrigger(GameState& state, const TriggerEvent& ev) const override {
        if (!state.objectExists(ev.source)) return;
        state.getObject(ev.source).card_counters[key(ev.controller)] =
            state.turn.turn_number + 1;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.predict(ctx.controller, 1);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1082;
        d.def_id = R"RB(rad-165-167)RB";
        d.name = R"RB(Rakelstake)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-165/167)RB";
        d.collector_number = 165;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(For each player, the first time they play a unit or gear here each turn, they [Predict]. (They look at the top card of their Main Deck. They may recycle it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/53c096e4bc843a4cfe84fd16d922033b7fcfb330-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1082(CardRegistry& r) {
    r.registerCard(1082, std::make_unique<Rakelstake_1082>());
}

}  // namespace riftbound
