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

class HighNote_1098 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId,
                                           GameObjectId object) const override {
        if (object == kInvalidId || !state.objectExists(object)) return FinalizeChoice::No;
        const auto& u = state.getObject(object);
        return (u.isUnit() && u.location.has_value()) ? FinalizeChoice::Yes : FinalizeChoice::No;
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        return !chooseableUnits(state, player, [](const GameObject&) { return true; }).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; });
        const GameObjectId t = pickTarget(ctx, "High Note: reduce a unit's Might", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        int n = 0;
        for (auto& [id, obj] : ctx.state.objects.onBoard())
            if (obj.controller == ctx.controller && obj.isUnit() && obj.is_exhausted) ++n;
        ctx.executor.giveTemporaryMight(t, -n);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1098;
        d.def_id = R"RB(rad-048-167)RB";
        d.name = R"RB(High Note)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-048/167)RB";
        d.collector_number = 48;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Give a unit –[M] this turn equal to the number of exhausted units you control.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/afba3669ea1063dafb0e4c2c766694e00540eb13-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1098(CardRegistry& r) {
    r.registerCard(1098, std::make_unique<HighNote_1098>());
}

}  // namespace riftbound
