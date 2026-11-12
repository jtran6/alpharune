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

class PortablePortal_1021 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId self, GameObject& me) {
            if (!me.location.has_value()) return;
            for (auto& [id, obj] : state.objects.atLocation(*me.location))
                if (obj.isUnit() && obj.controller == controller) auraKeyword(obj, self, Keyword::Ganking);
        });
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            if (!u.location.has_value() || !ctx.state.objectExists(ctx.source) || !ctx.state.getObject(ctx.source).location.has_value()) return false;
            return *u.location == *ctx.state.getObject(ctx.source).location;
        });
        int go = confirmOptional(ctx, "Portable Portal: pay [O] to ready a unit here", [ok=!units.empty()]{ return ok; });
        if (go < 0) return;
        if (finalizingChoices(ctx)) return;
        if (go != 1) return;
        if (!ctx.executor.payResources(ctx.controller, 0, 1, resource::maskOf(Domain::Body), SpendFor::Other)) return;
        auto t = pickTarget(ctx, "Portable Portal: ready", units);
        if (t != kInvalidId) ctx.executor.readyObject(t);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1021;
        d.def_id = R"RB(rad-087-167)RB";
        d.name = R"RB(Portable Portal)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-087/167)RB";
        d.collector_number = 87;
        d.artist = R"RB()RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Device)RB"};
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Deploy);
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
When you play this, you may pay [O] to ready a unit here.
Friendly units here have [Ganking]. (They can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/dba5c879aae5c614198eb8408e0104a1b0618102-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1021(CardRegistry& r) {
    r.registerCard(1021, std::make_unique<PortablePortal_1021>());
}

}  // namespace riftbound
