# Website

A placeholder for Veyra's account-facing website (Profiles & Identity Bible §3, §7; Moderation Bible §4).

The repository is connected to a Cloudflare Worker named `veyra` through Cloudflare's Workers Builds. Every push runs `npx wrangler deploy`, or `npx wrangler versions upload` on a branch other than the production branch. Both read `wrangler.jsonc` at the repository root. Before this folder existed there was nothing to deploy, so every pull request's "Workers Builds: veyra" check failed.

`worker.js` answers every request with one line of plain text. It keeps no data and asks for no permissions.

- **The website:** when it is built, it replaces `worker.js`, and `wrangler.jsonc` points at its entry point. It needs its own architecture decision first, since the hosting and identity vendors are undecided.
- **Fewer builds:** to build only the production branch, turn off non-production branch builds in the Worker's Build settings (Cloudflare dashboard → Workers & Pages → veyra → Settings → Build → Branch control). Branches then show no Cloudflare check at all.
