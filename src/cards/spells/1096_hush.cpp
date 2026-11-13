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

class Hush_1096 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId, GameObjectId) const override {
        return FinalizeChoice::No;
    }
    bool hasLegalTargets(const GameState& state, PlayerId, const CardDB& db) const override {
        return anyChainItem(state, [&](const ChainItem& ci) {
            if (!ci.is_spell || !state.objectExists(ci.source)) return false;
            const auto def = state.getObject(ci.source).card_def_id;
            if (def == kInvalidId || db.all().count(def) == 0) return false;
            return db.get(def).power_cost >= 2;
        });
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto pred = [&](const ChainItem& ci) {
            if (!ci.is_spell || !ctx.state.objectExists(ci.source) || !ctx.registry) return false;
            auto def = ctx.state.getObject(ci.source).card_def_id;
            if (def == kInvalidId || !ctx.registry->get(def)) return false;
            return ctx.registry->get(def)->def().power_cost >= 2;
        };
        auto spell = pickChainItem(ctx, "Hush: counter a spell with Power cost [A][A] or more", chainItemChoices(ctx, pred));
        if (!spell.has_value()) return;
        if (finalizingChoices(ctx)) return;
        if (*spell != kInvalidId) counterChosenItem(ctx, *spell);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1096;
        d.def_id = R"RB(rad-041-167)RB";
        d.name = R"RB(Hush)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-041/167)RB";
        d.collector_number = 41;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {};
        d.energy_cost = 1;
        d.power_cost = 2;
        d.might = 0;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Counter a spell with Power cost [A][A] or more.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b9c1b9441e42639805b715466938ccd26b89892d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1096(CardRegistry& r) {
    r.registerCard(1096, std::make_unique<Hush_1096>());
}

}  // namespace riftbound
