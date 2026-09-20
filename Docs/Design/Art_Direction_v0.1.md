# Art direction and the art pipeline

**Version:** 0.1
**Date:** 2026-09-20
**Supersedes:** `Sheet_Generation_Prompt_v0.1.md`, which prompted for a whole sheet with its text baked in. [`Vanguards/render_sheet.py`](Vanguards/render_sheet.py) now owns the text, so only the artwork needs generating.

## The pipeline, as it now stands

Artwork is **authored outside this repository** and the finished file is dropped in:

```
ConceptArt/Vanguards/<id>/hero.webp
python3 Docs/Design/Vanguards/render_sheet.py --all       # re-render every sheet
python3 Docs/Design/Vanguards/render_sheet.py --missing   # which slots have no art yet
```

`render_sheet.py` composes the sheet's **text** from canon at render time and fills each
image slot from whatever file is present. It does not generate artwork and does not write
prompts. The emitter that used to build a prompt per slot was removed on 2026-09-21, once
all 25 heroes had been authored by hand and the author confirmed no further art would be
generated from this repository.

What that emitter existed to guarantee still holds, because it was never the emitter's
property: **text is never baked into an image.** The bible owns the words, the sheet
renders them, and a canon change is picked up by re-running the renderer.

Whoever writes the next prompt writes it by hand, from the Character Bible paragraph, the
signature colour below, and the model notes at the end of this document. The house style,
the hues and the guardrail shape are all still canon — only the machine that assembled
them into prompt text is gone.

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

**A range of human rendering, painterly-realistic everything else, one saturated signature
hue.** Written from the art rather than from intent — the author has confirmed the art is
what they want, so the art defines the style and this section describes it.

Everything that is not a human face is painterly-realistic: creatures, machines, armour,
cloth, stone, water and foliage, heavily rendered with physically believable materials.

**Human rendering spans a range, and the range is the rule** (ruled 2026-09-21: *"the ones
that came out leaning towards anime are gonna stay that way"*).

| Idiom | Vanguards |
|---|---|
| High-end anime | Kade, Tavi, Vera, Marek |
| Painterly realism | Raska, Qazharr, Angeru, Mavra, Sylra, Bryn, Eudora, Neris |

Neris moved from the first row to the second when her regenerated hero came back painterly.
The table is classified from the art that exists, so a regeneration can move a character
across it — check the new face rather than carrying the old entry forward.

Which end each sits at is held per character in `RENDER_BY_ID` in `render_sheet.py`, beside
the hue overrides, because it is a production choice rather than a fact about the character
— the bible says who they are, that table says how they are drawn. It reaches all nine
generated slots per Vanguard; characters with no human face take neither clause, since the
materials line already covers them.

**Classified by cropping all twelve human faces and comparing them side by side**, not by
recollection. Sylra was the borderline case and decided the size of the list: put next to a
known anime face (Vera) and a known painterly one (Mavra), she sits with Mavra — realistic
nose and lip structure, adult proportions, no anime eye.

*This section has now been wrong in both directions.* It first said human faces read as
anime full stop, which was a generalisation from twelve heroes that happened to exclude
Qazharr. It then said painterly realism was the uniform target, which was true only while
the anime heroes were expected to be replaced. Neither the idiom nor the roster was ever
uniform; the table is.

Strong directional key with an opposing rim, deep shadows, high contrast. **The value key
follows the character, not a fixed rule** — Raska, Korruk, Torr and Moro are bright open
daylight; Patch, Vera, Marek and Neris are deep night. The earlier "mid-to-dark value key"
was a blanket requirement half the roster does not meet and was never meant to.

Environments are **full, detailed scenes held in depth** — ruined cathedrals, drowned
harbours, overgrown canyons — rather than the plain fall-off backdrops originally
specified. The silhouette still has to read first at thumbnail size; this is a MOBA and the
shape identifies the character before the detail does.

Never in the artwork: UI, watermarks, artist signatures, stat bars, health bars, ability
icons, or tuning numbers of any kind. Tuning belongs in data per `ARCHITECTURE.md` §1.3,
and sheets that printed `Tracked for 4 seconds` into their pixels are why this is written
down. **Lettering that is part of the object is fine, including slogans (ruled 2026-09-21).**
Gorraveth's site banner reads *"SCARS BUILD STRONGER THINGS"* and Aurelisse's reads *"SAFER
PATHS BRIGHTER HORIZONS"* — both in plain English, both approved. The rule is against baking
interface and data into pixels, not against a world that writes on things. Note for whoever
handles localisation: this text is painted into the artwork and cannot be swapped per
language. Relay's art has `R-01` stencilled
on his plating, which is his canonical designation and belongs on a machine. The rule is
against baking data and interface into pixels, not against a unit having its serial number
painted on it.

### Neris and Sylra collide, and hue cannot fix it

Measured 2026-09-21, after Sylra's hero landed. Side by side they are close to the same
character: dark hooded headwear, long layered grey-blue robes ribboned at the hem, a **lit
blue-white lantern held out on chains in the right hand**, a **ship's-wheel charm** at the
hip, white mist around them, and a night harbour with ruined terraces and waterfalls
behind. The reliable difference is hair colour, which is worth nothing at thumbnail size.

This is the failure the house style's silhouette rule and the whole hue table exist to
prevent, and **the hue table cannot reach it**: both are Drowned Cantons, and both measure
205–215° — inside the no-override band, so neither qualifies for a separated colour even
under the rule that moved Patch, Korruk, Marek and Moro.

**The overlap is partly canon's, not just the generator's.** Both paragraphs specified a lit
lantern before any of this art existed — Neris's *"a chained coat hung with working gear
including a lit lantern"*, Sylra's *"a large lit lantern carried at the hip"*. The
ship's-wheel charm then arrived in both from their art, and was written into Neris's
paragraph from hers. Two Drowned Cantons women with hoods and lanterns was always going to
converge; the art made it visible.

**Fixing it is a design call, not an art-direction one**, so nothing is changed here beyond
recording the requirement in Sylra's paragraph so it reaches every prompt she generates.
The cheap levers, in rough order of how little they disturb canon: give one of them a
different light source or carry position; lean Neris's silhouette back toward the wave her
kit is built on; or separate their headwear shapes decisively.

### Lighting and environment are per-slot, not global

A hero illustration wants a dramatic key light and a full environment built out in depth.
A turnaround wants neither: flat even light, plain mid-grey background, no scene. A detail
crop wants the hero's lighting on a bare background — one of the two, not both.

Asking for both at once is the mistake to avoid, and it is easy to make: the removed
emitter shipped that contradiction twice, telling the turnarounds to be flatly lit *and*
dramatically lit, and telling the weapon crop to sit "alone on a neutral background" *and*
carry "a full, detailed scene" behind it. Each slot's brief in the table below states which
combination it wants; the sheet prints that brief in every empty slot.

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

An override needs a measured gap, not an impression. Three of the four sit **75–90°**
of hue from their region colour — far enough that the sheet's accent fought the art. Neris
is the counter-example and was left alone: her art measures 205–215° against Drowned
Cantons at 190°, a 15–25° gap that reads as the same sea colour seen at night, and any
move would have landed her on top of the two Flux blues at 210° and 223°.

**An override can also be declined because the destination is occupied.** Oriel measures
20–30° against Merrin at 346° — a 44° gap, wider than the band Neris and Qazharr sit in and
narrower than the four below. She was left alone anyway, because every usable gold in that
band lands on Buried Riverlands `#c19a4b`: measured dE 13–25, with the best candidate a pale
cream that works poorly as an accent. Moving her would have created the exact collision this
table exists to prevent, with Silt and Cairn.

She is also the one Vanguard whose canon palette is explicitly **polychrome** — *"gold, pale
blue, violet, and jewel-colored refractions"* — which is in real tension with one saturated
signature hue per character. Merrin's rose stays as a defensible accent for a figure that
has no single colour of its own.

**Mimzi is the second, and blocked in both directions.** Bramblehollow's yellow-green at 87°
appears nowhere on her — she is cream fur, a teal hood and gold-and-blue rings. Both
candidates her art suggests are taken: a warm tan lands on Buried Riverlands `#c19a4b` at
dE 12–13, and a teal lands on Drowned Cantons `#2e8b9e` at dE 7–25. She keeps the region
hue because there is nowhere for her to go, which is worth recording as its own outcome
rather than as agreement with the table.

Four exist:

| Vanguard | Region default | Override | Why |
|---|---|---|---|
| Patch | `#8a6bbf` (Unknown) | `#ff2d55` | His art is crimson — the spectral bear carries the whole frame |
| Korruk | `#6fc3d4` (Shatterdeep) | `#ff1a1a` | Cyan appears nowhere in his design, and his own canon already names crimson his default spine colour |
| Marek | `#2e8b9e` (Drowned Cantons) | `#9b4dd6` | Violet end to end in the art, and his canon already names violet eyes, a violet coat and violet behind Nix's mask |
| Moro | `#a8c64a` (Wildwood) | `#4040e0` | His Wildlight measures 240° against a yellow-green 75° — a **165°** gap, the largest in the set |

Korruk and Marek are the clearest cases, because in both the **bible already disagreed
with the region map** before any art existed: Korruk's paragraph said *"crimson is the
canon default"* and Marek's named violet eyes, a violet coat and violet behind Nix's mask.
The art did not overrule the text there — it made a contradiction visible that was already
sitting in the canon.

Measured separation, CIE76, against the nearest neighbours: `#ff2d55` sits **27** from
Reed Provinces `#c0392b` and **38** from Iron March `#c4552b`; `#ff1a1a` sits **29** from
`#ff2d55` and **34** from `#c0392b`; `#9b4dd6` sits **32** from `#8a6bbf` and **45** from
Fluxborn `#5e7fd4`; `#4040e0` sits **26** from `#9b4dd6` and **53** from `#5e7fd4`. All
clear the ~20 where two swatches stop reading as distinct at thumbnail size.

**Moro's override has a cost the others did not.** The draft grid is satisfied, but the
World Bible calls Wildlight *"natural magic distinct from Flux technology"* and colour was
what carried that distinction. His markings now sit in the same band as Relay's and Torr's.
What still separates them is **form** — Wildlight grows organically along limb and grain,
Flux is geometric glyphwork on stone and machine — so that is now written into his
guardrails as the thing doing the job colour used to do.

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
question a guess cannot answer. `--missing` counts them apart from the art that could
be made today, so the outstanding figure is not inflated by work nobody can start.

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
approved art is canon (see above). Each replacement's appearance paragraph is reconciled
with its art in one of two ways:

- **Rewritten**, where the art contradicted the text. The guardrails are replaced along
  with the description, because a guardrail written for a superseded design is the line
  the prompt obeys hardest.
- **Added to**, where the art agreed with what was already written, guardrails included.
  The existing text stays and the art supplies what it was missing.

**The list of which is which lives in the files, not here.** `--missing` prints it, derived
from the `superseded_gen1_*.webp` each replacement leaves behind. It was previously a count
and a list maintained by hand in this paragraph, and it drifted twice in a day — a doc that
opens by explaining why the prompts are generated rather than pasted should not keep a
hand-updated roster in its own body.

The machine pass each replacement supersedes is kept beside it rather than overwritten, so
what was rejected stays inspectable.

Each paragraph follows the same three-part shape, and new ones should:

1. **Physical description** — build, materials, palette, silhouette-defining gear, and the scale if it is unusual.
2. **A readability note** — what has to stay legible at gameplay camera distance, and which visual states the character carries.
3. **An anti-drift guardrail**, phrased as *is X, **not** Y* — the specific wrong reading a generator or a new artist will reach for. This is the part that does the work; without it the prompt drifts toward the nearest genre cliché.

Guardrails record what the design is **not**, which is the half no image can express. They
are also why the paragraphs must never carry tuning values: an appearance paragraph is art
direction, and gameplay numbers live in their owning data files.

Write the negation so it **begins a clause**, and keep the description that precedes it in
its own clause. This was a machine rule while the emitter existed — it split each paragraph
into affirmative description and a trailing constraint block, and cut at the negation — but
it survives the emitter because it is the same rule a human reader needs. Mavra's paragraph
once read *"Her equipment is industrial, not arcane — labelled containers, valves, straps,
gauges and pressure fittings…"*, which buries a whole equipment list downstream of a
negation; Silt's *"He is a living riverbed in motion and never resolves into a face"* buries
his best line the same way. Both are reworded, and both read better for it.

The guards that carry a Vanguard's **species or nature** belong in the `**Visual language:**`
field, not in a neighbouring header field. Five of them — Gorraveth's, Cairn's, Oriel's,
Aurelisse's and Picket's — were sitting elsewhere and so reached nothing that read the
appearance paragraph. They have been moved.

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

**The canon does not bend to suit a model, and never should.** The conversion happens in
the prompt layer: lead with the affirmative description, and collect the guardrails into
one short delimited clause at the end — `Critically: …`. A single delimited constraint is
what the guides tolerate; negation scattered through the prompt is what they warn about.

`render_sheet.py` used to perform that conversion automatically. It no longer does, so it
is now done by hand when a prompt is written. The rule is unchanged; only its enforcement
moved from a script to a person.

### Aspect ratio does not come from the prompt

Asking for `3:4` in prompt text produced `16:9`. The Nano Banana guide is explicit that
*"aspect ratios can be unreliable through prompting alone."* Aspect is a **node
parameter**, not prose: read the model's schema with `creative_get_model_schema`, then set
it with `creative_update_node`. This matters most for the turnaround slots, which are
specified `1:2` and will otherwise silently come back as landscape.

### Cost and throughput — measured on the abandoned in-repo run

Kept as a reference figure, not as current practice: the heroes were ultimately authored
externally, one at a time, and no batch run ever happened from this repository.

`gemini-3-pro-image` at 1K resolution billed about **12¢ per image**, so a four-variation
slot was roughly **49¢** — on the order of **$140 for all 13 slots** across the roster at
four variations each, or around **$45** with the non-hero slots at a single variation.

The run that was attempted stopped after three images, on a **free-plan daily image cap**.
The cap counted images per day rather than credits, so it could not be worked around by
reducing variations — only by spreading the run across days. That is the practical reason
a batch pipeline was worth less here than it looks on paper, and part of why the emitter
was eventually removed rather than fixed.
