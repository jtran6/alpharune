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

class EntrenchedBoar_975 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool entersReadyOnPlay(const GameState& state, PlayerId, const GameObject& self) const override {
        return self.card_counters.count("__boar_ready") && self.card_counters.at("__boar_ready") == 1;
    }
    void asYouPlayChoices(CardContext& ctx) override {
        auto& me = ctx.state.getObject(ctx.source);
        if (ctx.executor.confirmNow(ctx.controller, "Entrenched Boar: enter ready (then you can't move this turn)")) {
            me.card_counters["__boar_ready"] = 1;
            me.cant_move_this_turn = true;
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 975;
        d.def_id = R"RB(rad-026-167)RB";
        d.name = R"RB(Entrenched Boar)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-026/167)RB";
        d.collector_number = 26;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {};
        d.tags = {};
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 2;
        d.rarity = Rarity::Common;

        d.ability_text = R"RB(You may have me enter ready. If you do, I can't move this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8d8688f43e219f7f4b5f5fd17cff204dcebc3bdc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_975(CardRegistry& r) {
    r.registerCard(975, std::make_unique<EntrenchedBoar_975>());
}

}  // namespace riftbound
