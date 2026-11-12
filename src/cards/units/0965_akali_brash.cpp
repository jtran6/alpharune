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

class AkaliBrash_965 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool triggeredAbilityAdds(TriggerType) const override { return true; }
    std::vector<TriggerType> triggerTypes() const override { return {TriggerType::WhenIMove}; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ps = ctx.state.player(ctx.controller);
        int n = 0;
        if (ctx.state.objectExists(ctx.source)) {
            auto it = ctx.state.getObject(ctx.source).card_counters.find("__akali_moves");
            n = it == ctx.state.getObject(ctx.source).card_counters.end() ? 0 : it->second;
            ctx.state.getObject(ctx.source).card_counters["__akali_moves"] = n + 1;
        }
        if (n == 0) {
            ps.rune_pool.energy += 1;
            ps.rune_pool.universal_power += 1;
        } else if (n == 1) {
            banishTopUntilEnding(ctx);
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 965;
        d.def_id = R"RB(rad-015-167)RB";
        d.name = R"RB(Akali, Brash)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-015/167)RB";
        d.collector_number = 15;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Ionia)RB", R"RB(Akali)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(The first time I move each turn, [Add] [1][A].
The second time I move each turn, banish the top card of your Main Deck. Until your next Ending Phase, you may play that card. (You still pay its costs.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/20997ddaef2cd714c36e5473c156faf331b80b7b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_965(CardRegistry& r) {
    r.registerCard(965, std::make_unique<AkaliBrash_965>());
}

}  // namespace riftbound
