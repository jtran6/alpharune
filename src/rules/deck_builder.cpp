/// @file deck_builder.cpp
/// Builds one constructed deck for a legend and writes how to play it.
///
/// The list is filled from `RandomDeckBuilder::legendCards` and accepted only
/// when `DeckValidator::validate` says it is legal. Mirror games go through
/// `GameEngine::runGame` with a `PlayByPlay` account, the same one the
/// `riftbound` binary prints after a game. Quoted lines are those accounts.
/// Printed text is quoted from `CardDef`. A section that has no usable game
/// line says it is unread.

#include "rules/deck_builder.h"

#include "agents/random_player_agent.h"
#include "cards/card_registry.h"
#include "engine/game_engine.h"
#include "io/play_by_play.h"
#include "rules/deck_breakdown.h"
#include "rules/deck_model.h"
#include "rules/random_deck.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace riftbound {
namespace {

constexpr int kMainDeckCards = 39;
constexpr int kRunes = 12;
constexpr char kAgent[] = "random-player:pass=0";

std::string trimCopy(std::string s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    const size_t b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string{} : s.substr(a, b - a + 1);
}

std::string lowerCopy(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string stripParentheticals(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    int depth = 0;
    for (char c : s) {
        if (c == '(') {
            ++depth;
            continue;
        }
        if (c == ')' && depth > 0) {
            --depth;
            continue;
        }
        if (depth == 0) out.push_back(c);
    }
    return out;
}

std::string printedRaw(const CardDef& card) {
    std::string text = card.ability_text;
    if (!card.effect_text.empty()) {
        if (!text.empty()) text.push_back('\n');
        text += card.effect_text;
    }
    return text;
}

std::set<std::string> wordsOf(const std::string& text) {
    std::set<std::string> words;
    std::string cur;
    auto flush = [&] {
        if (cur.size() >= 5) words.insert(cur);
        cur.clear();
    };
    for (char c : lowerCopy(stripParentheticals(text))) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (std::isalnum(u)) cur.push_back(static_cast<char>(std::tolower(u)));
        else flush();
    }
    flush();
    return words;
}

int wordOverlap(const std::set<std::string>& legend_words, const CardDef& card) {
    if (legend_words.empty()) return 0;
    int n = 0;
    for (const std::string& word : wordsOf(printedRaw(card)))
        if (legend_words.count(word)) ++n;
    return n;
}

bool domainsInside(const CardDef& card, const CardDef& legend) {
    for (Domain d : card.domains)
        if (!legend.hasDomain(d)) return false;
    return true;
}

std::string bareTitle(const CardDef& legend) {
    const std::string suffix = " - Starter";
    if (legend.name.size() > suffix.size() &&
        legend.name.compare(legend.name.size() - suffix.size(), suffix.size(), suffix) == 0)
        return legend.name.substr(0, legend.name.size() - suffix.size());
    return legend.name;
}

std::string aliasOf(const CardDef& legend) {
    const std::string& tag = legend.legendChampionTag();
    if (tag.empty()) return legend.name;
    return tag + ", " + bareTitle(legend);
}

int copiesOfName(const DeckSubmission& deck, const CardDB& db, const std::string& name) {
    int n = 0;
    if (deck.chosen_champion != kInvalidId && db.get(deck.chosen_champion).name == name) ++n;
    for (CardDefId id : deck.main_deck)
        if (db.get(id).name == name) ++n;
    return n;
}

/// Copies still allowed. An `unlimited_copies` card may be any number; this
/// builder still stops at 3 so one name does not fill the list.
int roomFor(const CardDef& card, const CopyCounts& copies, const DeckSubmission& deck,
            const CardDB& db) {
    if (card.unlimited_copies) return std::max(0, CopyCounts::kCopiesPerName - copiesOfName(deck, db, card.name));
    return copies.remaining(card);
}

void addCopies(DeckSubmission& deck, CopyCounts& copies, const CardDef& card, int n) {
    for (int i = 0; i < n; ++i) {
        deck.main_deck.push_back(card.id);
        copies.add(card);
    }
}

bool containsId(const std::vector<CardDefId>& ids, CardDefId id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

std::string joinErrors(const std::vector<std::string>& errors) {
    std::ostringstream out;
    for (const std::string& error : errors) out << "\n  " << error;
    return out.str();
}

struct Band {
    int lo;
    int hi;
    int want;
};

struct Mined {
    uint64_t seed = 0;
    std::vector<std::string> early;
    std::vector<std::string> mulligans;
    std::vector<std::string> holds;
    std::vector<std::string> plays;
    std::vector<std::string> runes;
    std::vector<std::string> combats;
    std::vector<std::string> conquers;
    std::vector<std::string> scores;
    std::vector<std::string> endings;
};

struct MirrorGame {
    uint64_t seed = 0;
    std::string text;
    bool usable = false;
};

MirrorGame playMirror(const CardDB& db, const CardRegistry& registry, const DeckSubmission& deck,
                      const std::string& legend_name, int number, uint64_t seed) {
    MirrorGame played;
    played.seed = seed;
    EventBus events;
    GameEngine engine(db, events, registry);
    PlayByPlay chronicle(engine.state(), events);
    PlayByPlay::Account account;
    account.number = number;
    account.seed = seed;
    account.deck1 = legend_name;
    account.deck2 = legend_name;
    account.agent1 = kAgent;
    account.agent2 = kAgent;
    chronicle.setAccount(account);
    RandomPlayerAgent seat1(seed * 2, 0.0);
    RandomPlayerAgent seat2(seed * 2 + 1, 0.0);
    engine.on_decision = [&chronicle](const GameState& state, const std::vector<Intent>&,
                                      const Intent& chosen) { chronicle.noteDecision(state, chosen); };
    try {
        const GameResult result = engine.runGame(deck, deck, seat1, seat2, seed);
        chronicle.complete(result.winner, result.final_scores[0], result.final_scores[1],
                           result.total_decisions, result.termination_reason);
        played.usable = true;
        played.text = chronicle.text();
    } catch (const std::exception& e) {
        chronicle.unusable(e.what());
        played.usable = false;
        played.text = chronicle.text();
    }
    engine.on_decision = nullptr;
    return played;
}

std::string trimLine(std::string line) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return trimCopy(line);
}

void mineGame(const MirrorGame& game, Mined& mined) {
    mined.seed = game.seed;
    if (!game.usable) return;
    std::istringstream in(game.text);
    std::string raw;
    int turn = 0;
    bool early = false;
    std::string block;
    auto flush = [&] {
        if (early && !block.empty()) mined.early.push_back(block);
        block.clear();
    };
    while (std::getline(in, raw)) {
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        if (raw.rfind("Turn ", 0) == 0) {
            flush();
            turn = 0;
            for (size_t i = 5; i < raw.size() && std::isdigit(static_cast<unsigned char>(raw[i])); ++i)
                turn = turn * 10 + (raw[i] - '0');
            early = turn >= 1 && turn <= 3;
            if (early) block = raw;
            continue;
        }
        if (early && !raw.empty() && (raw[0] == ' ' || raw[0] == '\t')) {
            block.push_back('\n');
            block += raw;
        }
        const std::string line = trimLine(raw);
        if (line.rfind("Mulligan:", 0) == 0) mined.mulligans.push_back(line);
        if (line.find(" holds ") != std::string::npos) mined.holds.push_back(line);
        if (line.find(" conquers ") != std::string::npos) mined.conquers.push_back(line);
        if (line.rfind("Combat at ", 0) == 0) mined.combats.push_back(line);
        if (line.find(" plays ") != std::string::npos) mined.plays.push_back(line);
        if (line.rfind("Runes at ", 0) == 0) mined.runes.push_back(line);
        if (line.rfind("Score ", 0) == 0) mined.scores.push_back(line);
        if (line.rfind("Ended by ", 0) == 0) mined.endings.push_back(line);
    }
    flush();
}

void writeSeeded(std::ostringstream& out, uint64_t seed, const std::vector<std::string>& lines, int cap) {
    if (lines.empty()) return;
    out << "Seed " << seed << ":\n";
    const int n = std::min(cap, static_cast<int>(lines.size()));
    for (int i = 0; i < n; ++i) out << lines[static_cast<size_t>(i)] << "\n";
    if (static_cast<int>(lines.size()) > cap) {
        out << (static_cast<int>(lines.size()) - cap)
            << " more are printed in the engine games below.\n";
    }
}

std::string unread(const char* what) {
    return std::string("Unread. No usable game printed ") + what + ".\n";
}

bool textAppears(const std::vector<MirrorGame>& games, const std::string& printed) {
    const std::string needle = trimCopy(printed);
    if (needle.empty()) return false;
    for (const MirrorGame& game : games) {
        if (!game.usable) continue;
        if (game.text.find(needle) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

DeckBuild buildLegendDeck(const CardDB& db, const std::string& legend_query) {
    const std::string query = trimCopy(legend_query);
    const RandomDeckBuilder builder(db);
    const CardDef* found = db.findByName(query);
    if (!found)
        throw std::runtime_error("no card named '" + query + "'");
    if (found->card_type != CardType::Legend)
        throw std::runtime_error("'" + found->name + "' is not a legend");
    const RandomDeckBuilder::LegendCards* pool = builder.legendCards(found->id);
    if (!pool) {
        std::string why = "no legal pool";
        for (const auto& [id, reason] : builder.unreachable()) {
            if (id == found->id) {
                why = reason;
                break;
            }
        }
        throw std::runtime_error("no deck can be built around '" + found->name + "': " + why);
    }

    const CardDef* neeko = db.findByName("Neeko, Blending In");
    auto skipNeeko = [&](CardDefId id) { return neeko && id == neeko->id; };

    DeckBuild built;
    built.legend_query = query;
    built.printed_name = found->name;
    built.alias = aliasOf(*found);
    DeckSubmission& deck = built.deck;
    deck.legend = found->id;

    // Champion units sit in the public main pool as well as the builder's
    // private champion list. The tag test is the one RandomDeckBuilder uses.
    const std::string& champion_tag = found->legendChampionTag();
    std::vector<CardDefId> champions;
    for (CardDefId id : pool->main) {
        const CardDef& card = db.get(id);
        if (card.card_type == CardType::Unit && card.super_type == SuperType::Champion &&
            card.hasTag(champion_tag))
            champions.push_back(id);
    }
    if (champions.empty())
        throw std::runtime_error("'" + found->name + "' has no champion unit in its pool");
    std::sort(champions.begin(), champions.end(), [&](CardDefId a, CardDefId b) {
        const CardDef& da = db.get(a);
        const CardDef& db_ = db.get(b);
        if (da.energy_cost != db_.energy_cost) return da.energy_cost < db_.energy_cost;
        if (da.power_cost != db_.power_cost) return da.power_cost < db_.power_cost;
        return da.id.raw() < db_.id.raw();
    });
    deck.chosen_champion = champions.front();

    CopyCounts copies;
    copies.add(db.get(deck.chosen_champion));

    std::vector<CardDefId> signatures = pool->signatures;
    std::sort(signatures.begin(), signatures.end(), [&](CardDefId a, CardDefId b) {
        const CardDef& da = db.get(a);
        const CardDef& db_ = db.get(b);
        if (da.energy_cost != db_.energy_cost) return da.energy_cost < db_.energy_cost;
        return da.id.raw() < db_.id.raw();
    });
    for (CardDefId id : signatures) {
        if (copies.signatures >= CopyCounts::kSignatureCards) break;
        const CardDef& card = db.get(id);
        if (roomFor(card, copies, deck, db) <= 0) continue;
        addCopies(deck, copies, card, 1);
    }

    const std::set<std::string> legend_words = wordsOf(printedRaw(*found));
    std::vector<CardDefId> ranked = pool->main;
    ranked.erase(std::remove_if(ranked.begin(), ranked.end(), skipNeeko), ranked.end());
    std::sort(ranked.begin(), ranked.end(), [&](CardDefId a, CardDefId b) {
        const int oa = wordOverlap(legend_words, db.get(a));
        const int ob = wordOverlap(legend_words, db.get(b));
        if (oa != ob) return oa > ob;
        return a.raw() < b.raw();
    });

    Band bands[] = {{0, 1, 12}, {2, 2, 12}, {3, 3, 9}, {4, 99, 6}};
    for (CardDefId id : deck.main_deck) {
        const int energy = db.get(id).energy_cost;
        for (Band& band : bands) {
            if (energy >= band.lo && energy <= band.hi && band.want > 0) {
                --band.want;
                break;
            }
        }
    }
    for (Band& band : bands) {
        for (CardDefId id : ranked) {
            if (band.want <= 0 || static_cast<int>(deck.main_deck.size()) >= kMainDeckCards) break;
            const CardDef& card = db.get(id);
            if (card.energy_cost < band.lo || card.energy_cost > band.hi) continue;
            const int room = roomFor(card, copies, deck, db);
            if (room <= 0) continue;
            const int n = std::min(room, std::min(band.want, kMainDeckCards - static_cast<int>(deck.main_deck.size())));
            addCopies(deck, copies, card, n);
            band.want -= n;
        }
    }
    bool progressed = true;
    while (static_cast<int>(deck.main_deck.size()) < kMainDeckCards && progressed) {
        progressed = false;
        for (CardDefId id : ranked) {
            if (static_cast<int>(deck.main_deck.size()) >= kMainDeckCards) break;
            const CardDef& card = db.get(id);
            const int room = roomFor(card, copies, deck, db);
            if (room <= 0) continue;
            const int n = std::min(room, kMainDeckCards - static_cast<int>(deck.main_deck.size()));
            addCopies(deck, copies, card, n);
            progressed = true;
        }
    }
    if (static_cast<int>(deck.main_deck.size()) != kMainDeckCards)
        throw std::runtime_error("the legal pool for '" + found->name +
                                 "' does not fill a 39-card main deck");
    if (neeko && (deck.chosen_champion == neeko->id || containsId(deck.main_deck, neeko->id)))
        throw std::runtime_error(
            "Neeko, Blending In is in the deck. DeckValidator requires exactly 3 additional "
            "legends then. This builder does not choose them.");

    if (pool->runes.empty())
        throw std::runtime_error("'" + found->name + "' has no rune list");
    const int lists = static_cast<int>(pool->runes.size());
    const int base = kRunes / lists;
    const int extra = kRunes % lists;
    for (int i = 0; i < lists; ++i) {
        const std::vector<CardDefId>& list = pool->runes[static_cast<size_t>(i)];
        CardDefId rune = list.front();
        for (CardDefId id : list)
            if (id < rune) rune = id;
        const int n = base + (i < extra ? 1 : 0);
        for (int k = 0; k < n; ++k) deck.rune_deck.push_back(rune);
    }

    std::vector<CardDefId> battlefields;
    for (const auto& [id, card] : db.all()) {
        if (card.card_type != CardType::Battlefield) continue;
        if (card.bannedOn(legalityDate()) || card.isTokenCard()) continue;
        if (!domainsInside(card, *found)) continue;
        battlefields.push_back(id);
    }
    std::sort(battlefields.begin(), battlefields.end());
    std::set<std::string> names;
    for (CardDefId id : battlefields) {
        if (names.insert(db.get(id).name).second) deck.battlefields.push_back(id);
        if (deck.battlefields.size() == 3) break;
    }
    if (deck.battlefields.size() != 3)
        throw std::runtime_error("fewer than 3 battlefields with different names for '" + found->name +
                                 "'");

    const DeckValidator validator(db);
    const ValidationResult result = validator.validate(deck);
    if (!result.is_legal)
        throw std::runtime_error("DeckValidator rejected the list for '" + found->name + "'" +
                                 joinErrors(result.errors));

    built.list = builder.toDeckList(deck);
    built.notes.push_back(
        "Sideboard: none. Tournament Rules 601.1.c.1 allows 10 or fewer cards where a sideboard "
        "is used. DeckValidator accepts a sideboard of 0 to 10 and does not require one. "
        "RandomDeckBuilder::toDeckList, which writes this list, has no sideboard section.");
    built.notes.push_back(
        "Battlefields: DeckValidator checks that there are 3, that each is a battlefield and not "
        "a token, and that the names differ. It does not check Domain Identity (CR 103.5.b, "
        "\"if applicable\"). This list keeps a battlefield whose domains are empty or inside the "
        "legend's identity, taking the three lowest card ids with different names. That skip is "
        "the builder's. It is not a validator result.");
    if (neeko && containsId(pool->main, neeko->id))
        built.notes.push_back(
            "Neeko, Blending In is in this legend's main-deck pool and is left out. When she is "
            "in a deck, DeckValidator requires exactly 3 additional legends. This builder does "
            "not choose them.");
    return built;
}

std::string formatLegendPlaybook(const CardDB& db, const CardRegistry& registry, const DeckBuild& built,
                                 int games, uint64_t seed) {
    if (games < 0 || games > 20)
        throw std::invalid_argument("mirror games must be from 0 to 20");
    if (seed == 0)
        throw std::invalid_argument("seed 0 is rejected so a printed game can be replayed");

    std::vector<MirrorGame> played;
    played.reserve(static_cast<size_t>(games));
    std::vector<Mined> mined;
    for (int i = 0; i < games; ++i) {
        const uint64_t game_seed = (seed + static_cast<uint64_t>(i)) & 0x7FFFFFFFu;
        MirrorGame game = playMirror(db, registry, built.deck, built.printed_name, i + 1, game_seed);
        Mined one;
        mineGame(game, one);
        mined.push_back(std::move(one));
        played.push_back(std::move(game));
    }

    const CardDef& legend = db.get(built.deck.legend);
    const CardDef& champion = db.get(built.deck.chosen_champion);
    const DeckReport report = analyzeDeck(built.deck, db);

    std::ostringstream out;
    out << "Playbook for " << built.legend_query << "\n";
    out << "Printed name: " << built.printed_name << "\n";
    if (built.alias != built.printed_name && built.alias != built.legend_query)
        out << "Deck-list name: " << built.alias << "\n";
    out << "\n";

    out << "How this list was built\n";
    out << "-----------------------\n";
    out << "The legend is the input. Cards are taken from the legal pool RandomDeckBuilder keeps "
           "for that legend: its Domain Identity, a champion unit with its champion tag, at most "
           "3 signature cards with that tag, runes of its domains, and no banned or token card.\n";
    out << "Chosen champion: " << champion.name << ", energy " << champion.energy_cost << ", power "
        << champion.power_cost << ", the champion unit in that pool with the lowest energy cost, "
           "then the lowest power cost, then the lowest card id.\n";
    out << "DeckValidator::validate accepted this list.\n";
    out << "\n";

    out << "Deck list\n";
    out << "---------\n";
    out << built.list;
    if (built.list.empty() || built.list.back() != '\n') out << "\n";
    out << "\n";

    out << "Rules the engine does not check\n";
    out << "-------------------------------\n";
    for (const std::string& note : built.notes) out << note << "\n\n";

    out << "Plan\n";
    out << "----\n";
    out << "Printed on " << legend.name << ":\n";
    const std::string legend_text = printedRaw(legend);
    if (legend_text.empty()) out << "This card has no printed ability text.\n";
    else out << legend_text << "\n";
    out << "\nPrinted on the chosen champion " << champion.name << ":\n";
    const std::string champion_text = printedRaw(champion);
    if (champion_text.empty()) out << "This card has no printed ability text.\n";
    else out << champion_text << "\n";
    out << "\nThe deck breakdown reads these roles from printed cost, keywords, and ability text.\n";
    if (report.roles.empty()) {
        out << "The deck breakdown listed no role.\n";
    } else {
        for (const DeckRole& role : report.roles) {
            out << "- " << role.title << " (" << role.reason << "): ";
            for (size_t i = 0; i < role.cards.size(); ++i) {
                if (i > 0) out << ", ";
                out << role.cards[i].copies << " " << role.cards[i].name;
                if (role.cards[i].champion) out << " (includes the champion)";
            }
            out << ".\n";
        }
    }
    out << "A played plan past the early turns below is unread.\n\n";

    out << "Early turns\n";
    out << "-----------\n";
    bool any_early = false;
    for (const Mined& game : mined) {
        if (game.early.empty()) continue;
        any_early = true;
        out << "Seed " << game.seed << ", turns 1 to 3, as the play runner printed them:\n";
        for (const std::string& block : game.early) out << block << "\n";
        out << "\n";
    }
    if (!any_early) out << unread("a turn 1, 2, or 3");
    out << "\n";

    out << "What to hold\n";
    out << "------------\n";
    bool any_mulligan = false;
    bool any_hold = false;
    for (const Mined& game : mined) {
        if (!game.mulligans.empty()) any_mulligan = true;
        if (!game.holds.empty()) any_hold = true;
    }
    if (!any_mulligan) out << unread("a mulligan");
    else {
        out << "Mulligan lines:\n";
        for (const Mined& game : mined) writeSeeded(out, game.seed, game.mulligans, 8);
    }
    if (!any_hold) out << unread("a hold");
    else {
        out << "Hold lines:\n";
        for (const Mined& game : mined) writeSeeded(out, game.seed, game.holds, 24);
    }
    out << "\n";

    out << "What to spend\n";
    out << "-------------\n";
    bool any_play = false;
    bool any_rune = false;
    for (const Mined& game : mined) {
        if (!game.plays.empty()) any_play = true;
        if (!game.runes.empty()) any_rune = true;
    }
    if (!any_play) out << unread("a play");
    else {
        out << "Plays, with the cost the play runner printed:\n";
        for (const Mined& game : mined) writeSeeded(out, game.seed, game.plays, 24);
    }
    if (!any_rune) out << unread("a rune count");
    else {
        out << "Rune counts:\n";
        for (const Mined& game : mined) writeSeeded(out, game.seed, game.runes, 12);
    }
    out << "\n";

    out << "Legend line\n";
    out << "-----------\n";
    out << "Printed on " << legend.name << ":\n";
    if (legend_text.empty()) out << "This card has no printed ability text.\n";
    else out << legend_text << "\n";
    out << "\n";
    bool any_combat = false;
    bool any_score = false;
    for (const Mined& game : mined) {
        if (!game.combats.empty()) any_combat = true;
        if (!game.conquers.empty() || !game.holds.empty() || !game.scores.empty() ||
            !game.endings.empty())
            any_score = true;
    }
    if (!any_combat) out << unread("a combat");
    else {
        out << "Combat lines:\n";
        for (const Mined& game : mined) writeSeeded(out, game.seed, game.combats, 16);
    }
    if (!any_score) out << unread("a conquer, a hold, or a score");
    else {
        out << "Conquers, holds, score lines, and how the game ended:\n";
        for (const Mined& game : mined) {
            std::vector<std::string> scoring = game.endings;
            scoring.insert(scoring.end(), game.conquers.begin(), game.conquers.end());
            scoring.insert(scoring.end(), game.holds.begin(), game.holds.end());
            scoring.insert(scoring.end(), game.scores.begin(), game.scores.end());
            writeSeeded(out, game.seed, scoring, 24);
        }
    }
    if (legend_text.empty() || !textAppears(played, legend_text))
        out << "Unread. No engine line says this printed text changed a combat or a score.\n";
    out << "\n";

    out << "Engine games\n";
    out << "------------\n";
    if (played.empty()) {
        out << "Unread. No mirror game was played.\n";
    } else {
        out << "Both seats played this list. The agent is " << kAgent
            << ". Replay one with the play runner:\n";
        out << "  ./riftbound --deck1 list.txt --deck2 list.txt --agent1 " << kAgent
            << " --agent2 " << kAgent << " --seed <seed> --web off\n\n";
        for (const MirrorGame& game : played) {
            if (!game.usable)
                out << "Seed " << game.seed
                    << " is not usable. The sections above quote usable games.\n";
            out << game.text;
            if (game.text.empty() || game.text.back() != '\n') out << "\n";
            out << "\n";
        }
    }
    return out.str();
}

}  // namespace riftbound
