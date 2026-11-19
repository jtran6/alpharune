#include "io/play_by_play.h"

#include "io/state_renderer.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace riftbound {

namespace {
std::string joinList(const std::vector<std::string>& parts) {
    if (parts.empty()) return {};
    if (parts.size() == 1) return parts[0];
    if (parts.size() == 2) return parts[0] + " and " + parts[1];
    std::string joined;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) joined += (i + 1 == parts.size()) ? ", and " : ", ";
        joined += parts[i];
    }
    return joined;
}

std::string named(const std::string& text) {
    return text.empty() ? std::string("unknown") : text;
}

/// A sentence fragment: a trailing period, unless the text already ends one.
std::string sentence(const std::string& text) {
    if (text.empty()) return "unknown error.";
    const char last = text.back();
    if (last == '.' || last == '!' || last == '?') return text;
    return text + ".";
}

std::string gameHeader(const PlayByPlay::Account& account) {
    return "Game " + std::to_string(account.number) + " [seed=" + std::to_string(account.seed) +
           "]: decks " + named(account.deck1) + " vs " + named(account.deck2) + ", agents " +
           named(account.agent1) + " vs " + named(account.agent2) + ".";
}

/// The words a reader uses to tell the endings apart: points, concede, an
/// alternate win (the card is named), or a draw. The engine's own reason
/// follows, so a burn-out point and a Gutter Palace win stay distinct.
std::string howEnded(const std::string& reason) {
    if (reason.find("conceded") != std::string::npos) return "concede: " + sentence(reason);
    if (reason.find("win condition") != std::string::npos)
        return "an alternate win: " + sentence(reason);
    if (reason.find("point") != std::string::npos) return "points: " + sentence(reason);
    std::string detail = reason;
    const bool draw = reason.find("Max turns") != std::string::npos ||
                      reason.find("505.6") != std::string::npos ||
                      reason.rfind("Draw", 0) == 0 || reason.find("draw") != std::string::npos;
    if (draw) {
        const std::string prefix = "Draw: ";
        if (detail.rfind(prefix, 0) == 0) detail.erase(0, prefix.size());
        return "a draw: " + sentence(detail);
    }
    if (reason.empty()) return "an unknown end.";
    return sentence(reason);
}

std::string cardLabel(const GameState& state, GameObjectId id) {
    if (id == kInvalidId || !state.objectExists(id)) return "a card";
    const std::string name = state.getObject(id).name.get();
    return name.empty() ? std::string("a card") : name;
}

// What the play line says after the card (and its target), before the period.
std::string costSuffix(const PlayCostNote* note) {
    if (!note || !note->known) return {};
    const int energy = note->totalEnergy();
    const int power = note->totalPower();
    std::vector<std::string> paid;
    if (energy > 0) paid.push_back(std::to_string(energy) + " energy");
    if (power > 0) paid.push_back(std::to_string(power) + " power");
    for (const auto& extra : note->other) paid.push_back(extra);
    const std::string list = joinList(paid);
    if (note->ignore_energy && note->ignore_power) {
        if (list.empty()) return ", ignoring its cost";
        return ", ignoring its cost, paying " + list;
    }
    std::string suffix;
    if (!list.empty()) suffix = " for " + list;
    else if (!note->ignore_energy && !note->ignore_power) return " for free";
    if (note->ignore_energy && energy == 0) suffix += ", ignoring its energy";
    if (note->ignore_power && power == 0) suffix += ", ignoring its power";
    return suffix;
}
} // namespace

PlayByPlay::PlayByPlay(const GameState& state, EventBus& events)
    : state_(state), events_(events) {
    events_.watchPlayCosts(true);
    connections_.emplace_back(events.on_phase_changed.connect(
        [this](const PhaseChangedEvent& e) { onPhase(e); }));
    connections_.emplace_back(events.on_card_played.connect(
        [this](const CardPlayedEvent& e) { onPlayed(e); }));
    connections_.emplace_back(events.on_chain_item_finalized.connect(
        [this](const ChainItemFinalizedEvent& e) { onFinalized(e); }));
    connections_.emplace_back(events.on_unit_moved.connect(
        [this](const UnitMovedEvent& e) { onMoved(e); }));
    connections_.emplace_back(events.on_score.connect(
        [this](const ScoreEvent& e) { onScored(e); }));
    connections_.emplace_back(events.on_combat_started.connect(
        [this](const CombatStartedEvent& e) { onCombatStarted(e); }));
    connections_.emplace_back(events.on_combat_ended.connect(
        [this](const CombatEndedEvent& e) { onCombatEnded(e); }));
    connections_.emplace_back(events.on_damage_dealt.connect(
        [this](const DamageDealtEvent& e) { onDamage(e); }));
    connections_.emplace_back(events.on_object_state_changed.connect(
        [this](const ObjectStateChangedEvent& e) { onStateChanged(e); }));
}

PlayByPlay::~PlayByPlay() { events_.watchPlayCosts(false); }

void PlayByPlay::setAccount(Account account) {
    account_ = std::move(account);
    have_account_ = true;
}

void PlayByPlay::noteDecision(const GameState& state, const Intent& chosen) {
    // A probe asks the same questions on a copy of the game. Those answers
    // are not what was played.
    if (events_.quiet()) return;
    if (chosen.type != IntentType::MulliganDecision) return;
    if (chosen.player != PlayerId::Player1 && chosen.player != PlayerId::Player2) return;

    Mulligan note;
    note.player = chosen.player;
    for (auto id : chosen.cards_to_mulligan) note.shipped.push_back(cardLabel(state, id));
    const auto& hand = state.player(chosen.player).hand;
    for (auto id : hand) {
        if (std::find(chosen.cards_to_mulligan.begin(), chosen.cards_to_mulligan.end(), id) !=
            chosen.cards_to_mulligan.end())
            continue;
        note.kept.push_back(cardLabel(state, id));
    }
    mulligans_.push_back(std::move(note));
}

void PlayByPlay::complete(PlayerId winner, int score1, int score2, int decisions,
                          std::string reason) {
    reported_ = true;
    failed_ = false;
    winner_ = winner;
    score1_ = score1;
    score2_ = score2;
    decisions_ = decisions;
    reason_ = std::move(reason);
}

void PlayByPlay::unusable(std::string error) {
    reported_ = true;
    failed_ = true;
    error_ = std::move(error);
    score1_ = state_.player(PlayerId::Player1).score;
    score2_ = state_.player(PlayerId::Player2).score;
    decisions_ = state_.decision_index;
    winner_ = state_.winner;
}

std::string PlayByPlay::failedGame(const Account& account, const std::string& error) {
    return gameHeader(account) + "\nEnded by a throw. Not usable: " + sentence(error) + "\n";
}

std::string PlayByPlay::text() {
    finishCombat();
    closeTurn();
    std::ostringstream out;
    if (reported_ && have_account_) {
        out << headerLine() << "\n";
        out << outcomeLine(static_cast<int>(turns_.size())) << "\n";
        const std::string mulligan = mulliganLine();
        if (!mulligan.empty()) out << mulligan;
    }
    for (const auto& turn : turns_) {
        out << "Turn " << turn.number << " - " << toString(turn.player);
        if (!turn.legend.empty()) out << " (" << turn.legend << ")";
        out << "\n";
        out << "  Runes at start: " << turn.runes_at_start << ".\n";
        for (const auto& line : turn.lines) {
            if (line.rfind("Score ", 0) == 0)
                out << "  Runes at end: " << turn.runes_at_end << ".\n";
            out << "  " << line << "\n";
        }
    }
    if (!turns_.empty()) out << "\n";
    return out.str();
}

std::string PlayByPlay::headerLine() const { return gameHeader(account_); }

std::string PlayByPlay::outcomeLine(int turns) const {
    std::ostringstream out;
    if (failed_) {
        out << "Ended by a throw. Not usable: " << sentence(error_) << " Score " << score1_ << "-"
            << score2_ << ". Turns " << turns << ". Decisions " << decisions_ << ".";
        return out.str();
    }
    out << "Ended by " << howEnded(reason_) << " ";
    if (winner_ == PlayerId::Player1 || winner_ == PlayerId::Player2)
        out << toString(winner_) << " wins. ";
    else
        out << "No winner. ";
    out << "Score " << score1_ << "-" << score2_ << ". Turns " << turns << ". Decisions "
        << decisions_ << ".";
    return out.str();
}

std::string PlayByPlay::mulliganLine() const {
    if (mulligans_.empty()) {
        // A throw before anyone answered has no mulligan to report. A game
        // that finished without one says so, so the line is not missing.
        if (failed_) return {};
        return "Mulligan: neither player was asked.\n";
    }
    std::ostringstream out;
    out << "Mulligan:";
    for (const auto& note : mulligans_) {
        out << " " << toString(note.player) << " ";
        if (!note.shipped.empty()) {
            out << "shipped " << joinList(note.shipped);
            if (!note.kept.empty()) out << ", and kept " << joinList(note.kept);
        } else {
            out << "kept " << (note.kept.empty() ? std::string("nothing") : joinList(note.kept));
        }
        out << ".";
    }
    out << "\n";
    return out.str();
}

void PlayByPlay::onPhase(const PhaseChangedEvent& e) {
    // runGame announces Setup -> Awaken once, before the turn loop, while the
    // phase is still the mulligan. The turn itself starts at its Awaken.
    if (e.new_phase != TurnPhase::AwakenPhase) return;
    if (e.old_phase == TurnPhase::Setup) return;
    beginTurn(e.turn_player);
}

void PlayByPlay::onPlayed(const CardPlayedEvent& e) {
    // A spell is recorded when it is finalized, once its targets are chosen.
    if (e.card_type == CardType::Spell) return;
    if (!state_.objectExists(e.object)) return;
    const auto& obj = state_.getObject(e.object);
    std::string line = std::string(toString(e.player)) + " plays " + obj.name.get();
    if (obj.location.has_value())
        line += " to " + placeName(*obj.location, e.player);
    line += paidFor(e.object);
    line += ".";
    add(std::move(line));
}

void PlayByPlay::collectUnits(const std::vector<GameObjectId>& ids,
                              std::vector<GameObjectId>& out) const {
    for (const auto id : ids) {
        if (id == kInvalidId || !state_.objectExists(id)) continue;
        // Friendly and enemy units are both named. Nothing here drops a
        // unit because its controller played the spell.
        if (!state_.getObject(id).isUnit()) continue;
        if (std::find(out.begin(), out.end(), id) != out.end()) continue;
        out.push_back(id);
    }
}

void PlayByPlay::onFinalized(const ChainItemFinalizedEvent& e) {
    const ChainItem* item = nullptr;
    for (const auto& ci : state_.chain.items)
        if (ci.id == e.item) { item = &ci; break; }
    if (!item || !item->is_spell) return;

    std::string line = std::string(toString(item->controller)) + " plays " + objectName(item->source);
    std::vector<GameObjectId> units;
    collectUnits(item->targets, units);
    collectUnits(item->chosen_objects, units);
    for (const auto& extra : item->repeat_choices) {
        collectUnits(extra.targets, units);
        collectUnits(extra.chosen_objects, units);
    }
    if (!units.empty()) {
        line += " on ";
        for (size_t i = 0; i < units.size(); ++i) {
            if (i > 0) line += (i + 1 == units.size()) ? " and " : ", ";
            line += objectName(units[i]);
        }
    }
    line += paidFor(item->source);
    line += ".";
    add(std::move(line));
}

void PlayByPlay::onMoved(const UnitMovedEvent& e) {
    const PlayerId actor = e.controller != PlayerId::None ? e.controller : PlayerId::Player1;
    // Origin and destination are both named. "base" is that player's base.
    add(std::string(toString(actor)) + " moves " + objectName(e.object) +
        " from " + placeName(e.from, actor) + " to " + placeName(e.to, actor) + ".");
}

void PlayByPlay::onScored(const ScoreEvent& e) {
    if (e.method != ScoreMethod::Conquer && e.method != ScoreMethod::Hold) return;
    const char* verb = e.method == ScoreMethod::Hold ? "holds" : "conquers";
    std::string who = toString(e.player);
    for (const auto& slot : state_.battlefields) {
        if (slot.id != e.battlefield) continue;
        if (slot.controller.has_value()) who = toString(*slot.controller);
        break;
    }
    add(std::string(toString(e.player)) + " " + verb + " " +
        StateRenderer::battlefieldLabel(state_, e.battlefield) + ". " + who + " controls it.");
}

void PlayByPlay::onCombatStarted(const CombatStartedEvent& e) {
    finishCombat();
    combat_ = Combat{};
    combat_.battlefield = e.battlefield;
    combat_.attacker = e.attacker;
    combat_.defender = e.defender;
    combat_.where = placeName(BattlefieldLocation{e.battlefield}, e.attacker);
    combat_open_ = true;
    const LocationId loc{BattlefieldLocation{e.battlefield}};
    for (auto id : state_.unitsAt(loc, e.attacker)) noteFighter(id);
    for (auto id : state_.unitsAt(loc, e.defender)) noteFighter(id);
}

void PlayByPlay::onCombatEnded(const CombatEndedEvent& e) {
    if (!combat_open_) return;
    if (combat_.battlefield.valid() && e.battlefield != combat_.battlefield) return;
    finishCombat();
}

void PlayByPlay::onDamage(const DamageDealtEvent& e) {
    // Spell damage during a showdown is not this combat's damage.
    if (!combat_open_ || !e.is_combat_damage || e.amount <= 0) return;
    noteFighter(e.target);
    for (auto& hit : combat_.damage) {
        if (hit.id != e.target) continue;
        hit.amount += e.amount;
        return;
    }
    PlayerId who = PlayerId::None;
    std::string name = "a unit";
    if (state_.objectExists(e.target)) {
        who = state_.getObject(e.target).controller;
        name = objectName(e.target);
    }
    combat_.damage.push_back(DamageNote{e.target, who, std::move(name), e.amount});
}

void PlayByPlay::onStateChanged(const ObjectStateChangedEvent& e) {
    // A unit that arrives after the combat has started still fought.
    if (!combat_open_ || !(e.what_changed == "joined_combat")) return;
    noteFighter(e.object);
}

void PlayByPlay::noteFighter(GameObjectId id) {
    if (!combat_open_ || id == kInvalidId || !state_.objectExists(id)) return;
    for (const auto& fighter : combat_.fighters)
        if (fighter.id == id) return;
    const auto& obj = state_.getObject(id);
    if (!obj.isUnit()) return;
    combat_.fighters.push_back(Fighter{id, obj.controller, objectName(id)});
}

void PlayByPlay::finishCombat() {
    if (!combat_open_) return;
    auto side = [&](PlayerId who) {
        std::vector<std::string> names;
        for (const auto& fighter : combat_.fighters)
            if (fighter.controller == who) names.push_back(fighter.name);
        if (names.empty()) return std::string("no one");
        return std::string(toString(who)) + "'s " + joinList(names);
    };
    std::string line = "Combat at " + combat_.where + ": " + side(combat_.attacker) + " against " +
                       side(combat_.defender) + ". ";
    if (combat_.damage.empty()) {
        line += "No combat damage.";
    } else {
        std::vector<std::string> parts;
        for (const auto& hit : combat_.damage) {
            std::string who;
            if (hit.controller == PlayerId::Player1 || hit.controller == PlayerId::Player2)
                who = std::string(toString(hit.controller)) + "'s ";
            parts.push_back(who + hit.name + " takes " + std::to_string(hit.amount) + " damage");
        }
        line += joinList(parts) + ".";
    }
    combat_open_ = false;
    add(std::move(line));
}

void PlayByPlay::beginTurn(PlayerId player) {
    closeTurn();
    // The engine's turn_number is 0 on the first turn.
    open_.number = state_.turn.turn_number + 1;
    if (open_.number < 1) open_.number = 1;
    open_.player = player;
    if (player == PlayerId::Player1 || player == PlayerId::Player2)
        open_.legend = legendName(player);
    open_.runes_at_start = runeCount(player);
    open_live_ = true;
}

void PlayByPlay::add(std::string line) {
    if (!open_live_) beginTurn(state_.turn.turn_player);
    open_.lines.push_back(std::move(line));
}

void PlayByPlay::closeTurn() {
    if (!open_live_) return;
    open_.runes_at_end = runeCount(open_.player);
    const int p1 = state_.player(PlayerId::Player1).score;
    const int p2 = state_.player(PlayerId::Player2).score;
    open_.lines.push_back("Score " + std::to_string(p1) + "-" + std::to_string(p2) + ".");
    turns_.push_back(std::move(open_));
    open_ = Turn{};
    open_live_ = false;
}

int PlayByPlay::runeCount(PlayerId player) const {
    if (player != PlayerId::Player1 && player != PlayerId::Player2) return 0;
    return static_cast<int>(state_.runesInBase(player).size());
}

std::string PlayByPlay::paidFor(GameObjectId card) const {
    return costSuffix(events_.playCost(card));
}

std::string PlayByPlay::placeName(const LocationId& loc, PlayerId actor) const {
    if (const auto* base = std::get_if<BaseLocation>(&loc)) {
        if (base->player == actor || base->player == PlayerId::None) return "base";
        return std::string(toString(base->player)) + "'s base";
    }
    if (const auto* bf = std::get_if<BattlefieldLocation>(&loc))
        return StateRenderer::battlefieldLabel(state_, bf->id);
    return "base";
}

std::string PlayByPlay::objectName(GameObjectId id) const {
    if (id == kInvalidId || !state_.objectExists(id)) return "a unit";
    const std::string name = state_.getObject(id).name.get();
    return name.empty() ? std::string("a unit") : name;
}

std::string PlayByPlay::legendName(PlayerId player) const {
    if (player != PlayerId::Player1 && player != PlayerId::Player2) return {};
    const auto id = state_.player(player).legend_zone;
    if (!state_.objectExists(id)) return {};
    return state_.getObject(id).name.get();
}

} // namespace riftbound
