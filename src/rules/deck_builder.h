#pragma once
/// @file deck_builder.h
/// A constructed deck for one legend, and a playbook read from printed text
/// and from mirror games of that deck.
///
/// The legend is an input. Cards are taken from the pool `RandomDeckBuilder`
/// already uses for that legend. `DeckValidator::validate` is what calls the
/// list legal. A rule that validator does not apply is named in `DeckBuild::notes`
/// and is not reported as a pass.
///
/// The playbook's played lines are `PlayByPlay` accounts of `GameEngine::runGame`,
/// the same account the `riftbound` binary and `GameRunner` print. Both seats
/// play this list with `random-player:pass=0`. A section with no line from a
/// usable game says it is unread.

#include "core/card_db.h"
#include "rules/deck_validator.h"

#include <cstdint>
#include <string>
#include <vector>

namespace riftbound {

class CardRegistry;

/// One built list. `list` is the deck-file text `DeckValidator::loadFromDeckList` reads.
struct DeckBuild {
    DeckSubmission deck;
    std::string list;
    std::string legend_query;
    std::string printed_name;
    /// "Champion, Title" when the legend has a champion tag. May equal `legend_query`.
    std::string alias;
    /// Builder choices, and rules `DeckValidator` does not apply.
    std::vector<std::string> notes;
};

/// The deck of `legend`. `legend` is a printed name or the "Champion, Title"
/// form deck lists use. Throws `std::runtime_error` when that card is missing,
/// is not a legend, or has no pool a legal deck can be built from, and when
/// the list `DeckValidator` returns is not legal.
DeckBuild buildLegendDeck(const CardDB& db, const std::string& legend);

/// The playbook for `built`. `games` mirror games start at `seed` (the next
/// game uses the next seed). `games` is 0 to 20. Seed 0 is rejected so a
/// printed game can be replayed. Throws `std::invalid_argument` for a bad
/// count or seed, and `std::runtime_error` when a game cannot be seated.
std::string formatLegendPlaybook(const CardDB& db, const CardRegistry& registry,
                                 const DeckBuild& built, int games, uint64_t seed);

}  // namespace riftbound
