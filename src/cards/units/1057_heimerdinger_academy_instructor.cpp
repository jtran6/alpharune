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

class HeimerdingerAcademyInstructor_1057 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool replacePlayedUnitToken(CardContext& ctx, std::string& name, int& might,
                                std::vector<std::string>& tags) const override {
        if (!ctx.state.objectExists(ctx.source)) return false;
        const auto& me = ctx.state.getObject(ctx.source);
        if (!me.location.has_value() || !std::holds_alternative<BattlefieldLocation>(*me.location)) return false;
        if (!ctx.executor.confirmNow(ctx.controller, "Heimerdinger: play a 3 [M] Mech instead?")) return false;
        name = "Mech"; might = 3; tags = {"Mech"};
        return true;
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1057;
        d.def_id = R"RB(rad-132-167)RB";
        d.name = R"RB(Heimerdinger, Academy Instructor)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-132/167)RB";
        d.collector_number = 132;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Yordle)RB", R"RB(Heimerdinger)RB", R"RB(Piltover)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(If you would play a unit token to a location while I'm at a battlefield, you may play a 3 [M] Mech unit token to that same location instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1795e905171c74b84b4b7f84a57fecbb4bbd6f75-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1057(CardRegistry& r) {
    r.registerCard(1057, std::make_unique<HeimerdingerAcademyInstructor_1057>());
}

}  // namespace riftbound
