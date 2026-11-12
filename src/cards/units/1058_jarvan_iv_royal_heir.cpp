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

class JarvanIVRoyalHeir_1058 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        BattlefieldId here = kInvalidId;
        if (ctx.state.objectExists(ctx.source) && ctx.state.getObject(ctx.source).location.has_value())
            if (const auto* bf = std::get_if<BattlefieldLocation>(&*ctx.state.getObject(ctx.source).location))
                here = bf->id;
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            if (u.controller != ctx.controller) return false;
            if (!here.valid() || !u.location.has_value()) return true;
            if (const auto* at = std::get_if<BattlefieldLocation>(&*u.location))
                return at->id != here;
            return true;
        });
        int go = confirmOptional(ctx, "Jarvan: move a unit you control to my battlefield", [ok=!units.empty()]{ return ok; });
        if (go < 0) return;
        GameObjectId t = kInvalidId;
        if (go == 1) {
            t = pickTarget(ctx, "Jarvan: move here", units);
            if (t == kInvalidId) return;
        }
        if (finalizingChoices(ctx)) return;
        if (go != 1 || t == kInvalidId || !ctx.state.objectExists(ctx.source) ||
            !ctx.state.getObject(ctx.source).location.has_value()) return;
        auto* bf = std::get_if<BattlefieldLocation>(&*ctx.state.getObject(ctx.source).location);
        if (!bf) return;
        if (ctx.state.getObject(t).location.has_value())
            if (const auto* at = std::get_if<BattlefieldLocation>(&*ctx.state.getObject(t).location))
                if (at->id == bf->id) return;
        ctx.executor.moveToBattlefield(t, bf->id);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1058;
        d.def_id = R"RB(rad-133-167)RB";
        d.name = R"RB(Jarvan IV, Royal Heir)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-133/167)RB";
        d.collector_number = 133;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Demacia)RB", R"RB(Jarvan IV)RB"};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 7;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Tank);
        d.assault_value = 1;
        d.ability_text = R"RB([Assault] (+1 [M] while I'm attacking.)
[Tank] (I must be assigned combat damage first.)
When I attack, you may move a unit you control to my battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/967de7aa18d5833896825bfd7d78b234e088ae73-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1058(CardRegistry& r) {
    r.registerCard(1058, std::make_unique<JarvanIVRoyalHeir_1058>());
}

}  // namespace riftbound
