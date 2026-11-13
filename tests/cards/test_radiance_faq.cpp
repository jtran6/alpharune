// Review fixes for the Radiance payment and move rules. Iron Revenant's
// Power pays unit and gear plays only. Restorationist's [Y] replaces the
// printed cost and still takes increases. Atakhan's discount requires the
// kill. Clocktower Guardian's banished card has no Ending Phase. The player
// who performs a move pays Gravity Field once per destination, and may
// decline. The Exile readies a unit on a battlefield that player controls
// as it enters, so Baron Nashor's uncontrolled Pit does not.

#include "tests/cards/card_test_fixture.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

namespace riftbound::test {
namespace test_radiance_faq {

class RadianceFaq : public CardTestFixture {
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

    GameObjectId gearAt(PlayerId owner, CardDefId def_id, int at_bf) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        const auto& def = card_db.get(def_id);
        obj.name = def.name;
        obj.card_type = CardType::Gear;
        obj.keywords = def.keywords;   // [Deploy] keeps it at the battlefield
        obj.zone = ZoneType::BattlefieldZone;
        obj.location = BattlefieldLocation{BattlefieldId::fromRaw(at_bf)};
        return id;
    }

    void toTrash(PlayerId owner, GameObjectId card) {
        auto& hand = state.player(owner).hand;
        hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
        state.getObject(card).zone = ZoneType::Trash;
        state.getObject(card).location.reset();
        state.player(owner).trash.push_back(card);
    }

    void toBanishment(PlayerId owner, GameObjectId card, int through) {
        auto& hand = state.player(owner).hand;
        hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
        state.getObject(card).zone = ZoneType::Banishment;
        state.getObject(card).location.reset();
        state.player(owner).banishment.push_back(card);
        state.getObject(card).card_counters["__playable_through_turn"] = through;
    }

    bool playOffered(PlayerId who, GameObjectId card) {
        bool yes = false;
        withEngine([&](GameEngine& e) {
            for (const auto& i : e.testHook_mainPhaseActions(who))
                if (i.type == IntentType::PlayCard && i.card == card) yes = true;
        }, nullptr, nullptr, who);
        return yes;
    }
};

TEST_F(RadianceFaq, IronRevenantPowerPaysUnitsAndGear) {
    legend(P1, 1064);
    auto corpse = addToHand(P1, 13);
    toTrash(P1, corpse);
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = state.player(P1).legend_zone;
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(corpse.raw()), EnginePathAgent::kRest}));
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(act); }, &agent);
    EXPECT_EQ(state.player(P1).rune_pool.unit_gear_power, 1);
    EXPECT_EQ(state.player(P1).rune_pool.gear_power, 0);
    EXPECT_NE(std::find(state.player(P1).banishment.begin(),
                        state.player(P1).banishment.end(), corpse),
              state.player(P1).banishment.end());
}

TEST_F(RadianceFaq, RestorationistAlternateCostSpendsUnitPower) {
    auto rest = addToHand(P1, 1048);
    toTrash(P1, rest);
    state.player(P1).rune_pool.unit_gear_power = 1;
    auto big = addToHand(P1, 75);   // Tasty Faefolk, Energy 7
    EnginePathAgent agent(EnginePathAgent::script({1, EnginePathAgent::kRest}));
    playThroughEngine(big, &agent);
    EXPECT_EQ(state.getObject(rest).zone, ZoneType::Base);
    EXPECT_EQ(state.player(P1).rune_pool.unit_gear_power, 0);
    EXPECT_EQ(state.player(P1).rune_pool.gear_power, 0);
}

TEST_F(RadianceFaq, AtakhanKillsAfterThePowerIsPaid) {
    auto fodder = addUnit(P1, 13);
    auto atakhan = addToHand(P1, 732);
    bool alive_while_paying = false;
    bool saw_payment = false;
    EnginePathAgent agent(EnginePathAgent::script(
        {1, static_cast<int>(fodder.raw()), EnginePathAgent::kRest}));
    withEngine([&](GameEngine& e) {
        auto conn = events.on_log.connect([&](const LogEvent& ev) {
            if (ev.message.find("PAY_COST: ") == std::string::npos) return;
            if (ev.message.find("Atakhan") == std::string::npos) return;
            saw_payment = true;
            auto& live = e.mutableState();
            alive_while_paying = live.objectExists(fodder) &&
                                 live.getObject(fodder).location.has_value();
        });
        e.mutableState().player(P1).rune_pool.energy += 10;
        e.mutableState().player(P1).rune_pool.universal_power += 3;
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = atakhan;
        play.play_location = BaseLocation{P1};
        e.testHook_executeIntent(play);
        settleLikeTheEngine(e);
        conn.disconnect();
    }, &agent);
    EXPECT_TRUE(saw_payment);
    EXPECT_TRUE(alive_while_paying);
    EXPECT_TRUE(state.getObject(atakhan).location.has_value());
    EXPECT_TRUE(inTrash(P1, fodder));
}

TEST_F(RadianceFaq, BanishedCardClosesWhenTheEndingPhaseBegins) {
    auto card = addToHand(P1, 13);
    state.player(P1).rune_pool.energy = 20;
    state.turn.turn_number = 4;
    state.turn.turn_player = P1;
    toBanishment(P1, card, /*through=*/4);

    state.turn.phase = TurnPhase::MainPhase;
    EXPECT_TRUE(playOffered(P1, card));

    state.turn.phase = TurnPhase::EndingStep;
    EXPECT_FALSE(playOffered(P1, card)) << "the permission ends as the Ending Phase begins";

    // Killed during this Ending Phase: playable until the next one begins.
    state.getObject(card).card_counters["__playable_through_turn"] = 5;
    EXPECT_TRUE(playOffered(P1, card));

    state.turn.turn_number = 5;
    state.turn.phase = TurnPhase::MainPhase;
    EXPECT_TRUE(playOffered(P1, card));
    state.turn.phase = TurnPhase::EndingStep;
    EXPECT_FALSE(playOffered(P1, card));
}

TEST_F(RadianceFaq, CharmPaysGravityFieldUnlessTheCasterControlsIt) {
    auto enemy = addUnit(P2, kInvalidId, 2);   // no Deflect; Charm would pay it
    auto charm = addToHand(P1, 43);
    gearAt(P2, 1000, 1);
    state.player(P1).rune_pool.energy = 1;
    state.player(P1).rune_pool.universal_power = 1;
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(enemy.raw()), 2, 1, EnginePathAgent::kRest}));
    playThroughEngine(charm, &agent, std::nullopt, nullptr, /*fund=*/true);
    ASSERT_TRUE(state.getObject(enemy).location.has_value());
    EXPECT_TRUE(std::holds_alternative<BattlefieldLocation>(*state.getObject(enemy).location));
    EXPECT_EQ(std::get<BattlefieldLocation>(*state.getObject(enemy).location).id.raw(), 1);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
}

TEST_F(RadianceFaq, CharmDoesNotPayItsOwnGravityField) {
    auto enemy = addUnit(P2, kInvalidId, 2);
    auto charm = addToHand(P1, 43);
    gearAt(P1, 1000, 1);
    state.player(P1).rune_pool.energy = 1;
    state.player(P1).rune_pool.universal_power = 1;
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(enemy.raw()), 2, EnginePathAgent::kRest}));
    playThroughEngine(charm, &agent);
    ASSERT_TRUE(state.getObject(enemy).location.has_value());
    EXPECT_EQ(std::get<BattlefieldLocation>(*state.getObject(enemy).location).id.raw(), 1);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1);
}

TEST_F(RadianceFaq, CharmDoesNotMoveWhenTheArrivalCostCannotBePaid) {
    auto enemy = addUnit(P2, kInvalidId, 2);
    auto charm = addToHand(P1, 43);
    gearAt(P2, 1000, 1);
    // Yes to the arrival cost: the pool holds only Charm's own cost, so the
    // move is not paid for and the unit stays.
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(enemy.raw()), 2, 1, EnginePathAgent::kRest}));
    playThroughEngine(charm, &agent);
    ASSERT_TRUE(state.getObject(enemy).location.has_value());
    EXPECT_TRUE(std::holds_alternative<BaseLocation>(*state.getObject(enemy).location));
}

TEST_F(RadianceFaq, AnglerOpponentPaysAndCannotPickAnImmovableUnit) {
    auto wyrm = addUnit(P1, 1042, 0, 0);
    gearAt(P1, 1000, 0);
    auto stuck = addUnit(P2, 13);
    auto mover = addUnit(P2, 13);
    auto other = addUnit(P2, 13);   // a second unit that can move, so the pick is asked
    state.getObject(stuck).cant_move_this_turn = true;
    state.player(P2).rune_pool.energy = 1;
    state.player(P2).rune_pool.universal_power = 1;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = wyrm;
    EnginePathAgent p2(EnginePathAgent::script(
        {static_cast<int>(mover.raw()), 1, EnginePathAgent::kRest}));
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(act);
        settleLikeTheEngine(e);
    }, nullptr, &p2);
    ASSERT_FALSE(p2.prompts.empty());
    for (const auto& opt : p2.prompts.front())
        for (auto id : opt.chosen_objects)
            EXPECT_NE(id, stuck);
    EXPECT_EQ(std::get<BattlefieldLocation>(*state.getObject(mover).location).id.raw(), 0);
    EXPECT_TRUE(std::holds_alternative<BaseLocation>(*state.getObject(stuck).location));
    EXPECT_TRUE(std::holds_alternative<BaseLocation>(*state.getObject(other).location));
    EXPECT_EQ(state.player(P2).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P2).rune_pool.universal_power, 0);
}

TEST_F(RadianceFaq, AnglerDoesNothingWhenNoUnitCanMove) {
    auto wyrm = addUnit(P1, 1042, 0, 0);
    auto stuck = addUnit(P2, 13);
    state.getObject(stuck).cant_move_this_turn = true;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = wyrm;
    EnginePathAgent p2;
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(act);
        settleLikeTheEngine(e);
    }, nullptr, &p2);
    EXPECT_TRUE(p2.prompts.empty());
    EXPECT_TRUE(std::holds_alternative<BaseLocation>(*state.getObject(stuck).location));
}

TEST_F(RadianceFaq, AnglerDeclineLeavesTheUnitWhereItIs) {
    auto wyrm = addUnit(P1, 1042, 0, 0);
    gearAt(P1, 1000, 0);
    auto mover = addUnit(P2, 13);
    state.player(P2).rune_pool.energy = 1;
    state.player(P2).rune_pool.universal_power = 1;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = wyrm;
    // One legal unit is taken without a prompt. The payment is what is asked.
    EnginePathAgent p2(EnginePathAgent::script({0, EnginePathAgent::kRest}));
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(act);
        settleLikeTheEngine(e);
    }, nullptr, &p2);
    EXPECT_TRUE(std::holds_alternative<BaseLocation>(*state.getObject(mover).location));
    EXPECT_EQ(state.player(P2).rune_pool.energy, 1);
    EXPECT_EQ(state.player(P2).rune_pool.universal_power, 1);
}

TEST_F(RadianceFaq, BaronEntersReadyAtThePitFromABattlefieldYouControl) {
    legend(P1, 1068);
    addUnit(P1, 13, 0, 0);
    state.battlefields[0].controller = P1;
    auto baron = addToHand(P1, 709);
    EnginePathAgent agent(EnginePathAgent::script({EnginePathAgent::kRest}));
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        ASSERT_TRUE(e.mutableState().player(P1).units_enter_ready_at_your_bf);
        e.mutableState().player(P1).rune_pool.energy += 10;
        e.mutableState().player(P1).rune_pool.universal_power += 3;
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = baron;
        play.play_location = BattlefieldLocation{BattlefieldId::fromRaw(0)};
        e.testHook_executeIntent(play);
        settleLikeTheEngine(e);
    }, &agent);
    ASSERT_TRUE(state.getObject(baron).location.has_value());
    ASSERT_TRUE(std::holds_alternative<BattlefieldLocation>(*state.getObject(baron).location));
    const auto pit = std::get<BattlefieldLocation>(*state.getObject(baron).location).id;
    EXPECT_NE(pit.raw(), 0);
    EXPECT_EQ(state.getObject(state.battlefields[pit.raw()].card_object_id).name, "Baron Pit");
    EXPECT_TRUE(state.getObject(baron).is_exhausted);
}

TEST_F(RadianceFaq, IronRevenantPowerDoesNotPayAbilitiesOrEquip) {
    state.player(P1).rune_pool.unit_gear_power = 1;
    withEngine([&](GameEngine& e) {
        auto& ex = e.testHook_executor();
        auto& pool = e.mutableState().player(P1).rune_pool;
        EXPECT_FALSE(ex.payResources(P1, 0, 1, resource::kAnyDomain, SpendFor::Unit));
        EXPECT_FALSE(ex.payResources(P1, 0, 1, resource::kAnyDomain, SpendFor::Gear));
        EXPECT_EQ(pool.unit_gear_power, 1);
        EXPECT_TRUE(ex.payResources(P1, 0, 1, resource::kAnyDomain, SpendFor::PlayGear));
        EXPECT_EQ(pool.unit_gear_power, 0);
        pool.unit_gear_power = 1;
        EXPECT_TRUE(ex.payResources(P1, 0, 1, resource::kAnyDomain, SpendFor::PlayUnit));
        EXPECT_EQ(pool.unit_gear_power, 0);
    });
}

TEST_F(RadianceFaq, AtakhanChargesTheDiscountBackWhenTheUnitIsGone) {
    auto fodder = addUnit(P1, 13);
    auto atakhan = addToHand(P1, 732);
    const int fodder_energy = card_db.get(13).energy_cost;
    EnginePathAgent agent(EnginePathAgent::script(
        {1, static_cast<int>(fodder.raw()), EnginePathAgent::kRest}));
    withEngine([&](GameEngine& e) {
        bool took = false;
        auto conn = events.on_log.connect([&](const LogEvent& ev) {
            if (took || ev.message.find("PAY_COST: ") == std::string::npos) return;
            if (ev.message.find("Atakhan") == std::string::npos) return;
            took = true;
            e.testHook_executor().banishObject(fodder);
        });
        e.mutableState().player(P1).rune_pool.energy += 10;
        e.mutableState().player(P1).rune_pool.universal_power += 3;
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = atakhan;
        play.play_location = BaseLocation{P1};
        e.testHook_executeIntent(play);
        settleLikeTheEngine(e);
        conn.disconnect();
        EXPECT_TRUE(took);
    }, &agent);
    EXPECT_TRUE(state.getObject(atakhan).location.has_value());
    EXPECT_FALSE(inTrash(P1, fodder));
    // Printed cost is 10 / 3. The poro's Energy was waived and then paid
    // back, so nothing of the 10 / 3 remains. A kill that never happens
    // would have left the poro's Energy in the pool.
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
    EXPECT_GT(fodder_energy, 0);
}

TEST_F(RadianceFaq, ClocktowerBanishedCardStaysPlayableIntoTheEndingPhase) {
    auto guardian = addUnit(P1, 1005);
    auto top = addToDeck(P1, 13);
    state.turn.turn_number = 3;
    state.turn.turn_player = P1;
    state.turn.phase = TurnPhase::MainPhase;
    fireTriggerThroughEngine(guardian, TriggerType::WhenIDie);
    EXPECT_EQ(state.getObject(top).zone, ZoneType::Banishment);
    EXPECT_EQ(state.getObject(top).card_counters.count("__playable_through_turn"), 0u);
    state.turn.phase = TurnPhase::EndingStep;
    state.player(P1).rune_pool.energy = 0;
    EXPECT_TRUE(playOffered(P1, top));
}

TEST_F(RadianceFaq, CharmDecliningTheArrivalCostDoesNotMove) {
    auto enemy = addUnit(P2, kInvalidId, 2);
    auto charm = addToHand(P1, 43);
    gearAt(P2, 1000, 1);
    state.player(P1).rune_pool.energy = 5;
    state.player(P1).rune_pool.universal_power = 5;
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(enemy.raw()), 2, 0, EnginePathAgent::kRest}));
    playThroughEngine(charm, &agent, std::nullopt, nullptr, /*fund=*/false);
    EXPECT_TRUE(std::holds_alternative<BaseLocation>(*state.getObject(enemy).location));
    // Charm's own [1] and [A] were paid. The arrival cost was not.
    EXPECT_EQ(state.player(P1).rune_pool.energy, 4);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 4);
}

TEST_F(RadianceFaq, TricksyTentaclesPaysGravityFieldOnceForTheGroup) {
    auto a = addUnit(P2, kInvalidId, 1);
    auto b = addUnit(P2, kInvalidId, 1);
    auto spell = addToHand(P1, 616);
    gearAt(P2, 1000, 0);
    state.player(P1).rune_pool.energy = 1;
    state.player(P1).rune_pool.universal_power = 1;
    // Both enemies are picked, so there is no "Done" prompt: the next
    // question is the destination (1 = battlefield 0), then one Yes for
    // the group's arrival cost.
    EnginePathAgent agent(EnginePathAgent::script({
        static_cast<int>(a.raw()), static_cast<int>(b.raw()), 1, 1,
        EnginePathAgent::kRest}));
    playThroughEngine(spell, &agent);
    ASSERT_TRUE(state.getObject(a).battlefieldId().has_value());
    ASSERT_TRUE(state.getObject(b).battlefieldId().has_value());
    EXPECT_EQ(state.getObject(a).battlefieldId()->raw(), 0);
    EXPECT_EQ(state.getObject(b).battlefieldId()->raw(), 0);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
}

TEST_F(RadianceFaq, SirensSongPaysAnArrivalCost) {
    auto enemy = addUnit(P2, kInvalidId, 1);
    state.getObject(enemy).moves_this_turn = 1;
    auto spell = addToHand(P1, 1107);
    gearAt(P2, 1000, 0);
    state.player(P1).rune_pool.energy = 1;
    state.player(P1).rune_pool.universal_power = 1;
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(enemy.raw()), 1, EnginePathAgent::kRest}));
    playThroughEngine(spell, &agent);
    ASSERT_TRUE(state.getObject(enemy).battlefieldId().has_value());
    EXPECT_EQ(state.getObject(enemy).battlefieldId()->raw(), 0);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
}

TEST_F(RadianceFaq, AnglerOpponentMayMoveAUnitEnemiesCannotMove) {
    auto wyrm = addUnit(P1, 1042, 0, 0);
    gearAt(P1, 1000, 0);
    auto locked = addUnit(P2, 13);
    state.getObject(locked).cant_be_moved_by_enemy = true;
    state.player(P2).rune_pool.energy = 1;
    state.player(P2).rune_pool.universal_power = 1;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = wyrm;
    EnginePathAgent p2(EnginePathAgent::script({1, EnginePathAgent::kRest}));
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(act);
        settleLikeTheEngine(e);
    }, nullptr, &p2);
    ASSERT_TRUE(state.getObject(locked).battlefieldId().has_value());
    EXPECT_EQ(state.getObject(locked).battlefieldId()->raw(), 0);
    EXPECT_EQ(state.player(P2).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P2).rune_pool.universal_power, 0);
}

TEST_F(RadianceFaq, RestorationistTaxAppliesToTheAlternateCost) {
    auto rest = addToHand(P1, 1048);
    toTrash(P1, rest);
    state.player(P1).rune_pool.unit_gear_power = 1;
    state.player(P1).rune_pool.energy = 1;
    PlayerState::CostModifier tax;
    tax.energy_increase = 1;
    tax.units_only = true;
    tax.this_turn_only = true;
    state.player(P1).cost_modifiers.push_back(tax);
    // A spell, so the unit tax is not also the cost of the card that
    // triggers the play, and its Power is not Iron Revenant Power.
    auto big = addToHand(P1, 1115);
    EnginePathAgent agent(EnginePathAgent::script({1, EnginePathAgent::kRest}));
    playThroughEngine(big, &agent);
    EXPECT_EQ(state.getObject(rest).zone, ZoneType::Base);
    EXPECT_EQ(state.player(P1).rune_pool.unit_gear_power, 0);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

TEST_F(RadianceFaq, HuntersCircleDiscountsOnlyAPlayAtThatBattlefield) {
    auto id = state.createObject();
    auto& circle = state.getObject(id);
    circle.card_def_id = 1080;
    circle.name = card_db.get(1080).name;
    circle.card_type = CardType::Battlefield;
    circle.owner = P1;
    circle.controller = P1;
    circle.zone = ZoneType::BattlefieldZone;
    state.battlefields[0].card_object_id = id;
    state.battlefields[0].controller = P1;
    addUnit(P1, 13, 0, 0);   // keeps control through the cleanup
    auto here = addToHand(P1, 13);
    auto elsewhere = addToHand(P1, 13);
    EnginePathAgent agent(EnginePathAgent::script({EnginePathAgent::kRest}));
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        // Elsewhere first: a discount that ignored the battlefield would
        // take [1] off this play and use up the once-per-turn permission.
        e.mutableState().player(P1).rune_pool.energy = 2;
        Intent other;
        other.type = IntentType::PlayCard;
        other.player = P1;
        other.card = elsewhere;
        other.play_location = BattlefieldLocation{BattlefieldId::fromRaw(1)};
        e.testHook_executeIntent(other);
        settleLikeTheEngine(e);
        EXPECT_EQ(e.mutableState().player(P1).rune_pool.energy, 0);
        e.mutableState().player(P1).rune_pool.energy = 2;
        Intent at;
        at.type = IntentType::PlayCard;
        at.player = P1;
        at.card = here;
        at.play_location = BattlefieldLocation{BattlefieldId::fromRaw(0)};
        e.testHook_executeIntent(at);
        settleLikeTheEngine(e);
        EXPECT_EQ(e.mutableState().player(P1).rune_pool.energy, 1);
    }, &agent);
}

// A ready gear at base. gearAt puts [Deploy] gear on a battlefield; this is
// an [Add] source, and it has to be ready or the exhaust cost cannot be paid.
GameObjectId readyGearAtBase(PlayerId owner, CardDefId def_id, const CardDB& db, GameState& state) {
    auto id = state.createObject();
    auto& obj = state.getObject(id);
    obj.owner = owner;
    obj.controller = owner;
    obj.card_def_id = def_id;
    obj.name = db.get(def_id).name;
    obj.card_type = CardType::Gear;
    obj.zone = ZoneType::Base;
    obj.location = BaseLocation{owner};
    return id;
}

TEST_F(RadianceFaq, GravityFieldArrivalCanBePaidWithAnAdd) {
    auto enemy = addUnit(P2, kInvalidId, 2);
    auto charm = addToHand(P1, 43);
    gearAt(P2, 1000, 1);
    auto fruit = readyGearAtBase(P1, 611, card_db, state);
    // Charm's own [1][A], plus the arrival Energy. The arrival Power is
    // not in the pool: Honeyfruit's [Add] [A] is the only way to pay it.
    state.player(P1).rune_pool.energy = 2;
    state.player(P1).rune_pool.universal_power = 1;
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(enemy.raw()), 2, 1, EnginePathAgent::kRest}));
    playThroughEngine(charm, &agent, std::nullopt, nullptr, /*fund=*/false);
    ASSERT_TRUE(state.getObject(enemy).battlefieldId().has_value());
    EXPECT_EQ(state.getObject(enemy).battlefieldId()->raw(), 1);
    EXPECT_TRUE(state.getObject(fruit).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
}

TEST_F(RadianceFaq, RestorationistAlternateCostCanBePaidWithAnAdd) {
    auto rest = addToHand(P1, 1048);
    toTrash(P1, rest);
    auto fruit = readyGearAtBase(P1, 611, card_db, state);
    auto big = addToHand(P1, 1115);   // Raucous Anthem, Energy 7, no choices
    EnginePathAgent agent(EnginePathAgent::script({1, EnginePathAgent::kRest}));
    playThroughEngine(big, &agent);
    EXPECT_EQ(state.getObject(rest).zone, ZoneType::Base);
    EXPECT_TRUE(state.getObject(fruit).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
}

}  // namespace test_radiance_faq
}  // namespace riftbound::test
