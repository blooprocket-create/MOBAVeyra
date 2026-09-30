// The launcher's window. It shows what the launcher's core reports and asks it to install, update,
// repair and launch; the core, in Rust, does all the work (Launcher/app/src/main.rs, ADR-022 §7).
"use strict";

const invoke = window.__TAURI__.core.invoke;

// How often the window asks how a launch or an install is going, in milliseconds.
const STATUS_INTERVAL_MS = 200;
// The download speed shown is the average over this many milliseconds.
const SPEED_WINDOW_MS = 3000;
// Where the window remembers the last account chosen, on this machine only.
const ACCOUNT_KEY = "veyra.launcher.account";

const sections = ["loading", "install", "installing", "sign-in", "progress", "problem"];
// The Vanguards whose art the launcher carries (art/, launcher.css); one fills the window each time it opens.
const FEATURED = ["bryn", "cairn", "gorraveth", "kade", "mimzi", "oriel", "patch", "qazharr", "raska", "vera"];
const element = (id) => document.getElementById(id);

// The install location shown on the Install screen, and the one the player chose, if they changed it.
let shownFolder = "";
let chosenFolder = null;
// Download samples for the speed: [milliseconds, bytes downloaded].
let samples = [];

function show(id) {
  for (const section of sections) {
    element(section).hidden = section !== id;
  }
}

// Shows a path cut short from the left (launcher.css); the marks keep its separators in place.
function showFolder(path) {
  shownFolder = path;
  element("folder").textContent = `\u200e${path}\u200e`;
  element("folder").title = path;
}

function showProblem(text, retry) {
  element("problem-text").textContent = text;
  element("retry").onclick = retry;
  show("problem");
}

function feature() {
  document.querySelector(".art").dataset.vanguard = FEATURED[Math.floor(Math.random() * FEATURED.length)];
}

// A size as Windows shows one.
function formatBytes(bytes) {
  const units = ["bytes", "KB", "MB", "GB", "TB"];
  let value = bytes;
  let unit = 0;
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit += 1;
  }
  const digits = unit === 0 || value >= 100 ? 0 : 1;
  return `${value.toFixed(digits)} ${units[unit]}`;
}

async function start() {
  show("loading");
  element("repair").hidden = true;
  const game = await invoke("game_status");
  element("build").textContent = game.buildVersion ? `Build ${game.buildVersion}` : "";
  if (game.problem) {
    showProblem(game.problem, start);
    return;
  }
  if (game.state === "update") {
    // Updates start by themselves, before sign-in (ADR-022 §7).
    install(null, false);
  } else if (game.state === "install" || game.state === "repair") {
    offerInstall(game);
  } else {
    signIn(game);
  }
}

function offerInstall(game) {
  const repair = game.state === "repair";
  chosenFolder = null;
  element("install-title").textContent = repair ? "Repair Veyra" : "Install Veyra";
  element("install-note").textContent = repair
    ? "Veyra's install record cannot be read. Repair checks every file and replaces what is wrong."
    : `Build ${game.availableVersion}: ${formatBytes(game.downloadBytes)} to download, ${formatBytes(game.installBytes)} installed.`;
  showFolder(game.folder);
  element("change-folder").hidden = repair;
  element("install-button").textContent = repair ? "Repair" : "Install";
  element("install-button").onclick = () => install(repair ? null : chosenFolder, repair);
  show("install");
}

async function changeFolder() {
  let chosen;
  try {
    chosen = await invoke("choose_folder", { current: shownFolder });
  } catch (problem) {
    showProblem(String(problem), start);
    return;
  }
  if (chosen) {
    chosenFolder = chosen;
    showFolder(chosen);
  }
}

async function install(folder, repair) {
  try {
    await invoke("start_install", { folder, repair });
  } catch (problem) {
    showProblem(String(problem), start);
    return;
  }
  samples = [];
  element("install-stage").textContent = "";
  show("installing");
  followInstall();
}

function renderInstall(status) {
  const percent = status.totalBytes > 0 ? Math.floor((status.doneBytes * 100) / status.totalBytes) : 100;
  element("install-stage").textContent = status.stage;
  element("install-fill").style.width = `${percent}%`;
  document.querySelector("#installing .bar").setAttribute("aria-valuenow", String(percent));
  element("install-percent").textContent = `${percent}%`;
  element("install-bytes").textContent =
    status.totalBytes > 0 ? `${formatBytes(status.doneBytes)} of ${formatBytes(status.totalBytes)}` : "";
  const now = performance.now();
  samples.push([now, status.downloadedBytes]);
  samples = samples.filter(([at]) => now - at <= SPEED_WINDOW_MS);
  const [firstAt, firstBytes] = samples[0];
  const speed = now > firstAt ? ((status.downloadedBytes - firstBytes) * 1000) / (now - firstAt) : 0;
  element("install-speed").textContent =
    status.downloadedBytes > 0 ? `${formatBytes(status.downloadedBytes)} downloaded · ${formatBytes(speed)}/s` : "";
}

async function followInstall() {
  const status = await invoke("install_status");
  renderInstall(status);
  if (status.problem) {
    showProblem(status.problem, start);
  } else if (status.running) {
    setTimeout(followInstall, STATUS_INTERVAL_MS);
  } else {
    start();
  }
}

async function signIn(game) {
  const found = await invoke("accounts");
  if (found.problem) {
    showProblem(found.problem, start);
    return;
  }
  const select = element("account");
  select.replaceChildren(
    ...found.accounts.map((name) => {
      const option = document.createElement("option");
      option.value = name;
      option.textContent = name;
      return option;
    }),
  );
  const remembered = localStorage.getItem(ACCOUNT_KEY);
  if (found.accounts.includes(remembered)) {
    select.value = remembered;
  }
  element("game-note").textContent = game.note || "";
  element("game-note").hidden = !game.note;
  element("repair").hidden = !game.installable;
  show("sign-in");
}

async function play() {
  const account = element("account").value;
  if (!account) {
    return;
  }
  localStorage.setItem(ACCOUNT_KEY, account);
  try {
    await invoke("start_launch", { account });
  } catch (problem) {
    showProblem(String(problem), start);
    return;
  }
  element("repair").hidden = true;
  show("progress");
  follow();
}

async function follow() {
  const status = await invoke("launch_status");
  element("stage").textContent = `${status.stage}…`;
  if (status.problem) {
    showProblem(status.problem, start);
  } else if (status.running) {
    setTimeout(follow, STATUS_INTERVAL_MS);
  }
}

element("play").addEventListener("click", play);
element("change-folder").addEventListener("click", changeFolder);
element("repair").addEventListener("click", () => {
  element("repair").hidden = true;
  install(null, true);
});
window.addEventListener("DOMContentLoaded", () => {
  feature();
  start();
});
