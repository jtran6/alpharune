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

class HuntingBlind_987 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId self, GameObject& me) {
            if (!me.location.has_value()) return;
            for (auto& [id, obj] : state.objects.atLocation(*me.location))
                if (obj.isUnit() && obj.controller == controller) auraKeyword(obj, self, Keyword::Shield, 2);
        });
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto enemies = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller != ctx.controller; });
        int go = confirmOptional(ctx, "Hunting Blind: move an enemy unit here", [ok=!enemies.empty()]{ return ok; });
        if (go < 0) return;
        GameObjectId t = kInvalidId;
        if (go == 1) {
            t = pickTarget(ctx, "Hunting Blind: move here", enemies);
            if (t == kInvalidId) return;
        }
        if (finalizingChoices(ctx)) return;
        if (go != 1 || t == kInvalidId || !ctx.state.objectExists(ctx.source) ||
            !ctx.state.getObject(ctx.source).location.has_value()) return;
        if (auto* bf = std::get_if<BattlefieldLocation>(&*ctx.state.getObject(ctx.source).location))
            ctx.executor.moveToBattlefield(t, bf->id);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 987;
        d.def_id = R"RB(rad-044-167)RB";
        d.name = R"RB(Hunting Blind)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-044/167)RB";
        d.collector_number = 44;
        d.artist = R"RB()RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Device)RB"};
        d.energy_cost = 4;
        d.power_cost = 2;
        d.might = 0;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Deploy);
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
When you play this, you may move an enemy unit to this battlefield.
Friendly units here have [Shield 2]. (+2 [M] while they're defenders.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0a625c22d828c83302132b0465dceb3ab11a08bc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_987(CardRegistry& r) {
    r.registerCard(987, std::make_unique<HuntingBlind_987>());
}

}  // namespace riftbound
