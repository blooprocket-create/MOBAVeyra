// Veyra's one public address (ADR-057): the backend's routes go to the backend, /releases/ to the release store,
// each through the tunnel Game/Scripts/Host.ps1 last opened on the host PC, whose address it keeps in this Worker's
// secrets API_ORIGIN and RELEASES_ORIGIN. Anything else answers with the placeholder page (Website/README.md).

const PLACEHOLDER = "Veyra is in development.\n";
const OFFLINE = "Veyra's servers are offline.\n";

// The backend's routes, which keep their paths: the launcher and the game take a bare base URL, with no path.
const BACKEND_PATHS = ["/v1/", "/healthz", "/readyz"];
// Match servers reach the backend inside Docker, never through here, so their routes are refused.
const SERVER_PATHS = "/v1/server/";
const RELEASES_PREFIX = "/releases";
// What Cloudflare answers for an origin it cannot reach, such as a quick tunnel that closed (ADR-062 §5): a bad
// gateway, and its own origin errors. The backend and the release store never send these themselves.
const BAD_GATEWAY = 502;
const ORIGIN_ERRORS_FIRST = 520;
const ORIGIN_ERRORS_LAST = 530;

function text(body, status) {
  return new Response(body, { status, headers: { "content-type": "text/plain; charset=utf-8", "cache-control": "no-store" } });
}

/** The request, sent on to origin with path in place of its own; 503 while the host has no tunnel open. */
async function forward(request, origin, path, search) {
  if (!origin) {
    return text(OFFLINE, 503);
  }
  const target = new URL(path + search, origin);
  let response;
  try {
    response = await fetch(new Request(target, request));
  } catch {
    // The tunnel it last recorded has closed: the host stopped.
    return text(OFFLINE, 503);
  }
  const status = response.status;
  if (status === BAD_GATEWAY || (status >= ORIGIN_ERRORS_FIRST && status <= ORIGIN_ERRORS_LAST)) {
    // Cloudflare could not reach the tunnel: the host stopped, or its PC is off.
    return text(OFFLINE, 503);
  }
  return response;
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const path = url.pathname;
    if (path.startsWith(SERVER_PATHS)) {
      return text("Not found.\n", 404);
    }
    if (BACKEND_PATHS.some((prefix) => path === prefix || path.startsWith(prefix))) {
      return forward(request, env.API_ORIGIN, path, url.search);
    }
    if (path.startsWith(RELEASES_PREFIX + "/")) {
      return forward(request, env.RELEASES_ORIGIN, path.slice(RELEASES_PREFIX.length), url.search);
    }
    return text(PLACEHOLDER, 200);
  },
};
