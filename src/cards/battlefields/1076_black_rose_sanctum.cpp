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

class BlackRoseSanctum_1076 : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyUnitDies; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.resuming) return;
        auto sub = ctx.state.chain.resuming->triggering_subject;
        if (sub == kInvalidId || !ctx.state.objectExists(sub)) return;
        if (ctx.state.getObject(sub).isToken()) return;
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1, {"Recruit"}, {}, BaseLocation{ctx.controller});
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1076;
        d.def_id = R"RB(rad-159-167)RB";
        d.name = R"RB(Black Rose Sanctum)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-159/167)RB";
        d.collector_number = 159;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Battlefield;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(For each player, the first time each turn a non-token unit they control dies here, they play a 1 [M] Recruit unit token to their base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/03ce18a231c7c919875b436b06bbe4e1a99c4831-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1076(CardRegistry& r) {
    r.registerCard(1076, std::make_unique<BlackRoseSanctum_1076>());
}

}  // namespace riftbound
