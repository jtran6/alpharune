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

class RallyForWar_1087 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId, GameObjectId) const override {
        return FinalizeChoice::No;
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        for (const auto& bf : state.battlefields) {
            for (const auto& [id, obj] : state.objects.atLocation(LocationId{BattlefieldLocation{bf.id}}))
                if (obj.isUnit() && obj.controller != player) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<std::string> labels;
        std::vector<int> idx;
        for (int i = 0; i < static_cast<int>(ctx.state.battlefields.size()); ++i) {
            const auto& bf = ctx.state.battlefields[i];
            bool occ = false;
            for (auto& [id, obj] : ctx.state.objects.atLocation(LocationId{BattlefieldLocation{bf.id}}))
                if (obj.isUnit() && obj.controller != ctx.controller) occ = true;
            if (!occ) continue;
            labels.push_back(battlefieldLabel(ctx.state, bf));
            idx.push_back(i);
        }
        if (labels.empty()) { if (finalizingChoices(ctx)) return; return; }
        int mode = pickMode(ctx, "Rally for War: an occupied enemy battlefield", static_cast<int>(labels.size()), labels);
        if (mode < 0) return;
        if (finalizingChoices(ctx)) return;
        auto loc = LocationId{BattlefieldLocation{ctx.state.battlefields[idx[mode]].id}};
        for (int n = 0; n < 3; ++n)
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1, {"Recruit"}, {}, loc);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1087;
        d.def_id = R"RB(rad-007-167)RB";
        d.name = R"RB(Rally for War)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-007/167)RB";
        d.collector_number = 7;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Common;

        d.ability_text = R"RB(Choose an occupied enemy battlefield. Play three 1 [M] Recruit unit tokens there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/09c0fc35d8f4b72f58a95d1745b99c4133d02513-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1087(CardRegistry& r) {
    r.registerCard(1087, std::make_unique<RallyForWar_1087>());
}

}  // namespace riftbound
