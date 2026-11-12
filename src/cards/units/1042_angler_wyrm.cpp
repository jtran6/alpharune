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

class AnglerWyrm_1042 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, opponent(ctx.controller), [&](const GameObject& u) {
            return u.controller == opponent(ctx.controller);
        });
        if (units.empty() || !ctx.state.objectExists(ctx.source) || !ctx.state.getObject(ctx.source).location.has_value()) return;
        auto* bf = std::get_if<BattlefieldLocation>(&*ctx.state.getObject(ctx.source).location);
        if (!bf) return;
        auto t = pickTarget(ctx, "Angler Wyrm: they move a unit here", units);
        if (t != kInvalidId) ctx.executor.moveToBattlefield(t, bf->id);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1042;
        d.def_id = R"RB(rad-113-167)RB";
        d.name = R"RB(Angler Wyrm)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-113/167)RB";
        d.collector_number = 113;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 8;
        d.power_cost = 0;
        d.might = 8;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Deflect);
        d.deflect_value = 1;
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
[E]: Choose an opponent. They must move a unit they control to my battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e805eba9add03fd3e1b1601b15c12a46fd403282-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1042(CardRegistry& r) {
    r.registerCard(1042, std::make_unique<AnglerWyrm_1042>());
}

}  // namespace riftbound
