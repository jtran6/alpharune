// Rules the coverage pass still had wrong: Battlefield Armorer's gear-death
// return, Meticulous Restorationist as a trash trigger rather than a standing
// play, Promising Future still charging Power, Heisho ignoring Deflect only
// at its battlefield, and Starry-Eyed Songstress letting the player name
// which three units to exhaust.

#include "tests/cards/card_test_fixture.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace riftbound::test {
namespace test_remaining_cards {

class RemainingCards : public CardTestFixture {
protected:
    GameObjectId gearAt(PlayerId owner, CardDefId def_id, int at_bf) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        const auto& def = card_db.get(def_id);
        obj.name = def.name;
        obj.card_type = def.card_type;
        obj.keywords = def.keywords;
        if (at_bf >= 0) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{BattlefieldId::fromRaw(at_bf)};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }

    void toTrash(PlayerId owner, GameObjectId card) {
        auto& hand = state.player(owner).hand;
        hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
        state.getObject(card).zone = ZoneType::Trash;
        state.getObject(card).location.reset();
        state.player(owner).trash.push_back(card);
    }

    bool trashPlayOf(PlayerId who, GameObjectId card) {
        bool yes = false;
        withEngine([&](GameEngine& e) {
            for (const auto& i : e.testHook_mainPhaseActions(who))
                if (i.card == card && i.play_source == Intent::PlaySource::Trash) yes = true;
        }, nullptr, nullptr, who);
        return yes;
    }
};

TEST_F(RemainingCards, ArmorerReturnsAGearThatDiedAtABattlefieldToItsOwner) {
    addUnit(P1, 1055);
    auto gear = gearAt(P1, 345, 0);
    state.getObject(gear).owner = P2;   // "its owner's hand", not the controller's
    state.player(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    withEngine([&](GameEngine& e) {
        e.testHook_executor().killObject(gear);
        settleLikeTheEngine(e);
    }, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(inHand(P2, gear));
    EXPECT_FALSE(inTrash(P2, gear));
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

TEST_F(RemainingCards, ArmorerDoesNotFireForAGearThatDiedAtBase) {
    addUnit(P1, 1055);
    auto gear = gearAt(P1, 345, -1);
    state.player(P1).rune_pool.energy = 1;
    EnginePathAgent agent;
    withEngine([&](GameEngine& e) {
        e.testHook_executor().killObject(gear);
        settleLikeTheEngine(e);
    }, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(inTrash(P1, gear));
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);
}

TEST_F(RemainingCards, ArmorerIsNotOfferedWhenTheEnergyCannotBePaid) {
    addUnit(P1, 1055);
    auto gear = gearAt(P1, 345, 0);
    EnginePathAgent agent;
    withEngine([&](GameEngine& e) {
        e.testHook_executor().killObject(gear);
        settleLikeTheEngine(e);
    }, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(inTrash(P1, gear));
}

TEST_F(RemainingCards, RestorationistPlaysFromTrashWhenASevenCostIsPlayed) {
    auto rest = addToHand(P1, 1048);
    toTrash(P1, rest);
    auto rune = addRune(P1, Domain::Order);
    auto big = addToHand(P1, 75);   // Tasty Faefolk, Energy 7
    EnginePathAgent agent(EnginePathAgent::script({1, EnginePathAgent::kRest}));
    playThroughEngine(big, &agent);
    EXPECT_FALSE(inTrash(P1, rest));
    EXPECT_EQ(state.getObject(rest).zone, ZoneType::Base);
    EXPECT_FALSE(state.getObject(rune).location.has_value()) << "[Y] was paid";
}

TEST_F(RemainingCards, RestorationistDoesNotStayPlayableAfterTheTriggerIsDeclined) {
    auto rest = addToHand(P1, 1048);
    toTrash(P1, rest);
    addRune(P1, Domain::Order);
    auto big = addToHand(P1, 75);
    EnginePathAgent agent(EnginePathAgent::script({0}));
    playThroughEngine(big, &agent);
    EXPECT_TRUE(inTrash(P1, rest));
    EXPECT_FALSE(trashPlayOf(P1, rest))
        << "the permission is the trigger, not the rest of the turn";
}

TEST_F(RemainingCards, RestorationistIgnoresACardUnderSevenEnergy) {
    auto rest = addToHand(P1, 1048);
    toTrash(P1, rest);
    addRune(P1, Domain::Order);
    for (int i = 0; i < 4; ++i) addToDeck(P1, 13);
    auto day = addToHand(P1, 114);   // Progress Day, Energy 6
    EnginePathAgent agent;
    playThroughEngine(day, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(inTrash(P1, rest));
}

TEST_F(RemainingCards, PromisingFutureLeavesACardBanishedWhenItsPowerCannotBePaid) {
    for (int i = 0; i < 4; ++i) addToDeck(P1, 13);
    auto blitz = addToDeck(P1, 67);   // 5 Energy, 1 Power
    auto spell = addToHand(P1, 115);
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(blitz.raw()), EnginePathAgent::kRest}));
    playThroughEngine(spell, &agent);
    EXPECT_FALSE(state.getObject(blitz).location.has_value());
    EXPECT_NE(std::find(state.player(P1).banishment.begin(),
                        state.player(P1).banishment.end(), blitz),
              state.player(P1).banishment.end());
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
}

TEST_F(RemainingCards, PromisingFuturePaysPowerAndIgnoresEnergy) {
    for (int i = 0; i < 4; ++i) addToDeck(P1, 13);
    auto blitz = addToDeck(P1, 67);
    auto spell = addToHand(P1, 115);
    state.player(P1).rune_pool.universal_power = 1;   // the spell's own [B] is funded on top
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(blitz.raw()), EnginePathAgent::kRest}));
    playThroughEngine(spell, &agent);
    EXPECT_EQ(state.getObject(blitz).zone, ZoneType::Base);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0)
        << "Blitz's 5 Energy is ignored; the spell spent the 5 it was funded";
}

TEST_F(RemainingCards, HeishoIgnoresDeflectOnlyAtThatBattlefield) {
    auto heisho = state.createObject();
    auto& card = state.getObject(heisho);
    card.card_def_id = 945;
    card.name = card_db.get(945).name;
    card.card_type = CardType::Battlefield;
    card.zone = ZoneType::BattlefieldZone;
    state.battlefields[0].card_object_id = heisho;
    auto here = addUnit(P2, 712, 0, 0);    // Vex, Deflect 1, at Heisho
    auto there = addUnit(P2, 712, 0, 1);
    auto spell = addToHand(P1, 9);         // Hextech Ray, [1][R], a unit at a battlefield
    state.player(P1).rune_pool.energy = 1;
    state.player(P1).rune_pool.universal_power = 1;   // the spell's Power; Deflect would need another
    withEngine([&](GameEngine& e) {
        e.testHook_recalculateAuras();
        EXPECT_TRUE(e.mutableState().battlefields[0].ignore_deflect_here);
        EXPECT_EQ(e.testHook_executor().deflectCostFor(P1, here), 0);
        EXPECT_EQ(e.testHook_executor().deflectCostFor(P1, there), 1);
        bool here_play = false, there_play = false;
        for (const auto& i : e.testHook_mainPhaseActions(P1)) {
            if (i.type != IntentType::PlayCard || i.card != spell || i.targets.empty()) continue;
            if (i.targets.front() == here) here_play = true;
            if (i.targets.front() == there) there_play = true;
        }
        EXPECT_TRUE(here_play);
        EXPECT_FALSE(there_play);
    });
}

TEST_F(RemainingCards, SongstressExhaustsTheThreeUnitsThePlayerNames) {
    auto legend = state.createObject();
    auto& L = state.getObject(legend);
    L.card_def_id = 1070;
    L.name = card_db.get(1070).name;
    L.card_type = CardType::Legend;
    L.owner = P1;
    L.controller = P1;
    L.zone = ZoneType::LegendZone;
    state.player(P1).legend_zone = legend;
    auto stay = addUnit(P1, 13);
    auto a = addUnit(P1, 13);
    auto b = addUnit(P1, 13);
    auto c = addUnit(P1, 13);
    auto spell = addToHand(P1, 114);   // Progress Day: no choices of its own
    EnginePathAgent agent(EnginePathAgent::script({
        1, static_cast<int>(c.raw()), static_cast<int>(b.raw()), static_cast<int>(a.raw()),
        EnginePathAgent::kRest}));
    playThroughEngine(spell, &agent);
    EXPECT_TRUE(state.getObject(legend).is_exhausted);
    EXPECT_FALSE(state.getObject(stay).is_exhausted);
    EXPECT_TRUE(state.getObject(a).is_exhausted);
    EXPECT_TRUE(state.getObject(b).is_exhausted);
    EXPECT_TRUE(state.getObject(c).is_exhausted);
    // Progress Day is [6]; the discount leaves the 2 the funding put in.
    EXPECT_EQ(state.player(P1).rune_pool.energy, 2);
}

TEST_F(RemainingCards, SongstressDeclinedLeavesEveryoneReady) {
    auto legend = state.createObject();
    auto& L = state.getObject(legend);
    L.card_def_id = 1070;
    L.name = card_db.get(1070).name;
    L.card_type = CardType::Legend;
    L.owner = P1;
    L.controller = P1;
    L.zone = ZoneType::LegendZone;
    state.player(P1).legend_zone = legend;
    auto a = addUnit(P1, 13);
    auto b = addUnit(P1, 13);
    auto c = addUnit(P1, 13);
    auto spell = addToHand(P1, 114);
    EnginePathAgent agent(EnginePathAgent::script({0, EnginePathAgent::kRest}));
    playThroughEngine(spell, &agent);
    EXPECT_FALSE(state.getObject(legend).is_exhausted);
    EXPECT_FALSE(state.getObject(a).is_exhausted);
    EXPECT_FALSE(state.getObject(b).is_exhausted);
    EXPECT_FALSE(state.getObject(c).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

}  // namespace test_remaining_cards
}  // namespace riftbound::test
