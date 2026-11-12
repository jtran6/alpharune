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

class GravesBlastingThrough_1019 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override { return {TriggerType::WhenIAttack, TriggerType::WhenIDefend}; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source) || !ctx.state.getObject(ctx.source).location.has_value()) return;
        int might = ctx.state.getObject(ctx.source).current_might;
        LocationId here = *ctx.state.getObject(ctx.source).location;
        std::vector<GameObjectId> enemies;
        for (auto& [id, obj] : ctx.state.objects.atLocation(here))
            if (obj.isUnit() && obj.controller != ctx.controller) enemies.push_back(id);
        for (auto id : enemies) ctx.executor.dealDamage(id, might, ctx.source);
    }
    bool hasMightDependentAura() const override { return true; }
    void applyMightDependentAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject& me) { me.aura_no_combat_damage = true; });
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1019;
        d.def_id = R"RB(rad-085-167)RB";
        d.name = R"RB(Graves, Blasting Through)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-085/167)RB";
        d.collector_number = 85;
        d.artist = R"RB()RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Bilgewater)RB", R"RB(Graves)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(When I attack or defend, deal damage equal to my Might to all enemy units here.
I don't deal combat damage.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5bc0671f9f78630e2932052bb9f7f42ef2742c86-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1019(CardRegistry& r) {
    r.registerCard(1019, std::make_unique<GravesBlastingThrough_1019>());
}

}  // namespace riftbound
