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

class SanctumConservator_977 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayACard; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.resuming) return;
        auto sub = ctx.state.chain.resuming->triggering_subject;
        if (sub == kInvalidId || !ctx.state.objectExists(sub)) return;
        auto id = ctx.state.getObject(sub).card_def_id;
        if (id == kInvalidId || !ctx.registry || ctx.registry->get(id) == nullptr) return;
        if (ctx.registry->get(id)->def().energy_cost < 7) return;
        ctx.executor.readyObject(ctx.source);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 977;
        d.def_id = R"RB(rad-030-167)RB";
        d.name = R"RB(Sanctum Conservator)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-030/167)RB";
        d.collector_number = 30;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.energy_cost = 5;
        d.power_cost = 0;
        d.might = 5;
        d.rarity = Rarity::Common;

        d.ability_text = R"RB(When you play a card with Energy cost [7] or more, ready me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/776973101613a15a7d0a2fc90587d19098ecc894-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_977(CardRegistry& r) {
    r.registerCard(977, std::make_unique<SanctumConservator_977>());
}

}  // namespace riftbound
