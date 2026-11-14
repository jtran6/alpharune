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
    if (const size_t p = text.find("minimum of "); p != std::string::npos) {
        const size_t j = p + 12;
        if (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j])))
            s.might_floor = numberAt(text, j);
    }
    if (card.getTargetRequirements().must_be_friendly) s.wants_friendly = true;
    if (card.getTargetRequirements().must_be_at_battlefield) s.at_battlefield = true;
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
    if (intent.type == IntentType::MakeChoice) return askingCard(state, cards);
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

bool harmKills(const GameObject& unit, const SpellShape& shape) {
    if (!unit.isUnit() || !unit.isAlive()) return false;
    if (shape.kills) return true;
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
/// on its own. A kill is dropped only when an enemy is also a legal target.
bool dropFriendly(const GameState& state, const GameObject& unit, PlayerId player,
                  const SpellShape& shape, bool enemy_legal, const CardRegistry& cards) {
    if (unit.controller != player) return false;
    if (!shape.harms() || shape.wants_friendly) return false;
    const bool killed = harmKills(unit, shape);
    if (killed && deathTrigger(state, unit, cards)) return false;
    if (killed && shape.additional_cost && shape.kills) return false;
    if (!killed) return true;
    return enemy_legal;
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
    if (!shape.harms()) return false;
    const auto units = namedUnits(state, intent);
    if (units.empty()) {
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
    if (!kept.empty()) return kept;
    return legal;
}

}  // namespace riftbound
