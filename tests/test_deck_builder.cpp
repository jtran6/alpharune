// A legend in, a deck a person could play out. DeckValidator judges it on
// 20261123. The main deck is not a pile of units.

#include <gtest/gtest.h>

#include "cards/card_registry.h"
#include "core/card_db.h"
#include "rules/deck_builder.h"
#include "rules/deck_validator.h"

#include <cctype>
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

// "Channel N rune(s)" tells a player to channel. "Only channel" limits the
// Channel Phase. "Channel that many" replaces a channel. Same reading as the builder.
bool textInstructsChannel(const CardDef& card) {
    std::string text = card.ability_text;
    if (!card.effect_text.empty()) {
        if (!text.empty()) text.push_back('\n');
        text += card.effect_text;
    }
    std::string stripped;
    int depth = 0;
    for (char c : text) {
        if (c == '(') {
            ++depth;
            continue;
        }
        if (c == ')' && depth > 0) {
            --depth;
            continue;
        }
        if (depth == 0) stripped.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    for (std::size_t pos = 0; (pos = stripped.find("channel", pos)) != std::string::npos;) {
        const bool only = pos >= 5 && stripped.compare(pos - 5, 5, "only ") == 0;
        std::size_t i = pos + 7;
        if (i < stripped.size() && stripped[i] == 's') ++i;
        while (i < stripped.size() && std::isspace(static_cast<unsigned char>(stripped[i]))) ++i;
        if (!only && i < stripped.size() && std::isdigit(static_cast<unsigned char>(stripped[i]))) {
            while (i < stripped.size() && std::isdigit(static_cast<unsigned char>(stripped[i]))) ++i;
            while (i < stripped.size() && std::isspace(static_cast<unsigned char>(stripped[i]))) ++i;
            if (stripped.compare(i, 4, "rune") == 0) return true;
        }
        pos += 7;
    }
    return false;
}

void expectNoChannelFiller(const DeckBuild& built) {
    for (CardDefId id : built.deck.main_deck) {
        const CardDef& card = g_db.get(id);
        if (card.card_type != CardType::Unit && card.card_type != CardType::Gear) continue;
        EXPECT_FALSE(textInstructsChannel(card)) << card.name;
    }
}

TEST_F(DeckBuilderTest, PlaybookStatesTheMedianAndTheAdjustmentTo39) {
    const auto root = std::filesystem::temp_directory_path() / "riftbound-deck-builder-median";
    std::filesystem::remove_all(root);
    const auto over_dir = root / "over" / "internal" / "deck-catalogue" / "lists";
    const auto under_dir = root / "under" / "internal" / "deck-catalogue" / "lists";
    std::filesystem::create_directories(over_dir);
    std::filesystem::create_directories(under_dir);

    const char* tail = R"(
Battlefields:
1 Hunters' Circle
1 Firelights' Hideout
1 Startipped Peak

Runes:
6 Calm Rune
6 Body Rune
)";
    {
        std::ofstream out(over_dir / "list-a.txt");
        out << R"(Legend:
1 Master Yi, Wuju Bladesman

Champion:
1 Master Yi, Tempered

MainDeck:
3 Irelia, Fervent
3 Poppy, Paragon
3 Skystrike Squadron
3 Kato the Arm
3 Qiyana, Victorious
3 Allay, Eager Admirer
2 Steel Paws
3 En Garde
3 Defy
3 Charm
3 Discipline
3 Rampage
3 Punch First
1 Not So Fast
)" << tail;
    }
    {
        std::ofstream out(over_dir / "list-b.txt");
        out << R"(Legend:
1 Master Yi, Wuju Bladesman

Champion:
1 Master Yi, Tempered

MainDeck:
3 Irelia, Fervent
3 Poppy, Paragon
3 Skystrike Squadron
3 Kato the Arm
3 Qiyana, Victorious
3 Allay, Eager Admirer
2 Steel Paws
3 En Garde
3 Defy
3 Charm
3 Discipline
3 Rampage
3 Punch First
1 Trinity Force
)" << tail;
    }

    const DeckBuild over = buildLegendDeck(g_db, "Master Yi, Wuju Bladesman", {(root / "over").string()});
    expectLegalOnRadianceDate(over);
    EXPECT_EQ(over.median_units, 20);
    EXPECT_EQ(over.median_spells, 19);
    EXPECT_EQ(over.median_gear, 1);
    EXPECT_EQ(over.asked_units, 19);
    EXPECT_EQ(over.asked_spells, 19);
    EXPECT_EQ(over.asked_gear, 1);
    const std::string over_book = formatLegendPlaybook(g_db, g_registry, over, 0, 1);
    EXPECT_NE(over_book.find(
                  "The median of these saved lists is 20 units, 19 spells, and 1 gear (40 cards)."),
              std::string::npos)
        << over_book;
    EXPECT_NE(over_book.find(
                  "1 unit was removed so those counts sum to 39: 19 units, 19 spells, and 1 gear."),
              std::string::npos)
        << over_book;
    EXPECT_EQ(over_book.find("is the median of these saved lists"), std::string::npos) << over_book;

    const char* units20 = R"(3 Irelia, Fervent
3 Poppy, Paragon
3 Skystrike Squadron
3 Kato the Arm
3 Qiyana, Victorious
3 Allay, Eager Admirer
2 Steel Paws
)";
    const char* units18 = R"(3 Irelia, Fervent
3 Poppy, Paragon
3 Skystrike Squadron
3 Kato the Arm
3 Qiyana, Victorious
3 Allay, Eager Admirer
)";
    const char* units16 = R"(3 Irelia, Fervent
3 Poppy, Paragon
3 Skystrike Squadron
3 Kato the Arm
3 Qiyana, Victorious
1 Steel Paws
)";
    auto writeUnder = [&](const char* file, const char* units, const char* spells, const char* gear) {
        std::ofstream out(under_dir / file);
        out << "Legend:\n1 Master Yi, Wuju Bladesman\n\nChampion:\n1 Master Yi, Tempered\n\nMainDeck:\n"
            << units << spells << gear << tail;
    };
    writeUnder("list-a.txt", units20,
               "3 En Garde\n3 Defy\n3 Charm\n3 Discipline\n3 Rampage\n",
               "3 Trinity Force\n1 Hand Hammer\n");
    writeUnder("list-b.txt", units18,
               "3 En Garde\n3 Defy\n3 Charm\n3 Discipline\n3 Rampage\n1 Punch First\n",
               "3 Trinity Force\n2 Hand Hammer\n");
    writeUnder("list-c.txt", units16,
               "3 En Garde\n3 Defy\n3 Charm\n3 Discipline\n3 Rampage\n3 Punch First\n1 Sabotage\n",
               "3 Trinity Force\n1 Hand Hammer\n");

    const DeckBuild under =
        buildLegendDeck(g_db, "Master Yi, Wuju Bladesman", {(root / "under").string()});
    expectLegalOnRadianceDate(under);
    EXPECT_EQ(under.split_sources.size(), 3u);
    EXPECT_EQ(under.median_units, 18);
    EXPECT_EQ(under.median_spells, 16);
    EXPECT_EQ(under.median_gear, 4);
    EXPECT_EQ(under.asked_units, 18);
    EXPECT_EQ(under.asked_spells, 17);
    EXPECT_EQ(under.asked_gear, 4);
    const std::string under_book = formatLegendPlaybook(g_db, g_registry, under, 0, 1);
    EXPECT_NE(under_book.find(
                  "The median of these saved lists is 18 units, 16 spells, and 4 gear (38 cards)."),
              std::string::npos)
        << under_book;
    EXPECT_NE(under_book.find("1 spell was added so those counts sum to 39: 18 units, 17 spells, and "
                              "4 gear."),
              std::string::npos)
        << under_book;
    EXPECT_EQ(under_book.find("is the median of these saved lists"), std::string::npos) << under_book;
}

TEST_F(DeckBuilderTest, FillerLeavesOutChannelUnitsAndGearTheListsDidNotPlay) {
    const DeckBuild storm = buildLegendDeck(g_db, "Relentless Storm", {});
    expectLegalOnRadianceDate(storm);
    expectNoChannelFiller(storm);
    EXPECT_EQ(countNamed(storm, "Stormclaw Ursine"), 0);
    EXPECT_EQ(countNamed(storm, "Boneshiver"), 0);
    EXPECT_EQ(countNamed(storm, "Coalesced Energy"), 0);
    EXPECT_EQ(countNamed(storm, "Baccai Witherclaw"), 0);

    const DeckBuild scout = buildLegendDeck(g_db, "Swift Scout", {});
    expectLegalOnRadianceDate(scout);
    expectNoChannelFiller(scout);
    EXPECT_EQ(countNamed(scout, "Treasure Trove"), 0);

    const DeckBuild sands = buildLegendDeck(g_db, "Emperor of the Sands", {});
    expectLegalOnRadianceDate(sands);
    expectNoChannelFiller(sands);
    EXPECT_EQ(countNamed(sands, "Tasty Faefolk"), 0);
    EXPECT_EQ(countNamed(sands, "Resplendent Rostrum"), 0);

    const auto root = std::filesystem::temp_directory_path() / "riftbound-deck-builder-channel";
    std::filesystem::remove_all(root);
    const auto dir = root / "internal" / "deck-catalogue" / "lists";
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir / "list-a.txt");
        out << R"(Legend:
1 Garen, Might of Demacia

Champion:
1 Garen, Rugged

MainDeck:
3 Soaring Scout
3 Recruit (DE)
3 Recruit (NX)
3 Noxian Drummer
3 Noxian Demolitionist
3 Irresistible Faefolk
3 Flurry of Blades
3 Repulse
3 Cannon Barrage
3 Stare Down
3 For Demacia!
3 Cataclysm
3 Symbol of the Solari

Battlefields:
1 Altar to Unity
1 Back-Alley Bar
1 Bandle Tree

Runes:
6 Body Rune
6 Order Rune
)";
    }
    const DeckBuild garen = buildLegendDeck(g_db, "Garen, Might of Demacia", {root.string()});
    expectLegalOnRadianceDate(garen);
    EXPECT_EQ(countNamed(garen, "Soaring Scout"), 3);
    EXPECT_EQ(countNamed(garen, "Boneshiver"), 0);
    EXPECT_EQ(countNamed(garen, "Stormclaw Ursine"), 0);
}

bool lineNamesAMainDeckCard(const std::string& book, const char* label, const DeckBuild& built) {
    const auto pos = book.find(label);
    if (pos == std::string::npos) return false;
    const auto end = book.find('\n', pos);
    const std::string line = book.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    for (CardDefId id : built.deck.main_deck)
        if (line.find(g_db.get(id).name) != std::string::npos) return true;
    return false;
}

TEST_F(DeckBuilderTest, PlaybookNamesAPlanForSavedListsAndForThePrintedAbility) {
    const auto root = std::filesystem::temp_directory_path() / "riftbound-deck-builder-plan";
    std::filesystem::remove_all(root);
    const auto dir = root / "internal" / "deck-catalogue" / "lists";
    std::filesystem::create_directories(dir);
    const char* list = R"(Legend:
1 Master Yi, Wuju Bladesman

Champion:
1 Master Yi, Tempered

MainDeck:
3 Irelia, Fervent
3 Poppy, Paragon
3 Skystrike Squadron
3 Kato the Arm
3 Qiyana, Victorious
3 Allay, Eager Admirer
3 En Garde
3 Defy
3 Discipline
3 Charm
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
)";
    {
        std::ofstream out(dir / "list-a.txt");
        out << list;
    }
    {
        std::ofstream out(dir / "list-b.txt");
        out << list;
    }

    const DeckBuild yi = buildLegendDeck(g_db, "Master Yi, Wuju Bladesman", {root.string()});
    expectLegalOnRadianceDate(yi);
    const MainCounts yi_counts = countMain(yi);
    EXPECT_LT(yi_counts.units, 39);
    EXPECT_GT(yi_counts.spells, 0);
    EXPECT_EQ(countNamed(yi, "Zhonya's Hourglass"), 0);
    EXPECT_EQ(countNamed(yi, "Find Your Center"), 0);
    EXPECT_EQ(countNamed(yi, "Catalyst of Aeons"), 0);
    EXPECT_GE(countNamed(yi, "Discipline"), 1);
    EXPECT_GE(countNamed(yi, "En Garde"), 1);
    EXPECT_GE(countNamed(yi, "Charm"), 1);

    const std::string yi_book = formatLegendPlaybook(g_db, g_registry, yi, 0, 1);
    EXPECT_NE(yi_book.find("Plan: Defend alone."), std::string::npos) << yi_book;
    EXPECT_NE(yi_book.find("While a friendly unit defends alone, it gets +2 [M]."), std::string::npos)
        << yi_book;
    EXPECT_NE(yi_book.find("Legality date: 20261123"), std::string::npos) << yi_book;
    EXPECT_NE(yi_book.find(std::to_string(yi.units) + " units, " + std::to_string(yi.spells) +
                           " spells, " + std::to_string(yi.gear) + " gear"),
              std::string::npos)
        << yi_book;
    EXPECT_NE(yi_book.find("list-a.txt"), std::string::npos) << yi_book;
    EXPECT_NE(yi_book.find("list-b.txt"), std::string::npos) << yi_book;
    EXPECT_NE(yi_book.find("Early turns pay"), std::string::npos) << yi_book;
    EXPECT_TRUE(lineNamesAMainDeckCard(yi_book, "Carries this plan: ", yi)) << yi_book;
    EXPECT_NE(yi_book.find("Curve filler: "), std::string::npos) << yi_book;
    EXPECT_TRUE(lineNamesAMainDeckCard(yi_book, "Curve filler: ", yi)) << yi_book;
    const auto filler_at = yi_book.find("Curve filler: ");
    ASSERT_NE(filler_at, std::string::npos) << yi_book;
    const auto filler_end = yi_book.find('\n', filler_at);
    const std::string filler_line =
        yi_book.substr(filler_at, filler_end == std::string::npos ? std::string::npos : filler_end - filler_at);
    EXPECT_NE(filler_line.find("Charm"), std::string::npos) << filler_line;
    EXPECT_EQ(yi_book.find("win rate"), std::string::npos) << yi_book;

    const DeckBuild garen = buildLegendDeck(g_db, "Garen, Might of Demacia", {});
    expectLegalOnRadianceDate(garen);
    const MainCounts garen_counts = countMain(garen);
    EXPECT_LT(garen_counts.units, 39);
    EXPECT_GT(garen_counts.spells, 0);
    const std::string garen_book = formatLegendPlaybook(g_db, g_registry, garen, 0, 1);
    EXPECT_NE(garen_book.find("Plan: Conquer with units."), std::string::npos) << garen_book;
    EXPECT_NE(garen_book.find("When you conquer, if you have 4+ units at that battlefield, draw 2."),
              std::string::npos)
        << garen_book;
    EXPECT_NE(garen_book.find("Legality date: 20261123"), std::string::npos) << garen_book;
    EXPECT_NE(garen_book.find("No saved list"), std::string::npos) << garen_book;
    EXPECT_NE(garen_book.find("Early turns pay"), std::string::npos) << garen_book;
    EXPECT_TRUE(lineNamesAMainDeckCard(garen_book, "Carries this plan: ", garen)) << garen_book;
    EXPECT_EQ(garen_book.find("win rate"), std::string::npos) << garen_book;
    EXPECT_TRUE(garen_book.find("No slot is curve filler.") != std::string::npos ||
                garen_book.find("Curve filler") != std::string::npos)
        << garen_book;
}

}  // namespace riftbound::test::deck_builder
