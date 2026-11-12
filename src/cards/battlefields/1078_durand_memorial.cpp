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

class DurandMemorial_1078 : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenIDefend; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        auto t = pickTarget(ctx, "Durand Memorial: Tank this combat", units);
        if (finalizingChoices(ctx)) return;
        if (t != kInvalidId) ctx.executor.giveKeywordThisCombat(t, Keyword::Tank, 1, BattlefieldId::fromRaw(0));
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1078;
        d.def_id = R"RB(rad-161-167)RB";
        d.name = R"RB(Durand Memorial)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-161/167)RB";
        d.collector_number = 161;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB(When you defend here, choose a unit you control here. Give it [Tank] this combat. (It must be assigned combat damage first.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b565fec4e11308f5f01416a1c914d50a1d17ca19-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1078(CardRegistry& r) {
    r.registerCard(1078, std::make_unique<DurandMemorial_1078>());
}

}  // namespace riftbound
