# Veyra Design Bibles

This directory contains the **current working canon** for Veyra's game design. Contributors and coding agents should use these files as the default design references.

## Current documents

- `Veyra_Initial_Roster_Character_Bible_v0.5.md` (current roster target: 20 Vanguards; expands as new characters are designed)
- `Veyra_World_Bible_v0.4.md` (includes Cairn, Tavra, and Vitra region/lore anchors)
- `Veyra_Battleground_Bible_v0.9.md`
- `Veyra_Item_Bible_v0.3.md`
- `Veyra_Combat_Bible_v0.4.md`
- `Veyra_Vision_Bible_v0.1.md`
- `Veyra_Economy_Progression_Bible_v0.1.md`
- `Veyra_Match_Flow_Bible_v0.1.md`
- `Veyra_Account_Collection_Mastery_Bible_v0.1.md` (persistent account XP, currencies, ownership, Collection, uncapped Mastery)
- `Veyra_Modes_Access_Bible_v0.1.md` (queues, Ranked entry, free weekly rotation, Co-op vs AI)
- `Veyra_Client_Platform_Bible_v0.1.md` (launcher, pre-game client, Unreal match client and reconnect handoff)
- `Veyra_Parties_Social_Matchmaking_Bible_v0.1.md` (parties, queue readiness/acceptance, friends/presence, strict blocks, future Ranked party target)
- `Veyra_Profiles_Identity_Bible_v0.1.md` (public profiles, customization, account creation, unique names and name claims)
- `Veyra_Moderation_Telemetry_Bible_v0.1.md` (participant reports, staff decisions/appeals, chat-evidence privacy, logging and telemetry)
- `Veyra_Replay_Spectator_Bible_v0.1.md` (playable recording, public links, per-account saves, retention and delayed live friend spectating)

Tavra's Sounding Flare uses Dense Fog **presence information**, not remote sight or targeting; the character bible does not create an exception to the Vision Bible. **Vitra's Gathering Light homes only onto a legally acquirable enemy hit by her spell**: blind hits into Dense Fog still deal ordinary untargeted spell damage, but cannot generate a remote-targeted fragment. **Cairn's Q pull triggers Deep Foundation, his R pays out its own capped shield without a duplicate passive proc, and his E reduces displacement distance rather than conferring immunity. Tavra's empowered third Breach hit has exactly one upgraded secondary explosion**, and her personal attack-interval floor does not add a separate Attack Speed overflow threshold. The Vision Bible is the current reference for vision tools and detailed Dense Fog detection interactions; the Battleground Bible still defines the map and its fog volumes. The Economy & Progression Bible is the current reference for individual Gold/XP, leveling, item shopping/delivery, and buyback. The Battleground Bible includes the current free prematch Flux Spell preselection, wave and jungle prototype schedules, inhibitors, base towers, and Prime Well rules. The Combat Bible owns tower targeting, minion aggression, and backdoor damage behavior. The Match Flow Bible owns champion-select cancellation, preparation, AFK/disconnect, voting, pause/resume, and match results. The Parties & Social Bible owns pre-select match-found confirmation and party/requeue/block rules. The Profiles & Identity Bible owns globally unique names and the website's account-creation entry point. The Moderation Bible owns staff-reviewed reports and appeals; the Replay Bible owns playback, storage/availability and the three-minute live spectator delay. The Client & Platform Bible coordinates application flows without duplicating these owners.

The account, modes, client, parties, profiles/identity, moderation/telemetry, and replay/spectator bibles capture **decisions reached so far**, not a finished design or permission to begin implementation. **Ranked is intentionally deferred from initial launch**; its documented selection, Account Level 30/20-owned entry gates, 1–5-person parties and adjacent-rank party restrictions are future targets. The new persistent earned account currency is **Flux** and the premium account currency is **Refined Flux**; these are distinct from in-match **Team Flux** and Gold. The current roster target is 20 designed Vanguards, satisfying the draft's global 16-distinct-Vanguard minimum on paper. Ranked remains deferred: the separate 20-permanently-owned-Vanguard entry gate requires at least 20 **released and ownable** Vanguards, sufficient account ownership, and completion of the other Ranked systems; 20 character designs alone do not launch Ranked. Superseded 17-roster readiness statements in archived versions are historical. The Match Flow Bible's shorthand for ten human loading connections applies to PvP; Co-op vs AI requires five human connections and five server-controlled enemies.

Older superseded versions are preserved under [`Archives/`](Archives/). They exist for design history and comparison only and must not be treated as current implementation requirements.

The original Word documents include richer layout and concept-art presentation. These Markdown exports prioritize searchable design content and may omit embedded images.

Design bibles are working canon and may evolve. Architecture rules live separately in the repository root and are not overridden by incidental implementation suggestions inside a design document.
