// Creating a Veyra account from the website (ADR-070 §3), in ADR-038's two steps: Firebase creates the user, then
// the backend creates the account with its display name. The two cannot share a transaction, so each outcome
// says what is left to do. The pages turn these outcomes into words; this module holds no wording.

import { BackendError } from "./backend.js";
import { FirebaseError } from "./firebase.js";
import { isValidDisplayName } from "./rules.js";

/**
 * Registers a player: { displayName, email, password }. Resolves to one of:
 * - { kind: "invalid-name" }: the name breaks the rules; nothing was created;
 * - { kind: "offline" }: the servers are offline, so nothing was created;
 * - { kind: "firebase", error }: Firebase refused or could not be reached; nothing was created;
 * - { kind: "created", account, auth }: done;
 * - { kind: "name-taken", auth }: the Firebase user exists; choose another name with finishAccount;
 * - { kind: "already-registered", auth }: this identity already has an account;
 * - { kind: "unfinished", auth, error }: the Firebase user exists but the account could not be made now.
 */
export async function createAccount({ firebase, backend, nameRules }, { displayName, email, password }) {
  if (!isValidDisplayName(displayName, nameRules)) {
    return { kind: "invalid-name" };
  }
  // Ask first, so a player isn't left with a Firebase user and no account while the host is down. It can
  // still go down in between; that is "unfinished".
  if ((await backend.status()) === "offline") {
    return { kind: "offline" };
  }
  let auth;
  try {
    auth = await firebase.signUp(email, password);
  } catch (error) {
    if (error instanceof FirebaseError) {
      return { kind: "firebase", error };
    }
    throw error;
  }
  return finishAccount({ backend, nameRules }, auth, displayName);
}

/** The second step alone, for a Firebase user with no Veyra account: create it with this name. */
export async function finishAccount({ backend, nameRules }, auth, displayName) {
  if (!isValidDisplayName(displayName, nameRules)) {
    return { kind: "invalid-name", auth };
  }
  try {
    const account = await backend.register(auth.idToken, displayName);
    return { kind: "created", account, auth };
  } catch (error) {
    if (!(error instanceof BackendError)) {
      throw error;
    }
    if (error.code === "display_name_taken") {
      return { kind: "name-taken", auth };
    }
    if (error.code === "invalid_display_name") {
      return { kind: "invalid-name", auth };
    }
    if (error.code === "already_registered") {
      return { kind: "already-registered", auth };
    }
    return { kind: "unfinished", auth, error };
  }
}
