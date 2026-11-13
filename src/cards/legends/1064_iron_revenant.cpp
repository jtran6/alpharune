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

class IronRevenant_1064 : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    bool abilityAddsResources() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto trash = ctx.state.player(ctx.controller).trash;
        std::vector<GameObjectId> opts;
        for (auto id : trash) {
            if (!ctx.state.objectExists(id)) continue;
            if (ctx.state.getObject(id).isUnit() || ctx.state.getObject(id).isGear()) opts.push_back(id);
        }
        auto t = pickUnseenCard(ctx, "Iron Revenant: banish a unit or gear from your trash", opts);
        if (t == kInvalidId) return;
        ctx.executor.banishObject(t);
        // Units or gear, not gear alone. gear_power would leave a unit play
        // unpaid (Meticulous Restorationist's [Y] is a unit's alternate cost).
        ctx.state.player(ctx.controller).rune_pool.unit_gear_power += 1;
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1064;
        d.def_id = R"RB(rad-143-167)RB";
        d.name = R"RB(Iron Revenant)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-143/167)RB";
        d.collector_number = 143;
        d.artist = R"RB(TUTU)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury, Domain::Order};
        d.tags = {R"RB(Mordekaiser)RB"};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([Reaction][>] Banish a unit or gear from your trash, [E]: [Add] [A]. Spend this Power only to play units or gear. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/90ec3319518771820909592eee963d34dab037bc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1064(CardRegistry& r) {
    r.registerCard(1064, std::make_unique<IronRevenant_1064>());
}

}  // namespace riftbound
