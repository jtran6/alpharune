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

class RunebladeAnvil_963 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenILeaveTheBoard};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        banishTopUntilEnding(ctx);
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c; c.energy = 2; c.power = 1; c.power_domain = Domain::Chaos; c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.killObject(ctx.source);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 963;
        d.def_id = R"RB(rad-012-167)RB";
        d.name = R"RB(Runeblade Anvil)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-012/167)RB";
        d.collector_number = 12;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 1;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(When you play this or when this leaves the board, banish the top card of your Main Deck. Until your next Ending Phase, you may play that card. (You still pay its costs.)
[2][P], [E]: Kill this.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/11e2ddea70ec4dd8e562681a1ab1dfbc2b2dca0f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_963(CardRegistry& r) {
    r.registerCard(963, std::make_unique<RunebladeAnvil_963>());
}

}  // namespace riftbound
