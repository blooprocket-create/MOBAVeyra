// Veyra's backend, reached through the website's own address: vercel.json rewrites these routes to the public
// Worker (ADR-057), so the browser makes no cross-origin request. The website registers accounts and reads the
// servers' state. It never calls /v1/login, which records a launcher login (ADR-049 §1): visiting the website
// must not count as one (Profiles & Identity Bible §4).

export const ROUTES = Object.freeze({
  status: "/api/status",
  register: "/api/register",
  launcherChannel: (channel) => `/api/launcher/${encodeURIComponent(channel)}.json`,
});

// What the Worker answers while the host is down (503), and what the website's host answers when it cannot
// reach the Worker (502, 504): the servers are offline, not refusing.
const OFFLINE_STATUSES = new Set([502, 503, 504]);

export class BackendError extends Error {
  /** kind: "offline", "unreachable", "refused" or "bad-answer"; code: the backend's error code when refused. */
  constructor(kind, code = null) {
    super(kind === "refused" ? `The backend refused: ${code}` : `The backend: ${kind}`);
    this.kind = kind;
    this.code = code;
  }
}

export function createBackend(fetchImpl = (...args) => globalThis.fetch(...args)) {
  return {
    /** "online" when the backend and its database answer, "offline" when they don't, "unknown" when offline ourselves. */
    async status() {
      try {
        const response = await fetchImpl(ROUTES.status, { headers: { accept: "application/json" }, cache: "no-store" });
        return response.ok ? "online" : "offline";
      } catch {
        return "unknown";
      }
    },

    /**
     * Creates the Veyra account for a Firebase ID token with the display name the player chose (ADR-038 §4).
     * Resolves to the account, { id, displayName }. The launcher session the backend also returns is dropped:
     * the website has no use for it, and players sign in to the launcher themselves.
     */
    async register(providerToken, displayName) {
      let response;
      try {
        response = await fetchImpl(ROUTES.register, {
          method: "POST",
          headers: { "content-type": "application/json", accept: "application/json" },
          body: JSON.stringify({ providerToken, displayName }),
        });
      } catch {
        throw new BackendError("unreachable");
      }
      if (OFFLINE_STATUSES.has(response.status)) {
        throw new BackendError("offline");
      }
      let answer = null;
      try {
        answer = await response.json();
      } catch {
        // Handled below.
      }
      if (!response.ok) {
        const code = typeof answer?.error === "string" ? answer.error : `http_${response.status}`;
        throw new BackendError("refused", code);
      }
      const account = answer?.account;
      if (typeof account?.id !== "string" || typeof account?.displayName !== "string") {
        throw new BackendError("bad-answer");
      }
      return { id: account.id, displayName: account.displayName };
    },
  };
}
