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

class PrideOfNazumah_1067 : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.energy = 1; c.exhaust = true;
        return c;
    }
    // Shield this object has right now: its own value, plus each aura or
    // Equipment instance. A bare [Shield] is 1, the same as the aura fold.
    // 0 stays 0 — there is no instance to round up.
    static int shieldNow(const GameObject& u) {
        int n = std::max(0, u.shield_value);
        for (const auto& ae : u.aura_effects)
            if (ae.keyword == Keyword::Shield) n += std::max(1, ae.keyword_value);
        return n;
    }
    // "I have [Assault] equal to my [Shield]" this turn, including 0. A
    // granted Assault of 0 is +0 [M]: the aura fold's Assault is at least 1,
    // so this is written on the unit. A second call with the same Shield
    // adds nothing.
    static void syncAssault(GameObject& u) {
        const int want = shieldNow(u);
        const int have = u.card_counters.count("__pride_have")
                             ? u.card_counters["__pride_have"] : 0;
        const int delta = want - have;
        if (delta != 0) {
            u.assault_value += delta;
            u.temp_assault_value += delta;
            if (u.assault_value < 0) u.assault_value = 0;
            if (u.temp_assault_value < 0) u.temp_assault_value = 0;
            u.card_counters["__pride_have"] = want;
        }
        if (want > 0) {
            u.keywords.set(Keyword::Assault);
            u.temp_keywords.set(Keyword::Assault);
        } else if (u.assault_value <= 0) {
            u.assault_value = 0;
            u.temp_assault_value = 0;
            u.keywords.clear(Keyword::Assault);
            u.temp_keywords.clear(Keyword::Assault);
        }
        u.recomputeMight();
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        auto t = pickTarget(ctx, "Pride of Nazumah: Assault equal to Shield", units);
        if (t == kInvalidId || !ctx.state.objectExists(t)) return;
        auto& u = ctx.state.getObject(t);
        u.card_counters["__pride_turn"] = ctx.state.turn.turn_number + 1;   // +1 so 0 = never
        // The unit this ability was given to. A new life (it left the board)
        // is not this one (CR 124.1).
        u.card_counters["__pride_life"] = static_cast<int>(u.zone.incarnation());
        syncAssault(u);
    }
    // After every Might-dependent hook of this pass, including a text copy
    // applied once that list is done, so [Shield] one of those grants is
    // already on the unit whichever hook ran first. Ordinary auras and
    // Equipment were applied before that list. A later change this turn moves
    // the Assault. The ability is the unit's: a control change does not
    // freeze the old number.
    bool appliesAfterMightDependentAuras() const override { return true; }
    void applyAfterMightDependentAuras(GameState& state, PlayerId) const override {
        std::vector<GameObjectId> units;
        for (auto& [id, u] : state.objects.onBoard()) {
            if (!u.isUnit() || !u.location.has_value()) continue;
            const auto it = u.card_counters.find("__pride_turn");
            if (it == u.card_counters.end() || it->second != state.turn.turn_number + 1) continue;
            units.push_back(id);
        }
        const bool expired = state.turn.phase == TurnPhase::ExpirationStep;
        for (auto id : units) {
            if (!state.objectExists(id)) continue;
            auto& u = state.getObject(id);
            const auto life = u.card_counters.find("__pride_life");
            const bool same = life != u.card_counters.end() &&
                              life->second == static_cast<int>(u.zone.incarnation());
            // The Expiration Step has already taken the temporary value back.
            // A unit that left and came back is a new object and does not
            // keep the grant.
            if (expired || !same) {
                u.card_counters.erase("__pride_turn");
                u.card_counters.erase("__pride_have");
                u.card_counters.erase("__pride_life");
                continue;
            }
            syncAssault(u);
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1067;
        d.def_id = R"RB(rad-147-167)RB";
        d.name = R"RB(Pride of Nazumah)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-147/167)RB";
        d.collector_number = 147;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.super_type = SuperType::None;
        d.domains = {Domain::Calm, Domain::Body};
        d.tags = {R"RB(K'Sante)RB"};
        d.energy_cost = 0;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([1], [E]: Give a friendly unit "I have [Assault] equal to my [Shield]" this turn. (+1 [M] for each instance of Assault while it's an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/190a27bf844fa21105824efc19de4e71c3137df6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1067(CardRegistry& r) {
    r.registerCard(1067, std::make_unique<PrideOfNazumah_1067>());
}

}  // namespace riftbound
