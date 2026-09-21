# Veyra Pre-Game Client UX Bible

**Version:** 0.1 — Approved Pre-Game Client Proposals 1–17 (2026-09-22)  
**Status:** Working design canon for the explicitly approved presentation and navigation decisions below. **Discussion paused after Proposal 17 by author request; resume at Proposal 18 only when the author says “continue”.**  
**Scope:** Persistent pre-game client shell, Home/Play, contextual social and party surfaces, notifications, matchmaking presentation, committed champion select, post-match return, and reconnect-only recovery. Not a final art direction, implementation blueprint, or authority specification.  
**Related:** [Client & Platform Bible](Veyra_Client_Platform_Bible_v0.1.md) owns launcher/client/Unreal transitions; [Parties, Social & Matchmaking Bible](Veyra_Parties_Social_Matchmaking_Bible_v0.1.md) owns authoritative party/queue/invite/readiness restrictions; [Chat & Communication Bible](Veyra_Chat_Communication_Bible_v0.1.md) owns channels and permission; [Match Flow Bible](Veyra_Match_Flow_Bible_v0.1.md) owns selection cancellation, reconnect, AFK/disconnect and outcomes; [Modes & Access Bible](Veyra_Modes_Access_Bible_v0.1.md) owns eligible modes; [Settings & Accessibility Bible](Veyra_Settings_Accessibility_Bible_v0.1.md) owns personal settings and the accessibility of all these client surfaces.

> **Client-shell rule:** The page being browsed, the social sidebar, and the party panel are independent in normal pre-game browsing. Time-critical acceptance, committed champion select, and reconnect recovery have priority and **temporarily supersede** the normal shell. Presentation cannot grant permission to bypass authoritative matchmaking, party, ownership, match, or chat rules.

## 1. Agreed overall direction and Home

- Use the familiar **functions** of a competitive MOBA pre-game client as a usability reference without duplicating another game's layout, assets, navigation or visual design. The eventual client should be recognizably Veyra, using its own world, Vanguards, Flux identity, artwork and typography. Previous basic diagram/HTML Settings wireframes are **not** the final visual standard.
- The main area swaps between ordinary pre-game pages such as **Home, Play, Vanguards/Collection, Shop, Profile and Match History**, with appropriate entry points for other established features. This enumeration is a proposed navigation grouping, not a new shop, item, feature, or permission.
- **Home** is the live, content-led landing hub, featuring **currently running events**, the **most recently released Vanguard**, related Vanguard **bundles/cosmetics**, and updates/announcements. Featured content leads to its existing event, Vanguard or shop/detail destination. Do not invent event mechanics, currencies, offers, prices or entitlements from a banner; do not assume the latest Vanguard is owned.
- **Play** is for eligible mode discovery/selection. Normal party management and queue entry need **not** require remaining on the Play page.
- On ordinary pages, keep **friends/chat in a persistent independently collapsible side panel** and **party/matchmaking in a persistent independently collapsible bottom panel**. A shared notification system brings time-sensitive actions to the current page. The client must not bury an action behind mandatory page navigation when the user can act from an approved persistent panel or notification.

## 2. Approved pre-game client proposals

### 1 — Client-wide actionable notifications (approved)

- Nonblocking ordinary notifications are available across regular pre-game pages and do not cover the friends sidebar or expanded party panel. **Party invitations** expose **Accept / Decline**; **ready-up requests** expose **Ready / Not Yet**; both act on the existing party service state.
- A persistent navigation notification icon provides pending actions after a brief popup disappears, subject to actual expiry or resolution. An expired/resolved invitation cannot still be accepted.
- No automatic page changes, party acceptance or Ready action. **Match Found** has higher priority and is never obscured by ordinary notifications; the established match acceptance deadline remains authoritative. No extra notification-settings panel is approved.

### 2 — Persistent bottom party panel and its states (approved; further refined by 6–7, 13, 16)

- The bottom party panel is independent of the current ordinary page and retains the player's expanded/collapsed state across navigation.
- **No Party:** compact entry to Play and ordinary party creation/invitation. **In Party:** expanded roster, selected mode, readiness and authorized party actions; players may ready/unready from the panel and the leader starts matchmaking when all are ready and eligible. **In Queue:** compact searching state with detail in expanded panel; queue-lock rules apply. **Match Found:** the panel reflects state but never replaces the priority acceptance overlay.
- Notifications remain an alternative to opening the party panel for pending actions. Do not put every control in the collapsed strip; do not duplicate Party Chat in it.
- Proposal 6 clarifies solo play: choosing a mode creates a **real one-person party**, not a separate solo-state queue; see below.

### 3 — Persistent friends/chat sidebar (approved **with modification**)

- Ordinary pages: the sidebar shows permitted friends/presence, **Party Chat**, friend **DMs**, conversation/unread states; collapsed view retains an unobtrusive tab and unread indicators. Conversation and collapse state persist across normal page navigation. The side panel collapses independently of the bottom party panel.
- **On entering champion select, the sidebar automatically minimizes.** Players may reopen chat **within champion select** to communicate without covering picks/bans/trades/countdown or enabling navigation to other pre-game features. This is **not** an invitation to open the ordinary friends-management interface or leave the selection.
- On cancellation/return to normal pre-game browsing, restore the prior sidebar state. Chat permissions, recipients and continuity remain owned by the Chat Bible. No built-in voice chat.

### 4 — Champion select owns the entire client (approved **with stronger modification**)

- Champion select is the **committed first stage of playing**, **not** a browsable pre-game page. It replaces ordinary content and **locks out all normal navigation**: no Home, Play, Vanguards, Collection, Shop, Profile, Settings, Match History, party management or other pre-game destinations until selection completes or is cancelled.
- The normal bottom party panel and party-management controls are **hidden** (not merely collapsed) while party membership persists. The social sidebar auto-minimizes under Proposal 3; only authorized selection-safe chat can be reopened without offering navigation elsewhere.
- Prioritize the relevant mode's Vanguard options, team information, pick/ban/trade and selection countdown. Ordinary notifications and secondary client actions cannot obstruct selection or escape the committed state.
- Completed selection proceeds to the existing Unreal match handoff. On selection cancellation, return to the normal pre-game client, restoring prior main page and panel states subject to actual party/queue state. Disconnect vs deliberate dodge penalties/timelines remain in Match Flow; no new leave/select rules are created.

### 5 — Match Found acceptance overlay (approved)

- Match Found presents a **prominent blocking overlay** above the current pre-game page with mode, authoritative acceptance countdown, and explicit **Accept / Decline**. While pending, ordinary client page navigation, party controls and other actionable notifications are unavailable; the player cannot dismiss the overlay to browse.
- After Accept, show waiting for remaining players. Do not launch champion select until match assembly and acceptance are confirmed by trusted services.
- Decline, timeout or failed assembly returns to the previous page/panel state, reflecting the actual resulting party and queue state. Never auto-accept or alter acceptance/penalty rules. Where supported, the existing privacy-conscious OS background Match Found notification can offer explicit Accept/Decline subject to the same deadline (Settings Proposal 50).

### 6 — Solo queue creates a normal one-person party (approved **with modification**)

- **Play → select mode automatically creates a party with the player as leader and sole member.** The solo player may queue immediately through the normal party controls, with no separate Create Party prerequisite, special Solo Ready state or extra solo-queue mechanic.
- The same lobby can invite/join friends and expand to an ordinary multi-member party. Existing rules apply when members join, including readiness reset as defined in Parties & Social.
- Before mode selection, a party may still be created **without a selected mode** through invitations (Proposal 9). A No Party state may exist for a player who has not selected a mode or otherwise formed a party.

### 7 — Play mode selection hands off to the party panel (approved)

- Play opens a mode-selection view; merely opening Play does not create an empty party.
- Selecting an eligible mode creates the one-person lobby if needed or, for an existing party, lets its **leader** update that party's selected mode; never silently duplicate or disband an existing party. Expand the bottom panel to reveal the lobby, invitations, readiness and permitted **Find Match** control.
- After leaving Play, the same party persists and can be readied/queued from the bottom panel while browsing other ordinary pages. Nonleaders can view the selected mode but cannot change it.

### 8 — Mode change resets readiness (approved)

- When the leader changes to a **different** mode, preserve party membership but reset **every member to Not Ready**; the bottom panel reflects the new mode and existing ready-up notifications permit members to respond from anywhere in the normal pre-game client.
- Reselecting the **same** mode makes no changes and sends no duplicate ready request. Queue entry still requires all members Ready and eligible. This is consistent with the existing Parties & Social Bible.

### 9 — Party invitations from the sidebar (approved)

- An eligible friend's sidebar entry offers **Invite to Party** without requiring Play or opening the bottom panel.
- Sending an invitation while not in a party creates a new lobby with the inviter as leader **without requiring mode selection**. If the inviter is already in a party, use the existing party; do not create another lobby.
- Accepted invites update the persistent bottom panel for all party members without forcibly switching pages. Existing invitation, block, party-capacity and live-match restrictions apply; proposal does not confer exclusive invite rights on leader.

### 10 — Expanded-panel party member cards (approved)

- Each occupied compact member card shows profile icon, display name, Ready/Not Ready and a leader marker where relevant. Empty slots provide an invite entry point. Selecting a member card offers only authorized **contextual** actions, not a permanent button farm.
- During matchmaking, cards may show status but party/readiness editing remains locked. Cards are not shown in the collapsed strip. Party Chat stays in the independent social sidebar.

### 11 — Manual leader handoff via member card (approved)

- While ordinary party controls are available **before matchmaking**, the leader may use another member's contextual card action **Make Party Leader**, with a brief confirmation naming the recipient.
- The new leader gets the ordinary mode selection and queue start/cancel privileges. Membership, currently selected mode and Ready states are preserved. Handoff is unavailable in queue or champion select. Automatic handoff on leader departure/offline remains governed by Parties & Social; this proposal does not select the replacement algorithm.

### 12 — Mode cards and eligibility presentation (approved)

- Play uses distinct, artwork-led mode cards with mode name, short description, team format and current availability.
- Eligible mode selection follows §2.7 and opens/updates the existing party lobby in the bottom panel. For an ineligible party or account, keep the mode visible with its actual existing restriction and do not change party state if selected. Unreleased modes are **not** presented as playable launch queues; **Ranked is deferred at initial launch**.
- Availability and eligibility come from trusted existing mode/matchmaking rules; a card is not permission for the pre-game client to invent or adjudicate a new access rule.

### 13 — Queue status throughout normal pages (approved **with modification**)

- During matchmaking the collapsed bottom panel shows **Finding Match**, selected mode, **elapsed queue time** and an **estimated time to find a match**. The expanded panel retains roster and queue detail; leader cancellation follows existing permissions.
- The estimate is informed by **current player/matchmaking activity** and **recent observed queue durations** for relevant mode and queue conditions. It is an **estimate, not a countdown, deadline or guaranteed time**, and may update as conditions change. If reliable relevant data is insufficient, show **Estimate unavailable** rather than fabricating a value.
- Treat both elapsed time and the displayed estimate as **non-authoritative informational telemetry**. Do not infer an approved precise estimator algorithm, disclosure of private player counts, new skill or region constraints, or an override of party/block/matchmaking rules. Match Found overlay overrides this status display.

### 14 — Ordinary notifications during queue (approved)

- Ordinary information and incoming permitted friend DMs may continue while a player browses during matchmaking. Actions that would change the **queue-locked party** (e.g. accepting another party invite or manual leadership handoff) cannot bypass that lock.
- An unresolved invitation may be revisited after the queue ends only if it has not expired and the action is still permitted. Match Found overrides ordinary notifications.

### 15 — Primary post-match results view (approved)

- After a **verified completed match**, return from Unreal to pre-game with post-match results as primary page and normal party/friends/chat shell restored. Display authoritative result/progression/reward values; label genuinely pending reward processing rather than showing fabricated awards.
- Members can browse or leave results independently without disbanding the party or waiting for one another. Party Chat and DMs continue according to existing permissions. A match still live is **not** a post-match results state: see Proposal 17.

### 16 — Ready for next game, from anywhere (approved)

- After completed match, retained party members are **Not Ready**, but the selected mode is preserved **if still available/eligible**. Members may ready using bottom panel or ordinary actionable notification even while others review results; the leader starts the next queue when all are ready and eligible.
- Changing mode again resets everyone's readiness (Proposal 8). If selected mode is unavailable/ineligible, explain why and direct leader to select a valid mode in Play. No automatic Ready, automatic queue, mandatory results dismissal or bypass of Match Found.

### 17 — Reconnect-only client when live match remains (approved **with modification**)

- **If Unreal closes/exits/crashes while the assigned match is still live, the pre-game client is a dedicated Reconnect-only screen.** It supersedes every normal page and shell panel: **Reconnect is the only available player action**. No Home/Play/Shop/Vanguards/Collection/Profile/Settings/Match History, no purchasing, no notification center, no friends/DM/Party Chat UI, no party management or fresh queue entry, and no navigable background pre-game UI. Do not allow a secondary route to browse through a popup, notification or keyboard shortcut.
- Show prominent **Match in Progress** status and **Reconnect** control; mode/assigned Vanguard may appear as **passive** identity/status information where confirmed. Reconnect relaunches/foregrounds Unreal into the **same ongoing match and assigned Vanguard**; never create a new match, reselect a Vanguard, reset an absence timer or bypass penalties.
- Party, friend/chat and account state **remain preserved by their authoritative owners**, but their pre-game UI is inaccessible in this recovery state. This is not deletion of messages or disabling of **in-match Unreal Party Chat/DM features** when the player reconnects. The operating system's ability to close the application is not an in-client second action and does not end the match.
- After the authoritative match ends, leave Reconnect-only mode and show the verified post-match results with normal shell restored; if results/rewards are delayed, present the appropriate pending state without falsely offering a live-match reconnect. Exact network-failure/retry/error-state implementation is open; it must not quietly reopen the shop or normal pages during a known live match.
- **This modification supersedes any earlier suggestion that normal browsing, shopping, social interaction or Settings remain accessible through the pre-game client while its player's live match needs reconnection.** The ordinary persistent social/party panels apply only to ordinary pre-game browsing and eligible post-match states.

## 3. Priority and state summary

| State | Primary client UI | Can browse ordinary pages? | Social/party UI | Permitted primary action |
|---|---|---|---|---|
| Ordinary pre-game / no selected mode | Current page with persistent shell | Yes | Independent sidebar and bottom panel | Play/mode browse, permitted invitation, party creation |
| Party lobby / mode selected | Current page with persistent shell | Yes | Independent sidebar and bottom panel; ready-up notifications | Ready, invite, leader-initiated Find Match when eligible |
| Matchmaking | Current page with compact/expanded queue state | Yes | Chat and non-disruptive notifications; membership/readiness locked | Leader may cancel queue; all may await match |
| Match Found | Priority acceptance overlay | **No** | Underlying surfaces blocked by overlay | Accept/Decline, then await assembly |
| Champion select | **Full-screen committed selection** | **No** | Bottom party UI hidden; sidebar auto-minimized; selection-safe text chat may reopen | Complete valid pick/ban/trade/Flux Spell selection under existing mode rules |
| Unreal live match | Unreal foreground; pre-game client background | No ordinary pre-game browsing through the game | Actual in-match communication remains per Chat Bible | Play the assigned match |
| Unreal unexpectedly absent while match live | **Dedicated Reconnect-only screen** | **No** | **No pre-game social, chat, party, notifications or shopping access** | **Reconnect only** |
| Verified post-match | Results primary with ordinary shell restored | Yes | Independent sidebar and party panel; readiness reset | Review, ready next match, navigate freely |

**Ordering:** Match Found is higher priority than ordinary notifications; champion select is committed, not a page; reconnect-only is mandatory whenever a live match needs return. Normal shell state is restored only when that higher-priority condition legitimately ends.

## 4. Explicitly open design details — not additional approved features

- Final visual layout, proportions, art direction, animation, collapsed-strip exact content, accessibility treatment, notification stacking and platform-specific interaction/viewport behavior.
- Precise live-event content administration, feature-card scheduling, bundle presentation and payment safety; their owning bibles retain gameplay/commerce/entitlement authority.
- Notification expiry/recovery, readiness-request throttling, queue-estimate calculation/data availability and privacy, and leader automatic-reassignment algorithm. Only the approved *visible behavior* is fixed above.
- Reliable state restoration across OS process restart, connection failure and select cancellation; no client-only fabricated state or invented acceptance timers. Reconnect error/status/retry mechanisms must preserve the **Reconnect-only** rule while the match remains live.
- The bottom panel for a **custom host lobby** must not assume an ordinary matchmade queue, five-human readiness or match acceptance; custom match setup and authorization remain governed by Custom Matches Bible.

**Checkpoint:** Pre-Game Client proposals **1–17 are approved**, with modifications on **3, 4, 6, 13 and 17**. The author explicitly paused here. **Do not propose 18 or continue expanding this bible until the author says “continue”.**
