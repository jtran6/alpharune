/// @file test_wave_b_repeat.cpp
/// Wave B [Repeat]-grant cluster (CR 820):
///   Syndra, Transcendent (708) — spells have [Repeat] [2][P] while in a showdown
///   The Academy (772)          — hold here grants next spell [Repeat] = base cost
///   Marai Spire (525)          — friendly [Repeat] costs cost [1] less

#include "tests/cards/card_test_fixture.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using RepeatTest = CardTestFixture;

constexpr CardDefId kSyndra = 708;
constexpr CardDefId kAcademy = 772;
constexpr CardDefId kMaraiSpire = 525;
constexpr CardDefId kDesertsCall = 354;   // [Repeat] [2]. Play a Sand Soldier.

TEST_F(RepeatTest, Syndra_GrantsRepeatOnlyDuringShowdown) {
    auto syndra = addUnit(P1, kSyndra, 6, /*at_bf=*/0);
    Card* c = card_registry.get(kSyndra);
    // No showdown -> no grant.
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).spells_have_repeat_energy, 0);
    // Showdown at her BF -> grant [2][P].
    state.battlefields[0].showdown_in_progress = true;
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).spells_have_repeat_energy, 2);
    EXPECT_EQ(state.player(P1).spells_have_repeat_power, 1);
    EXPECT_EQ(state.player(P1).spells_have_repeat_domain, Domain::Chaos);
    (void)syndra;
}

TEST_F(RepeatTest, Academy_GrantsRepeatBaseToNextSpell) {
    Card* c = card_registry.get(kAcademy);
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(state.player(P1).grant_repeat_base_to_next_spell);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, kInvalidId};
    ctx.firing_trigger = TriggerType::WhenYouHoldHere;
    c->onTrigger(ctx, {});
    EXPECT_TRUE(state.player(P1).grant_repeat_base_to_next_spell);
}

TEST_F(RepeatTest, MaraiSpire_ReducesRepeatCostForController) {
    Card* c = card_registry.get(kMaraiSpire);
    ASSERT_NE(c, nullptr);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P1).repeat_cost_reduction, 1);
    // Uncontrolled battlefield (controller None) -> early return, no change.
    state.player(P1).repeat_cost_reduction = 0;
    c->applyPassiveAura(state, PlayerId::None);
    EXPECT_EQ(state.player(P1).repeat_cost_reduction, 0);
}

int sandSoldiers(const GameState& state) {
    int n = 0;
    for (const auto& [id, obj] : state.objects) {
        (void)id;
        if (obj.location.has_value() && obj.name.get() == "Sand Soldier") ++n;
    }
    return n;
}

// CR 820.3: a granted [Repeat] is another instance, paid on its own, even
// when the spell already prints one. Desert's Call prints [Repeat] [2].
TEST_F(RepeatTest, GrantedRepeatStacksWithAPrintedRepeat) {
    auto spell = addToHand(P1, kDesertsCall);
    auto& ps = state.player(P1);
    // Syndra, Transcendent: spells have [Repeat] [2][P] during a showdown.
    ps.spells_have_repeat_energy = 2;
    ps.spells_have_repeat_power = 1;
    ps.spells_have_repeat_domain = Domain::Chaos;
    // Base [2], the printed [Repeat] [2] and Syndra's [2][P]. Nothing left
    // over, so a token is not also offered [Accelerate].
    ps.rune_pool.energy = 6;
    ps.rune_pool.power[static_cast<int>(Domain::Chaos)] = 1;
    // Pay the printed tranche and the granted one. Each paid instance plays
    // another Sand Soldier: the spell, plus two repeats, is three.
    EnginePathAgent both(EnginePathAgent::script({1, 1, EnginePathAgent::kRest}));
    playThroughEngine(spell, &both, std::nullopt, nullptr, /*fund=*/false);
    EXPECT_EQ(sandSoldiers(state), 3);
    EXPECT_EQ(ps.rune_pool.power[static_cast<int>(Domain::Chaos)], 0);
    EXPECT_EQ(ps.rune_pool.energy, 0);
}

TEST_F(RepeatTest, GrantedRepeatCanBePaidInsteadOfThePrintedOne) {
    auto spell = addToHand(P1, kDesertsCall);
    auto& ps = state.player(P1);
    ps.spells_have_repeat_energy = 2;
    ps.spells_have_repeat_power = 1;
    ps.spells_have_repeat_domain = Domain::Chaos;
    ps.rune_pool.energy = 4;
    ps.rune_pool.power[static_cast<int>(Domain::Chaos)] = 1;
    // Decline the printed [Repeat] [2]. Pay Syndra's [2][P]. One extra soldier.
    EnginePathAgent only_grant(EnginePathAgent::script({0, 1, EnginePathAgent::kRest}));
    playThroughEngine(spell, &only_grant, std::nullopt, nullptr, /*fund=*/false);
    EXPECT_EQ(sandSoldiers(state), 2);
    EXPECT_EQ(ps.rune_pool.energy, 0) << "base [2] plus the granted [2], not the printed [2]";
    EXPECT_EQ(ps.rune_pool.power[static_cast<int>(Domain::Chaos)], 0);
}

TEST_F(RepeatTest, OneShotRepeatGrantAppliesToASpellThatAlreadyHasRepeat) {
    auto spell = addToHand(P1, kDesertsCall);
    auto& ps = state.player(P1);
    // The Academy / Temporal Portal: the next spell gains [Repeat] equal to
    // its base cost. Desert's Call's base cost is [2], and it already prints
    // [Repeat] [2]. Both are payable. The one-shot is spent because it applied.
    ps.grant_repeat_base_to_next_spell = true;
    ps.rune_pool.energy = 6;
    EnginePathAgent both(EnginePathAgent::script({1, 1, EnginePathAgent::kRest}));
    playThroughEngine(spell, &both, std::nullopt, nullptr, /*fund=*/false);
    EXPECT_EQ(sandSoldiers(state), 3);
    EXPECT_FALSE(ps.grant_repeat_base_to_next_spell);
    EXPECT_EQ(ps.rune_pool.energy, 0) << "base [2] plus two [Repeat] [2]";
}

}  // namespace
}  // namespace riftbound::test
