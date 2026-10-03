// The Worker's routing (ADR-057 §1), with fetch replaced: run with `node Website/worker.test.mjs`.
import assert from "node:assert/strict";
import worker from "./worker.js";
// Forwarded requests answer with where they went.
globalThis.fetch = async (request) => new Response(request.method + " " + request.url, { status: 200 });
const env = { API_ORIGIN: "https://api-tunnel.trycloudflare.com", RELEASES_ORIGIN: "https://files-tunnel.trycloudflare.com" };
const base = "https://veyra.blooprocket.workers.dev";

async function call(path, init = {}, environment = env) {
  const response = await worker.fetch(new Request(base + path, init), environment);
  return [response.status, await response.text()];
}

assert.deepEqual(await call("/v1/login?x=1", { method: "POST", body: "{}" }), [200, "POST https://api-tunnel.trycloudflare.com/v1/login?x=1"]);
assert.deepEqual(await call("/healthz"), [200, "GET https://api-tunnel.trycloudflare.com/healthz"]);
assert.deepEqual(await call("/releases/channels/public.json"), [200, "GET https://files-tunnel.trycloudflare.com/channels/public.json"]);
assert.deepEqual(await call("/releases/chunks/ab/abcdef"), [200, "GET https://files-tunnel.trycloudflare.com/chunks/ab/abcdef"]);
assert.equal((await call("/v1/server/matches/m1/result", { method: "POST" }))[0], 404);
assert.deepEqual(await call("/"), [200, "Veyra is in development.\n"]);
assert.deepEqual(await call("/releases"), [200, "Veyra is in development.\n"]);
assert.equal((await call("/v1/me", {}, {}))[0], 503);
assert.equal((await call("/releases/channels/public.json", {}, {}))[0], 503);
// A closed tunnel reads as offline, not as an error.
globalThis.fetch = async () => { throw new TypeError("connection refused"); };
assert.equal((await call("/v1/me"))[0], 503);
// So does a tunnel Cloudflare can no longer reach, which it answers with its own origin errors (ADR-062 §5).
for (const status of [502, 520, 530]) {
  globalThis.fetch = async () => new Response("error code: " + status, { status });
  assert.deepEqual(await call("/releases/channels/public.json"), [503, "Veyra's servers are offline.\n"]);
}
// The backend's own answers pass through untouched, errors included.
globalThis.fetch = async () => new Response('{"error":"invalid_credentials"}', { status: 401 });
assert.deepEqual(await call("/v1/me"), [401, '{"error":"invalid_credentials"}']);
console.log("worker routing: all checks passed");
