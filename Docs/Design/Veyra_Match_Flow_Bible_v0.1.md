# Veyra Match Flow Bible

**Version:** 0.1 — First playable match lifecycle, voting, reconnection, AFK, and pauses  
**Status:** Current working design canon  
**Scope:** Champion-select exits, match preparation and start, disconnect/AFK recovery and personal results, remake, surrender, pause/resume votes, and match completion.

**Implementation rule:** All timings, percentages, thresholds, and penalties stated here are **initial tuning data**, never hardcoded C++/Blueprint literals. The Match system owns authoritative phase changes, voting, outcome adjudication, and orchestration. Combat/World/Economy own their respective underlying gameplay state. This document does not choose a matchmaking/rating provider, punishment escalation policy, or backend architecture.

**Related documents:** The [Modes & Access Bible v0.1](Veyra_Modes_Access_Bible_v0.1.md) owns mode access and Co-op vs AI composition; the [Custom Matches Bible v0.1](Veyra_Custom_Matches_Bible_v0.1.md) owns host-configured practice, optional victory rules and manual custom-session ending; the [Parties & Social Bible v0.1](Veyra_Parties_Social_Matchmaking_Bible_v0.1.md) owns pre-select acceptance, parties and queue restrictions; the [Client & Platform Bible v0.1](Veyra_Client_Platform_Bible_v0.1.md) owns launcher/pre-game/in-game handoff and reconnect UX; the [Replay Bible v0.1](Veyra_Replay_Spectator_Bible_v0.1.md) owns match replay coverage and spectator delay. The [Chat & Communication Bible v0.1](Veyra_Chat_Communication_Bible_v0.1.md) owns live/post-match text channels, including default-on All Chat and persistent Party Chat. The Battleground Bible owns map structures, wave/camp schedules, and the Prime Well win condition. The Combat Bible owns damage, actual death, and control/target validity. The Economy & Progression Bible owns Gold, XP, and buyback. The Vision Bible owns vision and ward tools.

## 1. Full match lifecycle

0. **Matchmaking and match-found confirmation (pre-select):** The party leader queues an eligible Ready party; **all required human participants must accept the match** before champion select. In PvP, all ten humans accept; in Co-op vs AI, all five humans accept. A decline/timeout aborts the proposed match **before select**, with **no launch decline/miss penalty**, under the Parties & Social Bible.
1. **Champion select:** Players select/lock and may trade Vanguards under the Battleground Bible's mode-specific draft rules. They choose up to two initial Flux Spells free of charge before the match.
2. **Loading:** Wait for all **required human player connections** up to a **configurable loading timeout** (ten for PvP; five human teammates for Co-op vs AI, with five server-controlled enemy AI Vanguards).
3. **Fountain preparation:** Players enter a synchronized **15–20-second prototype preparation period**. They can move **within their own fountain**, buy opening items, and allocate starting skill points, but **cannot leave the fountain**. This preparation countdown happens before the match clock starts.
4. **Live match:** When preparation ends, the fountain exits open simultaneously and the match clock begins at **0:00**. Waves, wildlife, objective openings, remake/surrender unlocks, and buyback use the authoritative elapsed match clock and their own editable timing schedules.
5. **Paused intermission:** Gameplay freezes only after an approved pause vote; the 10-minute real-time intermission countdown still runs.
6. **Match resolution:** Destruction of the enemy Prime Well or a passed surrender ends the live match. A passed remake instead resolves it as no-contest, subject to individual already-applied absence penalties.

A match has **no mandatory time limit or sudden-death winner**. The 20–45-minute target describes desired balance, not a cutoff. A match can continue for hours until a valid end condition occurs.

## 2. Champion-select disconnect and dodge

- If anyone **disconnects during champion select**, cancel that selection session and return the other players to the queue; no match starts and no remake vote is needed.
- If someone **deliberately leaves champion select**, cancel the session and return others to the queue, but give the leaving player a **separate configurable queue-dodge penalty**. This is **not a match loss**, since no match began.
- **Queue-dodge penalty is personal:** teammates do not receive it merely for being in the dodger's party. However, if they choose to queue **with** the restricted dodger, that party cannot queue until the dodger's restriction expires. They may queue without that player. After cancellation the existing party remains intact unless members leave and must Ready up again, under the Parties & Social Bible.
- The player-facing Ranked queue is **deferred from initial launch**, though the documented Ranked draft remains a future target. Champion select's current casual/draft/ranked pick, ban, hover, lock-in, and trade rules remain governed by the Battleground Bible. This section does not add a new draft format or invent a dodge-penalty schedule.

## 3. Loading, preparation, and no-show players

- The loading screen waits for all **required human players** up to a **tunable timeout** rather than waiting indefinitely; Co-op vs AI does not require network connections from enemy AI-controlled Vanguards.
- If someone never connects by that timeout, the match **still proceeds** through the defined start flow. That Vanguard is treated as disconnected at live-match start and follows the ordinary retreat/reconnect rules; its team may start a remake vote immediately at 0:00.
- The no-show player's team is not required to wait for a personal-loss penalty before initiating remake.
- During the pre-0:00 fountain preparation, players may move inside the fountain but **may not leave** until the shared preparation countdown ends.
- A player who disconnects during a live match or deliberately chooses **Leave Match** remains eligible to reconnect to the **same Vanguard and same ongoing match**.

## 4. Disconnect autopilot

- When an already-active player disconnects or deliberately leaves the live match, the Vanguard **immediately starts movement-only retreat toward a safe point behind an allied turret**.
- **After 1 minute continuously disconnected**, the Vanguard changes destination to its own fountain. If it arrives, it **remains at the fountain** until the player returns; it never automatically travels back to lane.
- Autopilot **only moves and paths**: it does not attack, cast abilities, farm, shop, allocate skills, or choose objectives.
- Autopilot grants **no teleport, special immunity, or invulnerability**. The Vanguard remains a normal, vulnerable unit, including while standing behind the turret.
- Reconnecting **immediately restores control of that same Vanguard and stops autopilot**. Reconnecting before the personal-loss threshold clears/resets the *continuous-disconnect penalty trigger*.
- If the Vanguard reaches its fountain, ordinary fountain services and protections apply as they would to a player-controlled Vanguard; being absent does not unlock extra gameplay actions.

The exact behind-turret retreat destination, fallback path if no safe allied turret exists, and pathing details must be implemented through editable map/pathing data and tested; do **not** invent a teleport or an AI combat mode to solve a pathing failure.

## 5. Connected-player inactivity and absence accounting

### 5.1 AFK detection and recovery

- A connected player is flagged AFK after a prototype **90 seconds of Vanguard inactivity**. **Shop browsing and shopping do not count as Vanguard activity**.
- At that 90-second threshold, display a warning to the **connected** player and begin the same two-stage movement-only retreat: first behind an allied turret, then toward the fountain after another minute if inactivity continues.
- If inactivity persists for the following **60 seconds after the warning**, apply the **personal-loss penalty**. The timing remains tunable.
- Resuming meaningful Vanguard activity before the penalty removes the AFK warning/active trigger and **immediately returns control**; a player who already incurred the penalty may still resume play normally.
- Define and test genuine meaningful activity using server-validated gameplay input/participation. **Shop use alone cannot keep resetting AFK detection**, and superficial input spam should not qualify as ongoing play.

### 5.2 Disconnect penalty and warning distinction

- **Disconnected** players cannot see an in-match warning: **do not depend on warning delivery** for their penalty flow.
- Prototype disconnect penalty threshold: **150 seconds of continuous disconnection** (matching the AFK 90-second trigger plus 60-second grace period). Autopilot itself still begins immediately and changes course to fountain after 60 seconds.
- Reconnection **before** the personal-loss threshold resets the current continuous-disconnection trigger fully. Repeated short disconnects do not accumulate toward *triggering* that penalty.
- The same underlying personal-loss penalty applies to connected AFK and disconnected players. Deliberately leaving an already-started match does not bypass it.

### 5.3 Economy and cumulative absence

- A disconnected or AFK Vanguard retains **normal Gold/XP eligibility** while still in the world: e.g. ordinary living proximity XP and qualifying nearby participation Gold when physically within range, plus legitimately credited lingering DoT/summon kills/assists.
- Autopilot does **not** actively last-hit or perform actions to earn resources. Death still removes ordinary living proximity-farm eligibility, under Economy Bible rules. There is no special AFK farm-tax or XP suppression.
- Track **total cumulative absence** across the match for end-of-match forgiveness: time spent disconnected **plus** time connected but AFK. Return to meaningful play stops counting absence; shop browsing by itself does not.
- Resetting a *continuous penalty trigger* on reconnection or recovery **does not erase previous absent time** from this cumulative total.
- During an approved intermission, **all gameplay and AFK/disconnect clocks freeze**; intermission time does not count toward absence or trigger penalties. Players may reconnect while the match is paused.

## 6. Personal-loss penalty and conditional forgiveness

- Once the AFK or disconnect threshold is reached during a live match, flag that Vanguard's player for a **personal loss**, even if their teammates later win. Teammates keep the match result they actually achieve.
- A penalized player may reconnect, resume full control, and continue to help the team; **a penalty does not lock them out of the match**.
- At final result adjudication, remove the personal-loss override **only if all of these are true**:
  1. The player's team **wins the match**.
  2. Their **total cumulative absent time** is at or below a tunable **10% of the final active match duration**.
  3. After returning, they made **meaningful positive contributions to that victory**. Kills/assists, objective participation, protection/defense, utility, and other real team play may qualify; **kills are not mandatory**, particularly for support-oriented Vanguards.
- The result check is not a client-reported claim: the authoritative match/participation record determines whether contribution occurred. The detailed anti-idle/activity scoring algorithm and final contribution thresholds are **to be designed and playtested**, not silently invented by implementation.
- A team loss, a no-contest remake, excessive cumulative absence, or failure to contribute after return **does not clear** an already-applied personal-loss penalty.
- Calculate the 10% comparison using the authoritative **active match clock** (paused time excluded). Exact duration treatment for loading and fountain preparation, absence from never-connected players, and result/rating side effects require explicit match-service implementation design; do not silently count paused time as active absence.

## 7. Remake voting

- **Either team** can initiate a remake vote **from match start at 0:00 through the 5:00 initiation cutoff**. It is generally available: **no AFK/disconnect prerequisite**.
- The team **starting** the vote is the only team that votes. A majority of its five players—**3 YES votes**—passes.
- A teammate who is **disconnected when their vote is to be recorded** automatically votes **YES** on remake. An already-recorded YES or NO remains locked even if the player later disconnects. Reconnecting does not alter a locked vote.
- A remake vote lasts **up to 30 seconds**. It passes as soon as the third YES is recorded; otherwise it fails when the window ends or approval becomes impossible.
- The **5:00 cutoff applies to *starting* a vote**, not finishing it. A vote started at 4:55 may finish after 5:00.
- After a failed remake vote, the initiating team has a **1-minute cooldown** before initiating another, provided the 5:00 start window remains open. No special auto-remake follows from a failed vote.
- A passed remake ends the game as **no contest**: no team winner or loser, and no match win/loss is recorded for otherwise unpenalized players.
- **Exception:** A player whose individual AFK/disconnect loss penalty already triggered **keeps that personal loss** when a remake passes. The other players receive the normal no-contest result unless independently penalized. Remake is not a team win and cannot satisfy forgiveness criteria.
- Never turn remake into a surrender loss merely because the team's opening minutes went badly; they are separate outcomes.

## 8. Surrender voting

- Surrender becomes available at **15:00 elapsed match time**, with its own editable unlock threshold.
- Only the initiating team votes; **3 YES votes out of the five teammate slots** pass the surrender.
- A disconnected player who has not cast a vote is an **abstention** for surrender, **not** an automatic YES. An already-cast vote stays locked across a later disconnect.
- The surrender vote window is **30 seconds**. It passes immediately on the third YES, otherwise fails on expiry or when approval becomes impossible.
- After a failed surrender vote, the initiating team waits **3 minutes** before it can request another. The opposing team has its own independent voting/cooldown state.
- On success, the initiating team **loses**, and the other team **wins**. Applicable individual AFK/disconnect result overrides and possible forgiveness are evaluated using the completed match outcome.

## 9. Vote integrity and concurrent votes

- A YES/NO vote becomes **locked immediately once recorded**; later disconnection/reconnection cannot retract or replace it.
- Automatic votes are recorded according to **the specific vote type**: remake = disconnected YES, surrender = disconnected abstention, pause/resume = AFK or disconnected YES.
- A vote requiring a majority is still measured against the **full five-player team**, never the reduced set of connected players. A unanimous vote requires **all ten** participant votes, including applicable automatic YES votes.
- Votes are initiated and tallied **server-authoritatively**. The UI merely shows votes and sends the player's selected response.
- Do not allow overlapping votes to create conflicting match transitions; authoritative match state must resolve a successfully completed end-of-match vote before accepting later state-changing results. Exact vote-request concurrency and spam control beyond the explicitly decided cooldowns are implementation details still to be specified.

## 10. Pause and resume

### 10.1 Starting a pause

- A pause vote may be requested while a live match is running. Gameplay **continues normally during the vote**; simply requesting a pause never freezes a fight.
- A pause vote lasts **up to 60 seconds**. It succeeds **only when all ten player votes are YES**.
- Players who are **AFK or disconnected automatically count as YES** for pause votes. Their automatic YES is locked once recorded; if they reconnect, they can request a separate early-resume vote during intermission.
- Failure/expiry starts a **3-minute pause-vote cooldown** before another attempt. It is a provisional, data-driven value.
- **No pause cooldown after a successful pause**, and **no limit on successful pauses per match**. A new pause may be requested again after play resumes; unanimous approval is required each time.

### 10.2 Intermission / suspended game state

- On unanimous approval, enter a **10-minute real-time intermission** (prototype editable duration).
- **All mutable gameplay state and all gameplay time stop:** player/autopilot movement; combat and target selection; healing/regeneration; skills and skill allocation; Gold/XP awards, purchasing, selling, queue delivery and swaps; objective and wave spawn timers; respawn/buyback/ability cooldowns; AFK and disconnect penalty clocks; and the elapsed match clock.
- The paused match does **not** offer a special shop phase. Gameplay actions remain frozen even for players in the fountain.
- **Only out-of-game/session functions remain available**, such as reconnecting, communication, and voting to resume. The **real-time intermission countdown** continues so an unanswered pause cannot freeze the match indefinitely.

### 10.3 Resuming

- The match **automatically resumes** when the intermission's 10-minute real-time duration expires.
- A player may request an **early-resume vote** during intermission. **All ten player votes must be YES** to resume early; AFK and disconnected players automatically vote YES. Recorded votes are locked.
- When the match resumes, gameplay and all frozen gameplay clocks continue from their saved values—not from elapsed real-world paused time.
- **No cooldown between completed pauses**. The failed-pause-vote cooldown concerns starting another *pause* after an unsuccessful vote, not early-resume voting.

## 11. Match completion and result precedence

- Destroying the enemy Prime Well grants the attacking team the match win under Battleground Bible vulnerability/defense rules. A passed surrender gives the non-surrendering team the win. No elapsed-time limit chooses a victor.
- A passed remake is **no contest** except for players with an already-triggered individual absence-loss penalty.
- Personal loss applies to its penalized player, **not automatically to their four teammates**. A successful team win may remove that player's personal loss **only** by satisfying the specific cumulative-absence and comeback-participation conditions in §6.
- A revival-style combat save is **not** a match respawn. Buyback and ordinary respawn obey Economy/Combat Bible rules and never reset an absence penalty.

## 12. Replays and spectator authority (cross-reference)

- **Every match produces a gameplay replay**, including games that end in remake; an invalidated/no-contest match has a replay but still awards no account XP under the Account Bible.
- **Custom practice may disable normal victory conditions**; its host can manually **End Custom Match**, producing an explicitly host-ended noncompetitive completion and a replay. **Solo 1v0 cannot auto-win at launch for lack of opposition.** This is a custom-only exception, not permission to change matchmade victory adjudication, force a team win/loss, or award account XP/Mastery; see Custom Matches Bible for the remaining lifecycle details.
- Live friend spectating reads an **authoritatively delayed three-minute recording frontier** through Unreal Spectator Mode; it does not create an extra player slot, bypass the match's fog-of-war data permissions or affect match results. Full replay format, retention, saved-slot and moderator-evidence design lives in the Replay Bible.
- Match chat is visible in live gameplay according to live communication rules; **public replay/spectator playback cannot expose archived chat**, which belongs to restricted moderator evidence under Moderation Bible.
- **Post-match chat is an optional cross-team results-screen conversation**, never a gate to leave, return to party or requeue. Party Chat continues across pre-game, select, live match and post-match; friend DMs may be answered during the match. Team/All Chat, default-on All Chat toggle and individual mute behavior are specified in the Chat & Communication Bible. Public replay/spectator UI remains chat-free.

## 13. Tuning, tests, and deliberately open questions

**Every number in this document is a data-driven prototype setting**, including fountain countdown, loading timeout, first/second autopilot stage, inactivity threshold, warning grace, disconnect penalty threshold, remake/surrender/pause unlocks, vote timeouts, failed-vote cooldowns, intermission duration, and the forgiveness percentage.

Implement automated tests for: disconnection across 60/90/150-second boundaries; reconnect/reset versus cumulative absence; AFK shopping inactivity; movement-only retreat and loss of enemy tower protection on step-out; Gold/XP while absent; vote locks and auto-YES distinctions; vote cutoff versus completion; no-contest plus penalized-player loss; pause freezing **all gameplay timers** but not its real-time countdown; early resume; and final forgiveness for a returning non-kill support contribution.

**Still open—not to be improvised as canon:** precise activity/contribution anti-abuse criteria; loading timeout value; detailed queue-dodge penalties; how reconnect/no-show behavior interacts with the pre-match loading/preparation boundary; pause/resume UI and early-resume voting timeout; concurrency rules for simultaneous pause/remake/surrender requests; and persistence/rating calculations for individual losses, forgiveness, and no-contest remakes. These choices can be specified when the first playable and match service require them.


## Loading-screen presentation checkpoint (Settings proposals 111–120; 2026-09-21)

The [Settings & Accessibility Bible](Veyra_Settings_Accessibility_Bible_v0.1.md) owns loading-screen presentation, not the authoritative match timeline. **Required graphics preparation** and any readable error/retry feedback appear on the **match loading screen**; an affected player does not enter live gameplay while required preparation is incomplete. Apply this within this bible's **existing tunable loading timeout, no-show and connection rules**—do not hold everyone indefinitely or invent a new match-start rule.

Show plain-language loading-stage labels and honest indeterminate progress, honor visual accessibility preferences and preserve actionable errors. Optional gameplay tips/lore facts have **Both (default), Tips Only, Lore Only or Off** selection, manual Previous/Next, varied non-repeating automatic rotation, and **at least 8 seconds** of automatic display per entry (longer for longer text). These presentation timings **never delay completion of loading or match entry**. No new loading audio cue is approved. See Settings Proposals 111–120 for detailed boundaries.


## Pre-game client selection and reconnect presentation checkpoint (2026-09-22)

The [Pre-Game Client UX Bible](Veyra_Pre_Game_Client_UX_Bible_v0.1.md) documents the approved client UX through **Proposal 17** (discussion paused). This section is a **presentation cross-reference**; match cancellation, dodge, reconnect, absence and outcomes remain authoritative under the preceding rules.

- **Match Found:** Prominent blocking Accept/Decline overlay prevents ordinary client navigation and other actions while authoritative match acceptance is pending. After acceptance, show waiting for remaining participants; start champion select only after the existing match-assembly confirmation.
- **Champion select:** Takes over the entire pre-game client as the committed opening match stage. Ordinary browsing, shopping, Settings, party management and other pre-game pages are unavailable; the party UI hides and friends/chat auto-minimizes, while permitted selection-safe chat may be reopened without allowing navigation away. On cancellation, return to an eligible normal pre-game view under the existing disconnect/dodge/party rules. This UI restriction does not create an extra match phase or new dodge penalty.
- **Unexpected Unreal exit while live match remains:** The pre-game client displays only a dedicated **Reconnect** view. **Reconnect is its sole available player action**; no pre-game shopping, account settings, social messaging, party controls or new queue. Reconnect returns the player to **the same assigned Vanguard and live match** and does not reset absence/penalty state. The party and other trusted state remain preserved; normal permitted in-match Unreal chat works again after reconnect. These restrictions govern client UI and **do not** alter match server timers, ongoing match state or OS process behavior.
- **Verified match completion:** Reconnect-only view ends and the pre-game client displays post-match results as primary content; clearly mark any pending trusted reward processing. Normal party/social surfaces resume and retained party members return to Not Ready while preserving an eligible selected mode. Other members may browse or prepare the next queue without waiting for everyone to dismiss results.

**Updated checkpoint (2026-09-23):** The pre-game client discussion has approved decisions through **Proposal 49**, then paused. No Proposal 50 until the author says “continue”. The earlier Proposal 17 checkpoint above is historical.


## Pre-game client selection and results continuation — Proposals 39–49 (2026-09-23)

During an interrupted champion-select connection, the UI may attempt to rejoin the **same active server-authoritative selection session**, restoring only confirmed picks/bans, assigned Vanguard, selected skin, starting Flux Spells, phase and remaining time. Existing deadlines, timeout/dodge outcomes and match authority continue unchanged; an expired session displays its actual outcome. A confirmed successful selection transitions through truthful Match Starting/Launching/Connecting states, then Unreal gameplay; launch failure during an assigned live match remains **Reconnect-only** without a second ready check or new queue.

On verified match completion, present trusted post-match outcome, progression and the approved match-statistics record. Statistics and final equipment are server-confirmed, not inferred by pre-game UI; pending fields remain honestly pending. **The detailed results-page layout was deferred at UX Proposal 41** and is not silently approved by subsequent statistics decisions. Statistics authority/definitions and the 47 rejection are documented in the Pre-Game Client UX Bible §§5–6 and Match Statistics Bible. Individual loss overrides and no-contest rules in this Match Flow Bible retain precedence over any shorthand team win/loss field in a scoreboard.
