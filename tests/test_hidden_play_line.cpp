/// Hide, then the later facedown play.
///
/// CR 811.1.b: on your turn, in an Open State, pay [A] to hide a [Hidden]
/// card at a battlefield you control. Beginning the next turn it has
/// [Reaction] and is played ignoring its base cost (CR 811.6, 422.3).
/// That play is still a play: "When you play me" happens then, not when
/// the card was hidden.

#include "agents/scripted_agent.h"
#include "cards/card_test_fixture.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace riftbound::test {
namespace test_hidden_play_line {
namespace {

constexpr CardDefId kScuttleCrab = 615;        // When you play me, draw 1. Cost [2].
constexpr CardDefId kLotusTrap = 575;          // [Hidden] spell
constexpr CardDefId kRecruit = 266;            // a card to draw
constexpr CardDefId kDeterminedSentry = 673;   // onPlay: I can't move to base.
constexpr CardDefId kConsultThePast = 83;      // [Hidden] [Reaction]. Draw 2. Cost [4].
constexpr CardDefId kMysticVortex = 947;       // Showdowns here: [Reaction] costs [A] more.
constexpr CardDefId kEdgeOfNight = 460;        // [Hidden] gear.
constexpr CardDefId kRakelstake = 1082;        // First unit or gear here each turn: Predict.

class AnswerAgent : public AgentInterface {
public:
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus)
                return i;
        return legal.empty() ? Intent{} : legal.front();
    }
};

class HiddenPlayLineTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    AnswerAgent a1_, a2_;
    GameState& s() { return engine_->mutableState(); }
    GameObject& o(GameObjectId id) { return s().getObject(id); }

    void SetUp() override {
        CardTestFixture::SetUp();
        engine_ = std::make_unique<GameEngine>(card_db, events, card_registry);
        auto& st = s();
        st.mode = ModeOfPlay{};
        st.players[0].id = P1;
        st.players[1].id = P2;
        for (int i = 0; i < 2; ++i) {
            BattlefieldState bf;
            bf.id = BattlefieldId::fromRaw(st.battlefields.size());
            st.battlefields.push_back(bf);
        }
        engine_->testHook_setAgents(&a1_, &a2_);
        st.turn.turn_player = P1;
        st.turn.phase = TurnPhase::MainPhase;
        st.turn.ns_state = NeutralShowdownState::Neutral;
        st.turn.oc_state = OpenClosedState::Open;
        st.turn.turn_number = 1;
        legend(P1);
        legend(P2);
        st.battlefields[0].controller = P1;
    }

    GameObjectId fromDef(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        auto& c = o(id);
        const auto& def = card_db.get(def_id);
        c.owner = c.controller = p;
        c.card_def_id = def_id;
        c.name = def.name;
        c.card_type = def.card_type;
        c.super_type = def.super_type;
        c.tags = def.tags;
        c.domains = def.domains;
        c.base_might = c.current_might = def.might;
        c.keywords = def.keywords;
        return id;
    }
    GameObjectId legend(PlayerId p) {
        auto id = s().createObject();
        auto& lo = o(id);
        lo.owner = lo.controller = p;
        lo.card_type = CardType::Legend;
        lo.zone = ZoneType::LegendZone;
        s().player(p).legend_zone = id;
        return id;
    }
    GameObjectId inHand(PlayerId p, CardDefId def_id) {
        auto id = fromDef(p, def_id);
        o(id).zone = ZoneType::Hand;
        s().player(p).hand.push_back(id);
        return id;
    }
    GameObjectId onDeck(PlayerId p, CardDefId def_id) {
        auto id = fromDef(p, def_id);
        o(id).zone = ZoneType::MainDeck;
        s().player(p).main_deck.push_back(id);
        return id;
    }
    GameObjectId rune(PlayerId p, Domain d, bool exhausted = false) {
        auto id = s().createObject();
        auto& r = o(id);
        r.owner = r.controller = p;
        r.card_type = CardType::Rune;
        r.name = "Rune";
        r.domains = {d};
        r.zone = ZoneType::Base;
        r.location = BaseLocation{p};
        r.is_exhausted = exhausted;
        return id;
    }
    bool inRuneDeck(PlayerId p, GameObjectId rune_id) {
        const auto& d = s().player(p).rune_deck;
        return std::find(d.begin(), d.end(), rune_id) != d.end() &&
               o(rune_id).zone == ZoneType::RuneDeck;
    }
    bool facedownAt(GameObjectId card, int bf) {
        const auto& f = s().battlefields[bf].facedown;
        return std::find(f.begin(), f.end(), card) != f.end();
    }
    /// A unit of the controller, so an Open State does not drop the battlefield
    /// (CR 323.5) and take the facedown card with it (CR 323.6.a).
    GameObjectId battlefieldCard(int bf, CardDefId def_id) {
        auto id = fromDef(PlayerId::None, def_id);
        o(id).zone = ZoneType::BattlefieldZone;
        s().battlefields[bf].card_object_id = id;
        return id;
    }
    GameObjectId hold(PlayerId p, int bf) {
        auto id = s().createObject();
        auto& u = o(id);
        u.owner = u.controller = p;
        u.card_type = CardType::Unit;
        u.name = "Holder";
        u.base_might = u.current_might = 1;
        u.zone = ZoneType::BattlefieldZone;
        u.location = BattlefieldLocation{BattlefieldId::fromRaw(bf)};
        s().battlefields[bf].controller = p;
        return id;
    }
    /// A card hidden on an earlier turn, so the next turn may play it.
    GameObjectId hiddenEarlier(PlayerId owner, CardDefId def, int bf) {
        const GameObjectId card = fromDef(owner, def);
        o(card).zone = ZoneType::FacedownZone;
        o(card).is_hidden = true;
        o(card).hidden_at = BattlefieldId::fromRaw(bf);
        o(card).hidden_on_turn = s().turn.turn_number;
        s().battlefields[bf].facedown.push_back(card);
        s().battlefields[bf].controller = owner;
        return card;
    }
    const Intent* findPlay(const std::vector<Intent>& legal, GameObjectId card) {
        for (const auto& i : legal)
            if (i.card == card && (i.type == IntentType::PlayReaction ||
                                   i.type == IntentType::PlayActionCard ||
                                   i.type == IntentType::PlayCard))
                return &i;
        return nullptr;
    }
};

// CR 811.1.b / 422.1: hiding pays one Power, by recycling a rune, and the
// card is facedown at the battlefield. The card's on-play effect does not
// happen: hiding is not a play (CR 811.1.c.1).
TEST_F(HiddenPlayLineTest, HideRecyclesOnePowerAndDoesNotPlay) {
    hold(P1, 0);
    auto card = inHand(P1, kLotusTrap);
    auto top = onDeck(P1, kRecruit);
    auto power = rune(P1, Domain::Calm, /*exhausted=*/true);
    const int deck_before = static_cast<int>(s().player(P1).main_deck.size());

    const auto legal = engine_->generateLegalActions();
    const Intent* hide = nullptr;
    for (const auto& i : legal)
        if (i.type == IntentType::HideCard && i.card == card) hide = &i;
    ASSERT_NE(hide, nullptr);

    engine_->testHook_executeIntent(*hide);
    settleLikeTheEngine(*engine_);

    EXPECT_TRUE(o(card).is_hidden);
    EXPECT_EQ(o(card).zone, ZoneType::FacedownZone);
    EXPECT_TRUE(facedownAt(card, 0));
    EXPECT_EQ(std::find(s().player(P1).hand.begin(), s().player(P1).hand.end(), card),
              s().player(P1).hand.end());
    EXPECT_TRUE(inRuneDeck(P1, power));
    EXPECT_FALSE(o(power).is_exhausted);
    EXPECT_EQ(s().player(P1).power_spent_this_turn, 1);
    EXPECT_EQ(s().player(P1).draws_this_turn, 0);
    EXPECT_EQ(static_cast<int>(s().player(P1).main_deck.size()), deck_before);
    EXPECT_EQ(s().player(P1).main_deck.back(), top);
}

// The turn after it was hidden, the card is a Reaction (CR 811.6) and the
// play ignores the printed Energy (CR 811.1.b). Scuttle Crab's "When you
// play me, draw 1" happens on that play. Hiding did not draw.
TEST_F(HiddenPlayLineTest, FacedownScuttleCrabDrawsAtTheNextReactionWindowForFree) {
    auto crab = hiddenEarlier(P1, kScuttleCrab, 0);
    auto top = onDeck(P1, kRecruit);
    auto energy_a = rune(P1, Domain::Calm);
    auto energy_b = rune(P1, Domain::Calm);
    // Printed cost is [2]. These two runes would pay it from hand. The
    // facedown play must leave them where they are.
    s().turn.turn_number = 2;
    s().turn.turn_player = P2;
    s().turn.oc_state = OpenClosedState::Closed;
    s().turn.priority_holder = P1;

    const auto legal = engine_->generateLegalActions();
    const Intent* play = findPlay(legal, crab);
    ASSERT_NE(play, nullptr) << "playable at the reaction window on the next turn";
    EXPECT_EQ(play->type, IntentType::PlayReaction);

    engine_->testHook_executeIntent(*play);
    settleLikeTheEngine(*engine_);

    EXPECT_FALSE(o(crab).is_hidden);
    EXPECT_FALSE(facedownAt(crab, 0));
    ASSERT_TRUE(o(crab).location.has_value());
    EXPECT_EQ(o(crab).zone, ZoneType::BattlefieldZone);
    EXPECT_EQ(s().player(P1).draws_this_turn, 1);
    EXPECT_NE(std::find(s().player(P1).hand.begin(), s().player(P1).hand.end(), top),
              s().player(P1).hand.end());
    EXPECT_TRUE(o(energy_a).location.has_value());
    EXPECT_TRUE(o(energy_b).location.has_value());
    EXPECT_FALSE(o(energy_a).is_exhausted);
    EXPECT_FALSE(o(energy_b).is_exhausted);
    EXPECT_EQ(s().player(P1).power_spent_this_turn, 0);
    EXPECT_FALSE(inRuneDeck(P1, energy_a));
    EXPECT_FALSE(inRuneDeck(P1, energy_b));
}

// A play from hand runs Card::onPlay before the permanent hits the chain
// (executePlayCard). A play from facedown is the same play: Determined
// Sentry's "I can't move to base" is that hook, and the flip sets it.
// Scuttle Crab's draw, above, is the triggered "When you play me".
TEST_F(HiddenPlayLineTest, FacedownPlayRunsOnPlayTheWayAHandPlayDoes) {
    auto sentry = hiddenEarlier(P1, kDeterminedSentry, 0);
    s().turn.turn_number = 2;

    const Intent* play = findPlay(engine_->generateLegalActions(), sentry);
    ASSERT_NE(play, nullptr);
    engine_->testHook_executeIntent(*play);
    settleLikeTheEngine(*engine_);

    EXPECT_TRUE(o(sentry).location.has_value());
    EXPECT_TRUE(o(sentry).cant_move_to_base);
}

// Mystic Vortex: during a showdown here, a [Reaction] costs [A] more.
// Ignoring the printed Energy does not skip that increase (CR 356.1.b.3).
// Consult the Past from facedown draws 2 and recycles the one ready rune.
// With no Power at all the play is not offered.
TEST_F(HiddenPlayLineTest, FacedownConsultThePastPaysTheVortexShowdownTax) {
    battlefieldCard(0, kMysticVortex);
    hold(P1, 0);
    s().turn.ns_state = NeutralShowdownState::Showdown;
    s().turn.oc_state = OpenClosedState::Open;
    s().turn.focus_holder = P1;
    s().battlefields[0].showdown_in_progress = true;
    engine_->testHook_cleanup();
    ASSERT_EQ(s().battlefields[0].reaction_tax_power, 1);

    // Hidden on an earlier turn, so this turn it may be played.
    auto spell = hiddenEarlier(P1, kConsultThePast, 0);
    s().turn.turn_number = 2;
    onDeck(P1, kRecruit);
    onDeck(P1, kRecruit);
    EXPECT_EQ(findPlay(engine_->generateLegalActions(), spell), nullptr)
        << "no Power: the [A] increase is not offered";

    auto power = rune(P1, Domain::Fury);
    const auto legal = engine_->generateLegalActions();
    const Intent* play = findPlay(legal, spell);
    ASSERT_NE(play, nullptr) << "one ready rune pays the [A]";

    engine_->testHook_executeIntent(*play);
    settleLikeTheEngine(*engine_);

    EXPECT_EQ(s().player(P1).draws_this_turn, 2);
    EXPECT_EQ(static_cast<int>(s().player(P1).hand.size()), 2);
    EXPECT_TRUE(inRuneDeck(P1, power));
    EXPECT_FALSE(o(power).is_exhausted);
    EXPECT_EQ(s().player(P1).power_spent_this_turn, 1);
    EXPECT_FALSE(o(spell).is_hidden);
}

// Rakelstake: the first gear played here this turn Predicts. A hidden gear
// is played to the battlefield it was hidden at, and that is where the play
// is announced. Cleanup must not recall it to base before that announcement.
TEST_F(HiddenPlayLineTest, FacedownEdgeOfNightIsAnnouncedAtRakelstake) {
    auto stake = battlefieldCard(0, kRakelstake);
    auto unit = hold(P1, 0);
    auto gear = hiddenEarlier(P1, kEdgeOfNight, 0);
    onDeck(P1, kRecruit);
    s().turn.turn_number = 2;
    s().turn.oc_state = OpenClosedState::Closed;
    s().turn.priority_holder = P1;

    int played_events = 0;
    std::optional<BattlefieldId> announced;
    auto played = events.on_card_played.connect([&](const CardPlayedEvent& e) {
        ++played_events;
        if (e.object == gear && s().objectExists(gear))
            announced = s().getObject(gear).battlefieldId();
    });
    int predicts = 0;
    auto looked = events.on_predicted.connect([&](const PredictEvent& e) {
        if (e.player == P1) ++predicts;
    });

    const auto legal = engine_->generateLegalActions();
    const Intent* play = findPlay(legal, gear);
    ASSERT_NE(play, nullptr);
    engine_->testHook_executeIntent(*play);
    settleLikeTheEngine(*engine_);

    auto stamp = o(stake).card_counters.find("__rakel_p1");
    ASSERT_NE(stamp, o(stake).card_counters.end())
        << "played_events=" << played_events
        << " predicts=" << predicts
        << " attached=" << o(gear).attached_to.has_value()
        << " zone=" << static_cast<int>(o(gear).zone.get());
    EXPECT_EQ(stamp->second, s().turn.turn_number + 1);
    EXPECT_EQ(predicts, 1);
    ASSERT_TRUE(announced.has_value());
    EXPECT_EQ(*announced, BattlefieldId::fromRaw(0));
    EXPECT_EQ(o(gear).attached_to, unit);
    (void)played;
    (void)looked;
}

// The same turn it is hidden, it is not yet a Reaction (CR 811.1.b).
TEST_F(HiddenPlayLineTest, NotPlayableTheTurnItIsHidden) {
    auto crab = hiddenEarlier(P1, kScuttleCrab, 0);
    s().turn.oc_state = OpenClosedState::Closed;
    s().turn.priority_holder = P1;
    EXPECT_EQ(findPlay(engine_->generateLegalActions(), crab), nullptr);
}

// With a [Hidden] card, a rune to recycle, and a battlefield, and nothing
// else to play, the agent hides it.
TEST_F(HiddenPlayLineTest, AgentHidesWhenThatIsThePlay) {
    auto card = inHand(P1, kLotusTrap);
    rune(P1, Domain::Fury, /*exhausted=*/true);

    const auto legal = engine_->generateLegalActions();
    bool hide_offered = false;
    for (const auto& i : legal) {
        if (i.type == IntentType::HideCard && i.card == card) hide_offered = true;
        EXPECT_NE(i.type, IntentType::PlayCard);
        EXPECT_NE(i.type, IntentType::StandardMove);
    }
    ASSERT_TRUE(hide_offered);

    ScriptedAgent agent(card_db);
    const Intent chosen = agent.selectAction(s(), legal);
    EXPECT_EQ(chosen.type, IntentType::HideCard);
    EXPECT_EQ(chosen.card, card);
}

}  // namespace
}  // namespace test_hidden_play_line
}  // namespace riftbound::test
