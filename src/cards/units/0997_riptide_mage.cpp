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

class RiptideMage_997 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller != ctx.controller; });
        const GameObjectId t = pickTarget(ctx, "Riptide Mage: an enemy gets -3 [M]", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        ctx.executor.giveTemporaryMight(t, -3);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 997;
        d.def_id = R"RB(rad-058-167)RB";
        d.name = R"RB(Riptide Mage)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-058/167)RB";
        d.collector_number = 58;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][B] as an additional cost to have me enter ready.)
When you play me, give an enemy unit -3 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4e3c24c1c315656772d0bfd8d100bc7893e94994-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_997(CardRegistry& r) {
    r.registerCard(997, std::make_unique<RiptideMage_997>());
}

}  // namespace riftbound
