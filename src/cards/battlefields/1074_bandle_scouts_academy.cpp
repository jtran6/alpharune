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

class BandleScoutsAcademy_1074 : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouHideACard; }
    // "When you hide a card here." "You" is this battlefield's controller, or
    // the turn player while it is uncontrolled (CR 190.6). The hide is here
    // when the hidden card's battlefield is this one. Judged as the card is
    // hidden (CR 383.2.a.1).
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType, GameObjectId subject) const override {
        if (subject == kInvalidId || !state.objectExists(subject)) return false;
        const auto here = battlefieldOfCard(state, source);
        if (!here || state.getObject(subject).hidden_at != *here) return false;
        PlayerId you = PlayerId::None;
        for (const auto& bf : state.battlefields)
            if (bf.id == *here) you = bf.controller.value_or(state.turn.turn_player);
        return you != PlayerId::None && controller == you;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        createGoldExhausted(ctx);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1074;
        d.def_id = R"RB(rad-157-167)RB";
        d.name = R"RB(Bandle Scouts' Academy)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-157/167)RB";
        d.collector_number = 157;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(When you hide a card here, play a Gold gear token exhausted. (It has "[Reaction][>] Kill this, [E]: [Add] [A]."))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f5bb93e1ad9005fd42a9129b91f72877f1dedbe1-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1074(CardRegistry& r) {
    r.registerCard(1074, std::make_unique<BandleScoutsAcademy_1074>());
}

}  // namespace riftbound
