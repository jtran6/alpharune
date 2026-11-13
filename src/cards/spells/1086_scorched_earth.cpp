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

class ScorchedEarth_1086 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId,
                                           GameObjectId object) const override {
        if (object == kInvalidId || !state.objectExists(object)) return FinalizeChoice::No;
        const auto& u = state.getObject(object);
        return (u.isUnit() && u.isAtBattlefield()) ? FinalizeChoice::Yes : FinalizeChoice::No;
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        return !chooseableUnits(state, player, [](const GameObject& u) {
            return u.isAtBattlefield();
        }).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // splitDamageAmong replaces resume_data when it starts. The chosen
        // battlefield is kept on the chain item, which Repeat resets, so the
        // next execution asks again. A card counter survived that reset and
        // skipped the prompt.
        int stored = -1;
        if (ctx.state.chain.resuming.has_value())
            stored = ctx.state.chain.resuming->execution_memo;
        if (stored < 0) {
            std::vector<std::string> labels;
            std::vector<int> idx;
            for (int i = 0; i < static_cast<int>(ctx.state.battlefields.size()); ++i) {
                bool any = false;
                LocationId there{BattlefieldLocation{ctx.state.battlefields[i].id}};
                for (const auto& [id, obj] : ctx.state.objects.atLocation(there)) {
                    (void)id;
                    if (!obj.isUnit() || !obj.location.has_value()) continue;
                    // Same rule as chooseableUnits: a unit the caster controls
                    // is damageable even when an enemy could not choose it.
                    if (obj.controller == ctx.controller || !obj.untargetable_by_enemy)
                        any = true;
                }
                if (!any) continue;
                const auto bf_card = ctx.state.battlefields[i].card_object_id;
                labels.push_back(ctx.state.objectExists(bf_card)
                    ? ctx.state.getObject(bf_card).name.get()
                    : "Battlefield #" + std::to_string(static_cast<int>(ctx.state.battlefields[i].id.raw())));
                idx.push_back(i);
            }
            if (labels.empty()) {
                std::vector<GameObjectId> damaged;
                splitDamageAmong(ctx, "Scorched Earth", 5, ctx.source, {}, damaged);
                return;
            }
            int mode = pickMode(ctx, "Scorched Earth: a battlefield", static_cast<int>(labels.size()), labels);
            if (mode < 0) return;
            if (mode >= static_cast<int>(idx.size())) {
                if (finalizingChoices(ctx)) return;
                return;
            }
            stored = idx[mode];
            if (ctx.state.chain.resuming.has_value())
                ctx.state.chain.resuming->execution_memo = stored;
        }
        if (stored < 0 || stored >= static_cast<int>(ctx.state.battlefields.size())) return;
        std::vector<GameObjectId> units;
        LocationId there{BattlefieldLocation{ctx.state.battlefields[stored].id}};
        units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.location.has_value() && *u.location == there;
        });
        std::vector<GameObjectId> damaged;
        splitDamageAmong(ctx, "Scorched Earth", 5, ctx.source, units, damaged);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1086;
        d.def_id = R"RB(rad-006-167)RB";
        d.name = R"RB(Scorched Earth)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-006/167)RB";
        d.collector_number = 6;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Deal 5 damage split among units at a single battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e646f9850571e20eca22e07af5d4dd68813fdd19-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1086(CardRegistry& r) {
    r.registerCard(1086, std::make_unique<ScorchedEarth_1086>());
}

}  // namespace riftbound
