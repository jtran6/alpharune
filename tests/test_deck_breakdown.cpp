#include <gtest/gtest.h>

#include "core/card_db.h"
#include "cards/card_registry.h"
#include "rules/deck_breakdown.h"
#include "rules/deck_validator.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace riftbound::test::deck_breakdown {

CardRegistry g_registry;
CardDB g_db;

uint64_t choose(int n, int k) {
    if (k < 0 || n < 0 || k > n) return 0;
    if (k == 0 || k == n) return 1;
    if (k > n - k) k = n - k;
    uint64_t r = 1;
    for (int i = 1; i <= k; ++i)
        r = r * static_cast<uint64_t>(n - k + i) / static_cast<uint64_t>(i);
    return r;
}

double plainDraw(int pile, int copies, int seen) {
    if (copies <= 0 || pile <= 0 || seen <= 0) return 0.0;
    if (seen > pile) seen = pile;
    if (seen > pile - copies) return 1.0;
    return 1.0 - static_cast<double>(choose(pile - copies, seen)) /
                     static_cast<double>(choose(pile, seen));
}

std::string listWith(const std::vector<std::pair<const char*, int>>& main,
                     const char* champion = "Teemo, Scout") {
    std::ostringstream out;
    out << "Legend:\n1 Deceiver\n\nChampion:\n1 " << champion
        << "\n\nMainDeck:\n";
    for (const auto& [name, copies] : main) out << copies << ' ' << name << '\n';
    // A comment is not a card. Anivia would be the whole late-game curve if it were read.
    out << "# 3 Anivia, Primal\n"
        << "\nBattlefields:\n1 Sunken Temple\n"
        << "\nRunes:\n2 Body Rune\n"
        << "\nSideboard:\n1 Concentrate\n";
    return out.str();
}

std::vector<std::pair<const char*, int>> baseMain() {
    return {
        {"Soaring Scout", 2},
        {"Hextech Ray", 1},
        {"First Mate", 3},
        {"Steadfast Outrider", 4},
    };
}

std::vector<std::pair<const char*, int>> swapOneSteadfast(const char* name) {
    return {
        {"Soaring Scout", 2},
        {"Hextech Ray", 1},
        {"First Mate", 3},
        {"Steadfast Outrider", 3},
        {name, 1},
    };
}

DeckReport loadReport(const std::string& text) {
    const auto path = std::filesystem::temp_directory_path() / "riftbound-deck-breakdown-test.txt";
    {
        std::ofstream out(path);
        out << text;
    }
    const DeckSubmission deck = DeckValidator::loadFromDeckList(path.string(), g_db);
    return analyzeDeck(deck, g_db);
}

const DeckRole* roleNamed(const DeckReport& report, const char* title) {
    for (const DeckRole& role : report.roles)
        if (role.title == title) return &role;
    return nullptr;
}

int copiesIn(const DeckReport& report, const char* title, const char* name) {
    const DeckRole* role = roleNamed(report, title);
    if (!role) return 0;
    for (const DeckCardLine& line : role->cards)
        if (line.name == name) return line.copies;
    return 0;
}

bool inAnyRole(const DeckReport& report, const char* name) {
    for (const DeckRole& role : report.roles)
        for (const DeckCardLine& line : role.cards)
            if (line.name == name) return true;
    return false;
}

double chanceOf(const DeckReport& report, const char* stage, const char* card) {
    for (const DeckStage& s : report.stages) {
        if (s.name != stage) continue;
        for (const auto& [name, p] : s.cards)
            if (name == card) return p;
    }
    return -1.0;
}

class DeckBreakdownTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        g_registry.loadAll();
        g_db.buildFromClasses(g_registry);
    }
};

TEST_F(DeckBreakdownTest, TwoDropThatAlsoRampsIsInBothRoles) {
    const DeckReport report = loadReport(listWith(baseMain()));
    EXPECT_EQ(copiesIn(report, "2-drops", "Soaring Scout"), 2);
    EXPECT_EQ(copiesIn(report, "Ramp", "Soaring Scout"), 2);
    EXPECT_EQ(copiesIn(report, "Combat tricks", "Soaring Scout"), 0);
    EXPECT_EQ(copiesIn(report, "Card draw", "Soaring Scout"), 0);
    // The champion costs 2 as well, and it does not ramp.
    EXPECT_EQ(copiesIn(report, "2-drops", "Teemo, Scout"), 1);
    EXPECT_EQ(copiesIn(report, "Ramp", "Teemo, Scout"), 0);
}

TEST_F(DeckBreakdownTest, CheapCombatTrickIsNotATwoDrop) {
    const DeckReport report = loadReport(listWith(baseMain()));
    // Hextech Ray is energy cost 1, [Action], "Deal 3 to a unit at a battlefield."
    EXPECT_EQ(copiesIn(report, "Combat tricks", "Hextech Ray"), 1);
    EXPECT_EQ(copiesIn(report, "2-drops", "Hextech Ray"), 0);
    EXPECT_EQ(copiesIn(report, "Ramp", "Hextech Ray"), 0);
    // A ready on play, and a Might buff that is not timed to a fight, stay out.
    EXPECT_EQ(copiesIn(report, "Combat tricks", "First Mate"), 0);
    EXPECT_EQ(copiesIn(report, "Ready", "First Mate"), 3);
    EXPECT_EQ(copiesIn(report, "Combat tricks", "Steadfast Outrider"), 0);
    EXPECT_EQ(copiesIn(report, "Might", "Steadfast Outrider"), 4);
    const DeckRole* tricks = roleNamed(report, "Combat tricks");
    ASSERT_NE(tricks, nullptr);
    ASSERT_EQ(tricks->cards.size(), 1u);
    EXPECT_EQ(tricks->cards[0].name, "Hextech Ray");
}

TEST_F(DeckBreakdownTest, LateGameIsTheTopOfThisList) {
    const DeckReport low = loadReport(listWith(baseMain()));
    EXPECT_EQ(low.late_game_cost, 4);
    EXPECT_EQ(copiesIn(low, "Late game", "Steadfast Outrider"), 4);
    EXPECT_EQ(copiesIn(low, "Late game", "Soaring Scout"), 0);
    EXPECT_EQ(copiesIn(low, "Late game", "Hextech Ray"), 0);
    const DeckRole* late = roleNamed(low, "Late game");
    ASSERT_NE(late, nullptr);
    EXPECT_NE(late->reason.find("energy cost 4"), std::string::npos);

    // A different list tops at the champion's 7, not at 4 and not at a fixed number.
    const DeckReport high = loadReport(listWith({{"Soaring Scout", 1}}, "Anivia, Primal"));
    EXPECT_EQ(high.late_game_cost, 7);
    EXPECT_EQ(copiesIn(high, "Late game", "Anivia, Primal"), 1);
    EXPECT_EQ(copiesIn(high, "Late game", "Soaring Scout"), 0);
    EXPECT_EQ(copiesIn(high, "2-drops", "Soaring Scout"), 1);
    EXPECT_EQ(copiesIn(high, "Combat tricks", "Anivia, Primal"), 1);
}

TEST_F(DeckBreakdownTest, DrawChanceStaysPlainWithoutADrawSpell) {
    const DeckReport report = loadReport(listWith(baseMain()));
    // Sunken Temple draws, and Concentrate is in the sideboard. Neither is the list.
    EXPECT_TRUE(report.plain_draw);
    EXPECT_EQ(report.draw_pile, 10);
    EXPECT_FALSE(inAnyRole(report, "Sunken Temple"));
    EXPECT_FALSE(inAnyRole(report, "Body Rune"));
    EXPECT_FALSE(inAnyRole(report, "Concentrate"));
    EXPECT_FALSE(inAnyRole(report, "Anivia, Primal"));
    ASSERT_EQ(report.stages.size(), 4u);
    EXPECT_EQ(report.stages[0].name, "Opening hand");
    EXPECT_EQ(report.stages[0].natural, 4);
    EXPECT_EQ(report.stages[1].natural, 5);
    EXPECT_EQ(report.stages[2].natural, 6);
    EXPECT_EQ(report.stages[3].natural, 7);

    const double opening = chanceOf(report, "Opening hand", "First Mate");
    const double turn1 = chanceOf(report, "Turn 1", "First Mate");
    EXPECT_NEAR(opening, plainDraw(10, 3, 4), 1e-12);
    EXPECT_NEAR(turn1, plainDraw(10, 3, 5), 1e-12);
    // 2-drops in the pile are the two Soaring Scouts. The champion is not drawn.
    const DeckRole* twos = roleNamed(report, "2-drops");
    ASSERT_NE(twos, nullptr);
    EXPECT_EQ(twos->copies, 3);
    EXPECT_EQ(twos->draw_copies, 2);
    ASSERT_FALSE(report.stages[0].roles.empty());
    EXPECT_EQ(report.stages[0].roles[0].first, "2-drops");
    EXPECT_NEAR(report.stages[0].roles[0].second, plainDraw(10, 2, 4), 1e-12);

    const DeckReport swapped = loadReport(listWith(swapOneSteadfast("Ember Monk")));
    EXPECT_TRUE(swapped.plain_draw);
    EXPECT_EQ(swapped.draw_pile, 10);
    EXPECT_DOUBLE_EQ(chanceOf(swapped, "Opening hand", "First Mate"), opening);
    EXPECT_DOUBLE_EQ(chanceOf(swapped, "Turn 1", "First Mate"), turn1);
    EXPECT_EQ(swapped.late_game_cost, 4);
}

TEST_F(DeckBreakdownTest, DrawChanceMovesWhenTheListContainsADrawSpell) {
    const DeckReport plain = loadReport(listWith(baseMain()));
    const DeckReport drawing = loadReport(listWith(swapOneSteadfast("Concentrate")));
    EXPECT_FALSE(drawing.plain_draw);
    EXPECT_EQ(drawing.draw_pile, 10);
    EXPECT_EQ(copiesIn(drawing, "Card draw", "Concentrate"), 1);
    EXPECT_EQ(copiesIn(drawing, "Ramp", "Concentrate"), 1);
    EXPECT_EQ(copiesIn(drawing, "2-drops", "Concentrate"), 0);
    // Concentrate costs 5, so the top of this list moved with the cards in it.
    EXPECT_EQ(drawing.late_game_cost, 5);
    EXPECT_EQ(copiesIn(drawing, "Late game", "Concentrate"), 1);
    EXPECT_EQ(copiesIn(drawing, "Late game", "Steadfast Outrider"), 0);
    // Same pile size, same 3 First Mates. The draw spell is what moves the chance.
    // 1 - C(7,4)/C(10,4) * (the hands that miss, then miss the 2 extra cards) = 191/210.
    const double moved = chanceOf(drawing, "Opening hand", "First Mate");
    EXPECT_NEAR(moved, 191.0 / 210.0, 1e-9);
    EXPECT_GT(moved, chanceOf(plain, "Opening hand", "First Mate"));
    EXPECT_GT(chanceOf(drawing, "Turn 1", "First Mate"), chanceOf(plain, "Turn 1", "First Mate"));
    EXPECT_NE(drawing.counted.find("Concentrate"), std::string::npos);
    EXPECT_NE(drawing.counted.find("draws 2"), std::string::npos);
}

TEST_F(DeckBreakdownTest, PredictRecycleAndTutorMoveTheChance) {
    const double plain = chanceOf(loadReport(listWith(baseMain())), "Opening hand", "First Mate");
    const DeckReport predict = loadReport(listWith(swapOneSteadfast("Eclipse")));
    const DeckReport tutor = loadReport(listWith(swapOneSteadfast("Command: Attack")));
    EXPECT_FALSE(predict.plain_draw);
    EXPECT_FALSE(tutor.plain_draw);
    EXPECT_GT(chanceOf(predict, "Opening hand", "First Mate"), plain);
    const DeckReport dig = loadReport(listWith(swapOneSteadfast("Dazzling Aurora")));
    EXPECT_GT(chanceOf(tutor, "Opening hand", "First Mate"), plain);
    EXPECT_FALSE(dig.plain_draw);
    EXPECT_GT(chanceOf(dig, "Opening hand", "First Mate"), plain);
    EXPECT_NE(dig.counted.find("looks through the top"), std::string::npos);
    EXPECT_NE(predict.counted.find("Predict"), std::string::npos);
    EXPECT_NE(tutor.counted.find("looks at 5"), std::string::npos);
    // Eclipse is a fight-timed Might spell, and it is not a 2-drop (it costs 3).
    EXPECT_EQ(copiesIn(predict, "Combat tricks", "Eclipse"), 1);
    EXPECT_EQ(copiesIn(predict, "2-drops", "Eclipse"), 0);
}

TEST_F(DeckBreakdownTest, ReportNamesCopiesAndDoesNotPlayAGame) {
    const DeckReport report = loadReport(listWith(baseMain()));
    const std::string text = formatDeckReport(report);
    EXPECT_NE(text.find("2 Soaring Scout"), std::string::npos);
    EXPECT_NE(text.find("1 Hextech Ray"), std::string::npos);
    EXPECT_NE(text.find("3 First Mate"), std::string::npos);
    EXPECT_NE(text.find("4 Steadfast Outrider"), std::string::npos);
    EXPECT_NE(text.find("1 Teemo, Scout (champion)"), std::string::npos);
    EXPECT_NE(text.find("1 Sunken Temple"), std::string::npos);
    EXPECT_NE(text.find("2 Body Rune"), std::string::npos);
    EXPECT_NE(text.find("1 Concentrate"), std::string::npos);
    EXPECT_NE(text.find("Sideboard (recorded, not part of the draw chance)"), std::string::npos);
    EXPECT_NE(text.find("Opening hand"), std::string::npos);
    EXPECT_NE(text.find("Turn 1"), std::string::npos);
    EXPECT_NE(text.find("Turn 2"), std::string::npos);
    EXPECT_NE(text.find("Turn 3"), std::string::npos);
    EXPECT_NE(text.find("plain draw"), std::string::npos);
    EXPECT_NE(text.find("does not play a game"), std::string::npos);
    EXPECT_EQ(text.find("Anivia, Primal"), std::string::npos);
    EXPECT_EQ(text.find(" pass"), std::string::npos);
    EXPECT_EQ(text.find(" fail"), std::string::npos);
    EXPECT_EQ(text.find("Pass"), std::string::npos);
    EXPECT_EQ(text.find("Fail"), std::string::npos);
}

TEST_F(DeckBreakdownTest, AddSymbolIsRampOnATwoDrop) {
    const DeckReport report = loadReport(listWith({
        {"Dragonsoul Sage", 2},
        {"Honeyfruit", 2},
        {"Sneaky Zeebles", 2},
        {"Steadfast Outrider", 4},
    }));
    for (const char* name : {"Dragonsoul Sage", "Honeyfruit", "Sneaky Zeebles"}) {
        EXPECT_EQ(copiesIn(report, "2-drops", name), 2) << name;
        EXPECT_EQ(copiesIn(report, "Ramp", name), 2) << name;
    }
    const DeckRole* ramp = roleNamed(report, "Ramp");
    ASSERT_NE(ramp, nullptr);
    EXPECT_EQ(ramp->copies, 6);
}

TEST_F(DeckBreakdownTest, DrawsVerbMovesTheChance) {
    const double plain = chanceOf(loadReport(listWith(baseMain())), "Opening hand", "First Mate");
    for (const char* name : {"Invert Timelines", "Minah Swiftfoot", "Hidden Blade", "Detonate"}) {
        const DeckReport drawing = loadReport(listWith(swapOneSteadfast(name)));
        EXPECT_FALSE(drawing.plain_draw) << name;
        EXPECT_EQ(copiesIn(drawing, "Card draw", name), 1) << name;
        EXPECT_GT(chanceOf(drawing, "Opening hand", "First Mate"), plain) << name;
        EXPECT_NE(drawing.counted.find("draws"), std::string::npos) << name;
    }
    // "Draw 2" is the same verb without the s.
    const DeckReport concentrate = loadReport(listWith(swapOneSteadfast("Concentrate")));
    EXPECT_NEAR(chanceOf(concentrate, "Opening hand", "First Mate"), 191.0 / 210.0, 1e-9);
    EXPECT_NE(concentrate.counted.find("draws 2"), std::string::npos);
}

TEST_F(DeckBreakdownTest, QuickDrawIsNotADraw) {
    const DeckReport plain = loadReport(listWith(baseMain()));
    const double opening = chanceOf(plain, "Opening hand", "First Mate");
    const double turn1 = chanceOf(plain, "Turn 1", "First Mate");
    for (const char* name : {"Long Sword", "Spinning Axe", "Cloth Armor"}) {
        const DeckReport gear = loadReport(listWith(swapOneSteadfast(name)));
        EXPECT_TRUE(gear.plain_draw) << name;
        EXPECT_EQ(copiesIn(gear, "Card draw", name), 0) << name;
        EXPECT_DOUBLE_EQ(chanceOf(gear, "Opening hand", "First Mate"), opening) << name;
        EXPECT_DOUBLE_EQ(chanceOf(gear, "Turn 1", "First Mate"), turn1) << name;
    }

    const DeckReport unmatched = loadReport(listWith(baseMain(), "Jax, Unmatched"));
    EXPECT_TRUE(unmatched.plain_draw);
    EXPECT_EQ(copiesIn(unmatched, "Card draw", "Jax, Unmatched"), 0);
    EXPECT_DOUBLE_EQ(chanceOf(unmatched, "Opening hand", "First Mate"), opening);
    EXPECT_DOUBLE_EQ(chanceOf(unmatched, "Turn 1", "First Mate"), turn1);

    // "pay [1] to draw 1" is a real dig. The champion is not in the opening hand.
    const DeckReport jax = loadReport(listWith(baseMain(), "Jax, Unrelenting"));
    EXPECT_FALSE(jax.plain_draw);
    EXPECT_EQ(copiesIn(jax, "Card draw", "Jax, Unrelenting"), 1);
    EXPECT_DOUBLE_EQ(chanceOf(jax, "Opening hand", "First Mate"), opening);
    EXPECT_GT(chanceOf(jax, "Turn 1", "First Mate"), turn1);
    EXPECT_NEAR(chanceOf(jax, "Turn 1", "First Mate"), 29.0 / 30.0, 1e-9);
    EXPECT_NE(jax.counted.find("not in the opening hand"), std::string::npos);
    EXPECT_NE(jax.counted.find("draws 1"), std::string::npos);
}

TEST_F(DeckBreakdownTest, TrashRecycleIsNotCardsSeenFromTheDeck) {
    const DeckReport plain = loadReport(listWith(baseMain()));
    const DeckReport forge = loadReport(listWith(swapOneSteadfast("Forge of the Future")));
    const double opening = chanceOf(plain, "Opening hand", "First Mate");
    EXPECT_EQ(copiesIn(forge, "Recycle", "Forge of the Future"), 1);
    EXPECT_EQ(copiesIn(forge, "2-drops", "Forge of the Future"), 1);
    EXPECT_TRUE(forge.plain_draw);
    EXPECT_DOUBLE_EQ(chanceOf(forge, "Opening hand", "First Mate"), opening);
    // A 4-card look at the remaining deck would be 195/210. The trash is not that look.
    EXPECT_LT(chanceOf(forge, "Opening hand", "First Mate"), 195.0 / 210.0 - 1e-6);
}

TEST_F(DeckBreakdownTest, ShieldOutsideTheReminderIsACombatTrick) {
    const DeckReport report = loadReport(listWith(swapOneSteadfast("Cloth Armor")));
    // [Shield 2] stays after "(+2 [M] while I'm a defender.)" is stripped.
    EXPECT_EQ(copiesIn(report, "Combat tricks", "Cloth Armor"), 1);
    EXPECT_EQ(copiesIn(report, "Might", "Cloth Armor"), 1);
    EXPECT_EQ(copiesIn(report, "2-drops", "Cloth Armor"), 0);
    EXPECT_EQ(copiesIn(report, "Card draw", "Cloth Armor"), 0);
    EXPECT_TRUE(report.plain_draw);
}

}  // namespace riftbound::test::deck_breakdown
