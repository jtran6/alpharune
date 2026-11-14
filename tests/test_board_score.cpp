#include "agents/board_score.h"
#include "cards/card_test_fixture.h"

namespace riftbound::test::board_score {

class BoardScoreTest : public CardTestFixture {};

// A point already scored outranks a board that is stronger in every other
// way the score reads: both battlefields held, and a fight the friendly
// would win. The empty line spent the card to shrink a friendly instead of
// scoring, so that position stays behind.
TEST_F(BoardScoreTest, ScoringAPointBeatsShrinkingAFriendly) {
    addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/-1);
    state.player(P1).score = 1;
    const GameState scored = state;

    state.player(P1).score = 0;
    auto shrunk = addUnit(P1, kInvalidId, /*might=*/1, /*at_bf=*/0);
    state.getObject(shrunk).current_might = 1;
    addUnit(P1, kInvalidId, /*might=*/10, /*at_bf=*/0);
    addUnit(P2, kInvalidId, /*might=*/1, /*at_bf=*/0);
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, /*might=*/10, /*at_bf=*/1);
    addUnit(P2, kInvalidId, /*might=*/1, /*at_bf=*/1);
    state.battlefields[1].controller = P1;

    EXPECT_GT(boardScore(scored), boardScore(state));
}

// Seven points, with a fight the unit would win on a battlefield it does
// not yet hold, outranks a score of zero even when that position holds
// every battlefield and would win the fights there. The same seven points
// with the fight lost rank lower: the conquer is in the score.
TEST_F(BoardScoreTest, SevenPointsWithAConquerOutranksZero) {
    addUnit(P1, kInvalidId, /*might=*/5, /*at_bf=*/0);
    addUnit(P2, kInvalidId, /*might=*/2, /*at_bf=*/0);
    state.battlefields[0].controller = P2;
    state.player(P1).score = 7;
    const GameState conquer = state;

    GameState losing = conquer;
    for (auto& [id, obj] : losing.objects) {
        (void)id;
        if (obj.controller == P1 && obj.isUnit()) obj.current_might = 2;
        if (obj.controller == P2 && obj.isUnit()) obj.current_might = 5;
    }

    state.player(P1).score = 0;
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, /*might=*/8, /*at_bf=*/0);
    addUnit(P2, kInvalidId, /*might=*/1, /*at_bf=*/0);
    addUnit(P1, kInvalidId, /*might=*/8, /*at_bf=*/1);
    addUnit(P2, kInvalidId, /*might=*/1, /*at_bf=*/1);

    EXPECT_GT(boardScore(conquer), boardScore(state));
    EXPECT_GT(boardScore(conquer), boardScore(losing));
}

}  // namespace riftbound::test::board_score
