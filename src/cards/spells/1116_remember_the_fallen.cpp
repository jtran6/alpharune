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

class RememberTheFallen_1116 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId, GameObjectId) const override {
        return FinalizeChoice::No;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& trash = ctx.state.player(ctx.controller).trash;
        std::vector<GameObjectId> opts;
        for (auto id : trash) {
            if (!ctx.state.objectExists(id)) continue;
            const auto& o = ctx.state.getObject(id);
            if (!o.isUnit() && !o.isGear()) continue;
            if (o.card_def_id == kInvalidId || !ctx.registry || !ctx.registry->get(o.card_def_id)) continue;
            const auto& d = ctx.registry->get(o.card_def_id)->def();
            if (d.energy_cost <= 3 && d.power_cost <= 1) opts.push_back(id);
        }
        auto picked = selectSeveral(ctx, "Remember the Fallen", opts, 3);
        if (!picked) return;
        if (finalizingChoices(ctx)) return;
        for (auto id : *picked) ctx.executor.playIgnoringCost(ctx.controller, id, LocationId{BaseLocation{ctx.controller}});
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1116;
        d.def_id = R"RB(rad-135-167)RB";
        d.name = R"RB(Remember the Fallen)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-135/167)RB";
        d.collector_number = 135;
        d.artist = R"RB(Original Force)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 0;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(Play up to 3 units and/or gear from your trash, each with Energy cost no more than [3] and Power cost no more than [A], ignoring their costs.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0bfd45446d5ffb816438ae2668ad4e5b7f188ad3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1116(CardRegistry& r) {
    r.registerCard(1116, std::make_unique<RememberTheFallen_1116>());
}

}  // namespace riftbound
