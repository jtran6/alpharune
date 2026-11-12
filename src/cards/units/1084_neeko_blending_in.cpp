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

class NeekoBlendingIn_1084 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const auto& legends = ctx.state.player(ctx.controller).neeko_legends;
        if (legends.empty()) { if (finalizingChoices(ctx)) return; return; }
        std::vector<std::string> labels;
        for (auto id : legends) labels.push_back("Legend " + std::to_string(id.raw()));
        int mode = pickMode(ctx, "Neeko: a legend while I'm on the board", static_cast<int>(labels.size()), labels);
        if (mode < 0) return;
        if (finalizingChoices(ctx)) return;
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).card_counters["__neeko_legend"] = legends[mode].raw();
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1084;
        d.def_id = R"RB(rad-167-167)RB";
        d.name = R"RB(Neeko, Blending In)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-167/167)RB";
        d.collector_number = 167;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {};
        d.tags = {R"RB(Ixtal)RB", R"RB(Neeko)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Epic;

        d.ability_text = R"RB((Neutral cards can go in decks of any Domain.)
If Neeko is in your deck, choose 3 different legends in addition to your starting legend. (Different means none match names.)
When you play me, choose one of those legends and add it to your Legend Zone while I'm on the board.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/607da6f534ce1b960ad02586ed4cd47c996e2c42-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1084(CardRegistry& r) {
    r.registerCard(1084, std::make_unique<NeekoBlendingIn_1084>());
}

}  // namespace riftbound
