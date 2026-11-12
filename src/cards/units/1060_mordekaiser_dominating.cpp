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

class MordekaiserDominating_1060 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override { return {TriggerType::WhenYouPlayMe, TriggerType::WhenIAttack}; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto opts = boardObjects(ctx.state, [](const GameObject& o) { return o.isUnit() || o.isGear(); });
        const bool can = !opts.empty() && !ctx.state.player(ctx.controller).hand.empty();
        int go = confirmOptional(ctx, "Mordekaiser: discard 1 to kill a unit or gear", [ok=can]{ return ok; });
        if (go < 0) return;
        GameObjectId t = kInvalidId;
        if (go == 1) {
            t = pickTarget(ctx, "Mordekaiser: kill", opts);
            if (t == kInvalidId) return;
            // The discard is the cost, paid once as this is finalized. The
            // kill is the effect and runs after the marker, so a prompt in
            // the discard cannot kill twice.
            if (costDueNow(ctx)) ctx.executor.discardCards(ctx.controller, 1);
        }
        if (finalizingChoices(ctx)) return;
        if (go == 1 && t != kInvalidId) ctx.executor.killObject(t);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1060;
        d.def_id = R"RB(rad-137-167)RB";
        d.name = R"RB(Mordekaiser, Dominating)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-137/167)RB";
        d.collector_number = 137;
        d.artist = R"RB(JunHuan)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Noxus)RB", R"RB(Mordekaiser)RB"};
        d.energy_cost = 9;
        d.power_cost = 2;
        d.might = 8;
        d.rarity = Rarity::Epic;

        d.ability_text = R"RB(When you play me or when I attack, you may discard 1 to kill a unit or gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/43068166258356908b364566abcfc57123e96caa-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1060(CardRegistry& r) {
    r.registerCard(1060, std::make_unique<MordekaiserDominating_1060>());
}

}  // namespace riftbound
