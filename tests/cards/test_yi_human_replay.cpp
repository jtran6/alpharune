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

    int exhaustedRunesOnBase(PlayerId owner) const {
        int n = 0;
        for (const auto& [id, obj] : state.objects) {
            (void)id;
            if (obj.card_type != CardType::Rune) continue;
            if (obj.zone != ZoneType::Base || obj.controller != owner) continue;
            if (obj.is_exhausted) ++n;
        }
        return n;
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

}  // namespace test_yi_human_replay
}  // namespace riftbound::test
