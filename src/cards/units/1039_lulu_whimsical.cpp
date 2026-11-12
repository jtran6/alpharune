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

class LuluWhimsical_1039 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.id != ctx.source; });
        int go = confirmOptional(ctx, "Lulu: a unit's base Might becomes 4 this turn", [ok=!units.empty()]{ return ok; });
        if (go < 0) return;
        if (finalizingChoices(ctx)) return;
        if (go != 1) return;
        auto t = pickTarget(ctx, "Lulu: base Might 4", units);
        if (t != kInvalidId) {
            ctx.state.getObject(t).base_might_this_turn = 4;
            ctx.state.getObject(t).recomputeMight();
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1039;
        d.def_id = R"RB(rad-110-167)RB";
        d.name = R"RB(Lulu, Whimsical)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-110/167)RB";
        d.collector_number = 110;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Yordle)RB", R"RB(Bandle City)RB", R"RB(Lulu)RB"};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(When I move, you may have another unit's base Might become 4 this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/defd09480671a1e394725f21b6c3448ebec77913-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1039(CardRegistry& r) {
    r.registerCard(1039, std::make_unique<LuluWhimsical_1039>());
}

}  // namespace riftbound
