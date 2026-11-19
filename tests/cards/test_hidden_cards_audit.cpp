/// The boards from the hidden-card audit. Each one changes a game if the
/// rule is missed: a facedown choice outside its battlefield, a cost increase
/// skipped, a granted [Repeat] or a spell-damage rider left for a later spell,
/// a trigger queued for a play from hand, a facedown card returned unseen, an
/// occupancy cap that does not trash, or Hide withheld in a Showdown Open state.

#include "tests/cards/card_test_fixture.h"
#include "engine/knowledge_tracker.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace riftbound::test::hidden_cards_audit {
namespace {

using riftbound::test::CardTestFixture;
using riftbound::test::EnginePathAgent;

class HiddenCardsAuditTest : public CardTestFixture {
protected:
    GameObject& o(GameObjectId id) { return state.getObject(id); }
    void hideAt(GameObjectId card, int bf) {
        auto& hand = state.player(o(card).owner).hand;
        hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
        o(card).zone = ZoneType::FacedownZone;
        o(card).is_hidden = true;
        o(card).hidden_at = BattlefieldId::fromRaw(bf);
        o(card).hidden_on_turn = state.turn.turn_number - 1;
        state.battlefields[bf].facedown.push_back(card);
    }
    void setPool(PlayerId p, int energy, int power) {
        state.player(p).rune_pool = RunePool{};
        state.player(p).rune_pool.energy = energy;
        state.player(p).rune_pool.universal_power = power;
    }
    void battlefieldCard(int bf, CardDefId def) {
        auto id = state.createObject();
        auto& b = o(id);
        b.card_type = CardType::Battlefield;
        b.card_def_id = def;
        b.name = card_db.get(def).name;
        b.zone = ZoneType::BattlefieldZone;
        state.battlefields[bf].card_object_id = id;
    }
    void reactionWindow() {
        pushSpellOnChain(P2);
        state.chain.items.back().status = ChainItemStatus::Finalized;
        state.turn.phase = TurnPhase::MainPhase;
        state.turn.oc_state = OpenClosedState::Closed;
        state.turn.priority_holder = P1;
        state.turn.turn_number = 2;
    }
    /// Apply continuous effects (a battlefield's [Reaction] tax, a gear's
    /// spell tax) before a generator or a payment reads them.
    void settle() {
        withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    }
    void playHidden(GameObjectId card, AgentInterface* agent,
                    std::vector<GameObjectId> targets = {}) {
        Intent i;
        i.type = IntentType::PlayReaction;
        i.player = o(card).controller;
        i.card = card;
        i.targets = std::move(targets);
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(i); }, agent, nullptr, i.player);
    }
    bool offered(GameObjectId card) {
        std::vector<Intent> legal;
        withEngine([&](GameEngine& e) { legal = e.generateLegalActions(); }, nullptr, nullptr, P1);
        for (const auto& i : legal)
            if (i.card == card && (i.type == IntentType::PlayCard ||
                                   i.type == IntentType::PlayActionCard ||
                                   i.type == IntentType::PlayReaction))
                return true;
        return false;
    }
};

TEST_F(HiddenCardsAuditTest, DemonShade_FacedownChoosesOnlyAFriendlyUnitThere) {
    state.battlefields[0].controller = P1;
    state.turn.turn_number = 2;
    auto here = addUnit(P1, kInvalidId, 2, 0);
    auto away = addUnit(P1, kInvalidId, 2, -1);
    auto shade = addToHand(P1, 1110);
    hideAt(shade, 0);
    reactionWindow();
    EXPECT_TRUE(offered(shade));
    EnginePathAgent agent;
    playHidden(shade, &agent);
    EXPECT_FALSE(agent.wasOffered(away)) << "CR 811.1.d.2: not a unit at another location";
    EXPECT_TRUE(agent.wasOffered(here));
}

TEST_F(HiddenCardsAuditTest, DemonShade_IsNotPlayedFromFacedownWithNoFriendlyUnitThere) {
    state.battlefields[0].controller = P1;
    state.turn.turn_number = 2;
    addUnit(P1, kInvalidId, 2, -1);
    auto shade = addToHand(P1, 1110);
    hideAt(shade, 0);
    reactionWindow();
    EXPECT_FALSE(offered(shade));
}

TEST_F(HiddenCardsAuditTest, ElnukHerd_FacedownBuffsOnlyAFriendlyUnitThere) {
    state.battlefields[0].controller = P1;
    state.turn.turn_number = 2;
    auto here = addUnit(P1, kInvalidId, 2, 0);
    auto away = addUnit(P1, kInvalidId, 2, -1);
    auto herd = addToHand(P1, 1017);
    hideAt(herd, 0);
    reactionWindow();
    EnginePathAgent agent;
    playHidden(herd, &agent);
    EXPECT_FALSE(agent.wasOffered(away));
    EXPECT_EQ(o(here).current_might, 5);
    EXPECT_EQ(o(away).current_might, 2);
    EXPECT_NE(o(herd).zone, ZoneType::BattlefieldZone) << "played from facedown, he is recycled";
}

TEST_F(HiddenCardsAuditTest, BedevilingStranger_FacedownChoosesOnlyHerBattlefield) {
    state.battlefields[0].controller = P1;
    state.turn.turn_number = 2;
    addUnit(P1, kInvalidId, 1, 0);
    auto enemy = addUnit(P2, kInvalidId, 1, 1);
    auto stranger = addToHand(P1, 1036);
    hideAt(stranger, 0);
    reactionWindow();
    EnginePathAgent agent;
    playHidden(stranger, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    for (const auto& opt : agent.prompts.front())
        EXPECT_NE(opt.chosen_value, 1) << "CR 811.1.d.3: not the other battlefield";
    EXPECT_EQ(o(enemy).zone, ZoneType::BattlefieldZone);
}

TEST_F(HiddenCardsAuditTest, BandleTree_LeavingTrashesTheExtraFacedownCard) {
    battlefieldCard(0, 273);
    state.battlefields[0].controller = P1;
    state.turn.turn_number = 2;
    addUnit(P1, kInvalidId, 1, 0);
    auto a = addToHand(P1, 83);
    auto b = addToHand(P1, 213);
    hideAt(a, 0);
    hideAt(b, 0);
    int reveals = 0;
    auto conn = events.on_card_revealed.connect([&](const CardRevealedEvent& e) {
        if (e.revealed_to_all && e.source_zone == ZoneType::FacedownZone) ++reveals;
    });
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(state.battlefields[0].facedown.size(), 2u) << "the tree holds the cap at 2";
    state.battlefields[0].card_object_id = kInvalidId;
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(state.battlefields[0].facedown.size(), 1u);
    EXPECT_EQ(reveals, 1) << "CR 422.4: the trashed card is revealed";
    const bool a_trashed = o(a).zone == ZoneType::Trash;
    const bool b_trashed = o(b).zone == ZoneType::Trash;
    EXPECT_NE(a_trashed, b_trashed);
    conn.disconnect();
}

TEST_F(HiddenCardsAuditTest, FacedownSpell_PaysAReactionTax) {
    battlefieldCard(0, 947);
    state.battlefields[0].controller = P1;
    state.battlefields[0].showdown_in_progress = true;
    state.turn.turn_number = 2;
    addUnit(P1, kInvalidId, 1, 0);
    auto spell = addToHand(P1, 83);
    hideAt(spell, 0);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    reactionWindow();
    settle();
    setPool(P1, 0, 0);
    EXPECT_FALSE(offered(spell)) << "the [A] increase cannot be paid";
    setPool(P1, 0, 1);
    EXPECT_TRUE(offered(spell));
    playHidden(spell, nullptr);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
    EXPECT_NE(o(spell).zone, ZoneType::FacedownZone);
}

TEST_F(HiddenCardsAuditTest, FacedownSpell_PaysAnOpponentsSpellTax) {
    auto helm = addToHand(P2, 832);
    state.player(P2).hand.clear();
    o(helm).zone = ZoneType::Base;
    o(helm).location = LocationId{BaseLocation{P2}};
    state.battlefields[0].controller = P1;
    state.turn.turn_number = 2;
    addUnit(P1, kInvalidId, 1, 0);
    auto spell = addToHand(P1, 83);
    hideAt(spell, 0);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    reactionWindow();
    settle();
    setPool(P1, 0, 0);
    EXPECT_FALSE(offered(spell));
    setPool(P1, 1, 0);
    EXPECT_TRUE(offered(spell));
    playHidden(spell, nullptr);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

TEST_F(HiddenCardsAuditTest, GrantedRepeat_IsPaidOnAFacedownSpell) {
    state.battlefields[0].controller = P1;
    state.turn.turn_number = 2;
    addUnit(P1, kInvalidId, 1, 0);
    for (int n = 0; n < 4; ++n) addToDeck(P1, kInvalidId);
    auto spell = addToHand(P1, 83);
    hideAt(spell, 0);
    reactionWindow();
    state.player(P1).grant_repeat_base_to_next_spell = true;
    setPool(P1, 4, 0);
    EnginePathAgent agent([](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.chosen_value && *i.chosen_value == 1) return i;
        return legal.front();
    });
    playHidden(spell, &agent);
    EXPECT_FALSE(state.player(P1).grant_repeat_base_to_next_spell);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0) << "the granted [Repeat] was paid";
    EXPECT_EQ(state.player(P1).hand.size(), 4u) << "draw 2, then the repeat draws 2";
}

TEST_F(HiddenCardsAuditTest, RavenbornRider_BindsToAFacedownSpell) {
    state.battlefields[0].controller = P1;
    state.turn.turn_number = 2;
    auto unit = addUnit(P2, kInvalidId, 3, 0);
    auto spell = addToHand(P1, 340);
    hideAt(spell, 0);
    reactionWindow();
    state.player(P1).next_spell_bonus_damage = 1;
    playHidden(spell, nullptr, {unit});
    EXPECT_EQ(state.player(P1).next_spell_bonus_damage, 0);
    EXPECT_EQ(o(unit).zone, ZoneType::Trash) << "2 damage plus the rider is lethal";
}

TEST_F(HiddenCardsAuditTest, Evelynn_DoesNotTriggerOnAPlayFromHand) {
    auto eve = addUnit(P1, 1024, 0, 0);
    o(eve).is_exhausted = true;
    const int might = o(eve).current_might;
    int triggers = 0;
    auto conn = events.on_log.connect([&](const LogEvent& e) {
        if (e.message.find("Evelynn, Consuming") != std::string::npos &&
            e.message.find("fires ability") != std::string::npos)
            ++triggers;
    });
    auto unit = addToHand(P1, kInvalidId);
    o(unit).card_type = CardType::Unit;
    playThroughEngine(unit, nullptr, LocationId{BaseLocation{P1}}, nullptr, false);
    EXPECT_EQ(triggers, 0);
    EXPECT_TRUE(o(eve).is_exhausted);
    EXPECT_EQ(o(eve).current_might, might);
    conn.disconnect();
}

TEST_F(HiddenCardsAuditTest, Evelynn_TriggersOnAPlayFromFacedown) {
    state.battlefields[0].controller = P1;
    state.turn.turn_number = 2;
    auto eve = addUnit(P1, 1024, 0, 0);
    o(eve).is_exhausted = true;
    const int might = o(eve).current_might;
    addUnit(P1, kInvalidId, 1, 0);
    auto spell = addToHand(P1, 83);
    hideAt(spell, 0);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    reactionWindow();
    playHidden(spell, nullptr);
    EXPECT_FALSE(o(eve).is_exhausted);
    EXPECT_EQ(o(eve).current_might, might + 1);
}

TEST_F(HiddenCardsAuditTest, PackOfWonders_RevealsAFacedownCardItReturns) {
    state.battlefields[0].controller = P1;
    state.turn.turn_number = 2;
    addUnit(P1, kInvalidId, 1, 0);
    auto pack = addToHand(P1, 181);
    state.player(P1).hand.clear();
    o(pack).zone = ZoneType::Base;
    o(pack).location = LocationId{BaseLocation{P1}};
    auto hidden = addToHand(P1, 213);
    hideAt(hidden, 0);
    int reveals = 0;
    auto conn = events.on_card_revealed.connect([&](const CardRevealedEvent& e) {
        if (e.card == hidden && e.revealed_to_all) ++reveals;
    });
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = pack;
    EnginePathAgent agent(EnginePathAgent::choosing(hidden));
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(act); }, &agent);
    EXPECT_EQ(reveals, 1);
    EXPECT_EQ(o(hidden).zone, ZoneType::Hand);
    EXPECT_TRUE(knowledge::identityVisible(state, hidden, P2));
    conn.disconnect();
}

TEST_F(HiddenCardsAuditTest, Ava_PaysAReactionTaxWhenSheIgnoresTheCost) {
    battlefieldCard(0, 947);
    state.battlefields[0].controller = P1;
    state.battlefields[0].showdown_in_progress = true;
    auto ava = addUnit(P1, 107, 0, 0);
    auto spell = addToHand(P1, 83);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    setPool(P1, 0, 2);
    settle();
    setPool(P1, 0, 2);
    EnginePathAgent agent(EnginePathAgent::accepting());
    fireTriggerThroughEngine(ava, TriggerType::WhenIAttack, &agent);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0)
        << "[C] and the showdown's [A] are both paid";
    EXPECT_NE(o(spell).zone, ZoneType::Hand);
}

TEST_F(HiddenCardsAuditTest, Hide_IsOfferedInAShowdownOpenStateOnYourTurn) {
    state.turn.phase = TurnPhase::MainPhase;
    state.turn.turn_player = P1;
    state.turn.ns_state = NeutralShowdownState::Showdown;
    state.turn.oc_state = OpenClosedState::Open;
    state.turn.focus_holder = P1;
    state.battlefields[0].controller = P1;
    state.battlefields[0].showdown_in_progress = true;
    addUnit(P1, kInvalidId, 1, 0);
    auto card = addToHand(P1, 83);
    setPool(P1, 0, 1);
    std::vector<Intent> legal;
    withEngine([&](GameEngine& e) { legal = e.generateLegalActions(); });
    const auto hide_it = std::find_if(legal.begin(), legal.end(), [&](const Intent& i) {
        return i.type == IntentType::HideCard && i.card == card;
    });
    ASSERT_NE(hide_it, legal.end());
    const Intent hide = *hide_it;

    state.turn.turn_player = P2;
    state.turn.focus_holder = P1;
    legal.clear();
    withEngine([&](GameEngine& e) { legal = e.generateLegalActions(); });
    EXPECT_FALSE(std::any_of(legal.begin(), legal.end(), [](const Intent& i) {
        return i.type == IntentType::HideCard;
    })) << "CR 811.1.b: on your turn, not when you merely have Focus";

    state.turn.turn_player = P1;
    state.turn.oc_state = OpenClosedState::Closed;
    state.turn.priority_holder = P1;
    legal.clear();
    withEngine([&](GameEngine& e) { legal = e.generateLegalActions(); });
    EXPECT_FALSE(std::any_of(legal.begin(), legal.end(), [](const Intent& i) {
        return i.type == IntentType::HideCard;
    }));

    state.turn.oc_state = OpenClosedState::Open;
    state.turn.ns_state = NeutralShowdownState::Showdown;
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(hide); });
    EXPECT_TRUE(o(card).is_hidden);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
    EXPECT_NE(std::find(state.battlefields[0].facedown.begin(),
                        state.battlefields[0].facedown.end(), card),
              state.battlefields[0].facedown.end());
}

}  // namespace
}  // namespace riftbound::test::hidden_cards_audit
