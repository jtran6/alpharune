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

class DetonationAmplifier_967 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "Friendly Deathknell effects here", not every Deathknell that player
    // has. The global counter would double one at another battlefield.
    int extraDeathknellTriggers(const GameState& state, GameObjectId self,
                                const GameObject& dying) const override {
        if (!state.objectExists(self)) return 0;
        const auto& me = state.getObject(self);
        // A board wipe kills the amplifier in the same batch. Its location
        // is the one it left from, not the empty field after it left.
        // Only a location it has now, or the one taken when this death batch
        // began. A gear that died in an earlier cleanup still has last_location,
        // and that must not double a later Deathknell.
        std::optional<LocationId> mine;
        if (me.location.has_value()) mine = *me.location;
        else if (me.last_known.batch_pending && me.last_known.valid &&
                 me.last_known.location.has_value())
            mine = me.last_known.location;
        if (!mine.has_value()) return 0;
        std::optional<LocationId> where;
        if (dying.last_known.valid && dying.last_known.location.has_value())
            where = dying.last_known.location;
        else if (dying.location.has_value())
            where = *dying.location;
        else
            where = dying.last_location;
        if (!where.has_value() || *where != *mine) return 0;
        return 1;
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto opts = boardObjects(ctx.state, [&](const GameObject& o) {
            return o.controller == ctx.controller && o.id != ctx.source && (o.isUnit() || o.isGear());
        });
        int go = confirmOptional(ctx, "Detonation Amplifier: kill another friendly unit or gear to draw 1", [ok=!opts.empty()]{ return ok; });
        if (go < 0) return;
        // The kill is a cost, paid as this is finalized. At resolution the
        // object is already dead, so it is not on the board: asking again
        // treats the paid cost as a mistarget and the draw never happens.
        if (go == 1 && !costAlreadyPaid(ctx)) {
            const GameObjectId id = pickCostObject(ctx, "Detonation Amplifier: kill", opts);
            if (id == kInvalidId) return;
            if (costDueNow(ctx)) killAsCost(ctx, id);
        }
        if (finalizingChoices(ctx)) return;
        if (go == 1 && costAlreadyPaid(ctx) && !costUnpaid(ctx))
            ctx.executor.drawCards(ctx.controller, 1);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 967;
        d.def_id = R"RB(rad-017-167)RB";
        d.name = R"RB(Detonation Amplifier)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-017/167)RB";
        d.collector_number = 17;
        d.artist = R"RB(Fish Art Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Device)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Deploy);
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
When you play this, you may kill another friendly unit or gear to draw 1.
Friendly [Deathknell] effects here trigger an additional time.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0cd07e77e69a53b78486b07dd7610183c953d581-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_967(CardRegistry& r) {
    r.registerCard(967, std::make_unique<DetonationAmplifier_967>());
}

}  // namespace riftbound
