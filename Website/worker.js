// A placeholder page, so the repository's Cloudflare Workers build has something to deploy until the
// account-facing website exists (Website/README.md). It serves one line of text and keeps nothing.
export default {
  async fetch() {
    return new Response("Veyra is in development.\n", {
      headers: { "content-type": "text/plain; charset=utf-8", "cache-control": "no-store" },
    });
  },
};
