# Website

Veyra's one public address, `https://veyra.blooprocket.workers.dev`, and a placeholder for its account-facing website (Profiles & Identity Bible §3, §7; Moderation Bible §4).

The repository is connected to a Cloudflare Worker named `veyra` through Cloudflare's Workers Builds:
- **The production branch:** a push runs `npx wrangler deploy`.
- **Any other branch:** a push runs `npx wrangler preview`, which builds a Preview with its own public URL for that branch. The author has turned preview builds off, so branches show no Cloudflare check.

Both read `wrangler.jsonc` at the repository root. A Preview needs the file's `previews` block, even an empty one.

## What the Worker does (ADR-057)

`worker.js` forwards players' launchers and games to the host PC while `Game/Scripts/Host.ps1` runs there:

| Path | Goes to |
|---|---|
| `/v1/…`, `/healthz`, `/readyz` | the backend, through the tunnel in the secret `API_ORIGIN` |
| `/v1/server/…` | refused: match servers reach the backend inside Docker |
| `/releases/…` | the release store, without the prefix, through the tunnel in the secret `RELEASES_ORIGIN` |
| anything else | the placeholder: one line of plain text |

- **Offline:** while no tunnel is recorded, or the recorded one has closed, it answers 503, "Veyra's servers are offline".
- **Secrets:** `Host.ps1` sets the two secrets with `wrangler secret put` each time it opens new tunnels. Secrets survive deploys from `main`.
- **Testing:** `node Website/worker.test.mjs` checks the routing with `fetch` replaced.

## Notes

- **The website:** when it is built, it takes over the placeholder's paths. It needs its own architecture decision first. Its Preview settings (variables, bindings) go in the `previews` block, pointed at test resources.
- **Free-plan quota:** the free plan allows 100,000 Worker requests a day. Each player's polling and each downloaded chunk counts.
- **Preview limits:** a Worker keeps a limited number of Previews (100 on the free plan). Old branches' Previews can be deleted in the dashboard (Workers & Pages → veyra → Previews).
