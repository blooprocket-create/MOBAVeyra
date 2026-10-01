# ADR-038: Player accounts with Firebase Authentication

**Status:** Proposed. On 2026-10-01 the author created the Firebase project `veyra-58ea4` for player accounts. They chose email-and-password registration in the launcher, without email verification for now. This ADR becomes Accepted when the author merges the pull request that adds it.  
**Date:** 2026-10-01  
**Related:**
- [ADR-005](ADR-005-launcher-session-handoff-and-local-first-hosting.md): L1 (the launcher's jobs), L3 (the session handoff), L4 (no remembered login unless the player opts in), H3 (Veyra-owned session tokens, so an identity provider sits behind Veyra's endpoints). This record chooses the identity provider ADR-005 deferred.
- [Profiles & Identity Bible](../Design/Veyra_Profiles_Identity_Bible_v0.1.md): §3 (account creation, login and verification) and §4 (globally unique display names).
- [Client & Platform Bible](../Design/Veyra_Client_Platform_Bible_v0.1.md): §1 (the website and the launcher's jobs).
- [Architecture Constitution](../../ARCHITECTURE.md): §1.3 (values are data) and §12.

## Context

Until now the launcher has signed in only with seeded development accounts on a local backend (ADR-005 H3). Players need real accounts to play. The author created a Firebase project for them and wants players to register and sign in now, from the launcher.

Two parts of canon say something different:

- The Profiles & Identity Bible §3 and the Client & Platform Bible §1 put account creation on the **official Veyra website**. Veyra has no website yet.
- Both bibles require **email verification before launcher login**.

The author decided both points for this record (§3, §5).

## Decision

### 1. Firebase Authentication is Veyra's identity provider

- Players register and sign in with an **email and password** through **Firebase Authentication**. The project is `veyra-58ea4`.
- Other sign-in methods are out of scope for now. Adding one later changes only the launcher and §2's verification.

### 2. Firebase proves identity; Veyra keeps owning sessions (ADR-005 H3)

- The launcher talks to Firebase's REST API itself, from Rust, and receives a **Firebase ID token**. This is a short-lived JWT that proves who signed in.
- The launcher sends that token once to the backend:
  - `POST /v1/login` signs an existing player in.
  - `POST /v1/register` creates a new player's account.
- The backend **verifies the token itself**:
  - The signature must be RS256, using Google's published securetoken keys. The keys are cached as long as Google's `Cache-Control` allows, up to six hours.
  - The audience must be the project ID, and the issuer must be `https://securetoken.google.com/<projectId>`.
  - The token must not have expired, and its issue and sign-in times must not lie in the future. A configured clock skew applies to these times.
  - The subject must be non-empty.
- Verification uses only the Go standard library, so the backend takes on no Firebase SDK and holds no Firebase credentials.
- After verification the backend issues the same **Veyra launcher session** it always has. Everything downstream is unchanged:
  - launch codes;
  - game sessions;
  - the game's login path.
- Firebase never reaches the game. Replacing Firebase later changes the launcher's sign-in and the backend's verifier, and nothing else.
- The backend stores only the link `(provider, subject) → account` in `identity.provider_links`. Email and password stay with Firebase.

### 3. Registration happens in the launcher, for now

- The launcher's sign-in card offers:
  - **Sign In**;
  - **Create an account** (display name, email and password);
  - **Forgot password?** (Firebase emails a reset link).
- This **amends** the bibles' placement of account creation on the website, until a website exists. When it does, the website can use the same `/v1/register` route.
- Login stays in the launcher, as canon requires.

### 4. Display names (provisional)

- The player chooses a display name when registering. It is globally unique (Profiles & Identity Bible §4).
- The bible leaves character rules and lengths to a later pass. Until then a name is **3–16 ASCII letters, digits and underscores**. It is **unique regardless of letter case**, enforced by a unique index on `lower(display_name)`.
- The backend checks the rules. The launcher checks them first, so a bad name never creates a Firebase user.
- Creating the account and linking the Firebase user happen in **one transaction**. A taken name (`409 display_name_taken`) or an identity that is already linked (`409 already_registered`) leaves nothing behind.
- Registering takes two steps that cannot share a transaction: Firebase creates the user, then the backend creates the account. If the second step fails, the player has a Firebase user with no Veyra account. This happens when someone takes the name in between, or when the backend is down. The launcher recovers as follows:
  - Signing in answers `404 not_registered`.
  - The launcher then asks for a display name, keeping the proof in memory.
  - It finishes the account with `/v1/register`.
- Name changes, reclaiming names and the bible's remaining name policy are unchanged and still open.

### 5. Email verification is not enforced yet

- This **amends** the bibles' requirement of a verified email before launcher login.
- Players can sign in as soon as they register.
- Enforcing verification later means two changes:
  - the backend rejects tokens whose `email_verified` claim is false;
  - the launcher asks Firebase to send the verification email.

### 6. Configuration

- Backend, `playerLogin` in `Backend/config/local.json`:
  - `provider` is `firebase` or `none`. With `none`, `/v1/login` and `/v1/register` answer `404 not_found`.
  - For Firebase: the project ID, the URL of Google's keys, the key-fetch timeout, and the clock skew (at most five minutes).
  - The file is validated like every other section.
- Launcher, `playerLogin.firebase` in `Launcher/config/*.json`:
  - Firebase's Auth URL, and the project's web API key. The key names the project; it is not a secret.
  - The section is optional. Without it, the launcher offers only the development accounts.
- Development sign-in is unchanged and still local-only. On a local backend, the development accounts appear folded away beneath a player's sign-in.

## Consequences

- Players can create an account and play against the local stack today. The backend needs outbound HTTPS to `www.googleapis.com` to fetch the keys that sign tokens.
- New backend routes: `POST /v1/login` and `POST /v1/register`. New package: `Backend/internal/firebaseauth`. New migration: `0021_player_login.sql`.
- In the launcher core: `firebase.rs` (the REST client), `player.rs` (registration, sign-in and choosing a name), and `launch::launch` (launching for a session the window already holds).
- The launcher still keeps its session in memory only. A remembered login, opt-in under ADR-005 L4, would store Firebase's refresh token in the Windows credential store. That is not built.
- Deferred:
  - email verification (§5);
  - a website for account creation and ban appeals;
  - account recovery beyond Firebase's password reset;
  - anti-bot protection on registration;
  - the bible's remaining name policy.

## Alternatives considered

- **The Firebase Admin SDK in the backend.** It is large and needs a service-account credential. Verifying ID tokens needs neither: Google publishes the keys.
- **Sending the Firebase token to the game.** Rejected by ADR-005 L3: a provider credential must not cross into the game.
- **Registering on a website first.** This is canon's placement, but there is no website yet. It is deferred, not discarded (§3).
- **Requiring email verification now.** This is canon's rule, but the author deferred it for faster testing (§5).
