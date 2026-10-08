// Creating an account (ADR-070 §3): the display name is checked here first, Firebase creates the sign-in, then
// Veyra's backend creates the account with its name. Each way that can end has its own words below.

import config from "../site-config.js";
import { createAccount, finishAccount } from "../account-flow.js";
import { createBackend } from "../backend.js";
import { startChrome, showSignedIn } from "../chrome.js";
import { createFirebase, describeFirebaseError } from "../firebase.js";
import { displayNameField, notify, passwordReveal, showAuthArt, showStep, whileBusy } from "../forms.js";
import { serverHoursPhrase } from "../hours.js";
import { looksLikeEmail } from "../rules.js";
import { clearNeedsName, saveSession, setNeedsName } from "../session.js";

const nameRules = config.account.displayName;
const passwordMinLength = config.account.passwordMinLength;
const clients = { firebase: createFirebase(config.firebase), backend: createBackend(), nameRules };

const card = document.querySelector(".auth__card");
const form = card.querySelector("[data-signup-form]");
const chooseForm = card.querySelector("[data-choose-form]");
const passwordInput = document.getElementById("signup-password");
const checkName = displayNameField(form.elements.displayName, nameRules);
const checkChosenName = displayNameField(chooseForm.elements.displayName, nameRules);
card.querySelector("[data-password-hint]").textContent = `At least ${passwordMinLength} characters.`;

// The Firebase sign-in made by this page, kept while the account still needs its name.
let pendingAuth = null;

const NAME_PROBLEM = `Choose a display name of ${nameRules.minLength}–${nameRules.maxLength} letters, digits or underscores.`;
const OFFLINE = `Veyra's servers are offline right now, so new accounts can't be finished. Nothing was created: try again during the servers' daily hours, ${serverHoursPhrase(config.serverHours)}.`;

function done(account) {
  clearNeedsName();
  pendingAuth = null;
  card.querySelector("[data-done-title]").textContent = `Welcome, ${account.displayName}`;
  card.querySelector("[data-done-text]").textContent =
    "Your Veyra account is ready. Download Veyra Setup, install the launcher, and sign in there with the same email and password.";
  showStep(card, "done");
  showSignedIn();
}

function askForName(auth, intro, problem) {
  pendingAuth = auth;
  saveSession(auth);
  setNeedsName(auth.localId);
  showSignedIn();
  card.querySelector("[data-choose-intro]").textContent = intro;
  showStep(card, "choose-name");
  notify(chooseForm.querySelector("[data-notice]"), problem ? "warning" : "info", problem ?? null);
}

/** Turns an outcome of account-flow.js into the next screen or message. */
function handle(outcome, notice, attemptedName) {
  switch (outcome.kind) {
    case "created":
      saveSession(outcome.auth);
      done(outcome.account);
      return;
    case "invalid-name":
      notify(notice, "problem", NAME_PROBLEM);
      return;
    case "offline":
      notify(notice, "problem", OFFLINE);
      return;
    case "firebase": {
      const refusal = outcome.error.refusal;
      notify(notice, "problem", describeFirebaseError(outcome.error, { passwordMinLength }), refusal === "email-exists" ? { href: "/login", text: "Log in" } : null);
      return;
    }
    case "name-taken":
      askForName(outcome.auth, "Your sign-in is ready.", `Someone already has the name ${attemptedName}. Choose another to finish your account.`);
      return;
    case "already-registered":
      saveSession(outcome.auth);
      clearNeedsName();
      card.querySelector("[data-done-title]").textContent = "You're all set";
      card.querySelector("[data-done-text]").textContent = "This sign-in already has a Veyra account. Download Veyra and sign in to the launcher with it.";
      showStep(card, "done");
      showSignedIn();
      return;
    default:
      // "unfinished": the sign-in exists; the account doesn't yet.
      askForName(
        outcome.auth,
        "Your sign-in was created, but Veyra's servers couldn't finish your account just now.",
        outcome.error?.kind === "offline" || outcome.error?.kind === "unreachable"
          ? "The servers went offline. Try again in a little while, or choose your name in the launcher when you next sign in."
          : "Something went wrong on our side. Try again in a moment, or choose your name in the launcher when you next sign in.",
      );
      chooseForm.elements.displayName.value = attemptedName;
      chooseForm.elements.displayName.dispatchEvent(new Event("input"));
  }
}

form.addEventListener("submit", (event) => {
  event.preventDefault();
  const notice = form.querySelector("[data-notice]");
  const displayName = form.elements.displayName.value.trim();
  const email = form.elements.email.value.trim();
  const password = passwordInput.value;
  form.elements.displayName.value = displayName;
  const nameOk = checkName();
  const emailOk = looksLikeEmail(email);
  const passwordOk = password.length >= passwordMinLength;
  form.elements.email.setAttribute("aria-invalid", String(!emailOk));
  passwordInput.setAttribute("aria-invalid", String(!passwordOk));
  if (!nameOk) {
    notify(notice, "problem", NAME_PROBLEM);
    form.elements.displayName.focus();
    return;
  }
  if (!emailOk) {
    notify(notice, "problem", "Enter a valid email address.");
    form.elements.email.focus();
    return;
  }
  if (!passwordOk) {
    notify(notice, "problem", `Your password needs at least ${passwordMinLength} characters.`);
    passwordInput.focus();
    return;
  }
  notify(notice, null, null);
  whileBusy(form, async () => handle(await createAccount(clients, { displayName, email, password }), notice, displayName));
});

chooseForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const notice = chooseForm.querySelector("[data-notice]");
  const displayName = chooseForm.elements.displayName.value.trim();
  chooseForm.elements.displayName.value = displayName;
  if (!checkChosenName()) {
    notify(notice, "problem", NAME_PROBLEM);
    return;
  }
  whileBusy(chooseForm, async () => {
    const outcome = await finishAccount(clients, pendingAuth, displayName);
    if (outcome.kind === "name-taken") {
      notify(notice, "problem", `Someone already has the name ${displayName}. Try another.`);
    } else if (outcome.kind === "unfinished") {
      notify(notice, "warning", "Veyra's servers couldn't finish your account just now. Try again in a little while.");
    } else {
      handle(outcome, notice, displayName);
    }
  });
});

passwordReveal(card);
showAuthArt(config.vanguards);
startChrome();
