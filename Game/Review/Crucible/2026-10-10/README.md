# Crucible capture correction — 2026-10-10

Pass 09 isolates the low-profile checkerboard to capture LOD handling. The
project capture script now sets `r.ForceLOD=-1` at console priority before
material warm-up, preventing HighResShot's temporary LOD 0 override. The manifest
records the effective LOD setting and any diagnostic commands.

| Previous capture method | Distance-based LOD capture |
|---|---|
| ![Before](low-overview-before.png) | ![After](low-overview-after.png) |

Original 1920 × 1080 Unreal captures; image bytes are unmodified. Both use the
low profile and the same overview camera and content. Detail selection differs,
so this is evidence of a capture correction, not an art improvement. Sources and
SHA-256 hashes are recorded in `evidence.json`. All content is Veyra-authored;
no third-party visual content was introduced.

## Investigation and verification

- Longer warm-up and setting low scalability before map load retained the issue.
- Separate fresh sessions with LOD 0 prepared before capture and with normal
  distance-based LOD selection each produced a fully textured overview.
- The final standard script captured Overview and Play_Jungle_A0 on low/high;
  inspect the local Pass09 comparison for all four frames.
- Python syntax, documentation routing and whitespace checks pass.
- PowerShell waits for the editor itself rather than persistent service children.

Owner: `Game/Plugins/VeyraWorldTools/Scripts/CaptureCrucible.py` and
`Game/Scripts/CaptureBattleground.ps1`; method authority is
[World Validation Standard §13](../../../../Docs/Production/VEYRA_WORLD_VALIDATION_STANDARD.md#13-standard-screenshot-suite).
No new dependencies, gameplay tuning/schema, networking, replication, layout,
protected geometry, asset or engine changes. Existing content seed: 48173.
Architecture review: editor-only automation observes generated content and
preserves gameplay ownership.

Reproduce with `Game/Scripts/CaptureBattleground.ps1 -Profile low -Views
'Overview,Play_Jungle_A0'`, then repeat with `-Profile high`.

The earlier low-profile warning is resolved for these editor captures only.
Live-match, temporal, streaming, GPU and packaged-client acceptance remain open.
Residual river glare, repeated cliff bands and foliage form repetition remain
art priorities. Older captures use different LOD handling; establish new
baselines for future visual comparisons.
