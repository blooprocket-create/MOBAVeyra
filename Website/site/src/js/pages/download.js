// The download page: the current Veyra Setup from the release store (version, size, SHA-256), and the guide's
// SmartScreen illustration, which clicks itself through More info and Run anyway while it is on screen.

import config from "../site-config.js";
import { startChrome } from "../chrome.js";
import { formatBytes, latestSetup } from "../release.js";
import { reducedMotion } from "../motion.js";

// How often the page looks again for a Setup while it has none to offer. A presentation value.
const RETRY_MS = 60 * 1000;
// The SmartScreen illustration's beats, in milliseconds. Presentation values.
const SMARTSCREEN_BEATS = Object.freeze({ start: 900, toMore: 900, press: 450, toRun: 1100, hold: 2600 });

const card = document.querySelector("[data-setup]");
const download = card.querySelector("[data-setup-download]");
const label = card.querySelector("[data-setup-label]");
const note = card.querySelector("[data-setup-note]");
let retry = 0;

function setText(selector, text) {
  document.querySelectorAll(selector).forEach((element) => (element.textContent = text));
}

function unavailable(labelText, noteText) {
  download.setAttribute("aria-disabled", "true");
  download.removeAttribute("download");
  download.href = "#install";
  label.textContent = labelText;
  note.textContent = noteText;
  for (const selector of ["[data-setup-version]", "[data-setup-size]", "[data-setup-hash]"]) {
    setText(selector, "—");
  }
  card.querySelector("[data-setup-copy]").hidden = true;
}

async function refresh() {
  clearTimeout(retry);
  const setup = await latestSetup(config.releases);
  card.dataset.state = setup.state;
  if (setup.state === "ready") {
    setText("[data-setup-version]", setup.version);
    setText("[data-setup-size]", formatBytes(setup.size));
    setText("[data-setup-hash]", setup.sha256);
    setText("[data-setup-filename]", setup.fileName);
    setText("[data-hash-command]", `Get-FileHash "$env:USERPROFILE\\Downloads\\${setup.fileName}" -Algorithm SHA256`);
    setText("[data-hash-sample]", `SHA256     ${setup.sha256.toUpperCase().slice(0, 24)}…`);
    card.querySelector("[data-setup-copy]").hidden = false;
    download.href = setup.url;
    download.setAttribute("download", setup.fileName);
    download.removeAttribute("aria-disabled");
    label.textContent = `Download Veyra Setup ${setup.version}`;
    note.textContent = `${setup.fileName} · ${formatBytes(setup.size)}`;
    return;
  }
  if (setup.state === "offline") {
    unavailable("Download unavailable", "Downloads come from Veyra's servers, which are offline right now. This page checks again every minute.");
  } else {
    unavailable("Not published yet", "There's no Veyra Setup published for players yet. Check back soon.");
  }
  retry = setTimeout(refresh, RETRY_MS);
}

download.addEventListener("click", (event) => {
  if (download.getAttribute("aria-disabled") === "true") {
    event.preventDefault();
    return;
  }
  note.textContent = "Your download should start now. Next, follow the steps below.";
});

// The servers coming back is the moment to look again.
document.addEventListener("veyra:server-status", (event) => {
  if (event.detail === "online" && card.dataset.state !== "ready" && card.dataset.state !== "loading") {
    refresh();
  }
});

/** Moves the illustration's pointer onto a target inside it. */
function pointAt(mock, pointer, target) {
  const box = mock.getBoundingClientRect();
  const spot = target.getBoundingClientRect();
  pointer.style.setProperty("--x", `${spot.left - box.left + spot.width * 0.45}px`);
  pointer.style.setProperty("--y", `${spot.top - box.top + spot.height * 0.55}px`);
}

function press(target) {
  target.classList.remove("is-pressed");
  void target.offsetWidth;
  target.classList.add("is-pressed");
}

function playSmartScreen(mock) {
  if (!mock || reducedMotion()) {
    mock?.classList.add("is-expanded");
    return;
  }
  const pointer = mock.querySelector(".mock__pointer");
  const more = mock.querySelector('[data-target="more"]');
  const run = mock.querySelector('[data-target="run"]');
  const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  let visible = false;
  let playing = false;

  async function loop() {
    playing = true;
    while (visible) {
      mock.classList.remove("is-expanded");
      pointer.style.setProperty("--x", "85%");
      pointer.style.setProperty("--y", "110%");
      await wait(SMARTSCREEN_BEATS.start);
      pointAt(mock, pointer, more);
      await wait(SMARTSCREEN_BEATS.toMore);
      press(more);
      mock.classList.add("is-expanded");
      await wait(SMARTSCREEN_BEATS.press);
      pointAt(mock, pointer, run);
      await wait(SMARTSCREEN_BEATS.toRun);
      press(run);
      await wait(SMARTSCREEN_BEATS.hold);
    }
    playing = false;
  }

  new IntersectionObserver(([entry]) => {
    visible = entry.isIntersecting;
    if (visible && !playing) {
      loop();
    }
  }, { threshold: 0.4 }).observe(mock);
}

function useLauncherArt() {
  const art = config.vanguards[Math.floor(Math.random() * config.vanguards.length)]?.art;
  if (art) {
    document.querySelectorAll(".mock--launcher").forEach((mock) => mock.style.setProperty("--art", `url("${art}")`));
  }
}

startChrome();
useLauncherArt();
playSmartScreen(document.querySelector("[data-smartscreen]"));
refresh();
