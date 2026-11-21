# Riftbound Simulation Engine

A high-performance C++20 game engine that simulates 1v1 Riftbound TCG matches
end-to-end. Drop-in compatible with [OpenSpiel](https://github.com/google-deepmind/open_spiel)
so any tree-search algorithm (MCTS, ISMCTS, CFR, …) can plug straight in.

> **Disclaimer.** Released **as-is** for research and hobby use.
> Expect rough edges in: card coverage (1,123 hand-authored cards; a few are
> still stubs or carry a known engine gap, see `scripts/card_coverage.py`),
> edge-case rules interactions, performance on stress workloads, and corners of the OpenSpiel
> wrapper. PRs welcome; production deployment at your own risk.

> The training / ML side — neural agents, self-play loops, model checkpoints —
> lives in a separate sibling repo. This repo is just the simulator + AI
> baselines via OpenSpiel.

## What works today
- **Deck files.** A `.txt` list with `Legend`, `Champion`, `MainDeck`,
  `Battlefields`, `Runes`, and an optional `Sideboard`. Lines that start
  with `#` are skipped. `Name #id` selects a later printing of a shared
  name. Sample lists are in `decks/`. Completed Radiance constructed
  events are in `internal/tournament-data/` (one file per deck).
- **Full rules engine.** Awaken → channel → draw → main → end turn loop,
  FEPR chain resolution, combat with damage assignment, scoring, mulligans,
  battlefields, gear/equip, keyword mechanics, replacement effects.
  `--game-mode duel` draws each setup battlefield (the default).
  `--game-mode match` lets each player choose theirs.
- **1,123 cards**, each a hand-authored C++ class owning its own data and
  behavior. `python3 scripts/card_coverage.py` classifies every card file
  (implemented / stub / vanilla, plus in-code gap comments); its work lists
  are under [`docs/card-coverage/`](docs/card-coverage/).
- **Ban list by date.** A card with `banned_from` is illegal in Standard on
  and after that date. The Radiance list starts 2026-11-23. `--wild` skips
  the ban check. `--deck-breakdown` only reads the file, so it still prints
  a list that contains a card the ban has reached.
- **Deterministic.** Given a seed, the same game replays identically.
- **OpenSpiel integration.** `RiftboundGame` / `RiftboundState` implement
  the OpenSpiel `Game` / `State` interfaces. MCTS, ISMCTS, `alpha`, random,
  and random-player agents work out of the box. `alpha` is the honest
  information-set search with a default of 50 simulations.
- **Deck breakdown.** `--deck-breakdown D.txt` prints one list's curve,
  roles, and draw chances, then exits. No second deck and no game.
- **Play-by-play.** After a single game, and after each game of a batch of
  20 or fewer, the runner prints a short account: seed, both decks, both
  agents, how the game ended, the score, the mulligan, what each play paid,
  moves, combat, and who conquered or held.
- **HTML replays.** Per-game rendered HTML with board snapshots,
  decision points, trace log, arrow-key navigation.
- **Tests.** Google Test in `tests/` (`riftbound_tests`) plus the OpenSpiel
  wrapper tests (`riftbound_openspiel_tests`).
- **Engine-fiber step machine.** `boost::context::fiber`-based cooperative
  yields at decision points. No OS thread per game state, supports
  thousands of `Clone()` calls per decision for branching search.

## Quick start

```bash
# Install deps (Ubuntu/Debian)
sudo apt-get install cmake ninja-build g++ libboost-all-dev nlohmann-json3-dev

# Configure + build (first time clones OpenSpiel + abseil into build/_deps, ~30s)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run unit tests
RIFTBOUND_ROOT=. ./build/riftbound_tests

# Play random vs MCTS (single game, stdout + HTML replay)
./build/riftbound \
    --agent1 random --agent2 mcts:sims=50 \
    --deck1 decks/fiora.txt \
    --deck2 decks/yi_rebuild.txt \
    --render-html on

# Batch 100 games on 8 threads (random vs random, no UI). Add
# --render-html on to write replays/<timestamp>/replay_gameN.html.
./build/riftbound \
    --agent1 random --agent2 random \
    --deck1 decks/fiora.txt \
    --deck2 decks/yi_rebuild.txt \
    --games 100 --threads 8

# Play AS A HUMAN against MCTS-50 in your browser. The binary
# auto-starts a Boost.Beast webserver on http://127.0.0.1:8080
# whenever any seat is human; trace logging + HTML replay are also
# auto-enabled. Open the URL in any browser to play.
./build/riftbound \
    --agent1 human --agent2 mcts:sims=50 \
    --deck1 decks/fiora.txt \
    --deck2 decks/yi_rebuild.txt

# Want to play as P2 instead? Swap the seats:
./build/riftbound \
    --agent1 mcts:sims=50 --agent2 human \
    --deck1 decks/fiora.txt \
    --deck2 decks/yi_rebuild.txt

# Hot-seat (two humans in one browser tab, take turns):
./build/riftbound --agent1 human --agent2 human \
    --deck1 decks/fiora.txt \
    --deck2 decks/yi_rebuild.txt

# Spectator (force the web UI ON for an AI-vs-AI game):
./build/riftbound --agent1 random --agent2 mcts:sims=20 --web on \
    --deck1 decks/fiora.txt \
    --deck2 decks/yi_rebuild.txt

# Read one list. No second deck, no game. Prints the curve, the roles,
# and the chance of seeing each role and card in the opening and the
# first three turns. The sideboard is listed and left out of the chances.
./build/riftbound --deck-breakdown decks/fiora.txt

# Same command on a published tournament list:
./build/riftbound --deck-breakdown \
    internal/tournament-data/stuttgart-regional-championship-2026-11-06/001-asc-evansrhim.txt
```

Pass `--help` for the full flag list. The web UI's WebSocket protocol
is documented in [`docs/play-api.md`](docs/play-api.md).

## Read one deck

```bash
./build/riftbound --deck-breakdown decks/fiora.txt
```

The command loads that file and prints a report, then exits. `--deck1`
and `--deck2` are not required.

The report names the legend and the champion. The champion is classified
and is not given a draw chance: it starts in the Champion Zone. Battlefields
and runes are listed on their own. The sideboard is recorded and left out
of every chance. The curve counts the main deck and the champion by energy
cost. Late game is the highest energy cost in that list.

Roles (ramp, combat tricks, and the rest) are read from the printed text
and keywords the card already carries. Empty roles are omitted. Each role
says why those cards are in it and how many copies it holds.

Draw chances cover the opening hand and each of the first three turns.
With no draw, Predict, recycle-from-deck, or tutor on the main deck or the
champion, each chance is the plain hypergeometric draw. When the list has
one of those effects, a copy already seen counts its printed extra cards
once. An effect that appears only on those extra cards is not followed.
That is a count of the list, not a played game.

## What a game prints

A run of one game, and each game of a batch of 20 or fewer, ends with a
play-by-play. A larger batch keeps the progress line and does not print a
block per game. The block is not the trace (`--trace`) and not the decision
log.

The header names the game number, the seed, both deck-list names, both
agent specs, how the game ended, the final score, the turns, and the
decision count. A game that throws still prints the header, the error, and
that the game is not usable.

Each turn names whose turn it is and how many runes that player has at the
start and at the end. The mulligan names what each player shipped and what
they kept. A play names the card, the player, and what was paid. A free
play, and a play that ignores its cost, says so. A spell's target is named,
including a friendly unit. A move names the unit and both locations. A
combat names who fought and the damage. A conquer or a hold names the
battlefield and who controls it afterwards. The score is one line at the
end of the turn.

```bash
./build/riftbound \
    --deck1 internal/tournament-data/stuttgart-regional-championship-2026-11-06/001-asc-evansrhim.txt \
    --deck2 internal/tournament-data/orlando-showdown-2026-10-31/001-merc-koopastar.txt \
    --agent1 random-player:pass=0 --agent2 random-player:pass=0 \
    --seed 1 --games 1 --web off --render-html off
```

`random-player` chooses uniformly among the legal actions that are not a
pass, and passes with probability `P` when it could act (`pass=0.2` if you
omit it). `pass=0` plays whenever anything else is legal. `random` can pass
on any decision. `mcts:sims=N` searches the real position, including the
opponent's hand and both deck orders. `ismcts:sims=N` searches worlds drawn
from what the deciding player knows. Add `,world=perfect` to search the
real hidden cards instead. `alpha` is the honest search, worlds drawn
from what the seat knows, with the 50-simulation budget already filled in
(the same budget as `ismcts:sims=50`). `alpha:sims=N` changes the budget.
`alpha` plays one of the actions the engine offers.

## Play-by-play PDF

`scripts/play_by_play_pdf.py` is the play-by-play PDF exporter. `write_pdf`
is the layout. A later export calls it.

A turn is a card you can read at a glance. The header names whose turn it
is, the score, and the runes at the start and at the end. Under that, the
engine's lines stay in order, and type and space separate them. A play is
its own line, and the cost is marked, Accelerate included when it was
paid. A move is quieter. A conquer or a hold is a result band. A combat is
a box: who fought, then who took damage. A hide stays on its line. A
facedown play still says it was from face down. A unit still names the
player who controls it, in parentheses.

A line that names a card shows that card's picture from the card class,
beside the line: the card played and what it was played on, the unit that
moved, the battlefield conquered or held, the units in a combat, and the
cards in the mulligan. The pictures are not gathered into a gallery. A
picture that cannot be fetched is left out. The words stay.

```bash
./build/riftbound \
    --deck1 decks/fiora.txt --deck2 decks/yi_rebuild.txt \
    --agent1 random-player:pass=0 --agent2 random-player:pass=0 \
    --seed 1 --games 1 --web off --render-html off \
  | python3 scripts/play_by_play_pdf.py --output game.pdf
```

`python3 scripts/play_by_play_pdf.py --self-check` checks that a wrapped
line stays inside its turn, that a combat box does not run into the next
line, and that a card picture sits beside its line. It needs `reportlab`
and `pymupdf`, which are not engine dependencies.

## Tournament lists

`internal/tournament-data/` holds one file per deck from the completed
constructed events with a start date from the Radiance release (2026-10-23)
through 2026-11-19. Riftwatch is 12 events and 1,283 lists, pulled at 12:28
PM ET on November 19, 2026. RiftDecks adds the S5 Shanghai City Challenge,
64 lists, pulled at 2:54 PM ET the same day. Thirteen events, 1,347 lists.
The folder's `README.md` is the event table, the source URLs, and where the
published top-cut scores are. Shanghai does not publish who beat whom.

A path works as `--deck1`, `--deck2`, or `--deck-breakdown`. `matchups.txt`
is the published top cut for Stuttgart and Orlando. It is not a deck file.
Do not pass it as one.

A header comment names the event, the date, the place, the player, the
record, and the source URL. The November 23, 2026 bans are still in the
list, and the header names each one that file contains. On and after that
date a normal game rejects those cards. `--wild` plays the list anyway.
`--deck-breakdown` does not apply the ban check.

`Master Yi, Wuju Bladesman` is the engine name for the Proving Grounds
starter. `Master Yi, Wuju Master` is the Unleashed legend. A line written
`Pouty Poro #776` is a later printing. A header that says a promo code is
not in the engine kept the card under the name the loader already has.

## Play against the AI (web UI)

The fastest way to play a game yourself, as Player 1, against a
50-simulation MCTS bot:

```bash
./build/riftbound \
    --agent1 human --agent2 mcts:sims=50 \
    --deck1 decks/fiora.txt \
    --deck2 decks/yi_rebuild.txt
```

To seat the default honest-search opponent instead, pass `alpha`. That is
50 simulations in worlds drawn from what the seat knows, the same budget as
`ismcts:sims=50`. Any deck file works as `--deck1` or `--deck2`:

```bash
./build/riftbound \
    --agent1 human --agent2 alpha \
    --deck1 decks/fiora.txt \
    --deck2 decks/yi_rebuild.txt
```

Because a seat is `human`, the binary auto-starts a small Boost.Beast
web server and prints a URL:

```
  → Open http://127.0.0.1:8080 in your browser to play.
```

Open that in a browser and play. The game blocks at each of your
decision points until you click an action; the MCTS opponent moves on
its own. (MCTS cost scales with `sims` — `sims=50` is snappy; `sims=500`
can take minutes *per decision*, so start low.)

Useful flags:

| Flag | Default | Meaning |
|---|---|---|
| `--deck-breakdown` | | Read one deck file, print its curve, roles, and draw chances, then exit. |
| `--agent1` / `--agent2` | `random` | Per-seat agent: `human`, `random`, `random-player[:pass=P]`, `mcts:sims=N`, `ismcts:sims=N[,world=sampled\|perfect]`, `alpha[:sims=N][,world=sampled\|perfect]` (default 50 simulations, honest worlds). |
| `--game-mode` | `duel` | `duel` draws each setup battlefield. `match` has each player choose theirs. |
| `--web` | `auto` | `auto` starts the UI whenever a seat is human; `on` forces it (spectate AI-vs-AI); `off` disables it. |
| `--port` / `--bind` | `8080` / `127.0.0.1` | Where the UI listens. Use `--bind 0.0.0.0` to reach it from another machine: the printed URL then carries an access token that the server requires. |
| `--god-mode` | `auto` | `auto`/`on` enables the in-browser state editor when a human is seated. While it is on the UI shows every hand and deck. `off` hides the editor **and** the opponent's hand and both decks' order, for a fair game. |
| `--seed` | `0` | Fix the RNG for a reproducible game (`0` = random per game). |
| `--wild` | off | Wild format: allow banned cards. Skips `banned` and a `banned_from` date that has arrived. |

### What the UI looks like

The page is a single resizable dashboard (drag the dividers between
panels to resize):

- **Header** — engine version + build, the game seed, and a live
  connection indicator.
- **Board** (left) — the live ASCII board state, with a status line at
  the top showing turn number, phase, both scores (P1 green / P2 red),
  and whose decision it is.
- **Legal Actions** (top-right) — your legal moves as clickable buttons.
  **Click one to take it.** When the engine is resolving or the MCTS
  opponent is thinking, the buttons grey out and show *"engine is
  processing…"* so you can't double-act. Mid-resolution choices (e.g.
  "Discard a card", "Choose one —") show a prompt header plus
  human-readable option labels (`Yes`, `No`, a mode name, an `X` value)
  rather than bare indices.
- **Recent Actions** — a calm, high-signal feed of what each side just
  did (plays, kills, scores) so you can follow the opponent without
  reading the firehose.
- **Trace** — the full event log (every phase, decision, effect, damage,
  draw, etc.). Verbose; the Recent Actions feed is the readable summary.
- **Card Details** (right) — **hover any card name** anywhere in the UI
  to load that card's art (fetched on demand from Riot's CDN).
- **God Mode** (only when `--god-mode` is on) — a state editor to move
  cards between zones, set might/damage/exhaustion, edit scores/energy,
  reorder decks, and change phase. Handy for setting up specific
  scenarios or debugging.

Other modes: `--agent1 mcts:sims=50 --agent2 human` to play as P2,
`--agent1 human --agent2 human` for hot-seat (two players, one tab,
taking turns), and `--web on` with two AI seats to spectate. The
underlying WebSocket protocol is in [`docs/play-api.md`](docs/play-api.md).

## Binaries

| Binary | Purpose |
|---|---|
| `build/riftbound` | Unified game runner. Single game, batch, web UI, spectator, `--deck-breakdown`, agents (`random` / `random-player` / `human` / `mcts:sims=N` / `ismcts:sims=N` / `alpha`), HTML replay, play-by-play, trace/debug logging — all driven by CLI flags. |
| `build/riftbound_tests` | Google Test suite. |
| `build/src/openspiel/riftbound_clone_equiv_test` | Validates Clone() correctness — clones mid-game, runs the same actions on original + clone, asserts identical terminal state. |
| `build/src/openspiel/riftbound_clone_microbench` | Microbenchmark — times Clone() throughput. |
| `build/src/openspiel/riftbound_parity_baseline` | Diagnostic — confirms OpenSpiel wrapper produces statistically identical results to the raw engine. |

## Plug in your own agent

Subclass `riftbound::AgentInterface` (see `src/agents/agent_interface.h`)
and add a branch to `buildAgent()` in `src/main.cpp` — adding a new
`--agent foo:...` spec is ~30 lines. The `MctsAgent` / `IsMctsAgent`
adapters in `src/agents/mcts_agent.{h,cpp}` show how to wrap an
OpenSpiel `Bot` behind the same interface.

For learned agents: a sibling repo holds the training pipeline + model
architectures (Deep CFR, OSFP, online CFR / ReBeL inference, etc.).
That repo links against this one as a static library — engine repo
stays slim and dependency-light.

## Card data

Card data is **hand-authored C++** — the source files are the single source of
truth. There is no `registry.json` and no code-generation pipeline. Each card
class implements `Card::def()`, returning its `CardDef` (cost, domains, tags,
keywords, art URL, `banned` flag, …); the engine builds its card table from those
at startup (`CardDB::buildFromClasses`). To add or edit a card, edit its
`src/cards/<type>/<id>_<slug>.cpp` directly.

```bash
# Export the compiled card table to registry-style JSON (validation / external use)
./build/riftbound --dump-registry /tmp/registry.json

# (optional) Pull raw card metadata from the official gallery, for reference when
# authoring new cards. Writes cards/raw/gallery_raw.json — it does NOT feed the engine.
python3 scripts/fetch_cards.py
```

| Script | Purpose |
|---|---|
| `scripts/fetch_cards.py` | Scrape Riftbound's official gallery into `cards/raw/gallery_raw.json`. Reference data for authoring new cards; not consumed by the engine. |
| `scripts/card_coverage.py` | Classify every `src/cards/<type>/<id>_<slug>.cpp` (implemented / metadata-only / stub / vanilla) and its in-code gap comments; exits non-zero while any card is a stub. `--write-lists` refreshes `docs/card-coverage/`. |
| `scripts/generate_replays.py` | Run `build/riftbound --render-html on` once per deck pair under `decks/` (the binary plays one pair per run) and file the HTML replays per pair. Visual V&V after card changes. |
| `scripts/rules_to_md.py` | Regenerate `rules/core-rules.md` from the rules PDF. |
| `scripts/play_by_play_pdf.py` | The play-by-play PDF exporter. `write_pdf` is the layout: whose turn, the score, the runes, the marked cost, the combat, and a conquer or a hold, with each card's picture beside its line. |

## Repository layout

```
src/core/        ID types, events, intents, game state, card database
src/engine/      Turn loop, chain manager, effect executor, trigger manager,
                 batch runner, step driver (fiber-based)
src/cards/       Card subclasses — one TU per card under units/ spells/ gear/
                 legends/ battlefields/ runes/ (named <id>_<slug>.cpp), each
                 owning its CardDef data + behavior. Per-type bases live in
                 their type dir (gear/equip_base.h, units/weaponmaster_base.h);
                 cross-cutting helpers in card_helpers.h; cards_init.cpp is the
                 checked-in registration aggregator (one line per card).
src/agents/      AgentInterface + RandomAgent + HumanAgent
src/io/          HTML replay writer, play-by-play, ASCII state renderer,
                 play server (web UI) and its god-mode state editor
src/openspiel/   OpenSpiel Game / State subclasses, action vocabulary,
                 clone-correctness tests
src/ml/          Engine-side shared infra: feature extractor +
                 CFR utilities. No torch — these emit data for
                 downstream ML consumers.
src/rules/       Deck validator, deck-breakdown report, tournament rules
src/effects/     Effect type definitions

tests/           Google Test — engine behavior + card mechanics
cards/           raw/gallery_raw.json (reference art/metadata for authoring)
decks/           Sample decks (.txt deck lists)
internal/tournament-data/   One file per deck from completed Radiance events
rules/           core-rules.md, tournament-rules.md, core-rules.pdf
errata/          Official errata documents
scripts/         Card gallery fetch, card coverage report, batch replay
                 generation, rules PDF -> Markdown
```

## Reference

- `rules/core-rules.md` — full game rules (2026-10-09 edition, sections 000–832)
- `rules/tournament-rules.md` — deck construction + tournament policies
- `CLAUDE.md` — coding standards + architecture conventions for contributors

## Status

This is a working development snapshot. The engine, OpenSpiel wrapper, and
test suite are stable. Card coverage is improving steadily — see
`scripts/card_coverage.py` output for per-card implementation status.

## License

TBD. Card data fetched from public Riot sources; card art is *not* bundled
(images are loaded on-demand from Riot's CDN by replay viewers / web UIs).
Riftbound is the property of Riot Games — this engine reproduces published
public rules for research and educational use.
