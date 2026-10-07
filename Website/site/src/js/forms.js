// What the account pages' forms share: the art beside them, the display-name field's live rules, showing a
// password, a form's message, and a button that is busy while a request runs.

import { describeDisplayName, displayNameProblems } from "./rules.js";

/** A featured Vanguard beside the form, a different one each visit, as the launcher does. */
export function showAuthArt(vanguards) {
  const vanguard = vanguards[Math.floor(Math.random() * vanguards.length)];
  const image = document.querySelector("[data-auth-art-image]");
  if (!vanguard || !image) {
    return;
  }
  image.src = vanguard.art;
  image.style.setProperty("--focus", vanguard.focus);
  document.querySelector("[data-auth-art-name]").textContent = vanguard.name;
  const title = document.querySelector("[data-auth-art-title]");
  title.textContent = vanguard.title;
  title.style.setProperty("--hue", vanguard.hue);
}

/** Wires a display-name input to its counter and rule list; returns a check that marks the field. */
export function displayNameField(input, rules) {
  const field = input.closest(".field");
  const count = field.querySelector("[data-name-count]");
  const length = field.querySelector('[data-rule="length"]');
  const characters = field.querySelector('[data-rule="characters"]');
  length.textContent = `${rules.minLength}–${rules.maxLength} characters`;
  input.maxLength = rules.maxLength;
  input.placeholder = describeDisplayName(rules);

  const update = (final) => {
    const name = input.value;
    const problems = displayNameProblems(name, rules);
    const tooShort = [...name].length < rules.minLength;
    count.textContent = `${[...name].length}/${rules.maxLength}`;
    length.classList.toggle("is-met", !problems.includes("length"));
    // Too short is only a problem once the player says they're done.
    length.classList.toggle("is-broken", problems.includes("length") && (final || !tooShort));
    characters.classList.toggle("is-met", name !== "" && !problems.includes("characters"));
    characters.classList.toggle("is-broken", problems.includes("characters"));
    const valid = problems.length === 0;
    if (final || valid) {
      input.setAttribute("aria-invalid", String(!valid));
    }
    return valid;
  };
  input.addEventListener("input", () => update(false));
  update(false);
  return () => update(true);
}

/** Show / Hide beside a password. */
export function passwordReveal(root = document) {
  root.querySelectorAll("[data-reveal-password]").forEach((button) => {
    const input = document.getElementById(button.getAttribute("aria-controls"));
    button.addEventListener("click", () => {
      const show = input.type === "password";
      input.type = show ? "text" : "password";
      button.textContent = show ? "Hide" : "Show";
      button.setAttribute("aria-pressed", String(show));
    });
  });
}

/** A form's message: kind is "problem", "info" or "warning"; null text hides it. Text may carry one link. */
export function notify(notice, kind, text, link) {
  if (!notice) {
    return;
  }
  if (!text) {
    notice.hidden = true;
    notice.textContent = "";
    return;
  }
  notice.className = `notice notice--${kind}`;
  notice.textContent = text;
  if (link) {
    const anchor = document.createElement("a");
    anchor.href = link.href;
    anchor.textContent = link.text;
    notice.append(" ", anchor);
  }
  // Re-run the entrance (a shake for a problem) even when the same message comes twice.
  notice.hidden = true;
  void notice.offsetWidth;
  notice.hidden = false;
}

/** Marks a submit button busy while work runs, and the form's fields read-only; a second submit is ignored. */
export async function whileBusy(form, work) {
  if (form.dataset.busy === "true") {
    return undefined;
  }
  const button = form.querySelector('[type="submit"]');
  const fields = [...form.querySelectorAll("input")];
  form.dataset.busy = "true";
  button.classList.add("is-busy");
  button.setAttribute("aria-busy", "true");
  fields.forEach((field) => (field.readOnly = true));
  try {
    return await work();
  } finally {
    form.dataset.busy = "false";
    button.classList.remove("is-busy");
    button.removeAttribute("aria-busy");
    fields.forEach((field) => (field.readOnly = false));
  }
}

/** Shows one [data-step] of a card and hides the others; moves focus to its heading. */
export function showStep(card, name) {
  card.querySelectorAll("[data-step]").forEach((step) => (step.hidden = step.dataset.step !== name));
  const heading = card.querySelector(`[data-step="${name}"] h1`);
  if (heading) {
    heading.tabIndex = -1;
    heading.focus({ preventScroll: true });
  }
}
