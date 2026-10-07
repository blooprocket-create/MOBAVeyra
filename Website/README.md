# Website

Two things live here, both outside Unreal:
- **The `veyra` Worker** (`worker.js`): Veyra's one public address, `https://veyra.blooprocket.workers.dev`, in front of the backend and the release store (ADR-057). Players' launchers and games know only it.
- **The website** (`site/`): download, install guide, support, sign-up and log-in, served by Vercel (ADR-070).

## The Worker

The repository is connected to a Cloudflare Worker named `veyra` through Cloudflare's Workers Builds:
- **The production branch:** a push runs `npx wrangler deploy`.
- **Any other branch:** a push runs `npx wrangler preview`, which builds a Preview with its own public URL for that branch. The author has turned preview builds off, so branches show no Cloudflare check.

Both read `wrangler.jsonc` at the repository root. A Preview needs the file's `previews` block, even an empty one.

### What the Worker does (ADR-057)

`worker.js` forwards players' launchers and games to the host PC while `Game/Scripts/Host.ps1` runs there:

| Path | Goes to |
|---|---|
| `/v1/…`, `/healthz`, `/readyz` | the backend, through the tunnel in the secret `API_ORIGIN` |
| `/v1/server/…` | refused: match servers reach the backend inside Docker |
| `/releases/…` | the release store, without the prefix, through the tunnel in the secret `RELEASES_ORIGIN` |
| anything else | the placeholder: one line of plain text (the website is on Vercel, not here) |

- **Offline:** while no tunnel is recorded, or the recorded one has closed, it answers 503, "Veyra's servers are offline".
- **Secrets:** `Host.ps1` sets the two secrets with `wrangler secret put` each time it opens new tunnels. Secrets survive deploys from `main`.
- **Testing:** `node Website/worker.test.mjs` checks the routing with `fetch` replaced.

### Notes

- **Free-plan quota:** the free plan allows 100,000 Worker requests a day. Each player's polling and each downloaded chunk counts, and so do the website's server checks and registrations.
- **Preview limits:** a Worker keeps a limited number of Previews (100 on the free plan). Old branches' Previews can be deleted in the dashboard (Workers & Pages → veyra → Previews).

## The website (ADR-070)

Static pages with no framework and no dependencies. `site/build.mjs` builds them into `site/dist` (ignored by git), reading what the rest of the repository owns:

| Source | What the website takes from it |
|---|---|
| `Launcher/config/public.json` | the Firebase project, the release store and the channel |
| `Docs/Design/Vanguards/<nn>-<id>.yaml` | each featured Vanguard's name, title, region and roles |
| `Docs/Design/Vanguards/render_sheet.py` | their signature colours |
| `ConceptArt/Vanguards/<id>/hero.webp` | their art |
| `Launcher/ui/fonts` | Roboto |
| `site/site.json` | the website's own data: the featured Vanguards and their framing, the home page's rotation, the display-name and password rules the forms check first, and the support contacts |

### Pages

| Page | What it does |
|---|---|
| `/` | The featured Vanguards cut in behind the title; the way in, in three steps; the roster. |
| `/download` | The current Setup from the release store (version, size, SHA-256) and the guide to installing it unsigned. |
| `/support` | Searchable answers; server status; contact channels from `site.json`. |
| `/signup` | Firebase creates the sign-in, then `/v1/register` creates the account with its display name. |
| `/login` | Firebase only, and a password-reset email. Never `/v1/login`: a website visit is not a launcher login. |
| `/account` | The sign-in, a password reset, and finishing an account that has no display name yet. |

### Routes to Veyra

`vercel.json` rewrites three paths to the Worker, so the browser makes no cross-origin request:

| Website path | Goes to |
|---|---|
| `/api/status` | the backend's `/readyz`: whether the servers are up |
| `/api/register` | the backend's `/v1/register` |
| `/api/launcher/:file` | the release store's `launcher/:file`: the current Setup |

The download button links straight to the Worker's `releases/setup/` file. The content security policy in `vercel.json` allows connections only to the website itself and to Firebase's Identity Toolkit, and no inline script or style, so pages set styles from script through the CSSOM.

### Build, preview and test

```sh
node Website/site/build.mjs                  # build into Website/site/dist
node Website/site/serve.mjs                  # preview on http://localhost:4173, with vercel.json's headers and rewrites
node --test "Website/site/tests/*.test.mjs"  # the website's tests
node Website/worker.test.mjs                 # the Worker's routing
```

CI runs all four in `.github/workflows/website.yml`.

### Deploying

- **Import** the repository as a Vercel project. `vercel.json` at the root sets the build, the output, clean URLs, the rewrites and the headers, so no setting needs changing.
- **When it builds:** each push to the production branch deploys; other branches get Preview deployments. Vercel skips a build when nothing the website reads has changed since the last deployment.
- **Support contacts:** set `support.email` or `support.links` in `site/site.json`. Until then the support page says contact details are on their way.
- **Firebase:** email and password sign-in is already on for the launcher. If the project's web API key is ever restricted to certain websites, add the website's domain.
