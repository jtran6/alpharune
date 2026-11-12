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

class RousingDisplay_1114 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    bool matchesShowOff(const CardContext& ctx, const GameObject& obj) const override {
        if (obj.isToken()) return false;
        if (obj.card_def_id == kInvalidId || !ctx.registry || !ctx.registry->get(obj.card_def_id)) return false;
        return ctx.registry->get(obj.card_def_id)->def().energy_cost >= 7;
    }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId, GameObjectId) const override {
        return FinalizeChoice::No;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (finalizingChoices(ctx)) return;
        int n = (spellShowOff(ctx) && spellShowOff(ctx)->did) ? 4 : 2;
        for (int i = 0; i < n; ++i)
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1, {"Recruit"}, {}, BaseLocation{ctx.controller});
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1114;
        d.def_id = R"RB(rad-127-167)RB";
        d.name = R"RB(Rousing Display)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-127/167)RB";
        d.collector_number = 127;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};
        d.energy_cost = 5;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::ShowOff);
        d.ability_text = R"RB([Show Off] a card with Energy cost [7] or more. (As you play this, you may reveal such a card from hand or pick a friendly one.)
Play two 1 [M] Recruit unit tokens to your base. If you showed off, play four to your base instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/18aa7cab082fd0e293260bcf480a1a76d12b64d9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1114(CardRegistry& r) {
    r.registerCard(1114, std::make_unique<RousingDisplay_1114>());
}

}  // namespace riftbound
