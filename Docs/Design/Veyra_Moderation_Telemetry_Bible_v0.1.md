# Veyra Moderation, Reporting & Telemetry Bible

**Version:** 0.1 — Moderation mechanics, replay evidence and telemetry design checkpoint  
**Status:** Locked player-facing/reporting rules where stated; evidence implementation, privacy and staff workflow require more design  
**Scope:** Player blocks (cross-reference), participant reports, moderator decisions and appeals, logging, evidence integrity, audit trail and operational/game-health telemetry.  
**Related:** [Parties & Social](Veyra_Parties_Social_Matchmaking_Bible_v0.1.md) owns comprehensive blocking and social interactions; [Replay & Spectating](Veyra_Replay_Spectator_Bible_v0.1.md) owns replay format, access, playback and retention; [Match Flow](Veyra_Match_Flow_Bible_v0.1.md) owns authoritative AFK/disconnect and result adjudication; [Profiles & Identity](Veyra_Profiles_Identity_Bible_v0.1.md) owns account identity and website appeal entry point.

> **Core principles:** Detailed **server-authoritative** evidence, proportionate human-reviewed sanctions, tamper-evident moderator decisions, data minimization, access control and clear retention. A player report is an allegation/request for review, never proof of misconduct or an automatic punishment. Moderation service/vendor and storage architecture are **not chosen**.

## 1. Locked — who may report, when, and what it creates

- **Only participants in a given match may submit player reports about that match.** A person merely watching someone else's replay via a shared link cannot report its participants through that replay.
- A participant may report **multiple participants in the same match**, directly from the **post-match player list** or from the reporting player's own **Match History**. Report submission does not require a friend request, existing chat, or blocking.
- **All reports referring to the same match are grouped into one moderation case/parent record**, while retaining each report's reporter, reported player, selected reason, timestamps and incident context distinctly. Grouping is not a blanket decision against every named participant.
- Repeated submissions of the same incident by the same reporter must not flood the moderation queue as duplicate cases. Exact report-count limits, merging policies, edit/correction rules, and whether a player can file separate incidents in one match require more design.
- **Blocking and reporting are independent** actions. Blocking someone does not automatically report them; a report does not automatically block anyone. See Parties & Social for the strict future-match exclusion created by a block.

## 2. Locked — evidence and staff access

- Each eligible match produces authoritative replay evidence under the Replay Bible's capture/retention rules, plus indexed match metadata and events sufficient to investigate relevant incidents.
- Event evidence should include verified participant IDs, match/mode, timestamps, match results, meaningful gameplay and objective events, deaths, disconnects, connected-AFK intervals, vote activity and relevant server-adjudicated performance/participation; specific sensitive fields and sampling policies need privacy review.
- The **dedicated match server and trusted services** produce authoritative gameplay, session and account events. The pre-game client may submit a report and its description but **cannot certify cheating, performance, chat authorship, bans or match outcomes** by itself.
- Replay/event timestamps must align so a moderator can jump to a reported incident, inspect surrounding play, switch player viewpoints and cross-reference server events.
- **Chat is omitted from publicly shared replay playback and from live public spectator playback.** Relevant match-associated chat evidence can be reviewed **only by authorized moderators**, through a restricted investigative interface; public replay links do not expose it.
- In-match participants can still use the actual live game's allowed communication features. "Only moderators can see chat" here is about **replay/spectator evidence access**, not a requirement to hide a player's own live chat from them.
- Do **not** assume the system records out-of-game voice calls, players' operating systems, unrelated private messages or all party communications. Specific chat channels to retain, sensitive-data treatment and notice/consent policy remain open.

## 3. Locked — human moderation and sanction options

- Authorized moderators review reported incidents and determine whether any punishment is warranted. Reports or telemetry **do not automatically punish accounts**.
- Available moderator responses include **warnings, chat restrictions, temporary matchmaking suspensions and permanent account bans**. Exact policy, offense categories, suspension durations, review roles and escalation are to be designed.
- Any sanction is applied **per account**, not to every participant named in a match case. Party teammates do not automatically inherit a penalty because they played with a reported player.
- An issued **permanent account ban** prevents access to game client and gameplay, including matchmaking, parties, chat and purchases; preserve records relevant to appeal/evidence. The player retains the ability to access the website's **ban-appeal/account-support path**. Exact website auth restriction and support access design is deferred to backend work.
- Do not confuse moderation restrictions with the **individual champion-select dodge penalty** or match AFK personal-loss result. They have different triggers/authorities, even if matchmaking must enforce them all.

## 4. Locked — appeals and case history

- **One appeal per moderation decision**. Avoid unlimited resubmission of the same appeal as a new queue entry.
- A player with continuing pre-game client access can submit an appeal there. **A banned player must appeal through the official Veyra website**; the ban cannot remove access to the account-linked appeal mechanism.
- A moderator **other than the person responsible for the appealed decision** reviews the appeal with the original case, applicable replay/chat evidence and action history.
- The reviewer may **uphold, reduce or overturn** the penalty. Filing an appeal does **not** automatically suspend the penalty.
- A genuinely **new moderation decision** with its own new penalty has its own appeal eligibility. Correcting/repeating an already-reviewed appeal is not automatically a new decision; exact reopening for new evidence or staff error remains open.
- An appeal's outcome, reviewer, date, reasoning and evidence references are retained as part of the moderation audit trail.

## 5. Locked — evidence retention relationship

- Public/account replay retention and ten per-account saved slots are defined in the Replay Bible. **Authorized moderators may access replay evidence for up to one month (30 days) from match completion**, even when its public link has expired.
- When an investigation or appeal is **active**, relevant replay and other required evidence must be **preserved beyond the ordinary/moderator expiration** until the hold is properly released. Automatic cleanup cannot erase a live case.
- A player saving a replay does not make **chat evidence public** and does not grant moderator permissions. Conversely, a moderator's 30-day evidence window does not extend the general public replay link.
- Precise retention for reports, chat evidence, moderation decisions, audit/access logs and preserved evidence following case closure is **not yet locked**. Set each separately with data-protection/legal review rather than assuming permanent capture of everything.

## 6. Telemetry architecture goals — substantial feature to design separately

**Moderation evidence:** Searchable authoritative events and contextual replay, accurate match/player identity, actions surrounding reports and AFK/disconnect/vote signals. Provide tamper detection and reconcile gaps; a replay alone does not prove player intent or justify an automatic sanction.

**Game health and balance:** Measure queue time, party composition, mode participation, picks/bans, objective timing, item builds, match duration, Vanguard performance, client crashes, disconnects, latency, server performance and replay storage/use. Aggregated analytics for balance must be separated from individual punishment decisions.

**Moderation audit:** Log every case view/access to restricted evidence, staff identity/role, issued or reversed sanction, appeal reviewer, reasoning and supporting evidence references. Ordinary moderators must not silently rewrite original server evidence, erase their actions or remove access logs.

**Security/privacy:** Granular role-based staff access, confidentiality of restricted chat, verified timestamps/identifiers, encrypted and protected storage/transit where appropriate, data minimization, retention schedules, access logging and incident response. Do not collect every keystroke/OS activity or record private calls in pursuit of "insane telemetry."

**Data ownership:** Match authority owns live events and gameplay truth; trusted account/commerce services own persistent balances and purchases; moderation services own case/sanction state; analytics consumes events without becoming gameplay authority. Service boundaries, schemas, replay synchronization and privacy policy need dedicated architecture decisions before code.

## 7. Deliberately open

- Reports taxonomy and detailed evidence required for alleged cheating, toxicity, AFK, griefing, exploits, harassment and false reports.
- Moderator role hierarchy, warnings vs restrictions thresholds, duration/escalation, staffing tools and conflict-of-interest policy beyond separate appeal reviewer.
- How to handle reports from custom/private games (match participants may report; mode-specific evidence/availability needs definition), timing limits, reporting a player who renamed/deleted their account, and duplicate submissions.
- Chat channel evidence scope, chat/report/audit retention, notice and data access/deletion obligations by jurisdiction.
- Detailed replay compatibility, storage and legal hold policy in the Replay Bible; appeal reopening and enforcement while an account is being reviewed.
- Validation and adversarial testing of report authorization, same-match grouping, client-forged evidence rejection, appeal count, moderator access permissions, and evidence deletion holds.

**This is design scope, not authorization to collect unrestricted personal data or to implement unapproved backend/security choices.**
