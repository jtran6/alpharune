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

class RivenBrokenBlade_986 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenIBecomeExhausted; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<std::string> labels;
        auto& me = ctx.state.getObject(ctx.source);
        if (!me.card_counters.count("__riven_buff")) labels.push_back("Buff me");
        if (!me.card_counters.count("__riven_stun") && me.buff_count > 0) labels.push_back("Spend my buff to stun an enemy");
        if (!me.card_counters.count("__riven_ready") && me.buff_count > 0) labels.push_back("Spend my buff to ready me");
        if (labels.empty()) { if (finalizingChoices(ctx)) return; return; }
        int mode = pickMode(ctx, "Riven, Broken Blade", static_cast<int>(labels.size()), labels);
        if (mode < 0) return;
        if (finalizingChoices(ctx)) return;
        if (labels[mode] == "Buff me") { ctx.executor.buffUnit(ctx.source); me.card_counters["__riven_buff"] = 1; }
        else if (labels[mode].find("stun") != std::string::npos) {
            auto enemies = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller != ctx.controller; });
            auto t = pickTarget(ctx, "Riven: stun", enemies);
            if (t != kInvalidId) { ctx.executor.spendBuff(ctx.source, ctx.controller); ctx.executor.stunUnit(t); me.card_counters["__riven_stun"] = 1; }
        } else {
            ctx.executor.spendBuff(ctx.source, ctx.controller);
            ctx.executor.readyObject(ctx.source);
            me.card_counters["__riven_ready"] = 1;
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 986;
        d.def_id = R"RB(rad-043-167)RB";
        d.name = R"RB(Riven, Broken Blade)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-043/167)RB";
        d.collector_number = 43;
        d.artist = R"RB(JunHuan)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Riven)RB"};
        d.energy_cost = 4;
        d.power_cost = 0;
        d.might = 4;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(When I become exhausted, choose one that hasn't been chosen this turn —
[Buff] me. (Give me a +1 [M] buff if I don't have one.)Spend my buff to [Stun] an enemy unit. (It doesn't deal combat damage this turn.)Spend my buff to ready me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0c9ac0da1c144f46a1e0c5423f51f9b8f6ba295b-744x1039.png)RB";
        d.banned_from = CardDef::kRadianceBanListDate;  // Standard and 2v2, 2026-11-23
        return d;
    }();
};

}  // anonymous namespace

void register_card_986(CardRegistry& r) {
    r.registerCard(986, std::make_unique<RivenBrokenBlade_986>());
}

}  // namespace riftbound
