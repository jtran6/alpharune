#include "io/play_by_play.h"

#include "io/state_renderer.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace riftbound {

namespace {
std::string joinPaid(const std::vector<std::string>& parts) {
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

// What the play line says after the card (and its target), before the period.
std::string costSuffix(const PlayCostNote* note) {
    if (!note || !note->known) return {};
    const int energy = note->totalEnergy();
    const int power = note->totalPower();
    std::vector<std::string> paid;
    if (energy > 0) paid.push_back(std::to_string(energy) + " energy");
    if (power > 0) paid.push_back(std::to_string(power) + " power");
    for (const auto& extra : note->other) paid.push_back(extra);
    const std::string list = joinPaid(paid);
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
}

PlayByPlay::~PlayByPlay() { events_.watchPlayCosts(false); }

std::string PlayByPlay::text() {
    closeTurn();
    std::ostringstream out;
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
