# Vanguard structural data

One YAML file per Vanguard, derived from [`Veyra_Initial_Roster_Character_Bible_v0.6.md`](../Veyra_Initial_Roster_Character_Bible_v0.6.md), plus a validator that checks them against the bibles.

```
python3 Docs/Design/Vanguards/validate.py
```

## What this is for

The Character Bible is prose. Prose cannot be diffed meaningfully, cannot be validated, and cannot tell you that a character sheet has drifted away from it — which is how the conflicts in [`../Sheet_Canon_Discrepancy_Register_v0.1.md`](../Sheet_Canon_Discrepancy_Register_v0.1.md) accumulated unnoticed.

These files hold the **structural** facts from each kit in a form a script can check:

- the crowd control a kit applies, validated against the Combat Bible's own vocabulary;
- the persistent entities it puts on the battlefield, categorised per [ADR-003](../../ADR/ADR-003-owned-field-entities.md);
- every point where a kit touches Dense Fog, stealth or target acquisition;
- the **guards** — the load-bearing rules that exist to stop an implementation quietly becoming more powerful than canon intends;
- which concept sheet, if any, is currently safe to work from.

The bible stays the authority on intent, fiction and nuance. These files are the machine-checkable subset, and the register is what they are checked against.

## What must never go in here

**No gameplay tuning.** No cooldowns, durations, ratios, damage values, ranges, radii, speeds, costs, caps or thresholds. `ARCHITECTURE.md` §1.3 requires all of that to live in validated, designer-editable engine data, and a YAML file in `Docs/` is not that.

The validator enforces this rather than trusting it, in three ways: any numeric value outside `roster_number` fails the run; so does any scalar that is entirely a quantity however it is typed, including the string form (`"1200"`, `"10 metres"`, `"50%"`); and so does any tuning-shaped key name, `range` among them. `range_class` is unaffected, being structural rather than a quantity.

Explanatory prose in `guards` and `note` is exempt, because it describes rules rather than configuring them.

Structural counts that canon fixes as *mechanics* rather than balance — Mimzi's two-stack preparatory cap, Oriel's three stacks, Bryn's third-hit Breach — are recorded in prose inside `guards`, not as numeric fields.

## Relationship to the engine

There is no Unreal project in this repository yet. When there is, Vanguard Data Assets belong under `Content/Veyra/Vanguards/` per [`PROJECT_STRUCTURE.md`](../../../PROJECT_STRUCTURE.md), and they own the tuning. These files are the **design-canon source** those assets should be generated from or validated against — they are not a substitute for them and do not preempt that layout.

## Schema

| Field | Notes |
|---|---|
| `id`, `roster_number`, `name`, `title` | `title` may be `null` — Silt currently has none |
| `canon_section` | section in the Character Bible |
| `origin_region`, `origin_locality`, `nature` | `nature` from a controlled list; `origin_locality` may be `null` |
| `role_tags` | descriptive archetypes. **Positional labels are rejected** — the roster enforces no role slots |
| `terrain_affinity` | where a kit is conditionally stronger (Moro, Gorraveth). This is where a jungle preference goes |
| `damage_profile`, `basic_attack`, `resource` | Mimzi's basic attacks are `magic`; Angeru and Relay use non-standard resources |
| `ability_layout` | `standard`, `stance` (Angeru), or `stance_modal` (Neris) |
| `abilities` | slot → canonical ability name |
| `marks` | named marks, stacks or meters the kit applies |
| `cc` | **validated against the Combat Bible's "Core CC types" list**, parsed at run time so it cannot drift |
| `mobility`, `stealth` | controlled lists |
| `owned_entities` | `id`, `category` (ADR-003), `destructible`, `note` |
| `vision_touchpoints` | `ability`, `effect`, `status` (`canon` / `unresolved` / `needs_classification`), `note`. Non-canon statuses raise a warning |
| `guards` | free prose. The rules that must survive implementation |
| `grants` | validated — the roster summary derives its sustain conclusions from these, so a typo would silently change the reported totals |
| `sheet` | `file`, `status`, `register_refs`. See the status table below — `withdrawn`, `superseded` and `missing` mean there is no sheet and require `file: null`; every other status must name a file that exists on disk. A status that asserts a *problem* must carry at least one `register_refs` entry, and every entry is checked against the register's actual section ids |

### `sheet.status`

The 22 baked-text sheets were deleted on 2026-09-21, so no entry names a file today. The
vocabulary is kept because **why** a sheet is absent is the part that matters.

| Status | Means | `file` | Needs a `register_ref` |
|---|---|---|---|
| `current` | The sheet is usable as-is | a real file | no |
| `contradicts_canon` | The sheet exists but disagrees with canon | a real file | **yes** |
| `incomplete` | The sheet exists but omits something load-bearing | a real file | **yes** |
| `withdrawn` | Pulled for third-party content, and stripped from Git history | `null` | **yes** |
| `superseded` | Retired because authored art replaced it, then deleted | `null` | no |
| `missing` | There never was a sheet | `null` | **yes** |

`superseded` is exempt from the register-ref rule because that rule exists to make a *problem*
point at its documentation, and being replaced by better art is not a problem. Five entries —
Torr, Qazharr, Gorraveth, Aurelisse and Eudora — never carried a conflict to cite, and requiring
one would have meant inventing it.

`withdrawn` and `superseded` both end at `file: null` and are deliberately **not** merged: one
records a legal problem, the other records housekeeping.

## What the validator reports

Beyond pass/fail it prints a roster summary derived from the data, which is the point — these are the questions that are tedious to answer from prose and trivial to answer from data:

**Owned field entities**, the ADR-003 inventory. Currently 3 combat units, 5 placed markers, 1 ride state, and **18 world volumes** across 9 Vanguards. World volumes are by a wide margin the largest category, and two of them are engine capabilities rather than ability features: Varkesh's Iron Wall modifies pathing for both teams, and Sylra creates true Dense Fog at runtime. Neither has a ruling in any bible.

**Crowd control coverage.** 6 Vanguards apply no CC at all; 13 apply no hard CC. Combined with open composition, a legal team can field none.

**Sustain.** Ally healing exists on exactly one Vanguard. Ally shielding exists on two. Any composition wanting sustain must pick Neris, which is a constraint the "no enforced roles" principle does not otherwise imply.

**Marks and meters.** 18 of 25 kits apply a named mark, stack or meter. The fiction varies; the mechanical shape often does not.

**Sheet status.** 12 of 25 Vanguards have no sheet, or a sheet that contradicts or under-describes canon.

These numbers are descriptive, not verdicts. They are here so the trade-offs are visible when the roster changes, rather than discovered during balance.

## Rendering a sheet

```
python3 Docs/Design/Vanguards/render_sheet.py bryn     # one
python3 Docs/Design/Vanguards/render_sheet.py --all    # all 25
```

`render_sheet.py` builds a self-contained HTML character sheet by reading the Character Bible and the Vanguard's YAML **at render time**. Nothing is typed by hand and nothing is copied into a third location, so there is no place for the text to drift from canon.

**Artwork is deliberately not part of the output.** Each image region renders as a labelled slot naming what belongs there and at what aspect, so the sheet doubles as the brief for the art that fills it. That separation is the point: text baked into a generated image cannot be corrected, validated or version-controlled, and that is how every render defect in `ConceptArt/` arrived — `CADNCE`, `a Heated meemy`, `substantail`, `Vangaurd` — along with half the canon drift in the discrepancy register. Here a canon change is picked up by re-running the renderer, and the expensive layer never moves.

The parser handles all three heading generations in the bible, including Angeru's stance layout, which yields eight ability entries rather than five.

`sheets/19-bryn.html` is committed as a worked example; the rest are build output and ignored. Bryn is the useful demonstration because her sheet is wrong in `ConceptArt/` — the PNG shows "Harbor Flare" reducing Dense Fog, and the rendered sheet shows "Sounding Flare" with presence-only behaviour, because it reads §19 rather than a year-old render.

## Changing a Vanguard

1. Update the Character Bible first. It remains the authority.
2. Update the YAML file to match.
3. Run the validator. It cross-checks the name **and** title against the bible's roster table, so renaming in one place and not the other fails the run.
4. If a concept sheet is now wrong, record it in the discrepancy register and set `sheet.status`.
