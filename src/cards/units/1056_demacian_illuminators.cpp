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

class DemacianIlluminators_1056 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool triggersOncePerBatch(TriggerType) const override { return true; }
    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyUnitDies; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        auto t = pickTarget(ctx, "Demacian Illuminators: buff a friendly unit", units);
        if (finalizingChoices(ctx)) return;
        if (t != kInvalidId) ctx.executor.buffUnit(t);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1056;
        d.def_id = R"RB(rad-131-167)RB";
        d.name = R"RB(Demacian Illuminators)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-131/167)RB";
        d.collector_number = 131;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(When one or more friendly units die, [Buff] a friendly unit. (Give it a +1 [M] buff if it doesn't have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/127c2700ee4e2557dfd463ed84c45f4600ad9723-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1056(CardRegistry& r) {
    r.registerCard(1056, std::make_unique<DemacianIlluminators_1056>());
}

}  // namespace riftbound
