# Gameplay tuning

Every gameplay balance, timing, range, cost and cap value lives here as validated text data, never in C++, Blueprints or binary assets ([ARCHITECTURE.md](../../ARCHITECTURE.md) §1.3, [ADR-006](../../Docs/ADR/ADR-006-unreal-project-scaffold.md) §6).

## Files

- `<Domain>.json` holds one owning domain's tuning, for example `Combat.json` for the Combat domain. The owning module loads it at startup.
- `Schemas/<Domain>.schema.json` describes it. Every data file has exactly one schema, and every schema has a data file.
- Both are UTF-8 without a byte-order mark and use LF line endings (`.gitattributes` enforces LF here). The game hashes the data file's exact bytes to compare tuning between builds.

## Loading rules

Loading fails explicitly. There are no silent defaults:

- every field in the schema is required, and the document may contain no other field;
- values must have the declared type (`1.0` is not an integer) and lie within the declared range;
- duplicate keys, comments, trailing commas, `NaN` and `Infinity` are errors;
- the document's `schemaVersion` must match the version the build reads.

The game (`VeyraTuning.cpp` in VeyraCore) and CI (`scripts/check_tuning.py`) apply the same rules. A shared corpus (`Game/Source/VeyraDeveloper/TestData`) keeps the two in agreement.

## Schema dialect

Schemas use a strict subset of JSON Schema draft-04:

| Schema type | Keywords |
|---|---|
| any | `$schema`, `title`, `description`, `type` |
| `object` | `properties`, `required` (must list every property), `additionalProperties` (must be `false`) |
| `number`, `integer` | `minimum` (required), `maximum`, `exclusiveMinimum`, `exclusiveMaximum` (draft-04 booleans), `enum` |

- The root declares `schemaVersion` as an integer with a one-value `enum`.
- A JSON key is its struct field's name with the first letter lower-cased: `mitigationConstant` binds to `MitigationConstant`.
- A `number` binds to a `double` and an `integer` to an `int32`. The schema and the struct must describe exactly the same fields.
- Describe each value's meaning and cite the canon section it tunes in its `description`.

## Content IDs

When tuning refers to content (a Vanguard, an item, a status), it uses a stable content ID: lowercase ASCII snake_case matching `^[a-z][a-z0-9]*(_[a-z0-9]+)*$`, such as `raska` or `code_black_zone`. IDs never change when display names do.
