// Logging in to the website: Firebase only (ADR-070 §4). The website never trades the sign-in for a launcher
// session, so visiting it never counts as a launcher login. A reset link comes from Firebase.

import config from "../site-config.js";
import { startChrome, showSignedIn } from "../chrome.js";
import { createFirebase, describeFirebaseError } from "../firebase.js";
import { notify, passwordReveal, showAuthArt, showStep, whileBusy } from "../forms.js";
import { looksLikeEmail } from "../rules.js";
import { currentSession, endSession, saveSession } from "../session.js";

const firebase = createFirebase(config.firebase);
const card = document.querySelector(".auth__card");
const loginForm = card.querySelector("[data-login-form]");
const resetForm = card.querySelector("[data-reset-form]");

/** Where to go once signed in: ?next= when it is a path on this site, else the account page. */
function destination() {
  const next = new URLSearchParams(location.search).get("next");
  return next && next.startsWith("/") && !next.startsWith("//") && !next.startsWith("/\\") ? next : "/account";
}

function showSignedInStep(session) {
  card.querySelector("[data-signed-in-as]").textContent = `This tab is logged in as ${session.email}.`;
  showStep(card, "signed-in");
}

loginForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const notice = loginForm.querySelector("[data-notice]");
  const email = loginForm.elements.email.value.trim();
  const password = loginForm.elements.password.value;
  if (!looksLikeEmail(email)) {
    notify(notice, "problem", "Enter the email address of your account.");
    loginForm.elements.email.focus();
    return;
  }
  if (password === "") {
    notify(notice, "problem", "Enter your password.");
    loginForm.elements.password.focus();
    return;
  }
  notify(notice, null, null);
  whileBusy(loginForm, async () => {
    try {
      saveSession(await firebase.signIn(email, password));
      location.assign(destination());
    } catch (error) {
      notify(notice, "problem", describeFirebaseError(error));
    }
  });
});

resetForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const notice = resetForm.querySelector("[data-notice]");
  const email = resetForm.elements.email.value.trim();
  if (!looksLikeEmail(email)) {
    notify(notice, "problem", "Enter a valid email address.");
    return;
  }
  whileBusy(resetForm, async () => {
    try {
      await firebase.sendPasswordReset(email);
      // Firebase answers alike whether or not the email has an account, and so does this.
      notify(notice, "info", `If an account uses ${email}, a reset link is on its way. Check your spam folder if it doesn't arrive.`);
    } catch (error) {
      notify(notice, "problem", describeFirebaseError(error));
    }
  });
});

card.querySelector("[data-show-reset]").addEventListener("click", () => {
  resetForm.elements.email.value = loginForm.elements.email.value.trim();
  showStep(card, "reset");
});

card.querySelector("[data-show-login]").addEventListener("click", () => showStep(card, "login"));

card.querySelector("[data-sign-out]").addEventListener("click", () => {
  endSession();
  showSignedIn();
  showStep(card, "login");
});

passwordReveal(card);
showAuthArt(config.vanguards);
startChrome();
const session = currentSession();
if (session) {
  showSignedInStep(session);
}
