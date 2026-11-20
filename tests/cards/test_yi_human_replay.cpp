// Lines from JT's three RiftAtlas games with the Master Yi, Wuju Bladesman
// list. Each assertion is a count taken from a replay frame, not a rule
// invented for the test.

#include "card_test_fixture.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace test_yi_human_replay {

class YiHumanReplay : public CardTestFixture {
protected:
    static constexpr CardDefId kWyvern = 1026;    // Seraphic Wyvern
    static constexpr CardDefId kMobilize = 134;   // Mobilize: channel 1 exhausted
    static constexpr CardDefId kRetreat = 104;    // Retreat: bounce, then channel 1 exhausted

    GameObjectId addRuneToRuneDeck(PlayerId owner) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_type = CardType::Rune;
        obj.name = "Rune";
        obj.domains = {Domain::Body};
        obj.zone = ZoneType::RuneDeck;
        state.player(owner).rune_deck.push_back(id);
        return id;
    }

    int runesOnBase(PlayerId owner, bool exhausted) const {
        int n = 0;
        for (const auto& [id, obj] : state.objects) {
            (void)id;
            if (obj.card_type != CardType::Rune) continue;
            if (obj.zone != ZoneType::Base || obj.controller != owner) continue;
            if (obj.is_exhausted == exhausted) ++n;
        }
        return n;
    }

    int exhaustedRunesOnBase(PlayerId owner) const { return runesOnBase(owner, true); }

    // Play `spell` from hand at `target` and settle, the way a main-phase
    // decision does. Funds the printed cost so the rune deck is not the payment.
    void playSpellAt(GameObjectId spell, GameObjectId target) {
        const PlayerId player = state.getObject(spell).controller;
        const auto& def = card_db.get(state.getObject(spell).card_def_id);
        state.player(player).rune_pool.energy += def.energy_cost;
        state.player(player).rune_pool.universal_power += def.power_cost;
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = player;
        play.card = spell;
        play.targets = {target};
        withEngineDefaultTurn([&](GameEngine& e) {
            e.testHook_executeIntent(play);
            settleLikeTheEngine(e);
        }, nullptr, nullptr, player);
    }
};

// D9X8F turn 11, Hart, frames F153-F158. Two Seraphic Wyverns were on base.
// Mobilize ("Channel 1 rune exhausted") took the rune deck from 4 to 1, and
// the three runes that entered were exhausted. One channel from the spell,
// plus one exhausted rune per Wyvern. On master the aura assigns
// extra_exhausted_channel = 1, so a second copy does not add its rune and
// the deck is left at 2.
TEST_F(YiHumanReplay, TwoWyvernsEachAddOneExhaustedRuneToMobilize) {
    addUnit(P1, kWyvern);
    addUnit(P1, kWyvern);
    for (int i = 0; i < 4; ++i) addRuneToRuneDeck(P1);

    playThroughEngine(addToHand(P1, kMobilize));

    EXPECT_EQ(state.player(P1).rune_deck.size(), 1u);
    EXPECT_EQ(exhaustedRunesOnBase(P1), 3);
}

// The same board with one Wyvern channels Mobilize's rune plus that one
// extra, and leaves the other two in the deck. This is what master already
// does for a single copy; the two-copy line above is the one it misses.
TEST_F(YiHumanReplay, OneWyvernAddsExactlyOneExhaustedRune) {
    addUnit(P1, kWyvern);
    for (int i = 0; i < 4; ++i) addRuneToRuneDeck(P1);

    playThroughEngine(addToHand(P1, kMobilize));

    EXPECT_EQ(state.player(P1).rune_deck.size(), 2u);
    EXPECT_EQ(exhaustedRunesOnBase(P1), 2);
}

// Two Wyverns, Retreat returns one, then its owner channels. The bounced
// copy has left before the channel, so only the one still in play adds a
// rune: Retreat's 1 plus that 1, both exhausted, deck of 4 left at 2.
// 859c7ebd still adds the copy that left (the last Cleanup stored 2) and
// leaves the deck at 1. On master the field is 1, so this Retreat channels 2.
TEST_F(YiHumanReplay, RetreatOfOneWyvernDoesNotChannelForTheCopyThatLeft) {
    const GameObjectId stayed = addUnit(P1, kWyvern);
    const GameObjectId bounced = addUnit(P1, kWyvern);
    for (int i = 0; i < 4; ++i) addRuneToRuneDeck(P1);

    playSpellAt(addToHand(P1, kRetreat), bounced);

    EXPECT_EQ(state.getObject(bounced).zone, ZoneType::Hand);
    EXPECT_TRUE(state.getObject(stayed).location.has_value());
    EXPECT_EQ(state.player(P1).rune_deck.size(), 2u);
    EXPECT_EQ(exhaustedRunesOnBase(P1), 2);
    EXPECT_EQ(runesOnBase(P1, false), 0);
}

// Channel Phase, one Wyvern in play, four runes in the deck. The phase
// channels 2 ready, and the Wyvern adds 1 exhausted. The deck is left at 1.
// GameEngine::channelRunes on 859c7ebd never reads the extra, so it channels
// 2 ready and leaves the deck at 2.
TEST_F(YiHumanReplay, ChannelPhaseWithOneWyvernChannelsTwoReadyAndOneExhausted) {
    addUnit(P1, kWyvern);
    for (int i = 0; i < 4; ++i) addRuneToRuneDeck(P1);
    state.turn.starting_player = P1;
    state.player(P1).is_first_turn = false;

    withEngineDefaultTurn([&](GameEngine& e) {
        e.testHook_cleanup();
        e.testHook_channelPhase();
    }, nullptr, nullptr, P1);

    EXPECT_EQ(state.player(P1).rune_deck.size(), 1u);
    EXPECT_EQ(runesOnBase(P1, false), 2);
    EXPECT_EQ(exhaustedRunesOnBase(P1), 1);
}

}  // namespace test_yi_human_replay
}  // namespace riftbound::test
