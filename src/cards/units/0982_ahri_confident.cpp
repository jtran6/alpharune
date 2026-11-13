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

class AhriConfident_982 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouReduceMight; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType, GameObjectId subject) const override {
        if (subject == kInvalidId || !state.objectExists(subject) || !state.objectExists(source))
            return false;
        const auto& enemy = state.getObject(subject);
        const auto& self = state.getObject(source);
        if (enemy.controller == controller) return false;
        if (!self.location.has_value() || !enemy.location.has_value()) return false;
        return *self.location == *enemy.location;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.giveTemporaryMight(ctx.source, 1);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 982;
        d.def_id = R"RB(rad-038-167)RB";
        d.name = R"RB(Ahri, Confident)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-038/167)RB";
        d.collector_number = 38;
        d.artist = R"RB(Naifan Zhang)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ahri)RB", R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Disarm);
        d.disarm_value = 1;
        d.ability_text = R"RB([Disarm] (When I attack, give an enemy unit here -1 [M] this turn.)
When you reduce the Might of an enemy unit here by 1 or more, give me +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cf3c6a5963ad27976960977b0a51ea185f551696-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_982(CardRegistry& r) {
    r.registerCard(982, std::make_unique<AhriConfident_982>());
}

}  // namespace riftbound
