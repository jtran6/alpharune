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

class MeticulousRestorationist_1048 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When you play a card with Energy cost [7] or more, you may play me
    // from your trash for [Y]." A triggered ability from the trash (the same
    // shape as Forbidden Technique), not a play that stays legal for the rest
    // of the turn. [Y] replaces the printed base cost (CR 356.1.a). Further
    // cost increases still apply, and a Reaction [Add] can pay it.
    bool triggersWhileInTrash(TriggerType t) const override {
        return t == TriggerType::WhenYouPlayACard;
    }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayACard};
    }
    bool triggerConditionOn(const GameState& state, const TriggerEvent& event) const override {
        if (event.trigger != TriggerType::WhenYouPlayACard) return false;
        if (!event.subject_card || event.subject_card->energy_cost < 7) return false;
        return state.objectExists(event.source) &&
               state.getObject(event.source).zone == ZoneType::Trash;
    }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto in_trash = [&] {
            if (!ctx.state.objectExists(ctx.source)) return false;
            const auto& trash = ctx.state.player(ctx.state.getObject(ctx.source).owner).trash;
            return std::find(trash.begin(), trash.end(), ctx.source) != trash.end();
        };
        EffectPlayCost alt;
        alt.replace_energy = 0;
        alt.replace_power = 1;
        auto can_pay = [&] {
            if (!in_trash()) return false;
            if (ctx.executor.canPlayPayingCost(ctx.controller, ctx.source, alt)) return true;
            // [Add] abilities cover a short pool (the play is during a
            // resolution, so there is no priority window first).
            return ctx.executor.canPayWithAdds(ctx.controller, 0, 1,
                                               resource::maskOf(Domain::Order),
                                               SpendFor::PlayUnit);
        };
        const int go = confirmOptional(ctx,
            "Meticulous Restorationist: pay [Y] to play from trash?", can_pay);
        if (go < 1) return;
        if (finalizingChoices(ctx)) return;
        if (!in_trash()) return;
        const int where = pickEffectPlayLocation(*this, ctx,
            "Meticulous Restorationist: play me to", ctx.source);
        if (where == -1) return;   // nothing was paid
        if (!ctx.executor.playPayingCost(ctx.controller, ctx.source,
                effectPlayLocation(ctx.state, ctx.controller, where), alt))
            return;
        RIFT_TRACE(ctx.events, "METICULOUS RESTORATIONIST: paid [Y] -> played from trash");
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1048;
        d.def_id = R"RB(rad-120-167)RB";
        d.name = R"RB(Meticulous Restorationist)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-120/167)RB";
        d.collector_number = 120;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Device)RB"};
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 2;
        d.rarity = Rarity::Common;

        d.ability_text = R"RB(When you play a card with Energy cost [7] or more, you may play me from your trash for [Y].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ee50fbf5e902e7109a3d54b417edc440b5a37198-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1048(CardRegistry& r) {
    r.registerCard(1048, std::make_unique<MeticulousRestorationist_1048>());
}

}  // namespace riftbound
