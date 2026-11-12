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

class ArmoredTusker_1015 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; });
        int go = confirmOptional(ctx, "Armored Tusker: deal damage equal to my Shield", [ok=!units.empty()]{ return ok; });
        if (go < 0) return;
        if (finalizingChoices(ctx)) return;
        if (go != 1) return;
        auto t = pickTarget(ctx, "Armored Tusker: deal damage", units);
        if (t == kInvalidId || !ctx.state.objectExists(ctx.source)) return;
        ctx.executor.dealDamage(t, std::max(1, ctx.state.getObject(ctx.source).shield_value), ctx.source);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1015;
        d.def_id = R"RB(rad-078-167)RB";
        d.name = R"RB(Armored Tusker)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-078/167)RB";
        d.collector_number = 78;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Tank);
        d.shield_value = 3;
        d.ability_text = R"RB([Shield 3] (+3 [M] while I'm a defender.)
[Tank] (I must be assigned combat damage first.)
When I move, you may deal damage equal to my [Shield] to a unit at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c479f2fda00506e71a57ac4e8cc297f7f3110a54-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1015(CardRegistry& r) {
    r.registerCard(1015, std::make_unique<ArmoredTusker_1015>());
}

}  // namespace riftbound
