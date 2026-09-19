# Veyra Design Bibles

This directory contains the **current working canon** for Veyra's game design. Contributors and coding agents should use these files as the default design references.

## Current documents

- `Veyra_Initial_Roster_Character_Bible_v0.4.md`
- `Veyra_World_Bible_v0.3.md`
- `Veyra_Battleground_Bible_v0.9.md`
- `Veyra_Item_Bible_v0.3.md`
- `Veyra_Combat_Bible_v0.4.md`
- `Veyra_Vision_Bible_v0.1.md`
- `Veyra_Economy_Progression_Bible_v0.1.md`
- `Veyra_Match_Flow_Bible_v0.1.md`
- `Veyra_Account_Collection_Mastery_Bible_v0.1.md` (persistent account XP, currencies, ownership, Collection, uncapped Mastery)
- `Veyra_Modes_Access_Bible_v0.1.md` (queues, Ranked entry, free weekly rotation, Co-op vs AI)
- `Veyra_Client_Platform_Bible_v0.1.md` (launcher, pre-game client, Unreal match client and reconnect handoff)

The Vision Bible is the current reference for vision tools and detailed Dense Fog detection interactions; the Battleground Bible still defines the map and its fog volumes. The Economy & Progression Bible is the current reference for individual Gold/XP, leveling, item shopping/delivery, and buyback. The Battleground Bible includes the current free prematch Flux Spell preselection, wave and jungle prototype schedules, inhibitors, base towers, and Prime Well rules. The Combat Bible owns tower targeting, minion aggression, and backdoor damage behavior. The Match Flow Bible owns champion-select cancellation, preparation, AFK/disconnect, voting, pause/resume, and match results.

The three account/mode/client v0.1 bibles capture **decisions reached so far**, not a finished design or permission to begin implementation. The Modes & Access Bible supersedes the older Battleground v0.9 statement that a 17-Vanguard roster makes Ranked player-accessible: it is mathematically large enough for the draft, but the separately locked 20-owned-Vanguard Ranked gate also requires at least 20 ownable Vanguards. The Match Flow Bible's shorthand for ten human loading connections applies to PvP; Co-op vs AI requires five human connections and five server-controlled enemies.

Older superseded versions are preserved under [`Archives/`](Archives/). They exist for design history and comparison only and must not be treated as current implementation requirements.

The original Word documents include richer layout and concept-art presentation. These Markdown exports prioritize searchable design content and may omit embedded images.

Design bibles are working canon and may evolve. Architecture rules live separately in the repository root and are not overridden by incidental implementation suggestions inside a design document.
