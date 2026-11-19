/// @file test_wave_b_misc2.cpp
///   Mageseeker Investigator (725) — sets BattlefieldState::surcharge_enemy_multi_move
///   The Dreaming Tree (287)       — first friendly-unit choice here each turn -> draw 1

#include "tests/cards/card_test_fixture.h"
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

using Misc2Test = CardTestFixture;
constexpr CardDefId kInvestigator = 725;
constexpr CardDefId kDreamingTree = 287;

TEST_F(Misc2Test, MageseekerInvestigator_FlagsItsBattlefield) {
    addUnit(P1, kInvestigator, 4, /*at_bf=*/0);
    // The tax is owed by the Investigator's opponents (P2), per extra unit.
    EXPECT_EQ(state.battlefields[0].multi_move_tax[1], 0);
    card_registry.get(kInvestigator)->applyPassiveAura(state, P1);
    EXPECT_EQ(state.battlefields[0].multi_move_tax[1], 1);
    EXPECT_EQ(state.battlefields[0].multi_move_tax[0], 0);
    EXPECT_EQ(state.battlefields[1].multi_move_tax[1], 0);
}

TEST_F(Misc2Test, DreamingTree_DrawsOncePerTurn) {
    auto tree = addUnit(P1, kDreamingTree, 0, /*at_bf=*/-1);
    state.getObject(tree).card_type = CardType::Battlefield;
    state.battlefields[0].card_object_id = tree;
    auto unit = addUnit(P1, kInvalidId, 2, /*at_bf=*/0);
    addToDeck(P1, 1);
    addToDeck(P1, 1);
    int hand0 = static_cast<int>(state.player(P1).hand.size());

    ChainItem spell;
    spell.is_spell = true;
    spell.controller = P1;
    spell.targets = {unit};
    spell.chosen_objects = {unit};
    state.chain.resuming = spell;

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, tree};
    ctx.firing_trigger = TriggerType::WhenAFriendlyUnitChosenHere;
    Card* c = card_registry.get(kDreamingTree);
    Card::TriggerEvent ev;
    ev.source = tree;
    ev.controller = P1;
    ev.trigger = TriggerType::WhenAFriendlyUnitChosenHere;
    ev.subject = unit;
    EXPECT_TRUE(c->triggerCondition(state, tree, P1, ev.trigger, unit));
    // Accepted, then countered: the first choice this turn is spent (CR 383.1).
    c->acceptTrigger(state, ev);
    EXPECT_FALSE(c->triggerCondition(state, tree, P1, ev.trigger, unit))
        << "the next spell this turn is not a second first";
    c->onTrigger(ctx, {});
    EXPECT_EQ(static_cast<int>(state.player(P1).hand.size()), hand0 + 1);
}

}  // namespace
}  // namespace riftbound::test
