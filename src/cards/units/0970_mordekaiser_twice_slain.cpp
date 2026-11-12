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

class MordekaiserTwiceSlain_970 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool hasReplacementEffect() const override { return true; }
    Applies replacementAppliesTo(const GameState& state, GameObjectId self, PlayerId, GameObjectId dying) const override {
        if (self != dying || !state.objectExists(self)) return Applies::No;
        if (state.getObject(self).card_counters.count("__mord_replaced")) return Applies::No;
        return Applies::Yes;
    }
    bool applyReplacement(CardContext& ctx, GameObjectId dying) override {
        auto friends = boardObjects(ctx.state, [&](const GameObject& o) {
            return o.isUnit() && o.controller == ctx.controller && o.id != dying;
        });
        if (friends.empty()) return false;
        if (!ctx.executor.confirmNow(ctx.controller, "Mordekaiser: kill a friendly unit to heal, exhaust, and recall me"))
            return false;
        auto id = ctx.executor.chooseObjectNow(ctx.controller, friends, "Mordekaiser: kill");
        if (id == kInvalidId) return false;
        ctx.state.getObject(dying).card_counters["__mord_replaced"] = 1;
        ctx.executor.killObject(id);
        ctx.executor.healExhaustRecall(dying);
        return true;
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 970;
        d.def_id = R"RB(rad-020-167)RB";
        d.name = R"RB(Mordekaiser, Twice Slain)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-020/167)RB";
        d.collector_number = 20;
        d.artist = R"RB()RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Noxus)RB", R"RB(Mordekaiser)RB"};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Accelerate);
        d.assault_value = 3;
        d.ability_text = R"RB([Accelerate] (You may pay [1][R] as an additional cost to have me enter ready.)
[Assault 3] (+3 [M] while I'm an attacker.)
Once each turn, if I would die, you may kill a friendly unit to heal me, exhaust me, and recall me instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/66db9510de566ac43157ecdcc96f33b5f553a38d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_970(CardRegistry& r) {
    r.registerCard(970, std::make_unique<MordekaiserTwiceSlain_970>());
}

}  // namespace riftbound
