// The five legend defects from the legend audit, and the lines a paid unit
// from hand does not play.
// Boy Who Shattered Time: the kill ends with the turn, and an end-of-turn
// ability is not queued when there is no unit to kill. The [2][A] and the
// kill are the same unit: a facedown or ignore-cost play spends both, a
// countered kill does not move to a later unit, and a unit played during the
// Ending Step is still killed.
// The Exile: a unit token played to a battlefield that player controls enters
// ready. The line does not reduce energy.
// Pride of Nazumah: Assault equals Shield this turn, including 0, and follows
// a later Shield change. Shield from an aura or an Equipment counts. The
// grant ends when the unit leaves the board, a second activation does not
// add another Assault, and a control change does not freeze the old number.
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

    // A second legend in the zone, beside the one legend() seated.
    GameObjectId legendBeside(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.card_def_id = def_id;
        obj.name = card_db.get(def_id).name;
        obj.card_type = CardType::Legend;
        obj.owner = owner;
        obj.controller = owner;
        obj.zone = ZoneType::LegendZone;
        return id;
    }

    GameObjectId gearOnBoard(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.card_def_id = def_id;
        obj.name = card_db.get(def_id).name;
        obj.card_type = CardType::Gear;
        obj.owner = owner;
        obj.controller = owner;
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            obj.tags = def.tags;
            obj.keywords = def.keywords;
            obj.might_bonus = def.might_bonus;
        }
        obj.zone = ZoneType::Base;
        obj.location = BaseLocation{owner};
        return id;
    }

    void activatePride(GameObjectId unit) {
        state.player(P1).rune_pool.energy = 1;
        state.getObject(state.player(P1).legend_zone).is_exhausted = false;
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = P1;
        act.ability_source = state.player(P1).legend_zone;
        EnginePathAgent agent(EnginePathAgent::choosing(unit));
        withEngine([&](GameEngine& e) {
            e.testHook_executeIntent(act);
            settleLikeTheEngine(e);
        }, &agent);
    }

    // Pay the printed cost from floating Energy added here, on top of whatever
    // the pool already holds. What is left is the discount: a [2] reduction
    // on a card that costs [2] leaves the Energy that was added.
    int energyAfterPaying(GameObjectId card) {
        const PlayerId player = state.getObject(card).controller;
        const auto& def = card_db.get(state.getObject(card).card_def_id);
        state.player(player).rune_pool.energy += def.energy_cost;
        playThroughEngine(card, nullptr, std::nullopt, nullptr, /*fund=*/false);
        return state.player(player).rune_pool.energy;
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

// A facedown unit is the next unit. It takes the kill, and it takes the
// discount too: the next unit paid from hand is full price and lives.
TEST_F(LegendDefects, BoyFacedownPlaySpendsTheDiscountWithTheKill) {
    auto boy = legend(P1, 1062);
    activateBoy(boy);
    addUnit(P1, 13, 0, 0);
    state.battlefields[0].controller = P1;
    auto facedown = hide(P1, 135, 0, 1);
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = facedown;
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(play);
        settleLikeTheEngine(e);
    });
    EXPECT_TRUE(state.player(P1).cost_modifiers.empty());
    ASSERT_TRUE(state.getObject(facedown).location.has_value());

    auto paid = addToHand(P1, 13);
    EXPECT_EQ(energyAfterPaying(paid), 0);
    ASSERT_TRUE(state.getObject(paid).location.has_value());

    withEngine([&](GameEngine& e) { e.testHook_endingStep(); });
    EXPECT_TRUE(inTrash(P1, facedown));
    EXPECT_FALSE(inTrash(P1, paid));
    EXPECT_TRUE(state.getObject(paid).location.has_value());
}

// A unit an effect plays while ignoring its cost is that same next unit.
TEST_F(LegendDefects, BoyIgnoreCostPlaySpendsTheDiscountWithTheKill) {
    auto boy = legend(P1, 1062);
    activateBoy(boy);
    auto free = addToHand(P1, 13);
    withEngine([&](GameEngine& e) {
        ASSERT_TRUE(e.testHook_executor().playIgnoringCost(
            P1, free, LocationId{BaseLocation{P1}}));
        settleLikeTheEngine(e);
    });
    EXPECT_TRUE(state.player(P1).cost_modifiers.empty());
    ASSERT_TRUE(state.getObject(free).location.has_value());

    auto paid = addToHand(P1, 13);
    EXPECT_EQ(energyAfterPaying(paid), 0);
    ASSERT_TRUE(state.getObject(paid).location.has_value());

    withEngine([&](GameEngine& e) { e.testHook_endingStep(); });
    EXPECT_TRUE(inTrash(P1, free));
    EXPECT_FALSE(inTrash(P1, paid));
    EXPECT_TRUE(state.getObject(paid).location.has_value());
}

// Countering the kill does not hand it to a later unit. The discount was
// already this unit's: the later one is full price, and neither dies.
TEST_F(LegendDefects, BoyCounteredKillDoesNotPassToALaterUnit) {
    auto boy = legend(P1, 1062);
    activateBoy(boy);
    auto first = addToHand(P1, 13);
    state.player(P1).rune_pool.energy = 2;
    withEngine([&](GameEngine& e) {
        bool countered = false;
        auto conn = events.on_chain_item_finalized.connect(
            [&](const ChainItemFinalizedEvent& ev) {
                if (countered || ev.source != boy) return;
                for (const auto& ci : e.mutableState().chain.items) {
                    if (ci.id != ev.item || ci.fired_trigger != TriggerType::WhenYouPlayAUnit)
                        continue;
                    countered = true;
                    // What a counter does: the item leaves the chain after it
                    // is finalized and before it resolves (CR 355.9.a.2).
                    e.testHook_executor().removeChainItem(ev.item);
                    return;
                }
            });
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = first;
        play.play_location = LocationId{BaseLocation{P1}};
        e.testHook_executeIntent(play);
        settleLikeTheEngine(e);
        conn.disconnect();
        EXPECT_TRUE(countered);
    });
    EXPECT_EQ(state.player(P1).rune_pool.energy, 2);
    EXPECT_TRUE(state.player(P1).cost_modifiers.empty());
    ASSERT_TRUE(state.getObject(first).location.has_value());

    auto later = addToHand(P1, 13);
    EXPECT_EQ(energyAfterPaying(later), 2);
    ASSERT_TRUE(state.getObject(later).location.has_value());

    withEngine([&](GameEngine& e) { e.testHook_endingStep(); });
    EXPECT_TRUE(state.getObject(first).location.has_value());
    EXPECT_FALSE(inTrash(P1, first));
    EXPECT_TRUE(state.getObject(later).location.has_value());
    EXPECT_FALSE(inTrash(P1, later));
}

// The discounted unit, played while the Ending Step's chain is resolving,
// is still killed at end of turn.
TEST_F(LegendDefects, BoyKillsTheDiscountedUnitPlayedDuringTheEndingStep) {
    auto boy = legend(P1, 1062);
    activateBoy(boy);
    gearOnBoard(P1, 160);   // Dazzling Aurora: at end of turn, play a unit ignoring its cost
    auto unit = addToDeck(P1, 13);
    withEngine([&](GameEngine& e) { e.testHook_endingStep(); });
    EXPECT_TRUE(state.player(P1).cost_modifiers.empty());
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

// Shield from an aura (Mechanized Menace's bare [Shield] on a Mech) and from
// Equipment counts, including a change later this turn. 0 is the existing
// shield_value case above.
TEST_F(LegendDefects, PrideAssaultFollowsAuraAndEquipmentShield) {
    legend(P1, 1067);
    legendBeside(P1, 551);
    auto mech = addUnit(P1, 88, 3, 0);
    state.battlefields[0].controller = P1;
    combatInProgress(0, P1);
    state.getObject(mech).shield_value = 0;
    activatePride(mech);
    EXPECT_EQ(state.getObject(mech).assault_value, 1);
    EXPECT_EQ(state.getObject(mech).current_might, 4);
    EXPECT_TRUE(state.getObject(mech).hasKeyword(Keyword::Assault));

    auto armor = gearOnBoard(P1, 387);
    withEngine([&](GameEngine& e) {
        ASSERT_TRUE(e.testHook_executor().attachGear(armor, mech));
        e.testHook_cleanup();
    });
    EXPECT_EQ(state.getObject(mech).assault_value, 3);
    EXPECT_EQ(state.getObject(mech).current_might, 6);
    EXPECT_TRUE(state.getObject(mech).hasKeyword(Keyword::Assault));
}

// The grant is this object, this turn. A second activation does not add a
// second Assault. Bounce and replay: the new object does not keep it, even
// once it has Shield again.
TEST_F(LegendDefects, PrideGrantEndsWhenTheUnitLeavesAndDoesNotStack) {
    legend(P1, 1067);
    auto unit = addUnit(P1, 13, 3, 0);
    state.battlefields[0].controller = P1;
    combatInProgress(0, P1);
    state.getObject(unit).shield_value = 3;
    activatePride(unit);
    EXPECT_EQ(state.getObject(unit).assault_value, 3);
    EXPECT_EQ(state.getObject(unit).current_might, 6);

    activatePride(unit);
    EXPECT_EQ(state.getObject(unit).assault_value, 3);
    EXPECT_EQ(state.getObject(unit).current_might, 6);

    withEngine([&](GameEngine& e) { e.testHook_executor().bounceToHand(unit); });
    EXPECT_FALSE(state.getObject(unit).location.has_value());
    EXPECT_TRUE(inHand(P1, unit));
    playThroughEngine(unit);
    state.getObject(unit).shield_value = 5;
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(state.getObject(unit).assault_value, 0);
    EXPECT_FALSE(state.getObject(unit).hasKeyword(Keyword::Assault));
    EXPECT_EQ(state.getObject(unit).current_might, state.getObject(unit).base_might);
}

// The ability is the unit's for the turn. A later Shield change after control
// passes still moves Assault, and the turn's end takes the grant off.
TEST_F(LegendDefects, PrideAssaultFollowsShieldAfterControlChanges) {
    legend(P1, 1067);
    auto unit = addUnit(P1, 13, 3);
    state.getObject(unit).shield_value = 2;
    activatePride(unit);
    EXPECT_EQ(state.getObject(unit).assault_value, 2);

    withEngine([&](GameEngine& e) {
        e.testHook_executor().takeControl(unit, P2);
        e.testHook_cleanup();
    });
    EXPECT_EQ(state.getObject(unit).controller, P2);
    state.getObject(unit).shield_value = 6;
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_EQ(state.getObject(unit).assault_value, 6);
    EXPECT_TRUE(state.getObject(unit).hasKeyword(Keyword::Assault));

    withEngine([&](GameEngine& e) { e.testHook_expirationStep(); });
    EXPECT_EQ(state.getObject(unit).assault_value, 0);
    EXPECT_FALSE(state.getObject(unit).hasKeyword(Keyword::Assault));
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
