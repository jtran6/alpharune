#pragma once
/// @file play_by_play.h
/// A short, human-readable account of one game: a header, then one block per turn.
///
/// Printed after every game of a normal run (a single game, and each game of
/// a batch small enough to list per game, including a batch of 20). It is not
/// the trace and not the decision log. The header names the game and its
/// seed, both decks, both agents, how the game ended, the final score, the
/// turns, and the decision count. A throw still produces the block: which
/// game, the seed, the error text, and that the game is not usable. Nothing
/// here says whether the game is a good training sample.
///
/// A turn names whose turn it is. The mulligan names what each player shipped
/// and what they kept. A play names the card and the player. A move names the
/// unit, where it came from, and where it went. A combat names who fought and
/// the damage. A conquer or a hold names the battlefield and who controls it
/// afterwards. A Predict names the player. The score is one line at the end
/// of the turn. A spell's target is named, including a friendly unit. Each
/// turn says how many runes that player has when the turn starts and again
/// when it ends. Each play says what was paid for it. A free play and a play
/// that ignores its cost say so. A quiet probe writes nothing.
///
/// A unit is named with the player who controls it when the line is written,
/// as "Bedeviling Stranger (P2)". A control change shows the new controller.
/// A unit that has already left the board keeps the controller it had when
/// the line's event saw it: leaving the board gives the card back to its
/// owner. Battlefields, spells, legends, and runes are not marked. A play
/// that paid [Accelerate] names it with the energy and power ("for 8 energy,
/// 1 power, and Accelerate"). A play that did not pay it says nothing extra.

#include "core/events.h"
#include "core/game_state.h"

#include <boost/signals2/connection.hpp>
#include <string>
#include <vector>

namespace riftbound {

class PlayByPlay {
public:
    /// What the header names. Deck and agent strings are the ones the run
    /// was started with (a deck-list stem, an agent spec).
    struct Account {
        int number = 1;
        uint64_t seed = 0;
        std::string deck1;
        std::string deck2;
        std::string agent1;
        std::string agent2;
    };

    /// Subscribes to `events` for the life of this object. `state` is the
    /// engine's state: events are read after they have been applied to it.
    PlayByPlay(const GameState& state, EventBus& events);
    ~PlayByPlay();
    PlayByPlay(const PlayByPlay&) = delete;
    PlayByPlay& operator=(const PlayByPlay&) = delete;

    void setAccount(Account account);

    /// A mulligan decision, as it was asked. Ignored while the bus is quiet
    /// (a probe) and for every other kind of decision.
    void noteDecision(const GameState& state, const Intent& chosen);

    /// The game finished. `text()` then opens with the header.
    void complete(PlayerId winner, int score1, int score2, int decisions, std::string reason);

    /// The game threw. `text()` still opens with the header, the error text,
    /// and that the game is not usable. Turns that happened are kept.
    void unusable(std::string error);

    /// The header for a game that threw before an account of its turns could
    /// be kept (the agents were never built). Same words as `unusable`.
    static std::string failedGame(const Account& account, const std::string& error);

    /// The turns so far, and the header once `complete` or `unusable` has
    /// been called. The turn still under way is closed with its score.
    std::string text();

private:
    struct Turn {
        int number = 0;
        PlayerId player = PlayerId::None;
        std::string legend;
        int runes_at_start = 0;
        int runes_at_end = 0;
        std::vector<std::string> lines;
    };

    struct Mulligan {
        PlayerId player = PlayerId::None;
        std::vector<std::string> shipped;
        std::vector<std::string> kept;
    };

    struct Fighter {
        GameObjectId id = kInvalidId;
        PlayerId controller = PlayerId::None;
        std::string name;
    };

    struct DamageNote {
        GameObjectId id = kInvalidId;
        PlayerId controller = PlayerId::None;
        std::string name;
        int amount = 0;
    };

    struct Combat {
        BattlefieldId battlefield = kInvalidId;
        PlayerId attacker = PlayerId::None;
        PlayerId defender = PlayerId::None;
        std::string where;
        std::vector<Fighter> fighters;
        std::vector<DamageNote> damage;
    };

    void onPhase(const PhaseChangedEvent& e);
    void onPlayed(const CardPlayedEvent& e);
    void onFinalized(const ChainItemFinalizedEvent& e);
    void onMoved(const UnitMovedEvent& e);
    void onScored(const ScoreEvent& e);
    void onCombatStarted(const CombatStartedEvent& e);
    void onCombatEnded(const CombatEndedEvent& e);
    void onDamage(const DamageDealtEvent& e);
    void onStateChanged(const ObjectStateChangedEvent& e);
    void onPredicted(const PredictEvent& e);

    void beginTurn(PlayerId player);
    void add(std::string line);
    void closeTurn();
    void finishCombat();
    void noteFighter(GameObjectId id);
    int runeCount(PlayerId player) const;
    std::string paidFor(GameObjectId card) const;
    std::string placeName(const LocationId& loc, PlayerId actor) const;
    std::string objectName(GameObjectId id) const;
    /// A unit, with the player who controls it now: "Lecturing Yordle (P1)".
    /// A card that is not a unit is its name alone.
    std::string unitLabel(GameObjectId id) const;
    /// The same, for a combat line written after the unit may have left.
    /// Still on the board: the controller now. Already gone: `seen`.
    std::string unitAsOfLine(GameObjectId id, PlayerId seen,
                             const std::string& seen_name) const;
    std::string legendName(PlayerId player) const;
    void collectUnits(const std::vector<GameObjectId>& ids,
                      std::vector<GameObjectId>& out) const;
    std::string headerLine() const;
    std::string outcomeLine(int turns) const;
    std::string mulliganLine() const;

    const GameState& state_;
    EventBus& events_;
    std::vector<Turn> turns_;
    Turn open_;
    bool open_live_ = false;
    Account account_{};
    bool have_account_ = false;
    bool reported_ = false;
    bool failed_ = false;
    std::string error_;
    std::string reason_;
    PlayerId winner_ = PlayerId::None;
    int score1_ = 0;
    int score2_ = 0;
    int decisions_ = 0;
    std::vector<Mulligan> mulligans_;
    Combat combat_{};
    bool combat_open_ = false;
    // Destroyed first (declared last): callbacks are disconnected before
    // anything else on this object goes away.
    std::vector<boost::signals2::scoped_connection> connections_;
};

} // namespace riftbound
