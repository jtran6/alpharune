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

class BilgewaterTracker_958 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; });
        auto picked = selectSeveralTargets(*this, ctx, "Bilgewater Tracker: deal 1", units, 2);
        if (!picked) return;
        if (finalizingChoices(ctx)) return;
        for (auto id : *picked) ctx.executor.dealDamage(id, 1, ctx.source);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 958;
        d.def_id = R"RB(rad-003-167)RB";
        d.name = R"RB(Bilgewater Tracker)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-003/167)RB";
        d.collector_number = 3;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.rarity = Rarity::Common;

        d.ability_text = R"RB(When you play me, deal 1 to each of up to 2 units.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b660759a6bcb9c7ca41ea969cf11463259e284c9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_958(CardRegistry& r) {
    r.registerCard(958, std::make_unique<BilgewaterTracker_958>());
}

}  // namespace riftbound
