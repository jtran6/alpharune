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

class DemonShade_1110 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId player,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, player, object);
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        return !friendly(state, player).empty();
    }
    // From facedown the friendly unit is at the battlefield the spell was
    // hidden at (CR 811.1.d.2). With none there the spell cannot be played.
    bool hasLegalChoicesHiddenAt(const GameState& state, PlayerId player, const CardDB&,
                                 BattlefieldId battlefield) const override {
        for (auto id : friendly(state, player)) {
            if (!state.objectExists(id)) continue;
            if (state.getObject(id).battlefieldId() == std::optional<BattlefieldId>{battlefield})
                return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = atHiddenBattlefield(ctx.state, ctx.source, friendly(ctx.state, ctx.controller));
        auto t = pickTarget(ctx, "Demon Shade: a friendly unit", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        std::vector<std::string> labels;
        std::vector<BattlefieldId> open;
        for (const auto& bf : ctx.state.battlefields) {
            if (bf.controller.has_value()) continue;
            labels.push_back("Open — " + battlefieldLabel(ctx.state, bf));
            open.push_back(bf.id);
        }
        if (!open.empty()) {
            int mode = pickMode(ctx, "Demon Shade: an open battlefield", static_cast<int>(labels.size()), labels);
            if (mode >= 0) ctx.executor.moveToBattlefield(t, open[mode]);
        } else {
            ctx.executor.giveTemporaryKeyword(t, Keyword::Ganking, 1);
        }
    }

private:
    static std::vector<GameObjectId> friendly(const GameState& state, PlayerId player) {
        return chooseableUnits(state, player, [&](const GameObject& u) {
            return u.controller == player;
        });
    }

    const CardDef def_ = [] {
        CardDef d;
        d.id = 1110;
        d.def_id = R"RB(rad-102-167)RB";
        d.name = R"RB(Demon Shade)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-102/167)RB";
        d.collector_number = 102;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
Choose a friendly unit, then choose one —
Move it to an open battlefield.
Give it [Ganking] this turn if there are no open battlefields. (It can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e9dd313886624aafc7e0c7f9e1689225c042b484-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1110(CardRegistry& r) {
    r.registerCard(1110, std::make_unique<DemonShade_1110>());
}

}  // namespace riftbound
