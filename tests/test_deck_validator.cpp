#include <gtest/gtest.h>
#include "core/card_db.h"
#include "cards/card_registry.h"
#include "rules/deck_validator.h"
#include "rules/random_deck.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace riftbound;

namespace {

std::string deckPath(const char* name) {
    for (auto& prefix : {"../decks/", "../../decks/", "decks/"}) {
        auto p = std::string(prefix) + name;
        if (std::filesystem::exists(p)) return p;
    }
    const char* root = std::getenv("RIFTBOUND_ROOT");
    if (root) {
        auto path = std::filesystem::path(root) / "decks" / name;
        if (std::filesystem::exists(path)) return path.string();
    }
    return std::string("decks/") + name;
}

} // namespace

class DeckValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry.loadAll();
        db.buildFromClasses(registry);
    }

    CardRegistry registry;

    /// Build a valid LeBlanc deck for testing modifications against.
    DeckSubmission makeValidDeck() {
        auto* legend = db.findByName("Deceiver");
        auto* champion = db.findByName("LeBlanc, Fragmented");
        EXPECT_NE(legend, nullptr);
        EXPECT_NE(champion, nullptr);
        if (!legend || !champion) return {};

        DeckSubmission deck;
        deck.legend = legend->id;
        deck.chosen_champion = champion->id;

        // 39 main deck cards (Mind/Order domain identity)
        auto addCards = [&](const char* name, int count) {
            auto* card = db.findByName(name);
            if (card) {
                for (int i = 0; i < count; ++i)
                    deck.main_deck.push_back(card->id);
            }
        };

        addCards("Watchful Sentry", 3);
        addCards("Cull the Weak", 3);
        addCards("Soaring Scout", 3);
        addCards("Honest Broker", 3);
        addCards("Deathgrip", 3);
        addCards("Glasc Mixologist", 3);
        addCards("Ruined Rex", 3);
        addCards("Sacrifice", 3);
        addCards("Mirror Image", 3);
        addCards("Hidden Blade", 2);
        addCards("Shadow's Call", 2);
        addCards("Thousand-Tailed Watcher", 1);
        addCards("Facebreaker", 1);
        addCards("Black Rose Dignitary", 1);
        addCards("Tactical Retreat", 1);
        addCards("LeBlanc, Fragmented", 1);
        addCards("Karthus, Eternal", 3);

        // 12 runes
        auto* order_rune = db.findByName("Order Rune");
        auto* mind_rune = db.findByName("Mind Rune");
        if (order_rune && mind_rune) {
            for (int i = 0; i < 8; ++i) deck.rune_deck.push_back(order_rune->id);
            for (int i = 0; i < 4; ++i) deck.rune_deck.push_back(mind_rune->id);
        }

        // 3 battlefields
        auto addBF = [&](const char* name) {
            auto* bf = db.findByName(name);
            if (bf) deck.battlefields.push_back(bf->id);
        };
        addBF("Windswept Hillock");
        addBF("Dusk Rose Lab");
        addBF("Forbidding Waste");

        // Sideboard
        auto addSB = [&](const char* name, int count) {
            auto* card = db.findByName(name);
            if (card) {
                for (int i = 0; i < count; ++i)
                    deck.sideboard.push_back(card->id);
            }
        };
        addSB("Unchecked Power", 2);
        addSB("Salvage", 2);
        addSB("Facebreaker", 1);
        addSB("LeBlanc, Everywhere at Once", 1);
        addSB("LeBlanc, Fragmented", 1);
        addSB("Tactical Retreat", 1);

        return deck;
    }

    CardDB db;
};

TEST_F(DeckValidatorTest, ValidDeckPasses) {
    auto deck = makeValidDeck();
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_TRUE(result.is_legal) << "Errors:";
    for (auto& e : result.errors) {
        ADD_FAILURE() << "  " << e;
    }
}

TEST_F(DeckValidatorTest, LoadFromDeckListAndValidate) {
    auto path = deckPath("yi_rebuild.txt");
    if (!std::filesystem::exists(path)) {
        GTEST_SKIP() << "yi_rebuild.txt not found";
    }

    auto deck = DeckValidator::loadFromDeckList(path, db);
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_TRUE(result.is_legal);
    for (auto& e : result.errors) {
        ADD_FAILURE() << "  " << e;
    }
}

TEST_F(DeckValidatorTest, MissingLegend) {
    auto deck = makeValidDeck();
    deck.legend = kInvalidId;
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
    EXPECT_EQ(result.errors.size(), 1u);
}

TEST_F(DeckValidatorTest, MissingChampion) {
    auto deck = makeValidDeck();
    deck.chosen_champion = kInvalidId;
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
}

TEST_F(DeckValidatorTest, WrongDeckSize) {
    auto deck = makeValidDeck();
    deck.main_deck.pop_back(); // 38 + champion = 39, not 40
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
    bool found_size_error = false;
    for (auto& e : result.errors) {
        if (e.find("40 cards") != std::string::npos) found_size_error = true;
    }
    EXPECT_TRUE(found_size_error);
}

TEST_F(DeckValidatorTest, WrongRuneCount) {
    auto deck = makeValidDeck();
    deck.rune_deck.pop_back(); // 11 runes
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
}

TEST_F(DeckValidatorTest, TooManyBattlefields) {
    auto deck = makeValidDeck();
    deck.battlefields.push_back(deck.battlefields[0]); // 4 BFs
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
}

TEST_F(DeckValidatorTest, DuplicateBattlefieldNames) {
    auto deck = makeValidDeck();
    deck.battlefields[1] = deck.battlefields[0]; // same name twice
    DeckValidator validator(db);
    auto result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
    bool found_dupe = false;
    for (auto& e : result.errors) {
        if (e.find("Duplicate") != std::string::npos) found_dupe = true;
    }
    EXPECT_TRUE(found_dupe);
}

namespace {
bool hasSideboardSizeError(const ValidationResult& r) {
    for (const auto& e : r.errors)
        if (e.find("Sideboard can have at most") != std::string::npos) return true;
    return false;
}
}  // namespace

// Tournament Rules 601.1.c.1: a sideboard can include 10 or fewer cards.
// (Other errors, e.g. the 3-copy cap from padding with one card, are ignored:
// this pins only the size boundary.)
TEST_F(DeckValidatorTest, SideboardSizeLimitIsTen) {
    auto* card = db.findByName("Salvage");
    ASSERT_NE(card, nullptr);
    DeckValidator validator(db);

    auto deck = makeValidDeck();
    deck.sideboard.assign(10, card->id);
    EXPECT_FALSE(hasSideboardSizeError(validator.validate(deck))) << "10 is legal";

    deck.sideboard.assign(11, card->id);
    auto result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
    EXPECT_TRUE(hasSideboardSizeError(result)) << "11 exceeds the limit";
}

TEST_F(DeckValidatorTest, DomainViolation) {
    auto deck = makeValidDeck();
    // Add a Fury card to a Mind/Order deck
    auto* fury_card = db.findByName("Blazing Scorcher");
    if (fury_card) {
        deck.main_deck.back() = fury_card->id; // replace last card
        DeckValidator validator(db);
        auto result = validator.validate(deck);
        EXPECT_FALSE(result.is_legal);
        bool found_domain = false;
        for (auto& e : result.errors) {
            if (e.find("domain") != std::string::npos) found_domain = true;
        }
        EXPECT_TRUE(found_domain);
    }
}

// Printed Runeblade Anvil is Fury. The gallery tag says Chaos, and that tag
// made Mordekaiser's Fury/Order legend reject the gear.
TEST_F(DeckValidatorTest, RunebladeAnvilIsFuryAndLegalInAFuryLegend) {
    auto* anvil = db.findByName("Runeblade Anvil");
    ASSERT_NE(anvil, nullptr);
    ASSERT_EQ(anvil->domains.size(), 1u);
    EXPECT_EQ(anvil->domains[0], Domain::Fury);

    auto* legend = db.findByName("Iron Revenant");
    ASSERT_NE(legend, nullptr);
    bool legend_is_fury = false;
    for (auto d : legend->domains)
        if (d == Domain::Fury) legend_is_fury = true;
    ASSERT_TRUE(legend_is_fury);

    DeckValidator validator(db);

    auto fury_deck = makeValidDeck();
    fury_deck.legend = legend->id;
    fury_deck.main_deck.back() = anvil->id;
    for (const auto& e : validator.validate(fury_deck).errors) {
        EXPECT_EQ(e.find("Runeblade Anvil"), std::string::npos) << e;
    }

    // Deceiver is Mind/Order, so a real Fury gear is illegal there. An empty
    // domain would have been allowed and this error would not appear.
    auto mind_deck = makeValidDeck();
    mind_deck.main_deck.back() = anvil->id;
    bool fury_error = false;
    for (const auto& e : validator.validate(mind_deck).errors) {
        if (e.find("Runeblade Anvil") != std::string::npos &&
            e.find("Fury") != std::string::npos)
            fury_error = true;
    }
    EXPECT_TRUE(fury_error);
}

// Printed Bushwhack Trap is Chaos and printed Gravity Field is Mind. The
// gallery tags say Fury and Calm. Neither card spends a domain rune: the trap
// costs Energy only, and the field's move tax is [1][A].
TEST_F(DeckValidatorTest, BushwhackTrapIsChaosAndGravityFieldIsMind) {
    auto* trap = db.findByName("Bushwhack Trap");
    auto* field = db.findByName("Gravity Field");
    ASSERT_NE(trap, nullptr);
    ASSERT_NE(field, nullptr);
    ASSERT_EQ(trap->domains.size(), 1u);
    ASSERT_EQ(field->domains.size(), 1u);
    EXPECT_EQ(trap->domains[0], Domain::Chaos);
    EXPECT_EQ(field->domains[0], Domain::Mind);

    auto namesCard = [](const ValidationResult& result, const char* card) {
        for (const auto& e : result.errors)
            if (e.find(card) != std::string::npos) return true;
        return false;
    };
    auto namesDomain = [](const ValidationResult& result, const char* card, const char* domain) {
        for (const auto& e : result.errors)
            if (e.find(card) != std::string::npos && e.find(domain) != std::string::npos)
                return true;
        return false;
    };

    DeckValidator validator(db);

    // Battle Mistress is Body/Chaos.
    auto* chaos = db.findByName("Battle Mistress");
    ASSERT_NE(chaos, nullptr);
    auto chaos_deck = makeValidDeck();
    chaos_deck.legend = chaos->id;
    chaos_deck.main_deck.back() = trap->id;
    EXPECT_FALSE(namesCard(validator.validate(chaos_deck), "Bushwhack Trap"));

    // Iron Revenant is Fury/Order, so a printed Chaos gear is illegal there.
    auto* fury = db.findByName("Iron Revenant");
    ASSERT_NE(fury, nullptr);
    auto fury_deck = makeValidDeck();
    fury_deck.legend = fury->id;
    fury_deck.main_deck.back() = trap->id;
    EXPECT_TRUE(namesDomain(validator.validate(fury_deck), "Bushwhack Trap", "Chaos"));

    // Deceiver, the fixture legend, is Mind/Order.
    auto mind_deck = makeValidDeck();
    mind_deck.main_deck.back() = field->id;
    EXPECT_FALSE(namesCard(validator.validate(mind_deck), "Gravity Field"));

    // Green Father is Calm/Order, so a printed Mind gear is illegal there.
    auto* calm = db.findByName("Green Father");
    ASSERT_NE(calm, nullptr);
    auto calm_deck = makeValidDeck();
    calm_deck.legend = calm->id;
    calm_deck.main_deck.back() = field->id;
    EXPECT_TRUE(namesDomain(validator.validate(calm_deck), "Gravity Field", "Mind"));
}

TEST_F(DeckValidatorTest, WrongRuneDomain) {
    auto deck = makeValidDeck();
    // Replace an Order Rune with a Fury Rune
    auto* fury_rune = db.findByName("Fury Rune");
    if (fury_rune) {
        deck.rune_deck[0] = fury_rune->id;
        DeckValidator validator(db);
        auto result = validator.validate(deck);
        EXPECT_FALSE(result.is_legal);
    }
}

TEST_F(DeckValidatorTest, ChampionTagMismatch) {
    auto deck = makeValidDeck();
    // Replace champion with one from a different legend
    auto* wrong_champ = db.findByName("Jinx, Demolitionist");
    if (wrong_champ) {
        deck.chosen_champion = wrong_champ->id;
        DeckValidator validator(db);
        auto result = validator.validate(deck);
        EXPECT_FALSE(result.is_legal);
        bool found_tag = false;
        for (auto& e : result.errors) {
            if (e.find("champion tag") != std::string::npos ||
                e.find("domain") != std::string::npos) {
                found_tag = true;
            }
        }
        EXPECT_TRUE(found_tag);
    }
}

TEST_F(DeckValidatorTest, CopyLimitExceeded) {
    auto deck = makeValidDeck();
    // Add a 4th copy of a card that already has 3 + 1 in sideboard
    auto* card = db.findByName("Facebreaker");
    if (card) {
        // Already have 1 main + 1 side = 2. Add 2 more to main.
        deck.main_deck.push_back(card->id);
        deck.main_deck.push_back(card->id);
        // Now need to remove 2 cards to keep deck at 40
        // (but we're testing copy limits, not size, so just check for that error)
        DeckValidator validator(db);
        auto result = validator.validate(deck);
        EXPECT_FALSE(result.is_legal);
        bool found_copies = false;
        for (auto& e : result.errors) {
            if (e.find("copies") != std::string::npos) found_copies = true;
        }
        EXPECT_TRUE(found_copies);
    }
}

// V1 / D12: [Unique] (CR 825.3.a): "A deck can contain only one card of a
// given name if the card has Unique" — across Main Deck and sideboard
// (Tournament Rules 403.3).
TEST_F(DeckValidatorTest, UniqueCardOnlyOnce) {
    auto hasUniqueError = [](const ValidationResult& r) {
        for (auto& e : r.errors)
            if (e.find("[Unique]") != std::string::npos) return true;
        return false;
    };
    for (const char* name : {"Forgefire Cape", "Rabadon's Deathcrown", "Shurelya's Requiem"}) {
        auto* card = db.findByName(name);
        ASSERT_NE(card, nullptr) << name;
        ASSERT_TRUE(card->keywords.has(Keyword::Unique)) << name;
        DeckValidator validator(db);

        auto one = makeValidDeck();
        one.main_deck.push_back(card->id);
        EXPECT_FALSE(hasUniqueError(validator.validate(one))) << "one copy of " << name;

        auto two = makeValidDeck();
        two.main_deck.push_back(card->id);
        two.main_deck.push_back(card->id);
        auto result = validator.validate(two);
        EXPECT_FALSE(result.is_legal);
        EXPECT_TRUE(hasUniqueError(result)) << "two copies of " << name;

        // One in the Main Deck and one in the sideboard is two in the deck.
        auto split = makeValidDeck();
        split.main_deck.push_back(card->id);
        split.sideboard.push_back(card->id);
        EXPECT_TRUE(hasUniqueError(validator.validate(split))) << "main + sideboard: " << name;
    }
}

// Ban-list enforcement (CSV source). A card present in the deck that appears on
// the loaded ban list must make the deck illegal.
TEST_F(DeckValidatorTest, BannedCardRejectedViaCsv) {
    auto deck = makeValidDeck();
    DeckValidator base(db);
    ASSERT_TRUE(base.validate(deck).is_legal) << "deck is legal before banning";

    // "Sacrifice" has 3 copies in the valid main deck — ban it via a temp CSV.
    auto* sac = db.findByName("Sacrifice");
    ASSERT_NE(sac, nullptr);
    auto csv = std::filesystem::temp_directory_path() / "rb_test_banlist.csv";
    { std::ofstream f(csv); f << "SFD,001,'Sacrifice'\n"; }

    DeckValidator validator(db);
    validator.loadBanList(csv.string());
    EXPECT_TRUE(validator.isBanned(sac->id));

    auto result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
    bool found_ban = false;
    for (auto& e : result.errors)
        if (e.find("Banned") != std::string::npos &&
            e.find("Sacrifice") != std::string::npos) found_ban = true;
    EXPECT_TRUE(found_ban) << "expected a 'Banned card ... Sacrifice' error";

    // Wild format (--wild): the same banned card is now allowed.
    DeckValidator wild(db);
    wild.loadBanList(csv.string());
    wild.setWild(true);
    EXPECT_FALSE(wild.isBanned(sac->id)) << "wild ignores the ban list";
    EXPECT_TRUE(wild.validate(deck).is_legal)
        << "--wild should let the banned-card deck pass";

    std::filesystem::remove(csv);
}

// Radiance ban list, effective 2026-11-23 (playriftbound.com announcement).
// A test pins the clock with setLegalityDate(YYYYMMDD) and restores the UTC
// calendar date with setLegalityDate(0). The builder reads the clock when it
// is constructed, the validator when it is asked.
TEST_F(DeckValidatorTest, RadianceBansAreLegalOnNovember22AndIllegalOnNovember23) {
    struct Pin {
        explicit Pin(int yyyymmdd) { setLegalityDate(yyyymmdd); }
        ~Pin() { setLegalityDate(0); }
    };
    auto namesBan = [](const ValidationResult& result, const char* name) {
        for (const auto& e : result.errors)
            if (e.find("Banned") != std::string::npos && e.find(name) != std::string::npos)
                return true;
        return false;
    };

    const char* names[] = {
        "Riven, Broken Blade",
        "Zhonya's Hourglass",
        "Stellacorn Herder",
        "Nocturne, Horrifying",
        "Rhasa the Sunderer",
        "Brynhir Thundersong",
        "Dazzling Aurora",
        "Baited Hook",
        "Emperor's Dais",
        "Star Spring",
    };
    for (const char* name : names) {
        const CardDef* card = db.findByName(name);
        ASSERT_NE(card, nullptr) << name;
        EXPECT_FALSE(card->banned) << name;
        EXPECT_EQ(card->banned_from, CardDef::kRadianceBanListDate) << name;
        EXPECT_EQ(db.findAllByName(name).size(), 1u) << name;
        {
            Pin day(20261122);
            EXPECT_FALSE(DeckValidator(db).isBanned(card->id))
                << name << " is legal on 2026-11-22";
        }
        {
            Pin day(20261123);
            EXPECT_TRUE(DeckValidator(db).isBanned(card->id))
                << name << " is banned on 2026-11-23";
        }
        {
            Pin day(20261124);
            EXPECT_TRUE(DeckValidator(db).isBanned(card->id))
                << name << " stays banned after 2026-11-23";
        }
    }

    // A ban already in effect does not wait for this date.
    {
        Pin day(20261122);
        const CardDef* ekko = db.findByName("Ekko, Recurrent");
        ASSERT_NE(ekko, nullptr);
        EXPECT_TRUE(ekko->banned);
        EXPECT_TRUE(DeckValidator(db).isBanned(ekko->id));
    }

    // Order gear in the Mind/Order fixture deck.
    const CardDef* hook = db.findByName("Baited Hook");
    ASSERT_NE(hook, nullptr);
    {
        auto deck = makeValidDeck();
        deck.main_deck.back() = hook->id;
        Pin day(20261122);
        const ValidationResult early = DeckValidator(db).validate(deck);
        EXPECT_TRUE(early.is_legal) << (early.errors.empty() ? "" : early.errors.front());
        EXPECT_FALSE(namesBan(early, "Baited Hook"));
        setLegalityDate(20261123);
        const ValidationResult late = DeckValidator(db).validate(deck);
        EXPECT_FALSE(late.is_legal);
        EXPECT_TRUE(namesBan(late, "Baited Hook"));
    }
    {
        auto deck = makeValidDeck();
        deck.sideboard.push_back(hook->id);
        Pin day(20261122);
        EXPECT_TRUE(DeckValidator(db).validate(deck).is_legal);
        setLegalityDate(20261123);
        const ValidationResult late = DeckValidator(db).validate(deck);
        EXPECT_FALSE(late.is_legal);
        EXPECT_TRUE(namesBan(late, "Baited Hook"));
    }

    for (const char* bf_name : {"Emperor's Dais", "Star Spring"}) {
        auto deck = makeValidDeck();
        const CardDef* bf = db.findByName(bf_name);
        ASSERT_NE(bf, nullptr);
        deck.battlefields.back() = bf->id;
        {
            Pin day(20261122);
            const ValidationResult early = DeckValidator(db).validate(deck);
            EXPECT_TRUE(early.is_legal) << bf_name
                                        << (early.errors.empty() ? "" : early.errors.front());
        }
        {
            Pin day(20261123);
            const ValidationResult late = DeckValidator(db).validate(deck);
            EXPECT_FALSE(late.is_legal) << bf_name;
            EXPECT_TRUE(namesBan(late, bf_name)) << bf_name;
        }
    }

    // Calm champion in a Mind/Order deck is a domain error either day, and a
    // ban error only once the list is in effect.
    {
        auto deck = makeValidDeck();
        deck.main_deck.back() = db.findByName("Riven, Broken Blade")->id;
        Pin day(20261122);
        EXPECT_FALSE(namesBan(DeckValidator(db).validate(deck), "Riven, Broken Blade"));
        setLegalityDate(20261123);
        EXPECT_TRUE(namesBan(DeckValidator(db).validate(deck), "Riven, Broken Blade"));
    }

    // --wild still allows the card on the effective date.
    {
        auto deck = makeValidDeck();
        deck.main_deck.back() = hook->id;
        Pin day(20261123);
        DeckValidator wild(db);
        wild.setWild(true);
        EXPECT_FALSE(wild.isBanned(hook->id));
        EXPECT_TRUE(wild.validate(deck).is_legal);
    }

    // The deck generator reads the same clock, at construction.
    {
        Pin day(20261122);
        const RandomDeckBuilder early(db);
        EXPECT_NE(std::find(early.reachable().begin(), early.reachable().end(), hook->id),
                  early.reachable().end());
        setLegalityDate(20261123);
        const RandomDeckBuilder late(db);
        bool banned = false;
        for (const auto& [id, why] : late.unreachable())
            if (id == hook->id && why == "banned") banned = true;
        EXPECT_TRUE(banned);
        const CardDef* dais = db.findByName("Emperor's Dais");
        ASSERT_NE(dais, nullptr);
        bool dais_banned = false;
        for (const auto& [id, why] : late.unreachable())
            if (id == dais->id && why == "banned") dais_banned = true;
        EXPECT_TRUE(dais_banned);
    }
}
