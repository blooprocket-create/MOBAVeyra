# Gameplay tuning

Every gameplay balance, timing, range, cost and cap value lives here as validated text data, never in C++, Blueprints or binary assets ([ARCHITECTURE.md](../../ARCHITECTURE.md) §1.3, [ADR-006](../../Docs/ADR/ADR-006-unreal-project-scaffold.md) §6).

## Files

- `<Domain>.json` holds one owning domain's tuning, for example `Combat.json` for the Combat domain. The owning module loads it at startup.
- `Schemas/<Domain>.schema.json` describes it. Every data file has exactly one schema, and every schema has a data file.
- Both are UTF-8 without a byte-order mark and use LF line endings (`.gitattributes` enforces LF here). The game hashes the data file's exact bytes to compare tuning between builds.

| File | Owner | Holds |
|---|---|---|
| `Combat.json` | `VeyraCombat` | Resistance mitigation, targeting, regeneration, movement and forced movement, crowd control, Combat State, assist attribution, Attack Speed, and moving toward enemy Vanguards. |
| `Progression.json` | `VeyraEconomy` | Levels, the XP curve, skill points, maximum ranks and the levels that open ultimate ranks. |
| `Abilities.json` | `VeyraAbilities` | Casting rules, statuses, and one map per ability archetype ([ADR-008](../../Docs/ADR/ADR-008-vanguard-definitions-and-ability-composition.md) §3). |
| `Vanguards.json` | `VeyraVanguards` | Vanguard definitions, and one map per unique passive (ADR-008 §2, §5). |
| `Match.json` | `VeyraMatch` | Teams, phases, respawn, abandonment, order limits and the developer match. |

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

A reference from one domain's file to content another domain defines is checked by the loading domain in the game and by the reference table in `scripts/check_tuning.py` ([ADR-008](../../Docs/ADR/ADR-008-vanguard-definitions-and-ability-composition.md) §7). For example, each Vanguard in `Match.json`'s developer order must be one `Vanguards.json` defines, and each ability in a Vanguard's kit must be defined by one of `Abilities.json`'s archetype maps. In the table, a `*` in a pointer stands for every key or item there, and a reference may name several maps, any of which may define the ID.

An ability is defined in exactly one archetype map, and a passive in exactly one passive map, since the map chooses the code that runs it.

## Provenance

Every record whose values were drafted rather than taken from canon carries `"provenance": "Provisional"`; canon values carry `"Canon"`, and the author marks reviewed drafts `"Reviewed"` ([ADR-008](../../Docs/ADR/ADR-008-vanguard-definitions-and-ability-composition.md) §7). `scripts/check_tuning.py` reports how many provisional records remain, and `--list-provisional` lists each one by file and JSON pointer, for review.

A provisional value is a playtest setting, not a balance decision. Retuning one is a data change: tests compute their expectations from the loaded tuning.
