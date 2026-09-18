# ADR-001: Unreal Engine version policy

**Status:** Accepted  
**Date:** 2026-09-18

## Context

Veyra needs a fixed engine target before the Unreal project is scaffolded. Leaving the version unresolved would allow different contributors or coding agents to create incompatible project files, APIs, plugins, build settings, and assets.

As of this decision, Unreal Engine 5.8 is Epic's current stable Unreal Engine release.

## Decision

Veyra will begin development on **Unreal Engine 5.8**, using the newest stable production release available when the project is initially scaffolded.

The repository is **version-pinned**, not floating. Once the project exists, contributors and coding agents must use the engine version declared by the project/repository rather than silently upgrading it.

Future stable Unreal releases may be adopted deliberately, but an engine upgrade must be treated as an explicit project migration with build, test, plugin, networking, dedicated-server, and asset validation.

A future move to Unreal Engine 6 is not implied by this ADR and requires a separate deliberate migration decision.

Preview, Beta, or Experimental engine releases must not replace the project's production engine version without a new ADR.

## Consequences

- Initial project files and build tooling target UE 5.8.
- Documentation, generated project files, CI, and local setup should agree on that engine version.
- Agents must not silently change the engine association to fix a task.
- Engine upgrades happen intentionally rather than opportunistically.
- If an Epic feature is Experimental, its presence in UE 5.8 does not automatically approve it for production use.

## Alternatives considered

### Leave the version open until later
Rejected because project scaffolding itself depends on the version.

### Track the newest available build automatically
Rejected because silent engine migrations would make builds and assets unstable.

### Use an older UE5 release
Rejected because the project is pre-production and has no legacy compatibility requirement that would justify starting behind the current stable release.
