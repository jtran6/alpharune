/// @file deck_breakdown.cpp
/// Roles and draw chances for one deck list.
///
/// Costs and words come from the CardDef the engine already has. The draw
/// pile is the main deck. The champion starts in the Champion Zone (CR 112),
/// so it is classified and not drawn. Battlefields, runes, and the sideboard
/// stay out of both the roles and the chances.
///
/// With no draw, Predict, recycle from the deck, or tutor on the main deck or
/// the champion, each chance is the plain hypergeometric draw: pile size,
/// copies, cards seen. Those effects change it. Each copy among the cards
/// already seen shows its printed extra cards once; an effect found only in
/// those extra cards is not followed. A champion's extra cards are counted
/// from the first turn, not in the opening hand. A recycle from a trash is
/// not cards seen in the main deck. That is a count, not a played game.

#include "rules/deck_breakdown.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <map>
#include <sstream>
#include <utility>

namespace riftbound {
namespace {

constexpr int kOpeningHand = 4;  // CR 116
constexpr int kFirstTurns = 3;   // CR 315.4, one draw on each of your turns

std::string lowerCopy(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

/// Reminder text in parentheses is the rules gloss, not an extra instruction.
std::string stripParentheticals(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    int depth = 0;
    for (char c : s) {
        if (c == '(') { ++depth; continue; }
        if (c == ')' && depth > 0) { --depth; continue; }
        if (depth == 0) out.push_back(c);
    }
    return out;
}

std::string printedText(const CardDef& card) {
    std::string raw = card.ability_text;
    if (!card.effect_text.empty()) {
        if (!raw.empty()) raw.push_back('\n');
        raw += card.effect_text;
    }
    return lowerCopy(stripParentheticals(raw));
}

bool boundary(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) == 0;
}

/// `stem` as a word, or as the start of one ("rune" matches "runes").
bool hasStem(const std::string& text, const char* stem) {
    const size_t n = std::char_traits<char>::length(stem);
    for (size_t i = 0; i + n <= text.size(); ++i) {
        if (text.compare(i, n, stem) != 0) continue;
        if (i > 0 && !boundary(text[i - 1])) continue;
        return true;
    }
    return false;
}

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

size_t skipSpace(const std::string& text, size_t i) {
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    return i;
}

/// Digits after `phrase`, skipping a plural suffix and "up to". 0 when none.
int numberAfter(const std::string& text, const char* phrase) {
    const size_t n = std::char_traits<char>::length(phrase);
    int best = 0;
    for (size_t i = 0; i + n <= text.size(); ++i) {
        if (text.compare(i, n, phrase) != 0) continue;
        if (i > 0 && !boundary(text[i - 1])) continue;
        size_t j = i + n;
        while (j < text.size() && std::isalpha(static_cast<unsigned char>(text[j]))) ++j;
        j = skipSpace(text, j);
        if (j + 5 <= text.size() && text.compare(j, 5, "up to") == 0 &&
            (j + 5 == text.size() || boundary(text[j + 5]))) {
            j = skipSpace(text, j + 5);
        }
        if (j >= text.size() || !std::isdigit(static_cast<unsigned char>(text[j]))) continue;
        int value = 0;
        while (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j]))) {
            value = value * 10 + (text[j] - '0');
            ++j;
        }
        if (value > best) best = value;
    }
    return best;
}

bool nextWordIs(const std::string& text, size_t i, const char* word) {
    i = skipSpace(text, i);
    const size_t n = std::char_traits<char>::length(word);
    if (i + n > text.size() || text.compare(i, n, word) != 0) return false;
    return i + n == text.size() || boundary(text[i + n]);
}

bool containsPhrase(const std::string& text, const char* phrase) {
    return text.find(phrase) != std::string::npos;
}

// What a printed draw, Predict, recycle, or tutor contributes once it is seen.
struct Selection {
    int dig = 0;             // extra cards counted as seen
    bool finds_any = false;  // searches the deck for a card
    bool is_draw = false;
    bool is_predict = false;
    bool is_recycle = false;
    bool is_tutor = false;
    std::string summary;     // what this card was counted as
};

int capDig(int n) { return std::min(n, 40); }

Selection readSelection(const CardDef& card) {
    const std::string text = printedText(card);
    Selection s;

    int look = numberAfter(text, "look at the top");
    look = std::max(look, numberAfter(text, "reveal the top"));
    if (look == 0 && (containsPhrase(text, "look at the top card") ||
                      containsPhrase(text, "reveal the top card")))
        look = 1;
    if (containsPhrase(text, "top") && numberAfter(text, "the top") > look)
        look = numberAfter(text, "the top");

    int predict = 0;
    if (hasWord(text, "predict")) {
        predict = numberAfter(text, "predict");
        if (predict == 0) predict = 1;
        s.is_predict = true;
    }

    int blind_draw = 0;
    int from_look = 0;
    for (size_t i = 0; i + 4 <= text.size(); ++i) {
        if (text.compare(i, 4, "draw") != 0) continue;
        if (i > 0 && !boundary(text[i - 1])) continue;
        // [Quick-Draw] is a keyword. The hyphen is a boundary, so the four
        // letters are not the verb that takes a card.
        if (i >= 6 && text.compare(i - 6, 6, "quick-") == 0) continue;
        // "draws" is the same verb. "drawer" is not.
        size_t end = i + 4;
        if (end < text.size() && text[end] == 's') ++end;
        if (end < text.size() && !boundary(text[end])) continue;
        const size_t after = end;
        size_t digits = skipSpace(text, after);
        if (digits + 5 <= text.size() && text.compare(digits, 5, "up to") == 0 &&
            (digits + 5 == text.size() || boundary(text[digits + 5])))
            digits = skipSpace(text, digits + 5);
        if (digits < text.size() && std::isdigit(static_cast<unsigned char>(text[digits]))) {
            int value = 0;
            while (digits < text.size() && std::isdigit(static_cast<unsigned char>(text[digits]))) {
                value = value * 10 + (text[digits] - '0');
                ++digits;
            }
            blind_draw += std::max(value, 1);
            continue;
        }
        if (nextWordIs(text, after, "it") || nextWordIs(text, after, "any") ||
            nextWordIs(text, after, "them") || nextWordIs(text, after, "those")) {
            from_look += 1;
            continue;
        }
        // "draw a card" / "draw one" is one card. "draw one of them" takes from a look.
        if (nextWordIs(text, after, "a") || nextWordIs(text, after, "an") || nextWordIs(text, after, "one")) {
            size_t word = skipSpace(text, after);
            while (word < text.size() && !boundary(text[word])) ++word;
            if (nextWordIs(text, word, "of")) from_look += 1;
            else blind_draw += 1;
            continue;
        }
        blind_draw += 1;
    }

    if (hasStem(text, "search") && hasStem(text, "deck")) {
        s.finds_any = true;
        s.is_tutor = true;
    }
    // "Reveal cards from the top ... until you reveal a unit" has no printed
    // count. It still looks at the deck, so it counts as one extra card.
    bool through_the_top = false;
    if (look == 0 && (containsPhrase(text, "from the top") || containsPhrase(text, "until you reveal"))) {
        look = 1;
        through_the_top = true;
        s.is_tutor = true;
    }
    if (look > 0) s.is_tutor = true;

    int recycle_n = 0;
    bool recycle_from_trash = false;
    if (hasStem(text, "recycle")) {
        s.is_recycle = true;
        const bool already_seen = containsPhrase(text, "recycle the rest") ||
                                  containsPhrase(text, "recycle them") ||
                                  containsPhrase(text, "then recycle");
        // "Recycle N from trashes" puts cards back. Those cards are not in
        // the main deck, so they are not an extra look at the pile.
        recycle_from_trash = containsPhrase(text, "trash");
        if (!already_seen && !recycle_from_trash) {
            recycle_n = numberAfter(text, "recycle");
            if (recycle_n == 0) recycle_n = 1;
        }
    }

    if (card.keywords.has(Keyword::Vision)) {
        s.is_recycle = true;
        s.is_tutor = true;
        if (look < 1) look = 1;
    }

    int dig = 0;
    if (look > 0) dig += look;
    if (predict > 0) dig += predict;
    dig += blind_draw;
    // "Draw it" takes one of the cards the look already counted.
    if (look == 0 && predict == 0) dig += from_look;
    if (look == 0 && predict == 0) dig += recycle_n;
    // A recycle from the deck that was not already part of a counted look
    // still changes the pile. A trash is a different zone.
    if (s.is_recycle && dig == 0 && !s.finds_any && !recycle_from_trash)
        dig += recycle_n > 0 ? recycle_n : 1;

    s.dig = capDig(dig);
    s.is_draw = blind_draw + from_look > 0;
    if (s.finds_any) s.is_tutor = true;

    std::vector<std::string> parts;
    if (blind_draw > 0) parts.push_back("draws " + std::to_string(blind_draw));
    if (from_look > 0 && look == 0 && predict == 0)
        parts.push_back("draws " + std::to_string(from_look) + " of the cards it sees");
    if (s.is_predict) parts.push_back("Predict " + std::to_string(predict));
    if (through_the_top) parts.push_back("looks through the top of the deck");
    else if (look > 0) parts.push_back("looks at " + std::to_string(look));
    if (s.finds_any) parts.push_back("searches the deck");
    if (s.is_recycle && recycle_n > 0) parts.push_back("recycles " + std::to_string(recycle_n));
    else if (s.is_recycle) parts.push_back("recycles");
    if (card.keywords.has(Keyword::Vision) && look <= 1 && predict == 0 && blind_draw == 0)
        parts.push_back("Vision");
    if (!parts.empty()) {
        std::string joined;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i) joined += ", ";
            joined += parts[i];
        }
        if (s.finds_any)
            joined += "; counted as finding one card still in the pile";
        else if (s.dig > 0)
            joined += "; counted as " + std::to_string(s.dig) + " extra card" + (s.dig == 1 ? "" : "s");
        s.summary = joined;
    }
    return s;
}

bool negatedDeal(const std::string& text, size_t at) {
    const size_t begin = at > 12 ? at - 12 : 0;
    const std::string before = text.substr(begin, at - begin);
    return before.find("n't") != std::string::npos || before.find(" not ") != std::string::npos ||
           before.rfind("not", 0) == 0;
}

bool dealsDamage(const std::string& text) {
    const char* word = "deal";
    for (size_t i = 0; i + 4 <= text.size(); ++i) {
        if (text.compare(i, 4, word) != 0) continue;
        if (i > 0 && !boundary(text[i - 1])) continue;
        // "deals" is the same verb. "dealer" is not.
        size_t j = i + 4;
        if (j < text.size() && text[j] == 's') ++j;
        if (j < text.size() && !boundary(text[j])) continue;
        if (negatedDeal(text, i)) continue;
        return true;
    }
    return false;
}

/// [Shield 2] / [Assault] / [Disarm] stay after the parenthetical "+N [M]"
/// reminder is stripped. That reminder is not the only place the keyword lives.
bool printedFightMight(const std::string& text) {
    return containsPhrase(text, "[shield") || containsPhrase(text, "[assault") ||
           containsPhrase(text, "[disarm");
}

bool changesMight(const CardDef& card, const std::string& text) {
    if (card.keywords.has(Keyword::Assault) || card.keywords.has(Keyword::Shield) ||
        card.keywords.has(Keyword::Disarm) || printedFightMight(text))
        return true;
    if (card.might_bonus != 0) return true;
    if (hasWord(text, "buff")) return true;
    // "+2 [M]" / "-4 [m]". A token's size ("1 [M] Recruit") is not a change.
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

bool readiesUnit(const std::string& text) {
    const char* word = "ready";
    for (size_t i = 0; i + 5 <= text.size(); ++i) {
        if (text.compare(i, 5, word) != 0) continue;
        if (i > 0 && !boundary(text[i - 1])) continue;
        if (i + 5 < text.size() && !boundary(text[i + 5])) continue;
        const std::string after = text.substr(i + 5, 48);
        const bool unit = containsPhrase(after, "unit") || containsPhrase(after, " me") ||
                          after.compare(0, 3, " me") == 0 || containsPhrase(after, "another") ||
                          containsPhrase(after, " it") || containsPhrase(after, "them") ||
                          containsPhrase(after, "friendly");
        const bool only_runes = containsPhrase(after, "rune") && !containsPhrase(after, "unit") &&
                                !containsPhrase(after, "another") && !containsPhrase(after, " me");
        if (unit && !only_runes) return true;
    }
    return false;
}

bool duringFight(const CardDef& card, const std::string& text) {
    if (card.keywords.has(Keyword::Action) || card.keywords.has(Keyword::Reaction) ||
        card.keywords.has(Keyword::Assault) || card.keywords.has(Keyword::Shield) ||
        card.keywords.has(Keyword::Disarm) || printedFightMight(text))
        return true;
    return hasStem(text, "attack") || hasStem(text, "defend") || hasStem(text, "combat") ||
           hasStem(text, "showdown") || hasStem(text, "conquer");
}

bool isCombatTrick(const CardDef& card, const std::string& text) {
    const bool spell_or_ability = card.card_type == CardType::Spell || !card.ability_text.empty() ||
                                  !card.effect_text.empty() || card.keywords.has(Keyword::Assault) ||
                                  card.keywords.has(Keyword::Shield) || card.keywords.has(Keyword::Disarm);
    if (!spell_or_ability || !duringFight(card, text)) return false;
    return changesMight(card, text) || dealsDamage(text) || readiesUnit(text);
}

bool isRamp(const std::string& text) {
    // [Add] is the printed energy or power symbol. The words "add resources"
    // live in the reminder, which is already stripped, so the brackets are
    // what the card still says.
    if (containsPhrase(text, "[add]")) return true;
    const bool cost = hasWord(text, "cost") || hasWord(text, "costs");
    if (cost && hasWord(text, "less")) return true;
    if (cost && containsPhrase(text, "reduc")) return true;
    if (cost && containsPhrase(text, "ignoring")) return true;
    if (containsPhrase(text, "without paying")) return true;
    if (hasWord(text, "channel") && hasStem(text, "rune")) return true;
    const bool adds = hasWord(text, "add") || hasWord(text, "adds") || hasWord(text, "gain") ||
                      hasWord(text, "gains") || hasWord(text, "extra");
    if (adds && hasWord(text, "energy")) return true;
    if (adds && hasWord(text, "power") && !containsPhrase(text, "power cost")) return true;
    if ((hasWord(text, "add") || hasWord(text, "adds") || containsPhrase(text, "additional rune") ||
         containsPhrase(text, "play a rune")) &&
        hasStem(text, "rune") && !hasWord(text, "channel"))
        return true;
    return false;
}

struct CardRow {
    const CardDef* def = nullptr;
    int main = 0;
    int champion = 0;
    Selection selection;
    std::string text;
};

struct Marks {
    bool two_drop = false;
    bool late = false;
    bool trick = false;
    bool ramp = false;
    bool draw = false;
    bool predict = false;
    bool recycle = false;
    bool tutor = false;
    bool removal = false;
    bool token = false;
    bool move = false;
    bool stun = false;
    bool might = false;
    bool ready = false;
    bool vision = false;
    bool hidden = false;
    bool ambush = false;
    bool equipment = false;
};

Marks markCard(const CardRow& row, int late_cost) {
    const CardDef& card = *row.def;
    const std::string& text = row.text;
    Marks m;
    m.two_drop = card.energy_cost == 2;
    m.late = card.energy_cost == late_cost;
    m.trick = isCombatTrick(card, text);
    m.ramp = isRamp(text);
    m.draw = row.selection.is_draw;
    m.predict = row.selection.is_predict;
    m.recycle = row.selection.is_recycle;
    m.tutor = row.selection.is_tutor;
    m.removal = hasWord(text, "kill") || hasWord(text, "kills") || hasStem(text, "banish");
    m.token = hasWord(text, "token") || hasWord(text, "tokens");
    m.move = hasWord(text, "move") || hasWord(text, "moves");
    m.stun = hasStem(text, "stun");
    m.might = changesMight(card, text);
    m.ready = readiesUnit(text);
    m.vision = card.keywords.has(Keyword::Vision);
    m.hidden = card.keywords.has(Keyword::Hidden);
    m.ambush = card.keywords.has(Keyword::Ambush);
    m.equipment = card.card_type == CardType::Gear || card.keywords.has(Keyword::Equip);
    return m;
}

uint64_t binom(int n, int k) {
    if (k < 0 || n < 0 || k > n) return 0;
    if (k == 0 || k == n) return 1;
    if (k > n - k) k = n - k;
    uint64_t r = 1;
    for (int i = 1; i <= k; ++i)
        r = r * static_cast<uint64_t>(n - k + i) / static_cast<uint64_t>(i);
    return r;
}

struct DigGroup {
    int copies = 0;
    int dig = 0;
    bool finds = false;
};

double missAfterExtra(int non_hits_left, int remain, int extra) {
    if (extra > remain) extra = remain;
    if (extra <= 0) return 1.0;
    const uint64_t denom = binom(remain, extra);
    if (denom == 0) return 0.0;
    return static_cast<double>(binom(non_hits_left, extra)) / static_cast<double>(denom);
}

/// Chance of at least one of `hits` copies in a pile of `pile` when `natural`
/// cards are drawn, then one wave of extra cards from the non-hits in that
/// draw (`groups`) and from the champion (`champion_dig` / `champion_finds`).
double chanceAtLeast(int pile, int hits, int natural, const std::vector<DigGroup>& groups,
                     int champion_dig, bool champion_finds) {
    if (hits <= 0 || pile <= 0 || natural <= 0) return 0.0;
    if (natural > pile) natural = pile;
    const int others = pile - hits;
    if (natural > others) return 1.0;

    const double natural_miss =
        static_cast<double>(binom(others, natural)) / static_cast<double>(binom(pile, natural));

    const double ways_total = static_cast<double>(binom(others, natural));
    double weighted_miss = 0.0;
    std::function<void(size_t, int, int, bool, double)> walk;
    walk = [&](size_t index, int taken, int dig, bool finds, double ways) {
        if (index == groups.size()) {
            if (taken != natural) return;
            if (finds || champion_finds) return;
            const int extra = dig + champion_dig;
            weighted_miss += ways * missAfterExtra(others - natural, pile - natural, extra);
            return;
        }
        const DigGroup& group = groups[index];
        const int max_take = std::min(group.copies, natural - taken);
        for (int take = 0; take <= max_take; ++take) {
            walk(index + 1, taken + take, dig + take * group.dig,
                 finds || (group.finds && take > 0),
                 ways * static_cast<double>(binom(group.copies, take)));
        }
    };
    walk(0, 0, 0, false, 1.0);
    const double extra_miss = ways_total == 0.0 ? 1.0 : weighted_miss / ways_total;
    const double miss = natural_miss * extra_miss;
    if (miss <= 0.0) return 1.0;
    if (miss >= 1.0) return 0.0;
    return 1.0 - miss;
}

std::vector<DigGroup> digGroups(const std::vector<CardRow>& rows, const std::vector<CardDefId>& hits) {
    auto is_hit = [&](CardDefId id) {
        return std::find(hits.begin(), hits.end(), id) != hits.end();
    };
    std::map<std::pair<int, bool>, int> by_kind;
    int blanks = 0;
    for (const CardRow& row : rows) {
        if (row.main <= 0 || is_hit(row.def->id)) continue;
        if (row.selection.dig > 0 || row.selection.finds_any)
            by_kind[{row.selection.dig, row.selection.finds_any}] += row.main;
        else
            blanks += row.main;
    }
    std::vector<DigGroup> groups;
    if (blanks > 0) groups.push_back(DigGroup{blanks, 0, false});
    for (const auto& [kind, copies] : by_kind)
        groups.push_back(DigGroup{copies, kind.first, kind.second});
    return groups;
}

std::string displayName(const CardDef& card, const std::vector<CardRow>& rows) {
    int same = 0;
    for (const CardRow& row : rows)
        if (row.def->name == card.name) ++same;
    if (same <= 1) return card.name;
    return card.name + " #" + std::to_string(card.id.raw());
}

void addCount(std::vector<DeckCardLine>& lines, const CardDef& card, int copies) {
    for (DeckCardLine& line : lines) {
        if (line.id == card.id) {
            line.copies += copies;
            return;
        }
    }
    DeckCardLine line;
    line.id = card.id;
    line.name = card.name;
    line.copies = copies;
    lines.push_back(std::move(line));
}

std::string percent(double p) {
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(1);
    out << (100.0 * p) << '%';
    return out.str();
}

struct RoleSpec {
    const char* title;
    const char* reason;
    bool Marks::* flag;
};

}  // namespace

DeckReport analyzeDeck(const DeckSubmission& deck, const CardDB& db) {
    DeckReport report;
    if (deck.legend != kInvalidId && db.all().count(deck.legend))
        report.legend = db.get(deck.legend).name;
    if (deck.chosen_champion != kInvalidId && db.all().count(deck.chosen_champion))
        report.champion = db.get(deck.chosen_champion).name;

    std::map<CardDefId, CardRow> by_id;
    auto touch = [&](CardDefId id) -> CardRow& {
        CardRow& row = by_id[id];
        if (!row.def) row.def = &db.get(id);
        return row;
    };
    for (CardDefId id : deck.main_deck) ++touch(id).main;
    if (deck.chosen_champion != kInvalidId) ++touch(deck.chosen_champion).champion;

    std::vector<CardRow> rows;
    rows.reserve(by_id.size());
    int late_cost = 0;
    for (auto& [id, row] : by_id) {
        (void)id;
        row.text = printedText(*row.def);
        row.selection = readSelection(*row.def);
        late_cost = std::max(late_cost, row.def->energy_cost);
        rows.push_back(row);
    }
    std::sort(rows.begin(), rows.end(), [](const CardRow& a, const CardRow& b) {
        if (a.def->name != b.def->name) return a.def->name < b.def->name;
        return a.def->id.raw() < b.def->id.raw();
    });
    report.late_game_cost = late_cost;
    report.draw_pile = static_cast<int>(deck.main_deck.size());

    std::map<int, int> curve;
    for (const CardRow& row : rows)
        curve[row.def->energy_cost] += row.main + row.champion;
    for (const auto& [cost, copies] : curve) report.curve.emplace_back(cost, copies);

    const RoleSpec specs[] = {
        {"2-drops", "energy cost 2", &Marks::two_drop},
        {"Late game", "", &Marks::late},
        {"Combat tricks",
         "a spell or ability that changes Might, deals damage, or readies a unit during a fight",
         &Marks::trick},
        {"Ramp", "adds energy, power, or runes, or reduces a cost", &Marks::ramp},
        {"Card draw", "draws a card", &Marks::draw},
        {"Predict", "Predict", &Marks::predict},
        {"Recycle", "recycles", &Marks::recycle},
        {"Tutors", "looks at the deck and takes a card, or searches it", &Marks::tutor},
        {"Removal", "kills or banishes", &Marks::removal},
        {"Tokens", "makes a token", &Marks::token},
        {"Movement", "moves a unit or a gear", &Marks::move},
        {"Stun", "stuns", &Marks::stun},
        {"Might", "changes Might", &Marks::might},
        {"Ready", "readies a unit", &Marks::ready},
        {"Vision", "the Vision keyword", &Marks::vision},
        {"Hidden", "the Hidden keyword", &Marks::hidden},
        {"Ambush", "the Ambush keyword", &Marks::ambush},
        {"Equipment", "a gear, or Equip", &Marks::equipment},
    };

    std::vector<Marks> marks;
    marks.reserve(rows.size());
    for (const CardRow& row : rows) marks.push_back(markCard(row, late_cost));

    for (const RoleSpec& spec : specs) {
        DeckRole role;
        role.title = spec.title;
        if (spec.flag == &Marks::late) {
            role.reason = "energy cost " + std::to_string(late_cost) + ", the top of this list";
        } else {
            role.reason = spec.reason;
        }
        for (size_t i = 0; i < rows.size(); ++i) {
            if (!(marks[i].*spec.flag)) continue;
            DeckCardLine line;
            line.id = rows[i].def->id;
            line.name = displayName(*rows[i].def, rows);
            line.copies = rows[i].main + rows[i].champion;
            line.champion = rows[i].champion > 0;
            role.copies += line.copies;
            role.draw_copies += rows[i].main;
            role.cards.push_back(std::move(line));
        }
        if (!role.cards.empty()) report.roles.push_back(std::move(role));
    }

    for (CardDefId id : deck.battlefields) addCount(report.battlefields, db.get(id), 1);
    for (CardDefId id : deck.rune_deck) addCount(report.runes, db.get(id), 1);
    for (CardDefId id : deck.sideboard) addCount(report.sideboard, db.get(id), 1);
    auto by_name = [](const DeckCardLine& a, const DeckCardLine& b) {
        if (a.name != b.name) return a.name < b.name;
        return a.id.raw() < b.id.raw();
    };
    std::sort(report.battlefields.begin(), report.battlefields.end(), by_name);
    std::sort(report.runes.begin(), report.runes.end(), by_name);
    std::sort(report.sideboard.begin(), report.sideboard.end(), by_name);

    int champion_dig = 0;
    bool champion_finds = false;
    std::vector<std::string> effects;
    bool any_effect = false;
    for (const CardRow& row : rows) {
        const bool affects = row.selection.dig > 0 || row.selection.finds_any;
        if (!affects) continue;
        any_effect = true;
        if (row.champion > 0) {
            champion_dig += row.selection.dig;
            champion_finds = champion_finds || row.selection.finds_any;
        }
        if (row.main > 0) {
            effects.push_back(std::to_string(row.main) + " " + displayName(*row.def, rows) + " " +
                              row.selection.summary);
        }
        if (row.champion > 0) {
            effects.push_back(displayName(*row.def, rows) + " (champion) " + row.selection.summary);
        }
    }
    report.plain_draw = !any_effect;

    std::ostringstream counted;
    counted << "Counted the main deck (" << report.draw_pile << " cards) as the draw pile. ";
    if (report.champion.empty())
        counted << "No champion is in the list. ";
    else
        counted << report.champion << " starts in the Champion Zone and is not drawn. ";
    const int opening = std::min(kOpeningHand, report.draw_pile);
    counted << "The opening hand is " << opening
            << (opening == 1 ? " card" : " cards")
            << " (CR 116). A mulligan sets aside up to 2 and draws that many back, so the hand "
               "stays this size and is not an extra look (CR 117). ";
    counted << "The first " << kFirstTurns
            << " turns each draw 1 more (CR 315.4). ";
    if (report.plain_draw) {
        counted << "No draw, Predict, recycle, or tutor is printed on the main deck or the "
                   "champion. Battlefields, runes, and the sideboard are not part of the chance, "
                   "so one printed only there does not move it. This is the plain draw: the pile "
                   "size and the copies decide it, and the other cards do not. ";
    } else {
        counted << "A draw, Predict, recycle, or tutor in the main deck or on the champion "
                   "changes the chance. Counted once, from cards already seen (one found only "
                   "in the extra cards is not counted again): ";
        for (size_t i = 0; i < effects.size(); ++i) {
            if (i) counted << "; ";
            counted << effects[i];
        }
        counted << ". ";
        if (champion_dig > 0 || champion_finds)
            counted << "The champion's effect is counted from the first turn, not in the "
                       "opening hand. ";
        counted << "Battlefields, runes, and the sideboard are not counted. ";
    }
    counted << "This does not play a game.";
    report.counted = counted.str();

    std::vector<CardDefId> pile_ids;
    for (const CardRow& row : rows)
        if (row.main > 0) pile_ids.push_back(row.def->id);

    auto stage_at = [&](const std::string& name, int natural, bool count_champion) {
        DeckStage stage;
        stage.name = name;
        stage.natural = std::min(natural, report.draw_pile);
        // The opening hand is dealt before the champion is played, so its
        // printed dig is not cards already seen in that hand.
        const int dig = count_champion ? champion_dig : 0;
        const bool finds = count_champion && champion_finds;
        for (const DeckRole& role : report.roles) {
            std::vector<CardDefId> hits;
            for (const DeckCardLine& line : role.cards) {
                const auto found = by_id.find(line.id);
                if (found != by_id.end() && found->second.main > 0) hits.push_back(line.id);
            }
            const auto groups = digGroups(rows, hits);
            const double p = chanceAtLeast(report.draw_pile, role.draw_copies, stage.natural, groups,
                                            dig, finds);
            stage.roles.emplace_back(role.title, p);
        }
        for (CardDefId id : pile_ids) {
            const CardRow& row = by_id[id];
            const auto groups = digGroups(rows, {id});
            const double p = chanceAtLeast(report.draw_pile, row.main, stage.natural, groups,
                                            dig, finds);
            stage.cards.emplace_back(displayName(*row.def, rows), p);
        }
        std::sort(stage.cards.begin(), stage.cards.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        report.stages.push_back(std::move(stage));
    };

    if (report.draw_pile > 0) {
        stage_at("Opening hand", kOpeningHand, false);
        for (int turn = 1; turn <= kFirstTurns; ++turn)
            stage_at("Turn " + std::to_string(turn), kOpeningHand + turn, true);
    }
    return report;
}

std::string formatDeckReport(const DeckReport& report) {
    std::ostringstream out;
    out << "Deck breakdown\n\n";
    out << "Legend: " << (report.legend.empty() ? "(none)" : report.legend) << "\n";
    out << "Champion: " << (report.champion.empty() ? "(none)" : report.champion)
        << " (Champion Zone, not drawn)\n";
    out << "Draw pile: " << report.draw_pile << (report.draw_pile == 1 ? " card\n" : " cards\n");

    auto section = [&](const char* title, const std::vector<DeckCardLine>& lines) {
        out << "\n" << title << "\n";
        if (lines.empty()) {
            out << "  (none)\n";
            return;
        }
        for (const DeckCardLine& line : lines)
            out << "  " << line.copies << " " << line.name << "\n";
    };
    section("Battlefields:", report.battlefields);
    section("Runes:", report.runes);
    out << "\nSideboard (recorded, not part of the draw chance):\n";
    if (report.sideboard.empty()) out << "  (none)\n";
    for (const DeckCardLine& line : report.sideboard)
        out << "  " << line.copies << " " << line.name << "\n";

    out << "\nCurve, main deck and champion, by energy cost:";
    if (report.curve.empty()) out << " (none)";
    for (const auto& [cost, copies] : report.curve)
        out << "  " << cost << " x" << copies;
    out << "\n";
    out << "Late game is energy cost " << report.late_game_cost
        << ", the highest cost in this list.\n";

    out << "\nRoles\n";
    if (report.roles.empty()) out << "(none)\n";
    for (const DeckRole& role : report.roles) {
        out << role.title << " — " << role.reason << " — " << role.copies
            << (role.copies == 1 ? " card\n" : " cards\n");
        for (const DeckCardLine& line : role.cards) {
            out << "  " << line.copies << " " << line.name;
            if (line.champion) out << " (champion)";
            out << "\n";
        }
    }

    out << "\nDraw chances\n";
    out << report.counted << "\n";
    if (!report.champion.empty())
        out << report.champion << " is not given a draw chance.\n";
    for (const DeckStage& stage : report.stages) {
        out << "\n" << stage.name << " — " << stage.natural
            << (stage.natural == 1 ? " card drawn\n" : " cards drawn\n");
        out << "  Roles\n";
        for (const auto& [title, p] : stage.roles) {
            const DeckRole* role = nullptr;
            for (const DeckRole& candidate : report.roles)
                if (candidate.title == title) role = &candidate;
            out << "    " << title;
            if (role && role->copies > 0 && role->draw_copies == 0) {
                out << ": not in the draw pile\n";
                continue;
            }
            if (role && role->draw_copies != role->copies)
                out << " (" << role->draw_copies << " in the pile)";
            out << ": " << percent(p) << "\n";
        }
        out << "  Cards\n";
        for (const auto& [name, p] : stage.cards)
            out << "    " << name << ": " << percent(p) << "\n";
    }
    return out.str();
}

}  // namespace riftbound
