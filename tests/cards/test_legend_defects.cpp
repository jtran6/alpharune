// The five legend defects from the legend audit.
// Boy Who Shattered Time: the kill ends with the turn, and an end-of-turn
// ability is not queued when there is no unit to kill.
// The Exile: a unit token played to a battlefield that player controls enters
// ready. The line does not reduce energy.
// Pride of Nazumah: Assault equals Shield this turn, including 0, and follows
// a later Shield change.
// Iron Revenant: the ability is not offered, and the legend is not exhausted,
// when the trash has no unit or gear.
// Agony's Embrace: the ability is queued only for a facedown play, and only
// once each turn. A hand play does not open priority for it.

#include "tests/cards/card_test_fixture.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>

namespace riftbound::test {
namespace test_legend_defects {

class LegendDefects : public CardTestFixture {
protected:
    GameObjectId legend(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.card_def_id = def_id;
        obj.name = card_db.get(def_id).name;
        obj.card_type = CardType::Legend;
        obj.owner = owner;
        obj.controller = owner;
        obj.zone = ZoneType::LegendZone;
        state.player(owner).legend_zone = id;
        return id;
    }

    void toTrash(PlayerId owner, GameObjectId card) {
        auto& hand = state.player(owner).hand;
        hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
        state.getObject(card).zone = ZoneType::Trash;
        state.getObject(card).location.reset();
        state.player(owner).trash.push_back(card);
    }

    GameObjectId hide(PlayerId owner, CardDefId def_id, int bf, int on_turn) {
        auto card = addToHand(owner, def_id);
        auto& hand = state.player(owner).hand;
        hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
        auto& c = state.getObject(card);
        c.zone = ZoneType::BattlefieldZone;
        c.is_hidden = true;
        c.hidden_at = BattlefieldId::fromRaw(bf);
        c.hidden_on_turn = on_turn;
        state.battlefields[bf].facedown.push_back(card);
        state.battlefields[bf].controller = owner;
        return card;
    }

    bool activationOffered(PlayerId who, GameObjectId source) {
        bool yes = false;
        withEngine([&](GameEngine& e) {
            for (const auto& i : e.testHook_mainPhaseActions(who))
                if ((i.type == IntentType::ActivateAbility ||
                     i.type == IntentType::ActivateReactionAbility) &&
                    i.ability_source == source)
                    yes = true;
        }, nullptr, nullptr, who);
        return yes;
    }

    // A chain item whose source is the legend is an ability of that legend.
    // Finalizing it is the Closed State both players get priority in.
    int legendItemsFinalized(GameObjectId source, const std::function<void()>& body) {
        int n = 0;
        auto conn = events.on_chain_item_finalized.connect(
            [&](const ChainItemFinalizedEvent& ev) {
                if (ev.source == source) ++n;
            });
        body();
        conn.disconnect();
        return n;
    }

    void activateBoy(GameObjectId boy) {
        state.getObject(boy).empowered = 1;
        state.turn.turn_number = 1;
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = P1;
        act.ability_source = boy;
        act.ability_index = 1;
        withEngine([&](GameEngine& e) {
            e.testHook_executeIntent(act);
            settleLikeTheEngine(e);
        });
    }
};

TEST_F(LegendDefects, BoyKillEndsWithTheTurnAndNoEmptyEndOfTurnAbility) {
    auto boy = legend(P1, 1062);
    activateBoy(boy);
    ASSERT_FALSE(state.player(P1).cost_modifiers.empty());
    EXPECT_EQ(state.player(P1).cost_modifiers.back().energy_reduction, 2);
    EXPECT_EQ(state.player(P1).cost_modifiers.back().power_reduction, 1);

    // No unit was played. The Ending Step must not put an ability on the chain.
    const int empty = legendItemsFinalized(boy, [&] {
        withEngine([&](GameEngine& e) { e.testHook_endingStep(); });
    });
    EXPECT_EQ(empty, 0);

    withEngine([&](GameEngine& e) { e.testHook_expirationStep(); });
    state.turn.turn_number = 2;
    state.turn.phase = TurnPhase::MainPhase;
    state.turn.turn_player = P1;
    state.player(P1).resetTurnTracking();
    state.player(P2).resetTurnTracking();

    auto later = addToHand(P1, 13);
    const int queued = legendItemsFinalized(boy, [&] { playThroughEngine(later); });
    EXPECT_EQ(queued, 0);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    ASSERT_TRUE(state.getObject(later).location.has_value());
    withEngine([&](GameEngine& e) { e.testHook_endingStep(); });
    EXPECT_TRUE(state.getObject(later).location.has_value());
    EXPECT_FALSE(inTrash(P1, later));
}

TEST_F(LegendDefects, BoyKillsTheUnitPlayedThisTurn) {
    auto boy = legend(P1, 1062);
    activateBoy(boy);
    auto unit = addToHand(P1, 13);
    playThroughEngine(unit);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 2);
    ASSERT_TRUE(state.getObject(unit).location.has_value());
    withEngine([&](GameEngine& e) { e.testHook_endingStep(); });
    EXPECT_FALSE(state.getObject(unit).location.has_value());
    EXPECT_TRUE(inTrash(P1, unit));
}

TEST_F(LegendDefects, ExileReadiesAUnitTokenAtABattlefieldYouControl) {
    legend(P1, 1068);
    addUnit(P1, 13, 0, 0);
    addUnit(P2, 13, 0, 1);
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P2;
    GameObjectId at_yours = kInvalidId;
    GameObjectId at_base = kInvalidId;
    GameObjectId at_theirs = kInvalidId;
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        ASSERT_TRUE(e.mutableState().player(P1).units_enter_ready_at_your_bf);
        auto& ex = e.testHook_executor();
        at_yours = ex.createToken(P1, CardType::Unit, "Recruit", 1, {"Recruit"}, {},
                                  BattlefieldLocation{BattlefieldId::fromRaw(0)}, false);
        at_base = ex.createToken(P1, CardType::Unit, "Recruit", 1, {"Recruit"}, {},
                                 BaseLocation{P1}, false);
        at_theirs = ex.createToken(P1, CardType::Unit, "Recruit", 1, {"Recruit"}, {},
                                   BattlefieldLocation{BattlefieldId::fromRaw(1)}, false);
    });
    ASSERT_NE(at_yours, kInvalidId);
    EXPECT_FALSE(state.getObject(at_yours).is_exhausted);
    EXPECT_TRUE(state.getObject(at_base).is_exhausted);
    EXPECT_TRUE(state.getObject(at_theirs).is_exhausted);
}

TEST_F(LegendDefects, ExileDoesNotReduceEnergy) {
    legend(P1, 1068);
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_TRUE(state.player(P1).units_enter_ready_at_your_bf);
    EXPECT_TRUE(state.player(P1).cost_modifiers.empty());
    EXPECT_EQ(card_db.get(1034).energy_cost, 3);
    auto unit = addToHand(P1, 13);
    playThroughEngine(unit);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_TRUE(state.getObject(unit).is_exhausted);
}

TEST_F(LegendDefects, PrideAssaultEqualsShieldAndFollowsIt) {
    legend(P1, 1067);
    auto unit = addUnit(P1, 13, 3, 0);
    state.battlefields[0].controller = P1;
    combatInProgress(0, P1);
    state.getObject(unit).shield_value = 0;
    state.player(P1).rune_pool.energy = 1;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = state.player(P1).legend_zone;
    EnginePathAgent agent(EnginePathAgent::choosing(unit));
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(act);
        settleLikeTheEngine(e);
    }, &agent);
    EXPECT_EQ(state.getObject(unit).assault_value, 0);
    EXPECT_EQ(state.getObject(unit).current_might, 3);
    EXPECT_FALSE(state.getObject(unit).hasKeyword(Keyword::Assault));

    state.getObject(unit).shield_value = 4;
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(state.getObject(unit).assault_value, 4);
    EXPECT_EQ(state.getObject(unit).current_might, 7);
    EXPECT_TRUE(state.getObject(unit).hasKeyword(Keyword::Assault));

    state.getObject(unit).shield_value = 0;
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(state.getObject(unit).assault_value, 0);
    EXPECT_EQ(state.getObject(unit).current_might, 3);
}

TEST_F(LegendDefects, IronRevenantIsNotOfferedWithNothingToBanish) {
    auto iron = legend(P1, 1064);
    EXPECT_FALSE(activationOffered(P1, iron));
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = iron;
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(act); });
    EXPECT_FALSE(state.getObject(iron).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.unit_gear_power, 0);

    auto spell = addToHand(P1, 253);
    toTrash(P1, spell);
    EXPECT_FALSE(activationOffered(P1, iron));
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(act); });
    EXPECT_FALSE(state.getObject(iron).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.unit_gear_power, 0);

    auto gear = addToHand(P1, 345);
    toTrash(P1, gear);
    EXPECT_TRUE(activationOffered(P1, iron));
}

TEST_F(LegendDefects, AgonyQueuesOnlyAFacedownPlayOnceEachTurn) {
    auto agony = legend(P1, 1071);
    state.turn.turn_number = 3;
    addUnit(P1, 13, 0, 0);
    state.battlefields[0].controller = P1;
    addToDeck(P1, 13);
    addToDeck(P1, 13);
    auto first = hide(P1, 135, 0, 1);
    auto second = hide(P1, 135, 0, 1);

    auto from_hand = addToHand(P1, 13);
    const int hand_items = legendItemsFinalized(agony, [&] { playThroughEngine(from_hand); });
    EXPECT_EQ(hand_items, 0);
    EXPECT_EQ(state.player(P1).draws_this_turn, 0);
    EXPECT_TRUE(state.player(P1).hand.empty());

    const int first_items = legendItemsFinalized(agony, [&] {
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = first;
        withEngine([&](GameEngine& e) {
            e.testHook_executeIntent(play);
            settleLikeTheEngine(e);
        });
    });
    EXPECT_EQ(first_items, 1);
    EXPECT_EQ(state.player(P1).draws_this_turn, 1);
    EXPECT_EQ(state.player(P1).hand.size(), 1u);

    const int second_items = legendItemsFinalized(agony, [&] {
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = second;
        withEngine([&](GameEngine& e) {
            e.testHook_executeIntent(play);
            settleLikeTheEngine(e);
        });
    });
    EXPECT_EQ(second_items, 0);
    EXPECT_EQ(state.player(P1).draws_this_turn, 1);
    EXPECT_EQ(state.player(P1).hand.size(), 1u);
}

TEST_F(LegendDefects, AgonyDiscardsWhenTheFacedownPlayIsNotYourTurn) {
    legend(P1, 1071);
    state.turn.turn_number = 3;
    addUnit(P1, 13, 0, 0);
    state.battlefields[0].controller = P1;
    auto drawn = addToDeck(P1, 13);
    auto hidden = hide(P1, 135, 0, 1);
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = hidden;
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(play);
        settleLikeTheEngine(e);
    }, nullptr, nullptr, P2);
    EXPECT_EQ(state.turn.turn_player, P2);
    EXPECT_EQ(state.player(P1).draws_this_turn, 1);
    EXPECT_TRUE(state.player(P1).has_discarded_this_turn);
    EXPECT_TRUE(state.player(P1).hand.empty());
    EXPECT_TRUE(inTrash(P1, drawn));
}

}  // namespace test_legend_defects
}  // namespace riftbound::test
