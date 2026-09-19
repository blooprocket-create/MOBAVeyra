# Veyra Player Profiles, Identity & Account-Facing Website Bible

**Version:** 0.1 — Player-facing profile, identity and website decisions checkpoint  
**Status:** Locked rules where stated; final UI, identity security and backend details remain open  
**Scope:** Public player profiles, cosmetics, account display names, account creation, email verification, name reclaiming and website access.  
**Related:** [Account, Collection & Mastery](Veyra_Account_Collection_Mastery_Bible_v0.1.md) owns entitlements and progression; [Parties & Social](Veyra_Parties_Social_Matchmaking_Bible_v0.1.md) owns blocks and visibility; [Client & Platform](Veyra_Client_Platform_Bible_v0.1.md) owns launcher/login and pre-game handoffs; [Moderation](Veyra_Moderation_Telemetry_Bible_v0.1.md) owns bans and appeals; [Replay & Spectating](Veyra_Replay_Spectator_Bible_v0.1.md) owns replay visibility/retention.

> Public profile content and name changes must resolve through stable internal **account IDs**, not the mutable display name. Entitlements, account-currency payments and names are server-validated authoritative persistent records. Profile appearance does not alter match/gameplay stats.

## 1. Locked — public profiles and viewing

- Players can view another player's **full public profile**, including account level, displayed cosmetics, per-Vanguard Mastery, most-played Vanguards, match history and match stats. Eligible replay links can be opened under Replay Bible access/retention rules.
- **Blocking takes precedence over profile viewing and social interaction:** two mutually blocked accounts cannot use each other's player profiles to interact or bypass their block.
- Full public *game* profile does **not** mean personal account data is public. Email, credentials, account security settings, private messages and moderator-only chat/evidence are never public profile fields.
- A player's own pre-game profile/Match History remains accessible independently of whether they are currently queueing. Account-level profile privacy adjustments beyond the current default public profile have not been agreed.

## 2. Locked — profile hero and appearance

- Profiles are customizable. The default featured/hero Vanguard is the player's **most-played permanently owned Vanguard**; the permanently unlocked tutorial starter ensures there is always at least one owned Vanguard to display. If playtime leaderboards/stat attribution need tie-breaking, that policy remains open.
- A player can **manually choose any permanently owned Vanguard** as the profile's featured hero, independent of their most-played Vanguard. A Vanguard seen only in free rotation is not eligible until permanently unlocked.
- For their chosen hero, players may display the **default appearance or a cosmetic skin they own** for that Vanguard. Ownership of a skin never creates Vanguard ownership or Ranked eligibility.
- Players choose their profile icon from **officially available Veyra icons only; user image uploads are not supported**.
- Launch selection includes **approximately 30 freely available icons**. Additional icons are unlockable through **events**.
- Additional profile customization, such as banners/backgrounds and other official display items, may be offered through **account milestones, events, eventual Ranked rewards and optional premium cosmetic purchases**. Their specific types, assets, catalog, launch availability and pricing are not yet locked.
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

## 6. Open work and validation

- Account settings, recovery, website account management and moderation access after permanent bans.
- Profile customization catalog, unlock events, Ranked/event border awards, profile visual/accessible presentation and skin thumbnail policy.
- Public match history/stat definitions, history indexing/pagination and match visibility where blocks or bans apply.
- Name normalization/impersonation policy, reserved names, anti-abuse checks, display name reuse during in-flight friend invites, and publication of a name-change history (not approved).
- Claim behavior for suspended/banned/deceased/recovered accounts, name reclaim notifications, handling simultaneous claim attempts and exact activity timestamp semantics are **not yet decided**.
- Tests must cover atomic duplicate-name competition, forced rename without charging/free-token consumption, 24-hour voluntary cooldown, immediate release of relinquished name, immutable ownership history across renames, and player-block profile restrictions.

**Design remains in progress; authentication/database implementation requires a dedicated backend discussion.**
