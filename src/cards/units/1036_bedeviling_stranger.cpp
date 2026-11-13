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

class BedevilingStranger_1036 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.state.battlefields.empty()) { if (finalizingChoices(ctx)) return; return; }
        std::vector<std::string> labels;
        for (const auto& bf : ctx.state.battlefields)
            labels.push_back(battlefieldLabel(ctx.state, bf));
        int mode = pickMode(ctx, "Bedeviling Stranger: a battlefield", static_cast<int>(labels.size()), labels);
        if (mode < 0) return;
        if (finalizingChoices(ctx)) return;
        LocationId there{BattlefieldLocation{ctx.state.battlefields[mode].id}};
        std::vector<GameObjectId> enemies;
        int n = 0;
        for (auto& [id, obj] : ctx.state.objects.atLocation(there)) {
            if (!obj.isUnit()) continue;
            ++n;
            if (obj.controller != ctx.controller) enemies.push_back(id);
        }
        if (n < 4 || enemies.empty()) return;
        int go = confirmOptional(ctx, "Bedeviling Stranger: return an enemy unit there", []{ return true; });
        if (go == 1) {
            auto t = pickTarget(ctx, "Bedeviling Stranger: return", enemies);
            if (t != kInvalidId) ctx.executor.bounceToHand(t);
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1036;
        d.def_id = R"RB(rad-107-167)RB";
        d.name = R"RB(Bedeviling Stranger)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-107/167)RB";
        d.collector_number = 107;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
When you play me, choose a battlefield. If there are 4 or more units there, you may return an enemy unit there to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d112a706fe957e5cea58ea1e4366ac02e3ba292b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1036(CardRegistry& r) {
    r.registerCard(1036, std::make_unique<BedevilingStranger_1036>());
}

}  // namespace riftbound
