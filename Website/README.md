# Website

A placeholder for Veyra's account-facing website (Profiles & Identity Bible §3, §7; Moderation Bible §4).

The repository is connected to a Cloudflare Worker named `veyra` through Cloudflare's Workers Builds:
- **The production branch:** a push runs `npx wrangler deploy`.
- **Any other branch:** a push runs `npx wrangler preview`, which builds a Preview with its own public URL for that branch.

Both read `wrangler.jsonc` at the repository root. A Preview needs the file's `previews` block, even an empty one. Without that block, every pull request's "Workers Builds: veyra" check failed.

`worker.js` answers every request with one line of plain text. It keeps no data and asks for no permissions, so its Previews expose nothing.

- **The website:** when it is built, it replaces `worker.js`, and `wrangler.jsonc` points at its entry point. It needs its own architecture decision first, since the hosting and identity vendors are undecided. Its Preview settings (variables, bindings) go in the `previews` block, pointed at test resources.
- **Preview limits:** a Worker keeps a limited number of Previews (100 on the free plan). Old branches' Previews can be deleted in the dashboard (Workers & Pages → veyra → Previews).
- **Fewer builds:** to build only the production branch, turn off preview builds in the Worker's Build settings (Cloudflare dashboard → Workers & Pages → veyra → Settings → Build → Branch control → Enable Preview Builds). Branches then show no Cloudflare check at all.
