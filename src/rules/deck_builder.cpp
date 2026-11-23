/// @file deck_builder.cpp
/// Builds one constructed deck for a legend and writes how to play it.
///
/// Saved lists under internal/deck-catalogue and internal/tournament-data set
/// the main deck's unit, spell, and gear counts. With none, the split stays
/// near 18, 18, and 3. When those three medians do not sum to 39, the playbook
/// says the median and the adjustment. The cards follow one plan read from the
/// printed legend line. Cards the saved lists play that carry the plan are
/// taken at the count those lists play. A card that does not carry the plan is
/// curve filler, and the playbook names it. Cards banned on 20261123 are left
/// out. A card that tells a player to channel is not added just to fill a type
/// count. A card the saved lists already play is kept. DeckValidator is the
/// judge on that date. Mirror games go through GameEngine::runGame with a
/// PlayByPlay account.

#include "rules/deck_builder.h"

#include "agents/random_player_agent.h"
#include "cards/card_registry.h"
#include "engine/game_engine.h"
#include "io/play_by_play.h"
#include "rules/deck_model.h"
#include "rules/random_deck.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace riftbound {
namespace {

constexpr int kMainDeckCards = 39;
constexpr int kRunes = 12;
constexpr int kLegalityDate = CardDef::kRadianceBanListDate;
static_assert(kLegalityDate == 20261123, "the playbook names this legality date");
constexpr char kAgent[] = "random-player:pass=0";

struct DateGuard {
    int restore = 0;
    bool unpin = false;
    DateGuard() {
        const int observed = legalityDate();
        setLegalityDate(0);
        unpin = observed == legalityDate();
        restore = observed;
        setLegalityDate(kLegalityDate);
    }
    ~DateGuard() { setLegalityDate(unpin ? 0 : restore); }
    DateGuard(const DateGuard&) = delete;
    DateGuard& operator=(const DateGuard&) = delete;
};

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

/// A spell whose printed text channels runes. Find Your Center and Catalyst of
/// Aeons are the ones a type count used to be filled with. The word is the
/// test: a fight or a counter that never channels is not one of these.
bool isChannelOrRampSpell(const CardDef& card) {
    if (card.card_type != CardType::Spell) return false;
    const std::string text = lowerCopy(printedRaw(card));
    return text.find("channel") != std::string::npos;
}

/// A printed instruction to channel runes, "Channel X rune(s)" (CR 431.5).
/// Filler uses this for units and gear. Spells stay on isChannelOrRampSpell.
/// "Only channel" limits the Channel Phase (Sandstone Chimera) and is not an
/// instruction. "If you would channel runes, channel that many" replaces a
/// channel (Seraphic Wyvern) and is not one either.
bool instructsChannel(const CardDef& card) {
    const std::string text = lowerCopy(stripParentheticals(printedRaw(card)));
    for (std::size_t pos = 0; (pos = text.find("channel", pos)) != std::string::npos;) {
        const bool only = pos >= 5 && text.compare(pos - 5, 5, "only ") == 0;
        std::size_t i = pos + (sizeof("channel") - 1);
        if (i < text.size() && text[i] == 's') ++i;
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
        if (!only && i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
            while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
            while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
            if (text.compare(i, 4, "rune") == 0) return true;
        }
        pos += sizeof("channel") - 1;
    }
    return false;
}

bool boundary(char c) { return std::isalnum(static_cast<unsigned char>(c)) == 0; }

bool hasWord(const std::string& text, const char* word) {
    const size_t n = std::char_traits<char>::length(word);
    for (size_t i = 0; i + n <= text.size(); ++i) {
        if (text.compare(i, n, word) != 0) continue;
        if (i > 0 && !boundary(text[i - 1])) continue;
        if (i + n < text.size() && !boundary(text[i + n])) continue;
        return true;
    }
    return false;
}

/// `stem` as a word, or as the start of one ("defend" matches "defends").
bool hasStem(const std::string& text, const char* stem) {
    const size_t n = std::char_traits<char>::length(stem);
    for (size_t i = 0; i + n <= text.size(); ++i) {
        if (text.compare(i, n, stem) != 0) continue;
        if (i > 0 && !boundary(text[i - 1])) continue;
        return true;
    }
    return false;
}

bool containsPhrase(const std::string& text, const char* phrase) {
    return text.find(phrase) != std::string::npos;
}

std::string planText(const CardDef& card) { return lowerCopy(stripParentheticals(printedRaw(card))); }

bool givesMight(const CardDef& card, const std::string& text) {
    if (card.keywords.has(Keyword::Assault) || card.keywords.has(Keyword::Shield) ||
        card.keywords.has(Keyword::Disarm))
        return true;
    if (containsPhrase(text, "[shield") || containsPhrase(text, "[assault") ||
        containsPhrase(text, "[disarm"))
        return true;
    if (hasWord(text, "buff") || hasWord(text, "buffs")) return true;
    for (size_t i = 0; i + 3 < text.size(); ++i) {
        if (text[i] != '+' && text[i] != '-') continue;
        size_t j = i + 1;
        while (j < text.size() && std::isspace(static_cast<unsigned char>(text[j]))) ++j;
        if (j >= text.size() || !std::isdigit(static_cast<unsigned char>(text[j]))) continue;
        while (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j]))) ++j;
        while (j < text.size() && std::isspace(static_cast<unsigned char>(text[j]))) ++j;
        if (j + 2 < text.size() && text[j] == '[' && text[j + 1] == 'm' && text[j + 2] == ']')
            return true;
    }
    return false;
}

/// A spell that channels, or a unit or gear whose text says "Channel N rune(s)",
/// does not carry a plan as filler. "Only channel" and "channel that many" stay
/// out of that test, the same way `instructsChannel` reads them.
bool channelsAsFiller(const CardDef& card) {
    if (isChannelOrRampSpell(card)) return true;
    return instructsChannel(card);
}

/// "Ready a gear" / "Ready 2 gear". The words after "ready" name a gear, not a unit
/// and not runes.
bool readiesGear(const std::string& text) {
    const char* word = "ready";
    for (size_t i = 0; i + 5 <= text.size(); ++i) {
        if (text.compare(i, 5, word) != 0) continue;
        if (i > 0 && !boundary(text[i - 1])) continue;
        size_t end = i + 5;
        while (end < text.size() && std::isalpha(static_cast<unsigned char>(text[end]))) ++end;
        const std::string after = text.substr(end, 32);
        if (containsPhrase(after, "gear") && !containsPhrase(after, "unit")) return true;
    }
    return false;
}

bool givesBuff(const std::string& text) {
    return hasWord(text, "buff") || hasWord(text, "buffs") || hasWord(text, "buffed");
}

bool drawsForUnitsAtBattlefield(const std::string& text) {
    if (!hasStem(text, "draw") || !containsPhrase(text, "unit") || !containsPhrase(text, "battlefield"))
        return false;
    for (char c : text)
        if (std::isdigit(static_cast<unsigned char>(c)) != 0 || c == '+') return true;
    return false;
}

/// The first number after "energy cost", when the line then readies runes.
int runeReadyEnergyGate(const std::string& text) {
    const auto at = text.find("energy cost");
    if (at == std::string::npos) return 0;
    if (!containsPhrase(text, "rune") || !hasStem(text, "ready")) return 0;
    for (size_t i = at; i < text.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(text[i])) == 0) continue;
        int n = 0;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) != 0)
            n = n * 10 + (text[i++] - '0');
        return n;
    }
    return 0;
}

std::string collapseSpace(std::string s) {
    std::string out;
    bool pending = false;
    for (char c : s) {
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (c == ' ') {
            pending = !out.empty();
            continue;
        }
        if (pending) out.push_back(' ');
        pending = false;
        out.push_back(c);
    }
    return trimCopy(out);
}

bool effectVerb(const std::string& lower) {
    static const char* verbs[] = {
        "move",     "ready",   "stun",    "draw",    "conquer", "hold",    "play",
        "buff",     "channel", "kill",    "banish",  "hide",    "attach",  "equip",
        "recycle",  "give",    "gain",    "spend",   "win",     "choose",  "discard",
        "reveal",   "return",  "heal",    "recall",  "replace", "become",  "deal",
        "fight",    "score",   "defend",  "attack",  "copy",    "reduce",  "assign"};
    for (const char* verb : verbs)
        if (hasStem(lower, verb)) return true;
    return false;
}

/// The printed effect, reminders removed. A sentence that is only an Empower
/// cost is not the effect. The playbook still quotes the raw card above this.
std::string printedPlanName(const CardDef& legend) {
    const std::string stripped = stripParentheticals(printedRaw(legend));
    std::string name;
    std::string cur;
    auto take = [&](std::string sentence) {
        sentence = collapseSpace(std::move(sentence));
        if (sentence.empty() || !effectVerb(lowerCopy(sentence))) return;
        if (!name.empty()) name.push_back(' ');
        name += sentence;
    };
    for (char c : stripped) {
        if (c == '\n' || c == '\r') c = ' ';
        cur.push_back(c);
        if (c == '.') {
            take(cur);
            cur.clear();
        }
    }
    take(cur);
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) name.pop_back();
    if (name.empty()) return "The printed line";
    return name;
}

/// What a spell or gear must do to carry this legend's own line. None means a
/// spell does not carry it: the line's payoff is the legend's.
enum class SpellNeed {
    None,
    DefendAlone,
    DrawForUnits,
    ShowdownToBase,
    Stuns,
    BecomesMighty,
    ChoosesFriendly,
    RecycleRune,
    Temporary,
    ReadiesGear,
    Brush,
    Buff,
    ExcessDamage,
    CopiesUnit,
    FriendlyBase,
    Deploy,
    ExhaustedToBase,
};

struct LegendPlan {
    SpellNeed spell = SpellNeed::None;
    std::string name;
    std::string detail;
    std::string text;
    /// A unit can be the unit the line uses.
    bool unit_body = false;
    /// Only a unit whose printed Might is 5 or more (the unit the line plays).
    bool mighty_body = false;
    /// Only a unit with Temporary.
    bool temporary_body = false;
    /// A gear is the object the line names.
    bool gear_body = false;
    /// Only a gear with Deploy.
    bool deploy_gear = false;
    /// A card whose energy cost is at least this. 0 means the cost is not the test.
    int min_energy = 0;
};

LegendPlan readLegendPlan(const CardDef& legend) {
    LegendPlan plan;
    plan.text = planText(legend);
    const std::string& text = plan.text;
    const std::string printed = printedPlanName(legend);
    auto printedAs = [&](const char* detail, SpellNeed spell) {
        plan.name = printed;
        plan.detail = detail;
        plan.spell = spell;
    };

    // "While a friendly unit defends alone, it gets +2 [M]." The +2 is the
    // legend's. "The only unit you control there" is not "defends alone."
    if (hasStem(text, "defend") && hasWord(text, "alone")) {
        plan.name = "Defend alone";
        plan.detail =
            "A friendly unit that defends alone gets the Might on that line. A unit carries it as "
            "that body. A spell or a gear carries it when it says a unit defends alone.";
        plan.spell = SpellNeed::DefendAlone;
        plan.unit_body = true;
        return plan;
    }
    // "When you conquer, if you have 4+ units at that battlefield, draw 2."
    // The draw is the legend's. Another conquer line does not get this reading.
    if (hasStem(text, "conquer") && containsPhrase(text, "4+") && containsPhrase(text, "unit") &&
        containsPhrase(text, "at that battlefield") && containsPhrase(text, "draw 2")) {
        plan.name = "Conquer with units";
        plan.detail =
            "The line draws 2 when you conquer if you have 4+ units at that battlefield. A unit "
            "carries it as one of those units. A spell or a gear carries it when it draws for 4+ "
            "units at a battlefield.";
        plan.spell = SpellNeed::DrawForUnits;
        plan.unit_body = true;
        return plan;
    }
    // "[E]: If it's your turn, move a friendly unit in a showdown to base and
    // if I'm Empowered, ready it." The ready checks this legend.
    if (hasStem(text, "move") && containsPhrase(text, "friendly") && containsPhrase(text, "showdown") &&
        containsPhrase(text, "to base") && hasStem(text, "ready") && containsPhrase(text, "empowered")) {
        plan.name = "Move a friendly unit in a showdown to base, and ready it only if this legend is "
                     "Empowered";
        plan.detail =
            "If it is your turn, the line moves a friendly unit in a showdown to base, and readies "
            "that unit only if this legend is Empowered. A unit carries it as the one moved. A spell "
            "or a gear carries it when it moves a friendly unit in a showdown to base. A card with "
            "Empower does not carry this line.";
        plan.spell = SpellNeed::ShowdownToBase;
        plan.unit_body = true;
        return plan;
    }

    const int gate = runeReadyEnergyGate(text);
    if (gate > 0) {
        plan.min_energy = gate;
        printedAs("The plan is the printed line above. A card with that energy cost or more carries "
                  "it as the play the line names. The line readies runes, not a unit.",
                  SpellNeed::None);
        plan.detail = "The plan is the printed line above. A card with energy cost " +
                      std::to_string(gate) +
                      " or more carries it as the play the line names. The line readies runes, not a "
                      "unit.";
        return plan;
    }
    if (readiesGear(text)) {
        plan.gear_body = true;
        printedAs("The plan is the printed line above. A gear carries it as the gear that is readied. "
                  "A spell carries it when it readies a gear. Readying a gear does not attach it.",
                  SpellNeed::ReadiesGear);
        return plan;
    }
    if (containsPhrase(text, "sprite") && containsPhrase(text, "temporary") &&
        containsPhrase(text, "play")) {
        plan.temporary_body = true;
        printedAs("The plan is the printed line above. A unit with Temporary carries it. The line "
                  "plays a ready Sprite token, and that ability costs less for each friendly unit "
                  "with Temporary.",
                  SpellNeed::Temporary);
        return plan;
    }
    if (hasStem(text, "recycle") && containsPhrase(text, "rune") && containsPhrase(text, "ready me")) {
        printedAs("The plan is the printed line above. A spell or a gear carries it when it recycles "
                  "a rune. The Gold token and readying this legend are this legend's.",
                  SpellNeed::RecycleRune);
        return plan;
    }
    if (hasStem(text, "stun") && givesBuff(text) && containsPhrase(text, "friendly")) {
        plan.unit_body = true;
        printedAs("The plan is the printed line above. A unit carries it as the friendly unit that is "
                  "buffed. A spell or a gear carries it when it stuns an enemy unit. The stun happens "
                  "before this legend buffs.",
                  SpellNeed::Stuns);
        return plan;
    }
    if (hasStem(text, "become") && containsPhrase(text, "mighty")) {
        plan.unit_body = true;
        printedAs("The plan is the printed line above. A unit carries it as the unit that becomes "
                  "Mighty. A spell or a gear carries it when it makes a unit become Mighty. The "
                  "channel is this legend's.",
                  SpellNeed::BecomesMighty);
        return plan;
    }
    if (containsPhrase(text, "play") && containsPhrase(text, "mighty")) {
        plan.mighty_body = true;
        printedAs("The plan is the printed line above. A unit with 5 or more Might carries it as the "
                  "Mighty unit this line plays. The channel is this legend's.",
                  SpellNeed::None);
        return plan;
    }
    if (containsPhrase(text, "choose") && containsPhrase(text, "friendly unit")) {
        plan.unit_body = true;
        printedAs("The plan is the printed line above. A unit carries it as the friendly unit that is "
                  "chosen. A spell or a gear carries it when it chooses a friendly unit. Readying "
                  "that unit, and readying this legend when you conquer, are this legend's.",
                  SpellNeed::ChoosesFriendly);
        return plan;
    }
    if (containsPhrase(text, "brush")) {
        plan.unit_body = true;
        printedAs("The plan is the printed line above. A unit carries it as a unit you conquer or "
                  "hold with. A spell or a gear carries it when it names Brush. The line replaces "
                  "that battlefield with a Brush token.",
                  SpellNeed::Brush);
        return plan;
    }
    if (containsPhrase(text, "would die") &&
        (hasStem(text, "recall") || hasStem(text, "heal"))) {
        plan.unit_body = true;
        printedAs("The plan is the printed line above. A unit carries it as the buffed unit the "
                  "replacement keeps. A spell or a gear carries it when it buffs a unit. When you "
                  "conquer, the line readies this legend.",
                  SpellNeed::Buff);
        return plan;
    }
    if (containsPhrase(text, "excess") && containsPhrase(text, "damage")) {
        plan.unit_body = true;
        printedAs("The plan is the printed line above. A unit carries it as the unit the line "
                  "readies. A spell or a gear carries it when it assigns excess damage.",
                  SpellNeed::ExcessDamage);
        return plan;
    }
    if (hasStem(text, "copy") && containsPhrase(text, "unit")) {
        plan.unit_body = true;
        printedAs("The plan is the printed line above. A unit carries it as the unit that is copied. "
                  "A spell or a gear carries it when it copies a unit.",
                  SpellNeed::CopiesUnit);
        return plan;
    }
    if (hasStem(text, "move") && containsPhrase(text, "exhausted") && containsPhrase(text, "friendly") &&
        containsPhrase(text, "base")) {
        plan.unit_body = true;
        printedAs("The plan is the printed line above. A unit carries it as the unit those abilities "
                  "buff or move. A spell or a gear carries it when it buffs a unit, or moves an "
                  "exhausted friendly unit from a battlefield to its base.",
                  SpellNeed::ExhaustedToBase);
        return plan;
    }
    if (containsPhrase(text, "deploy") && hasStem(text, "move") && containsPhrase(text, "friendly")) {
        plan.unit_body = true;
        plan.deploy_gear = true;
        printedAs("The plan is the printed line above. A gear with Deploy carries it. A unit carries "
                  "it as the friendly unit the line moves to that battlefield.",
                  SpellNeed::Deploy);
        return plan;
    }
    if (hasStem(text, "move") && containsPhrase(text, "friendly") && containsPhrase(text, "base")) {
        plan.unit_body = true;
        printedAs("The plan is the printed line above. A unit carries it as the friendly unit that is "
                  "moved. A spell or a gear carries it when it moves a friendly unit to or from its "
                  "base.",
                  SpellNeed::FriendlyBase);
        return plan;
    }

    plan.name = printed;
    plan.spell = SpellNeed::None;
    plan.unit_body = containsPhrase(text, "friendly unit") || containsPhrase(text, "your unit") ||
                     containsPhrase(text, "unit you") || containsPhrase(text, "units you");
    plan.gear_body = containsPhrase(text, "equipment") || containsPhrase(text, "attach") ||
                     containsPhrase(text, "play a gear") || containsPhrase(text, "friendly gear") ||
                     containsPhrase(text, "your gear");
    plan.detail = "The plan is the printed line above.";
    if (plan.unit_body) plan.detail += " A unit carries it as the unit that line uses.";
    if (plan.gear_body) plan.detail += " A gear carries it as the gear that line names.";
    return plan;
}

bool carriesPlan(const LegendPlan& plan, const CardDef& card,
                 const std::set<std::string>& /*legend_words*/) {
    if (channelsAsFiller(card)) return false;
    const std::string text = planText(card);
    const bool unit = card.card_type == CardType::Unit;
    const bool gear = card.card_type == CardType::Gear;
    const bool spellish = card.card_type == CardType::Spell || gear;

    if (plan.min_energy > 0) return card.energy_cost >= plan.min_energy;
    if (unit && plan.unit_body) return true;
    if (unit && plan.mighty_body && card.might >= 5) return true;
    if (unit && plan.temporary_body &&
        (card.keywords.has(Keyword::Temporary) || containsPhrase(text, "temporary")))
        return true;
    if (gear && plan.gear_body) return true;
    if (gear && plan.deploy_gear &&
        (card.keywords.has(Keyword::Deploy) || containsPhrase(text, "deploy")))
        return true;
    if (!spellish) return false;

    switch (plan.spell) {
        case SpellNeed::None:
            return false;
        case SpellNeed::DefendAlone:
            return hasStem(text, "defend") && hasWord(text, "alone");
        case SpellNeed::DrawForUnits:
            return drawsForUnitsAtBattlefield(text);
        case SpellNeed::ShowdownToBase:
            return hasStem(text, "move") && containsPhrase(text, "friendly") &&
                   containsPhrase(text, "showdown") && containsPhrase(text, "to base");
        case SpellNeed::Stuns:
            return hasStem(text, "stun");
        case SpellNeed::BecomesMighty:
            return (hasStem(text, "become") && containsPhrase(text, "mighty")) || givesMight(card, text);
        case SpellNeed::ChoosesFriendly:
            return containsPhrase(text, "choose") && containsPhrase(text, "friendly unit");
        case SpellNeed::RecycleRune:
            return hasStem(text, "recycle") && containsPhrase(text, "rune");
        case SpellNeed::Temporary:
            return containsPhrase(text, "temporary");
        case SpellNeed::ReadiesGear:
            return readiesGear(text);
        case SpellNeed::Brush:
            return containsPhrase(text, "brush");
        case SpellNeed::Buff:
            return givesBuff(text);
        case SpellNeed::ExcessDamage:
            return containsPhrase(text, "excess") && containsPhrase(text, "damage");
        case SpellNeed::CopiesUnit:
            return hasStem(text, "copy") && containsPhrase(text, "unit");
        case SpellNeed::FriendlyBase:
            return hasStem(text, "move") && containsPhrase(text, "friendly") &&
                   containsPhrase(text, "base") &&
                   (containsPhrase(text, "to or from") || containsPhrase(text, "to its base") ||
                    containsPhrase(text, "from its base") || containsPhrase(text, "to base") ||
                    containsPhrase(text, "from base"));
        case SpellNeed::Deploy:
            return containsPhrase(text, "deploy");
        case SpellNeed::ExhaustedToBase:
            return (hasStem(text, "move") && containsPhrase(text, "exhausted") &&
                    containsPhrase(text, "friendly") && containsPhrase(text, "base")) ||
                   givesBuff(text);
    }
    return false;
}

struct NamedCopy {
    std::string name;
    int copies = 0;
    int energy = 0;
    int power = 0;
};

std::vector<NamedCopy> copiesInMain(const DeckSubmission& deck, const CardDB& db) {
    std::vector<NamedCopy> lines;
    for (CardDefId id : deck.main_deck) {
        const CardDef& card = db.get(id);
        auto it = std::find_if(lines.begin(), lines.end(),
                               [&](const NamedCopy& line) { return line.name == card.name; });
        if (it == lines.end())
            lines.push_back(NamedCopy{card.name, 1, card.energy_cost, card.power_cost});
        else
            ++it->copies;
    }
    return lines;
}

std::string costPhrase(const NamedCopy& line) {
    std::string text = std::to_string(line.energy) + " energy";
    if (line.power > 0) text += " and " + std::to_string(line.power) + " power";
    return text;
}

std::string joinNamed(const std::vector<NamedCopy>& lines) {
    std::ostringstream out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) out << (i + 1 == lines.size() ? ", and " : ", ");
        out << lines[i].copies << " " << lines[i].name << " (" << costPhrase(lines[i]) << ")";
    }
    return out.str();
}

std::vector<NamedCopy> pickNamed(const std::vector<NamedCopy>& lines,
                                 const std::vector<std::string>& names) {
    std::vector<NamedCopy> out;
    for (const std::string& name : names) {
        auto it = std::find_if(lines.begin(), lines.end(),
                               [&](const NamedCopy& line) { return line.name == name; });
        if (it != lines.end()) out.push_back(*it);
    }
    return out;
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
    for (CardDefId id : deck.sideboard)
        if (db.get(id).name == name) ++n;
    return n;
}

/// Copies still allowed. This builder stops at 3 of a name, including a card
/// the rules would allow any number of, so one name does not fill the list.
int roomFor(const CardDef& card, const CopyCounts& copies, const DeckSubmission& deck,
            const CardDB& db) {
    if (card.super_type == SuperType::Signature && copies.signatures >= CopyCounts::kSignatureCards)
        return 0;
    const int cap = card.keywords.has(Keyword::Unique) ? 1 : CopyCounts::kCopiesPerName;
    return std::max(0, cap - copiesOfName(deck, db, card.name));
}

void addCopies(DeckSubmission& deck, CopyCounts& copies, const CardDef& card, int n) {
    for (int i = 0; i < n; ++i) {
        deck.main_deck.push_back(card.id);
        copies.add(card);
    }
}

std::string joinErrors(const std::vector<std::string>& errors) {
    std::ostringstream out;
    for (const std::string& error : errors) out << "\n  " << error;
    return out.str();
}

int medianRounded(std::vector<int> values) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    const std::size_t n = values.size();
    if (n % 2 == 1) return values[n / 2];
    return (values[n / 2 - 1] + values[n / 2] + 1) / 2;
}

std::string displayPath(const std::filesystem::path& path) {
    const std::string s = path.generic_string();
    for (const char* marker : {"internal/deck-catalogue", "internal/tournament-data"}) {
        const auto pos = s.find(marker);
        if (pos != std::string::npos) return s.substr(pos);
    }
    return path.string();
}

std::vector<std::string> discoverListRoots() {
    std::vector<std::filesystem::path> bases;
    if (const char* root = std::getenv("RIFTBOUND_ROOT"))
        if (root[0] != '\0') bases.emplace_back(root);
    std::error_code ec;
    std::filesystem::path cur = std::filesystem::current_path(ec);
    for (int i = 0; i < 6 && !ec && !cur.empty(); ++i) {
        bases.push_back(cur);
        const std::filesystem::path parent = cur.parent_path();
        if (parent.empty() || parent == cur) break;
        cur = parent;
    }
    std::vector<std::string> roots;
    for (const std::filesystem::path& base : bases) {
        for (const char* rel : {"internal/deck-catalogue", "internal/tournament-data"}) {
            const std::filesystem::path dir = base / rel;
            std::error_code err;
            if (!std::filesystem::is_directory(dir, err)) continue;
            const std::string s = dir.string();
            if (std::find(roots.begin(), roots.end(), s) == roots.end()) roots.push_back(s);
        }
    }
    return roots;
}

struct SavedList {
    std::string display;
    CardDefId champion = kInvalidId;
    std::vector<CardDefId> main;
    std::vector<CardDefId> battlefields;
    std::vector<std::pair<CardDefId, int>> runes;
    std::vector<std::pair<CardDefId, int>> sideboard;
};

std::optional<std::pair<int, std::string>> parseCountLine(const std::string& line) {
    std::istringstream iss(line);
    int count = 0;
    if (!(iss >> count) || count <= 0) return std::nullopt;
    std::string name;
    std::getline(iss, name);
    name = trimCopy(name);
    if (name.empty()) return std::nullopt;
    return std::make_pair(count, name);
}

CardDefId resolveCard(const CardDB& db, const std::string& entry) {
    std::string name = entry;
    std::optional<uint32_t> printing;
    if (const auto hash = entry.rfind(" #"); hash != std::string::npos && hash + 2 < entry.size() &&
        std::all_of(entry.begin() + static_cast<std::ptrdiff_t>(hash) + 2, entry.end(),
                    [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
        printing = static_cast<uint32_t>(std::stoul(entry.substr(hash + 2)));
        name = trimCopy(entry.substr(0, hash));
    }
    if (printing.has_value()) {
        for (const CardDef* card : db.findAllByName(name))
            if (card->id.raw() == *printing) return card->id;
        return kInvalidId;
    }
    const CardDef* card = db.findByName(name);
    return card ? card->id : kInvalidId;
}

/// A constructed list for `legend_id`, or nullopt when the file is not one.
std::optional<SavedList> readSavedList(const std::filesystem::path& path, const CardDB& db,
                                       CardDefId legend_id) {
    std::ifstream file(path);
    if (!file) return std::nullopt;
    SavedList saved;
    saved.display = displayPath(path);
    std::string raw;
    std::string section;
    bool legend_ok = false;
    bool main_failed = false;
    while (std::getline(file, raw)) {
        std::string line = trimCopy(raw);
        if (line.empty() || line[0] == '#') continue;
        if (line.back() == ':') {
            section = lowerCopy(trimCopy(line.substr(0, line.size() - 1)));
            continue;
        }
        const auto parsed = parseCountLine(line);
        if (!parsed || section.empty()) continue;
        const CardDefId id = resolveCard(db, parsed->second);
        if (section == "legend") {
            legend_ok = id == legend_id;
            if (!legend_ok) return std::nullopt;
            continue;
        }
        if (id == kInvalidId) {
            if (section == "maindeck" || section == "champion") main_failed = true;
            continue;
        }
        if (section == "champion" && saved.champion == kInvalidId) saved.champion = id;
        else if (section == "maindeck") {
            for (int i = 0; i < parsed->first; ++i) saved.main.push_back(id);
        } else if (section == "battlefields") {
            for (int i = 0; i < parsed->first; ++i) saved.battlefields.push_back(id);
        } else if (section == "runes") {
            saved.runes.emplace_back(id, parsed->first);
        } else if (section == "sideboard") {
            saved.sideboard.emplace_back(id, parsed->first);
        }
    }
    if (!legend_ok || main_failed || saved.champion == kInvalidId || saved.main.size() != 39)
        return std::nullopt;
    return saved;
}

std::vector<SavedList> loadSavedLists(const CardDB& db, CardDefId legend,
                                      const std::vector<std::string>& roots) {
    std::vector<std::filesystem::path> files;
    for (const std::string& root : roots) {
        std::error_code ec;
        if (!std::filesystem::is_directory(root, ec)) continue;
        std::filesystem::recursive_directory_iterator it(
            root, std::filesystem::directory_options::skip_permission_denied, ec);
        const std::filesystem::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            std::error_code ferr;
            if (!it->is_regular_file(ferr)) continue;
            if (it->path().extension() != ".txt") continue;
            std::error_code size_err;
            if (it->file_size(size_err) > 200000u) continue;
            files.push_back(it->path());
        }
    }
    std::sort(files.begin(), files.end());
    std::vector<SavedList> lists;
    for (const std::filesystem::path& path : files) {
        if (std::optional<SavedList> one = readSavedList(path, db, legend))
            lists.push_back(std::move(*one));
    }
    return lists;
}

struct Split {
    int units = 18;
    int spells = 18;
    int gear = 3;
    int sum() const { return units + spells + gear; }
};

/// `median` is each column on its own. `used` is that median brought to 39
/// cards. The two are equal when the median already sums to 39.
struct SplitPlan {
    Split median;
    Split used;
};

Split defaultSplit() { return {}; }

SplitPlan splitFrom(const std::vector<int>& units, const std::vector<int>& spells,
                    const std::vector<int>& gear) {
    SplitPlan plan;
    plan.median.units = medianRounded(units);
    plan.median.spells = medianRounded(spells);
    plan.median.gear = medianRounded(gear);
    plan.used = plan.median;
    auto sum = [&] { return plan.used.sum(); };
    while (sum() > kMainDeckCards) {
        if (plan.used.units >= plan.used.spells && plan.used.units >= plan.used.gear &&
            plan.used.units > 0)
            --plan.used.units;
        else if (plan.used.spells >= plan.used.gear && plan.used.spells > 0)
            --plan.used.spells;
        else
            --plan.used.gear;
    }
    while (sum() < kMainDeckCards) {
        if (plan.used.units <= plan.used.spells) ++plan.used.units;
        else ++plan.used.spells;
    }
    return plan;
}

std::string adjustmentSentence(const Split& median, const Split& used) {
    const int du = used.units - median.units;
    const int ds = used.spells - median.spells;
    const int dg = used.gear - median.gear;
    std::vector<std::string> parts;
    auto push = [&](int delta, const char* one, const char* many) {
        if (delta == 0) return;
        const int n = std::abs(delta);
        parts.push_back(std::to_string(n) + " " + (n == 1 ? one : many));
    };
    push(du, "unit", "units");
    push(ds, "spell", "spells");
    push(dg, "gear", "gear");
    std::ostringstream out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out << (i + 1 == parts.size() ? " and " : ", ");
        out << parts[i];
    }
    const int changed = std::abs(du) + std::abs(ds) + std::abs(dg);
    const bool added = du + ds + dg > 0;
    if (parts.size() == 1 && changed == 1) out << (added ? " was added" : " was removed");
    else out << (added ? " were added" : " were removed");
    out << " so those counts sum to 39: " << used.units << " units, " << used.spells
        << " spells, and " << used.gear << " gear.";
    return out.str();
}

struct Seen {
    CardDefId id = kInvalidId;
    std::vector<int> counts;
    int lists() const { return static_cast<int>(counts.size()); }
    int typical() const { return medianRounded(counts); }
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
        const std::string line = trimCopy(raw);
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
    return buildLegendDeck(db, legend_query, discoverListRoots());
}

DeckBuild buildLegendDeck(const CardDB& db, const std::string& legend_query,
                          const std::vector<std::string>& list_roots) {
    const DateGuard date;
    const std::string query = trimCopy(legend_query);
    const RandomDeckBuilder random(db);
    const CardDef* found = db.findByName(query);
    if (!found) throw std::runtime_error("no card named '" + query + "'");
    if (found->card_type != CardType::Legend)
        throw std::runtime_error("'" + found->name + "' is not a legend");
    const RandomDeckBuilder::LegendCards* pool = random.legendCards(found->id);
    if (!pool) {
        std::string why = "no legal pool";
        for (const auto& [id, reason] : random.unreachable()) {
            if (id == found->id) {
                why = reason;
                break;
            }
        }
        throw std::runtime_error("no deck can be built around '" + found->name + "': " + why);
    }

    const CardDef* neeko = db.findByName("Neeko, Blending In");
    std::set<CardDefId> pool_ids(pool->main.begin(), pool->main.end());
    pool_ids.insert(pool->signatures.begin(), pool->signatures.end());
    auto poolPrinting = [&](const std::string& name) -> CardDefId {
        for (CardDefId id : pool->main)
            if (db.get(id).name == name && !db.get(id).bannedOn(kLegalityDate)) return id;
        for (CardDefId id : pool->signatures)
            if (db.get(id).name == name && !db.get(id).bannedOn(kLegalityDate)) return id;
        return kInvalidId;
    };
    auto legalHere = [&](const CardDef& card) {
        return pool_ids.count(card.id) && !card.bannedOn(kLegalityDate) && !card.isTokenCard() &&
               isMainDeckType(card) && domainsInside(card, *found) &&
               !(neeko && card.id == neeko->id);
    };

    std::vector<SavedList> lists = loadSavedLists(db, found->id, list_roots);
    const std::string& champion_tag = found->legendChampionTag();
    lists.erase(std::remove_if(lists.begin(), lists.end(),
                               [&](const SavedList& list) {
                                   const CardDef& champion = db.get(list.champion);
                                   if (champion.card_type != CardType::Unit ||
                                       champion.super_type != SuperType::Champion)
                                       return true;
                                   if (!champion.hasTag(champion_tag) || !domainsInside(champion, *found))
                                       return true;
                                   if (champion.bannedOn(kLegalityDate) || champion.isTokenCard()) return true;
                                   int units = 0;
                                   int spells = 0;
                                   int gear = 0;
                                   for (CardDefId id : list.main) {
                                       switch (db.get(id).card_type) {
                                           case CardType::Unit: ++units; break;
                                           case CardType::Spell: ++spells; break;
                                           case CardType::Gear: ++gear; break;
                                           default: return true;
                                       }
                                   }
                                   return units + spells + gear != kMainDeckCards;
                               }),
                lists.end());

    DeckBuild built;
    built.legend_query = query;
    built.printed_name = found->name;
    built.alias = aliasOf(*found);
    built.legality_date = kLegalityDate;
    DeckSubmission& deck = built.deck;
    deck.legend = found->id;

    std::vector<CardDefId> champions;
    for (CardDefId id : pool->main) {
        const CardDef& card = db.get(id);
        if (card.card_type == CardType::Unit && card.super_type == SuperType::Champion &&
            card.hasTag(champion_tag) && !card.bannedOn(kLegalityDate))
            champions.push_back(id);
    }
    if (champions.empty())
        throw std::runtime_error("'" + found->name + "' has no champion unit in its pool");

    if (!lists.empty()) {
        std::map<CardDefId, int> votes;
        for (const SavedList& list : lists) ++votes[list.champion];
        std::sort(champions.begin(), champions.end(), [&](CardDefId a, CardDefId b) {
            if (votes[a] != votes[b]) return votes[a] > votes[b];
            const CardDef& da = db.get(a);
            const CardDef& db_ = db.get(b);
            if (da.energy_cost != db_.energy_cost) return da.energy_cost < db_.energy_cost;
            if (da.power_cost != db_.power_cost) return da.power_cost < db_.power_cost;
            return da.id.raw() < db_.id.raw();
        });
    } else {
        std::sort(champions.begin(), champions.end(), [&](CardDefId a, CardDefId b) {
            const CardDef& da = db.get(a);
            const CardDef& db_ = db.get(b);
            if (da.energy_cost != db_.energy_cost) return da.energy_cost < db_.energy_cost;
            if (da.power_cost != db_.power_cost) return da.power_cost < db_.power_cost;
            return da.id.raw() < db_.id.raw();
        });
    }
    deck.chosen_champion = champions.front();

    CopyCounts copies;
    copies.add(db.get(deck.chosen_champion));

    Split target = defaultSplit();
    std::map<std::string, Seen> played;
    std::set<std::string> banned_names;
    if (!lists.empty()) {
        std::vector<int> unit_counts;
        std::vector<int> spell_counts;
        std::vector<int> gear_counts;
        for (const SavedList& list : lists) {
            int units = 0;
            int spells = 0;
            int gear = 0;
            std::map<std::string, int> in_list;
            for (CardDefId id : list.main) {
                const CardDef& card = db.get(id);
                if (card.card_type == CardType::Unit) ++units;
                else if (card.card_type == CardType::Spell) ++spells;
                else if (card.card_type == CardType::Gear) ++gear;
                ++in_list[card.name];
            }
            unit_counts.push_back(units);
            spell_counts.push_back(spells);
            gear_counts.push_back(gear);
            for (const auto& [name, n] : in_list) {
                const CardDef* printed = db.findByName(name);
                if (printed && printed->bannedOn(kLegalityDate) && poolPrinting(name) == kInvalidId) {
                    banned_names.insert(name);
                    continue;
                }
                const CardDefId use = poolPrinting(name);
                if (use == kInvalidId || !legalHere(db.get(use))) continue;
                played[name].id = use;
                played[name].counts.push_back(n);
            }
            built.split_sources.push_back(list.display);
        }
        const SplitPlan plan = splitFrom(unit_counts, spell_counts, gear_counts);
        target = plan.used;
        built.median_units = plan.median.units;
        built.median_spells = plan.median.spells;
        built.median_gear = plan.median.gear;
        built.asked_units = plan.used.units;
        built.asked_spells = plan.used.spells;
        built.asked_gear = plan.used.gear;
    }
    built.banned_left_out.assign(banned_names.begin(), banned_names.end());

    const std::set<std::string> legend_words = wordsOf(printedRaw(*found));
    const LegendPlan legend_plan = readLegendPlan(*found);
    built.plan_name = legend_plan.name;
    built.plan_detail = legend_plan.detail;
    auto carries = [&](const CardDef& card) {
        return carriesPlan(legend_plan, card, legend_words);
    };
    auto noteName = [&](std::vector<std::string>& names, const std::string& name) {
        if (std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
    };
    auto room = [&](const CardDef& card) { return roomFor(card, copies, deck, db); };
    auto haveUnitAt = [&](int energy) {
        for (CardDefId id : deck.main_deck) {
            const CardDef& card = db.get(id);
            if (card.card_type == CardType::Unit && card.energy_cost == energy) return true;
        }
        return false;
    };

    int unit_left = target.units;
    int spell_left = target.spells;
    int gear_left = target.gear;

    auto take = [&](CardDefId id, int n, int& quota, bool filler, bool from_list) {
        if (n <= 0 || quota <= 0) return 0;
        const CardDef& card = db.get(id);
        if (!legalHere(card)) return 0;
        if (card.card_type == CardType::Spell && isChannelOrRampSpell(card)) return 0;
        n = std::min(n, std::min(room(card), quota));
        if (n <= 0) return 0;
        addCopies(deck, copies, card, n);
        quota -= n;
        if (filler) noteName(from_list ? built.curve_filler_lists : built.curve_filler_pool, card.name);
        else if (!from_list) noteName(built.plan_from_pool, card.name);
        return n;
    };

    auto betterSeen = [](const Seen& seen, const Seen* best) {
        if (!best) return true;
        if (seen.lists() != best->lists()) return seen.lists() > best->lists();
        if (seen.typical() != best->typical()) return seen.typical() > best->typical();
        return seen.id.raw() < best->id.raw();
    };

    // One copy at each early cost, from the lists. A unit that carries the plan
    // comes first. A unit that does not is the curve filler for that cost.
    if (!played.empty()) {
        for (int energy : {1, 2, 3, 4}) {
            if (unit_left <= 0 || haveUnitAt(energy)) continue;
            const Seen* best_plan = nullptr;
            const Seen* best_any = nullptr;
            for (const auto& [name, seen] : played) {
                (void)name;
                const CardDef& card = db.get(seen.id);
                if (card.card_type != CardType::Unit || card.energy_cost != energy) continue;
                if (room(card) <= 0 || seen.counts.empty()) continue;
                if (betterSeen(seen, best_any)) best_any = &seen;
                if (carries(card) && betterSeen(seen, best_plan)) best_plan = &seen;
            }
            const Seen* best = best_plan ? best_plan : best_any;
            if (best) take(best->id, 1, unit_left, best_plan == nullptr, true);
        }
    }

    // Saved-list cards that carry the plan, at the count those lists play.
    // Cards that do not carry it fill what is left, and the playbook names them.
    // The count stops at that median. A further copy would be a count no list plays.
    auto rankSaved = [&](CardType type, bool want_plan) {
        std::vector<const Seen*> ranked;
        for (const auto& [name, seen] : played) {
            (void)name;
            if (db.get(seen.id).card_type != type) continue;
            if (carries(db.get(seen.id)) != want_plan) continue;
            ranked.push_back(&seen);
        }
        std::sort(ranked.begin(), ranked.end(), [&](const Seen* a, const Seen* b) {
            if (a->lists() != b->lists()) return a->lists() > b->lists();
            if (a->typical() != b->typical()) return a->typical() > b->typical();
            const CardDef& da = db.get(a->id);
            const CardDef& db_ = db.get(b->id);
            if (da.energy_cost != db_.energy_cost) return da.energy_cost < db_.energy_cost;
            return a->id.raw() < b->id.raw();
        });
        return ranked;
    };
    auto takeRanked = [&](const std::vector<const Seen*>& ranked, int& quota, bool filler) {
        for (const Seen* seen : ranked) {
            if (quota <= 0) break;
            const CardDef& card = db.get(seen->id);
            const int already = copiesOfName(deck, db, card.name);
            const int want = std::min(seen->typical(), CopyCounts::kCopiesPerName);
            take(seen->id, want - already, quota, filler, true);
        }
    };
    auto fillSaved = [&](CardType type, int& quota) {
        if (played.empty() || quota <= 0) return;
        takeRanked(rankSaved(type, true), quota, false);
        takeRanked(rankSaved(type, false), quota, true);
    };
    fillSaved(CardType::Unit, unit_left);
    fillSaved(CardType::Spell, spell_left);
    fillSaved(CardType::Gear, gear_left);

    // Pool cards fill what the saved lists did not. A card the lists already
    // play is not raised past the count they play. Costs are spread the way a
    // played list is spread (more 1s and 2s than 5s). Inside a cost, a card
    // that shares words with the legend comes first. A channel spell, and a
    // unit or gear that says "Channel N rune(s)", is not this filler.
    auto fillFromPool = [&](CardType type, int& quota, bool plan_only) {
        if (quota <= 0) return;
        struct Band {
            int lo;
            int hi;
            int share;
        };
        std::vector<Band> bands;
        if (type == CardType::Gear) {
            bands.push_back({0, 99, quota});
        } else {
            const int share[5] = {6, 6, 3, 2, 1};
            const int edges[5][2] = {{0, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 99}};
            int given = 0;
            for (int i = 0; i < 5; ++i) {
                const int part = quota * share[i] / 18;
                bands.push_back({edges[i][0], edges[i][1], part});
                given += part;
            }
            bands[1].share += quota - given;
        }
        auto fillBand = [&](int lo, int hi, int need) {
            int got = 0;
            while (got < need && quota > 0) {
                const CardDef* best = nullptr;
                auto consider = [&](CardDefId id) {
                    const CardDef& card = db.get(id);
                    if (card.card_type != type || !legalHere(card)) return;
                    if (played.count(card.name)) return;
                    if (card.energy_cost < lo || card.energy_cost > hi) return;
                    if (type == CardType::Spell && isChannelOrRampSpell(card)) return;
                    if ((type == CardType::Unit || type == CardType::Gear) && instructsChannel(card))
                        return;
                    if (plan_only) {
                        if (!carries(card)) return;
                    } else if (carries(card)) {
                        return;
                    }
                    if (room(card) <= 0) return;
                    if (!best) {
                        best = &card;
                        return;
                    }
                    const int oa = wordOverlap(legend_words, card);
                    const int ob = wordOverlap(legend_words, *best);
                    const bool better =
                        oa > ob || (oa == ob && card.energy_cost < best->energy_cost) ||
                        (oa == ob && card.energy_cost == best->energy_cost && card.id.raw() < best->id.raw());
                    if (better) best = &card;
                };
                for (CardDefId id : pool->main) consider(id);
                for (CardDefId id : pool->signatures) consider(id);
                if (!best) break;
                const int n = std::min(need - got, std::min(quota, std::min(CopyCounts::kCopiesPerName, room(*best))));
                const int added = take(best->id, n, quota, !plan_only, false);
                if (added <= 0) break;
                got += added;
            }
            return got;
        };
        int spilled = 0;
        for (const Band& band : bands) {
            const int need = band.share + spilled;
            spilled = 0;
            const int got = fillBand(band.lo, band.hi, need);
            if (got < need) spilled = need - got;
        }
        if (quota > 0) fillBand(0, 99, quota);
    };
    auto fillPool = [&](CardType type, int& quota) {
        fillFromPool(type, quota, true);
        fillFromPool(type, quota, false);
    };
    fillPool(CardType::Unit, unit_left);
    fillPool(CardType::Spell, spell_left);
    fillPool(CardType::Gear, gear_left);

    bool shifted = false;
    if (spell_left > 0 || gear_left > 0) {
        shifted = true;
        unit_left += spell_left + gear_left;
        spell_left = 0;
        gear_left = 0;
        fillPool(CardType::Unit, unit_left);
    }
    if (unit_left > 0) {
        shifted = true;
        spell_left += unit_left;
        unit_left = 0;
        fillPool(CardType::Spell, spell_left);
    }
    if (static_cast<int>(deck.main_deck.size()) != kMainDeckCards)
        throw std::runtime_error("the legal pool for '" + found->name +
                                 "' does not fill a 39-card main deck without a channel or ramp spell");

    auto typeRank = [](CardType type) {
        if (type == CardType::Unit) return 0;
        if (type == CardType::Spell) return 1;
        if (type == CardType::Gear) return 2;
        return 3;
    };
    std::stable_sort(deck.main_deck.begin(), deck.main_deck.end(), [&](CardDefId a, CardDefId b) {
        const CardDef& da = db.get(a);
        const CardDef& db_ = db.get(b);
        if (da.card_type != db_.card_type) return typeRank(da.card_type) < typeRank(db_.card_type);
        if (da.energy_cost != db_.energy_cost) return da.energy_cost < db_.energy_cost;
        if (da.name != db_.name) return da.name < db_.name;
        return a.raw() < b.raw();
    });

    for (CardDefId id : deck.main_deck) {
        switch (db.get(id).card_type) {
            case CardType::Unit: ++built.units; break;
            case CardType::Spell: ++built.spells; break;
            case CardType::Gear: ++built.gear; break;
            default: break;
        }
    }
    if (shifted && (built.units != target.units || built.spells != target.spells || built.gear != target.gear)) {
        std::ostringstream why;
        why << "The split asked for " << target.units << " units, " << target.spells << " spells, and "
            << target.gear
            << " gear. This list differs because a card that tells a player to channel was not "
               "added to fill a type count and a banned card was not kept. The open slots went to "
               "other cards.";
        built.notes.push_back(why.str());
    }

    if (!lists.empty()) {
        std::map<std::string, int> rune_votes;
        std::map<std::string, std::vector<std::pair<std::string, int>>> rune_plans;
        for (const SavedList& list : lists) {
            std::vector<std::pair<std::string, int>> lines;
            bool ok = true;
            int total = 0;
            for (const auto& [id, n] : list.runes) {
                const CardDef& rune = db.get(id);
                if (rune.card_type != CardType::Rune || rune.bannedOn(kLegalityDate)) {
                    ok = false;
                    break;
                }
                lines.emplace_back(rune.name, n);
                total += n;
            }
            if (!ok || total != kRunes) continue;
            std::sort(lines.begin(), lines.end());
            std::ostringstream key;
            for (const auto& [name, n] : lines) key << n << " " << name << "\n";
            const std::string text = key.str();
            ++rune_votes[text];
            rune_plans[text] = lines;
        }
        std::string best_key;
        int best_votes = 0;
        for (const auto& [key, votes] : rune_votes) {
            if (votes > best_votes) {
                best_votes = votes;
                best_key = key;
            }
        }
        if (!best_key.empty()) {
            for (const auto& [name, n] : rune_plans[best_key]) {
                CardDefId id = kInvalidId;
                for (const auto& list : pool->runes) {
                    for (CardDefId rune : list) {
                        if (db.get(rune).name == name) {
                            id = rune;
                            break;
                        }
                    }
                    if (id != kInvalidId) break;
                }
                if (id == kInvalidId) {
                    deck.rune_deck.clear();
                    break;
                }
                for (int i = 0; i < n; ++i) deck.rune_deck.push_back(id);
            }
        }
    }
    if (deck.rune_deck.size() != static_cast<std::size_t>(kRunes)) {
        deck.rune_deck.clear();
        if (pool->runes.empty())
            throw std::runtime_error("'" + found->name + "' has no rune list");
        const int lists_n = static_cast<int>(pool->runes.size());
        const int base = kRunes / lists_n;
        const int extra = kRunes % lists_n;
        for (int i = 0; i < lists_n; ++i) {
            const std::vector<CardDefId>& list = pool->runes[static_cast<std::size_t>(i)];
            CardDefId rune = list.front();
            for (CardDefId id : list)
                if (id < rune) rune = id;
            const int n = base + (i < extra ? 1 : 0);
            for (int k = 0; k < n; ++k) deck.rune_deck.push_back(rune);
        }
    }

    auto legalBattlefield = [&](CardDefId id) {
        const CardDef& card = db.get(id);
        return card.card_type == CardType::Battlefield && !card.bannedOn(kLegalityDate) &&
               !card.isTokenCard() && domainsInside(card, *found);
    };
    std::map<CardDefId, int> battlefield_votes;
    for (const SavedList& list : lists) {
        std::set<CardDefId> once;
        for (CardDefId id : list.battlefields)
            if (legalBattlefield(id) && once.insert(id).second) ++battlefield_votes[id];
    }
    std::vector<CardDefId> battlefield_ranked;
    for (const auto& [id, votes] : battlefield_votes) {
        (void)votes;
        battlefield_ranked.push_back(id);
    }
    std::sort(battlefield_ranked.begin(), battlefield_ranked.end(), [&](CardDefId a, CardDefId b) {
        if (battlefield_votes[a] != battlefield_votes[b]) return battlefield_votes[a] > battlefield_votes[b];
        return a.raw() < b.raw();
    });
    std::set<std::string> battlefield_names;
    for (CardDefId id : battlefield_ranked) {
        if (battlefield_names.insert(db.get(id).name).second) deck.battlefields.push_back(id);
        if (deck.battlefields.size() == 3) break;
    }
    if (deck.battlefields.size() < 3) {
        std::vector<CardDefId> rest;
        for (const auto& [id, card] : db.all()) {
            (void)card;
            if (legalBattlefield(id)) rest.push_back(id);
        }
        std::sort(rest.begin(), rest.end());
        for (CardDefId id : rest) {
            if (battlefield_names.insert(db.get(id).name).second) deck.battlefields.push_back(id);
            if (deck.battlefields.size() == 3) break;
        }
    }
    if (deck.battlefields.size() != 3)
        throw std::runtime_error("fewer than 3 battlefields with different names for '" + found->name + "'");

    std::string sideboard_note = "Sideboard: 0.";
    if (!lists.empty()) {
        std::map<std::string, int> side_votes;
        std::map<std::string, std::vector<std::pair<CardDefId, int>>> side_plans;
        bool any_side = false;
        for (const SavedList& list : lists) {
            if (!list.sideboard.empty()) any_side = true;
            std::vector<std::pair<std::string, std::pair<CardDefId, int>>> lines;
            for (const auto& entry : list.sideboard)
                lines.push_back({db.get(entry.first).name, entry});
            std::sort(lines.begin(), lines.end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });
            std::ostringstream key;
            std::vector<std::pair<CardDefId, int>> plan;
            int total = 0;
            for (const auto& line : lines) {
                key << line.second.second << " " << line.first << "\n";
                plan.push_back(line.second);
                total += line.second.second;
            }
            if (total > static_cast<int>(kMaxSideboardSize)) continue;
            const std::string text = key.str();
            ++side_votes[text];
            side_plans[text] = plan;
        }
        const bool shared = side_votes.size() == 1 && lists.size() >= 1 &&
                            side_votes.begin()->second == static_cast<int>(lists.size()) &&
                            !side_plans.begin()->second.empty();
        if (shared) {
            const CopyCounts saved_copies = copies;
            const std::vector<CardDefId> saved_side = deck.sideboard;
            bool ok = true;
            for (const auto& [id, n] : side_plans.begin()->second) {
                const CardDef& card = db.get(id);
                if (!legalHere(card) || isChannelOrRampSpell(card) || room(card) < n) {
                    ok = false;
                    break;
                }
                for (int i = 0; i < n; ++i) {
                    deck.sideboard.push_back(id);
                    copies.add(card);
                }
            }
            if (!ok) {
                copies = saved_copies;
                deck.sideboard = saved_side;
                sideboard_note =
                    "Sideboard: 0. The saved lists share a sideboard, and it does not fit this "
                    "main deck on 20261123.";
            } else {
                sideboard_note = "Sideboard: " + std::to_string(deck.sideboard.size()) +
                                 ". Every saved list used for this split has that same sideboard, "
                                 "so it is part of the build.";
            }
        } else if (any_side) {
            sideboard_note =
                "Sideboard: 0. The saved lists have sideboards, and they are not one shared build.";
        } else {
            sideboard_note = "Sideboard: 0. The saved lists used for this split have no sideboard.";
        }
    }
    built.notes.push_back(sideboard_note);
    built.notes.push_back(
        "Battlefields: 3, with different names, each with no domain or only domains inside this "
        "legend's identity. DeckValidator checks the count, the type, and the names. It does not "
        "check Domain Identity. Saved lists choose first. The rest are the lowest card ids that "
        "still qualify.");
    if (neeko && pool_ids.count(neeko->id))
        built.notes.push_back(
            "Neeko, Blending In is in this legend's pool and is left out. When she is in a deck, "
            "DeckValidator requires exactly 3 additional legends. This builder does not choose them.");

    const DeckValidator validator(db);
    const ValidationResult result = validator.validate(deck);
    if (!result.is_legal)
        throw std::runtime_error("DeckValidator rejected the list for '" + found->name + "' on " +
                                 std::to_string(kLegalityDate) + joinErrors(result.errors));

    built.list = random.toDeckList(deck);
    if (!deck.sideboard.empty()) {
        built.list += "Sideboard:\n";
        std::vector<std::pair<std::string, int>> lines;
        for (CardDefId id : deck.sideboard) {
            std::string name = db.get(id).name;
            if (const CardDef* first = db.findByName(name); first && first->id != id)
                name += " #" + std::to_string(id.raw());
            auto it = std::find_if(lines.begin(), lines.end(),
                                   [&](const auto& line) { return line.first == name; });
            if (it == lines.end()) lines.emplace_back(name, 1);
            else ++it->second;
        }
        for (const auto& [name, n] : lines) built.list += std::to_string(n) + " " + name + "\n";
        built.list += "\n";
    }
    return built;
}

std::string formatLegendPlaybook(const CardDB& db, const CardRegistry& registry, const DeckBuild& built,
                                 int games, uint64_t seed) {
    if (games < 0 || games > 20)
        throw std::invalid_argument("mirror games must be from 0 to 20");
    if (seed == 0)
        throw std::invalid_argument("seed 0 is rejected so a printed game can be replayed");

    std::vector<MirrorGame> played;
    played.reserve(static_cast<std::size_t>(games));
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

    std::ostringstream out;
    out << "Playbook for " << built.legend_query << "\n";
    out << "Printed name: " << built.printed_name << "\n";
    if (built.alias != built.printed_name && built.alias != built.legend_query)
        out << "Deck-list name: " << built.alias << "\n";
    out << "\n";

    out << "How this list was built\n";
    out << "-----------------------\n";
    out << "Legend: " << built.legend_query << "\n";
    out << "Printed name: " << built.printed_name << "\n";
    out << "Legality date: " << built.legality_date << "\n";
    out << "Main deck: " << built.units << " units, " << built.spells << " spells, " << built.gear
        << " gear.\n";
    out << "Chosen champion: " << champion.name << ". The champion carries the legend's champion tag "
           "and is not one of those 39 cards.\n";
    if (built.split_sources.empty()) {
        out << "No saved list for this legend was found under internal/deck-catalogue or "
               "internal/tournament-data. The split stays near 18 units, 18 spells, and 3 gear.\n";
    } else if (built.median_units + built.median_spells + built.median_gear == kMainDeckCards) {
        out << "The unit, spell, and gear split is the median of these saved lists:\n";
        for (const std::string& source : built.split_sources) out << "- " << source << "\n";
    } else {
        const int median_cards = built.median_units + built.median_spells + built.median_gear;
        out << "The median of these saved lists is " << built.median_units << " units, "
            << built.median_spells << " spells, and " << built.median_gear << " gear ("
            << median_cards << (median_cards == 1 ? " card" : " cards") << ").\n";
        out << adjustmentSentence(Split{built.median_units, built.median_spells, built.median_gear},
                                  Split{built.asked_units, built.asked_spells, built.asked_gear})
            << "\n";
        out << "These are the saved lists:\n";
        for (const std::string& source : built.split_sources) out << "- " << source << "\n";
    }
    out << "A card that tells a player to channel is not added to fill a type count. A card the "
           "saved lists already play is kept.\n";
    if (!built.banned_left_out.empty()) {
        out << "Left out, banned on " << built.legality_date << ": ";
        for (std::size_t i = 0; i < built.banned_left_out.size(); ++i) {
            if (i > 0) out << ", ";
            out << built.banned_left_out[i];
        }
        out << ".\n";
    }
    out << "A keyword such as Deflect is not required of every unit. A curve hole is filled with a "
           "unit the saved lists play when they play one.\n";
    out << "DeckValidator::validate accepted this list on " << built.legality_date << ".\n";
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
    const std::string legend_text = printedRaw(legend);
    if (built.plan_name.empty()) out << "Plan: The printed line.\n";
    else out << "Plan: " << built.plan_name << ".\n";
    out << "Printed on " << legend.name << ": ";
    if (legend_text.empty()) out << "This card has no printed ability text.\n";
    else out << legend_text << "\n";
    if (!built.plan_detail.empty()) out << built.plan_detail << "\n";
    const std::vector<NamedCopy> main_copies = copiesInMain(built.deck, db);
    std::set<std::string> aside;
    for (const std::string& name : built.curve_filler_lists) aside.insert(name);
    for (const std::string& name : built.curve_filler_pool) aside.insert(name);
    for (const std::string& name : built.plan_from_pool) aside.insert(name);
    std::vector<std::string> list_carrier_names;
    for (const NamedCopy& line : main_copies)
        if (!aside.count(line.name)) list_carrier_names.push_back(line.name);
    const std::vector<NamedCopy> list_carriers = pickNamed(main_copies, list_carrier_names);
    const std::vector<NamedCopy> pool_carriers = pickNamed(main_copies, built.plan_from_pool);
    if (list_carriers.empty() && pool_carriers.empty()) {
        out << "Carries this plan: no card in this list.\n";
    } else if (!list_carriers.empty()) {
        out << "Carries this plan: " << joinNamed(list_carriers) << ".\n";
        if (!pool_carriers.empty()) {
            out << "Also from the card pool, because the saved lists do not fill 39 with cards that "
                   "carry this plan: "
                << joinNamed(pool_carriers) << ".\n";
        }
    } else {
        out << "Carries this plan: " << joinNamed(pool_carriers) << ".\n";
    }
    std::vector<NamedCopy> early;
    for (const NamedCopy& line : main_copies)
        if (std::max(line.energy, line.power) <= 2) early.push_back(line);
    if (early.empty()) {
        const NamedCopy* cheapest = nullptr;
        for (const NamedCopy& line : main_copies) {
            if (aside.count(line.name)) continue;
            if (!cheapest || std::max(line.energy, line.power) < std::max(cheapest->energy, cheapest->power))
                cheapest = &line;
        }
        if (!cheapest && !main_copies.empty()) cheapest = &main_copies.front();
        out << "Early turns pay ";
        if (cheapest)
            out << cheapest->copies << " " << cheapest->name << " (" << costPhrase(*cheapest) << ")";
        else
            out << "nothing in this list";
        out << ". Going first, 2 runes are ready on turn 1 and 4 on turn 2. A cost is the greater of "
               "the card's energy and its power.\n";
    } else {
        out << "Early turns pay " << joinNamed(early)
            << ". Going first, 2 runes are ready on turn 1 and 4 on turn 2. A cost is the greater of "
               "the card's energy and its power.\n";
    }
    const std::vector<NamedCopy> list_filler = pickNamed(main_copies, built.curve_filler_lists);
    const std::vector<NamedCopy> pool_filler = pickNamed(main_copies, built.curve_filler_pool);
    if (list_filler.empty() && pool_filler.empty()) {
        out << "No slot is curve filler.\n";
    } else {
        if (!list_filler.empty()) {
            out << "Curve filler: " << joinNamed(list_filler)
                << ". The saved lists play these cards. These slots are outside the plan.\n";
        }
        if (!pool_filler.empty()) {
            out << "Curve filler from the card pool: " << joinNamed(pool_filler) << ". ";
            if (built.split_sources.empty())
                out << "No saved list was found, so the card pool fills these slots.\n";
            else
                out << "The saved lists had no further card that could fill these slots on "
                    << built.legality_date << ".\n";
        }
    }
    out << "\nPrinted on the chosen champion " << champion.name << ": ";
    const std::string champion_text = printedRaw(champion);
    if (champion_text.empty()) out << "This card has no printed ability text.\n";
    else out << champion_text << "\n";
    out << "\n";

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
