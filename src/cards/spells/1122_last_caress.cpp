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

class LastCaress_1122 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ps = ctx.state.player(ctx.controller);
        ps.next_unit_any_battlefield = 1;
        ps.next_unit_buff = 1;
    }
    // The spell is in the trash when the unit is played. "When you play it"
    // still triggers from there.
    bool triggersWhileInTrash(TriggerType t) const override {
        return t == TriggerType::WhenYouPlayAUnit;
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType trigger, GameObjectId) const override {
        if (trigger != TriggerType::WhenYouPlayAUnit) return false;
        if (!state.objectExists(source) || state.getObject(source).zone != ZoneType::Trash)
            return false;
        return state.player(controller).next_unit_buff > 0;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // The grant was spent when this unit was played (announceUnitPlayed),
        // so the ability buffs that unit even though the flag is already 0. A
        // later unit is not queued, and a counter does not hand the buff on.
        if (!ctx.state.chain.resuming) return;
        const auto sub = ctx.state.chain.resuming->triggering_subject;
        if (sub == kInvalidId) return;
        ctx.executor.buffUnit(sub);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1122;
        d.def_id = R"RB(rad-154-167)RB";
        d.name = R"RB(Last Caress)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-154/167)RB";
        d.collector_number = 154;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body, Domain::Chaos};
        d.tags = {R"RB(Evelynn)RB"};
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
The next unit you play this turn (even from face down) can be played to any battlefield. When you play it, [Buff] it. (Give it a +1 [M] buff if it doesn't have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c34881b6fe2e63f6345845fe95cfe72eb9d4878c-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1122(CardRegistry& r) {
    r.registerCard(1122, std::make_unique<LastCaress_1122>());
}

}  // namespace riftbound
