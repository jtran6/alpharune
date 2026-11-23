#pragma once
/// @file deck_builder.h
/// A constructed deck for one legend, and a playbook for that list.
///
/// The main deck follows the unit, spell, and gear split of that legend's
/// saved lists when those lists are on disk under internal/deck-catalogue or
/// internal/tournament-data. With none, the split stays near 18 units, 18
/// spells, and 3 gear. `DeckValidator::validate` is the judge, on legality
/// date 20261123. A channel or ramp spell is not added to fill a type count.
///
/// The playbook's played lines are `PlayByPlay` accounts of `GameEngine::runGame`.
/// Both seats play this list with `random-player:pass=0`. A section with no
/// line from a usable game says it is unread.

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
    /// Builder choices the validator does not make: sideboard, battlefields, Neeko.
    std::vector<std::string> notes;
    /// The date `DeckValidator` used. Radiance Standard bans begin on this day.
    int legality_date = 20261123;
    /// Main-deck counts. The chosen champion is not one of these 39.
    int units = 0;
    int spells = 0;
    int gear = 0;
    /// Saved lists the unit/spell/gear split was taken from. Empty when none
    /// were found and the split stays near 18 / 18 / 3.
    std::vector<std::string> split_sources;
    /// Cards the saved lists played that this list left out because they are
    /// banned on `legality_date`.
    std::vector<std::string> banned_left_out;
};

/// The deck of `legend`. `legend` is a printed name or the "Champion, Title"
/// form deck lists use. Saved lists are read from internal/deck-catalogue and
/// internal/tournament-data under the working directory, its parents, and
/// `RIFTBOUND_ROOT` when that variable is set. Throws `std::runtime_error`
/// when that card is missing, is not a legend, or has no pool a legal deck
/// can be built from, and when the list `DeckValidator` returns is not legal
/// on 20261123.
DeckBuild buildLegendDeck(const CardDB& db, const std::string& legend);

/// Same deck, reading saved lists only under `list_roots`. An empty vector
/// means no saved lists, so the split stays near 18 units, 18 spells, and 3 gear.
DeckBuild buildLegendDeck(const CardDB& db, const std::string& legend,
                          const std::vector<std::string>& list_roots);

/// The playbook for `built`. `games` mirror games start at `seed` (the next
/// game uses the next seed). `games` is 0 to 20. Seed 0 is rejected so a
/// printed game can be replayed. Throws `std::invalid_argument` for a bad
/// count or seed, and `std::runtime_error` when a game cannot be seated.
std::string formatLegendPlaybook(const CardDB& db, const CardRegistry& registry,
                                 const DeckBuild& built, int games, uint64_t seed);

}  // namespace riftbound
