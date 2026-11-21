import { defineTool } from "@grok/agent-sdk/tools"
import * as z from "zod/v4"
import { lookupCard } from "../lib/riftbound.js"

export default defineTool({
  description:
    "Look up one printed card by name or numeric id in the engine card table. Use this before quoting a card's cost, keywords, or text. Returns the printed card, or found:false when the name is not in the table.",
  effect: "read",
  inputSchema: z.object({
    query: z.string().describe("Card name, 'Name #id' for one printing, or a numeric card id"),
  }),
  execute({ query }) {
    return lookupCard(query)
  },
})
