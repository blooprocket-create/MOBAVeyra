# Pull request record

**Version:** 0.1
**Date:** 2026-09-20

## Why this document exists

Five pull requests carried this repository from a sheet review to its current state. Their
**review threads** — sixteen findings and the responses to them — existed only on GitHub, which
means they would not survive the repository being deleted, transferred or recreated. They are
reproduced here in full.

The **PR descriptions** are not reproduced, because their substance was deliberately written into
the repository as it was decided: rulings into the Character Bible header, art rules into
`Design/Art_Direction_v0.1.md`, sheet conflicts into the discrepancy register, architecture into
`ADR/`. The index below says where each one's content lives. A description that survives only as
a PR body is a decision that was never really recorded, which is the failure this whole repository
is organised against.

Findings are quoted as written. The responses are quoted as posted. Where a later commit changed
the answer, this document says so rather than editing the record.

## The five pull requests

| # | Title | Merged | Where its content lives now |
|---|---|---|---|
| 1 | Sheet/canon authority, Vanguard data layer, roster rulings, ADR-003 | 2026-09-20 06:58 | `Sheet_Canon_Discrepancy_Register_v0.1.md`, `Design/Vanguards/*.yaml`, `validate.py`, `ADR/ADR-003`, the renames and vision rulings in the Character Bible |
| 2 | Ride-state canon; history purge executed | 2026-09-20 08:02 | Combat Bible §56, Character Bible §1, `Ride_State_Open_Questions_v0.1.md`, `Runbooks/purge-third-party-sheets.md` |
| 3 | Antagonist pass; sheets rendered from canon; appearance written into canon; hero art | 2026-09-20 10:24 | The four antagonist entries and their removed-rail notes, 14 new `**Visual language:**` paragraphs, `render_sheet.py`, `Art_Direction_v0.1.md` |
| 4 | Authored hero replacements, and approved art made canon | 2026-09-20 17:27 | The art-precedence rule in the Character Bible header and `Art_Direction_v0.1.md`, the hue override table, the rendering-idiom table, 25 hero images, 25 rendered sheets |
| 5 | Re-verify the purge exposure before the Support request is filed | open | `Runbooks/purge-third-party-sheets.md` step 6 |

## Review findings

All sixteen were raised by `chatgpt-codex-connector`. Every one was reproduced before being acted
on; several were reproduced and found **already fixed**, which is recorded as such rather than
claimed as a new fix.

### PR #1 — seven findings, all resolved

**1. Classify Magnetic Field consistently with ADR-003** (P2)
> ADR-003 explicitly places Relay's Magnetic Field node in the placed-marker category, while this
> record classifies it as a world volume. Because the validator and README present these YAML
> records as the ADR-003 inventory, this silently assigns the feature to a different primitive and
> distorts the reported category totals.

Already fixed in `f1ae394`, before the review landed. The ADR prose predated the data layer and was
wrong: Magnetic Field is a field, so `world_volume` was correct and the YAML was right.

**2. Reject tuning encoded as strings or range fields** (P2)
> The tuning guard does not inspect string values and its key pattern omits categories explicitly
> forbidden by the accompanying README, including `range`. Consequently entries such as
> `attack_range: "9999"` or `range: "10 metres"` pass with exit code 0.

Half right, both halves fixed in `0124401`. The string gap was real — `attack_reach: "1200 units"`
exited 0. The `range` half was already covered by the numeric-scalar rule rather than the key
pattern. One detail worth keeping: `\brange\b` deliberately does **not** match
`basic_attack.range_class`, because `_` is a word character so no boundary follows `range`. That
field is structural, not a quantity, and must keep passing.

**3. Validate grants before deriving sustain totals** (P2)
> A typo such as `ally_sheild` therefore passes while silently removing that Vanguard from the
> ally-shielding summary, making the roster-level sustain conclusions unreliable.

The most consequential of the seven. Reproduced with `ally_sheild` on Aurelisse: exit 0, and she
silently vanished from the ally-shielding line. Those totals are not decorative — "ally healing
exists on exactly one Vanguard" was being used to reason about a structural gap in the roster.
Fixed in `0124401` against a controlled vocabulary.

**4. Require files for sheets marked current** (P2)
> `status: current` with `file: null` is accepted. That record is then excluded from `sheets not
> usable as-is`, causing the validator to claim a missing sheet is current and undercount unusable
> sheets.

Reproduced on Torr: exit 0, and he dropped out of the unusable line. `withdrawn` and `missing` now
require `file: null`; every other status must name a file that exists. Asserted in both directions.

**5. Validate discrepancy-register references** (P2)
> Empty, misspelled, or nonexistent references such as `B99` all pass, so the validator can report
> clean after the sheet/canon linkage has broken.

`register_refs` are now parsed from the register's own headings, the same way the CC vocabulary is
parsed from the Combat Bible, so the two cannot drift. Any status other than `current` must carry
at least one ref.

**6. Compare complete roster names** (P2)
> Changing `Eudora Blackbridge` to `Eudora Wrongname` still validates successfully.

It did. The parser now splits each roster cell into name and title and compares both exactly; the
title check caught `The Final Volley` on Vera in testing. One normalisation was needed: the table
lists Marek as `**Marek, The Black Accord + Nix**` while his title is `The Black Accord`, so a
trailing `+ <word>` is stripped. This check is what caught the Tavra→Bryn and Vitra→Oriel rename
drift.

**7. Correct the world-volume owner count** (P2)
> The committed YAML contains world volumes for nine Vanguards — Silt, Relay, Neris, Moro, Varkesh,
> Sylra, Mavra, Celandrine, and Aurelisse — not eight.

Correct, already fixed in `f1ae394`. The same commit corrected ADR-003's companion figure: sixteen
Vanguards place a persistent entity, not thirteen. The response noted the count was still
hand-maintained in prose — an observation that proved right twice more later.

### PR #2 — four findings, all resolved

**1. Preserve the failed-cast cooldown from §26** (P1)
> For a ride entry interrupted before Commit, this says that no cooldown is spent, but §26 lines
> 850–854 requires a Cast-Time ability to enter 20% of its normal cooldown … leaving Kickstart
> implementations with two contradictory authoritative outcomes.

The sharpest of the four. §56 said no cooldown is spent *and* claimed that followed §26. Ride entry
now follows §26 without exception — inventing one silently is exactly what §57 prohibits. Q9 of the
decision record carried the same false claim and was corrected in place, with a note recording what
it originally said.

**2. Exclude displacements from the turn-rate constraint** (P1)
> As written, a knockback could curve or fail to travel directly away from its source.

Confirmed. §9 gives forced displacement outright ownership of the target's movement, so a rule
about the rider's own locomotion was silently overriding a rule about something done *to* the
rider. Now scoped so only locomotion-controlling CC — Fear, Taunt, Charm — travels within the
angular constraint.

**3. Put Raska-specific rules in their authoritative bible** (P1)
> This says distance-based Momentum, automatic Last Exit, and overlapping-hit displacement
> resolution remain in the Character Bible, but `Veyra_Initial_Roster_Character_Bible_v0.6.md`
> contains none of those rules.

The worst of the four, because it broke what the document was asserting: three gameplay-defining
decisions existed only in the one document that disclaims its own authority, while the
implementation gate was declared clear. All now in Character Bible §1, each cross-referenced to the
generic rule it sits on. A fourth rule — NO BRAKES granting Unstoppable — was missing by the same
reading and was added at the same time.

**4. Update the top-level design index to Combat Bible v0.5** (P2)
> The repository-level `README.md` still advertises "Combat Bible v0.4" in its design-bible list.

The post-bump sweep searched the filename pattern `Combat_Bible_v0\.4` and reported clean; the root
README writes it as prose. **A version bump has to be swept for both the filename form and the
prose form, and only the filename form is mechanically obvious.**

### PR #3 — three findings, never replied to, all since addressed

These were posted at 09:30 and the PR merged at 10:24. No reply was made at the time. All three
were resolved by later work; their disposition is recorded here so they are not re-opened.

**1. Use the required portrait aspect for hero assets** (P2)
> Every added `hero*.webp` is 1376×768 (roughly 16:9), while this slot requires a 3:4 full-figure
> image and `.slot.filled img` uses `object-fit: cover`. Consequently, each rendered sheet retains
> only about 42% of the image width — for example, Bryn loses much of her cannon.

**Correct, and resolved in PR #4 the other way round.** The slot was changed to 16:9 to match the
art rather than 66 images regenerated to match the slot, and `object-fit` was changed to `contain`
so any future mismatch shows as letterboxing instead of as a silently missing weapon. The 42%
figure was accurate.

**2. Keep Torr on the documented Flux hue** (P2)
> `09-torr.yaml` uses the `Ancient Fluxborn system` key, so Torr's prompts and sheet receive
> `#5e7fd4`; however, the new art-direction table explicitly assigns both Flux Network and Fluxborn
> to `#4a8fd4`.

**Resolved: the table was split, not the code.** `Art_Direction_v0.1.md` now carries
`Ancient Flux Network` `#4a8fd4` and `Ancient Fluxborn system` `#5e7fd4` as separate rows, matching
`HUE` exactly. The distinction is load-bearing — Torr's own art later measured **220°** against
Fluxborn's 223°, landing on the violet side of the split without being asked to, which is the only
measurement in the whole art pass that confirmed the table rather than challenging it.

**3. Recognize all negative guardrail phrasings** (P2)
> Current canon already triggers this with Vera's "not background decoration," Mimzi's "does not
> confer," Celandrine's "not another miniature mage," and Angeru's "Not snarling".

**Correct, and fixed in PR #4** — the guardrail pattern was generalised to match plain `not` /
`does not` forms. That code has since been removed entirely with the art-prompt emitter, so the
finding is moot in the present tree; the writing rule it protected survives in
`Art_Direction_v0.1.md`.

### PR #4 — two findings, both answered and resolved

Both concerned the art-prompt emitter, and both were reproduced before being acted on. Both were
then **removed rather than fixed**, when the emitter was deleted at the author's instruction.

**1. Include `Visual` fields in the audit** (P2)
> For Mimzi, Celandrine, Gorraveth, Aurelisse, and Eudora, `parse_section` stores the appearance
> paragraph under `Visual` … but `audit()` substitutes an empty string instead. Consequently
> `--audit` can report that every appearance paragraph is clean while silently skipping all five
> entries.

Reproduced: the five field-style entries measured **0 chars**, so `--audit` was checking 20 of 25
and printing "Clean". **This invalidated PR #4's own repeated claim that the audit was clean across
the roster**, and the claim was withdrawn in the PR body rather than left standing. The fallback
was applied and verified, then deleted with `audit()` itself.

**2. Keep full-scene direction out of detail prompts** (P2)
> When `--prompts` emits the `weapon` slot, its shot instruction requests the object alone on a
> neutral background, but this condition appends `HOUSE_LIT`, which requires a full detailed
> environment behind the subject.

Reproduced — the emitted prompt contained both. The same defect as the turnaround one already
fixed in that PR; only `grp == 'turn'` had been excluded and `detail` was never checked, which is
what comes of fixing the instance instead of the class. The rule outlives the code as
**"Lighting and environment are per-slot, not global"** in `Art_Direction_v0.1.md`.

### PR #5 — no findings

## What the findings have in common

Eleven of the sixteen are the same shape: **a check, a count or a claim that was reported as clean
while not covering what it said it covered.** The validator reporting 25/25 while skipping `grants`.
The audit printing "Clean" while reading 20 of 25. The version sweep reporting clean against a
pattern that could not match prose. A hand-maintained count in a document whose whole argument is
that counts should be derived.

The lesson that keeps recurring: *running a tool beats reading a diff, but neither tells you what
the tool silently skipped.*
