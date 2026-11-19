# Tournament lists

1,347 constructed deck lists from 13 completed events. 1,283 are from
[Riftwatch](https://riftwatch.gg/tournaments/), pulled at 12:28 PM ET on
November 19, 2026. 64 more are from
[RiftDecks](https://riftdecks.com/riftbound-tournaments/s5-shanghai-city-challenge-tournament-decks-19014),
the published lists for the S5 Shanghai City Challenge, pulled at 2:54 PM ET
the same day. The window is a start date on or after the Radiance release
(2026-10-23) through that day. The Piltover Archive tournament page
(`https://piltoverarchive.com/tournaments`) had no event rows, so nothing was
taken from it.

Each deck is one `.txt` file in the engine's deck format. A path works as
`--deck1`, `--deck2`, or `--deck-breakdown`. See the repository `README.md`
for those commands. `matchups.txt` is not a deck file.

```
Legend:
Champion:
MainDeck:
Battlefields:
Runes:
Sideboard:
```

Lines that start with `#` are the event, the date, the place, the player, the
record, and the source URL. The loader skips them. The sideboard stays out of
`MainDeck`. `Name #id` is used only when the first printing of that name is a
different card (`Pouty Poro #776` in 11 lists). A header that names a promo
code the engine does not have (`VEN-SP3`, `VEN-SP4`) kept the card under the
name the loader already resolves.

The November 23, 2026 bans have not started. Those cards stay in the list, and
the header names each one the file contains. On and after that date a game
rejects them unless you pass `--wild`. `--deck-breakdown` does not apply the
ban check.

`Master Yi, Wuju Bladesman` is the engine's name for the Proving Grounds
starter. `Master Yi, Wuju Master` is the Unleashed legend.

No list was marked illegal by the source. Every saved list is the constructed
shape: 1 legend, 1 champion, 39 main-deck cards, 3 battlefields, 12 runes, and
a sideboard of at most 10. One Orlando list (190th, cheeserdude) has no record
on the page. Place numbers that are missing were not published: Stuttgart has
no 6th, and Melbourne has no 3rd through 7th. Shanghai's pages do not state a
win-loss record. One Shanghai list (42nd, 懒狗) has the 8 sideboard cards the
page published. The 36th-place legend is printed "Jarvaniv, Exemplar of
Demacia" there and written as Jarvan IV, Exemplar of Demacia. RiftDecks does
not name a printing, so a shared name loads the first one and the header says
which id that is.

## Events

| Event | Date | Lists | Top cut |
| --- | --- | --- | --- |
| [S5 Shanghai City Challenge](https://riftdecks.com/riftbound-tournaments/s5-shanghai-city-challenge-tournament-decks-19014) | 2026-11-15 | 64 | not published |
| [CCS $50,000 Invitational + LCQs](https://riftwatch.gg/tournaments/ccs-50-000-riftbound-invitational-event-lcqs-2026-11-14/) | 2026-11-14 | 70 | not published |
| [Stuttgart Regional Championship](https://riftwatch.gg/tournaments/stuttgart-regional-championship-2026-11-06/) | 2026-11-06 | 43 | `stuttgart-regional-championship-2026-11-06/matchups.txt` (7 matches) |
| [Convergence IV](https://riftwatch.gg/tournaments/convergence-iv-2026-10-31/) | 2026-10-31 | 232 | not published |
| [Chaotic Goods $2k Chaos Cup](https://riftwatch.gg/tournaments/chaotic-goods-2k-chaos-cup-2026-10-31/) | 2026-10-31 | 26 | not published |
| [Orlando Showdown](https://riftwatch.gg/tournaments/orlando-showdown-2026-10-31/) | 2026-10-31 | 20 | `orlando-showdown-2026-10-31/matchups.txt` (7 matches) |
| [LDXP Sacramento Radiance-Win-A-Case](https://riftwatch.gg/tournaments/ldxp-sacramento-radiance-win-a-case-2026-10-25/) | 2026-10-25 | 34 | not published |
| [LDXP Sacramento Riftbound Challenge](https://riftwatch.gg/tournaments/ldxp-sacramento-riftbound-challenge-2026-10-25/) | 2026-10-25 | 33 | not published |
| [LDXP Sacramento Radiance-Win-A-Case](https://riftwatch.gg/tournaments/ldxp-sacramento-radiance-win-a-case-2026-10-24/) | 2026-10-24 | 13 | not published |
| [Sacramento Showdown](https://riftwatch.gg/tournaments/sacramento-showdown-2026-10-24/) | 2026-10-24 | 717 | not published |
| [Melbourne Showdown](https://riftwatch.gg/tournaments/melbourne-showdown-2026-10-24/) | 2026-10-24 | 19 | not published |
| [LDXP Sacramento Nexus Night Constructed](https://riftwatch.gg/tournaments/ldxp-sacramento-nexus-night-constructed-2026-10-23/) | 2026-10-23 | 32 | not published |
| [LDXP Sacramento Radiance-Win-A-Case](https://riftwatch.gg/tournaments/ldxp-sacramento-radiance-win-a-case-2026-10-23/) | 2026-10-23 | 44 | not published |

Sacramento's results card says 64. The page credits 717 decklists for a
755-player 1v1 Constructed event, including places past 32. Those lists are
included.

Stuttgart and Orlando are the only events whose live payload
(`/api/live?event=rc-europe-stuttgart` and `/api/live?event=showdown-orlando`)
published a top cut. Each file has the quarterfinals, semifinals, and final:
who beat whom, the score as winner's games then opponent's games, and the
deck path when that player published a list. Swiss rounds are not in either
payload. The other ten Riftwatch events publish standings and lists only.
Shanghai's RiftDecks stats tab is a legend-presence table. It does not say
who beat whom, so that folder has no `matchups.txt`.

Showdown Los Angeles (November 21–22, 2026) had not been played.
