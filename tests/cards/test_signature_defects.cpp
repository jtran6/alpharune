/// One test per signature defect named in the signature audit.
/// Each one is written against the printed line, so it fails while the
/// card still does the other thing.

#include "card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace riftbound::test {
namespace signature_defects {
namespace {

class SignatureDefectTest : public CardTestFixture {
protected:
    static constexpr CardDefId kBlade = 1069;
    static constexpr CardDefId kBall = 1066;
    static constexpr CardDefId kIcathian = 247;
    static constexpr CardDefId kGuillotine = 251;
    static constexpr CardDefId kSiphon = 933;
    static constexpr CardDefId kPursuit = 503;
    static constexpr CardDefId kFoxFire = 253;
    static constexpr CardDefId kRage = 255;
    static constexpr CardDefId kSpark = 320;
    static constexpr CardDefId kRocket = 250;
    static constexpr CardDefId kStorm = 248;
    static constexpr CardDefId kAlpha = 751;
    static constexpr CardDefId kVoidRush = 505;
    static constexpr CardDefId kCounter = 510;
    static constexpr CardDefId kCurtain = 743;
    static constexpr CardDefId kBelow = 747;
    static constexpr CardDefId kChrono = 1117;
    static constexpr CardDefId kMines = 1118;
    static constexpr CardDefId kNtofo = 1120;
    static constexpr CardDefId kEncore = 1121;
    static constexpr CardDefId kPyke = 707;
    static constexpr CardDefId kNocturne = 194;
    static constexpr CardDefId kCutlass = 860;
    static constexpr CardDefId kCloth = 387;
    static constexpr CardDefId kHelm = 832;
    static constexpr CardDefId kFiller = 48;

    GameObject& o(GameObjectId id) { return state.getObject(id); }

    void fund(GameObjectId card) {
        const auto& def = card_db.get(state.getObject(card).card_def_id);
        auto& pool = state.player(state.getObject(card).controller).rune_pool;
        pool.energy += def.energy_cost;
        pool.universal_power += def.power_cost;
    }
    Intent playOf(GameObjectId card, std::vector<GameObjectId> targets = {}) {
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = state.getObject(card).controller;
        play.card = card;
        play.targets = std::move(targets);
        return play;
    }
    void play(GameObjectId card, AgentInterface* agent, bool pay = true,
              std::vector<GameObjectId> targets = {}) {
        if (pay) fund(card);
        Intent intent = playOf(card, std::move(targets));
        withEngine([&](GameEngine& e) {
            e.testHook_executeIntent(intent);
            settleLikeTheEngine(e);
        }, agent, nullptr, state.getObject(card).controller);
    }
    /// The first time the opponent has priority, `probe` sees the live state.
    /// The chain then runs on, so the spell resolves after the probe returns.
    void playAndProbe(GameObjectId card, AgentInterface* agent,
                      const std::function<void(GameState&)>& probe, bool pay = true,
                      std::vector<GameObjectId> targets = {}) {
        if (pay) fund(card);
        Intent intent = playOf(card, std::move(targets));
        GameState* live = nullptr;
        bool probed = false;
        PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
            if (!probed && live) { probed = true; probe(*live); }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        });
        withEngine([&](GameEngine& e) {
            live = &e.mutableState();
            e.testHook_executeIntent(intent);
            e.testHook_runChain();
            live = nullptr;
        }, agent, &p2, state.getObject(card).controller);
        EXPECT_TRUE(probed);
    }
    GameObjectId gear(PlayerId who, CardDefId def, int at_bf = -1) {
        auto id = addUnit(who, def, 0, at_bf);
        o(id).card_type = CardType::Gear;
        o(id).base_might = 0;
        o(id).current_might = 0;
        return id;
    }
    void toRuneDeck(GameObjectId rune) {
        auto& r = o(rune);
        r.zone = ZoneType::RuneDeck;
        r.location = std::nullopt;
        state.player(r.owner).rune_deck.push_back(rune);
    }
    void backToHand(PlayerId who, GameObjectId card) {
        auto& ps = state.player(who);
        ps.trash.erase(std::remove(ps.trash.begin(), ps.trash.end(), card), ps.trash.end());
        ps.hand.erase(std::remove(ps.hand.begin(), ps.hand.end(), card), ps.hand.end());
        o(card).zone = ZoneType::Hand;
        o(card).location = std::nullopt;
        ps.hand.push_back(card);
    }
    int goldOnBoard() const {
        int n = 0;
        for (const auto& [id, obj] : state.objects) {
            (void)id;
            if (obj.card_def_id == kGoldGearCardDefId && obj.location.has_value()) ++n;
        }
        return n;
    }
    int bombsOnBoard() const {
        int n = 0;
        for (const auto& [id, obj] : state.objects) {
            (void)id;
            if (obj.card_def_id == kBombGearCardDefId && obj.location.has_value()) ++n;
        }
        return n;
    }
    /// Pyke, Returned is already lethally damaged when `victim` is damaged,
    /// so a kill that does not share his death lets him play a Gold.
    void playWithPykeAlreadyLethal(GameObjectId spell, GameObjectId victim, GameObjectId pyke,
                                   AgentInterface* agent,
                                   std::vector<GameObjectId> targets = {}) {
        fund(spell);
        Intent intent = playOf(spell, std::move(targets));
        GameState* live = nullptr;
        auto conn = events.on_damage_dealt.connect([&](const DamageDealtEvent& e) {
            if (!live || e.target != victim || e.is_combat_damage) return;
            auto& w = live->getObject(pyke);
            if (w.location.has_value()) w.damage_marked = std::max(w.damage_marked, w.current_might);
        });
        withEngine([&](GameEngine& e) {
            live = &e.mutableState();
            e.testHook_executeIntent(intent);
            settleLikeTheEngine(e);
            live = nullptr;
        }, agent, nullptr, P1);
        conn.disconnect();
    }
};

TEST_F(SignatureDefectTest, BladeOfTheExile_GrantsMightAndGankingWhileAttached) {
    auto unit = addUnit(P1, kInvalidId, 4, 0);
    auto blade = gear(P1, kBlade);
    state.player(P1).rune_pool.universal_power = 1;
    EnginePathAgent agent(EnginePathAgent::script({0}));   // "Pay [A]"
    ASSERT_TRUE(equipThroughEngine(blade, unit, &agent));
    EXPECT_EQ(o(unit).current_might, 7);
    EXPECT_TRUE(o(unit).hasKeyword(Keyword::Ganking));
    EXPECT_FALSE(o(unit).is_exhausted);
    withEngine([&](GameEngine& e) {
        bool to_other = false;
        for (const auto& intent : e.testHook_mainPhaseActions(P1)) {
            if (intent.type != IntentType::StandardMove) continue;
            if (intent.units_to_move.empty() || intent.units_to_move.front() != unit) continue;
            if (!intent.move_destination) continue;
            if (const auto* bf = std::get_if<BattlefieldLocation>(&*intent.move_destination))
                to_other = to_other || bf->id.raw() == 1;
        }
        EXPECT_TRUE(to_other) << "Ganking moves from battlefield to battlefield";
    });
}

TEST_F(SignatureDefectTest, TheBall_DoesNotTriggerWhenAnOpponentMovesIt) {
    auto ball = gear(P1, kBall, 0);
    // Deal 2, not Draw 1: the deck is empty, and drawing it burns the player out.
    EnginePathAgent enemy_move(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.moveToBattlefield(ball, BattlefieldId::fromRaw(1), P2);
    }, &enemy_move);
    EXPECT_TRUE(enemy_move.prompts.empty()) << "an opponent's spell moved it: no mode";
    EXPECT_EQ(o(ball).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});

    EnginePathAgent own_move(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.moveToBattlefield(ball, BattlefieldId::fromRaw(0), P1);
    }, &own_move);
    EXPECT_FALSE(own_move.prompts.empty()) << "its controller's move still chooses a mode";
}

TEST_F(SignatureDefectTest, IcathianRain_PaysDeflectEveryTimeTheUnitIsChosen) {
    auto enemy = addUnit(P2, kInvalidId, 20, 0);
    o(enemy).keywords.set(Keyword::Deflect);
    o(enemy).deflect_value = 1;
    auto spell = addToHand(P1, kIcathian);
    // The spell's [3] plus one Deflect. A second choice of the same unit is
    // another Deflect (CR 809.1.c) and cannot be paid.
    state.player(P1).rune_pool.energy = 7;
    state.player(P1).rune_pool.universal_power = 4;
    EnginePathAgent agent([enemy](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == enemy) return i;
        return legal.front();
    });
    play(spell, &agent, /*pay=*/false);
    EXPECT_EQ(o(enemy).damage_marked, 2) << "one instance, not six for one Deflect";
}

TEST_F(SignatureDefectTest, NoxianGuillotine_AReplayedUnitIsNotTheUnitItChose) {
    auto unit = addUnit(P1, kInvalidId, 5, 0);
    auto spell = addToHand(P1, kGuillotine);
    EnginePathAgent agent(EnginePathAgent::choosing(unit));
    play(spell, &agent);
    ASSERT_TRUE(o(unit).location.has_value()) << "Legion is off: it is not killed now";
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.bounceToHand(unit);
        ex.playIgnoringCost(P1, unit, LocationId{BattlefieldLocation{BattlefieldId::fromRaw(0)}});
        ex.dealDamage(unit, 1, kInvalidId);
    });
    EXPECT_TRUE(o(unit).location.has_value()) << "the replayed unit is a new object";
    EXPECT_EQ(o(unit).damage_marked, 1);
}

TEST_F(SignatureDefectTest, SiphoningStrike_AReplayedUnitDoesNotChannelWhenItDies) {
    auto unit = addUnit(P2, kInvalidId, 10, 0);
    auto rune = addRune(P1, Domain::Calm);
    toRuneDeck(rune);
    auto spell = addToHand(P1, kSiphon);
    EnginePathAgent agent(EnginePathAgent::choosing(unit));
    play(spell, &agent);
    EXPECT_EQ(o(unit).damage_marked, 4);
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.bounceToHand(unit);
        ex.playIgnoringCost(P2, unit, LocationId{BattlefieldLocation{BattlefieldId::fromRaw(0)}});
        ex.killObject(unit);
    });
    EXPECT_EQ(state.player(P1).rune_deck.size(), 1u);
    EXPECT_FALSE(o(rune).location.has_value());
}

TEST_F(SignatureDefectTest, RelentlessPursuit_DecliningTheConquerDoesNotSpendTheRider) {
    auto unit = addUnit(P1, kInvalidId, 3);   // base; the spell moves it to a battlefield
    auto spell = addToHand(P1, kPursuit);
    // Mode 1 is Battlefield #0. The conquer itself is scored afterwards: moving
    // there stages a showdown, and the rider is what a conquer offers.
    EnginePathAgent cast(EnginePathAgent::script({static_cast<int>(unit.raw()), 1}));
    play(spell, &cast);
    ASSERT_TRUE(o(unit).isAtBattlefield());
    ASSERT_EQ(state.delayed_abilities.size(), 1u);

    EnginePathAgent first;   // declines the "may"
    withEngine([&](GameEngine& e) {
        e.testHook_scoreConquer(P1, BattlefieldId::fromRaw(0));
        e.testHook_runChain();
    }, &first);
    EXPECT_EQ(first.prompts.size(), 1u);

    // The same unit conquers again. Declining the first one did not spend it.
    state.player(P1).battlefields_scored_this_turn.clear();
    EnginePathAgent second;
    withEngine([&](GameEngine& e) {
        e.testHook_scoreConquer(P1, BattlefieldId::fromRaw(0));
        e.testHook_runChain();
    }, &second);
    EXPECT_EQ(second.prompts.size(), 1u) << "declining the first conquer leaves the ability";

    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.bounceToHand(unit);
        ex.playIgnoringCost(P1, unit, LocationId{BattlefieldLocation{BattlefieldId::fromRaw(0)}});
    });
    state.player(P1).battlefields_scored_this_turn.clear();
    EnginePathAgent replayed;
    withEngine([&](GameEngine& e) {
        e.testHook_scoreConquer(P1, BattlefieldId::fromRaw(0));
        e.testHook_runChain();
    }, &replayed);
    EXPECT_TRUE(replayed.prompts.empty()) << "a new object does not keep the rider";
}

TEST_F(SignatureDefectTest, FoxFire_AReplayedUnitIsNotKilled) {
    auto unit = addUnit(P2, kInvalidId, 2, 0);
    auto spell = addToHand(P1, kFoxFire);
    int picks = 0;
    EnginePathAgent agent([unit, &picks](const std::vector<Intent>& legal) {
        if (picks == 0)
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == unit) {
                    ++picks;
                    return i;
                }
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 0) return i;
        return legal.front();
    });
    playAndProbe(spell, &agent, [&](GameState& live) {
        auto& u = live.getObject(unit);
        u.location = std::nullopt;
        u.zone = ZoneType::Hand;
        u.zone = ZoneType::BattlefieldZone;
        u.location = BattlefieldLocation{BattlefieldId::fromRaw(0)};
    });
    EXPECT_TRUE(o(unit).location.has_value());
    EXPECT_NE(o(unit).zone, ZoneType::Trash);
}

TEST_F(SignatureDefectTest, DragonsRage_ARefusedMoveDoesNotFight) {
    auto hunted = addUnit(P2, kInvalidId, 3);
    auto bystander = addUnit(P2, kInvalidId, 4);
    auto cutlass = gear(P2, kCutlass);
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.attachGear(cutlass, hunted); });
    auto spell = addToHand(P1, kRage);
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(hunted.raw()), 1, EnginePathAgent::kRest}));
    play(spell, &agent);
    EXPECT_TRUE(o(hunted).isAtBase()) << "Jagged Cutlass: an enemy spell does not move it";
    EXPECT_EQ(o(hunted).damage_marked, 0);
    EXPECT_TRUE(o(bystander).location.has_value());
    EXPECT_EQ(o(bystander).damage_marked, 0);
}

TEST_F(SignatureDefectTest, FinalSpark_AnAlreadyLethalUnitDoesNotSeeTheDeath) {
    auto victim = addUnit(P2, kInvalidId, 5, 0);
    auto pyke = addUnit(P1, kPyke, 1, 1);
    auto spell = addToHand(P1, kSpark);
    EnginePathAgent agent(EnginePathAgent::choosing(victim));
    playWithPykeAlreadyLethal(spell, victim, pyke, &agent);
    EXPECT_FALSE(o(victim).location.has_value());
    EXPECT_FALSE(o(pyke).location.has_value());
    EXPECT_EQ(goldOnBoard(), 0) << "Pyke died with the victim, so he does not see it";
}

TEST_F(SignatureDefectTest, SuperMegaDeathRocket_AnAlreadyLethalUnitDoesNotSeeTheDeath) {
    auto victim = addUnit(P2, kInvalidId, 3, 0);
    auto pyke = addUnit(P1, kPyke, 1, 1);
    auto spell = addToHand(P1, kRocket);
    EnginePathAgent agent(EnginePathAgent::choosing(victim));
    playWithPykeAlreadyLethal(spell, victim, pyke, &agent);
    EXPECT_FALSE(o(victim).location.has_value());
    EXPECT_FALSE(o(pyke).location.has_value());
    EXPECT_EQ(goldOnBoard(), 0);
}

TEST_F(SignatureDefectTest, Stormbringer_AnAlreadyLethalUnitDoesNotSeeTheDeath) {
    auto mover = addUnit(P1, kInvalidId, 5);
    auto victim = addUnit(P2, kInvalidId, 1, 0);
    auto pyke = addUnit(P1, kPyke, 1, 1);
    auto spell = addToHand(P1, kStorm);
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(mover.raw()), 1}));
    playWithPykeAlreadyLethal(spell, victim, pyke, &agent);
    EXPECT_FALSE(o(victim).location.has_value());
    EXPECT_TRUE(o(mover).isAtBattlefield());
    EXPECT_EQ(goldOnBoard(), 0);
    EXPECT_FALSE(o(pyke).location.has_value());
}

TEST_F(SignatureDefectTest, AlphaStrike_AnAlreadyLethalUnitDoesNotGrantXp) {
    auto striker = addUnit(P1, kInvalidId, 4);
    auto victim = addUnit(P2, kInvalidId, 1, 0);
    auto pyke = addUnit(P1, kPyke, 1, 1);
    auto spell = addToHand(P1, kAlpha);
    int step = 0;
    EnginePathAgent agent([victim, &step](const std::vector<Intent>& legal) {
        ++step;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == victim) return i;
        for (const auto& i : legal)
            if (i.chosen_objects.empty() && i.chosen_value.has_value() && *i.chosen_value == 0)
                return i;
        return legal.front();
    });
    playWithPykeAlreadyLethal(spell, victim, pyke, &agent, {striker});
    EXPECT_FALSE(o(victim).location.has_value());
    EXPECT_EQ(state.player(P1).xp, 1) << "only the unit the spell killed";
    EXPECT_EQ(goldOnBoard(), 0);
}

TEST_F(SignatureDefectTest, VoidRush_ACardThatLeavesAsItIsRevealedIsNotDrawn) {
    auto filler = addToDeck(P1, kFiller);
    auto other = addToDeck(P1, kFiller);
    auto nocturne = addToDeck(P1, kNocturne);   // top is the back
    auto spell = addToHand(P1, kVoidRush);
    EnginePathAgent agent([](const std::vector<Intent>& legal) {
        // Nocturne's replacement: banish him. Decline Void Rush's own "may",
        // so the only card that leaves is the one the reveal took.
        for (const auto& i : legal)
            if (i.choice_label.rfind("Yes", 0) == 0 &&
                i.choice_label.find("Nocturne") != std::string::npos)
                return i;
        for (const auto& i : legal)
            if (i.choice_label.rfind("No", 0) == 0) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 0) return i;
        return legal.front();
    });
    play(spell, &agent);
    EXPECT_EQ(o(nocturne).zone, ZoneType::Banishment);
    EXPECT_TRUE(inHand(P1, other));
    EXPECT_TRUE(inDeck(P1, filler)) << "a card that was never revealed is not drawn";
    EXPECT_FALSE(inHand(P1, filler));
}

TEST_F(SignatureDefectTest, CounterStrike_PreventionDoesNotExemptATankFromAssignment) {
    auto tank = addUnit(P1, kInvalidId, 3, 0);
    o(tank).keywords.set(Keyword::Tank);
    auto other = addUnit(P1, kInvalidId, 4, 0);
    auto spell = addToHand(P1, kCounter);
    EnginePathAgent agent(EnginePathAgent::choosing(tank));
    play(spell, &agent);
    ASSERT_TRUE(o(tank).prevent_next_damage_this_turn);
    combatInProgress(0, P2);
    withEngine([&](GameEngine& e) {
        auto adv = e.testHook_advanceCombatDamage(P2, 5, {tank, other});
        ASSERT_FALSE(adv.legal.empty());
        int to_tank = 0;
        for (const auto& a : adv.legal.front().damage_assignments)
            if (a.target_unit == tank) to_tank += a.damage;
        EXPECT_GE(to_tank, 3) << "prevention does not skip the Tank";
    });
}

TEST_F(SignatureDefectTest, CurtainCall_ARepeatedExecutionCannotChooseTheSameMode) {
    addToDeck(P1, kFiller);
    addToDeck(P1, kFiller);
    auto unit = addUnit(P2, kInvalidId, 9, 0);
    auto spell = addToHand(P1, kCurtain);
    state.player(P1).rune_pool.energy = 5;   // [4] and one [1] Repeat
    bool took_repeat = false;
    EnginePathAgent agent([unit, &took_repeat](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 0 &&
                i.choice_label.find("Draw") != std::string::npos)
                return i;
        if (!took_repeat) {
            bool bare = true;
            for (const auto& i : legal)
                if (!i.choice_label.empty() || !i.chosen_objects.empty()) bare = false;
            if (bare && legal.size() == 2) {
                took_repeat = true;
                for (const auto& i : legal)
                    if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
            }
        }
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1 &&
                i.choice_label.find("Deal 2") != std::string::npos)
                return i;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == unit) return i;
        return legal.front();
    });
    play(spell, &agent, /*pay=*/false);
    EXPECT_EQ(handSize(P1), 1) << "Draw runs once";
    EXPECT_EQ(o(unit).damage_marked, 2) << "the repeat took a mode that was still open";
}

TEST_F(SignatureDefectTest, DeathFromBelow_TheReplayPaysSpellCostIncreases) {
    auto first = addUnit(P2, kInvalidId, 3, 0);
    auto second = addUnit(P2, kInvalidId, 3, 1);
    auto helm = gear(P2, kHelm);
    (void)helm;
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });   // the Helm's tax
    auto spell = addToHand(P1, kBelow);
    // Printed [4][A] plus the Helm's [1], and one more [A] that would pay a
    // replay that ignored the tax.
    state.player(P1).rune_pool.energy = 5;
    state.player(P1).rune_pool.universal_power = 2;
    EnginePathAgent agent(EnginePathAgent::choosing(first, EnginePathAgent::May::Yes));
    play(spell, &agent, /*pay=*/false);
    EXPECT_FALSE(o(first).location.has_value());
    EXPECT_TRUE(o(second).location.has_value()) << "the replay is not played without the tax";
    EXPECT_TRUE(inTrash(P1, spell));
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1);
}

TEST_F(SignatureDefectTest, Chronobreak_TheUnitsAreChosenBeforeAnyoneCanRespond) {
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, 1);
    auto c = addUnit(P1, kInvalidId, 2);
    const auto c_born = o(c).zone.incarnation();
    auto spell = addToHand(P1, kChrono);
    state.player(P1).rune_pool.universal_power = 2;
    EnginePathAgent agent([a, b](const std::vector<Intent>& legal) {
        for (auto id : {a, b})
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 2) return i;
        return legal.front();
    });
    playAndProbe(spell, &agent, [&](GameState& live) {
        bool named_a = false, named_b = false;
        for (const auto& item : live.chain.items) {
            if (std::find(item.chosen_objects.begin(), item.chosen_objects.end(), a) !=
                item.chosen_objects.end())
                named_a = true;
            if (std::find(item.chosen_objects.begin(), item.chosen_objects.end(), b) !=
                item.chosen_objects.end())
                named_b = true;
        }
        EXPECT_TRUE(named_a);
        EXPECT_TRUE(named_b);
        auto& dead = live.getObject(a);
        dead.location = std::nullopt;
        dead.zone = ZoneType::Trash;
        live.player(P1).trash.push_back(a);
    }, /*pay=*/true);
    EXPECT_EQ(o(a).zone, ZoneType::Trash);
    EXPECT_TRUE(o(c).isAtBase()) << "a unit that was not chosen is not banished";
    EXPECT_EQ(o(c).zone.incarnation(), c_born);
    EXPECT_TRUE(o(b).location.has_value());
}

TEST_F(SignatureDefectTest, HexplosiveMinefield_EachPlayRemembersItsOwnBombCount) {
    std::vector<GameObjectId> bombs;
    for (int i = 0; i < 4; ++i) {
        auto id = gear(P1, kBombGearCardDefId, 0);
        o(id).tags.push_back("Bomb");
        bombs.push_back(id);
    }
    auto spell = addToHand(P1, kMines);
    EnginePathAgent first([b = bombs[0]](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == b) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 0) return i;
        return legal.front();
    });
    play(spell, &first);
    backToHand(P1, spell);
    EnginePathAgent second([&](const std::vector<Intent>& legal) {
        for (auto id : {bombs[1], bombs[2], bombs[3]})
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 0) return i;
        return legal.front();
    });
    play(spell, &second);
    withEngine([&](GameEngine& e) { e.testHook_endingStep(); });
    EXPECT_EQ(bombsOnBoard(), 4) << "one play's bombs, then the other's: 1 + 3";
}

TEST_F(SignatureDefectTest, NtofoStrikes_DamageEqualsEveryShieldInstance) {
    auto friendly = addUnit(P1, kInvalidId, 3, 0);
    auto enemy = addUnit(P2, kInvalidId, 10, 0);
    auto cloth = gear(P1, kCloth, 0);
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.attachGear(cloth, friendly); });
    auto spell = addToHand(P1, kNtofo);
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(friendly.raw()), static_cast<int>(enemy.raw())}));
    play(spell, &agent);
    EXPECT_EQ(o(enemy).damage_marked, 4) << "Shield 2 from Cloth Armor and Shield 2 this turn";
}

TEST_F(SignatureDefectTest, Encore_ACardThatLeavesAsItIsLookedAtIsNotDrawn) {
    addToDeck(P1, kFiller);
    auto other = addToDeck(P1, kFiller);
    auto nocturne = addToDeck(P1, kNocturne);
    auto spell = addToHand(P1, kEncore);
    EnginePathAgent agent([](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 3 &&
                i.choice_label.find("Look") != std::string::npos)
                return i;
        for (const auto& i : legal)
            if (i.choice_label.rfind("Yes", 0) == 0) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 0) return i;
        return legal.front();
    });
    play(spell, &agent);
    EXPECT_EQ(o(nocturne).zone, ZoneType::Banishment);
    EXPECT_FALSE(inHand(P1, nocturne));
    EXPECT_TRUE(inHand(P1, other));
}

}  // namespace
}  // namespace signature_defects
}  // namespace riftbound::test
