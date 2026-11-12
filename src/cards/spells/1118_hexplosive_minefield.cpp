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

class HexplosiveMinefield_1118 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId, GameObjectId) const override {
        return FinalizeChoice::No;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto bombs = boardObjects(ctx.state, [&](const GameObject& o) {
            return o.controller == ctx.controller &&
                   (o.card_def_id.raw() == kBombGearCardDefId.raw() ||
                    std::find(o.tags.begin(), o.tags.end(), "Bomb") != o.tags.end());
        });
        auto picked = selectSeveral(ctx, "Hexplosive Minefield: kill friendly Bombs", bombs, static_cast<int>(bombs.size()));
        if (!picked) return;
        if (finalizingChoices(ctx)) return;
        int n = static_cast<int>(picked->size());
        for (auto id : *picked) ctx.executor.killObject(id);
        if (ctx.state.objectExists(ctx.source)) ctx.state.getObject(ctx.source).card_counters["__mines"] = n;
        atEndOfTurnAbout(ctx, cardDefId(), ctx.source, 0);
    }
    TriggerType triggerType() const override { return TriggerType::AtEndOfTurn; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        int n = 0;
        if (ctx.state.objectExists(ctx.source)) n = ctx.state.getObject(ctx.source).card_counters["__mines"];
        if (n <= 0 || ctx.state.battlefields.empty()) return;
        const int mode = pickBattlefield(*this, ctx, "Hexplosive Minefield: a battlefield");
        if (mode < 0 || mode >= static_cast<int>(ctx.state.battlefields.size())) return;
        KeywordSet kw; kw.set(Keyword::Deploy); kw.set(Keyword::Deathknell);
        LocationId loc{BattlefieldLocation{ctx.state.battlefields[mode].id}};
        for (int i = 0; i < n; ++i)
            ctx.executor.createToken(ctx.controller, CardType::Gear, "Bomb", 0, {}, kw, loc,
                                     false, {}, kBombGearCardDefId);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1118;
        d.def_id = R"RB(rad-142-167)RB";
        d.name = R"RB(Hexplosive Minefield)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-142/167)RB";
        d.collector_number = 142;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Chaos};
        d.tags = {R"RB(Ziggs)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Kill any number of friendly Bomb tokens. At end of turn, play that many Bomb tokens to a single battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/65b599befcc6ed873de0e7965bf497a805505327-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1118(CardRegistry& r) {
    r.registerCard(1118, std::make_unique<HexplosiveMinefield_1118>());
}

}  // namespace riftbound
