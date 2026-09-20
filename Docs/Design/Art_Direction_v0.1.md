# Art direction and the art pipeline

**Version:** 0.1
**Date:** 2026-09-20
**Supersedes:** `Sheet_Generation_Prompt_v0.1.md`, which prompted for a whole sheet with its text baked in. [`Vanguards/render_sheet.py`](Vanguards/render_sheet.py) now owns the text, so only the artwork needs generating.

## Where the prompts are

**Not in this file.** They are generated, so they cannot go stale:

```
python3 Docs/Design/Vanguards/render_sheet.py --prompts bryn
python3 Docs/Design/Vanguards/render_sheet.py --prompts --all
python3 Docs/Design/Vanguards/render_sheet.py --missing
```

Each prompt is emitted with that Vanguard's appearance paragraph, signature colour, region and antagonist direction already injected from the Character Bible and their YAML. A rename or a rewrite changes the prompts automatically. A prompt pasted into a document would not.

`--missing` lists the art each Vanguard still needs, as a work order.

## House style

**Painterly realism with one saturated signature hue** — the rendering of Patch's sheet with the colour discipline of Tavi's.

Painterly and heavily rendered; not photoreal, not flat cel, not anime. Materials read physically. Every image for a Vanguard is built around **one saturated signature colour** that drives the key light and the accents, with everything else desaturated so that hue carries the frame. Strong directional key plus an opposing rim, deep shadows, high contrast, mid-to-dark value key. Silhouette readable at thumbnail size — this is a MOBA, and the shape identifies the character before the detail does.

Never in the artwork: text, lettering, logos, UI, watermarks, signatures, lens flare, stat bars, or numbers of any kind. Tuning belongs in data per `ARCHITECTURE.md` §1.3, and sheets that printed `Tracked for 4 seconds` into their pixels are why this is written down.

### Signature colours

Assigned by region so no two sheets collide in a draft grid. Held in `HUE` in `render_sheet.py` and injected into every prompt.

| Region | Hue | | Region | Hue |
|---|---|---|---|---|
| Iron March | `#c4552b` | | Ember Basin | `#ff7a1a` |
| Drowned Cantons | `#2e8b9e` | | Bramblehollow | `#7fa650` |
| Merrin | `#c8536f` | | Wildwood | `#a8c64a` |
| Buried Riverlands | `#c19a4b` | | Reed Provinces | `#c0392b` |
| Shatterdeep | `#6fc3d4` | | Flux Network / Fluxborn | `#4a8fd4` |

## Workflow

**Generate `hero` first, approve one variation, then use that image as a reference for the other twelve.** This is the whole trick to consistency. Without a reference the twelve slots produce twelve different people wearing similar clothes.

Use a text-to-image model for the hero and a reference-driven model for everything after it. Save results to `ConceptArt/Vanguards/<id>/<stem>.png` and re-render the sheet; slots fill automatically when the file appears, and show a labelled work order when it does not.

## The thirteen slots

| Stem | Aspect | Purpose |
|---|---|---|
| `hero` | 3:4 | Full-figure illustration, signature key light, region behind the subject |
| `front` `back` `side` | 1:2 | Orthographic A-pose turnaround, flat even light, plain background — modelling reference, not illustration |
| `scale` | 1:2 | Flat silhouette beside a 1.8 m human silhouette |
| `idle` `move` `cast` `ult` | 1:1 | Three-quarter top-down gameplay camera, silhouette readability first |
| `portrait` `weapon` `material` `signature` | 1:1 | Close crops |

**That is 325 images for the full roster.** If that is too much, the tiering that keeps the sheets useful is: `hero` alone makes a sheet presentable; `hero` + turnaround makes it buildable; the in-game views matter most for characters whose readability is in question; the detail crops are the first thing to cut.

## Before generating anything — a blocker

**Resolved on 2026-09-20 for 22 of 25 Vanguards; three remain.**

The original blocker was that only 8 of 25 Vanguards carried an appearance paragraph in the Character Bible, so for the other seventeen the only record of how the character looked was the concept sheet itself. That is the same failure as text baked into an image, in a more expensive form: **the design exists only as pixels, and nothing can validate, diff or correct it.**

**Fourteen of those seventeen have now been written up** — Silt, Relay, Patch, Tavi, Vera, Korruk, Torr, Marek, Neris, Moro, Qazharr, Varkesh, Sylra and Mavra each carry a `**Visual language:**` paragraph in the Character Bible, read off their surviving sheet before regeneration so the design survives the art it came from. Their prompts now emit a real subject description.

**Three do not, by decision: Raska, Kade and Angeru.** Their sheets were withdrawn for third-party content, so reconstructing the old look would carry forward designs developed against third-party reference material — the reason the sheets were withdrawn in the first place. A clean redesign is the safer path and the honest one. Their appearance is currently recorded nowhere, and `--prompts` says so rather than inventing a look.

So: **22 Vanguards are ready to generate. Raska, Kade and Angeru need a design decision first**, not a prompt.

Each paragraph follows the same three-part shape, and new ones should:

1. **Physical description** — build, materials, palette, silhouette-defining gear, and the scale if it is unusual.
2. **A readability note** — what has to stay legible at gameplay camera distance, and which visual states the character carries.
3. **An anti-drift guardrail**, phrased as *is X, **not** Y* — the specific wrong reading a generator or a new artist will reach for. This is the part that does the work; without it the prompt drifts toward the nearest genre cliché.

Guardrails record what the design is **not**, which is the half no image can express. They are also why the paragraphs must never carry tuning values: an appearance paragraph is art direction, and gameplay numbers live in their owning data files.

## Model choice is load-bearing

Established by the first generation run, 2026-09-20.

**The guardrails are the part a model is most likely to invert.** Both candidate
families say so in their own prompting guides:

- **FLUX** (`flux-2-pro`): *"No negative prompts. FLUX does not support them."* Its sweet
  spot is 30–80 words, and later tokens receive less attention weight.
- **Nano Banana** (`gemini-3-pro-image`): lists *"heavy use of negative phrasing"* under
  what to avoid, but handles a narrative paragraph well — *"a narrative paragraph always
  outperforms disconnected tags."*

Our appearance paragraphs run 180–270 words and end in an *is X, **not** Y* guardrail.
That form is correct for a human art director and wrong for FLUX, which would read
"not a mud golem with a skull for a head" as a request for one.

**So `gemini-3-pro-image` is the house model**, for three reasons: it tolerates the
negation the guardrails are built on, it rewards the long narrative prompts we already
generate, and it is the model built for *holding a character consistent across shots* —
which is the entire premise of the hero-first workflow.

The canon does not bend to suit a model. `render_sheet.py` does the conversion instead:
it splits each appearance paragraph into affirmative description and guardrails, leads
with the description, and collects the guardrails into one short delimited `Critically:`
clause at the end. A single delimited constraint is what the guides tolerate; negation
scattered through the prompt is what they warn about.

### Aspect ratio does not come from the prompt

Asking for `3:4` in prompt text produced `16:9`. The Nano Banana guide is explicit that
*"aspect ratios can be unreliable through prompting alone."* Aspect is a **node
parameter**, not prose: read the model's schema with `creative_get_model_schema`, then set
it with `creative_update_node`. This matters most for the turnaround slots, which are
specified `1:2` and will otherwise silently come back as landscape.

### Cost, measured

`gemini-3-pro-image` at 1K resolution bills about **12¢ per image**, so a four-variation
slot is roughly **49¢**. That puts the 22 ready Vanguards at about **$11 for hero images**
and roughly **$140 for all 13 slots** at four variations each. Dropping the non-hero slots
to a single variation brings the full roster to around **$45**.

### Account limits gate the run

The first run stopped after three images: the ElevenLabs account is on the **free plan**,
which has a daily image cap well below a 22-character batch. Generating the roster needs a
paid plan. The cap is on image count per day, not on credits, so it cannot be worked
around by reducing variations — only by spreading the run across days.
