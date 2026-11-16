// The illegal lines from the judge review. Each test fails on the engine
// before the fix in docs/engine-fix-plan.md and passes after it.

#include "tests/cards/card_test_fixture.h"
#include "cards/card_helpers.h"

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

}  // namespace test_engine_fix_lines
}  // namespace riftbound::test
