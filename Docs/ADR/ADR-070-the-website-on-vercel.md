# ADR-070: The website on Vercel

**Status:** Proposed. On 2026-10-07 the author asked for Veyra's website, hosted on Vercel, with the download link for Setup, a guide to installing Setup while it is unsigned, a support page, sign-up and log-in pages on the existing Firebase project, and stylized, anime-leaning animation in Veyra's look. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-07
**Related:**
- [ADR-057](ADR-057-public-test-hosting-from-the-authors-pc.md): the `veyra` Worker as the one public address, the backend and the release store behind it. The website reaches both through it.
- [ADR-038](ADR-038-player-accounts-with-firebase-authentication.md): Firebase Authentication, `POST /v1/register` and its recovery when an account has no name yet. This record puts registration back on the website, as §3 of that record anticipated.
- [ADR-022](ADR-022-installer-and-game-patching.md): Setup, unsigned until the certificate (§10), and the launcher channel file that names Setup (§11).
- [ADR-049](ADR-049-display-name-changes-and-claims.md): §1, the launcher login that keeps a name from being claimed.
- [ADR-005](ADR-005-launcher-session-handoff-and-local-first-hosting.md): L4, no remembered login unless the player opts in.
- [ADR-068](ADR-068-toon-characters-and-graphic-combat-effects.md): §1, the graphic effect language the website's motion borrows.
- [Client & Platform Bible](../Design/Veyra_Client_Platform_Bible_v0.1.md) §1 and [Profiles & Identity Bible](../Design/Veyra_Profiles_Identity_Bible_v0.1.md) §3–§4: account creation on the official website; website visits are not launcher logins.

## Context

Players can install and play the public tests (ADR-057), but there is nowhere to get Setup from: the release store holds it, and nothing points people at it. Windows warns about Setup because it is unsigned, and players need to be told how to get past that safely.

Canon places account creation on the official website. ADR-038 moved it into the launcher only because no website existed. `Website/README.md` reserved the website's paths for it and asked for its own decision first.

## Decision

### 1. A static site on Vercel, built from the repository

- **Hosting:** a Vercel project that imports the repository. `vercel.json` at the repository root holds its whole configuration, so the import needs no settings changed.
- **Build:** `node Website/site/build.mjs`, Node's standard library only, writes `Website/site/dist`, which Vercel serves with clean URLs (`/download`, not `/download.html`).
- **No server code** runs on Vercel: no functions and no secrets. Everything the website asks for, it asks the browser to fetch.
- **Builds only when the site changes:** the ignored-build step compares the commit with the last deployed one, over every path the build reads.
- **The Worker is unchanged.** It stays the one address players' software knows (ADR-057 §1). The website is a separate origin; the Worker's placeholder still answers its other paths.

### 2. One source for what the website shows

The build copies nothing by hand. It reads:
- **`Launcher/config/public.json`:** the Firebase project and the release store and channel, so a website account and download are the ones the public launcher uses;
- **`Docs/Design/Vanguards/<nn>-<id>.yaml`:** each featured Vanguard's name, title, region and roles;
- **`Docs/Design/Vanguards/render_sheet.py`:** their signature colours;
- **`ConceptArt/Vanguards/<id>/hero.webp`:** their approved art;
- **`Launcher/ui/fonts`:** Roboto, the game's type.

`Website/site/site.json` holds only the website's own data, validated strictly:
- the featured Vanguards and how their art is framed;
- how long each holds the home page;
- the display-name rules and the password's minimum length, which the forms check first (tests hold the name rules to the backend's and the launcher's);
- the support contacts;
- the servers' daily hours: the time zone, and when the host's scheduled task starts and stops the servers (added 2026-10-07, when the author scheduled them for 10:00–11:30 PM Eastern Time every day).

### 3. Creating an account on the website

The two steps of ADR-038 §4, from the browser:
1. **Firebase creates the sign-in,** through its REST API: the same calls the launcher's `firebase.rs` makes. Passwords go only to Firebase.
2. **The backend creates the account** with its display name: `POST /v1/register` with the Firebase ID token.

What the website adds:
- **It checks the name first,** so a bad name never creates a Firebase user.
- **It checks the servers first** (`/readyz`), so a player isn't left with a sign-in and no account while the host is down.
- **When the name is taken,** the player chooses another with the same proof, as in the launcher.
- **When the second step fails,** the player can finish later, on the account page or in the launcher, which already asks for a name (ADR-038 §4).
- **The launcher session** the route also returns is dropped. Registering records a launcher login (ADR-049 §1), so a new account's name-claim clock starts when it is created, from either place.

### 4. Logging in to the website is Firebase only

- **The website never calls `/v1/login`.** That route issues a launcher session and records a launcher login, and visiting the website must not count as one (Profiles & Identity Bible §4).
- **The website's sign-in** is the Firebase ID token, kept for the tab (`sessionStorage`) until it expires within the hour. No refresh token is kept: the website remembers no login (ADR-005 L4).
- **The account page** shows the sign-in's email and creation date (Firebase's `accounts:lookup`), sends a password-reset email, and lets an account with no display name finish it.
- **It cannot show the display name**: reading it needs a Veyra session. A website session is deferred.

### 5. The download

- **The current Setup** is read live from the release store's `launcher/<channel>.json`. The page shows its version, size and SHA-256.
- **The download link** goes straight to the Worker's `releases/setup/VeyraSetup-<version>-<channel>.exe`. It is not proxied through Vercel.
- **While the host is down,** the button says so and the page checks again.
- **The install guide** covers what an unsigned Setup meets:
  - the browser's "isn't commonly downloaded" warning;
  - checking the SHA-256 with `Get-FileHash`;
  - SmartScreen's More info and Run anyway;
  - Smart App Control and managed PCs, which offer no way past;
  - WebView2 on Windows 10;
  - the per-user install, and then updates, repair and uninstalling.

### 6. Three routes through the website, and nothing else

`vercel.json` rewrites, with Vercel's caching of them off, so the browser makes no cross-origin request to Veyra:

| Website path | Goes to, through the Worker |
|---|---|
| `/api/status` | the backend's `/readyz` |
| `/api/register` | the backend's `/v1/register` |
| `/api/launcher/:file` | the release store's `launcher/:file` |

- **No other backend route** is reachable through the website. Tests check the rewrites against the launcher's public configuration.
- **The content security policy** allows scripts, styles, fonts and images from the website only, and connections to it and to Firebase's Identity Toolkit only. Pages carry no inline script or style.

### 7. Look and motion

- **Design language:** the launcher's (Art Direction; `Launcher/ui/launcher.css`):
  - the ink background and smoked panels with ink corners;
  - a brass primary action and the Flux-teal accent;
  - Roboto, with heavy tracked capitals.
- **Motion** uses the match's graphic effect language (ADR-068 §1), timed like anime: a held pose, a fast smear, a settle.
  - **The home page** cuts between featured Vanguards behind a white-hot slash that rides the cut's edge, with speed lines and a smear.
  - **Elsewhere:** buttons burst where they are pressed, panels cut in as they scroll into view, Flux motes drift behind the page, and a sigil turns behind the account forms.
  - **The install guide** includes an illustration that clicks itself through SmartScreen.
- **Accessibility:**
  - Everything that moves stops for `prefers-reduced-motion`.
  - The rotation waits while the player reads or uses its controls.
  - A cut has one faint impact frame, never a strobe.
- **Presentation only:** these timings are the website's presentation, not gameplay tuning (ARCHITECTURE §1.3).

### 8. Support

- **Questions and answers** for installing, accounts and playing, searchable on the page.
- **Server status,** live.
- **The servers' hours,** here, in every page's footer, on the home page and the download card, and in the sign-up form's offline message. Pages show them on the host's clock and add the visitor's own when it differs. Beside the live status they say when the next session opens or, while the servers are offline during their hours, that the automatic start may not have run: the host computer is off, or the automation that starts it has run out of usage.
- **Contact channels** from `site.json`. None is configured yet; the page says so instead of showing a placeholder.

## Consequences

- Players can create an account where canon puts it, download Setup, and get past Windows' warnings with their eyes open.
- **To go live,** the author imports the repository in Vercel, with no settings changed. A custom domain can follow.
- **Firebase:**
  - Email and password sign-in is already on for the launcher.
  - If the web API key is ever restricted to certain websites, the website's domain must be added.
  - Reset emails use Firebase's own page.
- **Registration traffic** now also reaches the backend from Vercel's network, through the Worker. It is still verified by the backend alone.
- **New:**
  - `Website/site/` (the pages, `build.mjs`, `serve.mjs` for a local preview, and tests);
  - `vercel.json`;
  - the `Website checks` workflow, which also runs the Worker's test.
- **Deferred:**
  - email verification (ADR-038 §5);
  - ban appeals on the website (Moderation Bible);
  - a website session that can show the display name and profile;
  - anti-bot protection on registration;
  - a custom domain;
  - code signing (ADR-022 §10);
  - the Worker's placeholder pointing people to the website.

## Alternatives considered

- **Firebase's JavaScript SDK.** It loads a third-party script, which widens the security policy, and it keeps a refresh token, which is a remembered login. The REST calls match the launcher's.
- **Calling the Worker from the browser.** That needs CORS in the Worker and the backend. Same-origin rewrites need neither.
- **Proxying Setup through Vercel.** That adds a second proxy and Vercel bandwidth for a file the Worker already serves.
- **Serving the website from the Worker.** The author chose Vercel.
- **A front-end framework.** Static pages need none, and, like the launcher's window, the website keeps no Node toolchain.
