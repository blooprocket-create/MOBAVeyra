# Character-sheet generation prompt

**Version:** 0.1
**Date:** 2026-09-20
**Purpose:** Produce a consistent Vanguard character sheet from canon, in the agreed house style.

## Read this before using it

**Text baked into a generated image cannot be corrected, validated or version-controlled.** Every render defect in this repository arrived that way — `CADNCE`, `a Heated meemy`, `substantail`, `heevy`, `Vangaurd`, `wildliifie`, `Alley protection` — as did canon contradictions like Korruk's origin and Bryn's fog reveal. A single rename then costs a full re-render and a fresh crop of new defects.

The recommended pipeline is therefore **two layers**:

1. **Art layer, generated.** Hero illustration, turnaround, detail crops, environment strip, in-game views. No text.
2. **Text layer, composed.** Typeset from [`Vanguards/`](Vanguards/) and the Character Bible, in HTML/SVG or a layout tool.

A canon change then re-renders the cheap layer and leaves the expensive one alone, and the validator already guarantees the data matches the bibles.

**Use the single-image prompt below only for exploration**, and expect to proofread every word against canon before the result is treated as reference. Section D of the discrepancy register exists because numbers printed into artwork get read as specification.

---

## House style

Adopted 2026-09-20. **Painterly realism with saturated key light** — the rendering of Patch's sheet with the colour discipline of Tavi's.

- **Rendering:** painterly and heavily rendered, not photoreal and not flat-cel. Visible brushwork in the background, tight rendering on the focal character. Materials read physically: cloth, metal, scale, glass, fur.
- **Colour:** every sheet is built around **one saturated signature hue** carried through the key light, the ability icons, the panel accents and the palette chips. Everything else stays desaturated so that hue does the work.
- **Light:** strong directional key in the signature hue plus a cool or warm counter-rim. Deep shadows. High contrast. No flat ambient lighting.
- **Value key:** mid-to-dark overall. The character is the brightest thing in frame.
- **Background:** atmospheric location depth — ruins, harbour, canyon, forge — held well behind the subject and never competing with the silhouette.
- **Silhouette:** readable at thumbnail size. This is a MOBA; the shape must identify the character before the detail does.
- **Avoid:** photobashing, stock-photo faces, lens flares, HUD overlays, watermarks, signatures, and any third-party game's logo, UI, item icons or characters.

---

## Layout template

The standard is the **Eudora Blackbridge** sheet, the most recent generation. Portrait, roughly 1220 × 1290.

```
┌─────────────────────────────┬───────────────────────────────┐
│ NAME / TITLE / pull-quote   │ ROLE · SPECIES · REGION        │
│                             ├───────────────────────────────┤
│ HERO ILLUSTRATION           │ WEAPON / FOCUS card           │
│ (≈55% width, full bleed)    ├───────────────────────────────┤
│                             │ LORE (3 short paragraphs)     │
│                             ├───────────────────────────────┤
│                             │ VISUAL EXPLORATION            │
│                             │ front · back · side · scale   │
├──────────────┬──────────────┴───────────────────────────────┤
│ ABILITY KIT  │ PLAYSTYLE        │ IN-GAME VIEWS             │
│ P Q W E R    │ three-word verb  │ idle · move · cast · ult  │
│ icon + name  │ + short para     ├───────────────────────────┤
│ + tag line   │ STRENGTHS (5)    │ DETAILS — 4 crops         │
│ + 2 lines    │ WEAKNESSES (5)   │                           │
├──────────────┴──────────────────┼───────────┬───────────────┤
│ REGION strip                    │ PALETTE   │ KEY TRAITS (5)│
└─────────────────────────────────┴───────────┴───────────────┘
```

**Not on the sheet, ever:** difficulty pips, 1–5 stat bars, cooldowns, durations, damage numbers, ranges, percentages, or any other tuning value. Tuning lives in data per `ARCHITECTURE.md` §1.3, and earlier sheets printing `Tracked for 4 seconds` and `25% of current Charge` is why this rule is written down.

---

## The prompt

Paste the block below, then append the Vanguard's data block. Every text string must be **copied exactly** from the Character Bible and `Vanguards/<n>-<id>.yaml` — never paraphrased, never invented.

```text
Create a single-page character design sheet for a fantasy MOBA, portrait orientation,
approximately 1220 x 1290.

RENDERING: painterly realism, heavily rendered, visible brushwork in the background and
tight rendering on the focal figure. Not photoreal, not flat cel-shading, not anime.
Materials must read physically.

COLOUR AND LIGHT: build the entire sheet around ONE saturated signature colour, given
below. It drives the key light on the character, the ability icon glows, the panel
accents and the palette chips. Everything else is desaturated so that hue carries the
page. Strong directional key light in the signature colour, plus an opposing cool or
warm rim. Deep shadows, high contrast, mid-to-dark overall value. The character is the
brightest thing in frame.

COMPOSITION: dark panelled layout on a near-black background, thin light rules between
panels, generous margins. Hero illustration occupies the upper left at roughly 55% of
the width. Supporting panels are arranged as follows:
  - upper right: role / species / region header, then a weapon or focus card, then three
    short lore paragraphs, then a turnaround strip of front, back and side views with a
    human-scale silhouette
  - middle left: five ability entries, each with a glowing icon in the signature colour,
    the ability name, a short tag line, and two lines of description
  - middle centre: a three-word playstyle line in large type, a short paragraph, then a
    strengths list and a weaknesses list
  - middle right: four small in-game view thumbnails, then four close-up detail crops
  - bottom: a wide environment strip for the character's home region, a colour palette
    of six to eight chips, and five key-trait icons with one-word labels

TYPOGRAPHY: elegant serif for the name and headings, clean sans for body text. The name
is the largest element on the page.

STRICTLY EXCLUDE: any real-world game logo, wordmark, UI, item icon or character; stat
bars; difficulty ratings; numeric values of any kind; watermarks; signatures; lens flare.

Use the following content exactly as written. Do not paraphrase, summarise, reorder or
invent any text. If a field is absent, omit that panel rather than filling it.
```

### Data block to append

```text
NAME:               <name>
TITLE:              <title>
PULL QUOTE:         <one line of dialogue from the bible>
ROLE:               <role_tags, title-cased and slash-joined>
SPECIES / NATURE:   <nature>
REGION:             <origin_region>
SIGNATURE COLOUR:   <one saturated hue — see the table below>
WEAPON / FOCUS:     <name and one-line description>
LORE:               <three short paragraphs, condensed from the bible, no new facts>
ABILITIES:
  PASSIVE — <name> | <tags> | <two lines, behaviour only, no numbers>
  Q — <name> | <tags> | <two lines>
  W — <name> | <tags> | <two lines>
  E — <name> | <tags> | <two lines>
  R — <name> | <tags> | <two lines>
PLAYSTYLE:          <three imperative words> + <short paragraph>
STRENGTHS:          <five short phrases>
WEAKNESSES:         <five short phrases>
KEY TRAITS:         <five single words>
APPEARANCE:         <the Visual paragraph from the bible, verbatim where one exists>
```

### Signature colours

One hue per Vanguard, chosen so that no two neighbours in a draft-screen grid collide.

| Region | Hue family |
|---|---|
| Iron March | rust orange, ember red |
| Drowned Cantons | storm teal, lantern amber |
| Merrin | rose, bone gold, refracted violet |
| Buried Riverlands | ochre, wet stone grey-green |
| Shatterdeep | pale cyan, mineral white |
| Ember Basin | molten orange, slag black |
| Bramblehollow | moss green, brass |
| Wildwood | wildlight green-gold |
| Reed Provinces | ink black, single red accent |
| Fluxborn / Network | flux blue, violet |

---

## Antagonists

Four Vanguards were rewritten as genuine antagonists on 2026-09-20. Their sheets should read as such, **without tipping into cartoon villainy** — in each case the unsettling quality is that they are not cruel.

| Vanguard | Direction for the art |
|---|---|
| **Angeru** | Composed, methodical, unhurried. Not snarling. The threat is that he will explain himself. |
| **Marek** | Delighted, curious, entirely unbothered. What is behind him should be worse than he is. |
| **Gorraveth** | Indifferent rather than enraged. He is not hunting you; you are simply in the contract area. |
| **Tavi** | Bright, warm, genuinely happy. Nothing in the lighting should signal danger. That is the point. |

Do **not** restore the removed safety rails; each Vanguard's data file records them as removed, with an explicit instruction not to reinstate them.
