import { defineTool } from "@grok/agent-sdk/tools"
import * as z from "zod/v4"
import { checkDeck } from "../lib/riftbound.js"

export default defineTool({
  description:
    "Check a constructed deck list against the engine deck rules. Pass the list unchanged. Sections are Legend, Champion, MainDeck, Battlefields, Runes, and optional Sideboard. Returns legal, errors, the legend's domains, and curve counts from the card table.",
  effect: "read",
  inputSchema: z.object({
    list: z.string().describe("The deck list text, with section headers ending in a colon"),
    legalityDate: z
      .number()
      .int()
      .optional()
      .describe("YYYYMMDD. Omit for today's UTC date. A scheduled ban starts on its date."),
    wild: z.boolean().optional().describe("When true, banned cards are allowed"),
  }),
  execute({ list, legalityDate, wild }) {
    return checkDeck(list, { legalityDate, wild })
  },
})
