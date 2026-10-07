// The account page, for a signed-in tab only. It shows the Firebase sign-in, offers a password reset, and lets a
// player whose account never got its display name finish it (ADR-038 §4). It can't show the display name itself:
// reading it means a Veyra session, and the website never takes a launcher session (ADR-070 §4).

import config from "../site-config.js";
import { finishAccount } from "../account-flow.js";
import { createBackend } from "../backend.js";
import { startChrome } from "../chrome.js";
import { createFirebase, describeFirebaseError, FirebaseError } from "../firebase.js";
import { displayNameField, notify, whileBusy } from "../forms.js";
import { clearNeedsName, currentSession, endSession, needsName } from "../session.js";

const session = currentSession();
if (!session) {
  location.replace(`/login?next=${encodeURIComponent("/account")}`);
} else {
  const nameRules = config.account.displayName;
  const firebase = createFirebase(config.firebase);
  const clients = { backend: createBackend(), nameRules };
  const accountNotice = document.querySelector("[data-account-notice]");
  const nameCard = document.querySelector("[data-name-card]");
  const nameForm = nameCard.querySelector("[data-name-form]");
  const checkName = displayNameField(nameForm.elements.displayName, nameRules);

  document.querySelector("[data-account-lede]").textContent = `Logged in as ${session.email} in this tab.`;
  document.querySelector("[data-account-email]").textContent = session.email;

  if (needsName(session.localId)) {
    nameCard.querySelector("[data-name-intro]").textContent =
      "Your account doesn't have a display name yet. Choose one to finish it: it's the name every other player sees.";
  }

  firebase
    .lookup(session.idToken)
    .then((user) => {
      document.querySelector("[data-account-created]").textContent = user.createdAt
        ? user.createdAt.toLocaleDateString(undefined, { year: "numeric", month: "long", day: "numeric" })
        : "—";
    })
    .catch((error) => {
      if (error instanceof FirebaseError && error.refusal === "session-expired") {
        endSession();
        location.replace(`/login?next=${encodeURIComponent("/account")}`);
        return;
      }
      document.querySelector("[data-account-created]").textContent = "—";
    });

  nameForm.addEventListener("submit", (event) => {
    event.preventDefault();
    const notice = nameForm.querySelector("[data-notice]");
    const displayName = nameForm.elements.displayName.value.trim();
    nameForm.elements.displayName.value = displayName;
    if (!checkName()) {
      notify(notice, "problem", `Choose a display name of ${nameRules.minLength}–${nameRules.maxLength} letters, digits or underscores.`);
      return;
    }
    whileBusy(nameForm, async () => {
      const auth = currentSession();
      if (!auth) {
        location.replace(`/login?next=${encodeURIComponent("/account")}`);
        return;
      }
      const outcome = await finishAccount(clients, auth, displayName);
      switch (outcome.kind) {
        case "created":
          clearNeedsName();
          nameForm.hidden = true;
          nameCard.querySelector("[data-name-intro]").textContent = `Done: you're ${outcome.account.displayName}. Sign in to the launcher to play.`;
          return;
        case "already-registered":
          clearNeedsName();
          nameForm.hidden = true;
          nameCard.querySelector("[data-name-intro]").textContent = "Your account already has a display name, so nothing changed. You're all set.";
          return;
        case "name-taken":
          notify(notice, "problem", `Someone already has the name ${displayName}. Try another.`);
          return;
        case "invalid-name":
          notify(notice, "problem", `Choose a display name of ${nameRules.minLength}–${nameRules.maxLength} letters, digits or underscores.`);
          return;
        default:
          if (outcome.error?.code === "invalid_credentials") {
            endSession();
            location.replace(`/login?next=${encodeURIComponent("/account")}`);
            return;
          }
          notify(notice, "warning", "Veyra's servers couldn't take that just now. Try again when they're online.");
      }
    });
  });

  document.querySelector("[data-change-password]").addEventListener("click", async (event) => {
    const button = event.currentTarget;
    button.disabled = true;
    try {
      await firebase.sendPasswordReset(session.email);
      notify(accountNotice, "info", `We've emailed a link to ${session.email}. Follow it to choose a new password.`);
    } catch (error) {
      notify(accountNotice, "problem", describeFirebaseError(error));
    } finally {
      button.disabled = false;
    }
  });

  document.querySelector("[data-sign-out]").addEventListener("click", () => {
    endSession();
    location.assign("/");
  });

  startChrome();
}
