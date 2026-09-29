# Gameplay tuning

Every gameplay balance, timing, range, cost and cap value lives here as validated text data, never in C++, Blueprints or binary assets ([ARCHITECTURE.md](../../ARCHITECTURE.md) §1.3, [ADR-006](../../Docs/ADR/ADR-006-unreal-project-scaffold.md) §6).

## Files

- `<Domain>.json` holds one owning domain's tuning, for example `Combat.json` for the Combat domain. The owning module loads it at startup.
- `Schemas/<Domain>.schema.json` describes it. Every data file has exactly one schema, and every schema has a data file.
- Both are UTF-8 without a byte-order mark and use LF line endings (`.gitattributes` enforces LF here). The game hashes the data file's exact bytes to compare tuning between builds.

| File | Owner | Holds |
|---|---|---|
| `Combat.json` | `VeyraCombat` | Resistance mitigation, targeting, regeneration, movement and forced movement, crowd control, Combat State, assist attribution, kill credit, Structure Effectiveness, Attack Speed, and moving toward enemy Vanguards. |
| `Progression.json` | `VeyraEconomy` | Levels, the XP curve, skill points, maximum ranks and the levels that open ultimate ranks. |
| `Economy.json` | `VeyraEconomy` | Starting and passive Gold, and the Gold and XP that Fluxborn, Vanguard kills and structures pay: last hits, participation, assists, First Blood, structure pools, the Team Flux bonus, and who is near and recent enough to share (ADR-011 §11, §17); and what swapping a Flux Spell at the fountain costs ([ADR-015](../../Docs/ADR/ADR-015-flux-spells.md) §6). |
| `Bots.json` | `VeyraBots` | How AI Vanguards play: what they notice, where they stand, the Beginner and Intermediate behaviours, the role of each seat (a lane or the jungle) and the Flux Spells it takes, what each Flux Spell is for ([ADR-015](../../Docs/ADR/ADR-015-flux-spells.md) §8), how a jungler plays, and each Vanguard's build, skill priority and ability uses ([ADR-013](../../Docs/ADR/ADR-013-ai-vanguards.md) §5). |
| `Items.json` | `VeyraItems` | The shop's catalog and rules: each item's tier, recipe, cost, stats, Active and Attunement; consumables; the Attunements' values; inventory slots, resale, uniqueness and the Boots limit ([ADR-012](../../Docs/ADR/ADR-012-items-and-shop.md) §3, §10). |
| `Abilities.json` | `VeyraAbilities` | Casting rules, statuses, one map per ability archetype ([ADR-008](../../Docs/ADR/ADR-008-vanguard-definitions-and-ability-composition.md) §3), and the Flux Spell roster, whose spells are entries of those maps ([ADR-015](../../Docs/ADR/ADR-015-flux-spells.md) §3). |
| `Vanguards.json` | `VeyraVanguards` | Vanguard definitions, and one map per unique passive (ADR-008 §2, §5). |
| `Flux.json` | `VeyraFlux` | What each source of Team Flux grants, how active Team Flux strengthens Fluxborn ([ADR-011](../../Docs/ADR/ADR-011-battleground-runtime.md) §10), and the permanent Team Flux that unlocks each Flux Spell slot ([ADR-015](../../Docs/ADR/ADR-015-flux-spells.md) §4). |
| `Vision.json` | `VeyraVision` | How often the server works out what each team and player sees, how far each kind of unit sees, and the vision tools: the Persistent Ward's charges and wards ([ADR-016](../../Docs/ADR/ADR-016-vision.md) §2, §6, §9). |
| `World.json` | `VeyraWorld` | The battleground's grey-box layout, which the map commandlet bakes and the server spawns from; structures, inhibitor rebuilds, Prime Well regeneration, tower attacks and backdoor protection; the Fluxborn, how they think, and the wave schedule (ADR-011 §7–§9, §12, §17). |
| `Match.json` | `VeyraMatch` | Teams, phases, respawn, fountain recovery, Recall's channel ([ADR-012](../../Docs/ADR/ADR-012-items-and-shop.md) §8), abandonment, order limits, and the developer match. |

What players read about Vanguards, their abilities and passives is text, not tuning: it lives in `Game/Text/VeyraText.csv`, a string table VeyraUI reads, and carries no numbers.

## Loading rules

Loading fails explicitly. There are no silent defaults:

- every field in the schema is required, and the document may contain no other field;
- values must have the declared type (`1.0` is not an integer) and lie within the declared range;
- duplicate keys, comments, trailing commas, `NaN` and `Infinity` are errors;
- the document's `schemaVersion` must match the version the build reads.

The game (`VeyraTuning.cpp` in VeyraCore) and CI (`scripts/check_tuning.py`) apply the same rules. A shared corpus (`Game/Source/VeyraDeveloper/TestData`) keeps the two in agreement.

## Schema dialect

Schemas use a strict subset of JSON Schema draft-04:

| Schema form | Keywords | Binds to |
|---|---|---|
| any | `$schema`, `title`, `description`, `type` | |
| record: `object` | `properties`, `required` (must list every property), `additionalProperties` (must be `false`) | a `USTRUCT` |
| map: `object` | `patternProperties` (exactly one entry, the content ID format), `additionalProperties` (must be `false`) | `TMap<FVeyraContentId, Value>` |
| `number` | `minimum` (required), `maximum`, `exclusiveMinimum`, `exclusiveMaximum` (draft-04 booleans), `enum` | `double` |
| `integer` | the same as `number` | `int32` |
| enum: `string` | `enum`, listing exactly the enum's values as C++ spells them (`"Magic"`) | an `enum class` `UENUM` |
| content ID: `string` | `pattern`, which must be the content ID format | `FVeyraContentId` |
| text: `string` | `pattern` (required), anchored with `^` and `$`; the whole text must match | `FString` |
| `array` | `items` (one schema for every item), `minItems` (required), `maxItems` | `TArray` of any form above except a map |
| reference | `$ref` (`"#/definitions/<name>"`), and optionally `description` | whatever the named definition binds to |

- The root is a record and declares `schemaVersion` as an integer with a one-value `enum`.
- A record used in several places is declared once under the root's `definitions`, and each use is a reference to it ([ADR-008](../../Docs/ADR/ADR-008-vanguard-definitions-and-ability-composition.md) §7). A definition may refer to another, but not round a cycle. Only the root declares `definitions`, and a reference must use each one.
- A JSON key is its struct field's name with the first letter lower-cased: `mitigationConstant` binds to `MitigationConstant`. The editor checks the exact spelling. A cooked build keeps one spelling per engine name (the first registered, so a field `MatchId` can read back as `MatchID`), so there the key matches its field ignoring case. A document's keys must still match its schema's exactly.
- The schema and the struct must describe exactly the same fields. A map may be empty, and any valid content ID may be a key.
- No string or array is unbounded by accident: text always declares its format, and an array always declares `minItems`.
- An optional value is an array with `minItems` 0 and `maxItems` 1: `[]` means none. There is no `null`.
- A value that grows with an ability's rank is an array whose name ends in `ByRank`: one item means the same at every rank, otherwise one item per rank (ADR-008 §3).
- The game matches text patterns with ICU and CI with Python's `re`, so a pattern keeps to the syntax both share: literal characters, character classes (`[A-Za-z0-9 ]`, `[^...]`), `\d`-style escapes, quantifiers (`*`, `+`, `?`, `{m,n}`), groups and alternation. Put alternation inside a group (`^(a|b)$`, not `^a|b$`).
- These forms also describe documents that are not tuning, such as the roster a match server receives ([ADR-007](../../Docs/ADR/ADR-007-match-join-contract.md) §5). `scripts/check_tuning.py` lists each such schema in `CONTRACTS`, with an example that the other side of the contract writes, and validates the example in CI. These documents are not part of the tuning hash.
- Describe each value's meaning and cite the canon section it tunes in its `description`.

## Content IDs

When tuning refers to content (a Vanguard, an item, a status), it uses a stable content ID: lowercase ASCII snake_case matching `^[a-z][a-z0-9]*(_[a-z0-9]+)*$`, such as `raska` or `code_black_zone`. IDs never change when display names do. In C++ the type is `FVeyraContentId` (VeyraCore).

A reference from one domain's file to content another domain defines is checked by the loading domain in the game and by the reference table in `scripts/check_tuning.py` ([ADR-008](../../Docs/ADR/ADR-008-vanguard-definitions-and-ability-composition.md) §7). For example, each Vanguard in `Match.json`'s developer order must be one `Vanguards.json` defines, and each ability in a Vanguard's kit must be defined by one of `Abilities.json`'s archetype maps. In the table, a `*` in a pointer stands for every key or item there, a final `#` stands for every key of a map (so `Economy.json`'s Fluxborn prices must name Fluxborn `World.json` defines), and a reference may name several maps, any of which may define the ID. When the loading domain's layer cannot see the other domain, as Economy cannot see World, a test of the committed tuning checks the reference in the game instead.

An ability is defined in exactly one archetype map, and a passive in exactly one passive map, since the map chooses the code that runs it.

## Provenance

Every record whose values were drafted rather than taken from canon carries `"provenance": "Provisional"`; canon values carry `"Canon"`, and the author marks reviewed drafts `"Reviewed"` ([ADR-008](../../Docs/ADR/ADR-008-vanguard-definitions-and-ability-composition.md) §7). `scripts/check_tuning.py` reports how many provisional records remain, and `--list-provisional` lists each one by file and JSON pointer, for review.

A provisional value is a playtest setting, not a balance decision. Retuning one is a data change: tests compute their expectations from the loaded tuning.
