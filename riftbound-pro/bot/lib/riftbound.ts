// Constructed-deck checks and card lookup over a dump of the engine CardDB.
// Name indexing follows CardDB::buildFromClasses (src/core/card_db.cpp),
// including legend aliases. Legality follows DeckValidator::validate
// (src/rules/deck_validator.cpp). A deck list follows loadFromDeckList.

import { readFileSync } from "node:fs"
import { fileURLToPath } from "node:url"

export type Card = {
  id: number
  defId: string
  name: string
  type: string
  super: string
  domains: string[]
  tags: string[]
  championTag: string
  energy: number
  power: number
  might: number
  keywords: string[]
  deflect: number
  assault: number
  shield: number
  ability: string
  effect: string
  banned: boolean
  bannedFrom: number
  unlimited: boolean
  token: boolean
}

type Catalog = {
  version: number
  source: string
  numCards: number
  cards: Card[]
  names: Record<string, number[]>
}

export type CardView = {
  id: number
  defId: string
  name: string
  type: string
  super: string
  domains: string[]
  tags: string[]
  championTag: string
  energy: number
  power: number
  might: number
  keywords: string[]
  deflect: number
  assault: number
  shield: number
  ability: string
  effect: string
  banned: boolean
  bannedFrom: number
  unlimited: boolean
  token: boolean
}

export type LookupResult = {
  found: boolean
  query: string
  matches: CardView[]
  /** The id a deck list uses when the line has no " #id". */
  deckListUses: number | null
  error?: string
}

export type DeckLine = {
  section: string
  count: number
  id: number
  name: string
}

export type DeckCheck = {
  parsed: boolean
  legal: boolean
  errors: string[]
  legalityDate: number
  wild: boolean
  printingNotes: string[]
  identity: {
    legend: string
    legendId: number
    domains: string[]
    championTag: string
    champion: string
    championId: number
  } | null
  curve: {
    byType: Record<string, number>
    byEnergy: Record<string, number>
    deflectUnits: number
  } | null
  lines: DeckLine[] | null
}

const NEEKO_ID = 1084
const MAIN_DECK_SIZE = 40
const RUNE_DECK_SIZE = 12
const BATTLEFIELD_COUNT = 3
const MAX_SIDEBOARD = 10
const MAX_COPIES = 3
const MAX_SIGNATURES = 3

let catalogCache: { byId: Map<number, Card>; names: Record<string, number[]>; source: string; numCards: number } | null = null

function lower(value: string): string {
  let out = ""
  for (let i = 0; i < value.length; i++) {
    const code = value.charCodeAt(i)
    out += code >= 65 && code <= 90 ? String.fromCharCode(code + 32) : value[i]
  }
  return out
}

function catalog() {
  if (catalogCache) return catalogCache
  const path = fileURLToPath(new URL("./catalog.json", import.meta.url))
  const parsed = JSON.parse(readFileSync(path, "utf8")) as Catalog
  const byId = new Map<number, Card>()
  for (const card of parsed.cards) byId.set(card.id, card)
  catalogCache = { byId, names: parsed.names, source: parsed.source, numCards: parsed.numCards }
  return catalogCache
}

export function catalogSource(): { source: string; numCards: number } {
  const data = catalog()
  return { source: data.source, numCards: data.numCards }
}

function view(card: Card): CardView {
  return {
    id: card.id,
    defId: card.defId,
    name: card.name,
    type: card.type,
    super: card.super,
    domains: card.domains,
    tags: card.tags,
    championTag: card.type === "Legend" ? legendChampionTag(card) : card.championTag,
    energy: card.energy,
    power: card.power,
    might: card.might,
    keywords: card.keywords,
    deflect: card.deflect,
    assault: card.assault,
    shield: card.shield,
    ability: card.ability,
    effect: card.effect,
    banned: card.banned,
    bannedFrom: card.bannedFrom,
    unlimited: card.unlimited,
    token: card.token,
  }
}

function legendChampionTag(card: Card): string {
  if (card.championTag) return card.championTag
  return card.tags.length === 1 ? card.tags[0] : ""
}

function bannedOn(card: Card, date: number): boolean {
  return card.banned || (card.bannedFrom !== 0 && date >= card.bannedFrom)
}

function utcDate(now = new Date()): number {
  return now.getUTCFullYear() * 10000 + (now.getUTCMonth() + 1) * 100 + now.getUTCDate()
}

type Resolved = { id: number } | { error: string }

function resolveEntry(entry: string): Resolved {
  const data = catalog()
  let name = entry
  let printing: number | null = null
  const hash = entry.lastIndexOf(" #")
  if (hash !== -1 && hash + 2 < entry.length && /^\d+$/.test(entry.slice(hash + 2))) {
    printing = Number(entry.slice(hash + 2))
    name = entry.slice(0, hash).trim()
  }
  const ids = data.names[lower(name)] ?? []
  if (printing !== null) {
    if (!ids.includes(printing)) return { error: `no card '${name}' with id ${printing}` }
    return { id: printing }
  }
  if (ids.length === 0) return { error: `unknown card '${name}'` }
  return { id: ids[0] }
}

export function lookupCard(query: string): LookupResult {
  const trimmed = query.trim()
  if (/^\d+$/.test(trimmed)) {
    const card = catalog().byId.get(Number(trimmed))
    if (!card) return { found: false, query: trimmed, matches: [], deckListUses: null }
    return { found: true, query: trimmed, matches: [view(card)], deckListUses: card.id }
  }
  const resolved = resolveEntry(trimmed)
  if ("error" in resolved) {
    return { found: false, query: trimmed, matches: [], deckListUses: null, error: resolved.error }
  }
  const name = trimmed.includes(" #") ? trimmed.slice(0, trimmed.lastIndexOf(" #")).trim() : trimmed
  const ids = catalog().names[lower(name)] ?? [resolved.id]
  const matches: CardView[] = []
  for (const id of ids) {
    const card = catalog().byId.get(id)
    if (card) matches.push(view(card))
  }
  return { found: matches.length > 0, query: trimmed, matches, deckListUses: ids[0] ?? null }
}

type ParsedDeck = {
  legend: number
  champion: number
  main: number[]
  battlefields: number[]
  runes: number[]
  sideboard: number[]
  neeko: number[]
  notes: string[]
  lines: DeckLine[]
}

function parseDeckList(list: string): { deck: ParsedDeck } | { error: string } {
  const data = catalog()
  const deck: ParsedDeck = {
    legend: -1,
    champion: -1,
    main: [],
    battlefields: [],
    runes: [],
    sideboard: [],
    neeko: [],
    notes: [],
    lines: [],
  }
  let section = ""
  const rows = list.split(/\n/)
  for (let i = 0; i < rows.length; i++) {
    const lineNo = i + 1
    const line = rows[i].trim()
    if (!line || line.startsWith("#")) continue
    if (line.endsWith(":")) {
      section = lower(line.slice(0, -1).trim())
      continue
    }
    if (!section) return { error: `Deck list line ${lineNo}: card line before any section header` }
    const countMatch = /^(\d+)\s+([\s\S]+)$/.exec(line)
    if (!countMatch) return { error: `Deck list line ${lineNo}: expected '<count> <card name>'` }
    const count = Number(countMatch[1])
    if (!Number.isInteger(count) || count <= 0) {
      return { error: `Deck list line ${lineNo}: expected '<count> <card name>'` }
    }
    const entry = countMatch[2].trim()
    if (!entry) return { error: `Deck list line ${lineNo}: missing card name` }
    const resolved = resolveEntry(entry)
    if ("error" in resolved) return { error: `Deck list line ${lineNo}: ${resolved.error}` }
    const card = data.byId.get(resolved.id)
    if (!card) return { error: `Deck list line ${lineNo}: unknown card '${entry}'` }

    const bare = entry.includes(" #") ? entry.slice(0, entry.lastIndexOf(" #")).trim() : entry
    const printings = data.names[lower(bare)] ?? []
    if (printings.length > 1 && !entry.includes(" #")) {
      deck.notes.push(
        `${card.name} resolves to #${card.id}. Other printings: ${printings.filter((id) => id !== card.id).map((id) => `#${id}`).join(", ")}.`,
      )
    }

    const push = (bucket: number[]) => {
      for (let n = 0; n < count; n++) bucket.push(card.id)
      deck.lines.push({ section, count, id: card.id, name: card.name })
    }

    if (section === "legend") {
      if (deck.legend !== -1) return { error: `Deck list line ${lineNo}: multiple legends` }
      deck.legend = card.id
      deck.lines.push({ section, count, id: card.id, name: card.name })
    } else if (section === "champion") {
      if (deck.champion !== -1) return { error: `Deck list line ${lineNo}: multiple champions` }
      deck.champion = card.id
      deck.lines.push({ section, count, id: card.id, name: card.name })
    } else if (section === "maindeck") push(deck.main)
    else if (section === "battlefields") push(deck.battlefields)
    else if (section === "runes") push(deck.runes)
    else if (section === "sideboard") push(deck.sideboard)
    else if (section === "neeko") push(deck.neeko)
    else return { error: `Deck list line ${lineNo}: unknown section '${section}'` }
  }
  if (deck.legend === -1) return { error: "Deck list: no Legend section" }
  if (deck.champion === -1) return { error: "Deck list: no Champion section" }
  return { deck }
}

function must(id: number): Card {
  const card = catalog().byId.get(id)
  if (!card) throw new Error(`catalog is missing card id ${id}`)
  return card
}

export function checkDeck(list: string, options?: { legalityDate?: number; wild?: boolean }): DeckCheck {
  const date = options?.legalityDate ?? utcDate()
  const wild = options?.wild ?? false
  const parsed = parseDeckList(list)
  if ("error" in parsed) {
    return {
      parsed: false,
      legal: false,
      errors: [parsed.error],
      legalityDate: date,
      wild,
      printingNotes: [],
      identity: null,
      curve: null,
      lines: null,
    }
  }
  const deck = parsed.deck
  const errors: string[] = []
  const legend = must(deck.legend)
  if (legend.type !== "Legend") {
    return {
      parsed: true,
      legal: false,
      errors: [`Legend slot contains non-legend card: ${legend.name}`],
      legalityDate: date,
      wild,
      printingNotes: [...new Set(deck.notes)],
      identity: null,
      curve: null,
      lines: deck.lines,
    }
  }

  const champion = must(deck.champion)
  if (champion.type !== "Unit") errors.push(`Champion slot contains non-unit: ${champion.name}`)
  else if (champion.super !== "Champion") errors.push(`Champion slot contains non-champion unit: ${champion.name}`)
  else {
    const tag = legendChampionTag(legend)
    if (!tag) errors.push(`Legend '${legend.name}' has no single champion tag`)
    else if (!champion.tags.includes(tag)) {
      errors.push(
        `Chosen champion '${champion.name}' doesn't share a champion tag with legend '${legend.name}'`,
      )
    }
  }

  const total = deck.main.length + 1
  if (total !== MAIN_DECK_SIZE) {
    errors.push(`Main deck must be exactly 40 cards (including champion), got ${total}`)
  }

  const checkMainType = (id: number, source: string) => {
    const card = must(id)
    const mainType = card.type === "Unit" || card.type === "Spell" || card.type === "Gear"
    if (card.token) errors.push(`${source} contains token card: ${card.name}`)
    else if (!mainType) errors.push(`${source} contains ${card.type} card: ${card.name}`)
  }
  for (const id of deck.main) checkMainType(id, "Main deck")
  for (const id of deck.sideboard) checkMainType(id, "Sideboard")

  if (deck.runes.length !== RUNE_DECK_SIZE) {
    errors.push(`Rune deck must be exactly 12 runes, got ${deck.runes.length}`)
  }
  if (deck.battlefields.length !== BATTLEFIELD_COUNT) {
    errors.push(`Must have exactly 3 battlefields, got ${deck.battlefields.length}`)
  }
  const battlefieldNames = new Set<string>()
  for (const id of deck.battlefields) {
    const card = must(id)
    if (card.type !== "Battlefield") errors.push(`Battlefield slot contains non-battlefield: ${card.name}`)
    else if (card.token) errors.push(`Battlefield slot contains token battlefield: ${card.name}`)
    if (battlefieldNames.has(card.name)) errors.push(`Duplicate battlefield name: ${card.name}`)
    battlefieldNames.add(card.name)
  }
  if (deck.sideboard.length > MAX_SIDEBOARD) {
    errors.push(`Sideboard can have at most ${MAX_SIDEBOARD} cards, got ${deck.sideboard.length}`)
  }

  const legendDomains = new Set(legend.domains)
  const checkDomains = (id: number, source: string) => {
    const card = must(id)
    if (card.domains.length === 0) return
    for (const domain of card.domains) {
      if (!legendDomains.has(domain)) {
        errors.push(`${source} card '${card.name}' has domain ${domain} not in legend's domain identity`)
        return
      }
    }
  }
  for (const id of deck.main) checkDomains(id, "Main deck")
  checkDomains(deck.champion, "Champion")
  for (const id of deck.sideboard) checkDomains(id, "Sideboard")

  for (const id of deck.runes) {
    const card = must(id)
    if (card.type !== "Rune") {
      errors.push(`Rune deck contains non-rune: ${card.name}`)
      continue
    }
    for (const domain of card.domains) {
      if (!legendDomains.has(domain)) {
        errors.push(`Rune '${card.name}' has domain ${domain} not in legend's domain identity`)
      }
    }
  }

  const nameCounts = new Map<string, number>()
  const uniqueNames = new Set<string>()
  const countCard = (id: number) => {
    const card = must(id)
    if (card.unlimited) return
    nameCounts.set(card.name, (nameCounts.get(card.name) ?? 0) + 1)
    if (card.keywords.includes("Unique")) uniqueNames.add(card.name)
  }
  for (const id of deck.main) countCard(id)
  countCard(deck.champion)
  for (const id of deck.sideboard) countCard(id)
  for (const [name, count] of nameCounts) {
    if (uniqueNames.has(name)) {
      if (count > 1) {
        errors.push(`Too many copies of [Unique] card '${name}': ${count} (max 1 across main + sideboard)`)
      }
      continue
    }
    if (count > MAX_COPIES) {
      errors.push(`Too many copies of '${name}': ${count} (max 3 across main + sideboard)`)
    }
  }

  let signatures = 0
  const tag = legendChampionTag(legend)
  const checkSig = (id: number) => {
    const card = must(id)
    if (card.super !== "Signature") return
    signatures += 1
    if (!tag || !card.tags.includes(tag)) {
      errors.push(`Signature card '${card.name}' doesn't match legend's champion tag`)
    }
  }
  for (const id of deck.main) checkSig(id)
  checkSig(deck.champion)
  for (const id of deck.sideboard) checkSig(id)
  if (signatures > MAX_SIGNATURES) {
    errors.push(`Too many signature cards: ${signatures} (max 3)`)
  }

  if (!wild) {
    const checkBan = (id: number, zone: string) => {
      const card = must(id)
      if (bannedOn(card, date)) errors.push(`Banned card in ${zone}: ${card.name}`)
    }
    checkBan(deck.legend, "legend")
    checkBan(deck.champion, "champion")
    for (const id of deck.main) checkBan(id, "main deck")
    for (const id of deck.runes) checkBan(id, "rune deck")
    for (const id of deck.battlefields) checkBan(id, "battlefields")
    for (const id of deck.sideboard) checkBan(id, "sideboard")
  }

  const hasNeeko = deck.champion === NEEKO_ID || deck.main.includes(NEEKO_ID) || deck.sideboard.includes(NEEKO_ID)
  if (!hasNeeko) {
    if (deck.neeko.length > 0) errors.push("Neeko legends are only legal when Neeko, Blending In is in the deck")
  } else {
    if (deck.neeko.length !== 3) {
      errors.push(`Neeko chooses exactly 3 additional legends, got ${deck.neeko.length}`)
    }
    const seen = new Set<string>()
    for (const id of deck.neeko) {
      const card = must(id)
      if (card.type !== "Legend") errors.push(`Neeko legend is not a legend: ${card.name}`)
      if (card.name === legend.name) errors.push(`Neeko legend matches the starting legend: ${card.name}`)
      if (seen.has(card.name)) errors.push(`Neeko legends must be different: ${card.name}`)
      seen.add(card.name)
    }
  }

  const byType: Record<string, number> = {}
  const byEnergy: Record<string, number> = {}
  let deflectUnits = 0
  const tally = (id: number) => {
    const card = must(id)
    byType[card.type] = (byType[card.type] ?? 0) + 1
    const energy = String(card.energy)
    byEnergy[energy] = (byEnergy[energy] ?? 0) + 1
    if (card.type === "Unit" && card.keywords.includes("Deflect")) deflectUnits += 1
  }
  for (const id of deck.main) tally(id)
  tally(deck.champion)

  return {
    parsed: true,
    legal: errors.length === 0,
    errors,
    legalityDate: date,
    wild,
    printingNotes: [...new Set(deck.notes)],
    identity: {
      legend: legend.name,
      legendId: legend.id,
      domains: legend.domains,
      championTag: legendChampionTag(legend),
      champion: champion.name,
      championId: champion.id,
    },
    curve: { byType, byEnergy, deflectUnits },
    lines: deck.lines,
  }
}
