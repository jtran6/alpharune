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

class IndustriousMechsmith_990 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        LocationId here = BaseLocation{ctx.controller};
        if (ctx.state.objectExists(ctx.source) && ctx.state.getObject(ctx.source).location.has_value())
            here = *ctx.state.getObject(ctx.source).location;
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Mech", 3, {"Mech"}, {}, here);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 990;
        d.def_id = R"RB(rad-049-167)RB";
        d.name = R"RB(Industrious Mechsmith)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-049/167)RB";
        d.collector_number = 49;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 1;
        d.rarity = Rarity::Common;

        d.ability_text = R"RB(When you play me, play a 3 [M] Mech unit token here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c8f6b4191874f23b522566f8ebeeb6e5ee18eb7f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_990(CardRegistry& r) {
    r.registerCard(990, std::make_unique<IndustriousMechsmith_990>());
}

}  // namespace riftbound
