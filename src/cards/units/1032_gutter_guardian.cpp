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

class GutterGuardian_1032 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool matchesShowOff(const GameState&, const GameObject& obj) const override {
        return obj.hasKeyword(Keyword::Hidden);
    }
    bool triggeredAbilityAdds(TriggerType) const override { return true; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source) || !playedWithShowOff(ctx.state.getObject(ctx.source))) return;
        ctx.state.player(ctx.controller).rune_pool.universal_power += 1;
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1032;
        d.def_id = R"RB(rad-103-167)RB";
        d.name = R"RB(Gutter Guardian)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-103/167)RB";
        d.collector_number = 103;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::ShowOff);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
[Show Off] a card with [Hidden]. (As you play me, you may reveal such a card from hand or pick another friendly one on the board.)
When you play me, if you showed off, [Add] [A].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/756827744317003e4992163ff9bf1dc991682de8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1032(CardRegistry& r) {
    r.registerCard(1032, std::make_unique<GutterGuardian_1032>());
}

}  // namespace riftbound
