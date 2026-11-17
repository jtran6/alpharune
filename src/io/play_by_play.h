#pragma once
/// @file play_by_play.h
/// A short, human-readable account of one game: one block per turn.
///
/// Printed after every game of a normal run (a single game, and each game of
/// a batch small enough to list per game). It is not the trace and not the
/// decision log. A turn names whose turn it is. A play names the card and the
/// player. A move names the unit, where it came from, and where it went. A
/// conquer or a hold names the battlefield and who controls it afterwards.
/// The score is one line at the end of the turn. A spell's target is named,
/// including a friendly unit.

#include "core/events.h"
#include "core/game_state.h"

#include <boost/signals2/connection.hpp>
#include <string>
#include <vector>

namespace riftbound {

class PlayByPlay {
public:
    /// Subscribes to `events` for the life of this object. `state` is the
    /// engine's state: events are read after they have been applied to it.
    PlayByPlay(const GameState& state, EventBus& events);
    PlayByPlay(const PlayByPlay&) = delete;
    PlayByPlay& operator=(const PlayByPlay&) = delete;

    /// The turns so far. The turn still under way is closed with its score.
    std::string text();

private:
    struct Turn {
        int number = 0;
        PlayerId player = PlayerId::None;
        std::string legend;
        std::vector<std::string> lines;
    };

    void onPhase(const PhaseChangedEvent& e);
    void onPlayed(const CardPlayedEvent& e);
    void onFinalized(const ChainItemFinalizedEvent& e);
    void onMoved(const UnitMovedEvent& e);
    void onScored(const ScoreEvent& e);

    void add(std::string line);
    void closeTurn();
    std::string placeName(const LocationId& loc, PlayerId actor) const;
    std::string objectName(GameObjectId id) const;
    std::string legendName(PlayerId player) const;
    void collectUnits(const std::vector<GameObjectId>& ids,
                      std::vector<GameObjectId>& out) const;

    const GameState& state_;
    std::vector<Turn> turns_;
    Turn open_;
    bool open_live_ = false;
    // Destroyed first (declared last): callbacks are disconnected before
    // anything else on this object goes away.
    std::vector<boost::signals2::scoped_connection> connections_;
};

} // namespace riftbound
