/// Behavioral tests for the Radiance review fixes. Each one failed on the
/// implementation the reviews described and passes once that defect is gone.

#include "tests/cards/card_test_fixture.h"

#include <functional>
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
    state.getObject(market).card_counters["__ability_used_turn_0"] = state.turn.turn_number + 1;
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
    EXPECT_EQ(state.getObject(market).card_counters["__ability_used_turn_0"],
              state.turn.turn_number + 1);
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

// A priority window comes before a choice that is made as an item resolves.
// The callback runs on the engine's state (the fixture's copy).
class ResponseThenChoose : public AgentInterface {
public:
    std::function<void(GameState&)> during_priority;
    bool choice_before_response = false;
    bool responded = false;
    Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
        if (legal.empty()) return Intent{};
        const bool choice = std::all_of(legal.begin(), legal.end(),
            [](const Intent& i) { return i.type == IntentType::MakeChoice; });
        if (!choice) {
            if (!responded && during_priority) {
                during_priority(const_cast<GameState&>(state));
                responded = true;
            }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
            return legal.front();
        }
        if (!responded) choice_before_response = true;
        if (pick) return pick(legal);
        return legal.front();
    }
    EnginePathAgent::Picker pick;
};

TEST_F(RadianceReview, ChronobreakReplaysEachUnitOnceWhenXIsAtLeastTwo) {
    addRune(P1, Domain::Fury);
    addRune(P1, Domain::Fury);
    auto first = addUnit(P1, kInvalidId, 2, -1);
    auto second = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(first).damage_marked = 1;
    state.getObject(second).damage_marked = 1;
    auto spell = addToHand(P1, 1117);
    EnginePathAgent agent(EnginePathAgent::script({
        2, static_cast<int>(first.raw()), static_cast<int>(first.raw()),
        0, 0, EnginePathAgent::kRest}));
    playThroughEngine(spell, &agent, std::nullopt, nullptr, false);
    EXPECT_TRUE(state.getObject(first).location.has_value());
    EXPECT_TRUE(state.getObject(second).location.has_value());
    EXPECT_EQ(state.getObject(first).damage_marked, 0);
    EXPECT_EQ(state.getObject(second).damage_marked, 0);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

TEST_F(RadianceReview, EncoreLookTwiceDrawsTwoDifferentCards) {
    state.player(P1).rune_pool.energy = 3;
    state.player(P1).rune_pool.universal_power = 1;
    auto bottom = addToDeck(P1, 956);
    auto next = addToDeck(P1, 957);
    auto under = addToDeck(P1, 958);
    auto top = addToDeck(P1, 1085);
    auto spell = addToHand(P1, 1121);
    EnginePathAgent agent(EnginePathAgent::script({
        3, 3, static_cast<int>(top.raw()), static_cast<int>(next.raw()),
        EnginePathAgent::kRest}));
    playThroughEngine(spell, &agent);
    const auto& hand = state.player(P1).hand;
    ASSERT_EQ(hand.size(), 2u);
    EXPECT_NE(hand[0], hand[1]);
    EXPECT_NE(std::find(hand.begin(), hand.end(), top), hand.end());
    EXPECT_NE(std::find(hand.begin(), hand.end(), next), hand.end());
    EXPECT_EQ(std::find(hand.begin(), hand.end(), under), hand.end());
    EXPECT_EQ(std::find(hand.begin(), hand.end(), bottom), hand.end());
}

TEST_F(RadianceReview, EncoreCanDealToTheSameUnitTwice) {
    state.player(P1).rune_pool.energy = 3;
    state.player(P1).rune_pool.universal_power = 1;
    auto victim = addUnit(P2, kInvalidId, 5, 0);
    auto spell = addToHand(P1, 1121);
    EnginePathAgent agent(EnginePathAgent::script({
        0, 0, static_cast<int>(victim.raw()), static_cast<int>(victim.raw()),
        EnginePathAgent::kRest}));
    playThroughEngine(spell, &agent);
    EXPECT_EQ(state.getObject(victim).damage_marked, 4);
}

TEST_F(RadianceReview, StalePerObjectCounterDoesNotSteerTheNextCast) {
    state.player(P1).rune_pool.energy = 10;
    auto unit = addUnit(P1, kInvalidId, 2, -1);
    auto spell = addToHand(P1, 1088);
    state.getObject(spell).card_counters["__ft_banish"] = 1;
    EnginePathAgent agent(EnginePathAgent::script({
        static_cast<int>(unit.raw()), EnginePathAgent::kRest}));
    playThroughEngine(spell, &agent);
    EXPECT_EQ(state.getObject(spell).zone, ZoneType::Trash);
    EXPECT_EQ(std::find(state.player(P1).banishment.begin(), state.player(P1).banishment.end(), spell),
              state.player(P1).banishment.end());

    auto prepared = addToHand(P1, 1093);
    auto fighter = addUnit(P1, kInvalidId, 2, -1);
    EnginePathAgent once(EnginePathAgent::script({
        static_cast<int>(fighter.raw()), EnginePathAgent::kRest}));
    playThroughEngine(prepared, &once);
    const int after_first = state.getObject(fighter).current_might;
    EXPECT_EQ(after_first, state.getObject(fighter).base_might + 2);
    auto& trash = state.player(P1).trash;
    trash.erase(std::remove(trash.begin(), trash.end(), prepared), trash.end());
    state.getObject(prepared).zone = ZoneType::Hand;
    state.getObject(prepared).location.reset();
    state.player(P1).hand.push_back(prepared);
    EnginePathAgent twice(EnginePathAgent::script({
        static_cast<int>(fighter.raw()), EnginePathAgent::kRest}));
    playThroughEngine(prepared, &twice);
    EXPECT_EQ(state.getObject(fighter).current_might, after_first + 2);
}

TEST_F(RadianceReview, SpellIsNotOfferedWhenNoTargetCanBePaidFor) {
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    state.getObject(enemy).keywords.set(Keyword::Deflect);
    state.getObject(enemy).deflect_value = 5;
    state.player(P1).rune_pool.energy = 9;
    auto hand = addToHand(P1, 1098);
    auto banished = addToHand(P1, 1098);
    toBanishment(P1, banished);
    state.getObject(banished).card_counters["__playable_through_turn"] = state.turn.turn_number;
    withEngine([&](GameEngine& e) {
        for (const auto& intent : e.testHook_mainPhaseActions(P1)) {
            EXPECT_NE(intent.card, hand);
            EXPECT_NE(intent.card, banished);
        }
    });
}

TEST_F(RadianceReview, ExemplarDoesNotMoveWhenItIsAlreadyExhausted) {
    auto champion = state.createObject();
    auto& legend = state.getObject(champion);
    legend.card_def_id = 1072;
    legend.name = card_db.get(1072).name;
    legend.card_type = CardType::Legend;
    legend.owner = P1;
    legend.controller = P1;
    legend.zone = ZoneType::LegendZone;
    legend.is_exhausted = true;
    state.player(P1).legend_zone = champion;
    state.player(P1).rune_pool.universal_power = 5;
    auto gear = place(P1, 967, 0);
    auto unit = addUnit(P1, kInvalidId, 2, -1);
    EnginePathAgent agent(EnginePathAgent::script({
        1, static_cast<int>(unit.raw()), EnginePathAgent::kRest}));
    fireTriggerThroughEngine(champion, TriggerType::WhenYouPlayAGear, &agent, gear);
    EXPECT_TRUE(std::holds_alternative<BaseLocation>(*state.getObject(unit).location));
    EXPECT_TRUE(state.getObject(champion).is_exhausted);
}

TEST_F(RadianceReview, ScopeAndAnglerChooseAfterResponses) {
    state.player(P1).rune_pool.energy = 5;
    auto scope = place(P1, 1002, -1);
    auto buried = addToDeck(P1, 1085);
    auto top = addToDeck(P1, 1042);
    ResponseThenChoose scope_agent;
    scope_agent.during_priority = [&](GameState& st) {
        auto& deck = st.player(P1).main_deck;
        deck.erase(std::remove(deck.begin(), deck.end(), buried), deck.end());
        deck.push_back(buried);
    };
    scope_agent.pick = [&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.chosen_value && *i.chosen_value == 2) return i;
        return legal.front();
    };
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = scope;
    act.ability_index = 0;
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(act); }, &scope_agent, &scope_agent);
    EXPECT_TRUE(scope_agent.responded);
    EXPECT_FALSE(scope_agent.choice_before_response);
    EXPECT_EQ(std::find(state.player(P1).hand.begin(), state.player(P1).hand.end(), buried),
              state.player(P1).hand.end());
    (void)top;

    auto wyrm = place(P1, 1042, 0);
    auto old_unit = addUnit(P2, kInvalidId, 2, -1);
    ResponseThenChoose wyrm_agent;
    GameObjectId late = kInvalidId;
    wyrm_agent.during_priority = [&](GameState& st) {
        st.getObject(old_unit).zone = ZoneType::Trash;
        st.getObject(old_unit).location.reset();
        st.player(P2).trash.push_back(old_unit);
        late = st.createObject();
        auto& born = st.getObject(late);
        born.owner = P2;
        born.controller = P2;
        born.card_type = CardType::Unit;
        born.name = "Late";
        born.zone = ZoneType::Base;
        born.location = BaseLocation{P2};
        born.base_might = 1;
        born.current_might = 1;
    };
    wyrm_agent.pick = [&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == late) return i;
        return legal.front();
    };
    Intent bite;
    bite.type = IntentType::ActivateAbility;
    bite.player = P1;
    bite.ability_source = wyrm;
    bite.ability_index = 0;
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(bite); }, &wyrm_agent, &wyrm_agent);
    EXPECT_FALSE(wyrm_agent.choice_before_response);
    ASSERT_NE(late, kInvalidId);
    ASSERT_TRUE(state.objectExists(late));
    ASSERT_TRUE(state.getObject(late).location.has_value());
    EXPECT_TRUE(std::holds_alternative<BattlefieldLocation>(*state.getObject(late).location));
}

TEST_F(RadianceReview, NeekoAddsNothingIfSheLeavesBeforeTheTriggerResolves) {
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
    auto neeko = addUnit(P1, 1084, 4, -1);
    ResponseThenChoose agent;
    agent.during_priority = [&](GameState& st) {
        st.getObject(neeko).zone = ZoneType::Trash;
        st.getObject(neeko).location.reset();
        st.player(P1).trash.push_back(neeko);
    };
    fireTriggerThroughEngine(neeko, TriggerType::WhenYouPlayMe, &agent, kInvalidId, &agent);
    for (auto& [id, obj] : state.objects)
        EXPECT_NE(obj.card_def_id.raw(), 1065);
}

TEST_F(RadianceReview, UnitGearScoresItsHoldTriggerOnce) {
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 3, 0);
    auto dock = place(P1, 1059, 0);
    state.getObject(dock).is_unit_gear = true;
    withEngine([&](GameEngine& e) {
        e.testHook_scoreHold(P1, BattlefieldId::fromRaw(0));
        int holds = 0;
        for (const auto& item : e.mutableState().chain.items)
            if (item.source == dock && item.fired_trigger == TriggerType::WhenIHold) ++holds;
        EXPECT_EQ(holds, 1);
    });
}

TEST_F(RadianceReview, BlackMarketStampBlocksASecondActivationBeforeItResolves) {
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
    state.getObject(market).card_counters["__ability_used_turn_0"] = state.turn.turn_number + 1;
    withEngine([&](GameEngine& e) {
        for (const auto& intent : e.testHook_mainPhaseActions(P1))
            EXPECT_FALSE(intent.type == IntentType::ActivateAbility && intent.ability_source == market);
    }, nullptr, nullptr, P1);
    EXPECT_TRUE(state.player(P1).hand.empty());
}

}  // namespace test_radiance_review
}  // namespace riftbound::test
