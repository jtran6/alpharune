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

class StoicFerryman_978 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source) || !ctx.state.getObject(ctx.source).location.has_value()) {
            if (finalizingChoices(ctx)) return;
            return;
        }
        LocationId here = *ctx.state.getObject(ctx.source).location;
        if (!std::holds_alternative<BattlefieldLocation>(here)) {
            if (finalizingChoices(ctx)) return;
            return;
        }
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.controller == ctx.controller && u.id != ctx.source && u.location.has_value() && *u.location == here;
        });
        int go = confirmOptional(ctx, "Stoic Ferryman: move another unit you control here to base", [ok=!units.empty()]{ return ok; });
        if (go < 0) return;
        if (finalizingChoices(ctx)) return;
        if (go != 1) return;
        auto t = pickTarget(ctx, "Stoic Ferryman: move to base", units);
        if (t != kInvalidId) ctx.executor.moveToBase(t);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 978;
        d.def_id = R"RB(rad-031-167)RB";
        d.name = R"RB(Stoic Ferryman)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-031/167)RB";
        d.collector_number = 31;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 2;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Shield);
        d.shield_value = 3;
        d.ability_text = R"RB([Shield 3] (+3 [M] while I'm a defender.)
When you play me to a battlefield, you may move another unit you control here to your base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8513fbe02a37f957b2a98bcee5badfdc56f41e82-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_978(CardRegistry& r) {
    r.registerCard(978, std::make_unique<StoicFerryman_978>());
}

}  // namespace riftbound
