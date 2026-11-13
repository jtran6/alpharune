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

class ClocktowerGuardian_1005 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIHold, TriggerType::WhenIDie};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.resuming) return;
        if (ctx.state.chain.resuming->fired_trigger == TriggerType::WhenIDie) {
            const GameObjectId top = banishTopUntilEnding(ctx, /*until_ending=*/false);
            if (top != kInvalidId)
                ctx.state.getObject(top).card_counters["__ignore_energy"] = 1;
            return;
        }
        ctx.executor.predict(ctx.controller, 3);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1005;
        d.def_id = R"RB(rad-067-167)RB";
        d.name = R"RB(Clocktower Guardian)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-067/167)RB";
        d.collector_number = 67;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Mech)RB"};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 8;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB(When you play me or when I hold, [Predict 3].
[Deathknell][>] Banish the top card of your Main Deck. You may play it, ignoring its Energy cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b846d604eee450a4ab8f457f2738b0596e7b361e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1005(CardRegistry& r) {
    r.registerCard(1005, std::make_unique<ClocktowerGuardian_1005>());
}

}  // namespace riftbound
