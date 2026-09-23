# Veyra Parties, Social & Matchmaking Bible

**Version:** 0.1 — Pre-game party, social, presence and queue decisions checkpoint  
**Status:** Locked rules where stated; additional matchmaking and social design remains open  
**Scope:** Parties, invitations, readiness, queue lifecycle, match-found acceptance, block restrictions, presence and friend requests.  
**Related:** [Pre-Game Client UX](Veyra_Pre_Game_Client_UX_Bible_v0.1.md) owns player-visible party panel, Play mode selection, actionable notifications, queue status and the reconnect-only client state (Proposals 1–17); this bible retains authoritative party/matchmaking rules. [Client & Platform](Veyra_Client_Platform_Bible_v0.1.md) owns the application handoff; [Modes & Access](Veyra_Modes_Access_Bible_v0.1.md) owns mode eligibility and Vanguard selection; [Match Flow](Veyra_Match_Flow_Bible_v0.1.md) owns champion-select dodge and live-match departure; [Moderation](Veyra_Moderation_Telemetry_Bible_v0.1.md) owns reports and sanctions. [Chat & Communication](Veyra_Chat_Communication_Bible_v0.1.md) owns continuing Party Chat, friend DMs and user-level All Chat/mute behavior.

> Persistent party/friend/block/queue state belongs to trusted services, not client widgets. Privacy and block checks must be authoritative on **both** matchmaking teams. All capacity, timers, rank thresholds and queue settings must be editable, validated data; the startup policy below records initial agreed values. This document does not select a backend vendor or matchmaking algorithm.

## 1. Locked — party membership and leader

- A party supports **one through five players**. It persists when members browse any pre-game screen, including when the **leader** changes screens, and persists on return from a completed match. It does not dissolve because someone opens the Collection, store, profile or other pre-game features.
- **Any member can invite a friend** into an available party slot. Only the leader controls the party's selected mode, starting/cancelling matchmaking, privacy setting, and removing members.
- The leader can **manually transfer leadership** to any other current member when party controls are available. If the leader departs or is removed for going offline, leadership passes automatically to another remaining member. The selection rule for choosing the replacement leader is open.
- Any member may leave voluntarily; the leader may remove other members. Removal itself does **not** penalize the removed player. The remaining members stay together.
- Party capacity is enforced at acceptance, not simply when an invitation is issued. The joining person's block relationships and other access restrictions must still be checked.
- Leader may choose **Public** (friends may join directly when an open slot exists) or **Private** (joining requires an invitation from an existing party member). **Any party member can send invites under either privacy setting.** Public is not carte blanche for a stranger to bypass friend/party permissions.
- Switching pre-game screens never transfers leadership or changes party membership/visibility.

## 2. Locked — readiness and queue entry

- Every member individually marks **Ready**. Only the leader starts or cancels the queue; **Find Match** is available once all current members are Ready and eligible for the selected mode.
- Changing the selected mode or adding a new member resets **everyone's Ready state**. Following queue cancellation due to member departure/removal/disconnect or failed match-found confirmation, remaining members Ready up again before the leader restarts matchmaking.
- **All party members must satisfy the selected mode's individual access requirements.** If even one member lacks Ranked's required Account Level 30 and 20 permanently owned Vanguards, the party remains together but **cannot queue Ranked**; the leader may choose another eligible mode instead.
- **The party's joining/membership composition locks the instant the leader presses Find Match.** No one may join or accept a party invitation into it during queueing, match-found confirmation or champion select, even if Public and not full. Existing members may still depart before match start; the queue/selection is then handled under the cancellation rules. Invitations into the locked party must not reserve a future slot or bypass the lock.
- A member leaving, being removed, or going offline **during matchmaking cancels the whole party's queue**, resets readiness and leaves the remaining group intact. If that person is the leader, leadership transfers.
- Party of **1–5** may enter matchmaking in Casual Select, Draft Pick, Co-op vs AI Beginner/Intermediate, and **eventual Ranked**. Matchmaking supplies other **human** teammates to fill available team slots; Co-op never substitutes friendly AI. A four-player party plus a solo player is allowed.
- Launch priority: **keep queue times reasonable** without splitting Solo/Duo versus full-party queues or banning four-person parties. Exact matchmaking fairness/balance policies and future party restrictions can be revisited using real population and match data; no client may bypass block restrictions to accelerate matching.

## 3. Locked — match-found acceptance, select and penalties

- **Every required human participant** confirms a found match before champion select. **Ready is not automatic acceptance.** Ordinary PvP requires ten human acceptances; Co-op vs AI requires five human acceptances (enemy AI does not accept invitations).
- The acceptance prompt has a **configurable timeout**. If any participant declines or does not accept in time, the proposed match is abandoned **before champion select**. Accepted players return to matchmaking; the player who declined/timed out is removed from the queue. If that player was in a party, the party remains intact but must Ready up again to requeue. Exact cross-party matchmaking restoration and acceptance race handling remain implementation design.
- **At launch, declining or missing match-found acceptance incurs no queue penalty**, even repeatedly. Revisit only if actual player experience warrants it. This is distinct from **deliberately leaving champion select**, which incurs the separate configurable dodge penalty defined in Match Flow.
- A champion-select disconnect/dodge cancels the selection session and returns other players to the queue under Match Flow. A deliberate dodger's penalty is **individual**, not inflicted as another penalty on teammates.
- **A party that chooses to queue with a restricted dodger cannot start matchmaking until that member's queue restriction expires.** Their teammates can queue immediately if they choose a party without that restricted player; the restriction is never transferred to them.
- Party members returning from champion select after a dodge remain together unless they choose to leave; Ready states must reset for a fresh party queue.

## 4. Locked — active matches and reconnection

- Once a match begins, **no party-management controls exist in the Unreal in-game client**. Membership, leader and privacy setting are preserved through the live match; no joining, leaving, kicking, invitations to in-match players, or leadership changes through the in-game client.
- Players in an active match are **unavailable for party invitations**; do not queue such invitations for later. This is different from pending **friend requests**, which are allowed during a match.
- **The same installed Unreal application** transitions into live gameplay and Reconnect-only states; ordinary client UI does **not** remain independently accessible in a second background pre-game process. This does not allow bypassing the no-mid-match-party-management rule. Authoritative party membership/presence persists independently of UI/world lifecycle.
- If a member does not return to the pre-game client after the match, preserve the party during a **configurable short reconnect grace period**. If still offline afterward, remove them automatically and transfer leadership if necessary. Remaining members may Ready up and requeue without the missing player.
- Outside that specific post-match reconnect grace period, a player going offline in a pre-game party is **removed from the party** (no penalty); reconnecting does not automatically rejoin it. Brief disconnect detection/thresholds for presence and exact grace duration are not yet tuned.

## 5. Locked — friends, presence and recent players

- **Appear Offline** makes a player look offline to friends outside their current party; outside friends cannot send them party invitations. The player may still browse the pre-game client, queue, play and invite friends themselves. Presence is visibility, **not** a way to bypass blocks, mode eligibility, penalties or matchmaking rules.
- Activating Appear Offline **does not remove a player from their existing party**. That party's members can see the player is present; outsiders see the player as offline. The setting stays enabled on return from a match until changed by the player.
- There is **no global switch to disable incoming friend requests**. Players accept/decline requests individually; blocking a sender prevents further requests from them.
- **Friend requests sent while the recipient is in a match wait in the pre-game client** for later acceptance/decline. Party invitations cannot target a player currently in a match.
- **Friend direct messages are permitted during a live match**, with reply via the in-game client and conversation continuity on return to pre-game. **Party Chat also follows existing party members across all pre-game/in-game/post-match transitions.** These are text-only chat features, not party-management permissions or exceptions to blocks; see Chat & Communication Bible.
- The pre-game client keeps a **Recent Players list** covering people the user played with or against. Players can send a friend request or block from that list. Reporting is tied to a verified match the reporter participated in; see Moderation Bible.

## 6. Locked — comprehensive block enforcement

- If **either account blocks the other**, the two accounts cannot interact through friend requests, party invitations, direct messages or the same party; neither may be placed **in the same future match on either team** (including Co-op human teammates). This is a strict restriction, **even at the cost of longer queue times**.
- A block can be applied from the post-match player list or Recent Players. Blocking someone does not automatically file a report; reporting someone does not require blocking them.
- A new block during a **live** match does not terminate or alter that already-started match. Enforce it for all subsequent communication/party interactions and match assembly.
- Profile visibility and live friend-spectate features must respect an existing mutual block; a public replay link is **not** authority to reestablish a social relationship or launch a live match interaction.
- A blocked player must never be silently matchmade with the blocking account to satisfy a search-time target. Scale/abuse controls and what to do if broad block lists reduce feasible matches need explicit future design **without** weakening the no-shared-match promise.

## 7. Ranked — future target, not a launch blocker

- Ranked is **deferred from the initial launch scope**; the current draft structure, Level 30/20-owned access requirements and support for parties of one through five are **future design targets**.
- When Ranked is implemented, members of a Ranked party must be **within one rank of each other across the entire party** (same rank or adjacent rank). A middle-ranked member cannot bridge two players whose ranks are two steps apart. Exact ranked tiers, division semantics and enforcement at promotion/demotion remain undecided.
- Launch matchmaking has no requirement to implement a Ranked rating/placement/season system or to split queues by party size. More restrictive party/queue structures may be explored if real matchmaking data warrants them.

## 8. Open work and validation

- Matchmaking algorithm and tradeoffs among party-size pairing, latency, skill estimation, region, acceptable queue time and hard block constraints.
- Invitation expiry/cancellation, pending invites during queue changes, privacy edge cases, presence status propagation, offline thresholds and post-match grace duration.
- Match-found handling for parties where one member declines, group requeue restoration, cross-team acceptance cancellation and queue-disconnect distinction.
- Ranked tier definitions and eventual rating/season design; current launch plan excludes Ranked.
- Social moderation, direct-message retention, privacy features, anti-spam/harassment measures and appeals in the Moderation Bible.
- Tests must cover **both directions of blocks**, cross-team and same-team match assembly, Co-op five-human acceptance, queue lock on leader action, offline leader transfer, requeue readiness, and individual-versus-party dodge penalty enforcement.

**Do not start implementing unspecified backend infrastructure or treat this checkpoint as the end of pre-game design.**


## Pre-game client UX cross-reference — Proposals 1–17 (2026-09-22)

The [Pre-Game Client UX Bible](Veyra_Pre_Game_Client_UX_Bible_v0.1.md) establishes the following **presentation and navigation** details without changing this bible's party, capacity, block, permission, readiness, queue-lock or match-found authority:

- Normal browsing retains an independently collapsible bottom party panel; its expanded **member cards** display profile icon, display name, readiness and leader marker. Member-card contextual actions include eligible invitation and **confirmed leader transfer** before queue, preserving mode/readiness. Invitations can also be sent from the persistent social sidebar. Sending an invite while solo creates an ordinary **mode-less party**; selecting an eligible Play mode while solo creates a normal **one-player party** automatically. No special solo-matchmaking privilege is created.
- Mode changes reset all Ready states (already established in §2); members may ready from the panel or an actionable notification on any ordinary pre-game page. Only the leader starts/cancels matchmaking. Queue status is shown persistently with **elapsed time and an explicitly non-guaranteed estimated wait** from relevant live activity and observed recent queue times; display **Estimate unavailable** without reliable data. Estimate does not reveal private player counts or replace matching and block rules.
- While the party is queue-locked, ordinary social information/DMs may be received but party-changing invitation and leadership actions remain disabled. Match Found's acceptance overlay blocks ordinary navigation and uses the existing acceptance timer. Champion select **completely takes over** the pre-game client: no browsing, shopping, Settings or party management; the sidebar auto-minimizes with only selection-safe text chat available. Party state persists but its bottom management UI hides.
- On verified match completion, restore party and social UI; retained party members return to **Not Ready** and may prepare the next eligible queue independently of whether others remain on results. If Unreal is closed **while the match is still live**, the pre-game client provides **Reconnect as its only action**: no party panel, social/chat UI, shopping or new queue. This blocks client access, **not** preservation of party membership or normal communication through the player's reconnected Unreal game. The dedicated match and other trusted services retain actual state authority.

**UX discussion is paused after Proposal 17. Resume at Proposal 18 only when the author says “continue”.**


## Unified Unreal client and in-queue Test Skin checkpoint — UX Proposals 86–92 (2026-09-23)

The ordinary Shop's interactive Test Skin map is available **during normal matchmaking** in the **same installed Unreal application**. The persistent bottom party panel/queue status remains available: opening/closing or loading a preview must not mutate, pause or restart queue state. A successful Match Found event interrupts/blocks the preview **immediately** with the existing authoritative Accept/Decline overlay; on accepted assembly, close preview before committed champion select. Optional preview asset loading is always lower priority than match assembly, selection, assigned live-match entry and reconnect. On failed assembly, return to eligible ordinary client state, reflecting the **actual** resulting party/queue state; a preview may resume if still permitted, not as a way to avoid readiness/queue locks.

One Unreal application handles these as separate states under [ADR-004](../ADR/ADR-004-unified-unreal-client-states.md); the **dedicated match and trusted party/matchmaking owners are unchanged**. During an assigned live match, neither ordinary client state nor Test Skin permits mid-match party management, purchases or fresh queue. Reconnect-only offers **Reconnect as the sole pre-game action** after crash/process restart while match live. Verified completion restores ordinary shell and approved post-match readiness. **Current UX pause: after Proposal 92.**
