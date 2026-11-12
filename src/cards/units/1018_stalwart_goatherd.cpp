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

class StalwartGoatherd_1018 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto gears = boardObjects(ctx.state, [&](const GameObject& o) {
            if (!o.isGear()) return false;
            if (o.card_def_id == kInvalidId || !ctx.registry || !ctx.registry->get(o.card_def_id)) return true;
            return ctx.registry->get(o.card_def_id)->def().energy_cost <= 4;
        });
        int go = confirmOptional(ctx, "Stalwart Goatherd: kill a gear with Energy cost 4 or less", [ok=!gears.empty()]{ return ok; });
        if (go < 0) return;
        if (finalizingChoices(ctx)) return;
        if (go != 1) return;
        auto t = pickTarget(ctx, "Stalwart Goatherd: kill a gear", gears);
        if (t != kInvalidId) ctx.executor.killObject(t);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1018;
        d.def_id = R"RB(rad-083-167)RB";
        d.name = R"RB(Stalwart Goatherd)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-083/167)RB";
        d.collector_number = 83;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {};
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Shield);
        d.shield_value = 2;
        d.ability_text = R"RB([Shield 2] (+2 [M] while I'm a defender.)
When you play me, you may kill a gear with Energy cost [4] or less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bff85727b58017cbd3921fd73e9b363f5a0205e4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1018(CardRegistry& r) {
    r.registerCard(1018, std::make_unique<StalwartGoatherd_1018>());
}

}  // namespace riftbound
