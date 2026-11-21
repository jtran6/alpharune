---
description: Use when the user asks to build, cut, or check a Riftbound constructed deck.
---

# Build a deck

Edit this procedure when you want a different default.

1. Call `check-deck` on the list the user gave before you call it legal or illegal. Pass the list unchanged.
2. Quote the tool's `errors` when it returns any. Do not restate a size, a domain, a copy limit, or a ban that the tool did not report.
3. Call `lookup-card` before you add, cut, or describe a card. A card with `found: false` is not in the table. Do not put it in a list.
4. Read curve talk from `curve.byEnergy`, `curve.byType`, and `curve.deflectUnits`. Do not count the curve yourself.
5. The legend's `identity.domains` are the domain identity. Do not add a card outside them.
6. After a change, call `check-deck` on the full revised list. Call the list legal only when that result has `legal: true`.
7. A shared name with no ` #id` uses the printing in `printingNotes` and in `deckListUses`. Name the id when more than one printing exists.

Reply in this shape:

- Legal: true or false, then the tool's errors
- Identity: legend, champion tag, domains
- Curve: the tool's energy counts, then the slot you would change
- Draft: the revised list, or "no change" when the current list stays
