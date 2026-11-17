/// The short account printed after a game. A move that drops its origin or
/// its destination fails, and so does a short game whose summary was not printed.

#include <gtest/gtest.h>

#include "agents/agent_interface.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_runner.h"
#include "io/play_by_play.h"
#include "rules/deck_validator.h"

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
    bus.emit(ChainItemFinalizedEvent{item.id, spell, PlayerId::Player1});

    state.players[0].score = 1;
    const std::string text = chronicle.text();

    EXPECT_NE(text.find("Turn 1 - P1"), std::string::npos) << text;
    EXPECT_NE(text.find("P1 moves Lecturing Yordle from base to Frozen Fortress."),
              std::string::npos) << text;
    EXPECT_NE(text.find("P1 conquers Frozen Fortress. P1 controls it."), std::string::npos) << text;
    EXPECT_NE(text.find("P1 plays Smoke Screen on Lecturing Yordle."), std::string::npos) << text;
    EXPECT_NE(text.find("Score 1-0."), std::string::npos) << text;

    std::istringstream lines(text);
    std::string line;
    int moves = 0;
    while (std::getline(lines, line)) {
        if (line.find(" moves ") == std::string::npos) continue;
        ++moves;
        EXPECT_TRUE(moveNamesPlaces(line)) << line;
    }
    EXPECT_EQ(moves, 1) << text;
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
    cfg.total_games = 1;
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

    const auto game = out.find("Game 1 [seed=19]:");
    const auto turn = out.find("Turn 1 - ");
    ASSERT_NE(game, std::string::npos) << out;
    ASSERT_NE(turn, std::string::npos) << out;
    EXPECT_LT(game, turn) << out;
    EXPECT_NE(out.find(" wins "), std::string::npos) << out;
    EXPECT_NE(out.find("\n  Score "), std::string::npos) << out;
    EXPECT_EQ(out.find("[TRC]"), std::string::npos) << out;

    std::istringstream lines(out);
    std::string line;
    int moves = 0;
    while (std::getline(lines, line)) {
        if (line.find(" moves ") == std::string::npos) continue;
        ++moves;
        EXPECT_TRUE(moveNamesPlaces(line)) << line;
    }
    EXPECT_GE(moves, 1) << out;
}

} // namespace riftbound::test::play_by_play
