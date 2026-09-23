# Veyra Player Profiles, Identity & Account-Facing Website Bible

**Version:** 0.1 — Player-facing profile, identity and website decisions checkpoint  
**Status:** Locked rules where stated; final UI, identity security and backend details remain open  
**Scope:** Public player profiles, cosmetics, account display names, account creation, email verification, name reclaiming and website access.  
**Related:** [Account, Collection & Mastery](Veyra_Account_Collection_Mastery_Bible_v0.1.md) owns entitlements and progression; [Parties & Social](Veyra_Parties_Social_Matchmaking_Bible_v0.1.md) owns blocks and visibility; [Client & Platform](Veyra_Client_Platform_Bible_v0.1.md) owns launcher/login handoff and client-state transitions; [Moderation](Veyra_Moderation_Telemetry_Bible_v0.1.md) owns bans and appeals; [Replay & Spectating](Veyra_Replay_Spectator_Bible_v0.1.md) owns replay visibility/retention.

> Public profile content and name changes must resolve through stable internal **account IDs**, not the mutable display name. Entitlements, account-currency payments and names are server-validated authoritative persistent records. Profile appearance does not alter match/gameplay stats.

## 1. Locked — public profiles and viewing

- Players can view another player's **permitted public profile**: display name, official profile icon, confirmed account level and **one player-chosen owned featured Vanguard** (base artwork, name and their confirmed mastery level). **Full Match History, detailed match statistics and final builds are private by default**, visible on another player's profile only when that player enables **Show Match History on My Profile** (UX Proposal 72); eligible replay links remain governed by Replay Bible access/retention, independently of profile-based access. Do **not** expose a public all-Vanguard Mastery collection (Proposal 73 rejected).
- **Blocking takes precedence over profile viewing and social interaction:** two mutually blocked accounts cannot use each other's player profiles to interact or bypass their block.
- Full public *game* profile does **not** mean personal account data is public. Email, credentials, account security settings, private messages and moderator-only chat/evidence are never public profile fields.
- A player's own ordinary-client profile/Match History remains accessible while queueing (subject to Match Found, champion-select and live-match Reconnect-only restrictions). **Match History on the public profile is private by default; its owner can opt into profile-based sharing and later disable it.** Turning it off does not delete records or revoke other participants' independent access to their copy of shared-match results.

## 2. Locked — profile hero and appearance

- Profiles are customizable. **No Vanguard is automatically featured**: the player may explicitly choose one of their permanently owned Vanguards or clear the selection; otherwise show a neutral empty state. This UX Proposal 71 decision **supersedes** the earlier most-played default. Do not auto-select the highest-Mastery or most-played Vanguard.
- A player may **manually choose one permanently owned Vanguard** as their featured hero or clear it; free rotation without permanent ownership does not qualify. Feature uses **base artwork and confirmed mastery level** (or no recorded progress), not a public full mastery collection.
- The featured Vanguard uses **base artwork** under UX Proposals 71 and 74; an owned skin does not automatically change its profile presentation or equip it in a match. Historical allowance for featured-hero skin display is superseded by the approved base-artwork profile presentation. Skin ownership never creates Vanguard ownership or Ranked eligibility.
- Players choose their profile icon from **officially available Veyra icons only; user image uploads are not supported**.
- Launch selection includes **approximately 30 freely available icons**. Additional icons are unlockable through **events**.
- Players may select and preview **available officially provided profile icons and profile backgrounds**, with a default background always available (UX Proposal 75 confirms earlier profile customization; do not build a duplicate system). Sources, catalogs, launch assets, pricing and other optional display-item types remain undecided; no user-uploaded images.
- Profile cosmetics are display-only and must not provide in-match advantages or modify the accuracy of account stats, Mastery or Match History.

## 3. Locked — account creation, login and verification

- There is **no separate player-avatar/character creation process**. The player chooses a display name during **account creation on the Veyra website**. This is account setup, not the in-match Vanguard or starter choice.
- Players create accounts through the **official Veyra website**. **Email verification is required before launcher login/game access.**
- Login/authentication happens in the **launcher**, not in the pre-game client. After launcher login, first-time players complete their tutorial and choose a starter Vanguard.
- The website must offer an account-linked **ban appeal channel** accessible to players who cannot enter the launcher/pre-game client because of a permanent ban. Account creation and ban appeals are minimum website capabilities, not the complete intended website scope.
- Verification method, authentication provider, account recovery, anti-bot protections, website layout, security operations and exact ban-access boundary are **explicitly deferred to backend/platform design**. Do not turn this bible into an unapproved auth implementation.

## 4. Locked — globally unique display names

- Display names are **globally unique**: only one account may hold a particular available name at a time. No shared display names with numeric suffix/account tags.
- A player sets their first display name during account creation. They receive **one free voluntary name change** after creation. Thereafter a voluntary name change costs either **Flux** (earned account currency) or **Refined Flux** (premium account currency); **both costs are TBD** and remain data-driven.
- There is a **24-hour wait between voluntary name changes**, regardless of whether paid in Flux or Refined Flux. A payment does not bypass the cooldown. Account creation itself is not a name change.
- A **voluntarily relinquished name becomes immediately claimable by anyone else** once the change commits. No name reservation/hold period or automatic undo. Confirmation should convey that risk, but detailed interface behavior is a UI design concern.
- A display-name change never resets or transfers the account's internal ID, purchases, earned currencies, Mastery, match history, blocks, reports, replay saves or moderation record. Previously recorded attribution remains linked to the durable account ID, not a cached display-name string.
- All checks for uniqueness, availability, cooldown, payment and reassignment must be **atomic/authoritative**, not decided from client-side availability search results.
- Profanity/impersonation policy, Unicode/confusable names, case normalization, allowed characters, name lengths, cooldown after forced renames and account security verification for identity changes need an explicit later pass.

## 5. Locked — claiming names from inactive accounts (working threshold)

- A name holder who has **not successfully logged into the launcher for at least one year** becomes eligible to have that display name claimed by another player. **No match play is required** to count as active. Visiting the public website alone does **not** reset this launcher-inactivity clock.
- **Do not automatically strip the name at the one-year mark.** The existing account keeps it unless and until another account successfully claims that eligible name.
- On a successful claim, the old holder's account and all purchases, progress and records **remain intact**, but its display name must be replaced; when the original player returns they choose a new globally unique available name **for free**, without consuming their one free voluntary name change.
- One year is the agreed initial **working threshold**, explicitly open to revisiting during backend/identity design. Its configuration and edge-case handling must not be hardcoded.
- Account creation vs rename vs forced-rename claim flows must enforce the same global uniqueness atomically. Name claiming is not account deletion, ban evasion, entitlement transfer, or a mechanism to change someone's account ID.


## 6. Pre-game Client profile/privacy checkpoint — Proposals 68–75 (2026-09-23)

The player's **own** ordinary Vanguard roster/detail pages show confirmed Mastery level/progress/reward requirements even on unowned Vanguards (68–69); **expanded reward inspection was rejected** (70). Public profile showcases **one chosen permanently owned Vanguard** using base artwork and own confirmed Mastery level (71, 74), or a neutral no-featured-Vanguard state if unset. No automated most-played/highest-mastery featured selection. The **public browsable Mastery collection was rejected** (73), notwithstanding older generic “per-Vanguard Mastery” wording in profile concepts.

**Full match history/stats/builds are private by default through the profile** (72); the owner may explicitly enable or disable “Show Match History on My Profile”. Independent match participants retain access to their own copy of shared match results, and Replay Bible links/permissions remain separate. Owner may preview and choose an available official profile icon and background; a default background is always available (75). No skin is equipped by changing profile cosmetics, and the profile hero uses base artwork under 71/74. These decisions supersede older conflicting profile defaults; see [Pre-Game Client UX Bible](Veyra_Pre_Game_Client_UX_Bible_v0.1.md) §8.

**Architecture:** Profile display is an ordinary state in **one Unreal application**, not a separate standalone pre-game executable; website identity/launcher authentication and trusted account records keep their current boundaries under [ADR-004](../ADR/ADR-004-unified-unreal-client-states.md). **Current client design pause: after Proposal 92.**


## 7. Open work and validation

- Account settings, recovery, website account management and moderation access after permanent bans.
- Profile customization catalog, unlock events, Ranked/event border awards, profile visual/accessible presentation and skin thumbnail policy.
- Profile-shared Match History uses the owner's explicit consent and approved history access/filter/pagination rules; retention, block interactions and cross-account replay-link access still follow their authoritative owners.
- Name normalization/impersonation policy, reserved names, anti-abuse checks, display name reuse during in-flight friend invites, and publication of a name-change history (not approved).
- Claim behavior for suspended/banned/deceased/recovered accounts, name reclaim notifications, handling simultaneous claim attempts and exact activity timestamp semantics are **not yet decided**.
- Tests must cover atomic duplicate-name competition, forced rename without charging/free-token consumption, 24-hour voluntary cooldown, immediate release of relinquished name, immutable ownership history across renames, and player-block profile restrictions.

**Design remains in progress; authentication/database implementation requires a dedicated backend discussion.**
