// The website's own modules, with fetch and storage replaced: `node --test "Website/site/tests/*.test.mjs"`.

import assert from "node:assert/strict";
import { describe, test } from "node:test";
import { createAccount, finishAccount } from "../src/js/account-flow.js";
import { BackendError, createBackend, ROUTES } from "../src/js/backend.js";
import { createFirebase, describeFirebaseError, FirebaseError, refusalFromMessage } from "../src/js/firebase.js";
import { describeServerHours, formatWait, serverHoursPhrase, serverNote, sessionAt } from "../src/js/hours.js";
import { formatBytes, latestSetup, parseLauncherChannel, setupDownloadUrl } from "../src/js/release.js";
import { displayNameProblems, isValidDisplayName } from "../src/js/rules.js";

const nameRules = { minLength: 3, maxLength: 16, pattern: "^[A-Za-z0-9_]+$" };
const ID_TOKEN = "eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiJ1MSJ9.c2lnbmF0dXJl";
const json = (status, body) => new Response(typeof body === "string" ? body : JSON.stringify(body), { status, headers: { "content-type": "application/json" } });

/** A fetch that answers from a list and records what it was asked. */
function fakeFetch(...answers) {
  const calls = [];
  const fetch = async (url, init = {}) => {
    calls.push({ url: String(url), method: init.method ?? "GET", body: init.body ? JSON.parse(init.body) : undefined });
    const answer = answers.shift();
    if (answer instanceof Error) {
      throw answer;
    }
    return typeof answer === "function" ? answer(url, init) : answer;
  };
  return { fetch, calls };
}

describe("display names", () => {
  // The same cases as the launcher's test (Launcher/core/src/player.rs).
  test("follow the backend's rules", () => {
    for (const good of ["Ember", "abc", "Ember_Wing_12345", "___"]) {
      assert.ok(isValidDisplayName(good, nameRules), good);
    }
    for (const bad of ["ab", "Ember_Wing_123456", "Ember Wing", " Ember", "Émber", ""]) {
      assert.ok(!isValidDisplayName(bad, nameRules), bad);
    }
    assert.deepEqual(displayNameProblems("a b", nameRules), ["characters"]);
    assert.deepEqual(displayNameProblems("ab", nameRules), ["length"]);
  });
});

describe("the servers' hours", () => {
  const hours = { timeZone: "America/New_York", opens: "22:00", closes: "23:30" };
  const at = (iso) => new Date(iso);
  const session = (iso, of = hours) => {
    const { opensAt, closesAt, live } = sessionAt(of, at(iso));
    return { opensAt: opensAt.toISOString(), closesAt: closesAt.toISOString(), live };
  };

  test("find today's session, the one running, or tomorrow's", () => {
    // 2:00 PM in New York, daylight time (UTC-4).
    assert.deepEqual(session("2026-10-07T18:00:00Z"), { opensAt: "2026-10-08T02:00:00.000Z", closesAt: "2026-10-08T03:30:00.000Z", live: false });
    assert.deepEqual(session("2026-10-08T02:30:00Z"), { opensAt: "2026-10-08T02:00:00.000Z", closesAt: "2026-10-08T03:30:00.000Z", live: true });
    assert.deepEqual(session("2026-10-08T03:30:00Z"), { opensAt: "2026-10-09T02:00:00.000Z", closesAt: "2026-10-09T03:30:00.000Z", live: false });
  });

  test("follow the host's clock through standard and daylight time", () => {
    assert.equal(session("2026-12-01T12:00:00Z").opensAt, "2026-12-02T03:00:00.000Z");
    // The day daylight time ends: the morning is UTC-4, the evening UTC-5.
    assert.equal(session("2026-11-01T12:00:00Z").opensAt, "2026-11-02T03:00:00.000Z");
    assert.equal(session("2026-03-08T12:00:00Z").opensAt, "2026-03-09T02:00:00.000Z");
  });

  test("may run past midnight", () => {
    const late = { ...hours, opens: "23:00", closes: "01:00" };
    // 12:30 AM on the 8th is still the 7th's session.
    assert.deepEqual(session("2026-10-08T04:30:00Z", late), { opensAt: "2026-10-08T03:00:00.000Z", closesAt: "2026-10-08T05:00:00.000Z", live: true });
  });

  test("read on the host's clock, and on the viewer's when it differs", () => {
    const now = at("2026-10-07T18:00:00Z");
    assert.deepEqual(describeServerHours(hours, "America/New_York", now), { host: "10:00–11:30 PM Eastern Time", local: null });
    assert.deepEqual(describeServerHours(hours, "America/Detroit", now), { host: "10:00–11:30 PM Eastern Time", local: null });
    assert.equal(describeServerHours(hours, "America/Los_Angeles", now).local, "7:00–8:30 PM your time");
    assert.equal(describeServerHours(hours, "Asia/Kolkata", now).local, "7:30–9:00 AM your time");
    assert.equal(describeServerHours(hours, "Asia/Tokyo", now).local, "11:00 AM–12:30 PM your time");
    assert.equal(describeServerHours(hours, null, now).local, null);
    assert.equal(serverHoursPhrase(hours, "America/Los_Angeles", now), "10:00–11:30 PM Eastern Time (7:00–8:30 PM your time)");
  });

  test("explain the live state", () => {
    assert.match(serverNote(hours, "offline", at("2026-10-08T02:20:00Z")), /starts at 10:00 PM Eastern Time.*run out of usage/);
    assert.equal(serverNote(hours, "offline", at("2026-10-07T18:00:00Z")), "The next session opens in 8 h.");
    assert.equal(serverNote(hours, "offline", at("2026-10-08T01:15:30Z")), "The next session opens in 45 min.");
    assert.equal(serverNote(hours, "online", at("2026-10-08T02:20:00Z")), "Today's session runs until 11:30 PM Eastern Time.");
    // Online outside the hours (hosted by hand), checking, or unknown: nothing to add.
    assert.equal(serverNote(hours, "online", at("2026-10-07T18:00:00Z")), "");
    assert.equal(serverNote(hours, "checking", at("2026-10-08T02:20:00Z")), "");
    assert.equal(serverNote(hours, "unknown", at("2026-10-08T02:20:00Z")), "");
  });

  test("count down in hours and minutes, never to zero", () => {
    assert.equal(formatWait((3 * 60 + 20) * 60 * 1000), "3 h 20 min");
    assert.equal(formatWait(60 * 60 * 1000 + 1), "1 h 1 min");
    assert.equal(formatWait(1), "1 min");
    assert.equal(formatWait(0), "1 min");
  });
});

describe("Firebase", () => {
  const config ={ authUrl: "https://identitytoolkit.googleapis.com/", apiKey: "key-1" };

  test("reads its refusals as the launcher does", () => {
    assert.equal(refusalFromMessage("EMAIL_EXISTS"), "email-exists");
    assert.equal(refusalFromMessage("INVALID_LOGIN_CREDENTIALS"), "wrong-email-or-password");
    assert.equal(refusalFromMessage("WEAK_PASSWORD : Password should be at least 6 characters"), "weak-password");
    assert.equal(refusalFromMessage("TOO_MANY_ATTEMPTS_TRY_LATER : Access disabled"), "too-many-attempts");
    assert.equal(refusalFromMessage("API_KEY_INVALID"), "bad-api-key");
    assert.equal(refusalFromMessage("SOMETHING_NEW"), "other");
    assert.equal(refusalFromMessage(undefined), "other");
    assert.match(describeFirebaseError(new FirebaseError("refused", "weak-password"), { passwordMinLength: 6 }), /at least 6 characters/);
    assert.match(describeFirebaseError(new FirebaseError("unreachable")), /couldn't reach/);
  });

  test("signs up and in through the REST API", async () => {
    const { fetch, calls } = fakeFetch(json(200, { idToken: ID_TOKEN, localId: "u1", email: "a@b.co", expiresIn: "3600" }), json(200, { idToken: ID_TOKEN, localId: "u1", expiresIn: "3600" }));
    const firebase = createFirebase(config, fetch);
    assert.deepEqual(await firebase.signUp("a@b.co", "secret1"), { idToken: ID_TOKEN, localId: "u1", email: "a@b.co", expiresIn: 3600 });
    assert.equal((await firebase.signIn("a@b.co", "secret1")).email, "a@b.co");
    assert.equal(calls[0].url, "https://identitytoolkit.googleapis.com/v1/accounts:signUp?key=key-1");
    assert.deepEqual(calls[0].body, { email: "a@b.co", password: "secret1", returnSecureToken: true });
    assert.equal(calls[1].url, "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=key-1");
  });

  test("tells refusals, silence and nonsense apart", async () => {
    const firebase = createFirebase(config, fakeFetch(json(400, { error: { message: "EMAIL_EXISTS" } }), new TypeError("offline"), json(200, { idToken: "not a jwt", localId: "u1", expiresIn: "3600" })).fetch);
    await assert.rejects(firebase.signUp("a@b.co", "secret1"), (error) => error.kind === "refused" && error.refusal === "email-exists");
    await assert.rejects(firebase.signUp("a@b.co", "secret1"), (error) => error.kind === "unreachable");
    await assert.rejects(firebase.signUp("a@b.co", "secret1"), (error) => error.kind === "bad-answer");
  });

  test("asks for a reset email and looks up the user", async () => {
    const { fetch, calls } = fakeFetch(json(200, { email: "a@b.co" }), json(200, { users: [{ email: "a@b.co", emailVerified: false, createdAt: "1759800000000" }] }));
    const firebase = createFirebase(config, fetch);
    await firebase.sendPasswordReset("a@b.co");
    assert.deepEqual(calls[0].body, { requestType: "PASSWORD_RESET", email: "a@b.co" });
    const user = await firebase.lookup(ID_TOKEN);
    assert.equal(user.createdAt.getTime(), 1759800000000);
    assert.deepEqual(calls[1].body, { idToken: ID_TOKEN });
  });
});

describe("the backend", () => {
  test("registers through the website's own route and drops the launcher session", async () => {
    const { fetch, calls } = fakeFetch(json(201, { token: "vls_secret", expiresAt: "2026-10-08T00:00:00Z", account: { id: "a1", displayName: "Ember" } }));
    const account = await createBackend(fetch).register(ID_TOKEN, "Ember");
    assert.deepEqual(account, { id: "a1", displayName: "Ember" });
    assert.equal(calls[0].url, ROUTES.register);
    assert.equal(calls[0].method, "POST");
    assert.deepEqual(calls[0].body, { providerToken: ID_TOKEN, displayName: "Ember" });
  });

  test("tells a refusal from the servers being offline", async () => {
    const backend = createBackend(fakeFetch(json(409, { error: "display_name_taken" }), new Response("Veyra's servers are offline.\n", { status: 503 }), new Response("", { status: 504 }), new TypeError("down"), json(500, "oops")).fetch);
    await assert.rejects(backend.register(ID_TOKEN, "Ember"), (error) => error instanceof BackendError && error.kind === "refused" && error.code === "display_name_taken");
    await assert.rejects(backend.register(ID_TOKEN, "Ember"), (error) => error.kind === "offline");
    await assert.rejects(backend.register(ID_TOKEN, "Ember"), (error) => error.kind === "offline");
    await assert.rejects(backend.register(ID_TOKEN, "Ember"), (error) => error.kind === "unreachable");
    await assert.rejects(backend.register(ID_TOKEN, "Ember"), (error) => error.kind === "refused" && error.code === "http_500");
  });

  test("reads the servers' state", async () => {
    const backend = createBackend(fakeFetch(json(200, { status: "ready" }), new Response("", { status: 503 }), new TypeError("down")).fetch);
    assert.equal(await backend.status(), "online");
    assert.equal(await backend.status(), "offline");
    assert.equal(await backend.status(), "unknown");
  });
});

describe("the Setup download", () => {
  const channel = { schemaVersion: 1, version: "0.2.3", setup: "a".repeat(64), size: 9437184 };
  const releases = { url: "https://veyra.example/releases", channel: "public" };

  test("checks the launcher channel file as the launcher does", () => {
    assert.deepEqual(parseLauncherChannel(channel), { version: "0.2.3", sha256: "a".repeat(64), size: 9437184 });
    for (const bad of [{ ...channel, schemaVersion: 2 }, { ...channel, setup: "A".repeat(64) }, { ...channel, size: 0 }, { ...channel, version: "0.2 3" }, { ...channel, extra: 1 }, null, "text"]) {
      assert.equal(parseLauncherChannel(bad), null, JSON.stringify(bad));
    }
  });

  test("names Setup where the store keeps it for people", () => {
    assert.equal(setupDownloadUrl("https://veyra.example/releases/", "0.2.3", "public"), "https://veyra.example/releases/setup/VeyraSetup-0.2.3-public.exe");
    assert.equal(formatBytes(9437184), "9.4 MB");
    assert.equal(formatBytes(512), "512 bytes");
    assert.equal(formatBytes(123456789), "123 MB");
  });

  test("is ready, offline or unavailable", async () => {
    const { fetch, calls } = fakeFetch(json(200, channel), new Response("", { status: 503 }), new TypeError("down"), new Response("", { status: 404 }), json(200, { nope: 1 }));
    const ready = await latestSetup(releases, fetch);
    assert.equal(calls[0].url, "/api/launcher/public.json");
    assert.deepEqual(ready, { state: "ready", version: "0.2.3", sha256: "a".repeat(64), size: 9437184, url: "https://veyra.example/releases/setup/VeyraSetup-0.2.3-public.exe", fileName: "VeyraSetup-0.2.3-public.exe" });
    assert.equal((await latestSetup(releases, fetch)).state, "offline");
    assert.equal((await latestSetup(releases, fetch)).state, "offline");
    assert.equal((await latestSetup(releases, fetch)).state, "unavailable");
    assert.equal((await latestSetup(releases, fetch)).state, "unavailable");
  });
});

describe("creating an account", () => {
  const auth = { idToken: ID_TOKEN, localId: "u1", email: "a@b.co", expiresIn: 3600 };
  const form = { displayName: "Ember", email: "a@b.co", password: "secret1" };

  function clients({ status = "online", signUp = async () => auth, register = async () => ({ id: "a1", displayName: "Ember" }) } = {}) {
    const calls = { signUp: 0, register: 0 };
    return {
      calls,
      firebase: { signUp: async (...args) => (calls.signUp++, signUp(...args)) },
      backend: { status: async () => status, register: async (...args) => (calls.register++, register(...args)) },
      nameRules,
    };
  }

  test("creates the sign-in, then the account", async () => {
    const c = clients();
    assert.deepEqual(await createAccount(c, form), { kind: "created", account: { id: "a1", displayName: "Ember" }, auth });
    assert.deepEqual(c.calls, { signUp: 1, register: 1 });
  });

  test("creates nothing for a bad name, or while the servers are offline", async () => {
    const bad = clients();
    assert.equal((await createAccount(bad, { ...form, displayName: "no spaces" })).kind, "invalid-name");
    const offline = clients({ status: "offline" });
    assert.equal((await createAccount(offline, form)).kind, "offline");
    assert.deepEqual([bad.calls.signUp, offline.calls.signUp], [0, 0]);
  });

  test("passes on Firebase's refusal", async () => {
    const outcome = await createAccount(clients({ signUp: async () => { throw new FirebaseError("refused", "email-exists"); } }), form);
    assert.equal(outcome.kind, "firebase");
    assert.equal(outcome.error.refusal, "email-exists");
  });

  test("keeps the sign-in when the account can't be made yet", async () => {
    const refuse = (kind, code) => clients({ register: async () => { throw new BackendError(kind, code); } });
    assert.equal((await createAccount(refuse("refused", "display_name_taken"), form)).kind, "name-taken");
    assert.equal((await createAccount(refuse("refused", "already_registered"), form)).kind, "already-registered");
    const unfinished = await createAccount(refuse("offline"), form);
    assert.equal(unfinished.kind, "unfinished");
    assert.equal(unfinished.auth, auth);
  });

  test("finishes with another name", async () => {
    const c = clients({ register: async (token, name) => ({ id: "a1", displayName: name }) });
    assert.equal((await finishAccount(c, auth, "Ember_2")).account.displayName, "Ember_2");
    assert.equal((await finishAccount(c, auth, "x")).kind, "invalid-name");
    assert.deepEqual(c.calls, { signUp: 0, register: 1 });
  });
});

describe("the website's sign-in", async () => {
  // session.js reads sessionStorage, which Node lacks: give it one.
  const store = new Map();
  globalThis.sessionStorage = { getItem: (key) => store.get(key) ?? null, setItem: (key, value) => store.set(key, String(value)), removeItem: (key) => store.delete(key) };
  const { currentSession, endSession, needsName, saveSession, setNeedsName } = await import("../src/js/session.js");

  test("lasts until the token expires, and no longer", () => {
    saveSession({ idToken: ID_TOKEN, localId: "u1", email: "a@b.co", expiresIn: 3600 }, 0);
    assert.equal(currentSession(1000).email, "a@b.co");
    assert.equal(currentSession(3600 * 1000), null);
    assert.equal(store.size, 0);
  });

  test("remembers an account that still needs its name, until sign-out", () => {
    setNeedsName("u1");
    assert.ok(needsName("u1"));
    assert.ok(!needsName("u2"));
    endSession();
    assert.ok(!needsName("u1"));
  });

  test("survives storage that refuses", () => {
    globalThis.sessionStorage.setItem = () => {
      throw new Error("quota");
    };
    saveSession({ idToken: ID_TOKEN, localId: "u1", email: "a@b.co", expiresIn: 3600 });
    assert.equal(currentSession(), null);
  });
});
