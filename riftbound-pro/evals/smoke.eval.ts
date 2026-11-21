import { defineEval } from "@grok/agent-sdk/evals"

const shortList = `Is this constructed deck legal? Pass the list to check-deck unchanged.

Legend:
1 Grandmaster at Arms

Champion:
1 Jax, Unmatched

MainDeck:
3 Steel Paws

Battlefields:
1 Hunters' Circle
1 Firelights' Hideout
1 Startipped Peak

Runes:
6 Calm Rune
6 Body Rune
`

export default defineEval({
  tags: ["smoke"],
  description: "A short constructed list is checked with check-deck before it is called legal.",
  async test(t) {
    await t.send(shortList)
    t.succeeded()
    t.calledTool("check-deck", {
      output: (value) => {
        if (!value || typeof value !== "object") return false
        const result = value as { legal?: boolean; errors?: string[] }
        return result.legal === false && (result.errors ?? []).some((error) => error.includes("exactly 40"))
      },
    })
  },
})
