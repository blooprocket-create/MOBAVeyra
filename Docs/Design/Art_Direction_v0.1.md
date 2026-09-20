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

## Approved art is canon

**Artwork approved by the author outranks any text that disagrees with it.** Where a
`**Visual language:**` paragraph, a header field, or an entry in the hue table below
contradicts approved art, the art wins and the text is corrected to match. The same rule
is recorded in the Character Bible header, because that is where the corrected paragraphs
actually live.

Correcting a paragraph means correcting **its guardrails too**. A guardrail written for a
superseded design is worse than no guardrail: it is the line of the prompt a model is told
to obey hardest, so it steers all eight remaining slots toward a character who no longer
exists.

Text keeps what an image cannot carry — what the design is **not**, the figures gameplay
needs, and the reason behind a choice. Relay's scale is the worked example: the art
withdraws "roughly 3.8 m" but cannot supply the number replacing it, so the bible records
the old figure as superseded and the exact one as still owed.

## House style

**Painterly realism with one saturated signature hue** — the rendering of Patch's sheet with the colour discipline of Tavi's.

Painterly and heavily rendered; not photoreal, not flat cel, not anime. Materials read physically. Every image for a Vanguard is built around **one saturated signature colour** that drives the key light and the accents, with everything else desaturated so that hue carries the frame. Strong directional key plus an opposing rim, deep shadows, high contrast, mid-to-dark value key. Silhouette readable at thumbnail size — this is a MOBA, and the shape identifies the character before the detail does.

Never in the artwork: text, lettering, logos, UI, watermarks, signatures, lens flare, stat bars, or numbers of any kind. Tuning belongs in data per `ARCHITECTURE.md` §1.3, and sheets that printed `Tracked for 4 seconds` into their pixels are why this is written down.

### Signature colours

Assigned by region so no two sheets collide in a draft grid. Held in `HUE` in `render_sheet.py`, overridden per Vanguard in `HUE_BY_ID`, and injected into every prompt.

| Region | Hue | | Region | Hue |
|---|---|---|---|---|
| Iron March | `#c4552b` | | Ember Basin | `#ff7a1a` |
| Drowned Cantons | `#2e8b9e` | | Bramblehollow | `#7fa650` |
| Merrin | `#c8536f` | | Wildwood | `#a8c64a` |
| Buried Riverlands | `#c19a4b` | | Reed Provinces | `#c0392b` |
| Shatterdeep | `#6fc3d4` | | Ancient Flux Network | `#4a8fd4` |
| Unknown / unplaced | `#8a6bbf` | | Ancient Fluxborn system | `#5e7fd4` |

#### Per-Vanguard overrides

A Vanguard whose approved art diverges from their regional hue takes an override in
`HUE_BY_ID`, keyed by id; the region map stays the default for everyone without one.
Two exist:

| Vanguard | Region default | Override | Why |
|---|---|---|---|
| Patch | `#8a6bbf` (Unknown) | `#ff2d55` | His art is crimson — the spectral bear carries the whole frame |
| Korruk | `#6fc3d4` (Shatterdeep) | `#ff1a1a` | Cyan appears nowhere in his design, and his own canon already names crimson his default spine colour |
| Marek | `#2e8b9e` (Drowned Cantons) | `#9b4dd6` | Violet end to end in the art, and his canon already names violet eyes, a violet coat and violet behind Nix's mask |

Korruk is the clearer of the two. His appearance paragraph said *"crimson is the canon
default"* before any art existed, so the region map was already contradicting the bible
and the art only made that visible.

Measured separation, CIE76, against the nearest neighbours in a crowded band: `#ff2d55`
sits **27** from Reed Provinces `#c0392b` and **38** from Iron March `#c4552b`; `#ff1a1a`
sits **29** from `#ff2d55` and **34** from `#c0392b`. Both clear the ~20 where two
swatches stop reading as the same colour at thumbnail size.

**The table has a pre-existing collision (measured 2026-09-20).** Reed Provinces `#c0392b`
and Iron March `#c4552b` are **dE 13.4** apart — under the threshold, and so already the
exact failure this table exists to prevent. It puts Angeru in the same apparent colour as
Raska, Kade, Mavra and Eudora in a draft grid. Neither hue is moved here: five Vanguards'
sheets change colour whichever one gives way, which is a deliberate call rather than a
cleanup to fold into an art commit.

A **second** pair sits just under the line: `Unknown / unplaced` `#8a6bbf` and Ancient
Fluxborn `#5e7fd4` measure **dE 18.8**. Nobody collides on it today — region `Unknown`
holds only Patch, who has an override — but it is why Marek took a separated violet
rather than the vacant `#8a6bbf`: squatting on a region hue would have made the next
unplaced Vanguard a collision instead of an assignment.

The two Flux origins are deliberately **not** the same hue. Relay came out of the
network itself; Torr began as a Fluxborn unit and stopped being one. They would
otherwise collide in a draft grid, which is the single thing this table exists to
prevent, so Fluxborn sits one step violet of the network blue.

**The art confirms the split (2026-09-20).** Torr's approved hero measures a dominant
saturated hue of **220°**, against Fluxborn `#5e7fd4` at 223° and network blue `#4a8fd4`
at 210°. It landed on the violet side without being asked to — the prompt carries the hex
but a generator is under no obligation to honour it — so the distinction is one the design
actually carries rather than a bookkeeping convenience. This is the first measurement that
has confirmed the table instead of challenging it.

## Workflow

**Generate `hero` first, approve one variation, then use that image as a reference for the other eight.** This is the whole trick to consistency. Without a reference the eight slots produce eight different people wearing similar clothes. (Eight, not twelve: the four in-game views are captured, not generated — see below.)

Use a text-to-image model for the hero and a reference-driven model for everything after it. Save results to `ConceptArt/Vanguards/<id>/<stem>.webp` and re-render the sheet; slots fill automatically when the file appears, and show a labelled work order when they do not. `.png`, `.jpg` and `.webp` all resolve, but WebP is what the repository stores — the heroes were 152 MB as PNG and are 14 MB as WebP.

## The thirteen slots — nine generated, four captured

| Stem | Aspect | Purpose |
|---|---|---|
| `hero` | 16:9 | Full-figure illustration, signature key light, region behind the subject |
| `front` `back` `side` | 1:2 | Orthographic A-pose turnaround, flat even light, plain background — modelling reference, not illustration |
| `scale` | 1:2 | Flat silhouette beside a 1.8 m human silhouette |
| `idle` `move` `cast` `ult` | 1:1 | Three-quarter top-down gameplay camera, silhouette readability first — **captured in engine, never generated** |
| `portrait` `weapon` `material` `signature` | 1:1 | Close crops |

### The hero slot is 16:9, not portrait

It was specified 3:4 and the generator produced 16:9, because **aspect ratio does
not come from the prompt** (see below). The slot has been changed to match the art
rather than the art regenerated to match the slot: a landscape hero banner is what
the original concept sheets used, 3:4 was an arbitrary choice, and regenerating 66
images to satisfy it would cost more credits than a month's plan holds.

The mismatch was not visible while it existed, which is the part worth recording.
`.slot.filled img` used `object-fit: cover`, so a 16:9 image in a 3:4 slot rendered
at **42% of its width** with the rest silently cropped — Bryn lost most of her
cannon. Slots now use `object-fit: contain`, so any future mismatch shows up as
letterboxing instead of as a missing weapon.

### The in-game views are captured, not generated

`idle` `move` `cast` `ult` are **screen captures of the running game**, taken once
there is a build to take them from. A generated picture of a gameplay camera is a
guess about a build that does not exist, and the one question these slots exist to
answer — does the silhouette actually read at gameplay distance? — is precisely the
question a guess cannot answer. `--prompts` refuses to emit prompts for them and
`--missing` counts them apart from the art that is genuinely outstanding.

That leaves **nine generated slots per Vanguard**, not thirteen.

| | Per Vanguard | Roster of 25 | The 22 with an appearance |
|---|---|---|---|
| Generated | 9 | 225 | 198 |
| Captured in engine | 4 | 100 | 88 |

With all 25 heroes done, **200 generated images remain** — eight apiece. If that is
too much at once, the tiering that keeps the sheets useful is: `hero` alone makes a
sheet presentable; `hero` + turnaround makes it buildable; the detail crops are the
first thing to cut.

## Appearance coverage — resolved

**All 25 Vanguards carry a `**Visual language:**` paragraph.** The original blocker was that
only 8 did, so for the other seventeen the sole record of how the character looked was the
concept sheet itself — the same failure as text baked into an image, in a more expensive
form: the design existed only as pixels, and nothing could validate, diff or correct it.

Fourteen were written up by reading their surviving art before it was regenerated. Raska,
Kade and Angeru were designed clean from written canon, because their sheets had been
withdrawn for third-party content and reconstructing that look would have carried the
reason for the withdrawal forward.

Since then the author has begun regenerating characters externally, one at a time, and
approved art is canon (see above): **10 of 25** paragraphs have been reconciled with their
art rather than the art judged against them. Nine were rewritten — Raska, Kade, Silt, Relay,
Patch, Tavi, Vera, Korruk and Marek. Torr's was only added to, because his art agreed with what was
already written, guardrails included; where that happens the existing text stays and the art
supplies what it was missing. Each
rewrite replaces the guardrails as well as the description. The machine pass each one
replaces is kept beside it as `superseded_gen1_*.webp`, so what was rejected stays
inspectable instead of being silently overwritten.

Each paragraph follows the same three-part shape, and new ones should:

1. **Physical description** — build, materials, palette, silhouette-defining gear, and the scale if it is unusual.
2. **A readability note** — what has to stay legible at gameplay camera distance, and which visual states the character carries.
3. **An anti-drift guardrail**, phrased as *is X, **not** Y* — the specific wrong reading a generator or a new artist will reach for. This is the part that does the work; without it the prompt drifts toward the nearest genre cliché.

Guardrails record what the design is **not**, which is the half no image can express. They
are also why the paragraphs must never carry tuning values: an appearance paragraph is art
direction, and gameplay numbers live in their owning data files.

Write the negation so it **begins a clause** — `split_guardrails` cuts there, and a negation
buried mid-clause ("a war he was never built for") sends the whole sentence to the
constraint instead of splitting it.

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
