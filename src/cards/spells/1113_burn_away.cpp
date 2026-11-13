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

class BurnAway_1113 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId player,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, player, object, [](const GameObject& u) {
            return u.current_might <= 3;
        });
    }
    bool hasLegalTargets(const GameState& state, PlayerId) const override {
        return !boardObjects(state, [](const GameObject& o) {
            return o.isGear() || (o.isUnit() && o.current_might <= 3);
        }).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto opts = boardObjects(ctx.state, [](const GameObject& o) {
            return o.isGear() || (o.isUnit() && o.current_might <= 3);
        });
        auto t = pickTarget(ctx, "Burn Away: banish a small unit or a gear", opts);
        if (finalizingChoices(ctx)) return;
        if (t != kInvalidId) ctx.executor.banishObject(t);
        ctx.executor.burn(ctx.controller, 2);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1113;
        d.def_id = R"RB(rad-124-167)RB";
        d.name = R"RB(Burn Away)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-124/167)RB";
        d.collector_number = 124;
        d.artist = R"RB(Michal Ivan)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Uncommon;

        d.ability_text = R"RB(Banish a unit with 3 [M] or less or a gear. [Burn 2]. (Put the top 2 cards of your Main Deck into your trash.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/751629a34ee6ba6476ce2f0a821b9f84b5562acd-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1113(CardRegistry& r) {
    r.registerCard(1113, std::make_unique<BurnAway_1113>());
}

}  // namespace riftbound
