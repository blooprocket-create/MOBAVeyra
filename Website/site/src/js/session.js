// The website's sign-in, kept in this tab only (sessionStorage) and only until Firebase's ID token expires: the
// website remembers no login, as the launcher doesn't (ADR-005 L4). Storage can be missing or refuse (a private
// window, blocked site data); then the website works for the page in hand and forgets on the next.

const SESSION_KEY = "veyra.session";
const PENDING_NAME_KEY = "veyra.pendingName";
// Treat a token as spent a little before Firebase does, so a request made with it doesn't fail in flight.
const EXPIRY_MARGIN_MS = 60 * 1000;

function storage() {
  try {
    return globalThis.sessionStorage ?? null;
  } catch {
    return null;
  }
}

function read(key) {
  try {
    const text = storage()?.getItem(key);
    return text ? JSON.parse(text) : null;
  } catch {
    return null;
  }
}

function write(key, value) {
  try {
    if (value === null) {
      storage()?.removeItem(key);
    } else {
      storage()?.setItem(key, JSON.stringify(value));
    }
  } catch {
    // Storage refused: the session lasts this page.
  }
}

/** Keeps a Firebase sign-in: { idToken, localId, email, expiresIn }. */
export function saveSession(auth, now = Date.now()) {
  write(SESSION_KEY, { idToken: auth.idToken, localId: auth.localId, email: auth.email, expiresAt: now + auth.expiresIn * 1000 });
}

/** The sign-in, or null when there is none or it has expired. */
export function currentSession(now = Date.now()) {
  const session = read(SESSION_KEY);
  if (!session || typeof session.idToken !== "string" || !(session.expiresAt - EXPIRY_MARGIN_MS > now)) {
    if (session) {
      write(SESSION_KEY, null);
    }
    return null;
  }
  return session;
}

export function endSession() {
  write(SESSION_KEY, null);
  write(PENDING_NAME_KEY, null);
}

/** Whether this sign-in's Veyra account still needs a display name (registration's second step failed). */
export function needsName(localId) {
  return read(PENDING_NAME_KEY) === localId;
}

export function setNeedsName(localId) {
  write(PENDING_NAME_KEY, localId);
}

export function clearNeedsName() {
  write(PENDING_NAME_KEY, null);
}
