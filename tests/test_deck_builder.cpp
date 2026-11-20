// A legend in, a legal deck and a playbook out. The deck is judged by
// DeckValidator. The playbook names the legend that was asked for.

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

TEST_F(DeckBuilderTest, GivenALegendTheDeckIsLegalAndThePlaybookNamesIt) {
    const DeckBuild built = buildLegendDeck(g_db, "Garen, Might of Demacia");
    const ValidationResult result = DeckValidator(g_db).validate(built.deck);
    EXPECT_TRUE(result.is_legal) << joined(result.errors);
    EXPECT_EQ(g_db.get(built.deck.legend).name, "Might of Demacia - Starter");
    EXPECT_EQ(built.deck.main_deck.size(), 39u);
    EXPECT_EQ(built.deck.rune_deck.size(), 12u);
    EXPECT_EQ(built.deck.battlefields.size(), 3u);
    EXPECT_TRUE(built.deck.sideboard.empty());

    const auto path = std::filesystem::temp_directory_path() / "riftbound-deck-builder-test.txt";
    {
        std::ofstream out(path);
        out << built.list;
    }
    const DeckSubmission loaded = DeckValidator::loadFromDeckList(path.string(), g_db);
    const ValidationResult again = DeckValidator(g_db).validate(loaded);
    EXPECT_TRUE(again.is_legal) << joined(again.errors);
    EXPECT_EQ(loaded.legend, built.deck.legend);
    EXPECT_EQ(loaded.chosen_champion, built.deck.chosen_champion);
    EXPECT_EQ(loaded.main_deck, built.deck.main_deck);

    const DeckBuild other = buildLegendDeck(g_db, "Swift Scout");
    const ValidationResult other_result = DeckValidator(g_db).validate(other.deck);
    EXPECT_TRUE(other_result.is_legal) << joined(other_result.errors);
    EXPECT_NE(other.deck.legend, built.deck.legend);

    const std::string book = formatLegendPlaybook(g_db, g_registry, built, 1, 1);
    EXPECT_NE(book.find("Garen, Might of Demacia"), std::string::npos) << book;
    EXPECT_NE(book.find("Might of Demacia - Starter"), std::string::npos) << book;
    EXPECT_NE(book.find("\nPlan\n"), std::string::npos) << book;
    EXPECT_NE(book.find("\nEarly turns\n"), std::string::npos) << book;
    EXPECT_NE(book.find("\nWhat to hold\n"), std::string::npos) << book;
    EXPECT_NE(book.find("\nWhat to spend\n"), std::string::npos) << book;
    EXPECT_NE(book.find("\nLegend line\n"), std::string::npos) << book;
}

TEST_F(DeckBuilderTest, UnknownLegendIsRejected) {
    EXPECT_THROW(buildLegendDeck(g_db, "Not A Real Legend"), std::runtime_error);
}

}  // namespace riftbound::test::deck_builder
