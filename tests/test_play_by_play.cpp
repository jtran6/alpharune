/// The short account printed after a game. A move that drops its origin or
/// its destination fails, and so does a short game whose summary was not
/// printed. A turn without its rune lines fails, and so does a play that
/// names no cost. A finished game whose header omits the seed, the decks,
/// the agents, the ending, the score, the turns, or the decisions fails.
/// So does a combat that omits who fought or the damage, a mulligan that
/// omits what was shipped or kept, and a throw that is dropped or that
/// calls the game usable. A unit named on a play, a move, a spell, or a
/// combat line without its controller in parentheses fails. A control
/// change that still shows the old controller fails. An [Accelerate] play
/// that never says Accelerate fails, and so does a play that did not pay
/// it and says the word anyway.

#include <gtest/gtest.h>

#include "agents/agent_interface.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_runner.h"
#include "io/play_by_play.h"
#include "rules/deck_validator.h"
#include "tests/cards/card_test_fixture.h"

#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>

namespace riftbound::test::play_by_play {

namespace {

/// Plays a unit, moves it, and concedes at the next chance to pass. Each
/// seat gets its own agent, so a seat concedes only after its own move.
class MoveThenStop : public AgentInterface {
public:
    Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
        if (legal.empty()) return Intent::concede(PlayerId::Player1);
        const PlayerId who = legal.front().player;
        if (++decisions_ > 500) return Intent::concede(who);

        for (const auto& action : legal)
            if (action.type == IntentType::MulliganDecision && action.cards_to_mulligan.empty())
                return action;

        auto pass = [](IntentType type) {
            return type == IntentType::EndTurn || type == IntentType::PassPriority ||
                   type == IntentType::PassFocus;
        };
        if (stop_) {
            for (const auto& action : legal)
                if (pass(action.type)) return Intent::concede(who);
        }

        for (const auto& action : legal) {
            if (action.type != IntentType::StandardMove || !action.move_destination.has_value())
                continue;
            stop_ = true;
            return action;
        }

        for (const auto& action : legal) {
            if (action.type != IntentType::PlayCard && action.type != IntentType::PlayActionCard &&
                action.type != IntentType::PlayReaction)
                continue;
            if (!state.objectExists(action.card) || !state.getObject(action.card).isUnit()) continue;
            return action;
        }

        for (const auto& action : legal)
            if (pass(action.type)) return action;
        for (const auto& action : legal)
            if (action.type != IntentType::Concede) return action;
        return legal.front();
    }

private:
    bool stop_ = false;
    int decisions_ = 0;
};

/// A line that reports a move names a non-empty origin and a non-empty
/// destination. A line that is not a move is not this function's concern.
bool moveNamesPlaces(const std::string& line) {
    const auto moves = line.find(" moves ");
    if (moves == std::string::npos) return true;
    const auto from = line.find(" from ", moves);
    const auto to = line.find(" to ", from == std::string::npos ? moves : from);
    if (from == std::string::npos || to == std::string::npos || !(from < to)) return false;
    const std::string origin = line.substr(from + 6, to - (from + 6));
    const auto dest_end = line.find_first_of(".\n", to + 4);
    const std::string dest = line.substr(to + 4, dest_end == std::string::npos
                                                      ? std::string::npos
                                                      : dest_end - (to + 4));
    auto blank = [](const std::string& text) {
        return text.find_first_not_of(" \t") == std::string::npos;
    };
    return !blank(origin) && !blank(dest);
}

/// The unit a move names is "Name (P1)" or "Name (P2)", the controller when
/// the line was written. A line that is not a move is not this function's
/// concern.
bool moveNamesOwner(const std::string& line) {
    const auto moves = line.find(" moves ");
    if (moves == std::string::npos) return true;
    const auto from = line.find(" from ", moves);
    if (from == std::string::npos || from < moves + 7) return false;
    const std::string unit = line.substr(moves + 7, from - (moves + 7));
    if (unit.size() < 6) return false;
    const std::string tail = unit.substr(unit.size() - 5);
    return tail == " (P1)" || tail == " (P2)";
}

/// The bare name of the unit a move line names, without " (P1)" / " (P2)".
std::string movedUnitName(const std::string& line) {
    if (!moveNamesOwner(line)) return {};
    const auto moves = line.find(" moves ");
    const auto from = line.find(" from ", moves);
    const std::string unit = line.substr(moves + 7, from - (moves + 7));
    return unit.substr(0, unit.size() - 5);
}

/// Every move names its unit's controller. Every later play, move, or combat
/// line that uses one of those unit names keeps the parenthetical on it.
void expectUnitMentionsCarryOwner(const std::string& text) {
    std::vector<std::string> names;
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
        if (line.find(" moves ") == std::string::npos) continue;
        EXPECT_TRUE(moveNamesOwner(line)) << line;
        const std::string name = movedUnitName(line);
        if (!name.empty() && std::find(names.begin(), names.end(), name) == names.end())
            names.push_back(name);
    }
    for (const auto& row : lines) {
        const bool about_a_unit = row.find(" plays ") != std::string::npos ||
                                  row.find(" moves ") != std::string::npos ||
                                  row.find("Combat at ") != std::string::npos;
        if (!about_a_unit) continue;
        for (const auto& name : names) {
            for (size_t at = 0; (at = row.find(name, at)) != std::string::npos;) {
                const std::string after = row.substr(at + name.size());
                EXPECT_TRUE(after.rfind(" (P1)", 0) == 0 || after.rfind(" (P2)", 0) == 0)
                    << row;
                at += name.size();
            }
        }
    }
}

/// A play names what was paid, or says that it was free or that its cost was
/// ignored. A line that is not a play is not this function's concern.
bool playNamesCost(const std::string& line) {
    if (line.find(" plays ") == std::string::npos) return true;
    return line.find(" for ") != std::string::npos ||
           line.find("ignoring its ") != std::string::npos;
}

int countOf(const std::string& text, const std::string& needle) {
    int n = 0;
    for (size_t at = 0; (at = text.find(needle, at)) != std::string::npos; at += needle.size())
        ++n;
    return n;
}

/// Every turn block has a rune line for its start and another for its end,
/// and every play names a cost.
void expectRunesAndCosts(const std::string& text) {
    const int turns = countOf(text, "\nTurn ") + (text.rfind("Turn ", 0) == 0 ? 1 : 0);
    EXPECT_GT(turns, 0) << text;
    EXPECT_EQ(countOf(text, "Runes at start:"), turns) << text;
    EXPECT_EQ(countOf(text, "Runes at end:"), turns) << text;
    std::istringstream lines(text);
    std::string line;
    int plays = 0;
    while (std::getline(lines, line)) {
        if (line.find(" plays ") == std::string::npos) continue;
        ++plays;
        EXPECT_TRUE(playNamesCost(line)) << line;
    }
    EXPECT_GE(plays, 1) << text;
}

} // namespace

// The sentence a reader was promised: unit, origin, destination. The spell's
// target is the caster's own unit, and the line still names it.
TEST(PlayByPlayText, MoveNamesOriginAndDestination) {
    GameState state;
    EventBus bus;

    const auto unit = state.createObject();
    auto& yordle = state.getObject(unit);
    yordle.name = "Lecturing Yordle";
    yordle.card_type = CardType::Unit;
    yordle.controller = PlayerId::Player1;
    yordle.owner = PlayerId::Player1;

    const auto spell = state.createObject();
    auto& smoke = state.getObject(spell);
    smoke.name = "Smoke Screen";
    smoke.card_type = CardType::Spell;
    smoke.controller = PlayerId::Player1;

    const auto field = state.createObject();
    auto& fortress = state.getObject(field);
    fortress.name = "Frozen Fortress";
    fortress.card_type = CardType::Battlefield;
    BattlefieldState bf;
    bf.id = BattlefieldId::fromRaw(0);
    bf.card_object_id = field;
    state.battlefields.push_back(bf);

    state.turn.turn_number = 0;
    state.turn.turn_player = PlayerId::Player1;

    const auto first_rune = state.createObject();
    auto& fury = state.getObject(first_rune);
    fury.name = "Fury Rune";
    fury.card_type = CardType::Rune;
    fury.controller = PlayerId::Player1;
    fury.owner = PlayerId::Player1;
    fury.zone = ZoneType::Base;
    fury.location = BaseLocation{PlayerId::Player1};

    PlayByPlay chronicle(state, bus);
    bus.emit(PhaseChangedEvent{TurnPhase::Mulligan, TurnPhase::AwakenPhase, PlayerId::Player1});
    bus.emit(UnitMovedEvent{unit, PlayerId::Player1, BaseLocation{PlayerId::Player1},
                            BattlefieldLocation{0}, true});

    state.battlefields[0].controller = PlayerId::Player1;
    bus.emit(ScoreEvent{PlayerId::Player1, BattlefieldId::fromRaw(0), ScoreMethod::Conquer, 1,
                        std::nullopt});

    ChainItem item;
    item.id = ChainItemId::fromRaw(1);
    item.source = spell;
    item.controller = PlayerId::Player1;
    item.is_spell = true;
    item.targets = {unit};
    item.chosen_objects = {unit};
    state.chain.items.push_back(item);
    bus.notePlayCost(spell, 1, 0, false, false);
    bus.emit(ChainItemFinalizedEvent{item.id, spell, PlayerId::Player1});

    const auto second_rune = state.createObject();
    auto& calm = state.getObject(second_rune);
    calm.name = "Calm Rune";
    calm.card_type = CardType::Rune;
    calm.controller = PlayerId::Player1;
    calm.owner = PlayerId::Player1;
    calm.zone = ZoneType::Base;
    calm.location = BaseLocation{PlayerId::Player1};

    state.players[0].score = 1;
    const std::string text = chronicle.text();

    EXPECT_NE(text.find("Turn 1 - P1"), std::string::npos) << text;
    EXPECT_NE(text.find("Runes at start: 1."), std::string::npos) << text;
    EXPECT_NE(text.find("Runes at end: 2."), std::string::npos) << text;
    EXPECT_NE(text.find("P1 moves Lecturing Yordle (P1) from base to Frozen Fortress."),
              std::string::npos) << text;
    EXPECT_NE(text.find("P1 conquers Frozen Fortress. P1 controls it."), std::string::npos) << text;
    EXPECT_NE(text.find("P1 plays Smoke Screen on Lecturing Yordle (P1) for 1 energy."),
              std::string::npos) << text;
    EXPECT_EQ(text.find("Smoke Screen (P"), std::string::npos) << text;
    EXPECT_EQ(text.find("Frozen Fortress (P"), std::string::npos) << text;
    EXPECT_EQ(text.find("Accelerate"), std::string::npos) << text;
    EXPECT_NE(text.find("Score 1-0."), std::string::npos) << text;
    expectRunesAndCosts(text);

    std::istringstream lines(text);
    std::string line;
    int moves = 0;
    while (std::getline(lines, line)) {
        if (line.find(" moves ") == std::string::npos) continue;
        ++moves;
        EXPECT_TRUE(moveNamesPlaces(line)) << line;
        EXPECT_TRUE(moveNamesOwner(line)) << line;
    }
    EXPECT_EQ(moves, 1) << text;
    expectUnitMentionsCarryOwner(text);
}

// A real game, with tracing and the HTML replay off. The per-game result
// line is still there, and the summary under it has a turn and a move that
// names both places.
TEST(PlayByPlayText, ShortGamePrintsTheSummary) {
    CardRegistry registry;
    registry.loadAll();
    CardDB db;
    db.buildFromClasses(registry);
    const char* env = std::getenv("RIFTBOUND_ROOT");
    const std::string root = env ? env : ".";
    const auto deck1 = DeckValidator::loadFromDeckList(root + "/decks/fiora.txt", db);
    const auto deck2 = DeckValidator::loadFromDeckList(root + "/decks/yi_rebuild.txt", db);

    GameConfig cfg;
    cfg.base_seed = 19;
    // A batch of 20 is still listed, one block per game.
    cfg.total_games = 20;
    cfg.deck1_name = "fiora";
    cfg.deck2_name = "yi_rebuild";
    cfg.agent1_spec = "move-then-stop";
    cfg.agent2_spec = "move-then-stop";
    cfg.debug_mode = false;
    cfg.trace_mode = false;
    cfg.do_render = false;
    cfg.agent_factory = [](int, uint64_t) {
        return std::make_unique<MoveThenStop>();
    };
    AggregateResults results;
    results.total_games = 1;
    GameRunner runner(db, registry, deck1, deck2, cfg, results);

    testing::internal::CaptureStdout();
    runner.run();
    const std::string out = testing::internal::GetCapturedStdout();

    const auto game = out.find(
        "Game 1 [seed=19]: decks fiora vs yi_rebuild, agents move-then-stop vs move-then-stop.");
    const auto turn = out.find("Turn 1 - ");
    ASSERT_NE(game, std::string::npos) << out;
    ASSERT_NE(turn, std::string::npos) << out;
    EXPECT_LT(game, turn) << out;
    EXPECT_NE(out.find("Ended by concede:"), std::string::npos) << out;
    EXPECT_NE(out.find(" wins."), std::string::npos) << out;
    EXPECT_NE(out.find("Mulligan:"), std::string::npos) << out;
    EXPECT_NE(out.find("P1 kept "), std::string::npos) << out;
    EXPECT_NE(out.find("P2 kept "), std::string::npos) << out;
    EXPECT_EQ(out.find("shipped"), std::string::npos) << out;
    EXPECT_NE(out.find("Decisions "), std::string::npos) << out;
    EXPECT_EQ(out.find("Not usable"), std::string::npos) << out;
    EXPECT_NE(out.find("\n  Score "), std::string::npos) << out;
    EXPECT_EQ(out.find("[TRC]"), std::string::npos) << out;
    const auto turns_at = out.find("Turns ");
    ASSERT_NE(turns_at, std::string::npos) << out;
    const int header_turns = std::atoi(out.c_str() + turns_at + 6);
    const int blocks = countOf(out, "\nTurn ");
    EXPECT_EQ(header_turns, blocks) << out;
    expectRunesAndCosts(out);

    std::istringstream lines(out);
    std::string line;
    int moves = 0;
    while (std::getline(lines, line)) {
        if (line.find(" moves ") == std::string::npos) continue;
        ++moves;
        EXPECT_TRUE(moveNamesPlaces(line)) << line;
        EXPECT_TRUE(moveNamesOwner(line)) << line;
    }
    EXPECT_GE(moves, 1) << out;
    expectUnitMentionsCarryOwner(out);
}

PlayByPlay::Account sampleAccount() {
    PlayByPlay::Account account;
    account.number = 4;
    account.seed = 99;
    account.deck1 = "fiora";
    account.deck2 = "yi_rebuild";
    account.agent1 = "random";
    account.agent2 = "ismcts:sims=20";
    return account;
}

std::string endedBy(const std::string& reason, PlayerId winner, int score1, int score2) {
    GameState state;
    EventBus bus;
    PlayByPlay chronicle(state, bus);
    chronicle.setAccount(sampleAccount());
    chronicle.complete(winner, score1, score2, 40, reason);
    return chronicle.text();
}

// The header is the fact list for the data decision. It names the ending
// and does not say whether the game should be trained on.
TEST(PlayByPlayText, HeaderNamesHowTheGameEnded) {
    const std::string gutter = endedBy("Gutter Palace win condition", PlayerId::Player1, 2, 2);
    EXPECT_NE(gutter.find("Game 4 [seed=99]: decks fiora vs yi_rebuild, agents random vs "
                          "ismcts:sims=20."),
              std::string::npos) << gutter;
    EXPECT_NE(gutter.find("Ended by an alternate win: Gutter Palace win condition."),
              std::string::npos) << gutter;
    EXPECT_NE(gutter.find("P1 wins. Score 2-2. Turns 0. Decisions 40."), std::string::npos)
        << gutter;
    EXPECT_NE(gutter.find("Mulligan: neither player was asked."), std::string::npos) << gutter;
    EXPECT_EQ(gutter.find("Not usable"), std::string::npos) << gutter;

    const std::string plaza =
        endedBy("The Grand Plaza win condition (7+ units here)", PlayerId::Player2, 1, 3);
    EXPECT_NE(plaza.find("Ended by an alternate win: The Grand Plaza win condition (7+ units here)."),
              std::string::npos) << plaza;

    const std::string points = endedBy("P1 reached 8 points", PlayerId::Player1, 8, 3);
    EXPECT_NE(points.find("Ended by points: P1 reached 8 points."), std::string::npos) << points;

    const std::string burned =
        endedBy("P2 wins via burn-out point (CR 432.3)", PlayerId::Player2, 4, 8);
    EXPECT_NE(burned.find("Ended by points: P2 wins via burn-out point (CR 432.3)."),
              std::string::npos) << burned;

    const std::string conceded = endedBy("P1 conceded", PlayerId::Player2, 1, 0);
    EXPECT_NE(conceded.find("Ended by concede: P1 conceded."), std::string::npos) << conceded;

    const std::string capped = endedBy("Max turns reached", PlayerId::None, 4, 4);
    EXPECT_NE(capped.find("Ended by a draw: Max turns reached. No winner."), std::string::npos)
        << capped;

    const std::string loop = endedBy(
        "Draw: a loop of mandatory actions no player broke (Tournament Rules 505.6), at Self",
        PlayerId::None, 0, 0);
    EXPECT_NE(loop.find("Ended by a draw: a loop of mandatory actions no player broke "
                        "(Tournament Rules 505.6), at Self."),
              std::string::npos) << loop;

    GameState state;
    EventBus bus;
    PlayByPlay thrown(state, bus);
    thrown.setAccount(sampleAccount());
    state.decision_index = 3;
    thrown.unusable("injected fault");
    const std::string failed = thrown.text();
    EXPECT_NE(failed.find("Game 4 [seed=99]:"), std::string::npos) << failed;
    EXPECT_NE(failed.find("Ended by a throw. Not usable: injected fault."), std::string::npos)
        << failed;
    EXPECT_NE(failed.find("Score 0-0. Turns 0. Decisions 3."), std::string::npos) << failed;
    EXPECT_EQ(failed.find("wins"), std::string::npos) << failed;

    const std::string never_started = PlayByPlay::failedGame(sampleAccount(), "agent factory failed");
    EXPECT_NE(never_started.find("Game 4 [seed=99]: decks fiora vs yi_rebuild"), std::string::npos)
        << never_started;
    EXPECT_NE(never_started.find("Ended by a throw. Not usable: agent factory failed."),
              std::string::npos) << never_started;
}

// Shipped cards are the ones set aside. Kept cards are the rest of the hand,
// in hand order, not the cards drawn to replace them.
TEST(PlayByPlayText, MulliganNamesWhatWasShippedAndKept) {
    GameState state;
    EventBus bus;
    auto add = [&](const char* name) {
        const auto id = state.createObject();
        state.getObject(id).name = name;
        return id;
    };
    const auto smoke = add("Smoke Screen");
    const auto yordle = add("Lecturing Yordle");
    const auto charm = add("Charm");
    const auto fiora = add("Fiora");
    state.player(PlayerId::Player1).hand = {smoke, yordle, charm};
    state.player(PlayerId::Player2).hand = {fiora, smoke, yordle, charm};

    PlayByPlay chronicle(state, bus);
    chronicle.setAccount(sampleAccount());
    chronicle.noteDecision(state, Intent::mulligan(PlayerId::Player1, {smoke}));
    chronicle.noteDecision(state, Intent::mulligan(PlayerId::Player2, {}));
    chronicle.complete(PlayerId::Player2, 0, 1, 2, "P1 conceded");
    const std::string text = chronicle.text();

    EXPECT_NE(text.find("Mulligan: P1 shipped Smoke Screen, and kept Lecturing Yordle and Charm. "
                        "P2 kept Fiora, Smoke Screen, Lecturing Yordle, and Charm."),
              std::string::npos) << text;
}

// Who fought, including a unit that joined after the combat started, and the
// combat damage. A spell's damage during the combat is not that line.
TEST(PlayByPlayText, CombatNamesFightersAndDamage) {
    GameState state;
    EventBus bus;

    const auto field = state.createObject();
    auto& fortress = state.getObject(field);
    fortress.name = "Frozen Fortress";
    fortress.card_type = CardType::Battlefield;
    BattlefieldState bf;
    bf.id = BattlefieldId::fromRaw(0);
    bf.card_object_id = field;
    state.battlefields.push_back(bf);

    auto place = [&](const char* name, PlayerId who) {
        const auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.name = name;
        obj.card_type = CardType::Unit;
        obj.controller = who;
        obj.owner = who;
        obj.zone = ZoneType::BattlefieldZone;
        obj.location = BattlefieldLocation{0};
        return id;
    };
    const auto yordle = place("Lecturing Yordle", PlayerId::Player1);
    const auto recruit = place("Recruit", PlayerId::Player2);
    // Not at the battlefield when the combat starts.
    const auto eager = state.createObject();
    auto& late = state.getObject(eager);
    late.name = "Eager Recruit";
    late.card_type = CardType::Unit;
    late.controller = PlayerId::Player1;
    late.owner = PlayerId::Player1;

    state.turn.turn_number = 2;
    state.turn.turn_player = PlayerId::Player1;

    PlayByPlay chronicle(state, bus);
    bus.emit(CombatStartedEvent{BattlefieldId::fromRaw(0), PlayerId::Player1, PlayerId::Player2});
    // Arrived after the combat started. Still part of who fought.
    bus.emit(ObjectStateChangedEvent{eager, "joined_combat"});

    bus.emit(DamageDealtEvent{recruit, 2, kInvalidId, true});
    bus.emit(DamageDealtEvent{recruit, 1, kInvalidId, true});
    bus.emit(DamageDealtEvent{yordle, 9, kInvalidId, false});
    bus.emit(DamageDealtEvent{yordle, 2, kInvalidId, true});
    bus.emit(CombatEndedEvent{BattlefieldId::fromRaw(0), PlayerId::Player1});

    const std::string text = chronicle.text();
    EXPECT_NE(text.find("Combat at Frozen Fortress: P1's Lecturing Yordle (P1) and Eager Recruit (P1) "
                        "against P2's Recruit (P2). P2's Recruit (P2) takes 3 damage and "
                        "P1's Lecturing Yordle (P1) takes 2 damage."),
              std::string::npos) << text;
    EXPECT_EQ(text.find("Frozen Fortress (P"), std::string::npos) << text;
    EXPECT_EQ(text.find("9"), std::string::npos) << text;
}

// Emissions and mulligan notes inside a quiet stretch are a probe. They are
// not part of the game that was played.
TEST(PlayByPlayText, QuietProbeWritesNothing) {
    GameState state;
    EventBus bus;
    const auto unit = state.createObject();
    auto& yordle = state.getObject(unit);
    yordle.name = "Lecturing Yordle";
    yordle.card_type = CardType::Unit;
    yordle.controller = PlayerId::Player1;
    const auto smoke = state.createObject();
    state.getObject(smoke).name = "Smoke Screen";
    state.player(PlayerId::Player1).hand = {smoke};

    PlayByPlay chronicle(state, bus);
    const auto quiet = bus.beginQuiet();
    bus.emit(PhaseChangedEvent{TurnPhase::Mulligan, TurnPhase::AwakenPhase, PlayerId::Player1});
    bus.emit(CardPlayedEvent{unit, PlayerId::Player1, CardType::Unit, 1});
    bus.emit(PredictEvent{PlayerId::Player1, 1});
    chronicle.noteDecision(state, Intent::mulligan(PlayerId::Player1, {smoke}));
    bus.endQuiet(quiet);

    chronicle.setAccount(sampleAccount());
    chronicle.complete(PlayerId::Player1, 8, 0, 1, "P1 reached 8 points");
    const std::string text = chronicle.text();
    EXPECT_EQ(text.find("Lecturing Yordle"), std::string::npos) << text;
    EXPECT_EQ(text.find("Smoke Screen"), std::string::npos) << text;
    EXPECT_EQ(text.find("plays "), std::string::npos) << text;
    EXPECT_EQ(text.find("Predicts"), std::string::npos) << text;
    EXPECT_NE(text.find("Mulligan: neither player was asked."), std::string::npos) << text;

    bus.emit(PhaseChangedEvent{TurnPhase::Mulligan, TurnPhase::AwakenPhase, PlayerId::Player1});
    bus.notePlayCost(unit, 0, 0, false, false);
    bus.emit(CardPlayedEvent{unit, PlayerId::Player1, CardType::Unit, 1});
    const std::string after = chronicle.text();
    EXPECT_NE(after.find("P1 plays Lecturing Yordle (P1) for free."), std::string::npos) << after;
    EXPECT_EQ(after.find("Accelerate"), std::string::npos) << after;
}

/// Keeps the opening hand, then throws. The mulligan was a real decision.
class KeepThenThrow : public AgentInterface {
public:
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        // Each seat is its own agent, and each is asked to mulligan once.
        // The next decision, after both have kept, is where the game throws.
        for (const auto& action : legal)
            if (action.type == IntentType::MulliganDecision && action.cards_to_mulligan.empty())
                return action;
        throw std::runtime_error("boom after the mulligan");
    }
};

// The block is printed, and the exception still leaves the runner. The game
// is not counted as played.
TEST(PlayByPlayText, AThrowStillPrintsTheGameAndSaysItIsNotUsable) {
    CardRegistry registry;
    registry.loadAll();
    CardDB db;
    db.buildFromClasses(registry);
    const char* env = std::getenv("RIFTBOUND_ROOT");
    const std::string root = env ? env : ".";
    const auto deck1 = DeckValidator::loadFromDeckList(root + "/decks/fiora.txt", db);
    const auto deck2 = DeckValidator::loadFromDeckList(root + "/decks/yi_rebuild.txt", db);

    GameConfig cfg;
    cfg.base_seed = 23;
    cfg.total_games = 1;
    cfg.deck1_name = "fiora";
    cfg.deck2_name = "yi_rebuild";
    cfg.agent1_spec = "keep-then-throw";
    cfg.agent2_spec = "keep-then-throw";
    cfg.agent_factory = [](int, uint64_t) {
        return std::make_unique<KeepThenThrow>();
    };
    AggregateResults results;
    results.total_games = 1;
    GameRunner runner(db, registry, deck1, deck2, cfg, results);

    testing::internal::CaptureStdout();
    std::string what;
    try {
        runner.run();
    } catch (const std::runtime_error& e) {
        what = e.what();
    }
    const std::string out = testing::internal::GetCapturedStdout();

    ASSERT_NE(what.find("game 1 [seed=23]: boom after the mulligan"), std::string::npos) << what;
    EXPECT_NE(out.find("Game 1 [seed=23]: decks fiora vs yi_rebuild, agents keep-then-throw vs "
                       "keep-then-throw."),
              std::string::npos) << out;
    EXPECT_NE(out.find("Ended by a throw. Not usable: boom after the mulligan."), std::string::npos)
        << out;
    EXPECT_NE(out.find("Mulligan:"), std::string::npos) << out;
    EXPECT_NE(out.find("P1 kept "), std::string::npos) << out;
    EXPECT_NE(out.find("P2 kept "), std::string::npos) << out;
    EXPECT_EQ(results.games_completed.load(), 0);
}

// Past 20 games the progress line stays, and a game is not listed.
TEST(PlayByPlayText, ABatchLargerThanTwentyDoesNotListEachGame) {
    CardRegistry registry;
    registry.loadAll();
    CardDB db;
    db.buildFromClasses(registry);
    const char* env = std::getenv("RIFTBOUND_ROOT");
    const std::string root = env ? env : ".";
    const auto deck1 = DeckValidator::loadFromDeckList(root + "/decks/fiora.txt", db);
    const auto deck2 = DeckValidator::loadFromDeckList(root + "/decks/yi_rebuild.txt", db);

    GameConfig cfg;
    cfg.base_seed = 19;
    cfg.total_games = 21;
    cfg.agent_factory = [](int, uint64_t) {
        return std::make_unique<MoveThenStop>();
    };
    AggregateResults results;
    results.total_games = 21;
    GameRunner runner(db, registry, deck1, deck2, cfg, results);

    testing::internal::CaptureStdout();
    runner.run();
    const std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(out.find("Game 1 [seed="), std::string::npos) << out;
    EXPECT_EQ(out.find("Mulligan:"), std::string::npos) << out;
    EXPECT_EQ(out.find("Ended by "), std::string::npos) << out;
    EXPECT_EQ(results.games_completed.load(), 1);
}

// The parenthetical is the controller when the line is written. Owner stays
// P1; after the control change the unit is (P2). A spell, a gear, a rune, a
// legend, and a battlefield are not marked.
TEST(PlayByPlayText, ControlChangeNamesTheNewOwner) {
    GameState state;
    EventBus bus;

    const auto unit = state.createObject();
    auto& stranger = state.getObject(unit);
    stranger.name = "Bedeviling Stranger";
    stranger.card_type = CardType::Unit;
    stranger.owner = PlayerId::Player1;
    stranger.controller = PlayerId::Player1;
    stranger.zone = ZoneType::Base;
    stranger.location = BaseLocation{PlayerId::Player1};

    const auto gear = state.createObject();
    auto& angel = state.getObject(gear);
    angel.name = "Guardian Angel";
    angel.card_type = CardType::Gear;
    angel.owner = PlayerId::Player1;
    angel.controller = PlayerId::Player1;
    angel.zone = ZoneType::Base;
    angel.location = BaseLocation{PlayerId::Player1};

    const auto rune = state.createObject();
    auto& fury = state.getObject(rune);
    fury.name = "Fury Rune";
    fury.card_type = CardType::Rune;
    fury.owner = PlayerId::Player1;
    fury.controller = PlayerId::Player1;
    fury.zone = ZoneType::Base;
    fury.location = BaseLocation{PlayerId::Player1};

    const auto legend = state.createObject();
    auto& heart = state.getObject(legend);
    heart.name = "Heart of the Tempest";
    heart.card_type = CardType::Legend;
    heart.owner = PlayerId::Player2;
    heart.controller = PlayerId::Player2;

    const auto spell = state.createObject();
    auto& discipline = state.getObject(spell);
    discipline.name = "Discipline";
    discipline.card_type = CardType::Spell;
    discipline.controller = PlayerId::Player1;

    const auto field = state.createObject();
    auto& lair = state.getObject(field);
    lair.name = "Vilemaw's Lair";
    lair.card_type = CardType::Battlefield;
    BattlefieldState bf;
    bf.id = BattlefieldId::fromRaw(0);
    bf.card_object_id = field;
    state.battlefields.push_back(bf);

    state.turn.turn_number = 2;
    state.turn.turn_player = PlayerId::Player2;

    PlayByPlay chronicle(state, bus);
    bus.emit(PhaseChangedEvent{TurnPhase::Mulligan, TurnPhase::AwakenPhase, PlayerId::Player2});

    bus.notePlayCost(unit, 2, 0, false, false);
    bus.emit(CardPlayedEvent{unit, PlayerId::Player1, CardType::Unit, 1});
    bus.emit(UnitMovedEvent{unit, PlayerId::Player2, BaseLocation{PlayerId::Player1},
                            BattlefieldLocation{0}, true});

    stranger.controller = PlayerId::Player2;
    EXPECT_EQ(stranger.owner, PlayerId::Player1);

    ChainItem item;
    item.id = ChainItemId::fromRaw(1);
    item.source = spell;
    item.controller = PlayerId::Player1;
    item.is_spell = true;
    item.targets = {unit};
    state.chain.items.push_back(item);
    bus.notePlayCost(spell, 2, 0, false, false);
    bus.emit(ChainItemFinalizedEvent{item.id, spell, PlayerId::Player1});

    bus.emit(UnitMovedEvent{unit, PlayerId::Player2, BattlefieldLocation{0},
                            BaseLocation{PlayerId::Player2}, false});

    bus.notePlayCost(gear, 2, 0, false, false);
    bus.emit(CardPlayedEvent{gear, PlayerId::Player1, CardType::Gear, 2});
    bus.notePlayCost(rune, 0, 0, false, false);
    bus.emit(CardPlayedEvent{rune, PlayerId::Player1, CardType::Rune, 3});
    bus.notePlayCost(legend, 0, 0, false, false);
    bus.emit(CardPlayedEvent{legend, PlayerId::Player2, CardType::Legend, 1});

    state.battlefields[0].controller = PlayerId::Player2;
    bus.emit(ScoreEvent{PlayerId::Player2, BattlefieldId::fromRaw(0), ScoreMethod::Conquer, 1,
                        std::nullopt});

    const std::string text = chronicle.text();
    EXPECT_NE(text.find("P1 plays Bedeviling Stranger (P1) to base for 2 energy."),
              std::string::npos) << text;
    EXPECT_NE(text.find("P2 moves Bedeviling Stranger (P1) from P1's base to Vilemaw's Lair."),
              std::string::npos) << text;
    EXPECT_NE(text.find("P1 plays Discipline on Bedeviling Stranger (P2) for 2 energy."),
              std::string::npos) << text;
    EXPECT_NE(text.find("P2 moves Bedeviling Stranger (P2) from Vilemaw's Lair to base."),
              std::string::npos) << text;
    EXPECT_NE(text.find("P1 plays Guardian Angel to base for 2 energy."), std::string::npos) << text;
    EXPECT_NE(text.find("P1 plays Fury Rune to base for free."), std::string::npos) << text;
    EXPECT_NE(text.find("P2 plays Heart of the Tempest for free."), std::string::npos) << text;
    EXPECT_NE(text.find("P2 conquers Vilemaw's Lair. P2 controls it."), std::string::npos) << text;
    EXPECT_EQ(text.find("Discipline (P"), std::string::npos) << text;
    EXPECT_EQ(text.find("Guardian Angel (P"), std::string::npos) << text;
    EXPECT_EQ(text.find("Fury Rune (P"), std::string::npos) << text;
    EXPECT_EQ(text.find("Heart of the Tempest (P"), std::string::npos) << text;
    EXPECT_EQ(text.find("Vilemaw's Lair (P"), std::string::npos) << text;
    expectUnitMentionsCarryOwner(text);
}

/// Plays Tasty Faefolk through the engine so the account is what a real
/// payment writes, not a note the test filled in itself.
class PlayByPlayEngine : public CardTestFixture {};

TEST_F(PlayByPlayEngine, AccelerateIsNamedWhenItIsPaid) {
    constexpr CardDefId kFaefolk = 75;   // [7], [Accelerate] ([1][G])
    auto fae = addToHand(P1, kFaefolk);
    // Printed 7, plus Accelerate's [1] and one power.
    state.player(P1).rune_pool.energy = 8;
    state.player(P1).rune_pool.universal_power = 1;
    EnginePathAgent pay(EnginePathAgent::script({1}));   // pay Accelerate

    std::string account;
    withEngine([&](GameEngine& e) {
        PlayByPlay chronicle(e.state(), events);
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = fae;
        play.play_location = LocationId{BaseLocation{P1}};
        e.testHook_executeIntent(play);
        settleLikeTheEngine(e);
        account = chronicle.text();
    }, &pay);

    EXPECT_NE(account.find(
                  "P1 plays Tasty Faefolk (P1) to base for 8 energy, 1 power, and Accelerate."),
              std::string::npos) << account;
    EXPECT_FALSE(state.getObject(fae).is_exhausted) << account;
}

TEST_F(PlayByPlayEngine, APlayThatDoesNotPayAccelerateStaysUnchanged) {
    constexpr CardDefId kFaefolk = 75;
    auto fae = addToHand(P1, kFaefolk);
    state.player(P1).rune_pool.energy = 8;
    state.player(P1).rune_pool.universal_power = 1;
    EnginePathAgent decline(EnginePathAgent::script({0}));   // decline Accelerate

    std::string account;
    withEngine([&](GameEngine& e) {
        PlayByPlay chronicle(e.state(), events);
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = fae;
        play.play_location = LocationId{BaseLocation{P1}};
        e.testHook_executeIntent(play);
        settleLikeTheEngine(e);
        account = chronicle.text();
    }, &decline);

    EXPECT_NE(account.find("P1 plays Tasty Faefolk (P1) to base for 7 energy."),
              std::string::npos) << account;
    EXPECT_EQ(account.find("Accelerate"), std::string::npos) << account;
    EXPECT_TRUE(state.getObject(fae).is_exhausted) << account;
}

} // namespace riftbound::test::play_by_play
