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

class StarryEyedSongstress_1070 : public LegendCard {
public:
    const CardDef& def() const override { return def_; }

    int optionalSpellEnergyOff(const GameState& state, GameObjectId self) const override {
        if (!state.objectExists(self) || state.getObject(self).is_exhausted) return 0;
        int ready = 0;
        for (auto id : state.allUnitsControlledBy(state.getObject(self).controller))
            if (!state.getObject(id).is_exhausted) ++ready;
        return ready >= 3 ? 2 : 0;
    }
    int takeOptionalSpellEnergyOff(CardContext& ctx) const override {
        if (optionalSpellEnergyOff(ctx.state, ctx.source) < 2) return 0;
        if (!ctx.executor.confirmNow(ctx.controller,
                "Starry-Eyed Songstress: exhaust her and three units to reduce a spell by [2]"))
            return 0;
        // Which three is the player's: more than three ready units is a choice,
        // not the three with the lowest ids.
        std::vector<GameObjectId> picked;
        for (int n = 0; n < 3; ++n) {
            auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
                return u.controller == ctx.controller && !u.is_exhausted &&
                       std::find(picked.begin(), picked.end(), u.id) == picked.end();
            });
            if (static_cast<int>(units.size()) < 3 - n) return 0;
            static constexpr const char* kWhich[3] = {
                "Starry-Eyed Songstress: exhaust the first unit",
                "Starry-Eyed Songstress: exhaust the second unit",
                "Starry-Eyed Songstress: exhaust the third unit",
            };
            const GameObjectId id = ctx.executor.chooseObjectNow(
                ctx.controller, units, kWhich[n]);
            if (id == kInvalidId) return 0;
            picked.push_back(id);
        }
        ctx.executor.exhaustObject(ctx.source);
        for (auto id : picked) ctx.executor.exhaustObject(id);
        return 2;
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1070;
        d.def_id = R"RB(rad-151-167)RB";
        d.name = R"RB(Starry-Eyed Songstress)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-151/167)RB";
        d.collector_number = 151;
        d.artist = R"RB(Naifan Zhang)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Mind, Domain::Order};
        d.tags = {R"RB(Seraphine)RB"};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(As an additional cost to play a spell, you may exhaust me and three friendly units to reduce the cost of that spell by [2].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/399262205b15aa3b6306eb1d80a308791dcbe2ca-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1070(CardRegistry& r) {
    r.registerCard(1070, std::make_unique<StarryEyedSongstress_1070>());
}

}  // namespace riftbound
