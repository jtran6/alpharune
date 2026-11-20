#include "agents/battlefield_assignment.h"
#include "agents/board_score.h"
#include "cards/card_test_fixture.h"

#include <algorithm>

namespace riftbound::test::battlefield_assignment {

// Wuju Bladesman - Starter, the legend Master Yi, Wuju Bladesman prints:
// "While a friendly unit defends alone, it gets +2 [M]."
constexpr CardDefId kLegend = 317;
constexpr CardDefId kYi = 675;       // Master Yi, Tempered. Might 4.
constexpr CardDefId kPoro = 777;     // Lonely Poro. Might 2.
constexpr CardDefId kHerder = 371;   // Stellacorn Herder. Printed 3; the log fought at 6.

class AssignmentTest : public CardTestFixture {
protected:
    GameObjectId addLegend(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& legend = state.getObject(id);
        legend.owner = legend.controller = owner;
        legend.card_def_id = def_id;
        legend.card_type = card_db.get(def_id).card_type;
        legend.name = card_db.get(def_id).name;
        legend.zone = ZoneType::LegendZone;
        state.player(owner).legend_zone = id;
        return id;
    }

    bool contains(const std::vector<Intent>& legal, const Intent& intent) const {
        return std::find(legal.begin(), legal.end(), intent) != legal.end();
    }
};

// The leaf on master scores the stack higher: Yi at 4 loses to a 6, and
// Yi plus the Poro ties it. The +2 makes the single defender the tie, and
// the extra body is then the worse line.
TEST_F(AssignmentTest, PrintedLineIsDefendAlone) {
    const Card* card = card_registry.get(kLegend);
    ASSERT_NE(card, nullptr);
    EXPECT_EQ(card->def().ability_text, kWujuBladesmanDefendAlone);
    EXPECT_EQ(std::string(kWujuBladesmanDefendAlone),
              "While a friendly unit defends alone, it gets +2 [M].");

    addLegend(P1, kLegend);
    EXPECT_EQ(defendAloneBonus(state, P1, &card_registry), 2);
    EXPECT_EQ(defendAloneBonus(state, P1, nullptr), 2);

    addLegend(P2, 784);  // Wuju Master: a different legend, no defend-alone line.
    EXPECT_EQ(defendAloneBonus(state, P2, &card_registry), 0);
}

// Turn 10 shape. Yi is already at the battlefield. Stellacorn Herder is 6.
// With the +2, Yi alone is 6. The Poro's 2 replaces that bonus and adds
// nothing. The other battlefield is already held, so the Poro is not sent
// there either.
TEST_F(AssignmentTest, UselessStackLeavesOneDefender) {
    addLegend(P1, kLegend);
    auto yi = addUnit(P1, kYi, 4, 0);
    auto poro = addUnit(P1, kPoro, 2, -1);
    addUnit(P2, kHerder, 6, 0);
    addUnit(P1, kInvalidId, 1, 1);
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;
    combatInProgress(0, P2);

    const Assignment chosen = assignUnits(state, P1, &card_registry);
    const auto defenders = assignedUnits(chosen, BattlefieldId::fromRaw(0));
    ASSERT_EQ(defenders.size(), 1u);
    EXPECT_EQ(defenders.front(), yi);
    EXPECT_EQ(assignedUnits(chosen, BattlefieldId::fromRaw(1)).size(), 1u);

    const auto to_fight = Intent::standardMove(P1, {poro}, BattlefieldLocation{0});
    const auto to_base = Intent::standardMove(P1, {poro}, BaseLocation{P1});
    const auto end = Intent::endTurn(P1);
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = poro;
    play.play_location = BattlefieldLocation{0};
    Intent choice;
    choice.type = IntentType::MakeChoice;
    choice.player = P1;
    choice.chosen_value = 0;
    choice.chosen_objects = {yi};

    const auto kept = keepBattlefieldAssignment(
        state, {to_fight, to_base, play, end, choice}, card_registry);
    EXPECT_FALSE(contains(kept, to_fight));
    EXPECT_FALSE(contains(kept, play));
    EXPECT_TRUE(contains(kept, to_base));
    EXPECT_TRUE(contains(kept, end));
    EXPECT_TRUE(contains(kept, choice));

    const double solo = boardScore(state);
    const double solo_text = boardScore(state, card_registry);
    state.getObject(poro).zone = ZoneType::BattlefieldZone;
    state.getObject(poro).location = BattlefieldLocation{0};
    EXPECT_GT(solo, boardScore(state));
    EXPECT_GT(solo_text, boardScore(state, card_registry));
}

// The Poro's 2 does not reach an enemy of 7 (4+2 is 6, and 4+2 bodies is
// still 6). A 4 Might unit does: alone the defender is 6 and loses, together
// they are 8 and the enemy dies.
TEST_F(AssignmentTest, RequiredSecondUnitStillStacks) {
    addLegend(P1, kLegend);
    auto yi = addUnit(P1, kYi, 4, 0);
    auto second = addUnit(P1, kInvalidId, 4, -1);
    addUnit(P2, kHerder, 7, 0);
    addUnit(P1, kInvalidId, 1, 1);
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;
    combatInProgress(0, P2);

    const auto defenders =
        assignedUnits(assignUnits(state, P1, &card_registry), BattlefieldId::fromRaw(0));
    ASSERT_EQ(defenders.size(), 2u);
    EXPECT_NE(std::find(defenders.begin(), defenders.end(), yi), defenders.end());
    EXPECT_NE(std::find(defenders.begin(), defenders.end(), second), defenders.end());

    const auto to_fight = Intent::standardMove(P1, {second}, BattlefieldLocation{0});
    const auto to_base = Intent::standardMove(P1, {second}, BaseLocation{P1});
    const auto end = Intent::endTurn(P1);
    const auto kept =
        keepBattlefieldAssignment(state, {to_fight, to_base, end}, card_registry);
    EXPECT_TRUE(contains(kept, to_fight));
    EXPECT_FALSE(contains(kept, to_base));
    EXPECT_TRUE(contains(kept, end));

    const double solo = boardScore(state);
    state.getObject(second).zone = ZoneType::BattlefieldZone;
    state.getObject(second).location = BattlefieldLocation{0};
    EXPECT_GT(boardScore(state), solo);
}

TEST_F(AssignmentTest, HopelessExtraIsNotThrownIn) {
    addLegend(P1, kLegend);
    auto yi = addUnit(P1, kYi, 4, 0);
    auto poro = addUnit(P1, kPoro, 2, -1);
    addUnit(P2, kHerder, 7, 0);
    addUnit(P1, kInvalidId, 1, 1);
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;

    const auto defenders =
        assignedUnits(assignUnits(state, P1, &card_registry), BattlefieldId::fromRaw(0));
    ASSERT_EQ(defenders.size(), 1u);
    EXPECT_EQ(defenders.front(), yi);

    const double solo = boardScore(state);
    state.getObject(poro).zone = ZoneType::BattlefieldZone;
    state.getObject(poro).location = BattlefieldLocation{0};
    EXPECT_GT(solo, boardScore(state));
}

// No legend: the +2 does not apply, so the second unit still joins the fight.
TEST_F(AssignmentTest, StacksWhenTheBonusDoesNotApply) {
    auto yi = addUnit(P1, kYi, 4, 0);
    auto poro = addUnit(P1, kPoro, 2, -1);
    addUnit(P2, kHerder, 6, 0);
    addUnit(P1, kInvalidId, 1, 1);
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;

    const auto defenders =
        assignedUnits(assignUnits(state, P1, nullptr), BattlefieldId::fromRaw(0));
    EXPECT_NE(std::find(defenders.begin(), defenders.end(), yi), defenders.end());
    EXPECT_NE(std::find(defenders.begin(), defenders.end(), poro), defenders.end());

    const auto to_fight = Intent::standardMove(P1, {poro}, BattlefieldLocation{0});
    const auto to_base = Intent::standardMove(P1, {poro}, BaseLocation{P1});
    const auto kept = keepBattlefieldAssignment(state, {to_fight, to_base}, card_registry);
    EXPECT_TRUE(contains(kept, to_fight));
    EXPECT_TRUE(contains(kept, to_base));
}

// The fight is covered by Yi plus the bonus. The spare unit holds the other
// battlefield instead of stacking on the first.
TEST_F(AssignmentTest, SecondUnitHoldsAnotherBattlefield) {
    addLegend(P1, kLegend);
    auto yi = addUnit(P1, kYi, 4, 0);
    auto poro = addUnit(P1, kPoro, 2, -1);
    addUnit(P2, kHerder, 6, 0);
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;

    const Assignment chosen = assignUnits(state, P1, &card_registry);
    const auto first = assignedUnits(chosen, BattlefieldId::fromRaw(0));
    const auto second = assignedUnits(chosen, BattlefieldId::fromRaw(1));
    ASSERT_EQ(first.size(), 1u);
    EXPECT_EQ(first.front(), yi);
    ASSERT_EQ(second.size(), 1u);
    EXPECT_EQ(second.front(), poro);
}

// An empty battlefield we do not control is a conquer. The bonus is not a
// reason to stay off it.
TEST_F(AssignmentTest, SecondUnitConquersAnEmptyBattlefield) {
    addLegend(P1, kLegend);
    auto yi = addUnit(P1, kYi, 4, 0);
    auto poro = addUnit(P1, kPoro, 2, -1);
    addUnit(P2, kHerder, 6, 0);
    state.battlefields[0].controller = P1;

    const Assignment chosen = assignUnits(state, P1, &card_registry);
    EXPECT_EQ(assignedUnits(chosen, BattlefieldId::fromRaw(0)),
              std::vector<GameObjectId>{yi});
    EXPECT_EQ(assignedUnits(chosen, BattlefieldId::fromRaw(1)),
              std::vector<GameObjectId>{poro});
}

// The bonus is for a defender. Attacking, the second unit still joins.
TEST_F(AssignmentTest, SecondUnitJoinsWhenTheBonusDoesNotApply) {
    addLegend(P1, kLegend);
    auto yi = addUnit(P1, kYi, 4, 0);
    auto poro = addUnit(P1, kPoro, 2, -1);
    addUnit(P2, kHerder, 6, 0);
    addUnit(P1, kInvalidId, 1, 1);
    state.battlefields[0].controller = P2;
    state.battlefields[1].controller = P1;
    combatInProgress(0, P1);

    const auto attackers =
        assignedUnits(assignUnits(state, P1, &card_registry), BattlefieldId::fromRaw(0));
    EXPECT_NE(std::find(attackers.begin(), attackers.end(), yi), attackers.end());
    EXPECT_NE(std::find(attackers.begin(), attackers.end(), poro), attackers.end());
}

}  // namespace riftbound::test::battlefield_assignment
