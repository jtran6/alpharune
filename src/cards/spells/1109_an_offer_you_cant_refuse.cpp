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

class AnOfferYouCanTRefuse_1109 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId, GameObjectId) const override {
        return FinalizeChoice::No;
    }
    bool hasLegalTargets(const GameState& state, PlayerId) const override {
        return !boardObjects(state, [](const GameObject& o) { return o.isGear(); }).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto gears = boardObjects(ctx.state, [](const GameObject& o) { return o.isGear(); });
        auto t = pickTarget(ctx, "An Offer You Can't Refuse: kill a gear", gears);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        PlayerId who = ctx.state.getObject(t).controller;
        ctx.executor.killObject(t);
        CardContext theirs = ctx; theirs.controller = who;
        createGoldExhausted(theirs);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1109;
        d.def_id = R"RB(rad-101-167)RB";
        d.name = R"RB(An Offer You Can't Refuse)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-101/167)RB";
        d.collector_number = 101;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Kill a gear. Its controller plays a Gold gear token exhausted. (It has "[Reaction][>] Kill this, [E]: [Add] [A]."))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f724f95257abeacee82c774eff64973b56035357-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1109(CardRegistry& r) {
    r.registerCard(1109, std::make_unique<AnOfferYouCanTRefuse_1109>());
}

}  // namespace riftbound
