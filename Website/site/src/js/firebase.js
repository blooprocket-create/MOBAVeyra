// Firebase Authentication over its REST API (ADR-038 §2, ADR-070 §3), the same calls the launcher makes from
// Rust: register, sign in, a password-reset email, and looking up the signed-in user. Passwords go only to
// Firebase. Nothing here logs a request or an answer, because they hold credentials.

/** Why Firebase refused, in the launcher's terms (Launcher/core/src/firebase.rs, Refusal). */
export function refusalFromMessage(message) {
  // Firebase's message is a code, sometimes followed by " : " and an explanation.
  const code = String(message ?? "").split(" : ")[0].trim();
  switch (code) {
    case "EMAIL_EXISTS":
      return "email-exists";
    case "INVALID_LOGIN_CREDENTIALS":
    case "INVALID_PASSWORD":
    case "EMAIL_NOT_FOUND":
      return "wrong-email-or-password";
    case "WEAK_PASSWORD":
      return "weak-password";
    case "INVALID_EMAIL":
    case "MISSING_EMAIL":
      return "invalid-email";
    case "MISSING_PASSWORD":
      return "missing-password";
    case "TOO_MANY_ATTEMPTS_TRY_LATER":
      return "too-many-attempts";
    case "USER_DISABLED":
      return "user-disabled";
    case "OPERATION_NOT_ALLOWED":
    case "PASSWORD_LOGIN_DISABLED":
      return "not-enabled";
    case "INVALID_ID_TOKEN":
    case "TOKEN_EXPIRED":
    case "USER_NOT_FOUND":
      return "session-expired";
    default:
      return code.startsWith("API_KEY") || code === "INVALID_API_KEY" ? "bad-api-key" : "other";
  }
}

export class FirebaseError extends Error {
  /** kind: "unreachable", "refused" or "bad-answer"; refusal: refusalFromMessage's answer when refused. */
  constructor(kind, refusal = null) {
    super(kind === "refused" ? `Firebase refused: ${refusal}` : `Firebase: ${kind}`);
    this.kind = kind;
    this.refusal = refusal;
  }
}

/** What to tell the player, in a sentence. */
export function describeFirebaseError(error, { passwordMinLength } = {}) {
  if (!(error instanceof FirebaseError) || error.kind !== "refused") {
    return error?.kind === "unreachable"
      ? "We couldn't reach the sign-in service. Check your connection and try again."
      : "The sign-in service answered in a way we didn't expect. Try again in a moment.";
  }
  switch (error.refusal) {
    case "email-exists":
      return "An account already uses this email. Log in instead.";
    case "wrong-email-or-password":
      return "That email and password don't match an account.";
    case "weak-password":
      return `Choose a stronger password${passwordMinLength ? `: at least ${passwordMinLength} characters` : ""}.`;
    case "invalid-email":
      return "Enter a valid email address.";
    case "missing-password":
      return "Enter your password.";
    case "too-many-attempts":
      return "Too many attempts. Wait a few minutes, then try again.";
    case "user-disabled":
      return "This account has been disabled.";
    case "session-expired":
      return "Your session has expired. Log in again.";
    case "not-enabled":
    case "bad-api-key":
      return "Accounts can't be created or signed in to from the website right now. Try again later.";
    default:
      return "The sign-in service refused. Try again in a moment.";
  }
}

const isIdToken = (text) => typeof text === "string" && text.length <= 8 * 1024 && /^[\w-]+\.[\w-]+\.[\w-]+$/.test(text);

/** A client for the project the configuration names: { authUrl, apiKey }. */
export function createFirebase({ authUrl, apiKey }, fetchImpl = (...args) => globalThis.fetch(...args)) {
  const base = authUrl.replace(/\/+$/, "");

  async function post(route, body) {
    let response;
    try {
      response = await fetchImpl(`${base}/v1/${route}?key=${encodeURIComponent(apiKey)}`, {
        method: "POST",
        headers: { "content-type": "application/json", accept: "application/json" },
        body: JSON.stringify(body),
      });
    } catch {
      throw new FirebaseError("unreachable");
    }
    let answer = null;
    try {
      answer = await response.json();
    } catch {
      // Handled below: a refusal with no body, or a success that is not JSON.
    }
    if (!response.ok) {
      throw new FirebaseError("refused", refusalFromMessage(answer?.error?.message));
    }
    if (answer === null || typeof answer !== "object") {
      throw new FirebaseError("bad-answer");
    }
    return answer;
  }

  async function token(route, email, password) {
    const answer = await post(route, { email, password, returnSecureToken: true });
    const expiresIn = Number(answer.expiresIn);
    if (!isIdToken(answer.idToken) || typeof answer.localId !== "string" || !(expiresIn > 0)) {
      throw new FirebaseError("bad-answer");
    }
    return { idToken: answer.idToken, localId: answer.localId, email: answer.email ?? email, expiresIn };
  }

  return {
    /** Creates a Firebase user; resolves to { idToken, localId, email, expiresIn }. */
    signUp: (email, password) => token("accounts:signUp", email, password),
    /** Signs in; resolves as signUp does. */
    signIn: (email, password) => token("accounts:signInWithPassword", email, password),
    /** Emails a reset link. Firebase answers alike whether or not the email has an account. */
    async sendPasswordReset(email) {
      await post("accounts:sendOobCode", { requestType: "PASSWORD_RESET", email });
    },
    /** The signed-in user's email, verification and creation time. */
    async lookup(idToken) {
      const answer = await post("accounts:lookup", { idToken });
      const user = Array.isArray(answer.users) ? answer.users[0] : null;
      if (!user) {
        throw new FirebaseError("refused", "session-expired");
      }
      return {
        email: user.email ?? "",
        emailVerified: user.emailVerified === true,
        createdAt: user.createdAt ? new Date(Number(user.createdAt)) : null,
      };
    },
  };
}
