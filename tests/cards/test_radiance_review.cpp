/// Behavioral tests for the Radiance review fixes. Each one failed on the
/// implementation the reviews described and passes once that defect is gone.

#include "tests/cards/card_test_fixture.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace test_radiance_review {

class RadianceReview : public CardTestFixture {
protected:
    GameObjectId place(PlayerId owner, CardDefId def_id, int at_bf) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        const auto& def = card_db.get(def_id);
        obj.name = def.name;
        obj.card_type = def.card_type;
        obj.super_type = def.super_type;
        obj.keywords = def.keywords;
        obj.domains = def.domains;
        obj.tags = def.tags;
        copyPrintedStats(obj, def);
        if (at_bf >= 0) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{BattlefieldId::fromRaw(at_bf)};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }

    void toBanishment(PlayerId owner, GameObjectId card) {
        auto& hand = state.player(owner).hand;
        hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
        state.getObject(card).zone = ZoneType::Banishment;
        state.getObject(card).location.reset();
        state.player(owner).banishment.push_back(card);
    }

    void toTrash(PlayerId owner, GameObjectId card) {
        auto& hand = state.player(owner).hand;
        hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
        state.getObject(card).zone = ZoneType::Trash;
        state.getObject(card).location.reset();
        state.player(owner).trash.push_back(card);
    }
};

TEST_F(RadianceReview, ShowOffDoesNotStealTheSpellTarget) {
    auto friendly = addUnit(P1, kInvalidId, 5, -1);
    state.getObject(friendly).base_might = 5;
    state.getObject(friendly).current_might = 5;
    auto enemy = addUnit(P2, kInvalidId, 5, 0);
    auto spell = addToHand(P1, 1104);
    EnginePathAgent agent(EnginePathAgent::choosing(enemy, EnginePathAgent::May::Yes));
    playThroughEngine(spell, &agent);
    EXPECT_EQ(state.getObject(enemy).zone, ZoneType::Trash);
    EXPECT_TRUE(state.getObject(friendly).location.has_value());
    EXPECT_EQ(state.getObject(friendly).damage_marked, 0);
}

TEST_F(RadianceReview, BattlePreparationsReadiesTheRunesYouChoose) {
    auto gear = place(P1, 967, -1);
    auto unit = addUnit(P1, kInvalidId, 2, -1);
    auto r0 = addRune(P1, Domain::Fury, true);
    auto r1 = addRune(P1, Domain::Fury, true);
    auto r2 = addRune(P1, Domain::Fury, true);
    auto spell = addToHand(P1, 1093);
    EnginePathAgent agent(EnginePathAgent::script({
        1, static_cast<int>(gear.raw()), static_cast<int>(unit.raw()),
        static_cast<int>(r2.raw()), 0, EnginePathAgent::kRest}));
    playThroughEngine(spell, &agent);
    EXPECT_TRUE(state.getObject(r0).is_exhausted);
    EXPECT_TRUE(state.getObject(r1).is_exhausted);
    EXPECT_FALSE(state.getObject(r2).is_exhausted);
    EXPECT_EQ(state.getObject(unit).current_might, state.getObject(unit).base_might + 2);
}

TEST_F(RadianceReview, DeployHoldAndConquerSeeGear) {
    state.battlefields[0].controller = P1;
    auto holder = addUnit(P1, kInvalidId, 3, 0);
    auto good = place(P1, 1035, 0);
    auto enemy_deploy = place(P2, 967, 0);
    auto jarvan = place(P1, 1025, 0);
    auto traveler = place(P2, 1009, 0);
    withEngine([&](GameEngine& e) {
        e.testHook_scoreHold(P1, BattlefieldId::fromRaw(0));
        e.testHook_runChain();
    });
    EXPECT_EQ(state.getObject(good).zone, ZoneType::Hand);
    EXPECT_EQ(state.getObject(enemy_deploy).zone, ZoneType::Trash);
    EXPECT_TRUE(state.getObject(jarvan).location.has_value());
    EXPECT_TRUE(state.getObject(traveler).location.has_value());
    EXPECT_TRUE(state.getObject(holder).location.has_value());

    auto armory = place(P1, 1010, 1);
    auto buffed = addUnit(P1, kInvalidId, 2, 1);
    withEngine([&](GameEngine& e) {
        e.testHook_scoreConquer(P1, BattlefieldId::fromRaw(1), P2);
        e.testHook_runChain();
    });
    int buffs = 0;
    for (auto& [id, obj] : state.objects)
        if (obj.isUnit() && obj.controller == P1) buffs += obj.buff_count;
    EXPECT_GE(buffs, 1);
    (void)armory;
    (void)buffed;
}

TEST_F(RadianceReview, ScorchedEarthFinishes) {
    addUnit(P1, kInvalidId, 3, 0);
    addUnit(P2, kInvalidId, 3, 0);
    auto spell = addToHand(P1, 1086);
    EnginePathAgent agent(EnginePathAgent::script({EnginePathAgent::kRest}));
    EXPECT_NO_THROW(playThroughEngine(spell, &agent));
    int dead = 0;
    for (auto& [id, obj] : state.objects)
        if (obj.isUnit() && obj.zone == ZoneType::Trash) ++dead;
    EXPECT_EQ(dead, 1);
}

TEST_F(RadianceReview, BanishedSpellNeedsALegalTarget) {
    auto spell = addToHand(P1, 457);
    toBanishment(P1, spell);
    state.getObject(spell).card_counters["__playable_through_turn"] = state.turn.turn_number;
    state.player(P1).rune_pool.energy = 10;
    state.player(P1).rune_pool.universal_power = 10;
    withEngine([&](GameEngine& e) {
        for (const auto& intent : e.testHook_mainPhaseActions(P1))
            EXPECT_NE(intent.card, spell);
    });
}

TEST_F(RadianceReview, DeployGearFromTrashIsNotPlayedToBase) {
    auto gear = addToHand(P1, 967);
    toTrash(P1, gear);
    state.player(P1).may_play_from_trash = true;
    state.player(P1).rune_pool.energy = 10;
    state.player(P1).rune_pool.universal_power = 10;
    bool battlefield = false;
    bool base = false;
    withEngine([&](GameEngine& e) {
        for (const auto& intent : e.testHook_mainPhaseActions(P1)) {
            if (intent.card != gear || intent.type != IntentType::PlayCard) continue;
            if (!intent.play_location.has_value()) continue;
            if (std::holds_alternative<BaseLocation>(*intent.play_location)) base = true;
            if (std::holds_alternative<BattlefieldLocation>(*intent.play_location)) battlefield = true;
        }
    });
    EXPECT_TRUE(battlefield);
    EXPECT_FALSE(base);
}

TEST_F(RadianceReview, PrintedKeywordsMatchTheCard) {
    EXPECT_FALSE(card_db.get(1025).keywords.has(Keyword::Deploy));
    EXPECT_FALSE(card_db.get(1009).keywords.has(Keyword::Deploy));
    EXPECT_FALSE(card_db.get(1029).keywords.has(Keyword::Temporary));
    EXPECT_FALSE(card_db.get(1047).keywords.has(Keyword::Deflect));
    EXPECT_FALSE(card_db.get(1085).keywords.has(Keyword::Assault));
    EXPECT_FALSE(card_db.get(1090).keywords.has(Keyword::Shield));
    EXPECT_FALSE(card_db.get(1094).keywords.has(Keyword::Deploy));
    EXPECT_FALSE(card_db.get(1110).keywords.has(Keyword::Ganking));
    EXPECT_TRUE(card_db.get(1110).keywords.has(Keyword::Hidden));
    EXPECT_FALSE(card_db.get(1072).keywords.has(Keyword::Deploy));
    EXPECT_FALSE(card_db.get(1078).keywords.has(Keyword::Tank));
    EXPECT_FALSE(card_db.get(1079).keywords.has(Keyword::Accelerate));
    EXPECT_FALSE(card_db.get(967).keywords.has(Keyword::Deathknell));
    EXPECT_TRUE(card_db.get(967).keywords.has(Keyword::Deploy));
    EXPECT_NE(std::find(card_db.get(1123).tags.begin(), card_db.get(1123).tags.end(), "Bomb"),
              card_db.get(1123).tags.end());
    EXPECT_EQ(card_db.get(1114).ability_text.find("disa"), std::string::npos);
    const Card* empyrean = card_registry.get(998);
    ASSERT_NE(empyrean, nullptr);
    GameObject rune;
    rune.card_type = CardType::Rune;
    rune.card_def_id = 1;
    EXPECT_FALSE(empyrean->matchesShowOff(state, rune));
    GameObject token;
    token.is_token = true;
    token.card_type = CardType::Unit;
    token.card_def_id = 1;
    EXPECT_FALSE(empyrean->matchesShowOff(state, token));
    GameObject legend;
    legend.zone = ZoneType::LegendZone;
    legend.card_type = CardType::Legend;
    legend.card_def_id = 1065;
    EXPECT_FALSE(empyrean->matchesShowOff(state, legend));
}

TEST_F(RadianceReview, MomentaryCompanionIsTemporaryOnlyOnTheOpponentsTurn) {
    auto unit = addToHand(P1, 1029);
    playThroughEngine(unit, nullptr, LocationId{BaseLocation{P1}});
    ASSERT_TRUE(state.objectExists(unit));
    EXPECT_FALSE(state.getObject(unit).keywords.has(Keyword::Temporary));
    state.turn.turn_player = P2;
    fireTriggerThroughEngine(unit, TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(state.getObject(unit).keywords.has(Keyword::Temporary));
}

TEST_F(RadianceReview, SirensSongOnlyAnEnemyThatMoved) {
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    const Card* song = card_registry.get(1107);
    ASSERT_NE(song, nullptr);
    EXPECT_FALSE(song->hasLegalTargets(state, P1));
    state.getObject(enemy).moves_this_turn = 1;
    EXPECT_TRUE(song->hasLegalTargets(state, P1));
}

TEST_F(RadianceReview, NeekoAddsAReadyLegendThatLeavesWithHer) {
    auto champion = state.createObject();
    auto& legend = state.getObject(champion);
    legend.card_def_id = 1072;
    legend.name = card_db.get(1072).name;
    legend.card_type = CardType::Legend;
    legend.owner = P1;
    legend.controller = P1;
    legend.zone = ZoneType::LegendZone;
    state.player(P1).legend_zone = champion;
    state.player(P1).neeko_legends = {1065};
    auto neeko = addToHand(P1, 1084);
    EnginePathAgent agent(EnginePathAgent::script({EnginePathAgent::kRest}));
    playThroughEngine(neeko, &agent, LocationId{BaseLocation{P1}});
    GameObjectId added = kInvalidId;
    for (auto& [id, obj] : state.objects) {
        if (obj.zone == ZoneType::LegendZone && obj.card_def_id.raw() == 1065) added = id;
    }
    ASSERT_NE(added, kInvalidId);
    EXPECT_NE(added, state.player(P1).legend_zone);
    EXPECT_FALSE(state.getObject(added).is_token);
    EXPECT_FALSE(state.getObject(added).is_exhausted);
    withEngine([&](GameEngine& e) {
        e.testHook_executor().killObject(neeko);
    });
    EXPECT_FALSE(state.objectExists(added));
}

TEST_F(RadianceReview, BlackMarketEachPlayerOnceOnTheirTurn) {
    auto market = state.createObject();
    auto& obj = state.getObject(market);
    obj.card_def_id = 1075;
    obj.name = card_db.get(1075).name;
    obj.card_type = CardType::Battlefield;
    obj.owner = P1;
    obj.controller = P1;
    obj.zone = ZoneType::BattlefieldZone;
    state.battlefields[0].card_object_id = market;
    state.player(P2).rune_pool.universal_power = 2;
    auto offered = [&](PlayerId who) {
        bool yes = false;
        withEngine([&](GameEngine& e) {
            for (const auto& intent : e.testHook_mainPhaseActions(who))
                if (intent.type == IntentType::ActivateAbility && intent.ability_source == market)
                    yes = true;
        }, nullptr, nullptr, who);
        return yes;
    };
    EXPECT_TRUE(offered(P2));
    state.getObject(market).card_counters["__market_p2"] = state.turn.turn_number + 1;
    EXPECT_FALSE(offered(P2));
}

TEST_F(RadianceReview, BlackMarketActivationDrawsOnce) {
    auto market = state.createObject();
    auto& obj = state.getObject(market);
    obj.card_def_id = 1075;
    obj.name = card_db.get(1075).name;
    obj.card_type = CardType::Battlefield;
    obj.owner = P1;
    obj.controller = P1;
    obj.zone = ZoneType::BattlefieldZone;
    state.battlefields[0].card_object_id = market;
    state.player(P1).rune_pool.universal_power = 4;
    addToDeck(P1, 956);
    addToDeck(P1, 956);
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = market;
    act.ability_index = 0;
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(act); }, nullptr, nullptr, P1);
    EXPECT_EQ(state.player(P1).hand.size(), 1u);
    EXPECT_EQ(state.getObject(market).card_counters["__market_p1"], state.turn.turn_number + 1);
    bool again = false;
    withEngine([&](GameEngine& e) {
        for (const auto& intent : e.testHook_mainPhaseActions(P1))
            if (intent.type == IntentType::ActivateAbility && intent.ability_source == market)
                again = true;
    }, nullptr, nullptr, P1);
    EXPECT_FALSE(again);
}

TEST_F(RadianceReview, ChronobreakPaysAnyPowerAndTheControllerChooses) {
    state.player(P1).rune_pool.universal_power = 1;
    auto low = addUnit(P1, kInvalidId, 2, -1);
    auto high = addUnit(P1, kInvalidId, 2, -1);
    state.getObject(low).damage_marked = 1;
    state.getObject(high).damage_marked = 1;
    auto spell = addToHand(P1, 1117);
    EnginePathAgent agent(EnginePathAgent::script({
        1, static_cast<int>(high.raw()), 0, 1, EnginePathAgent::kRest}));
    playThroughEngine(spell, &agent);
    EXPECT_EQ(state.getObject(low).damage_marked, 1);
    EXPECT_EQ(state.getObject(high).damage_marked, 0);
}

TEST_F(RadianceReview, EncoreModesAreIndependent) {
    auto victim = addUnit(P2, kInvalidId, 5, 0);
    auto spell = addToHand(P1, 1121);
    EnginePathAgent agent(EnginePathAgent::script({
        1, 0, static_cast<int>(victim.raw()), EnginePathAgent::kRest}));
    playThroughEngine(spell, &agent);
    EXPECT_EQ(state.getObject(victim).damage_marked, 2);
    int recruits = 0;
    for (auto& [id, obj] : state.objects)
        if (obj.isUnit() && obj.is_token && obj.name.get() == "Recruit") ++recruits;
    EXPECT_EQ(recruits, 2);
}

TEST_F(RadianceReview, EquipCostsAreWhatTheCardPrints) {
    const Card* ntofo = card_registry.get(1008);
    const Card* shield = card_registry.get(1050);
    const Card* blade = card_registry.get(1069);
    ASSERT_NE(ntofo, nullptr);
    ASSERT_NE(shield, nullptr);
    ASSERT_NE(blade, nullptr);
    EXPECT_TRUE(ntofo->hasEquipAbility());
    EXPECT_TRUE(shield->hasEquipAbility());
    EXPECT_TRUE(blade->hasEquipAbility());
    auto gear = place(P1, 1008, -1);
    auto unit = addUnit(P1, kInvalidId, 2, -1);
    auto cost = ntofo->equipCost(state, P1, gear, unit);
    EXPECT_TRUE(cost.valid);
    EXPECT_EQ(cost.energy, 1);
    EXPECT_EQ(cost.power, 0);
    cost = shield->equipCost(state, P1, gear, unit);
    EXPECT_EQ(cost.energy, 1);
    EXPECT_EQ(cost.power, 0);
    EXPECT_TRUE(blade->canPayEquipExtraCost(state, P1, gear, unit));
}

}  // namespace test_radiance_review
}  // namespace riftbound::test
