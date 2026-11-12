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

class SeraphineNotAlone_1061 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override { return {TriggerType::WhenYouPlayMe, TriggerType::WhenIBecomeExhausted}; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1, {"Recruit"}, {}, BaseLocation{ctx.controller});
    }
    bool hasMightDependentAura() const override { return true; }
    void applyMightDependentAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId self, GameObject& me) {
            int n = 0;
            for (auto id : state.allUnitsControlledBy(controller))
                if (id != self && state.getObject(id).is_exhausted) ++n;
            if (n) auraMight(me, self, n);
        });
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1061;
        d.def_id = R"RB(rad-138-167)RB";
        d.name = R"RB(Seraphine, Not Alone)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-138/167)RB";
        d.collector_number = 138;
        d.artist = R"RB()RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Piltover)RB", R"RB(Seraphine)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 1;
        d.rarity = Rarity::Epic;

        d.ability_text = R"RB(When you play me or when I become exhausted, play a 1 [M] Recruit unit token to your base.
I have +1 [M] for each other exhausted unit you control.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/756f0d5d9e83ba0ebcfaf1b69055fdc29458b084-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1061(CardRegistry& r) {
    r.registerCard(1061, std::make_unique<SeraphineNotAlone_1061>());
}

}  // namespace riftbound
