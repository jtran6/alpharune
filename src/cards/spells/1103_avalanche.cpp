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

class Avalanche_1103 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<GameObjectId> units;
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            if (std::holds_alternative<BaseLocation>(*obj.location)) units.push_back(id);
        }
        for (auto id : units) ctx.executor.dealDamage(id, 3, ctx.source);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1103;
        d.def_id = R"RB(rad-079-167)RB";
        d.name = R"RB(Avalanche)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-079/167)RB";
        d.collector_number = 79;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.might = 0;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(Deal 3 to all units in bases.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7b2103352935061e19cf90036a6aa4cd35ad5922-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1103(CardRegistry& r) {
    r.registerCard(1103, std::make_unique<Avalanche_1103>());
}

}  // namespace riftbound
