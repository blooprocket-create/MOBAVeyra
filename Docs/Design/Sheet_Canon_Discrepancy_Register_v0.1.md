# Concept-Sheet ↔ Design-Canon Discrepancy Register

**Version:** 0.1
**Date:** 2026-09-20
**Scope:** Every known conflict between a character sheet in [`ConceptArt/Characters/`](../../ConceptArt/Characters/) and the current design canon in `Docs/Design/`.

## Why this file exists

Character sheets are **visual development references**. They are *not* specification. Where a sheet and a design bible disagree, the **bible wins**, without exception.

That rule is easy to state and easy to ignore, because a finished-looking sheet is far more persuasive to an implementer than a paragraph of Markdown. This register exists so that the disagreements are written down in one place, in the same repository, and so that any future art regeneration has a concrete correction checklist instead of relying on someone remembering a ruling from months earlier.

**Nothing in this file changes canon.** It records what the canon already says and where the art has fallen behind it.

## Authority order

1. `ARCHITECTURE.md` — technical boundaries.
2. `Docs/ADR/` — accepted architecture decisions.
3. `Docs/Design/*.md` — current working design canon (the bibles).
4. `ConceptArt/` — visual reference only. **Never** a source of gameplay rules, ability names, numeric values, role assignments, or lore facts.

Where two bibles disagree, the owner named in [`Docs/Design/README.md`](README.md) wins for that subject. The Vision Bible owns Dense Fog; the Combat Bible owns control, displacement, shields and hit validation.

---

## A. Sheets withdrawn from the index — third-party material

Three sheets carry visible third-party branding, third-party character/item art, or third-party lore terms. They must not be used as reference in their current form and must not remain published in a public repository.

| Sheet | Third-party content present |
|---|---|
| `Raska_The_Redline_Character_Sheet_v2.png` | League of Legends wordmark and logo; lore text placing Raska in "the undercity of Zaun"; two rows of League champion portrait art (Orianna, Zilean, Lulu, Miss Fortune, Rell / Poppy, Cassiopeia, Taliyah, Nasus, Anivia, Vex) |
| `Kade_Dead_Reckoning_Character_Sheet.png` | League of Legends wordmark and logo; "A Zaunite surveyor"; League champion portrait rows; a "Recommended Build" panel of League item icons (Kraken Slayer, Infinity Edge, Rapid Firecannon, Lord Dominik's Regards, Guardian Angel, Berserker's Greaves) |
| `Angeru_The_Housebreaker_Character_Sheet.png` | An "Inspiration" panel reproducing League champion splash art and naming Zed, Katarina, Yasuo and Kayn against specific mechanics |

**Status:** deleted from the working tree and withdrawn from the sheet index.

**Remaining action:** remove the originals **from Git history**. A deletion commit is not sufficient — the files stay retrievable in every existing clone and fork, and remain reachable by commit SHA on a public repository, until history is rewritten (for example with `git-filter-repo`) and force-pushed. Regenerate all three with Veyra-owned visual material only, on the current sheet template.

These are Vanguards **#1, #2 and #15**. Raska and Kade are the roster's lead pair, so these are the first sheets an outside reader encounters.

---

## B. Confirmed canon contradictions

Each row states what the sheet shows, what canon says, and which document owns the ruling. **Canon is correct in every row below.**

### B1. Korruk — origin and nature

- **Sheet:** "Korruk is an **Echo** born from the final siege of **Merrin**." Environment panel titled "Environment Explorations — Merrin."
- **Canon** (Character Bible §8): Origin **Shatterdeep**. Nature: **native biological fauna**. Explicitly: "He is **not** an Echo, Flux mutation, construct, hidden god, or secretly humanoid intelligence."
- **Impact:** total origin conflict — wrong region, wrong species category, wrong environment art. This is the largest single discrepancy in the set.
- **On regeneration:** Shatterdeep environments; biological predator framing; confirm the silhouette reads as the canonical **six-legged** body plan.
- **Regeneration checked (2026-09-20)** against the authored hero art now installed at `ConceptArt/Vanguards/korruk/hero.webp`:
  - *Biological predator framing* — **met.** He reads as a mineralised animal: low, horizontal, load-bearing, head carried forward. No construct or humanoid reading.
  - *Six-legged body plan* — **unconfirmed.** The pose is a low three-quarter front view and the far-side limbs are occluded, so the count cannot be read off this image either way. The `side` and `scale` turnaround slots are what settle it, and they are specified flat and orthographic precisely for this.
  - *Shatterdeep environment* — **still open.** The backdrop is generic ruined stone with red heraldic banners, carrying no Shatterdeep-specific reading. It also hangs the **same white cross-and-sword device** as Vera's hero art (§7, Merrin). That device is not yet assigned to a faction in canon, so this is flagged rather than called a repeat of the original error — but if it turns out to be Merrin's, this entry's core discrepancy has come back. The device is **not** simply a constant of the generator's backdrop: Patch's art carries a different one, Raska's banners are blank, and Torr's hangs a compass-rose. Two images sharing one is therefore worth a ruling rather than a shrug.

### B2. Bryn — W ability name and Dense Fog behaviour

- **Sheet:** ability named "**Harbor Flare**"; text reads "*reducing local Dense Fog in the area, revealing enemies within the radius*". Weakness list: "Requires line of sight (Dense Fog blocks vision)".
- **Canon** (Character Bible §19; Vision Bible; Design README): ability is "**Sounding Flare**". It reports that an enemy Vanguard is **present in a fog volume** — not their model, outline, coordinates or targetability. It does **not** "thin, remove, or convert Dense Fog", does not grant remote targeted acquisition, and does not bypass stealth or terrain rules.
- **Impact:** the sheet describes a fog-clearing reveal, which is the exact behaviour the Vision Bible and the Design README rule out by name.

### B3. Cairn — E displacement handling

- **Sheet:** "*becomes **immune** to pulls, pushes, and knockbacks*".
- **Canon** (Character Bible §18): Immovable "**reduces the distance** he is forcibly moved ... by a configurable amount". Explicitly: "he is **not immune** to Pull or Knockback, does not gain Unstoppable, and can still be interrupted or affected by other valid crowd control".
- **Impact:** immunity versus distance reduction is a different ability with different counterplay. Combat Bible displacement rules govern.

### B4. Patch — passive is a different ability

- **Sheet:** passive "**Unbreakable**" — stacks of Resolve gained by taking damage from enemy Vanguards; at max stacks an empowered basic attack that **taunts** and refreshes defensive resistances. Q listed as causing "*a short stun*". R lists "*All taunts last longer*".
- **Canon** (Character Bible §5): passive is "**Haunted Attachment**" — the first enemy Vanguard to damage Patch becomes **Haunted**; while Haunted, damaging one of Patch's *allies* instead of Patch triggers a spirit retaliation (small magic damage, brief slow), on a per-enemy cooldown. Q's recast produces "a slight opposite-direction **stumble**", not a stun. **Taunt does not appear anywhere in Patch's canonical kit.**
- **Impact:** wrong passive, wrong control type, wrong design intent. The canonical passive expresses "*You started this with me. Finish it with me.*"; the sheet's expresses a generic damage-tank ramp.

### B5. Moro — passive is incomplete

- **Sheet:** passive "**Wild Recovery**" — healing from damaging jungle wildlife only.
- **Canon** (Character Bible §12): passive is "**Wild Dominion**" — while in **jungle terrain**, increased percentage damage **and Attack Speed**; healing from damaging wildlife; Wildlight markings brighten while active.
- **Impact:** the sheet drops the terrain-conditional power that is Moro's entire identity ("strongest in its own terrain") and keeps only the sustain.

### B6. Kade — role label and reveal effect

*(Sheet also withdrawn under section A; recorded here for regeneration.)*

- **Sheet:** `Role: ADC`; W "*Enemies inside are **revealed***"; R "*up to **three** massive piercing shots*"; title rendered "THE Dead Reckoning".
- **Canon:** the roster principles open with "Veyra's first playable roster is intentionally **not** organized around mandatory top/jungle/mid/carry/support slots" — "ADC" is a foreign role taxonomy. W's canonical effects are listed as *possible* ("reveal/vision along the corridor" among options), not settled. R is "a **limited series** of powerful piercing shots", count unfixed. Roster table title is "**Kade, Dead Reckoning**" with no leading article.
- **Impact:** any reveal effect is Vision-Bible-governed and must be specified there before art asserts it.

### B7. Raska — origin

*(Sheet also withdrawn under section A; recorded here for regeneration.)*

- **Sheet:** "Born in the undercity of **Zaun**."
- **Canon** (Character Bible §1): **Iron March, Calder's Run**.

### B8. Silt — pronouns, and an unresolved title

- **Sheet:** uses **they/them** ("They are a convergence"; "They do not speak often"). Titled "**The Living Mire**".
- **Canon** (Character Bible §3): uses **he/him** ("heals **him**"; "**His** body visibly repairs"). The roster table lists Silt with **no title at all** — the only Vanguard without one.
- **Ruling (2026-09-20): the bible wins on pronouns.** Silt is **he/him**. The sheet is wrong and is corrected on re-render.
- **Ruling (2026-09-20): the sheet wins on the title.** Silt is now **Silt, The Living Mire** in canon, adopting the title the sheet already carries. He was previously the only Vanguard without one. This element of the sheet needs no correction.

### B9. Neris — pronouns

- **Sheet:** uses **they/them** ("Neris gave **their** life"; "**They** can calm the sea").
- **Canon** (Character Bible §11): uses **she/her** ("**Her** kit shifts between two sea states").
- **Ruling (2026-09-20): the bible wins.** Neris is **she/her**. The sheet is wrong and is corrected on re-render.

---

## C. Omissions — sheet is not wrong, but drops a load-bearing rule

These do not contradict canon; they silently omit a guard that exists specifically to protect a Vision or Combat rule. An implementer working only from the sheet would build the unguarded version.

| Vanguard | Omitted guard | Source |
|---|---|---|
| **Oriel** | Gathering Light's fragment may only home onto an enemy Oriel can **legally acquire at fragment launch**; a blind hit into Dense Fog deals normal damage but generates no fragment, and the passive **stays primed** | Character Bible §20; Design README |
| **Mimzi** | The post-Camouflage attack applies two Hex stacks **only if the target was not already primed**; on a primed target it consumes the existing stacks for one normal proc without re-priming | Character Bible §21 |
| **Bryn** | Q's Breach-consuming explosion **replaces** the passive's explosion — one impact, one secondary explosion, and neither explosion recursively triggers Breach or On-Hit effects | Character Bible §19; Design README |
| **Cairn** | Passive and R shields share a **single configurable total cap** on Cairn-generated shielding; repeated immobilisation of the same target within a per-target lockout grants no second shield | Character Bible §18; Design README |

---

## D. Numeric values printed on sheets

`ARCHITECTURE.md` §1.3 requires all gameplay tuning to live in validated, designer-editable data. Several sheets print concrete numbers into the artwork, where they cannot be edited, reviewed or validated, and where they read as settled specification:

- **Kade:** "Tracked for **4 seconds**"; "up to **three** ... shots"
- **Relay:** "Consumes **25%** of your current Charge"
- Various stack ceilings and durations across the older sheet generations

**Rule for regeneration:** sheets must describe behaviour qualitatively ("a short duration", "a portion of current Charge") and print no tuning values. Where a count is genuinely structural rather than balance — Mimzi's two-stack preparatory cap, Oriel's three stacks, Celandrine's three empowered attacks, Bryn's third-hit Breach — it may appear, because canon fixes it as a mechanic rather than a tuning knob.

---

## E. Render defects to correct

Text baked into the artwork, so each requires a re-render:

| Sheet | Defect |
|---|---|
| **Vera** | Passive titled "**CADNCE**" → *Cadence* |
| **Varkesh** | "a Heated **meemy**" → *enemy*; "**substantail** shield" → *substantial*; "**heevy** magic damage" → *heavy* |
| **Celandrine** | "an enemy **Vangaurd**" → *Vanguard*; "**wildliifie**" → *wildlife*; corrupted glyph in "REPOSITI**Ω**N" |
| **Cairn** | Strength listed as "**Alley** protection" → *Ally protection* |

---

## F. Presentation inconsistency across the set

Not errors, but they prevent the set reading as one product.

**Three layout generations are in circulation:**

1. **Third-party-templated** — Raska, Kade, Angeru. Withdrawn under section A.
2. **"A World in Motion" generation** — Silt, Patch, Moro, Relay, Tavi, Vera, Korruk, Torr, Marek, Neris, and others. Hero art, role block, 1–5 stat bars, difficulty pips, concept sketches, scale comparison, ability strip, personality/key traits, environment explorations.
3. **Current generation** — Mimzi, Celandrine, Gorraveth, Aurelisse, Eudora Blackbridge. Role/species/region header, weapon card, lore, turnaround with silhouette and scale, tagged ability kit, playstyle with strengths/weaknesses, in-game views, detail crops, region strip, colour palette, key traits.

**Generation 3 should be adopted as the standard sheet template.** It is the only generation that carries a species/region taxonomy, an explicit weaknesses list, in-game view mockups and a colour palette — the four things a modeller, a VFX artist and a balance designer respectively need. The older sheets should be retrofitted to it as art budget allows.

**Sub-brand line varies** across sheets: "A World in Motion", "A Living World", "Vanguards of a Higher Tomorrow", "Vanguards of a Shattered World", and none at all on generation 3. One line should be selected and applied uniformly.

**Difficulty scale varies**: five pips, six pips, five stars, five diamonds, or absent entirely on generation 3. If difficulty is shown at all it needs one scale with defined anchors.

---

## G. Resolved and remaining

### Resolved 2026-09-20

| Question | Ruling |
|---|---|
| Silt's and Neris's pronouns | **The bible wins for both.** Silt is he/him, Neris is she/her (§B8, §B9). Sheets corrected on re-render. |
| Silt's title | **Adopted from the sheet:** Silt, **The Living Mire**. |
| *Tavi* / *Tavra* collision | **Tavra renamed to Bryn.** Tavi keeps her name — its softness serves the childlike Echo, and Bryn's identity sits in "The Harbor Gun" and Mournwake rather than the name. |
| *Vera* / *Vitra* collision | **Vitra renamed to Oriel.** An oriel is a projecting bay window, which carries her origin in Merrin's shattered glass exactly. Vera, the earlier design, keeps hers. |
| "The Last —" repetition | Reduced from four to three. Oriel is retitled **The Waiting Light**; Patch, Vera and Relay keep theirs. |
| Kade's Sightline | **No reveal.** Ordinary corridor vision only; never into Dense Fog, never exposing stealth, never remote acquisition. Character Bible §2. |
| Tavi's Hide! | **Brief Invisibility**, not Camouflage. Character Bible §6. |
| Owned field entities | **ADR-003 accepted** — three primitives, ride state scoped separately, implementation sequenced from the nine zero-entity Vanguards. Build order only; all 25 remain in the first-playable roster. |
| Machine-readable Vanguard data | **Built.** See [`Vanguards/`](Vanguards/) and its validator. |

Renames apply to **current canon only**. Archived bible versions under [`Archives/`](Archives/) retain the former names and are not retro-edited; see the archive index.

### Remaining

1. **Two sheets now also carry a former name.** Bryn's sheet renders "Tavra" and Oriel's renders "Vitra, The Last Light". Both need re-render regardless — Bryn's for the Dense Fog error (§B2) — so this adds no new work, but the filenames still use the old names until then.
2. **Initial clustering.** Four M-names (Marek, Moro, Mavra, Mimzi), three T-names, three V-names remain. B, D, F, H, I, J, L, O, U, W, X, Y and Z were unused before these renames; O and B are now taken. Worth drawing from the remainder for future additions.
3. **Two bible rulings ADR-003 defers**, both required before the corresponding Vanguard is built:
   - the **Vision Bible** must state whether Dense Fog volumes are runtime-spawnable (Sylra's Lay the Mist and Through the White assume yes);
   - the **Battleground Bible** must state whether an ability may modify pathing for both teams at runtime (Varkesh's Iron Wall assumes yes).
4. **Third-party material still in Git history** (§A). Deleted from the working tree; removal from history is outstanding.
