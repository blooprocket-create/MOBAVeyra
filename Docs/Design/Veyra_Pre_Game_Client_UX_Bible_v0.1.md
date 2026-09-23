# Veyra Pre-Game Client UX Bible

**Version:** 0.1 — Approved decisions through Pre-Game Client Proposal 92 (2026-09-23)  
**Status:** Working design canon through Proposal 92, recording approvals, modifications, rejections and deferrals. **Paused after Proposal 92; do not propose 93 until the author says “continue”.**  
**Scope:** Persistent pre-game client shell, Home/Play, contextual social and party surfaces, notifications, matchmaking presentation, Vanguard roster/detail and Mastery pages, committed champion select, post-match results, Match History and Watch Replay entry, player profiles, Shop and checkout, interactive Test Skin, the unified Unreal application's client-state transitions, and reconnect-only recovery. Not a final art direction, implementation blueprint, or authority specification.  
**Related:** [Client & Platform Bible](Veyra_Client_Platform_Bible_v0.1.md) owns the launcher and one unified Unreal application's state transitions; [Parties, Social & Matchmaking Bible](Veyra_Parties_Social_Matchmaking_Bible_v0.1.md) owns authoritative party/queue/invite/readiness restrictions; [Chat & Communication Bible](Veyra_Chat_Communication_Bible_v0.1.md) owns channels and permission; [Match Flow Bible](Veyra_Match_Flow_Bible_v0.1.md) owns selection cancellation, reconnect, AFK/disconnect and outcomes; [Modes & Access Bible](Veyra_Modes_Access_Bible_v0.1.md) owns eligible modes; [Settings & Accessibility Bible](Veyra_Settings_Accessibility_Bible_v0.1.md) owns personal settings and the accessibility of all these client surfaces.

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
- **On entering champion select, the sidebar automatically minimizes.** During champion select, the normal sidebar stays minimized; the dedicated compact single chat panel in Proposal 33 provides selection-safe communication without covering picks/bans/trades/countdown or enabling navigation to other pre-game features. This is **not** an invitation to open the ordinary friends-management interface or leave the selection.
- On cancellation/return to normal pre-game browsing, restore the prior sidebar state. Chat permissions, recipients and continuity remain owned by the Chat Bible. No built-in voice chat.

### 4 — Champion select owns the entire client (approved **with stronger modification**)

- Champion select is the **committed first stage of playing**, **not** a browsable pre-game page. It replaces ordinary content and **locks out all normal navigation**: no Home, Play, Vanguards, Collection, Shop, Profile, Settings, Match History, party management or other pre-game destinations until selection completes or is cancelled.
- The normal bottom party panel and party-management controls are **hidden** (not merely collapsed) while party membership persists. The social sidebar auto-minimizes under Proposal 3; only the authorized compact champion-select chat is available without offering navigation elsewhere.
- Prioritize the relevant mode's Vanguard options, team information, pick/ban/trade and selection countdown. Ordinary notifications and secondary client actions cannot obstruct selection or escape the committed state.
- Completed selection proceeds to the assigned live-match state within **the same Unreal application** (Proposals 90–92; no separate game executable handoff). On selection cancellation, return to the normal pre-game client, restoring prior main page and panel states subject to actual party/queue state. Disconnect vs deliberate dodge penalties/timelines remain in Match Flow; no new leave/select rules are created.

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

- After a **verified completed match**, transition from the live Unreal match state to ordinary client results **within the same Unreal application**, with post-match results as primary page and normal party/friends/chat shell restored. Display authoritative result/progression/reward values; label genuinely pending reward processing rather than showing fabricated awards.
- Members can browse or leave results independently without disbanding the party or waiting for one another. Party Chat and DMs continue according to existing permissions. A match still live is **not** a post-match results state: see Proposal 17.

### 16 — Ready for next game, from anywhere (approved)

- After completed match, retained party members are **Not Ready**, but the selected mode is preserved **if still available/eligible**. Members may ready using bottom panel or ordinary actionable notification even while others review results; the leader starts the next queue when all are ready and eligible.
- Changing mode again resets everyone's readiness (Proposal 8). If selected mode is unavailable/ineligible, explain why and direct leader to select a valid mode in Play. No automatic Ready, automatic queue, mandatory results dismissal or bypass of Match Found.

### 17 — Reconnect-only client when live match remains (approved **with modification**)

- **If the unified Unreal application closes/exits/crashes while an assigned match is still live, it launches into a dedicated Reconnect-only state.** It supersedes every normal page and shell panel: **Reconnect is the only available player action**. No Home/Play/Shop/Vanguards/Collection/Profile/Settings/Match History, no purchasing, no notification center, no friends/DM/Party Chat UI, no party management or fresh queue entry, and no navigable background pre-game UI. Do not allow a secondary route to browse through a popup, notification or keyboard shortcut.
- Show prominent **Match in Progress** status and **Reconnect** control; mode/assigned Vanguard may appear as **passive** identity/status information where confirmed. Reconnect returns **the same Unreal application** to the **same ongoing match and assigned Vanguard**; never create a new match, reselect a Vanguard, reset an absence timer or bypass penalties.
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
| Champion select | **Full-screen committed selection** | **No** | Bottom party UI hidden; sidebar minimized; compact selection-safe text chat | Complete valid pick/ban/trade/Flux Spell selection under existing mode rules |
| Unreal live match | Same Unreal application in committed gameplay state; ordinary shell hidden | No ordinary pre-game browsing through the game | Actual in-match communication remains per Chat Bible | Play the assigned match |
| Unified Unreal application restarted while assigned match remains live | **Dedicated Reconnect-only state** | **No** | **No pre-game social, chat, party, notifications or shopping access** | **Reconnect only** |
| Verified post-match | Results primary with ordinary shell restored | Yes | Independent sidebar and party panel; readiness reset | Review, ready next match, navigate freely |

**Ordering:** Match Found is higher priority than ordinary notifications; champion select is committed, not a page; reconnect-only is mandatory whenever a live match needs return. Normal shell state is restored only when that higher-priority condition legitimately ends.

## 4. Checkpoint history and later sections

**Historical checkpoint:** Proposals 1–49 were captured here and the author paused at 49. The discussion later resumed; the **current pause is after Proposal 92**, with Proposal 93 awaiting “continue”. See §§7–9 for decisions made after 49. The explicitly open design-details list is at the end of this document (§10).


## 5. Approved continuation — proposals 18–40 (2026-09-23)

These decisions extend §2. Proposals 18–40 were approved in discussion, with modifications explicitly called out below. Existing owning-system rules override any purely presentational inference.

### Ordinary pages and Vanguard browsing (18–22)

- **18 — Home content:** Feature the current event prominently, the newest Vanguard and related bundle separately, and news below. If no event is active, use a Vanguard or announcement in the principal feature slot. Cards lead to eligible ordinary client destinations and preserve the social/party shell.
- **19 — Vanguards roster:** Search all released Vanguards; show Owned / Free Rotation / Locked status and All / Owned / Free Rotation filters. Owned status takes precedence where applicable. Even locked Vanguards have browsable details; linking to an existing purchase listing never purchases automatically.
- **20 — Vanguard detail:** Overview, Abilities, Lore, Skins and Mastery information is accessible for owned and unowned Vanguards. Shop links work only in the ordinary pre-game client.
- **21 — Ability demonstrations:** Optional player-controlled play/pause/replay of short accurate ability, stance or stage demonstrations; never auto-play. Written descriptions remain available if a demo is missing or stale.
- **22 — modified, cosmetic interaction:** Detail pages permit comparing/previewing base and owned/unowned skins and show ownership and shop links for unowned skins, but have **no Equip action**. Actual match skin selection happens **only after choosing a Vanguard in champion select**. No shopping or Shop navigation during select.

### Champion-select skin interaction (23–27)

- **23 — Skin carousel:** Once a Vanguard is chosen, a horizontal carousel offers the base appearance and owned skins for match selection. Appearance selection cannot change the Vanguard, deadline, or gameplay. Original default of base unless changed was **superseded by Proposal 26**.
- **24 — rejected:** Do **not** limit carousel visibility to owned skins. Show unowned skins as visibly locked/previewable but not selectable for the match. No purchase flow, prices that imply in-select purchase, or Shop navigation.
- **25 — after lock-in and trades:** Skin preview and eligible appearance selection remain available after Vanguard lock-in while select is active. A valid Vanguard trade loads the carousel for the newly assigned Vanguard; the former Vanguard's appearance choice does not transfer. The final eligible choice for the assigned Vanguard enters the match, otherwise base.
- **26 — remembered appearance:** On future selection of a Vanguard, preselect the skin **actually used in the last match with that Vanguard**, if still owned and available; otherwise base. Save actual match appearance, not a mere preview. After a valid trade load the newly assigned Vanguard's own remembered appearance. This supersedes 23's unconditional base default.
- **27 — preview versus selected:** Browsing changes the large Vanguard art, including previews of unowned skins. Explicit **Previewing** versus **Selected for Match** state preserves the last eligible selection; end-of-select uses the selected appearance, not whichever unowned or owned skin was last previewed.

### Selection roster, communication and awareness (28–35)

- **28 — team overview:** Persistent teammate display of names, mode-permitted hovers/selections, lock-in status and final assigned Vanguards after picks/trades. Never enforce or infer fixed lanes or roles; opposing-team visibility follows mode rules.
- **29 — roster search and filters:** Search by name with All / Owned / Free Rotation filters; show true pick eligibility, bans, already-picked and other mode restrictions. Filters never grant access. Reset search/filters at the beginning of each selection session. The roster stays inside committed select.
- **30 — favorites:** Mark/unmark Vanguards as favorites on detail pages in ordinary browsing; champion select adds Favorites filter. A favorite is not an automatic hover, selection or lock-in and never overrides eligibility. No leaving select to edit favorites.
- **31 — modified, turn awareness:** Prominent phase countdown and player action/turn state remain visible while browsing roster, skins and selection-safe chat. When the player's pick turn begins, **request foreground focus from the OS**, even if another app is in use; do this once on turn start, not continuous focus stealing. If OS denies focus, rely on supported window/taskbar attention behavior. Never pause the authoritative timer or alter selection/timeout rules.
- **32 — modified, one audio cue:** On the player's pick or ban turn beginning, play **one brief distinct sound**, subject to existing audio/background settings. **No additional popup, toast, notification-center entry or standalone turn-alert overlay.** The existing phase/turn indicator and Proposal 31 focus request still apply; no new alert-volume slider.
- **33 — modified, single selection chat:** Normal social sidebar stays minimized throughout select; a compact collapsible **single chat panel** sits beside team information without covering key controls. Ordinary messages address Team. Prefix `/p` addresses the existing Party Chat channel instead; **no extra Party Chat panel/tab or duplicate conversation**. No client-page navigation through chat.
- **34 — recipient clarity:** Chat composer shows **Team** by default and **Party** when typing `/p` with a message. Strip `/p` from displayed sent text; clearly label party messages in shared chat history. After sending a party message, default the composer back to Team.
- **35 — selection status:** Team overview distinctly shows **Waiting**, **Not Locked In** (permitted tentative hover) and **Locked In** (confirmed pick). Trades refresh assignment and applicable status. These are not pre-queue Ready/Not Ready states; no second ready check.

### Flux Spell starting loadout and handoff (36–40)

- **36 — starting Flux Spells:** Champion select offers two optional slots, including empty slots, with spell effects and shared-team-Flux unlock thresholds. Choose/replace available starting spells without spending match gold, altering Vanguard selection/lock-in, or extending the timer. Equipped spells enter the match inactive until their shared Flux thresholds. In-match shop replacement still costs gold under established spell rules.
- **37 — saved loadout per Vanguard:** Prefill slots with the last **starting** spell loadout actually taken into a match with that Vanguard; no saved record means both slots empty. Player can edit before match. Valid trade loads the new Vanguard's saved loadout, not the old Vanguard's choices. In-match shop changes do not overwrite this starting preference.
- **38 — Your Match Setup:** After Vanguard lock-in show an informational, live-updating summary of the actual assigned Vanguard and lock state, **skin selected for match** (not mere preview), and both starting Flux Spell slots with thresholds/empty state. Valid changes and trades update summary; no extra confirmation or ready step.
- **39 — selection-session recovery:** Temporary disconnect shows accurate connection status and attempts to rejoin the **same active server-authoritative** selection session. Restore server-accepted picks/bans, assignment, selected skin, starting spells, current phase and remaining timer; never present unacknowledged local actions as accepted. Server deadlines continue; existing dodge/timeout rules remain. If session ended, present its actual outcome instead of reopening expired selection.
- **40 — Match Starting / Unreal:** After confirmed select completion, show Match Starting with confirmed Vanguard, skin and starting spells, followed by honest Preparing Gameplay / Loading and Connecting to Match states (the former separate-executable “Launching Game” wording is superseded by 91–92); do not invent progress or claim successful join prematurely. Unreal takes focus after joining the assigned match. If launch/connect fails while that match remains live, pre-game offers **Reconnect only**, not ordinary browsing or new queue. No second ready check. [Per ADR-004: “Unreal takes focus” means the same application enters its assigned live-gameplay state; a failed launch/connect while the match remains live leaves that application in its Reconnect-only state, whose only pre-game action is Reconnect.]

## 6. Post-match results and recording decisions — proposals 41–49 (2026-09-23)

**41 — modified / screen layout still open:** Author requested deciding **what gameplay records first**. The proposed results overview and detailed-stats presentation are **not yet approved as a final layout**. Existing approved result gating still holds: only verified completed matches open results, pending information is labeled truthfully, and nobody is automatically readied or requeued.

**42 — core per-player match record (approved with additions):**
- Identity: player, Vanguard, team and actual result; distinguish team outcome from a separately adjudicated personal-loss override where relevant.
- Combat: K/D/A; damage to enemy Vanguards separately from damage to other targets. Record damage **dealt** and **taken** by physical/magical/true type; damage shielded by player-provided shields; effective healing **self / teammate** separately.
- Economy/progression: final level, **total gold earned**, last hits on minions and jungle creatures as separate counts.
- Objectives: direct damage to enemy towers; count of Flux Wells secured with the player's eligible participation.
- No forced or inferred role/lane assignment.

**43 — effective-damage attribution (approved):**
- Damage dealt/taken records **actual health removed/lost**, physical/magical/true; shield-absorbed damage is recorded separately, not also as health damage dealt/taken. Preserve enemy-Vanguard-specific damage as a distinct target-category record.
- Shield provider receives credit for damage **actually absorbed** by their shield, including a self-applied shield; unused shield capacity gives none.
- Healing records only **health actually restored**, split into self and teammates; no overhealing.

**44 — crowd control (approved):** Record per-player effective duration of movement/action-restricting crowd control on enemy Vanguards, with control-type breakdown. Exclude missed, immune, and non-Vanguard effects; do not double count same-player same-target overlap. Exact status taxonomy and cross-provider attribution edge cases are implementation definitions, not license to fabricate a global score.

**45 — vision (approved with modification):** Track distinct instances of enemy Vanguard reveals attributable to player effects when previously hidden from team; count continuous reveal only once per reveal event. Track effective reveal duration without counting overlap from same player/target twice. If the actual game supports vision objects, separately record placements and enemy objects disabled/destroyed; otherwise omit those categories. **A per-player Vision Score is approved**, aggregating legitimate vision contributions, **but its weights/formula are unresolved**; do not assume wards or introduce mechanics to justify score fields. Preserve underlying data so the score is interpretable.

**46 — neutral Flux Well objectives (approved):** Count secured Wells with eligible player participation, damage each player deals to active neutral Well objectives, player landing securing final hit where that mechanic applies, and **team-level** records of each Well secured and its capture time. No weighted objective score.

**47 — tower/Fluxborn expansion (rejected):** Do **not** add proposed individual tower-participation credits, team tower-destruction timeline or team Fluxborn tower-damage attribution as end-screen stats. Proposal 42's **direct player damage to enemy towers remains approved**; the rejection does not change tower/Fluxborn gameplay.

**48 — gold sources (approved):** Record per-player gold from Vanguard kills/assists, minions, jungle creatures, objectives **only if the existing rules grant gold**, and passive/other supported income. Categories reconcile to total gold earned; spending is separate and never subtracts from total earned. No new source or economy rule is implied.

**49 — final build snapshot (approved):** Capture server-confirmed final **item inventory** with actual occupied/empty slots, item upgrade tiers and applicable Attunements; and the two **final equipped Flux Spells**, including empty slots. Do not display replaced items or spells as part of the final build. No inferred build recommendation, grade or forced role. Full item-purchase/replacement history is **not approved** by this snapshot.

### Authority, scope and next decision

- Match-side authoritative gameplay systems record the underlying events and compute statistics once per event; **pre-game UI only displays trusted completed-match records**. Do not duplicate combat, Flux, economy, item or vision calculation in UI or invent unavailable values. Reconcile aggregations and avoid double counting. Separate actual team result from any individually adjudicated match outcome.
- At this historical checkpoint the final end-of-match **screen layout remained open**, as do Vision Score's formula, detailed vision-object mechanics if any, specific crowd-control taxonomy/overlap implementation, full purchase history, and any further stats not expressly approved above.
- **Historical pause checkpoint:** Proposal 49 was accepted; the author later resumed at Proposal 50. **Current pause: after 92.**


## 7. Results, match history and post-match social — proposals 50–67

- **50 — approved, results layout:** Verified completed-match results open on a two-team scoreboard displaying confirmed outcome/mode/duration and each player's Vanguard, K/D/A, final level, gold earned, separate minion/jungle last hits, final item build and inspectable two final Flux Spell slots. **Detailed Statistics** groups approved Combat, Objectives, Economy and Vision figures, with truthful Pending states; Vision Score appears only once its formula/data are defined. No automatic roles, build ranking or performance grade. This resolves Proposal 41's layout deferral at that scope.
- **51 — approved, Match History:** Ordinary-client history lists the player's accessible completed matches newest first with date, mode, duration, Vanguard and actual adjudicated outcome; opening one shows saved Scoreboard/Detailed Statistics with final equipment. Personal loss override versus team outcome and no-contest remain accurate. No history navigation through Match Found, committed select or live-match Reconnect-only.
- **52 — rejected, match timeline:** No separate timestamped match-event Timeline in results/history. Existing Flux Well capture records under 46 still exist.
- **53 — approved, team summary:** Show per-team total Vanguard kills, total player gold earned and Flux Wells secured, counting each team capture once instead of summing player participations. No invented winner/performance grade or rejected tower/Fluxborn aggregate.
- **54 — deferred, comparison:** All-ten-player selected-stat chart/table was **not approved** and is deferred, not a current results feature.
- **55 — approved, progression/rewards:** Dedicated results area shows confirmed account XP/level and Vanguard Mastery progress plus actually awarded rewards under account rules. Show Pending for unfinished processing; no mandatory claim, forfeiture on navigation, requeue or client-calculated award.
- **56 — approved, reporting:** Other match participants' results rows offer match- and account-linked report action with existing reasons/optional details, private moderation submission and no automatic sanction or changed results/party status.
- **57 — approved, player menu:** Contextual menu for other participants on immediate results and saved Match History offers permitted profile, friend request, party invitation, and report actions under privacy/block/party and committed-state restrictions. Does not force party transfer.
- **58 — approved, commendation:** One optional general commendation for **one other teammate** from immediate results only; not a role-specific MVP vote, retroactive action from saved history, honor score, reward or matchmaking effect.
- **59 — modified, post-match chat opt-in:** Optional compact cross-team conversation on immediate results remains **inactive until the player sends their first message**. Before sending they do not receive or see conversation messages; joining shows messages **from that point forward**, not earlier history. Separate from Party Chat and governed by All Chat preference, restriction/mute/block rules; no voice chat.
- **60 — approved, post-match chat lifetime:** Player participation ends upon leaving immediate results or entering another committed match; saved history cannot reopen it; no persistent social conversation or public replay chat archive.
- **61 — rejected:** No Home “View Last Match” shortcut; use Match History.
- **62 — approved, Play Again:** Immediate results offers a shortcut expanding the **existing bottom party panel**, preserving party/mode and showing actual readiness. It never auto-readies, starts matchmaking, creates a new party or changes modes; leader queues only under existing conditions.
- **63 — approved, Watch Replay:** Eligible history records offer a replay entry into the **same Unreal application's Replay Mode** (Proposals 91–92), with honest processing/unavailable/expired status. Exiting returns to the originating history entry, without changing party readiness. Existing replay permission/retention/chat restrictions apply.
- **64 — approved, history filters:** Filter own accessible completed matches by Vanguard, mode and actual personal Win/Loss/No Contest outcome; combinable/clearable and newest-first, respecting personal loss override.
- **65 — rejected:** No Match History participant-name search.
- **66 — rejected:** No history date/date-range filter.
- **67 — approved, history pagination:** Newest entries load in batches, older records via **Load More**; filters apply across **all accessible records**, not just the loaded subset; no retention extension.

## 8. Vanguard profiles and Shop — proposals 68–78

- **68 — approved:** Vanguard detail Mastery view shows confirmed own mastery level/progress, established milestones/emote reward states and requirements, including progress on unowned rotation Vanguards. Inspecting it grants nothing and does not authorize play.
- **69 — approved:** Ordinary Vanguard roster cards display the player's mastery level or No Mastery Progress alongside ownership/rotation/locked state; search/filter and pick eligibility unaffected.
- **70 — rejected:** No expanded Mastery reward-inspection/cosmetic preview screen. Existing own milestone/reward information remains.
- **71 — approved:** Player explicitly chooses one **owned** featured Vanguard for their profile or clears the choice; show default/base artwork, name and confirmed mastery level. No automatic selection of most-played/highest-mastery Vanguard; featured choice does not affect champion-select pick, saved skin or spell loadout.
- **72 — approved:** Full personal Match History/details/builds are **private on the public profile by default**; explicit “Show Match History on My Profile” controls profile-based sharing. Switching off removes that route, not the owner's records, a participant's independent access to shared-match results, or existing moderation/replay authority.
- **73 — rejected:** No separately shared public Vanguard Mastery collection or “Show Vanguard Mastery” setting; the chosen featured Vanguard may display its own mastery level.
- **74 — approved:** Ordinary Player Profile Overview displays name, official profile icon, confirmed account level and optional featured Vanguard; owner-only editing respects permissions. Visitors see only permitted information, including profile-shared history under 72.
- **75 — approved/confirmation of existing functionality:** Available official profile icon and background selection with preview; default background always available; no separate duplicate customization system, user-uploaded icons or gameplay benefit. This does not change the selected featured Vanguard or its match skin.
- **76 — approved, Shop checkout:** Exact offer contents/ownership and authoritative price/currency shown; explicit purchase confirmation is non-charging until confirmed; only successful trusted purchase changes entitlements. Retry must not double-charge. No access during Match Found/select/live-match Reconnect-only.
- **77 — approved, bundles:** Disclose individual Owned/Not Owned, exact price under current commerce rules (never assume an ownership discount), newly unlocked contents; Everything Owned disables repurchase. No duplicate entitlements.
- **78 — approved with art-direction rule:** **Every skin is a bespoke character model and animation treatment, never a simple color reskin.** Shop must accurately preview bespoke visuals/animations/VFX/audio; artwork alone is insufficient. Preview/purchase does not equip skin; choose match skin in select.

## 9. Interactive Test Skin and unified Unreal platform — proposals 79–92

- **79 — modified:** No separate static 3D viewer as the principal preview. A Shop **Test Skin** action enters a small interactive map **within the same Unreal application / pre-game client experience**: click-to-move, abilities on demand, authentic skin assets; exits to the origin Shop listing. Test-only preview grants no XP, gold, Mastery, match reward or ownership and never equips/purchases.
- **80 — approved:** In-map **Base / Skin** switch applies each complete model, animations, ability VFX/audio under comparable position, camera and map conditions; safely finish/clear active effects before swap; no mechanical changes or entitlement.
- **81 — approved:** Nearby and distant recovering **test dummies** receive attacks/abilities and show true cosmetic impact effects. No progression or promised real-match damage outcomes.
- **82 — approved:** Support authentic Vanguard forms/stances/transformations, mobility, mounts, companions and relevant dummy behaviors/test-only statuses needed to trigger skin cosmetics; never invent kit mechanics.
- **83 — approved:** Reset Abilities and unlimited test-only ability resources for repeated inspection, with safe effect cleanup; live ability targeting/cast/movement/stance rules remain representative, and live-match cooldown/Flux/economy remain untouched.
- **84 — approved:** In-map selector switches between base and multiple available owned/unowned skins **for the same Vanguard** with real bespoke assets under comparable map conditions; close returns to original Shop listing even after switching.
- **85 — approved:** Load selected appearance/map first, other preview assets on demand. While loading, keep current appearance usable; safe transition once ready; unavailable assets are clearly labeled, not faked with recolored base stand-ins.
- **86 — approved:** Test Skin remains available while a party searches for a match and the **persistent bottom party/queue panel remains available**. Match Found blocks the test immediately; after confirmed acceptance, test closes before champion select. Not available in committed select or Reconnect-only.
- **87 — approved:** Test loads/renders/asset work is **lower priority than Match Found**, subsequent selection and assigned-match entry. Immediately suspend/stop optional work at match acceptance; release unneeded preview resources. On failed match assembly, may restore the last successfully loaded test appearance if returning to eligible normal browsing, never a purchase or fabricated party state.
- **88 — approved:** Cache/reuse valid local art, model, animation, effects and map assets; selective prediction-based preloading subject to configurable memory/storage budgets, eviction and version invalidation. Author requested reuse whenever safe and useful. Never treat cache as authoritative ownership, pricing or queue state; critical match transitions/reconnect outrank optional preload.
- **89 — approved with native-install clarification:** The installed Unreal game contains its gameplay/cosmetic assets under the **default native PC installation model**. Launcher installs/verifies/patches required compatible content before queueing; Test Skin normally **loads locally installed assets into memory**, not requiring a new per-skin network download. Optional on-demand packs are neither required nor implied. Required update/progress states truthful; optional download work, if later added, yields to critical match events.
- **90 — approved with Unreal client clarification:** Pre-game UI and Test Skin use the actual **Unreal Engine 5.8** application and share real assets and reusable C++/GAS ability systems. The test map uses isolated test-only rules, not duplicated gameplay code in Shop UI. Party/matchmaking priority and dedicated-server authority remain separate.
- **91 — approved, single application:** One installed Unreal application hosts ordinary pre-game client, Test Skin, champion select and live gameplay as distinct states. **Supersedes earlier assumption of launching a separate Unreal game executable after select**. Separate website and authentication/install/update launcher still exist; replay/spectator are distinct modes in the same application.
- **92 — approved, controlled in-application transitions:** Preserve appropriate origin Shop navigation, party and authority-backed session state across browsing/test/select/match/results; prevent test input once Match Found begins and release unnecessary test resources before committed transitions. Show honest Match Starting / Preparing Gameplay / Connecting status and enter playable match only when required assets + authoritative assignment/connection are ready. Verified completion transitions to results within the same application with social/party shell restored and no auto-ready/queue. One application **must not** become one enormous persistent map, UI owner or duplicated gameplay framework. Exact Unreal level travel/resource implementation remains open.

**Current checkpoint:** Author approved 92 and explicitly paused to update documentation. **Do not propose 93 until “continue”.** Older pauses at 17 and 49 are historical only. See [Client & Platform Bible](Veyra_Client_Platform_Bible_v0.1.md) and [Match Statistics Bible](Veyra_Match_Statistics_Bible_v0.1.md) for authoritative domain cross-references.


## 10. Explicitly open design details — not additional approved features

- Final visual layout, proportions, art direction, animation, collapsed-strip exact content, accessibility treatment, notification stacking and platform-specific interaction/viewport behavior.
- Precise live-event content administration, feature-card scheduling, bundle presentation and payment safety; their owning bibles retain gameplay/commerce/entitlement authority.
- Notification expiry/recovery, readiness-request throttling, queue-estimate calculation/data availability and privacy, and leader automatic-reassignment algorithm. Only the approved *visible behavior* is fixed above.
- Reliable state restoration across OS process restart, connection failure and select cancellation; no client-only fabricated state or invented acceptance timers. Reconnect error/status/retry mechanisms must preserve the **Reconnect-only** rule while the match remains live.
- The bottom panel for a **custom host lobby** must not assume an ordinary matchmade queue, five-human readiness or match acceptance; custom match setup and authorization remain governed by Custom Matches Bible.

*Note:* this list has not been re-audited for Proposals 18–92. Open items recorded with later proposals appear in their own sections — for example §6 “Authority, scope and next decision”, the Proposal 54 deferral in §7 and the open Unreal level-travel/resource implementation in Proposal 92 (§9).
