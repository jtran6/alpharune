#pragma once
/// @file deck_breakdown.h
/// One deck list, read as text: roles from printed cost and ability text, and
/// the chance of drawing each role and each card. Nothing here plays a game.

#include "core/card_db.h"
#include "rules/deck_validator.h"

#include <string>
#include <utility>
#include <vector>

namespace riftbound {

/// One card name in a role, a battlefield, a rune, or the sideboard.
struct DeckCardLine {
    CardDefId id = kInvalidId;
    std::string name;
    int copies = 0;
    bool champion = false;  // this line includes the Champion Zone copy
};

/// A role the list actually has. Empty roles are not reported.
struct DeckRole {
    std::string title;
    std::string reason;
    std::vector<DeckCardLine> cards;
    int copies = 0;       // main deck + champion
    int draw_copies = 0;  // main deck only; the champion is not drawn
};

/// One moment in the opening: the hand, then each of the first three turns.
struct DeckStage {
    std::string name;
    int natural = 0;  // cards drawn by the turn itself, before extra looks
    // Probability of seeing at least one copy. Role titles match DeckRole::title.
    std::vector<std::pair<std::string, double>> roles;
    std::vector<std::pair<std::string, double>> cards;
};

/// The whole breakdown. `counted` is the sentence that says what the chances use.
struct DeckReport {
    std::string legend;
    std::string champion;
    int draw_pile = 0;
    int late_game_cost = 0;  // highest energy cost in the main deck and champion
    bool plain_draw = true;  // no draw, Predict, recycle, or tutor in that list
    std::string counted;
    std::vector<std::pair<int, int>> curve;  // energy cost, copies (main + champion)
    std::vector<DeckCardLine> battlefields;
    std::vector<DeckCardLine> runes;
    std::vector<DeckCardLine> sideboard;
    std::vector<DeckRole> roles;
    std::vector<DeckStage> stages;
};

/// Roles and draw chances for `deck`.
///
/// The main deck and the champion are classified. Battlefields and runes are
/// listed on their own. The sideboard is recorded and left out of every chance.
/// A 2-drop has energy cost 2. Late game is the highest energy cost in this
/// list. Ramp, combat tricks, and the other roles are read off the printed
/// text and keywords the card already carries.
DeckReport analyzeDeck(const DeckSubmission& deck, const CardDB& db);

/// Human-readable form of `analyzeDeck`. No pass or fail line.
std::string formatDeckReport(const DeckReport& report);

}  // namespace riftbound
