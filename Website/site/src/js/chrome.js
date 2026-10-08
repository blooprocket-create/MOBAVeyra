// What every page shares: the header (its menu, its scrolled state and the account links), the servers' state and
// daily hours wherever the page shows them, the footer's year and the ambient motion.

import config from "./site-config.js";
import { createBackend } from "./backend.js";
import { serverHoursPhrase, serverNote } from "./hours.js";
import { burstOnPress, revealOnScroll, startMotes } from "./motion.js";
import { currentSession } from "./session.js";

// How often a page asks again whether the servers are up while it stays open. A presentation value.
const STATUS_REFRESH_MS = 60 * 1000;

const STATUS_TEXT = {
  checking: "Checking Veyra's servers…",
  online: "Servers online",
  offline: "Servers offline",
  unknown: "Couldn't check the servers",
};

function setStatus(state) {
  for (const element of document.querySelectorAll("[data-server-status]")) {
    element.dataset.state = state;
    const text = element.querySelector(".status__text");
    if (text) {
      text.textContent = STATUS_TEXT[state];
    }
  }
  const note = serverNote(config.serverHours, state);
  for (const element of document.querySelectorAll("[data-server-note]")) {
    element.textContent = note;
    element.hidden = note === "";
  }
  document.dispatchEvent(new CustomEvent("veyra:server-status", { detail: state }));
}

/** Keeps every [data-server-status] on the page current; resolves to the first answer. */
export function watchServerStatus(backend = createBackend()) {
  if (!document.querySelector("[data-server-status]")) {
    return Promise.resolve(null);
  }
  const check = async () => {
    const state = await backend.status();
    setStatus(state);
    return state;
  };
  setInterval(() => {
    if (!document.hidden) {
      check();
    }
  }, STATUS_REFRESH_MS);
  return check();
}

function setUpHeader() {
  const header = document.querySelector("[data-header]");
  if (!header) {
    return;
  }
  const toggle = header.querySelector("[data-nav-toggle]");
  const close = () => {
    header.classList.remove("is-open");
    toggle?.setAttribute("aria-expanded", "false");
  };
  toggle?.addEventListener("click", () => {
    const open = !header.classList.contains("is-open");
    header.classList.toggle("is-open", open);
    toggle.setAttribute("aria-expanded", String(open));
  });
  header.querySelectorAll(".site-nav a").forEach((link) => link.addEventListener("click", close));
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape" && header.classList.contains("is-open")) {
      close();
      toggle?.focus();
    }
  });
  const onScroll = () => header.classList.toggle("is-scrolled", scrollY > 24);
  addEventListener("scroll", onScroll, { passive: true });
  onScroll();
}

/** Shows Account instead of Log in and Sign up while this tab is signed in. */
export function showSignedIn() {
  const signedIn = currentSession() !== null;
  document.querySelectorAll("[data-signed-in]").forEach((element) => (element.hidden = !signedIn));
  document.querySelectorAll("[data-signed-out]").forEach((element) => (element.hidden = signedIn));
}

/** Copies the text of the element a [data-copy] button names, and says so on the button. */
function setUpCopyButtons() {
  document.addEventListener("click", async (event) => {
    const button = event.target.closest?.("[data-copy]");
    if (!button) {
      return;
    }
    const source = document.getElementById(button.dataset.copy);
    if (!source) {
      return;
    }
    const label = button.textContent;
    try {
      await navigator.clipboard.writeText(source.textContent.trim());
      button.textContent = "Copied";
      button.classList.add("is-copied");
    } catch {
      button.textContent = "Select and copy";
    }
    setTimeout(() => {
      button.textContent = label;
      button.classList.remove("is-copied");
    }, 1800);
  });
}

/** Starts what every page shares; resolves to the servers' first state, or null on a page that doesn't show it. */
export function startChrome() {
  document.documentElement.classList.add("js");
  setUpHeader();
  showSignedIn();
  setUpCopyButtons();
  // Art framing from the markup (data-focus), applied here because the content security policy forbids style attributes.
  document.querySelectorAll("[data-focus]").forEach((element) => element.style.setProperty("--focus", element.dataset.focus));
  document.querySelectorAll("[data-year]").forEach((element) => (element.textContent = String(new Date().getFullYear())));
  // The build wrote the hours on the host's clock; this adds the viewer's own when it reads differently.
  const hours = serverHoursPhrase(config.serverHours);
  document.querySelectorAll("[data-server-hours]").forEach((element) => (element.textContent = hours));
  startMotes(document.querySelector("[data-motes]"));
  revealOnScroll();
  burstOnPress();
  return watchServerStatus();
}
