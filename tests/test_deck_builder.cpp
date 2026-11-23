// A legend in, a deck a person could play out. DeckValidator judges it on
// 20261123. The main deck is not a pile of units.

#include <gtest/gtest.h>

#include "cards/card_registry.h"
#include "core/card_db.h"
#include "rules/deck_builder.h"
#include "rules/deck_validator.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace riftbound::test::deck_builder {

CardRegistry g_registry;
CardDB g_db;

class DeckBuilderTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        g_registry.loadAll();
        g_db.buildFromClasses(g_registry);
    }
};

std::string joined(const std::vector<std::string>& errors) {
    std::ostringstream out;
    for (const std::string& error : errors) out << error << "\n";
    return out.str();
}

int countNamed(const DeckBuild& built, const std::string& name) {
    int n = 0;
    if (built.deck.chosen_champion != kInvalidId && g_db.get(built.deck.chosen_champion).name == name)
        ++n;
    for (CardDefId id : built.deck.main_deck)
        if (g_db.get(id).name == name) ++n;
    for (CardDefId id : built.deck.sideboard)
        if (g_db.get(id).name == name) ++n;
    for (CardDefId id : built.deck.rune_deck)
        if (g_db.get(id).name == name) ++n;
    for (CardDefId id : built.deck.battlefields)
        if (g_db.get(id).name == name) ++n;
    return n;
}

struct MainCounts {
    int units = 0;
    int spells = 0;
    int gear = 0;
    int other = 0;
};

MainCounts countMain(const DeckBuild& built) {
    MainCounts counts;
    for (CardDefId id : built.deck.main_deck) {
        switch (g_db.get(id).card_type) {
            case CardType::Unit: ++counts.units; break;
            case CardType::Spell: ++counts.spells; break;
            case CardType::Gear: ++counts.gear; break;
            default: ++counts.other; break;
        }
    }
    return counts;
}

void expectLegalOnRadianceDate(const DeckBuild& built) {
    const int saved = legalityDate();
    setLegalityDate(20261123);
    const ValidationResult result = DeckValidator(g_db).validate(built.deck);
    setLegalityDate(saved == 20261123 ? 0 : saved);
    EXPECT_TRUE(result.is_legal) << joined(result.errors);
    EXPECT_EQ(built.legality_date, 20261123);
    EXPECT_EQ(built.deck.main_deck.size(), 39u);
    EXPECT_EQ(built.deck.rune_deck.size(), 12u);
    EXPECT_EQ(built.deck.battlefields.size(), 3u);
    const CardDef& legend = g_db.get(built.deck.legend);
    const CardDef& champion = g_db.get(built.deck.chosen_champion);
    EXPECT_EQ(champion.card_type, CardType::Unit);
    EXPECT_EQ(champion.super_type, SuperType::Champion);
    EXPECT_TRUE(champion.hasTag(legend.legendChampionTag()));
    for (CardDefId id : built.deck.main_deck) {
        const CardDef& card = g_db.get(id);
        EXPECT_FALSE(card.bannedOn(20261123)) << card.name;
        if (card.card_type == CardType::Spell) {
            const std::string text = card.ability_text + card.effect_text;
            EXPECT_EQ(text.find("channel"), std::string::npos) << card.name;
            EXPECT_EQ(text.find("Channel"), std::string::npos) << card.name;
        }
    }
    const MainCounts counts = countMain(built);
    EXPECT_EQ(counts.other, 0);
    EXPECT_LT(counts.units, 39);
    EXPECT_GT(counts.spells, 0);
    EXPECT_EQ(counts.units, built.units);
    EXPECT_EQ(counts.spells, built.spells);
    EXPECT_EQ(counts.gear, built.gear);
}

TEST_F(DeckBuilderTest, BuiltDeckIsLegalOnRadianceDateAndIsNotAllUnits) {
    const DeckBuild built = buildLegendDeck(g_db, "Garen, Might of Demacia", {});
    expectLegalOnRadianceDate(built);
    EXPECT_EQ(g_db.get(built.deck.legend).name, "Might of Demacia - Starter");
    const MainCounts counts = countMain(built);
    EXPECT_EQ(counts.units, 18);
    EXPECT_EQ(counts.spells, 18);
    EXPECT_EQ(counts.gear, 3);
    EXPECT_TRUE(built.deck.sideboard.empty());
    EXPECT_TRUE(built.split_sources.empty());
    EXPECT_EQ(countNamed(built, "Zhonya's Hourglass"), 0);
    EXPECT_EQ(countNamed(built, "Find Your Center"), 0);
    EXPECT_EQ(countNamed(built, "Catalyst of Aeons"), 0);

    const auto path = std::filesystem::temp_directory_path() / "riftbound-deck-builder-test.txt";
    {
        std::ofstream out(path);
        out << built.list;
    }
    const DeckSubmission loaded = DeckValidator::loadFromDeckList(path.string(), g_db);
    setLegalityDate(20261123);
    const ValidationResult again = DeckValidator(g_db).validate(loaded);
    setLegalityDate(0);
    EXPECT_TRUE(again.is_legal) << joined(again.errors);
    EXPECT_EQ(loaded.legend, built.deck.legend);
    EXPECT_EQ(loaded.chosen_champion, built.deck.chosen_champion);
    EXPECT_EQ(loaded.main_deck, built.deck.main_deck);

    const DeckBuild other = buildLegendDeck(g_db, "Swift Scout", {});
    expectLegalOnRadianceDate(other);
    EXPECT_NE(other.deck.legend, built.deck.legend);
    const MainCounts other_counts = countMain(other);
    EXPECT_EQ(other_counts.units, 18);
    EXPECT_EQ(other_counts.spells, 18);
    EXPECT_EQ(other_counts.gear, 3);

    const std::string book = formatLegendPlaybook(g_db, g_registry, built, 0, 1);
    EXPECT_NE(book.find("Garen, Might of Demacia"), std::string::npos) << book;
    EXPECT_NE(book.find("Might of Demacia - Starter"), std::string::npos) << book;
    EXPECT_NE(book.find("Legality date: 20261123"), std::string::npos) << book;
    EXPECT_NE(book.find("18 units, 18 spells, 3 gear"), std::string::npos) << book;
    EXPECT_NE(book.find("No saved list"), std::string::npos) << book;
    EXPECT_NE(book.find("internal/deck-catalogue"), std::string::npos) << book;
    EXPECT_NE(book.find("internal/tournament-data"), std::string::npos) << book;
    EXPECT_NE(book.find("\nPlan\n"), std::string::npos) << book;
    EXPECT_NE(book.find("\nEarly turns\n"), std::string::npos) << book;
    EXPECT_NE(book.find("\nWhat to hold\n"), std::string::npos) << book;
    EXPECT_NE(book.find("\nWhat to spend\n"), std::string::npos) << book;
    EXPECT_NE(book.find("\nLegend line\n"), std::string::npos) << book;
    EXPECT_EQ(book.find("win rate"), std::string::npos) << book;
}

TEST_F(DeckBuilderTest, SavedListsSetTheSplitAndLeaveOutBannedAndChannelFiller) {
    const auto root = std::filesystem::temp_directory_path() / "riftbound-deck-builder-lists";
    std::filesystem::remove_all(root);
    const auto dir = root / "internal" / "deck-catalogue" / "lists";
    std::filesystem::create_directories(dir);

    const char* shared = R"(Legend:
1 Master Yi, Wuju Bladesman

Champion:
1 Master Yi, Tempered

MainDeck:
3 Irelia, Fervent
3 Poppy, Paragon
3 Skystrike Squadron
3 Kato the Arm
3 Qiyana, Victorious
)";
    {
        std::ofstream out(dir / "list-a.txt");
        out << shared << R"(3 Allay, Eager Admirer
3 En Garde
3 Defy
3 Charm
3 Discipline
3 Find Your Center
3 Catalyst of Aeons
3 Zhonya's Hourglass

Battlefields:
1 Hunters' Circle
1 Firelights' Hideout
1 Startipped Peak

Runes:
6 Calm Rune
6 Body Rune

Sideboard:
1 Rampage
)";
    }
    {
        std::ofstream out(dir / "list-b.txt");
        out << shared << R"(2 Allay, Eager Admirer
1 Steel Paws
3 En Garde
3 Defy
3 Charm
3 Discipline
3 Find Your Center
3 Catalyst of Aeons
3 Zhonya's Hourglass

Battlefields:
1 Hunters' Circle
1 Firelights' Hideout
1 Startipped Peak

Runes:
6 Calm Rune
6 Body Rune

Sideboard:
1 Not So Fast
)";
    }

    const DeckBuild built =
        buildLegendDeck(g_db, "Master Yi, Wuju Bladesman", {root.string()});
    expectLegalOnRadianceDate(built);
    const MainCounts counts = countMain(built);
    EXPECT_EQ(counts.units, 18);
    EXPECT_EQ(counts.spells, 18);
    EXPECT_EQ(counts.gear, 3);
    EXPECT_TRUE(built.deck.sideboard.empty());
    EXPECT_EQ(countNamed(built, "Zhonya's Hourglass"), 0);
    EXPECT_EQ(countNamed(built, "Find Your Center"), 0);
    EXPECT_EQ(countNamed(built, "Catalyst of Aeons"), 0);
    EXPECT_GE(countNamed(built, "Steel Paws"), 1);
    EXPECT_GE(countNamed(built, "Discipline"), 1);
    EXPECT_GE(countNamed(built, "En Garde"), 1);

    const std::string book = formatLegendPlaybook(g_db, g_registry, built, 0, 1);
    EXPECT_NE(book.find("Master Yi, Wuju Bladesman"), std::string::npos) << book;
    EXPECT_NE(book.find("Legality date: 20261123"), std::string::npos) << book;
    EXPECT_NE(book.find("18 units, 18 spells, 3 gear"), std::string::npos) << book;
    EXPECT_NE(book.find("list-a.txt"), std::string::npos) << book;
    EXPECT_NE(book.find("list-b.txt"), std::string::npos) << book;
    EXPECT_NE(book.find("internal/deck-catalogue"), std::string::npos) << book;
    EXPECT_EQ(book.find("win rate"), std::string::npos) << book;
}

TEST_F(DeckBuilderTest, UnknownLegendIsRejected) {
    EXPECT_THROW(buildLegendDeck(g_db, "Not A Real Legend", {}), std::runtime_error);
}

}  // namespace riftbound::test::deck_builder
