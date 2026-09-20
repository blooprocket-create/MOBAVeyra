# Veyra Design Bibles

This directory contains the **current working canon** for Veyra's game design. Contributors and coding agents should use these files as the default design references.

## Current documents

- `Veyra_Initial_Roster_Character_Bible_v0.6.md` (25 designed Vanguards, including Mimzi, Celandrine, Gorraveth, Aurelisse, and Eudora; full kits and lore)
- `Veyra_World_Bible_v0.5.md` (adds Bramblehollow, Thistlewick, the Root Vaults, and expanded Ember Basin, Shatterdeep, and Iron March lore)
- `Veyra_Battleground_Bible_v0.9.md`
- `Veyra_Item_Bible_v0.3.md`
- `Veyra_Combat_Bible_v0.5.md` (adds §56 Ride states)
- `Veyra_Vision_Bible_v0.1.md`
- `Veyra_Economy_Progression_Bible_v0.1.md`
- `Veyra_Match_Flow_Bible_v0.1.md`
- `Veyra_Account_Collection_Mastery_Bible_v0.1.md` (persistent account XP, currencies, ownership, Collection, uncapped Mastery)
- `Veyra_Modes_Access_Bible_v0.1.md` (queues, Ranked entry, free weekly rotation, Co-op vs AI)
- `Veyra_Custom_Matches_Bible_v0.1.md` (invite-only sandbox, adjustable rules, open-ended host-ended solo/team practice, selectable AI and invited spectator-only seats with host-selected three-minute or zero-delay viewing)
- `Veyra_Client_Platform_Bible_v0.1.md` (launcher, pre-game client, Unreal match client and reconnect handoff)
- `Veyra_Parties_Social_Matchmaking_Bible_v0.1.md` (parties, queue readiness/acceptance, friends/presence, strict blocks, future Ranked party target)
- `Veyra_Chat_Communication_Bible_v0.1.md` (text-only Team/All/Party/DM/post-match channels; All Chat default-on with per-player Settings off switch; muting and replay privacy)
- `Veyra_Profiles_Identity_Bible_v0.1.md` (public profiles, customization, account creation, unique names and name claims)
- `Veyra_Moderation_Telemetry_Bible_v0.1.md` (participant reports, staff decisions/appeals, chat-evidence privacy, logging and telemetry)
- `Veyra_Replay_Spectator_Bible_v0.1.md` (playable recording, public links, per-account saves, retention and delayed live friend spectating)

Mimzi uses normal Camouflage and her homing ultimate bolts cannot acquire hidden Vanguards from outside Dense Fog. Celandrine's piercing follow-throughs are not additional basic attacks. Gorraveth's rapid jungle clear changes neither camp identities nor lane freedom. Aurelisse's displacement and shielding obey normal Combat rules, and Eudora's Picket cannot use Designation to gain remote sight or target enemies in Dense Fog. Bryn's Sounding Flare uses Dense Fog **presence information**, not remote sight or targeting; the character bible does not create an exception to the Vision Bible. **Oriel's Gathering Light homes only onto a legally acquirable enemy hit by her spell**: blind hits into Dense Fog still deal ordinary untargeted spell damage, but cannot generate a remote-targeted fragment. **Cairn's Q pull triggers Deep Foundation, his R pays out its own capped shield without a duplicate passive proc, and his E reduces displacement distance rather than conferring immunity. Bryn's empowered third Breach hit has exactly one upgraded secondary explosion**, and her personal attack-interval floor does not add a separate Attack Speed overflow threshold. The Vision Bible is the current reference for vision tools and detailed Dense Fog detection interactions; the Battleground Bible still defines the map and its fog volumes. The Economy & Progression Bible is the current reference for individual Gold/XP, leveling, item shopping/delivery, and buyback. The Battleground Bible includes the current free prematch Flux Spell preselection, wave and jungle prototype schedules, inhibitors, base towers, and Prime Well rules. The Combat Bible owns tower targeting, minion aggression, and backdoor damage behavior. The Match Flow Bible owns champion-select cancellation, preparation, AFK/disconnect, voting, pause/resume, and match results. The Parties & Social Bible owns pre-select match-found confirmation and party/requeue/block rules. The Chat & Communication Bible owns **no built-in voice chat**, live Team and default-on All Chat (individually disabled in Settings without disabling Team/Party), persistent Party Chat, in-match friend DMs, optional nonblocking cross-team post-match chat, muting and public spectator/replay chat exclusion. A substantial independent Settings Bible remains to be designed. The Custom Matches Bible owns separate invite-only host-run lobbies: human picks remain owned-or-rotation; host-assigned AI may use any released Vanguard and per-bot difficulty; sessions may launch below ten participants, including 1v0 solo practice; the host may disable normal victory and manually end a noncompetitive practice session, which confers no Account XP/Mastery. The Profiles & Identity Bible owns globally unique names and the website's account-creation entry point. The Moderation Bible owns staff-reviewed reports and appeals; the Replay Bible owns playback, storage/availability and **three-minute matchmade friend-spectator delay**; only a host-authorized spectator-only seat in an invite-only custom match may use that custom host's **pre-match zero-delay option**. The Client & Platform Bible coordinates application flows without duplicating these owners.

The account, modes, client, parties, profiles/identity, moderation/telemetry, and replay/spectator bibles capture **decisions reached so far**, not a finished design or permission to begin implementation. **Ranked is intentionally deferred from initial launch**; its documented selection, Account Level 30/20-owned entry gates, 1–5-person parties and adjacent-rank party restrictions are future targets. The new persistent earned account currency is **Flux** and the premium account currency is **Refined Flux**; these are distinct from in-match **Team Flux** and Gold. The current roster target is **25 designed Vanguards**, satisfying the draft's global 16-distinct-Vanguard minimum on paper. Ranked remains deferred: the separate **20 permanently owned Vanguards per eligible player** entry gate requires at least 20 **released and ownable** Vanguards plus completion of the other Ranked systems. Designing 25 does not itself establish release or launch Ranked. Superseded 17-roster readiness statements in archived versions are historical. The Match Flow Bible's shorthand for ten human loading connections applies to PvP; Co-op vs AI requires five human connections and five server-controlled enemies.

## Known canon gap — the ride state

Raska's Hound was the only feature in the roster with no supporting rules in any bible. **That gap is now closed.** All 26 ride-state questions were answered on 2026-09-20 and are canon as **Combat Bible §56, Ride states**, in v0.5.

§56 is written **generically** — "a Vanguard in a ride state", never "Hound" — so a future second ride user costs nothing and no named-content branch enters the rules. The reasoning behind each ruling, and the alternatives rejected, are preserved in [`Ride_State_Open_Questions_v0.1.md`](Ride_State_Open_Questions_v0.1.md); that document is now a decision record rather than an open list.

All 25 Vanguards remain in the first-playable roster. Raska needs none of ADR-003's three primitives and is now fully specified.

## Antagonist pass — 2026-09-20

Four Vanguards are now genuine antagonists: **Angeru**, **Marek**, **Gorraveth** and **Tavi**. In each case the change was removing an explicit moral safety rail rather than writing a new backstory — origins, kits, roles, regions and the roster count are untouched.

None of them is cruel, and none should be written as sadistic. Angeru is methodical, Marek is curious, Gorraveth is indifferent, and Tavi is affectionate. Each data file records the removed rail with an instruction not to reinstate it.

## Canon rulings — 2026-09-20

**Renames.** Two Vanguards were renamed to resolve name collisions that would have been expensive to fix after voice-over and localisation:

- **Tavra → Bryn**, The Harbor Gun (collided with Tavi)
- **Vitra → Oriel**, retitled **The Waiting Light** (collided with Vera; the retitle also reduces the roster's four "The Last —" titles to three)

**Silt is now Silt, The Living Mire**, adopting the title his concept sheet already carried. He was previously the only Vanguard without one.

These apply to current canon only. Archived bible versions retain the former names and are not retro-edited.

**Vision.** Kade's **Sightline grants no reveal** — ordinary corridor vision only, never into Dense Fog, never exposing stealth, never remote acquisition. Tavi's **Hide! is brief Invisibility**, not Camouflage. Sylra and Bryn remain the only Vanguards with fog information tools, both capped at presence rather than position.

**Pronouns.** Silt is he/him; Neris is she/her. Their concept sheets use they/them and are wrong.

Full context and the art-correction checklist are in [`Sheet_Canon_Discrepancy_Register_v0.1.md`](Sheet_Canon_Discrepancy_Register_v0.1.md).

## Machine-checkable roster data

[`Vanguards/`](Vanguards/) holds one structural data file per Vanguard, derived from the Character Bible, plus a validator that checks them against the Combat Bible's crowd-control vocabulary and the roster table, and rejects any gameplay tuning that leaks into the layer. Run `python3 Docs/Design/Vanguards/validate.py`.

The bible remains the authority on intent, fiction and nuance. These files are the subset a script can verify, and the reason sheet drift is now detectable rather than discovered late.

## Making new sheets

[`Art_Direction_v0.1.md`](Art_Direction_v0.1.md) carries the house art style, the signature colours and the pipeline.

Artwork is authored outside this repository and the finished file is dropped into `ConceptArt/Vanguards/<id>/`; `render_sheet.py --all` then re-renders the sheets and `--missing` lists which slots still have no art. Prompts are written by hand from the Character Bible paragraph, the signature-colour table and the model notes in the Art Direction document — the emitter that used to build them was removed once the roster was complete.

All 25 rendered sheets are committed under [`Vanguards/sheets/`](Vanguards/sheets/). They are generated, so they can drift: re-run `render_sheet.py --all` after any change to the Character Bible or the Vanguard YAML, and `git diff` on that directory shows whether the sheets still match canon.

Text is never baked into the artwork — [`Vanguards/render_sheet.py`](Vanguards/render_sheet.py) composes it from canon at render time.

## Concept art is not canon

Current artwork lives under [`ConceptArt/Vanguards/`](../../ConceptArt/Vanguards/) and is reconciled against these bibles character by character. **Approved artwork supplied by the author is canon**: where an appearance paragraph and approved art disagree, the art wins and the text is corrected to match, including its anti-drift guardrails. Everything an image cannot carry — what a design is *not*, the figures gameplay needs, and the reasons behind a choice — stays with the text. The rule and its limits are recorded in the Character Bible header and in [`Art_Direction_v0.1.md`](Art_Direction_v0.1.md).

The older baked-text sheets under [`ConceptArt/Archives/Characters/`](../../ConceptArt/Archives/Characters/) are **superseded historical reference**, not current input. Three contained third-party branding; they were withdrawn and removed from Git history, with one server-side step still outstanding — see [`../Runbooks/purge-third-party-sheets.md`](../Runbooks/purge-third-party-sheets.md).

Every known conflict, plus the required corrections for the next art pass, is catalogued in [`Sheet_Canon_Discrepancy_Register_v0.1.md`](Sheet_Canon_Discrepancy_Register_v0.1.md). Consult it before treating any sheet as input to implementation.

Older superseded versions are preserved under [`Archives/`](Archives/). They exist for design history and comparison only and must not be treated as current implementation requirements.

The original Word documents include richer layout and concept-art presentation. These Markdown exports prioritize searchable design content and may omit embedded images.

Design bibles are working canon and may evolve. Architecture rules live separately in the repository root and are not overridden by incidental implementation suggestions inside a design document.
