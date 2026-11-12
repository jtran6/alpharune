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

class Cataclysm_1105 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId player,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, player, object);
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        return !chooseableUnits(state, player, [&](const GameObject& u) {
            return u.controller == player;
        }).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        auto t = pickTarget(ctx, "Cataclysm: move a friendly unit", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        ctx.state.player(opponent(ctx.controller)).cant_play_spells_this_turn = true;
        std::vector<std::string> labels;
        std::vector<BattlefieldId> ids;
        for (const auto& bf : ctx.state.battlefields) {
            if (!bf.controller.has_value() || *bf.controller == ctx.controller) continue;
            bool occ = false;
            for (auto& [id, obj] : ctx.state.objects.atLocation(LocationId{BattlefieldLocation{bf.id}}))
                if (obj.isUnit()) occ = true;
            if (!occ) continue;
            labels.push_back(battlefieldLabel(ctx.state, bf));
            ids.push_back(bf.id);
        }
        if (labels.empty()) return;
        int mode = pickMode(ctx, "Cataclysm: an occupied enemy battlefield", static_cast<int>(labels.size()), labels);
        if (mode >= 0) ctx.executor.moveToBattlefield(t, ids[mode]);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1105;
        d.def_id = R"RB(rad-084-167)RB";
        d.name = R"RB(Cataclysm)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-084/167)RB";
        d.collector_number = 84;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(Opponents can't play spells this turn.
Move a friendly unit to an occupied enemy battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/dc551ece6cf007c76a3a16feba5f26fb7dd4ed65-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1105(CardRegistry& r) {
    r.registerCard(1105, std::make_unique<Cataclysm_1105>());
}

}  // namespace riftbound
