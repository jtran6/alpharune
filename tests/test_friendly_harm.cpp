#include "agents/friendly_harm.h"
#include "cards/card_test_fixture.h"

#include <algorithm>

namespace riftbound::test::friendly_harm {

class FriendlyHarmTest : public CardTestFixture {
protected:
    void fund(PlayerId player) {
        state.player(player).rune_pool.energy = 10;
        state.player(player).rune_pool.universal_power = 10;
    }

    static bool plays(const std::vector<Intent>& legal, GameObjectId card) {
        for (const auto& it : legal)
            if ((it.type == IntentType::PlayCard || it.type == IntentType::PlayReaction ||
                 it.type == IntentType::PlayActionCard) &&
                it.card == card)
                return true;
        return false;
    }

    static bool playsAt(const std::vector<Intent>& legal, GameObjectId card,
                        GameObjectId target) {
        for (const auto& it : legal) {
            if (it.card != card) continue;
            if (std::find(it.targets.begin(), it.targets.end(), target) != it.targets.end())
                return true;
        }
        return false;
    }

    static bool chooses(const std::vector<Intent>& legal, GameObjectId unit) {
        for (const auto& it : legal)
            if (std::find(it.chosen_objects.begin(), it.chosen_objects.end(), unit) !=
                it.chosen_objects.end())
                return true;
        return false;
    }

    static bool playsNothing(const std::vector<Intent>& legal, GameObjectId card) {
        for (const auto& it : legal) {
            if (it.card != card) continue;
            if (it.type != IntentType::PlayCard && it.type != IntentType::PlayReaction &&
                it.type != IntentType::PlayActionCard)
                continue;
            if (it.targets.empty() && it.chosen_objects.empty()) return true;
        }
        return false;
    }
};

// The rules engine still offers Bellows when the only unit is the caster's
// own 2-Might Lecturing Yordle. Search does not: 1 damage does not kill it.
TEST_F(FriendlyHarmTest, EmptyBellowsStaysLegalAndIsNotSearched) {
    addUnit(P1, 87, 2);
    const auto bellows = addToHand(P1, 402);
    fund(P1);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(plays(legal, bellows));
        const auto kept = searchCandidates(engine.state(), legal, card_registry);
        EXPECT_FALSE(plays(kept, bellows));
        EXPECT_FALSE(kept.empty());
    });
}

// An enemy the spell can hit keeps the play. The friendly line is dropped
// at the choice, not by taking the spell away.
TEST_F(FriendlyHarmTest, BellowsPlayStaysWhenAnEnemyCanBeHit) {
    addUnit(P1, 87, 2);
    addUnit(P2, 371, 3);
    const auto bellows = addToHand(P1, 402);
    fund(P1);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(plays(legal, bellows));
        EXPECT_TRUE(plays(searchCandidates(engine.state(), legal, card_registry), bellows));
    });
}

// Smoke Screen on the caster's Bilgewater Tracker does not kill it (Might
// floors at 1). Stellacorn Herder is a legal enemy target, and that is the
// line search keeps.
TEST_F(FriendlyHarmTest, SmokeScreenDropsTheFriendlyTarget) {
    const auto tracker = addUnit(P1, 958, 3);
    const auto herder = addUnit(P2, 371, 3);
    const auto smoke = addToHand(P1, 93);
    fund(P1);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(playsAt(legal, smoke, tracker));
        EXPECT_TRUE(playsAt(legal, smoke, herder));
        const auto kept = searchCandidates(engine.state(), legal, card_registry);
        EXPECT_FALSE(playsAt(kept, smoke, tracker));
        EXPECT_TRUE(playsAt(kept, smoke, herder));
    });
}

// Bellows kills Watchful Sentry and the Sentry's [Deathknell] draws. That
// friendly line stays in the search.
TEST_F(FriendlyHarmTest, BellowsOnDeathknellStays) {
    const auto sentry = addUnit(P1, 96);
    ASSERT_TRUE(state.getObject(sentry).hasKeyword(Keyword::Deathknell));
    ASSERT_EQ(state.getObject(sentry).current_might, 1);
    const auto bellows = addToHand(P1, 402);
    fund(P1);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(plays(legal, bellows));
        EXPECT_TRUE(plays(searchCandidates(engine.state(), legal, card_registry), bellows));
    });
}

// While Bellows is choosing: the 2-Might Yordle is dropped, the Sentry
// (the damage kills it and it has [Deathknell]) stays, and so does the enemy.
TEST_F(FriendlyHarmTest, BellowsChoiceKeepsTheDeathknell) {
    const auto yordle = addUnit(P1, 87, 2);
    const auto sentry = addUnit(P1, 96);
    const auto enemy = addUnit(P2, 371, 3);
    const auto spell = state.createObject();
    auto& obj = state.getObject(spell);
    obj.card_def_id = 402;
    obj.card_type = CardType::Spell;
    obj.owner = P1;
    obj.controller = P1;
    obj.zone = ZoneType::Chain;
    ChainItem item;
    item.source = spell;
    item.is_spell = true;
    state.chain.resuming = item;

    auto option = [](GameObjectId unit) {
        Intent intent;
        intent.type = IntentType::MakeChoice;
        intent.player = PlayerId::Player1;
        if (unit != kInvalidId) intent.chosen_objects = {unit};
        return intent;
    };
    const std::vector<Intent> legal = {option(kInvalidId), option(yordle), option(sentry),
                                       option(enemy)};
    const auto kept = searchCandidates(state, legal, card_registry);
    EXPECT_FALSE(chooses(kept, yordle));
    EXPECT_TRUE(chooses(kept, sentry));
    EXPECT_TRUE(chooses(kept, enemy));
    EXPECT_EQ(kept.size(), 3u);
}

// Rebuke names its unit as it resolves, so the play itself has no target.
// With an enemy at a battlefield the play stays. The choice of that enemy
// stays; bouncing the caster's own unit does not.
TEST_F(FriendlyHarmTest, RebukeOnTheEnemyStays) {
    const auto enemy = addUnit(P1, 87, 2, 0);
    const auto own = addUnit(P2, 87, 2, 1);
    const auto rebuke = addToHand(P2, 172);
    fund(P2);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(plays(legal, rebuke));
        EXPECT_TRUE(plays(searchCandidates(engine.state(), legal, card_registry), rebuke));
    }, nullptr, nullptr, P2);

    const auto spell = state.createObject();
    auto& obj = state.getObject(spell);
    obj.card_def_id = 172;
    obj.card_type = CardType::Spell;
    obj.owner = P2;
    obj.controller = P2;
    obj.zone = ZoneType::Chain;
    ChainItem item;
    item.source = spell;
    item.is_spell = true;
    state.chain.resuming = item;
    auto option = [&](GameObjectId unit) {
        Intent intent;
        intent.type = IntentType::MakeChoice;
        intent.player = P2;
        intent.chosen_objects = {unit};
        return intent;
    };
    const std::vector<Intent> choices = {option(enemy), option(own)};
    const auto kept = searchCandidates(state, choices, card_registry);
    EXPECT_TRUE(chooses(kept, enemy));
    EXPECT_FALSE(chooses(kept, own));
}

// A printed friendly bounce is what the card wants. Grim still offers it.
TEST_F(FriendlyHarmTest, GrimFriendlyBounceStays) {
    const auto friendly = addUnit(P1, 87, 2, 0);
    addUnit(P2, 371, 3, 1);
    const auto grim = state.createObject();
    auto& obj = state.getObject(grim);
    obj.card_def_id = 583;
    obj.card_type = CardType::Unit;
    obj.owner = P1;
    obj.controller = P1;
    obj.zone = ZoneType::Chain;
    ChainItem item;
    item.source = grim;
    item.is_ability = true;
    state.chain.resuming = item;
    Intent bounce;
    bounce.type = IntentType::MakeChoice;
    bounce.player = P1;
    bounce.chosen_objects = {friendly};
    // End Turn stays either way. If the bounce were dropped, the old
    // empty-result fallback would have put it back and a one-option
    // assertion would still have passed.
    const std::vector<Intent> legal = {bounce, Intent::endTurn(P1)};
    const auto kept = searchCandidates(state, legal, card_registry);
    EXPECT_TRUE(chooses(kept, friendly));
    EXPECT_EQ(kept.size(), legal.size());
}

// The unit is the line. A healthy friendly already on the board must not
// remove the play: Tracker's ping is optional, and Leona / the Watcher
// never harm a friendly.
TEST_F(FriendlyHarmTest, UnitPlayStaysBesideAHealthyFriendly) {
    addUnit(P1, 87, 2);
    fund(P1);
    const auto tracker = addToHand(P1, 958);
    const auto leona = addToHand(P1, 79);
    const auto watcher = addToHand(P1, 116);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(plays(legal, tracker));
        EXPECT_TRUE(plays(legal, leona));
        EXPECT_TRUE(plays(legal, watcher));
        const auto kept = searchCandidates(engine.state(), legal, card_registry);
        EXPECT_TRUE(plays(kept, tracker));
        EXPECT_TRUE(plays(kept, leona));
        EXPECT_TRUE(plays(kept, watcher));
    });
}

// Stupefy's −1 Might still draws. With only Noxus Hopeful on the board the
// draw is the reason to cast it.
TEST_F(FriendlyHarmTest, StupefyOnTheOnlyFriendlyStillDraws) {
    const auto hopeful = addUnit(P1, 12);
    ASSERT_EQ(state.getObject(hopeful).current_might, 4);
    const auto spell = addToHand(P1, 95);
    fund(P1);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(playsAt(legal, spell, hopeful));
        EXPECT_TRUE(playsAt(searchCandidates(engine.state(), legal, card_registry), spell, hopeful));
    });
}

// The draw happens on an enemy target too, so the friendly debuff is the
// empty line once an enemy is legal.
TEST_F(FriendlyHarmTest, StupefyPrefersTheEnemy) {
    const auto hopeful = addUnit(P1, 12);
    const auto enemy = addUnit(P2, 371, 3);
    const auto spell = addToHand(P1, 95);
    fund(P1);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(playsAt(legal, spell, hopeful));
        EXPECT_TRUE(playsAt(legal, spell, enemy));
        const auto kept = searchCandidates(engine.state(), legal, card_registry);
        EXPECT_FALSE(playsAt(kept, spell, hopeful));
        EXPECT_TRUE(playsAt(kept, spell, enemy));
    });
}

// Eclipse's Predict happens even when the −4 Might does not kill.
TEST_F(FriendlyHarmTest, EclipsePredictOnTheOnlyFriendlyStays) {
    const auto friendly = addUnit(P1, 87, 5);
    const auto spell = addToHand(P1, 625);
    fund(P1);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(playsAt(legal, spell, friendly));
        EXPECT_TRUE(playsAt(searchCandidates(engine.state(), legal, card_registry), spell, friendly));
    });
}

// Temporal Breach banishes the unit and its owner plays it again. That
// replay is a real line on your own unit.
TEST_F(FriendlyHarmTest, TemporalBreachReplayOnOwnUnitStays) {
    const auto own = addUnit(P1, 958, 3);
    const auto spell = addToHand(P1, 853);
    fund(P1);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(plays(legal, spell));
        EXPECT_TRUE(plays(searchCandidates(engine.state(), legal, card_registry), spell));
    });

    const auto source = state.createObject();
    auto& obj = state.getObject(source);
    obj.card_def_id = 853;
    obj.card_type = CardType::Spell;
    obj.owner = P1;
    obj.controller = P1;
    obj.zone = ZoneType::Chain;
    ChainItem item;
    item.source = source;
    item.is_spell = true;
    state.chain.resuming = item;
    Intent choice;
    choice.type = IntentType::MakeChoice;
    choice.player = P1;
    choice.chosen_objects = {own};
    const auto kept = searchCandidates(state, {choice, Intent::endTurn(P1)}, card_registry);
    EXPECT_TRUE(chooses(kept, own));
}

// Gust cannot target a 4-Might enemy. The only legal unit is a friendly the
// bounce does not kill, so the play leaves the search. The choice list is
// not handed back when every option was dropped.
TEST_F(FriendlyHarmTest, GustEmptyFriendlyIsNotRestored) {
    const auto friendly = addUnit(P1, 87, 2, 0);
    addUnit(P2, 12, 4, 1);
    const auto spell = addToHand(P1, 169);
    fund(P1);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(plays(legal, spell));
        EXPECT_FALSE(plays(searchCandidates(engine.state(), legal, card_registry), spell));
    });

    const auto source = state.createObject();
    auto& obj = state.getObject(source);
    obj.card_def_id = 169;
    obj.card_type = CardType::Spell;
    obj.owner = P1;
    obj.controller = P1;
    obj.zone = ZoneType::Chain;
    ChainItem item;
    item.source = source;
    item.is_spell = true;
    state.chain.resuming = item;
    Intent choice;
    choice.type = IntentType::MakeChoice;
    choice.player = P1;
    choice.chosen_objects = {friendly};
    const auto kept = searchCandidates(state, {choice}, card_registry);
    EXPECT_FALSE(chooses(kept, friendly));
    EXPECT_TRUE(kept.empty());
}

// Smoke Screen's −4 floors at 1, so Noxus Hopeful (4) and Plundering Poro
// (2) stay. Watchful Sentry is 1 Might with [Deathknell]: the floor is the
// digit after "minimum of " (11 characters). Reading one past it leaves the
// floor unset, scores Might 0, and keeps the Sentry because a scored kill
// with [Deathknell] stays. No enemy is a legal target. None of these is
// searched. Jayce seeds 22 (Hopeful) and 31 (Poro).
TEST_F(FriendlyHarmTest, SmokeScreenDropsALivingFriendlyWhenNoEnemyIsLegal) {
    const auto hopeful = addUnit(P1, 12);
    const auto poro = addUnit(P1, 392);
    const auto sentry = addUnit(P1, 96);
    ASSERT_EQ(state.getObject(hopeful).current_might, 4);
    ASSERT_EQ(state.getObject(poro).current_might, 2);
    ASSERT_EQ(state.getObject(sentry).current_might, 1);
    ASSERT_TRUE(state.getObject(sentry).hasKeyword(Keyword::Deathknell));
    const auto smoke = addToHand(P1, 93);
    fund(P1);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(playsAt(legal, smoke, hopeful));
        EXPECT_TRUE(playsAt(legal, smoke, poro));
        EXPECT_TRUE(playsAt(legal, smoke, sentry));
        const auto kept = searchCandidates(engine.state(), legal, card_registry);
        EXPECT_FALSE(playsAt(kept, smoke, hopeful));
        EXPECT_FALSE(playsAt(kept, smoke, poro));
        EXPECT_FALSE(playsAt(kept, smoke, sentry));
    });
}

// Stupefy still draws when the only unit is a 1-Might [Deathknell]. The
// floor leaves her alive, and the draw is the line.
TEST_F(FriendlyHarmTest, StupefyOnTheOnlyDeathknellStillDraws) {
    const auto sentry = addUnit(P1, 96);
    ASSERT_EQ(state.getObject(sentry).current_might, 1);
    ASSERT_TRUE(state.getObject(sentry).hasKeyword(Keyword::Deathknell));
    const auto spell = addToHand(P1, 95);
    fund(P1);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(playsAt(legal, spell, sentry));
        EXPECT_TRUE(playsAt(searchCandidates(engine.state(), legal, card_registry), spell, sentry));
    });
}

// Singularity may deal 6 to nobody. That empty set names no unit, so it is
// not an enemy, and it does not keep the line that kills her own champion
// or her own Tracker. The empty cast itself stays: it harms nobody.
TEST_F(FriendlyHarmTest, SingularityDropsTheFriendlyKillWhenNoEnemyIsLegal) {
    const auto kaisa = addUnit(P1, 39);
    const auto tracker = addUnit(P1, 958);
    ASSERT_EQ(state.getObject(kaisa).current_might, 4);
    ASSERT_EQ(state.getObject(tracker).current_might, 3);
    const auto spell = addToHand(P1, 105);
    fund(P1);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(playsAt(legal, spell, kaisa));
        EXPECT_TRUE(playsAt(legal, spell, tracker));
        EXPECT_TRUE(playsNothing(legal, spell));
        const auto kept = searchCandidates(engine.state(), legal, card_registry);
        EXPECT_FALSE(playsAt(kept, spell, kaisa));
        EXPECT_FALSE(playsAt(kept, spell, tracker));
        EXPECT_TRUE(playsNothing(kept, spell));
    });
}

// Hextech Ray has no empty target. 3 damage kills a Poro already at 1
// damage on 2 Might, and her conquer text is not a death. No enemy unit
// is at a battlefield. The kill is not searched.
TEST_F(FriendlyHarmTest, HextechRayDropsTheFriendlyKillWhenNoEnemyIsLegal) {
    const auto poro = addUnit(P1, 392, /*might=*/0, /*at_bf=*/0);
    state.getObject(poro).damage_marked = 1;
    ASSERT_EQ(state.getObject(poro).current_might, 2);
    ASSERT_EQ(state.getObject(poro).damage_marked, 1);
    const auto ray = addToHand(P1, 9);
    fund(P1);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(playsAt(legal, ray, poro));
        EXPECT_FALSE(playsAt(searchCandidates(engine.state(), legal, card_registry), ray, poro));
    });
}

// Tracker deals 1 to each of up to 2 units. The first prompt can take the
// enemy Scuttle Crab. selectSeveralTargets then erases that id, so the
// second prompt no longer offers him, and the decline names nobody. The
// second point, which kills her own Lecturing Yordle (1 damage on 2 Might,
// no death text), is not searched. Riven seed 19 turn 6.
TEST_F(FriendlyHarmTest, TrackerSecondPointDropsTheFriendlyKill) {
    const auto yordle = addUnit(P1, 87);
    state.getObject(yordle).damage_marked = 1;
    ASSERT_EQ(state.getObject(yordle).current_might, 2);
    const auto crab = addUnit(P2, 615, /*might=*/1);
    const auto tracker = addToHand(P1, 958);

    struct Agent : AgentInterface {
        GameObjectId crab = kInvalidId;
        GameObjectId yordle = kInvalidId;
        const CardRegistry* cards = nullptr;
        bool saw_second = false;
        bool crab_on_second = false;
        bool yordle_offered = false;
        bool yordle_kept = true;
        bool decline_kept = false;

        Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
            const bool choice = !legal.empty() && std::all_of(legal.begin(), legal.end(),
                [](const Intent& i) { return i.type == IntentType::MakeChoice; });
            auto names = [](const Intent& i, GameObjectId id) {
                return std::find(i.chosen_objects.begin(), i.chosen_objects.end(), id) !=
                       i.chosen_objects.end();
            };
            bool tracker_prompt = false;
            for (const auto& i : legal)
                if (i.choice_label.find("Bilgewater Tracker") != std::string::npos)
                    tracker_prompt = true;
            if (choice && tracker_prompt) {
                bool offers_crab = false;
                bool offers_yordle = false;
                for (const auto& i : legal) {
                    if (names(i, crab)) offers_crab = true;
                    if (names(i, yordle)) offers_yordle = true;
                }
                if (offers_crab) {
                    for (const auto& i : legal)
                        if (names(i, crab)) return i;
                }
                saw_second = true;
                crab_on_second = offers_crab;
                yordle_offered = offers_yordle;
                const auto kept = searchCandidates(state, legal, *cards);
                yordle_kept = false;
                decline_kept = false;
                for (const auto& i : kept) {
                    if (names(i, yordle)) yordle_kept = true;
                    if (i.chosen_objects.empty() && i.targets.empty()) decline_kept = true;
                }
                for (const auto& i : legal)
                    if (i.chosen_objects.empty()) return i;
            }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus)
                    return i;
            return legal.front();
        }
    } agent;
    agent.crab = crab;
    agent.yordle = yordle;
    agent.cards = &card_registry;
    playThroughEngine(tracker, &agent);

    EXPECT_TRUE(agent.saw_second);
    EXPECT_FALSE(agent.crab_on_second);
    EXPECT_TRUE(agent.yordle_offered);
    EXPECT_FALSE(agent.yordle_kept);
    EXPECT_TRUE(agent.decline_kept);
}

// +Might on a friendly unit is a buff, not harm.
TEST_F(FriendlyHarmTest, EnGardeOnAFriendlyStays) {
    const auto yordle = addUnit(P1, 87, 2);
    const auto spell = addToHand(P1, 46);
    fund(P1);
    withEngine([&](GameEngine& engine) {
        const auto legal = engine.generateLegalActions();
        EXPECT_TRUE(playsAt(legal, spell, yordle));
        EXPECT_TRUE(playsAt(searchCandidates(engine.state(), legal, card_registry), spell, yordle));
    });
}

}  // namespace riftbound::test::friendly_harm
