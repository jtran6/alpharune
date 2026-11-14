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
    const auto kept = searchCandidates(state, {bounce}, card_registry);
    EXPECT_TRUE(chooses(kept, friendly));
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
