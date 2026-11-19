#include "agents/friendly_harm.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_object.h"
#include "core/game_state.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace riftbound {
namespace {

struct SpellShape {
    int damage = 0;
    int might_delta = 0;
    int might_floor = -1;
    bool bounce = false;
    bool banishes = false;
    bool kills = false;
    bool wants_friendly = false;
    bool additional_cost = false;
    bool at_battlefield = false;
    // "3 [M] or less": a unit above this is not a legal target.
    int max_might = -1;
    // The spell still does something when the friendly harm does not kill
    // (Stupefy's draw, Eclipse's Predict).
    bool rider = false;
    // "Banish a unit, then its owner plays it": the replay is the line.
    bool replays = false;
    // "Give a unit +N [M]": any unit is a legal target. A friendly one is
    // the line that is scored while one is legal.
    bool buffs_any_unit = false;

    bool harms() const {
        return damage > 0 || might_delta < 0 || bounce || banishes || kills;
    }
};

std::string stripped(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    int depth = 0;
    for (unsigned char c : text) {
        if (c == '(') { ++depth; continue; }
        if (c == ')' && depth > 0) { --depth; continue; }
        if (depth == 0) out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

bool atWord(const std::string& text, size_t i) {
    return i == 0 || !std::isalpha(static_cast<unsigned char>(text[i - 1]));
}

bool hasWord(const std::string& text, const char* word) {
    const size_t n = std::char_traits<char>::length(word);
    for (size_t i = 0; (i = text.find(word, i)) != std::string::npos; i += n) {
        if (!atWord(text, i)) continue;
        const size_t end = i + n;
        if (end == text.size() || !std::isalpha(static_cast<unsigned char>(text[end])))
            return true;
    }
    return false;
}

// "Give a unit +2 [M]". Not "a friendly unit" and not "an enemy unit":
// those name who the printed card wants.
bool givesAnyUnitMight(const std::string& text) {
    static constexpr char kPhrase[] = "give a unit +";
    const size_t n = sizeof(kPhrase) - 1;
    for (size_t i = 0; (i = text.find(kPhrase, i)) != std::string::npos; i += n) {
        size_t j = i + n;
        if (j >= text.size() || !std::isdigit(static_cast<unsigned char>(text[j]))) continue;
        while (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j]))) ++j;
        while (j < text.size() && text[j] == ' ') ++j;
        if (text.compare(j, 3, "[m]") == 0) return true;
        if (atWord(text, j) && text.compare(j, 5, "might") == 0) return true;
    }
    return false;
}

int numberAt(const std::string& text, size_t i) {
    int n = 0;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
        n = n * 10 + (text[i] - '0');
        ++i;
    }
    return n;
}

SpellShape shapeOf(const Card& card) {
    SpellShape s;
    const std::string text = stripped(card.def().ability_text);
    s.wants_friendly = text.find("friendly") != std::string::npos ||
                       text.find("unit you control") != std::string::npos ||
                       text.find("units you control") != std::string::npos;
    s.additional_cost = text.find("additional cost") != std::string::npos;
    s.at_battlefield = text.find("at a battlefield") != std::string::npos;
    s.kills = hasWord(text, "kill") || hasWord(text, "kills");
    s.bounce = text.find("return ") != std::string::npos &&
               text.find("hand") != std::string::npos;
    s.banishes = hasWord(text, "banish") || hasWord(text, "banishes");

    for (size_t i = 0; (i = text.find("deal", i)) != std::string::npos; i += 4) {
        if (!atWord(text, i)) continue;
        size_t j = i + 4;
        if (j < text.size() && text[j] == 's') ++j;
        if (j >= text.size() || text[j] != ' ') continue;
        while (j < text.size() && text[j] == ' ') ++j;
        if (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j]))) {
            s.damage = numberAt(text, j);
            break;
        }
    }

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '+' && text[i] != '-') continue;
        if (i + 1 >= text.size() || !std::isdigit(static_cast<unsigned char>(text[i + 1])))
            continue;
        const int n = numberAt(text, i + 1);
        size_t j = i + 1;
        while (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j]))) ++j;
        while (j < text.size() && text[j] == ' ') ++j;
        const bool might = text.compare(j, 3, "[m]") == 0 ||
                           (atWord(text, j) && text.compare(j, 5, "might") == 0);
        if (!might) continue;
        const int delta = text[i] == '-' ? -n : n;
        if (delta < 0) s.might_delta = std::min(s.might_delta, delta);
        else if (s.might_delta == 0) s.might_delta = delta;
    }
    // "minimum of " is 11 characters. The digit is the next one; one past
    // that is the space, which is not a digit, so the floor stayed unset
    // and a −4 that leaves the unit at 1 Might was scored as a kill.
    static constexpr char kMinimumOf[] = "minimum of ";
    if (const size_t p = text.find(kMinimumOf); p != std::string::npos) {
        const size_t j = p + sizeof(kMinimumOf) - 1;
        if (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j])))
            s.might_floor = numberAt(text, j);
    }
    if (card.getTargetRequirements().must_be_friendly) s.wants_friendly = true;
    if (card.getTargetRequirements().must_be_at_battlefield) s.at_battlefield = true;
    s.rider = hasWord(text, "draw") || hasWord(text, "predict");
    s.replays = s.banishes && text.find("plays it") != std::string::npos;
    s.buffs_any_unit = s.might_delta > 0 && !s.harms() && givesAnyUnitMight(text);
    for (size_t p = 0; (p = text.find(" or less", p)) != std::string::npos; p += 8) {
        size_t j = p;
        while (j > 0 && text[j - 1] == ' ') --j;
        bool might = false;
        if (j >= 3 && text.compare(j - 3, 3, "[m]") == 0) {
            might = true;
            j -= 3;
        } else if (j >= 5 && text.compare(j - 5, 5, "might") == 0 && atWord(text, j - 5)) {
            might = true;
            j -= 5;
        }
        if (!might) continue;
        while (j > 0 && text[j - 1] == ' ') --j;
        size_t end = j;
        while (j > 0 && std::isdigit(static_cast<unsigned char>(text[j - 1]))) --j;
        if (j < end) s.max_might = numberAt(text, j);
    }
    return s;
}

const Card* cardFor(const GameState& state, GameObjectId id, const CardRegistry& cards) {
    if (id == kInvalidId || !state.objectExists(id)) return nullptr;
    const auto def = state.getObject(id).card_def_id;
    if (def == kInvalidId) return nullptr;
    return cards.get(def);
}

const Card* askingCard(const GameState& state, const CardRegistry& cards) {
    const ChainItem* item = nullptr;
    if (state.chain.resuming.has_value()) item = &*state.chain.resuming;
    else if (!state.chain.items.empty()) item = &state.chain.items.back();
    if (!item) return nullptr;
    return cardFor(state, item->source, cards);
}

bool isPlay(IntentType type) {
    return type == IntentType::PlayCard || type == IntentType::PlayReaction ||
           type == IntentType::PlayActionCard;
}

bool isActivation(IntentType type) {
    return type == IntentType::ActivateAbility ||
           type == IntentType::ActivateReactionAbility ||
           type == IntentType::ActivateActionAbility;
}

const Card* cardOf(const GameState& state, const Intent& intent, const CardRegistry& cards) {
    if (isPlay(intent.type) || isActivation(intent.type))
        return cardFor(state, intent.card, cards);
    if (intent.type == IntentType::MakeChoice) {
        // A spell's target is chosen before the spell is on the chain.
        if (const Card* choosing = cardFor(state, state.choosing_spell, cards))
            return choosing;
        return askingCard(state, cards);
    }
    return nullptr;
}

void addUnit(const GameState& state, GameObjectId id, std::vector<GameObjectId>& out) {
    if (id == kInvalidId || !state.objectExists(id)) return;
    if (!state.getObject(id).isUnit()) return;
    out.push_back(id);
}

std::vector<GameObjectId> namedUnits(const GameState& state, const Intent& intent) {
    std::vector<GameObjectId> out;
    for (auto id : intent.targets) addUnit(state, id, out);
    for (auto id : intent.chosen_objects) addUnit(state, id, out);
    return out;
}

bool sameSpell(const Intent& a, const Intent& b) {
    if (isPlay(a.type) || isActivation(a.type))
        return (isPlay(b.type) || isActivation(b.type)) && a.card == b.card;
    return a.type == IntentType::MakeChoice && b.type == IntentType::MakeChoice;
}

bool siblingNamesEnemy(const GameState& state, const Intent& intent,
                       const std::vector<Intent>& legal) {
    for (const auto& other : legal) {
        if (!sameSpell(intent, other)) continue;
        for (auto id : namedUnits(state, other)) {
            if (state.getObject(id).controller != intent.player) return true;
        }
    }
    return false;
}

bool siblingNamesFriendly(const GameState& state, const Intent& intent,
                          const std::vector<Intent>& legal) {
    for (const auto& other : legal) {
        if (!sameSpell(intent, other)) continue;
        for (auto id : namedUnits(state, other)) {
            if (state.getObject(id).controller == intent.player) return true;
        }
    }
    return false;
}

/// "Give a unit +N [M]". The opponent's unit is not the ordinary line while
/// a friendly unit is also a legal target. It stays when it is the only one,
/// and the rules engine still offers it either way. A choice that names a
/// friendly unit, or names nobody, is kept.
bool dropBuffOnEnemy(const GameState& state, const Intent& intent,
                     const std::vector<Intent>& legal, const SpellShape& shape,
                     const Card& card) {
    if (!shape.buffs_any_unit) return false;
    if (card.getTargetRequirements().must_be_enemy) return false;
    const auto units = namedUnits(state, intent);
    if (units.empty()) return false;
    for (auto id : units)
        if (state.getObject(id).controller == intent.player) return false;
    return siblingNamesFriendly(state, intent, legal);
}

bool harmKills(const GameObject& unit, const SpellShape& shape) {
    if (!unit.isUnit() || !unit.isAlive()) return false;
    if (shape.kills) return true;
    // A bounce does not kill. A banish does not kill either; one that plays
    // the unit again is kept in dropFriendly (shape.replays) instead of
    // being scored as an empty removal.
    if (shape.bounce || shape.banishes) return false;
    int damage = shape.damage;
    if (unit.immune_to_damage || unit.prevent_next_damage_this_turn) damage = 0;
    else if (unit.prevent_damage_remaining > 0)
        damage = std::max(0, damage - unit.prevent_damage_remaining);
    int might = unit.current_might;
    if (shape.might_delta < 0) {
        might += shape.might_delta;
        if (shape.might_floor >= 0) might = std::max(shape.might_floor, might);
        if (might < 0) might = 0;
    }
    return unit.damage_marked + damage >= might;
}

bool deathTrigger(const GameState& state, const GameObject& unit, const CardRegistry& cards) {
    if (unit.hasKeyword(Keyword::Deathknell)) return true;
    const Card* card = cardFor(state, unit.id, cards);
    if (!card) return false;
    const std::string text = stripped(card->def().ability_text);
    return text.find("deathknell") != std::string::npos ||
           text.find("when i die") != std::string::npos;
}

bool passesLocation(const GameObject& unit, const SpellShape& shape) {
    if (!unit.isUnit() || !unit.location.has_value() || !unit.isAlive()) return false;
    if (shape.at_battlefield && !unit.isAtBattlefield()) return false;
    if (shape.max_might >= 0 && unit.current_might > shape.max_might) return false;
    return true;
}

bool enemyCouldBeChosen(const GameState& state, PlayerId player, const SpellShape& shape) {
    for (auto& [id, obj] : state.objects.onBoard()) {
        (void)id;
        if (obj.controller == player) continue;
        if (passesLocation(obj, shape)) return true;
    }
    return false;
}

/// A friendly the printed card does not want, harmed without a death
/// trigger and without a kill that pays. A non-killing result is dropped
/// on its own. A kill with no death trigger is dropped whether or not an
/// enemy is also a legal target: an empty target set is not that enemy.
bool dropFriendly(const GameState& state, const GameObject& unit, PlayerId player,
                  const SpellShape& shape, bool enemy_legal, const CardRegistry& cards) {
    if (unit.controller != player) return false;
    if (!shape.harms() || shape.wants_friendly || shape.replays) return false;
    const bool killed = harmKills(unit, shape);
    if (killed && deathTrigger(state, unit, cards)) return false;
    if (killed && shape.additional_cost && shape.kills) return false;
    // The draw or Predict is why the line is cast when no enemy is legal,
    // whether or not the friendly dies.
    if (shape.rider && !enemy_legal) return false;
    return true;
}

bool dropUntargetedPlay(const GameState& state, PlayerId player, const SpellShape& shape,
                        const CardRegistry& cards) {
    if (!shape.harms() || shape.wants_friendly) return false;
    const bool enemy_legal = enemyCouldBeChosen(state, player, shape);
    bool any = false;
    for (auto& [id, obj] : state.objects.onBoard()) {
        (void)id;
        if (!passesLocation(obj, shape)) continue;
        any = true;
        if (!dropFriendly(state, obj, player, shape, enemy_legal, cards)) return false;
    }
    return any;
}

// A play that already named no one ("up to two units", including zero).
// That set is not an enemy and it is not a friendly harm. A spell that
// has not chosen yet still goes through dropUntargetedPlay.
bool choseNoOne(const Card& card) {
    const auto req = card.getTargetRequirements();
    if (!req.optional || req.count <= 0) return false;
    if (card.needsPlayTimeTarget() || card.needsPlayTimeTargetPair()) return false;
    if (card.choicesAtFinalization() & Card::kFinalizeResolve) return false;
    return true;
}

bool isPass(const Intent& intent) {
    switch (intent.type) {
        case IntentType::EndTurn:
        case IntentType::PassPriority:
        case IntentType::PassFocus:
        case IntentType::DeclineOptionalTrigger:
        case IntentType::DeclineTriggeredCost:
            return true;
        case IntentType::MakeChoice:
            return intent.declines ||
                   (intent.chosen_objects.empty() && intent.targets.empty() &&
                    intent.chosen_value == 0 && intent.chosen_card_def == kInvalidId);
        default:
            return intent.declines;
    }
}

bool dropIntent(const GameState& state, const Intent& intent,
                const std::vector<Intent>& legal, const CardRegistry& cards) {
    const Card* card = cardOf(state, intent, cards);
    if (!card) return false;
    const SpellShape shape = shapeOf(*card);
    if (shape.buffs_any_unit)
        return dropBuffOnEnemy(state, intent, legal, shape, *card);
    if (!shape.harms()) return false;
    const auto units = namedUnits(state, intent);
    if (units.empty()) {
        // Playing a unit is the line. Its text may ping, and that ping may
        // be optional (Bilgewater Tracker) or may name only enemies (Leona,
        // Zealot; Thousand-Tailed Watcher). The choice, if one is asked, is
        // filtered on its own.
        if (isPlay(intent.type) && card->def().card_type != CardType::Spell)
            return false;
        // Singularity's empty set names nobody. It does not keep the kill
        // of a friendly, and it is not dropped as if it were that kill.
        if (isPlay(intent.type) && card->def().card_type == CardType::Spell &&
            choseNoOne(*card))
            return false;
        if (isPlay(intent.type) || isActivation(intent.type))
            return dropUntargetedPlay(state, intent.player, shape, cards);
        return false;
    }
    const bool enemy_legal = siblingNamesEnemy(state, intent, legal);
    for (auto id : units) {
        if (dropFriendly(state, state.getObject(id), intent.player, shape, enemy_legal, cards))
            return true;
    }
    return false;
}

}  // namespace

std::vector<Intent> searchCandidates(const GameState& state,
                                     const std::vector<Intent>& legal,
                                     const CardRegistry& cards) {
    std::vector<Intent> kept;
    kept.reserve(legal.size());
    for (const auto& intent : legal) {
        if (!dropIntent(state, intent, legal, cards)) kept.push_back(intent);
    }
    if (!kept.empty()) return kept;
    for (const auto& intent : legal) {
        if (isPass(intent)) kept.push_back(intent);
    }
    // Every option was an empty friendly harm and none is a pass (Gust's
    // target prompt). Do not hand that list back.
    return kept;
}

}  // namespace riftbound
