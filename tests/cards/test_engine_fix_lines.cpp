// The illegal lines from the judge review. Each test fails on the engine
// before the fix in docs/engine-fix-plan.md and passes after it.

#include "tests/cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "io/play_by_play.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <variant>

namespace riftbound::test {
namespace test_engine_fix_lines {

class EngineFixLines : public CardTestFixture {
protected:
    static constexpr CardDefId kUnit = 13;          // Pouty Poro, [2]
    static constexpr CardDefId kRose = 1076;
    static constexpr CardDefId kVista = 1077;
    static constexpr CardDefId kMemorial = 1078;
    static constexpr CardDefId kAcademy = 1074;
    static constexpr CardDefId kCircle = 1080;
    static constexpr CardDefId kAmphitheater = 1081;
    static constexpr CardDefId kRakelstake = 1082;
    static constexpr CardDefId kExile = 1068;
    static constexpr CardDefId kInnovator = 1034;   // [3], [Accelerate]
    static constexpr CardDefId kHideout = 1079;
    static constexpr CardDefId kCaress = 1122;
    static constexpr CardDefId kInferna = 560;      // [Ambush], [2]
    static constexpr CardDefId kSongstress = 1070;
    static constexpr CardDefId kHighNote = 1098;    // [3], choices in the finalize pass
    static constexpr CardDefId kShen = 241;         // [3][A], [Reaction]
    static constexpr CardDefId kDeployGear = 1037;  // Bushwhack Trap, [1], [Deploy]

    GameObjectId battlefieldCard(int slot, CardDefId def, PlayerId controller) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.card_def_id = def;
        obj.name = card_db.get(def).name;
        obj.card_type = CardType::Battlefield;
        obj.owner = controller;
        obj.controller = controller;
        obj.zone = ZoneType::BattlefieldZone;
        state.battlefields[slot].card_object_id = id;
        state.battlefields[slot].controller = controller;
        return id;
    }

    void kill(GameObjectId unit) {
        resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(unit); });
    }

    int recruitsOf(PlayerId who) const {
        int n = 0;
        for (const auto& [id, obj] : state.objects) {
            (void)id;
            if (!obj.isUnit() || obj.name != "Recruit" || obj.controller != who) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{BaseLocation{who}}) continue;
            if (obj.base_might != 1) continue;
            ++n;
        }
        return n;
    }

    static int exhaustedGolds(const GameState& st, PlayerId who) {
        int n = 0;
        for (const auto& [id, obj] : st.objects) {
            (void)id;
            if (obj.card_def_id == kGoldGearCardDefId && obj.controller == who &&
                obj.isGear() && obj.is_exhausted)
                ++n;
        }
        return n;
    }

    void playAt(GameEngine& e, GameObjectId card, LocationId where) {
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = e.mutableState().getObject(card).controller;
        play.card = card;
        play.play_location = where;
        e.testHook_executeIntent(play);
    }

    // The later hand play is full price: a [2] unit from 2 Energy leaves 0,
    // and a [1] gear from 1 Energy leaves 0. A discount still in force leaves 1.
    void expectNoDiscount(GameEngine& e, GameObjectId card, int energy) {
        settleLikeTheEngine(e);
        e.mutableState().player(P1).rune_pool.energy = energy;
        playAt(e, card, BattlefieldLocation{BattlefieldId::fromRaw(0)});
        EXPECT_EQ(e.mutableState().player(P1).rune_pool.energy, 0);
    }

    static void toTrash(GameState& st, GameObjectId card) {
        auto& obj = st.getObject(card);
        auto& ps = st.player(obj.owner);
        ps.hand.erase(std::remove(ps.hand.begin(), ps.hand.end(), card), ps.hand.end());
        obj.zone = ZoneType::Trash;
        obj.location.reset();
        ps.trash.push_back(card);
    }

    GameObjectId legend(PlayerId owner, CardDefId def) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.card_def_id = def;
        obj.name = card_db.get(def).name;
        obj.card_type = CardType::Legend;
        obj.owner = owner;
        obj.controller = owner;
        obj.zone = ZoneType::LegendZone;
        state.player(owner).legend_zone = id;
        return id;
    }

    // What separates two turns: the turn number advances, then the next turn
    // starts by clearing both players' this-turn counters (advanceTurnQueue,
    // then runTurn).
    void beginNextTurn() {
        state.turn.turn_number += 1;
        state.player(P1).resetTurnTracking();
        state.player(P2).resetTurnTracking();
    }

    void holdBattlefield(int slot, PlayerId who) {
        state.battlefields[slot].controller = who;
        addUnit(who, kUnit, 1, slot);
    }

    void faceDown(GameObjectId card, int slot) {
        auto& obj = state.getObject(card);
        auto& hand = state.player(obj.owner).hand;
        hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
        obj.zone = ZoneType::FacedownZone;
        obj.location.reset();
        obj.is_hidden = true;
        obj.hidden_at = BattlefieldId::fromRaw(slot);
        obj.hidden_on_turn = state.turn.turn_number - 1;
        state.battlefields[slot].facedown.push_back(card);
        state.battlefields[slot].controller = obj.controller;
    }

    // Pouty Poro is [2]. [Accelerate] is [1] plus a Power of its Domain, paid
    // from the pool here so the only prompt is the Accelerate yes/no.
    void fundPrintedPlusAccelerate() {
        state.player(P1).rune_pool.energy = card_db.get(kUnit).energy_cost + 1;
        state.player(P1).rune_pool.universal_power = 1;
    }

    struct PlaySpots {
        bool base = false;
        bool bf[3] = {};
    };

    static PlaySpots spotsFor(const std::vector<Intent>& actions, GameObjectId card,
                              IntentType type) {
        PlaySpots spots;
        for (const auto& intent : actions) {
            if (intent.card != card || intent.type != type || !intent.play_location) continue;
            if (std::holds_alternative<BaseLocation>(*intent.play_location)) spots.base = true;
            if (const auto* bf = std::get_if<BattlefieldLocation>(&*intent.play_location)) {
                const int slot = static_cast<int>(bf->id.raw());
                if (slot >= 0 && slot < 3) spots.bf[slot] = true;
            }
        }
        return spots;
    }

    // BF 0 is friendly, BF 1 is an enemy battlefield, BF 2 blocks unit plays.
    void threeBattlefields() {
        holdBattlefield(0, P1);
        holdBattlefield(1, P2);
        addBattlefield();
        state.battlefields[2].blocks_unit_play = true;
    }
};

TEST_F(EngineFixLines, BlackRose_FirstNonTokenDeathHerePlaysTheirRecruit) {
    battlefieldCard(0, kRose, P1);
    addUnit(P1, kUnit, 1, 0);   // keeps the battlefield's controller through a cleanup
    auto token = addUnit(P2, kUnit, 1, 0);
    state.getObject(token).is_token = true;
    kill(token);
    EXPECT_EQ(recruitsOf(P2), 0) << "a token dying here is not the first non-token death";

    auto at_base = addUnit(P2, kUnit, 1);
    kill(at_base);
    EXPECT_EQ(recruitsOf(P2), 0) << "a death at base is not a death here";

    auto elsewhere = addUnit(P2, kUnit, 1, 1);
    kill(elsewhere);
    EXPECT_EQ(recruitsOf(P2), 0) << "a death at the other battlefield is not a death here";

    auto here = addUnit(P2, kUnit, 1, 0);
    kill(here);
    EXPECT_EQ(recruitsOf(P2), 1);
    EXPECT_EQ(recruitsOf(P1), 0) << "the Recruit is played by the player whose unit died";

    auto again = addUnit(P2, kUnit, 1, 0);
    kill(again);
    EXPECT_EQ(recruitsOf(P2), 1) << "the second non-token death here this turn plays nothing";
}

TEST_F(EngineFixLines, HuntersCircle_SecondUnitHereIsFullPrice) {
    battlefieldCard(0, kCircle, P1);
    addUnit(P1, kUnit, 1, 0);
    auto first = addToHand(P1, kUnit);
    auto second = addToHand(P1, kUnit);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        e.mutableState().player(P1).rune_pool.energy = 2;
        playAt(e, first, BattlefieldLocation{BattlefieldId::fromRaw(0)});
        settleLikeTheEngine(e);
        EXPECT_EQ(e.mutableState().player(P1).rune_pool.energy, 1);
        e.mutableState().player(P1).rune_pool.energy = 2;
        playAt(e, second, BattlefieldLocation{BattlefieldId::fromRaw(0)});
        EXPECT_EQ(e.mutableState().player(P1).rune_pool.energy, 0);
    });
}

TEST_F(EngineFixLines, HuntersCircle_TheOtherPlayerGetsItOnTheirTurn) {
    battlefieldCard(0, kCircle, P1);
    addUnit(P1, kUnit, 1, 0);
    auto theirs = addToHand(P2, kUnit);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        e.mutableState().player(P2).rune_pool.energy = 2;
        playAt(e, theirs, BattlefieldLocation{BattlefieldId::fromRaw(0)});
        EXPECT_EQ(e.mutableState().player(P2).rune_pool.energy, 1);
    }, nullptr, nullptr, P2);
}

TEST_F(EngineFixLines, HuntersCircle_NotOnTheOpponentsTurn) {
    battlefieldCard(0, kCircle, P1);
    addUnit(P1, kUnit, 1, 0);
    auto during = addToHand(P1, kUnit);
    auto later = addToHand(P1, kUnit);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        e.mutableState().player(P1).rune_pool.energy = 2;
        playAt(e, during, BattlefieldLocation{BattlefieldId::fromRaw(0)});
        EXPECT_EQ(e.mutableState().player(P1).rune_pool.energy, 0);
        settleLikeTheEngine(e);
        e.mutableState().turn.turn_player = P1;
        e.testHook_cleanup();
        e.mutableState().player(P1).rune_pool.energy = 2;
        playAt(e, later, BattlefieldLocation{BattlefieldId::fromRaw(0)});
        EXPECT_EQ(e.mutableState().player(P1).rune_pool.energy, 1);
    }, nullptr, nullptr, P2);
}

TEST_F(EngineFixLines, HuntersCircle_AGearHereUsesTheSamePermission) {
    battlefieldCard(0, kCircle, P1);
    addUnit(P1, kUnit, 1, 0);
    auto gear = addToHand(P1, kDeployGear);
    auto unit = addToHand(P1, kUnit);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        e.mutableState().player(P1).rune_pool.energy = 1;
        playAt(e, gear, BattlefieldLocation{BattlefieldId::fromRaw(0)});
        EXPECT_EQ(e.mutableState().player(P1).rune_pool.energy, 1);
        settleLikeTheEngine(e);
        e.mutableState().player(P1).rune_pool.energy = 2;
        playAt(e, unit, BattlefieldLocation{BattlefieldId::fromRaw(0)});
        EXPECT_EQ(e.mutableState().player(P1).rune_pool.energy, 0);
    });
}

TEST_F(EngineFixLines, HuntersCircle_AReactionPayableOnlyHereIsOffered) {
    battlefieldCard(0, kCircle, P1);
    addUnit(P1, kUnit, 1, 0);
    auto shen = addToHand(P1, kShen);
    state.player(P1).rune_pool.energy = 2;
    state.player(P1).rune_pool.universal_power = 1;
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        e.mutableState().turn.phase = TurnPhase::MainPhase;
        e.mutableState().turn.oc_state = OpenClosedState::Closed;
        e.mutableState().turn.priority_holder = P1;
        bool at_circle = false;
        bool at_base = false;
        for (const auto& intent : e.generateLegalActions()) {
            if (intent.type != IntentType::PlayReaction || intent.card != shen) continue;
            if (!intent.play_location.has_value()) continue;
            if (std::holds_alternative<BaseLocation>(*intent.play_location)) at_base = true;
            if (const auto* bf = std::get_if<BattlefieldLocation>(&*intent.play_location))
                if (bf->id == BattlefieldId::fromRaw(0)) at_circle = true;
        }
        EXPECT_TRUE(at_circle);
        EXPECT_FALSE(at_base);
    });
}

TEST_F(EngineFixLines, HuntersCircle_IgnoreCostPlaySpendsThePermission) {
    battlefieldCard(0, kCircle, P1);
    addUnit(P1, kUnit, 1, 0);
    auto first = addToHand(P1, kUnit);
    auto gear = addToHand(P1, kDeployGear);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        const LocationId here{BattlefieldLocation{BattlefieldId::fromRaw(0)}};
        EXPECT_TRUE(e.testHook_executor().playIgnoringCost(P1, first, here));
        EXPECT_EQ(e.mutableState().getObject(first).location, here);
        // A gear is the same permission. This one is not discounted.
        expectNoDiscount(e, gear, 1);
    });
}

TEST_F(EngineFixLines, HuntersCircle_EffectPaySpendsThePermission) {
    battlefieldCard(0, kCircle, P1);
    addUnit(P1, kUnit, 1, 0);
    auto first = addToHand(P1, kUnit);
    auto second = addToHand(P1, kUnit);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        e.mutableState().player(P1).rune_pool.energy = 2;
        const LocationId here{BattlefieldLocation{BattlefieldId::fromRaw(0)}};
        EXPECT_TRUE(e.testHook_executor().playPayingCost(P1, first, here));
        EXPECT_EQ(e.mutableState().getObject(first).location, here);
        // The effect play itself is the first play here, so it takes the [1].
        EXPECT_EQ(e.mutableState().player(P1).rune_pool.energy, 1);
        expectNoDiscount(e, second, 2);
    });
}

TEST_F(EngineFixLines, HuntersCircle_AltCostPlaySpendsThePermission) {
    battlefieldCard(0, kCircle, P1);
    addUnit(P1, kUnit, 1, 0);
    auto first = addToHand(P1, kUnit);
    auto second = addToHand(P1, kUnit);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = first;
        play.use_alt_play_cost = true;
        play.play_location = BattlefieldLocation{BattlefieldId::fromRaw(0)};
        e.testHook_executeIntent(play);
        EXPECT_EQ(e.mutableState().getObject(first).location,
                  LocationId{BattlefieldLocation{BattlefieldId::fromRaw(0)}});
        expectNoDiscount(e, second, 2);
    });
}

TEST_F(EngineFixLines, HuntersCircle_TrashReplaySpendsThePermission) {
    battlefieldCard(0, kCircle, P1);
    addUnit(P1, kUnit, 1, 0);
    auto first = addToHand(P1, kUnit);
    auto gear = addToHand(P1, kDeployGear);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        toTrash(e.mutableState(), first);
        PlayerState::TrashReplayGrant grant;
        grant.card = first;
        e.mutableState().player(P1).trash_replay_grants.push_back(grant);
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = first;
        play.play_source = Intent::PlaySource::Trash;
        play.play_location = BattlefieldLocation{BattlefieldId::fromRaw(0)};
        e.testHook_executeIntent(play);
        EXPECT_EQ(e.mutableState().getObject(first).location,
                  LocationId{BattlefieldLocation{BattlefieldId::fromRaw(0)}});
        expectNoDiscount(e, gear, 1);
    });
}

TEST_F(EngineFixLines, CosmicVista_EmptyRuneDeckScoresOnHold) {
    battlefieldCard(0, kVista, P1);
    addUnit(P1, kUnit, 1, 0);
    withEngine([&](GameEngine& e) {
        e.testHook_scoreHold(P1, BattlefieldId::fromRaw(0));
        e.testHook_runChain();
        EXPECT_EQ(e.mutableState().player(P1).score, 2);
    });

    state.player(P1).score = 0;
    state.player(P1).battlefields_scored_this_turn.clear();
    battlefieldCard(1, kVista, P1);
    addUnit(P1, kUnit, 1, 1);
    auto rune = addRune(P1, Domain::Calm);
    state.getObject(rune).zone = ZoneType::RuneDeck;
    state.getObject(rune).location.reset();
    state.player(P1).rune_deck.push_back(rune);
    withEngine([&](GameEngine& e) {
        e.testHook_scoreHold(P1, BattlefieldId::fromRaw(1));
        e.testHook_runChain();
        EXPECT_EQ(e.mutableState().player(P1).score, 1);
    });
}

TEST_F(EngineFixLines, CosmicVista_EmptyDeckIsJudgedWhenYouHold) {
    battlefieldCard(0, kVista, P1);
    addUnit(P1, kUnit, 1, 0);
    withEngine([&](GameEngine& e) {
        e.testHook_scoreHold(P1, BattlefieldId::fromRaw(0));
        auto& st = e.mutableState();
        auto rune = st.createObject();
        auto& obj = st.getObject(rune);
        obj.owner = P1;
        obj.controller = P1;
        obj.card_type = CardType::Rune;
        obj.zone = ZoneType::RuneDeck;
        obj.name = "Calm Rune";
        st.player(P1).rune_deck.push_back(rune);
        e.testHook_runChain();
        // The hold saw an empty deck. A rune that arrives before the ability
        // resolves does not take the point back.
        EXPECT_EQ(st.player(P1).score, 2);
    });

    state.player(P1).score = 0;
    state.player(P1).battlefields_scored_this_turn.clear();
    state.player(P1).rune_deck.clear();
    battlefieldCard(1, kVista, P1);
    addUnit(P1, kUnit, 1, 1);
    auto rune = addRune(P1, Domain::Calm);
    state.getObject(rune).zone = ZoneType::RuneDeck;
    state.getObject(rune).location.reset();
    state.player(P1).rune_deck = {rune};
    withEngine([&](GameEngine& e) {
        e.testHook_scoreHold(P1, BattlefieldId::fromRaw(1));
        e.mutableState().player(P1).rune_deck.clear();
        e.testHook_runChain();
        // The hold saw a rune. Emptying the deck in response does not score.
        EXPECT_EQ(e.mutableState().player(P1).score, 1);
    });
}

TEST_F(EngineFixLines, DurandMemorial_DefenderChoosesAUnitHereForTank) {
    battlefieldCard(0, kMemorial, P2);
    addUnit(P1, kUnit, 4, 0);
    auto chosen = addUnit(P2, kUnit, 4, 0);
    auto other = addUnit(P2, kUnit, 4, 0);
    state.battlefields[0].is_contested = true;
    state.battlefields[0].contested_by = P1;
    EnginePathAgent p2(EnginePathAgent::script(
        {static_cast<int>(chosen.raw()), EnginePathAgent::kRest}));
    withEngine([&](GameEngine& e) {
        bool chosen_tank = false;
        bool other_tank = false;
        e.on_decision = [&](const GameState& st, const std::vector<Intent>&, const Intent&) {
            auto tank = [&](GameObjectId id) {
                if (st.getObject(id).hasKeyword(Keyword::Tank)) return true;
                for (const auto& ck : st.combat_keywords)
                    if (ck.unit == id && ck.keyword == Keyword::Tank) return true;
                return false;
            };
            if (tank(chosen)) chosen_tank = true;
            if (tank(other)) other_tank = true;
        };
        e.testHook_runCombat(BattlefieldId::fromRaw(0));
        EXPECT_TRUE(chosen_tank);
        EXPECT_FALSE(other_tank);
    }, nullptr, &p2);
}

TEST_F(EngineFixLines, Bandle_HidingHerePlaysAnExhaustedGold) {
    battlefieldCard(0, kAcademy, P1);
    addUnit(P1, kUnit, 1, 0);
    auto here = addToHand(P1, kUnit);
    auto elsewhere = addToHand(P1, kUnit);
    state.getObject(here).keywords.set(Keyword::Hidden);
    state.getObject(elsewhere).keywords.set(Keyword::Hidden);
    state.player(P1).hide_ignoring_costs_this_turn = true;
    withEngine([&](GameEngine& e) {
        Intent hide;
        hide.type = IntentType::HideCard;
        hide.player = P1;
        hide.card = here;
        hide.chosen_battlefield = BattlefieldId::fromRaw(0);
        e.testHook_executeIntent(hide);
        settleLikeTheEngine(e);
        EXPECT_EQ(exhaustedGolds(e.mutableState(), P1), 1);
        hide.card = elsewhere;
        hide.chosen_battlefield = BattlefieldId::fromRaw(1);
        e.testHook_executeIntent(hide);
        settleLikeTheEngine(e);
        EXPECT_EQ(exhaustedGolds(e.mutableState(), P1), 1);
    });
}

TEST_F(EngineFixLines, Songstress_FinalizeSpellShortByTwoIsPlayed) {
    auto songstress = legend(P1, kSongstress);
    auto a = addUnit(P1, kUnit);
    auto b = addUnit(P1, kUnit);
    auto c = addUnit(P1, kUnit);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    auto spell = addToHand(P1, kHighNote);
    state.player(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({
        static_cast<int>(enemy.raw()),
        static_cast<int>(a.raw()), static_cast<int>(b.raw()), static_cast<int>(c.raw()),
        EnginePathAgent::kRest}));
    EXPECT_NO_THROW(playThroughEngine(spell, &agent, std::nullopt, nullptr, /*fund=*/false));
    EXPECT_FALSE(inHand(P1, spell));
    EXPECT_TRUE(state.getObject(songstress).is_exhausted);
    EXPECT_TRUE(state.getObject(a).is_exhausted);
    EXPECT_TRUE(state.getObject(b).is_exhausted);
    EXPECT_TRUE(state.getObject(c).is_exhausted);

    auto dear = addToHand(P1, kHighNote);
    state.player(P1).rune_pool.energy = 0;
    withEngine([&](GameEngine& e) {
        for (const auto& intent : e.testHook_mainPhaseActions(P1))
            EXPECT_FALSE(intent.type == IntentType::PlayCard && intent.card == dear);
    });
}

// Packed Amphitheater and Rakelstake. Each assertion below fails before the
// fix and passes after it.

TEST_F(EngineFixLines, PackedAmphitheater_ChampionsHereHavePlusOneMight) {
    battlefieldCard(0, kAmphitheater, P1);
    auto shen = addUnit(P1, kShen, 0, 0);
    auto poro = addUnit(P1, kUnit, 0, 0);
    auto elsewhere = addUnit(P1, kShen, 0, 1);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        // Printed Might 3, plus the battlefield's +1. A cleanup is what combat
        // and every Might check read.
        EXPECT_EQ(e.mutableState().getObject(shen).current_might, 4);
        EXPECT_EQ(e.mutableState().getObject(poro).current_might, 2);
        EXPECT_EQ(e.mutableState().getObject(elsewhere).current_might, 3);
    });
}

TEST_F(EngineFixLines, PackedAmphitheater_CombatDealsTheBonusMight) {
    // P2 holds the battlefield, so Shen is the attacker and [Shield] does not
    // add. He deals 4 and the 4 [M] defender dies; at printed Might he deals 3
    // and that defender lives.
    battlefieldCard(0, kAmphitheater, P2);
    auto shen = addUnit(P1, kShen, 0, 0);
    auto defender = addUnit(P2, kInvalidId, 4, 0);
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        EXPECT_EQ(e.mutableState().getObject(shen).current_might, 4);
        EXPECT_EQ(e.mutableState().battlefields[0].contested_by, P1);
        e.testHook_runCombat(BattlefieldId::fromRaw(0));
        EXPECT_FALSE(e.mutableState().getObject(defender).location.has_value());
    });
}

TEST_F(EngineFixLines, Rakelstake_FirstPlayHerePredictsAndTheSecondDoesNot) {
    battlefieldCard(0, kRakelstake, P1);
    addUnit(P1, kUnit, 1, 0);
    addToDeck(P1, kUnit);
    auto away = addToHand(P1, kUnit);
    auto first = addToHand(P1, kUnit);
    auto second = addToHand(P1, kUnit);
    const LocationId here{BattlefieldLocation{BattlefieldId::fromRaw(0)}};
    const LocationId other{BattlefieldLocation{BattlefieldId::fromRaw(1)}};
    EnginePathAgent p1;
    playThroughEngine(away, &p1, other);
    EXPECT_EQ(p1.predict_kept_by_default, 0);
    playThroughEngine(first, &p1, here);
    EXPECT_EQ(p1.predict_kept_by_default, 1);
    playThroughEngine(second, &p1, here);
    EXPECT_EQ(p1.predict_kept_by_default, 1);
    state.turn.turn_number += 1;
    auto later = addToHand(P1, kUnit);
    playThroughEngine(later, &p1, here);
    EXPECT_EQ(p1.predict_kept_by_default, 2);
}

TEST_F(EngineFixLines, Rakelstake_TheOtherPlayerGetsTheirOwnFirstPlay) {
    battlefieldCard(0, kRakelstake, P1);
    addUnit(P1, kUnit, 1, 0);
    addToDeck(P1, kUnit);
    addToDeck(P2, kUnit);
    auto mine = addToHand(P1, kUnit);
    auto theirs = addToHand(P2, kUnit);
    auto again = addToHand(P2, kUnit);
    const LocationId here{BattlefieldLocation{BattlefieldId::fromRaw(0)}};
    EnginePathAgent p1, p2;
    playThroughEngine(mine, &p1, here, &p2);
    EXPECT_EQ(p1.predict_kept_by_default, 1);
    EXPECT_EQ(p2.predict_kept_by_default, 0);
    playThroughEngine(theirs, &p1, here, &p2);
    EXPECT_EQ(p2.predict_kept_by_default, 1);
    playThroughEngine(again, &p1, here, &p2);
    EXPECT_EQ(p2.predict_kept_by_default, 1);
}

TEST_F(EngineFixLines, Rakelstake_AGearHerePredictsAndUsesTheSamePermission) {
    battlefieldCard(0, kRakelstake, P1);
    addUnit(P1, kUnit, 1, 0);
    addToDeck(P1, kUnit);
    auto gear = addToHand(P1, kDeployGear);
    auto unit = addToHand(P1, kUnit);
    const LocationId here{BattlefieldLocation{BattlefieldId::fromRaw(0)}};
    const LocationId other{BattlefieldLocation{BattlefieldId::fromRaw(1)}};
    EnginePathAgent p1;
    playThroughEngine(gear, &p1, other);
    EXPECT_EQ(p1.predict_kept_by_default, 0);
    auto here_gear = addToHand(P1, kDeployGear);
    playThroughEngine(here_gear, &p1, here);
    EXPECT_EQ(p1.predict_kept_by_default, 1);
    playThroughEngine(unit, &p1, here);
    EXPECT_EQ(p1.predict_kept_by_default, 1);
}

// The Exile reads "Units you play to battlefields you control enter ready."
// That readies the unit. It does not change what the unit costs. Reckless
// Innovator is [3] with [Accelerate] ([1][P] to enter ready), so paying
// Accelerate is 4 energy and 1 power. Rakelstake reads "For each player, the
// first time they play a unit or gear here each turn, they Predict." This is
// that first play: the battlefield is already controlled, and nothing has
// been played there this turn.
TEST_F(EngineFixLines, Exile_RecklessInnovatorAtRakelstakePredictsOnce) {
    legend(P2, kExile);
    battlefieldCard(0, kRakelstake, P2);
    addUnit(P2, kUnit, 1, 0);
    battlefieldCard(1, kAmphitheater, P2);
    addUnit(P2, kUnit, 1, 1);
    addToDeck(P2, kUnit);
    auto innovator = addToHand(P2, kInnovator);
    auto gear = addToHand(P2, kDeployGear);
    auto elsewhere = addToHand(P2, kUnit);
    // 4 for the Innovator with Accelerate, 1 for the gear, 2 for the Poro.
    state.player(P2).rune_pool.energy = 7;
    state.player(P2).rune_pool.universal_power = 1;
    const LocationId here{BattlefieldLocation{BattlefieldId::fromRaw(0)}};
    const LocationId other{BattlefieldLocation{BattlefieldId::fromRaw(1)}};
    EnginePathAgent p2(EnginePathAgent::script({1, EnginePathAgent::kRest}));

    std::string account;
    withEngine([&](GameEngine& e) {
        PlayByPlay chronicle(e.state(), events);
        e.testHook_cleanup();
        ASSERT_TRUE(e.mutableState().player(P2).units_enter_ready_at_your_bf);
        playAt(e, innovator, here);
        settleLikeTheEngine(e);
        EXPECT_EQ(e.mutableState().player(P2).rune_pool.energy, 3);
        EXPECT_EQ(e.mutableState().player(P2).rune_pool.universal_power, 0);
        EXPECT_EQ(e.mutableState().getObject(innovator).location, here);
        EXPECT_FALSE(e.mutableState().getObject(innovator).is_exhausted);

        playAt(e, gear, here);
        settleLikeTheEngine(e);
        EXPECT_EQ(e.mutableState().getObject(gear).location, here);
        EXPECT_EQ(e.mutableState().player(P2).rune_pool.energy, 2);

        playAt(e, elsewhere, other);
        settleLikeTheEngine(e);
        EXPECT_EQ(e.mutableState().getObject(elsewhere).location, other);
        EXPECT_EQ(e.mutableState().player(P2).rune_pool.energy, 0);
        account = chronicle.text();
    }, nullptr, &p2, P2);

    int looked = 0;
    for (const auto& prompt : p2.prompts) {
        if (prompt.size() == 2 && prompt[1].choice_label == "Keep it") ++looked;
    }
    EXPECT_EQ(looked, 1) << "the first unit here Predicts; the gear here and the "
                            "unit at the other battlefield do not";
    const auto play = account.find(
        "P2 plays Reckless Innovator (P2) to Rakelstake for 4 energy, 1 power, and Accelerate.");
    const std::string predicted_line = "P2 Predicts.";
    const auto predicted = account.find(predicted_line);
    EXPECT_NE(play, std::string::npos) << account;
    EXPECT_NE(predicted, std::string::npos) << account;
    EXPECT_LT(play, predicted) << account;
    EXPECT_EQ(account.find(predicted_line, predicted + predicted_line.size()),
              std::string::npos) << account;
    EXPECT_NE(account.find("P2 plays Bushwhack Trap to Rakelstake for 1 energy."),
              std::string::npos) << account;
    EXPECT_NE(account.find("P2 plays Pouty Poro (P2) to Packed Amphitheater for 2 energy."),
              std::string::npos) << account;
}

// Firelights' Hideout and Last Caress. The expiry, the buff, and the facedown
// destinations fail before the fix and pass after it.

TEST_F(EngineFixLines, FirelightsHideout_TheNextUnitThisTurnHasAccelerate) {
    auto hideout = battlefieldCard(0, kHideout, P1);
    addUnit(P1, kUnit, 1, 0);
    fireTriggerThroughEngine(hideout, TriggerType::WhenYouConquerHere);
    ASSERT_EQ(state.player(P1).next_unit_accelerate, 1);

    auto unit = addToHand(P1, kUnit);
    fundPrintedPlusAccelerate();
    EnginePathAgent pay(EnginePathAgent::script({1}));
    playThroughEngine(unit, &pay, LocationId{BaseLocation{P1}}, nullptr, false);
    EXPECT_FALSE(state.getObject(unit).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
    EXPECT_EQ(state.player(P1).next_unit_accelerate, 0);

    // The grant is the next unit, and entering spends it.
    auto another = addToHand(P1, kUnit);
    fundPrintedPlusAccelerate();
    EnginePathAgent payAgain(EnginePathAgent::script({1}));
    playThroughEngine(another, &payAgain, LocationId{BaseLocation{P1}}, nullptr, false);
    EXPECT_TRUE(state.getObject(another).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1);
}

TEST_F(EngineFixLines, FirelightsHideout_AccelerateEndsWhenTheNextTurnStarts) {
    auto hideout = battlefieldCard(0, kHideout, P1);
    addUnit(P1, kUnit, 1, 0);
    fireTriggerThroughEngine(hideout, TriggerType::WhenYouConquerHere);
    ASSERT_EQ(state.player(P1).next_unit_accelerate, 1);

    beginNextTurn();
    EXPECT_EQ(state.player(P1).next_unit_accelerate, 0);

    auto unit = addToHand(P1, kUnit);
    fundPrintedPlusAccelerate();
    EnginePathAgent pay(EnginePathAgent::script({1}));
    playThroughEngine(unit, &pay, LocationId{BaseLocation{P1}}, nullptr, false);
    EXPECT_TRUE(state.getObject(unit).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1);
}

TEST_F(EngineFixLines, LastCaress_ThePlayedUnitIsBuffed) {
    threeBattlefields();
    auto spell = addToHand(P1, kCaress);
    playThroughEngine(spell);
    ASSERT_EQ(state.getObject(spell).zone, ZoneType::Trash);
    ASSERT_EQ(state.player(P1).next_unit_any_battlefield, 1);

    auto unit = addToHand(P1, kUnit);
    state.player(P1).rune_pool.energy = card_db.get(kUnit).energy_cost;
    withEngine([&](GameEngine& e) {
        const auto spots = spotsFor(e.testHook_mainPhaseActions(P1), unit, IntentType::PlayCard);
        EXPECT_TRUE(spots.base);
        EXPECT_TRUE(spots.bf[0]);
        EXPECT_TRUE(spots.bf[1]);
        EXPECT_FALSE(spots.bf[2]);
    });

    const int printed = card_db.get(kUnit).might;
    const LocationId enemy{BattlefieldLocation{BattlefieldId::fromRaw(1)}};
    playThroughEngine(unit, nullptr, enemy, nullptr, false);
    EXPECT_EQ(state.getObject(unit).location, enemy);
    EXPECT_EQ(state.getObject(unit).buff_count, 1);
    EXPECT_EQ(state.getObject(unit).current_might, printed + 1);
    EXPECT_EQ(state.player(P1).next_unit_any_battlefield, 0);

    auto second = addToHand(P1, kUnit);
    state.player(P1).rune_pool.energy = card_db.get(kUnit).energy_cost;
    withEngine([&](GameEngine& e) {
        const auto spots = spotsFor(e.testHook_mainPhaseActions(P1), second, IntentType::PlayCard);
        EXPECT_TRUE(spots.base);
        EXPECT_TRUE(spots.bf[0]);
        EXPECT_FALSE(spots.bf[1]);
    });
    playThroughEngine(second, nullptr, LocationId{BaseLocation{P1}}, nullptr, false);
    EXPECT_EQ(state.getObject(second).buff_count, 0);
    EXPECT_EQ(state.getObject(second).current_might, printed);
}

TEST_F(EngineFixLines, LastCaress_AFacedownUnitCanBePlayedToAnyBattlefield) {
    threeBattlefields();
    auto spell = addToHand(P1, kCaress);
    playThroughEngine(spell);
    auto hidden = addToHand(P1, kUnit);
    faceDown(hidden, 0);

    int plays = 0;
    withEngine([&](GameEngine& e) {
        for (const auto& intent : e.testHook_mainPhaseActions(P1))
            if (intent.card == hidden && intent.type == IntentType::PlayActionCard) ++plays;
        const auto spots = spotsFor(e.testHook_mainPhaseActions(P1), hidden,
                                    IntentType::PlayActionCard);
        EXPECT_TRUE(spots.bf[0]);
        EXPECT_TRUE(spots.bf[1]);
        EXPECT_FALSE(spots.bf[2]);
        EXPECT_FALSE(spots.base);
    });
    EXPECT_EQ(plays, 2);

    const LocationId enemy{BattlefieldLocation{BattlefieldId::fromRaw(1)}};
    withEngine([&](GameEngine& e) {
        Intent play;
        play.type = IntentType::PlayActionCard;
        play.player = P1;
        play.card = hidden;
        play.play_location = enemy;
        e.testHook_executeIntent(play);
        settleLikeTheEngine(e);
    });
    const int printed = card_db.get(kUnit).might;
    EXPECT_EQ(state.getObject(hidden).location, enemy);
    EXPECT_FALSE(state.getObject(hidden).is_hidden);
    EXPECT_EQ(state.getObject(hidden).buff_count, 1);
    EXPECT_EQ(state.getObject(hidden).current_might, printed + 1);
    EXPECT_EQ(state.player(P1).next_unit_any_battlefield, 0);

    auto later = addToHand(P1, kUnit);
    state.player(P1).rune_pool.energy = card_db.get(kUnit).energy_cost;
    withEngine([&](GameEngine& e) {
        const auto spots = spotsFor(e.testHook_mainPhaseActions(P1), later, IntentType::PlayCard);
        EXPECT_FALSE(spots.bf[1]);
    });
}

TEST_F(EngineFixLines, LastCaress_AnyBattlefieldEndsWhenTheNextTurnStarts) {
    threeBattlefields();
    auto spell = addToHand(P1, kCaress);
    playThroughEngine(spell);
    auto unit = addToHand(P1, kUnit);
    state.player(P1).rune_pool.energy = card_db.get(kUnit).energy_cost;
    withEngine([&](GameEngine& e) {
        const auto spots = spotsFor(e.testHook_mainPhaseActions(P1), unit, IntentType::PlayCard);
        EXPECT_TRUE(spots.bf[1]);
        EXPECT_FALSE(spots.bf[2]);
    });

    beginNextTurn();
    EXPECT_EQ(state.player(P1).next_unit_any_battlefield, 0);
    state.player(P1).rune_pool.energy = card_db.get(kUnit).energy_cost;
    withEngine([&](GameEngine& e) {
        const auto spots = spotsFor(e.testHook_mainPhaseActions(P1), unit, IntentType::PlayCard);
        EXPECT_TRUE(spots.base);
        EXPECT_TRUE(spots.bf[0]);
        EXPECT_FALSE(spots.bf[1]);
        EXPECT_FALSE(spots.bf[2]);
    });
    const int printed = card_db.get(kUnit).might;
    playThroughEngine(unit, nullptr, LocationId{BaseLocation{P1}}, nullptr, false);
    EXPECT_EQ(state.getObject(unit).buff_count, 0);
    EXPECT_EQ(state.getObject(unit).current_might, printed);
}

// Plays `second` (a facedown unit) in the priority window of the buff ability
// the play of `first` just queued. `counter` removes that ability first, the
// way a counter does (EffectExecutor::removeChainItem): no printed card
// counters an ability that never chose a unit.
class PlayWhileTheBuffHasPriority : public EnginePathAgent {
public:
    GameEngine* engine = nullptr;
    GameObjectId first = kInvalidId;
    GameObjectId second = kInvalidId;
    CardDefId spell = kInvalidId;
    bool counter = false;
    bool did = false;

    Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
        if (!did && engine && st.objectExists(first) &&
            st.getObject(first).location.has_value()) {
            ChainItemId buff = kInvalidId;
            for (const auto& ci : st.chain.items) {
                if (!ci.is_ability || ci.triggering_subject != first) continue;
                if (!st.objectExists(ci.source) || st.getObject(ci.source).card_def_id != spell)
                    continue;
                buff = ci.id;
                break;
            }
            if (buff != kInvalidId) {
                if (counter) engine->testHook_executor().removeChainItem(buff);
                for (const auto& intent : legal) {
                    if (intent.card != second) continue;
                    if (intent.type != IntentType::PlayReaction &&
                        intent.type != IntentType::PlayActionCard)
                        continue;
                    did = true;
                    return intent;
                }
            }
        }
        return EnginePathAgent::selectAction(st, legal);
    }
};

// The buff belongs to the unit that was played. A unit played while that
// ability still has priority does not take it.
TEST_F(EngineFixLines, LastCaress_ThePlayedUnitKeepsTheBuff) {
    threeBattlefields();
    auto spell = addToHand(P1, kCaress);
    playThroughEngine(spell);

    auto first = addToHand(P1, kUnit);
    auto hidden = addToHand(P1, kUnit);
    faceDown(hidden, 0);
    state.player(P1).rune_pool.energy = card_db.get(kUnit).energy_cost;
    const LocationId enemy{BattlefieldLocation{BattlefieldId::fromRaw(1)}};

    PlayWhileTheBuffHasPriority agent;
    agent.first = first;
    agent.second = hidden;
    agent.spell = kCaress;
    withEngine([&](GameEngine& e) {
        agent.engine = &e;
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = first;
        play.play_location = enemy;
        e.testHook_executeIntent(play);
        settleLikeTheEngine(e);
    }, &agent);
    ASSERT_TRUE(agent.did) << "the facedown unit was played while the buff had priority";

    const int printed = card_db.get(kUnit).might;
    EXPECT_EQ(state.getObject(first).location, enemy);
    EXPECT_EQ(state.getObject(first).buff_count, 1);
    EXPECT_EQ(state.getObject(first).current_might, printed + 1);
    EXPECT_EQ(state.getObject(hidden).buff_count, 0);
    EXPECT_EQ(state.getObject(hidden).current_might, printed);
    EXPECT_EQ(state.player(P1).next_unit_buff, 0);
    EXPECT_EQ(state.player(P1).next_unit_any_battlefield, 0);
}

// Countering the buff does not hand it to a later unit.
TEST_F(EngineFixLines, LastCaress_ACounteredBuffDoesNotMove) {
    threeBattlefields();
    auto spell = addToHand(P1, kCaress);
    playThroughEngine(spell);

    auto first = addToHand(P1, kUnit);
    auto hidden = addToHand(P1, kUnit);
    faceDown(hidden, 0);
    state.player(P1).rune_pool.energy = card_db.get(kUnit).energy_cost;

    PlayWhileTheBuffHasPriority agent;
    agent.first = first;
    agent.second = hidden;
    agent.spell = kCaress;
    agent.counter = true;
    withEngine([&](GameEngine& e) {
        agent.engine = &e;
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = first;
        play.play_location = LocationId{BaseLocation{P1}};
        e.testHook_executeIntent(play);
        settleLikeTheEngine(e);
    }, &agent);
    ASSERT_TRUE(agent.did);

    const int printed = card_db.get(kUnit).might;
    EXPECT_EQ(state.getObject(first).buff_count, 0);
    EXPECT_EQ(state.getObject(first).current_might, printed);
    EXPECT_EQ(state.getObject(hidden).buff_count, 0);
    EXPECT_EQ(state.getObject(hidden).current_might, printed);
    EXPECT_EQ(state.player(P1).next_unit_buff, 0);

    auto later = addToHand(P1, kUnit);
    state.player(P1).rune_pool.energy = card_db.get(kUnit).energy_cost;
    playThroughEngine(later, nullptr, LocationId{BaseLocation{P1}}, nullptr, false);
    EXPECT_EQ(state.getObject(later).buff_count, 0);
    EXPECT_EQ(state.getObject(later).current_might, printed);
}

// An effect play is the next unit: it is buffed, it can pay [Accelerate], and
// it spends the destination too. The unit after it has none of the three.
TEST_F(EngineFixLines, LastCaress_AnEffectPlaySpendsEveryGrant) {
    threeBattlefields();
    auto hideout = battlefieldCard(0, kHideout, P1);
    fireTriggerThroughEngine(hideout, TriggerType::WhenYouConquerHere);
    auto spell = addToHand(P1, kCaress);
    playThroughEngine(spell);
    ASSERT_EQ(state.player(P1).next_unit_accelerate, 1);
    ASSERT_EQ(state.player(P1).next_unit_any_battlefield, 1);
    ASSERT_EQ(state.player(P1).next_unit_buff, 1);

    auto unit = addToHand(P1, kUnit);
    fundPrintedPlusAccelerate();
    EnginePathAgent pay(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.playIgnoringCost(P1, unit, LocationId{BaseLocation{P1}});
    }, &pay);

    const int printed = card_db.get(kUnit).might;
    EXPECT_FALSE(state.getObject(unit).is_exhausted);
    EXPECT_EQ(state.getObject(unit).buff_count, 1);
    EXPECT_EQ(state.getObject(unit).current_might, printed + 1);
    EXPECT_EQ(state.player(P1).next_unit_accelerate, 0);
    EXPECT_EQ(state.player(P1).next_unit_any_battlefield, 0);
    EXPECT_EQ(state.player(P1).next_unit_buff, 0);

    auto second = addToHand(P1, kUnit);
    fundPrintedPlusAccelerate();
    withEngine([&](GameEngine& e) {
        const auto spots = spotsFor(e.testHook_mainPhaseActions(P1), second, IntentType::PlayCard);
        EXPECT_TRUE(spots.base);
        EXPECT_TRUE(spots.bf[0]);
        EXPECT_FALSE(spots.bf[1]);
        EXPECT_FALSE(spots.bf[2]);
    });
    EnginePathAgent payAgain(EnginePathAgent::script({1}));
    playThroughEngine(second, &payAgain, LocationId{BaseLocation{P1}}, nullptr, false);
    EXPECT_TRUE(state.getObject(second).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1);
    EXPECT_EQ(state.getObject(second).buff_count, 0);
    EXPECT_EQ(state.getObject(second).current_might, printed);
}

// A token is that same next unit: any battlefield that does not block unit
// plays, then the buff, [Accelerate], and the destination are all spent.
TEST_F(EngineFixLines, LastCaress_ATokenIsOfferedAnyBattlefieldAndSpendsTheGrants) {
    threeBattlefields();
    auto hideout = battlefieldCard(0, kHideout, P1);
    fireTriggerThroughEngine(hideout, TriggerType::WhenYouConquerHere);
    auto spell = addToHand(P1, kCaress);
    playThroughEngine(spell);

    const auto dests = tokenPlayDests(state, P1);
    EXPECT_NE(std::find(dests.begin(), dests.end(), 0), dests.end());
    EXPECT_NE(std::find(dests.begin(), dests.end(), 1), dests.end());
    EXPECT_NE(std::find(dests.begin(), dests.end(), 2), dests.end());
    EXPECT_EQ(std::find(dests.begin(), dests.end(), 3), dests.end());

    state.player(P1).rune_pool.energy = 1;
    state.player(P1).rune_pool.universal_power = 1;
    GameObjectId token = kInvalidId;
    const LocationId enemy{BattlefieldLocation{BattlefieldId::fromRaw(1)}};
    EnginePathAgent pay(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        token = ex.createToken(P1, CardType::Unit, "Recruit", 1, {"Recruit"}, KeywordSet{},
                               enemy, /*enter_ready=*/false);
    }, &pay);
    ASSERT_NE(token, kInvalidId);
    EXPECT_EQ(state.getObject(token).location, enemy);
    EXPECT_FALSE(state.getObject(token).is_exhausted);
    EXPECT_EQ(state.getObject(token).buff_count, 1);
    EXPECT_EQ(state.getObject(token).current_might, 2);
    EXPECT_EQ(state.player(P1).next_unit_accelerate, 0);
    EXPECT_EQ(state.player(P1).next_unit_any_battlefield, 0);
    EXPECT_EQ(state.player(P1).next_unit_buff, 0);

    auto later = addToHand(P1, kUnit);
    fundPrintedPlusAccelerate();
    withEngine([&](GameEngine& e) {
        const auto spots = spotsFor(e.testHook_mainPhaseActions(P1), later, IntentType::PlayCard);
        EXPECT_FALSE(spots.bf[1]);
        EXPECT_FALSE(spots.bf[2]);
    });
    EnginePathAgent payAgain(EnginePathAgent::script({1}));
    playThroughEngine(later, &payAgain, LocationId{BaseLocation{P1}}, nullptr, false);
    EXPECT_TRUE(state.getObject(later).is_exhausted);
    EXPECT_EQ(state.getObject(later).buff_count, 0);
}

// Showdown and closed-state Ambush offer the battlefields the spell names.
TEST_F(EngineFixLines, LastCaress_AnAmbushUnitIsOfferedAnyBattlefield) {
    threeBattlefields();
    auto spell = addToHand(P1, kCaress);
    playThroughEngine(spell);
    auto ambush = addToHand(P1, kInferna);
    state.player(P1).rune_pool.energy = card_db.get(kInferna).energy_cost;

    std::optional<Intent> to_enemy;
    withEngine([&](GameEngine& e) {
        auto& st = e.mutableState();
        st.turn.phase = TurnPhase::MainPhase;
        st.turn.turn_player = P1;
        st.turn.ns_state = NeutralShowdownState::Showdown;
        st.turn.oc_state = OpenClosedState::Open;
        st.turn.focus_holder = P1;
        const auto showdown = spotsFor(e.generateLegalActions(), ambush, IntentType::PlayActionCard);
        EXPECT_TRUE(showdown.bf[0]);
        EXPECT_TRUE(showdown.bf[1]);
        EXPECT_FALSE(showdown.bf[2]);
        EXPECT_FALSE(showdown.base);

        st.turn.ns_state = NeutralShowdownState::Neutral;
        st.turn.oc_state = OpenClosedState::Closed;
        st.turn.priority_holder = P1;
        const auto closed = spotsFor(e.generateLegalActions(), ambush, IntentType::PlayReaction);
        EXPECT_TRUE(closed.bf[0]);
        EXPECT_TRUE(closed.bf[1]);
        EXPECT_FALSE(closed.bf[2]);
        EXPECT_FALSE(closed.base);
        for (const auto& intent : e.generateLegalActions()) {
            if (intent.card != ambush || intent.type != IntentType::PlayReaction) continue;
            if (!intent.play_location) continue;
            if (const auto* bf = std::get_if<BattlefieldLocation>(&*intent.play_location))
                if (bf->id == BattlefieldId::fromRaw(1)) to_enemy = intent;
        }
    });
    ASSERT_TRUE(to_enemy.has_value());

    withEngine([&](GameEngine& e) {
        e.mutableState().turn.oc_state = OpenClosedState::Closed;
        e.mutableState().turn.priority_holder = P1;
        e.testHook_executeIntent(*to_enemy);
        settleLikeTheEngine(e);
    });
    const int printed = card_db.get(kInferna).might;
    const LocationId enemy{BattlefieldLocation{BattlefieldId::fromRaw(1)}};
    EXPECT_EQ(state.getObject(ambush).location, enemy);
    EXPECT_EQ(state.getObject(ambush).buff_count, 1);
    EXPECT_EQ(state.getObject(ambush).current_might, printed + 1);
    EXPECT_EQ(state.player(P1).next_unit_any_battlefield, 0);
    EXPECT_EQ(state.player(P1).next_unit_buff, 0);

    auto later = addToHand(P1, kUnit);
    state.player(P1).rune_pool.energy = card_db.get(kUnit).energy_cost;
    withEngine([&](GameEngine& e) {
        const auto spots = spotsFor(e.testHook_mainPhaseActions(P1), later, IntentType::PlayCard);
        EXPECT_FALSE(spots.bf[1]);
    });
    playThroughEngine(later, nullptr, LocationId{BaseLocation{P1}}, nullptr, false);
    EXPECT_EQ(state.getObject(later).buff_count, 0);
}

}  // namespace test_engine_fix_lines
}  // namespace riftbound::test
