// The launcher's window. It shows what the launcher's core reports and asks it to install, update,
// repair, sign in or register a player (ADR-038) and launch; the core, in Rust, does all the work
// (Launcher/app/src/main.rs, ADR-022 §7). Passwords pass straight through to the core and are never
// kept here. Beside the card it shows whether the servers are up, when they run, and what the build
// changed, from the website (ADR-081).
//
// generated/ is written by Launcher/app/build.rs: the art it carries, and the website's own reading of
// the servers' hours (Website/site/src/js/hours.js), so the two word them alike.
import { VANGUARD_ART } from "./generated/art.js";
import { serverHoursPhrase, serverNote, sessionAt } from "./generated/hours.js";

const invoke = window.__TAURI__.core.invoke;

// How often the window asks how a launch or an install is going, in milliseconds.
const STATUS_INTERVAL_MS = 200;
// The download speed shown is the average over this many milliseconds.
const SPEED_WINDOW_MS = 3000;
// Where the window remembers the last development account chosen and the last email signed in with,
// on this machine only. Never a password.
const ACCOUNT_KEY = "veyra.launcher.account";
const EMAIL_KEY = "veyra.launcher.email";
// How often the window asks whether the servers are up, and rewords their countdown, in milliseconds.
const SERVERS_INTERVAL_MS = 60 * 1000;
const SERVERS_WORDING_INTERVAL_MS = 30 * 1000;

const sections = ["loading", "install", "installing", "sign-in", "register", "choose-name", "ready", "progress", "problem"];
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

// A Vanguard fills the window, a different one each time it opens.
function feature() {
  const id = VANGUARD_ART[Math.floor(Math.random() * VANGUARD_ART.length)];
  const art = document.querySelector(".art");
  art.dataset.vanguard = id;
  art.style.setProperty("--art", `url("art/${id}.webp")`);
}

// The website's feed (ADR-081): { serverHours, patchNotes }, or null when the launcher has none.
let feed = null;
// "online", "offline" or "unknown", once the core has asked.
let servers = null;
// The build whose patch notes the window shows: the one installing, or the one that would launch.
let shownVersion = null;

// What's new in the shown build, when the website has notes for it.
function renderWhatsNew() {
  const note = feed?.patchNotes.find(({ version }) => version === shownVersion);
  element("whats-new").hidden = !note;
  if (!note) {
    return;
  }
  element("whats-new-version").textContent = note.version;
  element("whats-new-title").textContent = note.title;
  element("whats-new-changes").replaceChildren(
    ...note.changes.map((change) => {
      const item = document.createElement("li");
      item.textContent = change;
      return item;
    }),
  );
}

function showWhatsNew(version) {
  shownVersion = version ?? null;
  renderWhatsNew();
}

// The servers' state, their hours, and, on the cards that need them, a plain word that Play cannot
// find a match while they are down.
function renderServers() {
  const hours = feed?.serverHours ?? null;
  const panel = element("servers");
  panel.hidden = servers === null || (servers === "unknown" && !hours);
  if (panel.hidden) {
    return;
  }
  const now = new Date();
  const live = hours ? sessionAt(hours, now).live : false;
  panel.dataset.state = servers;
  element("servers-state").textContent =
    servers === "online" ? "Servers online" : servers === "offline" ? "Servers offline" : "Server status unknown";
  element("servers-hours").textContent = hours ? `Open daily ${serverHoursPhrase(hours)}` : "";
  element("servers-hours").hidden = !hours;
  const detail = hours ? serverNote(hours, servers, now) : "";
  element("servers-detail").textContent = detail;
  element("servers-detail").hidden = !detail;
  // Signing in, downloading and Play all need the servers: say so plainly wherever one was asked for.
  const closed =
    servers !== "offline"
      ? ""
      : live
        ? "Veyra's servers should be up but haven't answered, so signing in, downloading and playing must wait."
        : ["Veyra's servers are closed, so signing in, downloading and playing wait until they open.", detail].filter(Boolean).join(" ");
  for (const note of document.querySelectorAll(".servers-note")) {
    note.textContent = closed;
    note.hidden = !closed;
  }
}

async function followServers() {
  try {
    servers = await invoke("server_status");
  } catch {
    servers = "unknown";
  }
  renderServers();
}

async function loadFeed() {
  try {
    feed = await invoke("launcher_feed");
  } catch {
    feed = null;
  }
  renderWhatsNew();
  renderServers();
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
  element("loading-status").textContent = "Contacting Veyra's services…";
  element("repair").hidden = true;
  // The launcher updates itself first, before the game (ADR-022 §11): it closes, and Setup opens the
  // new one.
  const update = await invoke("launcher_update");
  let updateNote = update.note;
  if (update.state === "ready") {
    element("loading-status").textContent = `Updating the launcher to ${update.version}…`;
    try {
      await invoke("apply_launcher_update");
      return;
    } catch (problem) {
      // A Setup that did not start is tried again the next time the launcher opens; the game opens now,
      // as it does when the update could not be downloaded.
      updateNote = String(problem);
      element("loading-status").textContent = "Contacting Veyra's services…";
    }
  }
  const game = await invoke("game_status");
  // What's new in the build being installed, or else in the one that would launch.
  showWhatsNew(game.availableVersion ?? game.buildVersion);
  if (updateNote) {
    game.note = game.note ? `${updateNote} ${game.note}` : updateNote;
  }
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
  // A run that downloads shows the download itself: what the player waits for. Files it only rebuilds
  // from chunks already on disk come second, and lead only when nothing is to download.
  const downloads = status.downloadTotalBytes > 0;
  const [done, total] = downloads ? [status.downloadedBytes, status.downloadTotalBytes] : [status.doneBytes, status.totalBytes];
  const percent = total > 0 ? Math.min(100, Math.floor((done * 100) / total)) : 100;
  element("install-stage").textContent = status.stage;
  element("install-fill").style.width = `${percent}%`;
  document.querySelector("#installing .bar").setAttribute("aria-valuenow", String(percent));
  element("install-percent").textContent = `${percent}%`;
  element("install-bytes").textContent =
    total > 0 ? `${formatBytes(done)} of ${formatBytes(total)}${downloads ? " downloaded" : ""}` : "";
  const now = performance.now();
  samples.push([now, status.downloadedBytes]);
  samples = samples.filter(([at]) => now - at <= SPEED_WINDOW_MS);
  const [firstAt, firstBytes] = samples[0];
  const speed = now > firstAt ? ((status.downloadedBytes - firstBytes) * 1000) / (now - firstAt) : 0;
  element("install-speed").textContent =
    downloads && status.totalBytes > 0 ? `${formatBytes(speed)}/s · updating ${formatBytes(status.totalBytes)} of game files` : "";
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

function remembered(key) {
  try {
    return localStorage.getItem(key);
  } catch {
    return null;
  }
}

function remember(key, value) {
  try {
    localStorage.setItem(key, value);
  } catch {
    // Remembering is a convenience; the launcher works without it.
  }
}

// A form's message, or none.
function say(id, text) {
  element(id).textContent = text || "";
  element(id).hidden = !text;
}

// Whether a sign-in, registration or name choice is in flight. The core keeps one session and one
// pending proof, so a second request started beside the first could race it: a late registration
// could even create an account after the player signed into another. One runs at a time.
let authenticating = false;

// Every control outside the account forms that starts an account request or moves between them.
const ACCOUNT_NAVIGATION = ["show-register", "show-sign-in", "choose-name-back", "forgot-password", "sign-out", "dev-play"];

// Disables a form's controls, and every way to another account form, while the core works on it,
// so a click can neither send it twice nor start another request beside it.
async function busy(form, work) {
  const controls = [...element(form).querySelectorAll("input, button"), ...ACCOUNT_NAVIGATION.map(element)];
  const wasDisabled = controls.map((control) => control.disabled);
  controls.forEach((control) => (control.disabled = true));
  authenticating = true;
  try {
    return await work();
  } finally {
    authenticating = false;
    controls.forEach((control, index) => (control.disabled = wasDisabled[index]));
  }
}

// A click handler that does nothing while an account request is in flight.
function whenIdle(handler) {
  return (...args) => (authenticating ? undefined : handler(...args));
}

async function signIn(game) {
  const options = await invoke("sign_in_options");
  if (options.problem) {
    showProblem(options.problem, start);
    return;
  }
  element("repair").hidden = !game.installable;
  for (const id of ["game-note", "ready-note"]) {
    say(id, game.note);
  }
  showDevAccounts(options.accounts);
  if (options.signedInAs) {
    showReady(options.signedInAs);
  } else if (options.playerLogin) {
    showSignIn();
  } else {
    // A development launcher: only the development accounts.
    element("sign-in-form").hidden = true;
    document.querySelector("#sign-in .links").hidden = true;
    element("dev-sign-in").open = true;
    show("sign-in");
  }
}

function showDevAccounts(accounts) {
  element("dev-sign-in").hidden = accounts.length === 0;
  const select = element("account");
  select.replaceChildren(
    ...accounts.map((name) => {
      const option = document.createElement("option");
      option.value = name;
      option.textContent = name;
      return option;
    }),
  );
  const last = remembered(ACCOUNT_KEY);
  if (accounts.includes(last)) {
    select.value = last;
  }
}

function showSignIn() {
  say("sign-in-note", "");
  element("sign-in-password").value = "";
  element("sign-in-email").value = element("sign-in-email").value || remembered(EMAIL_KEY) || "";
  show("sign-in");
  (element("sign-in-email").value ? element("sign-in-password") : element("sign-in-email")).focus();
}

function showRegister() {
  say("register-note", "");
  element("register-password").value = "";
  element("register-email").value = element("register-email").value || element("sign-in-email").value;
  show("register");
  element("register-name").focus();
}

function showChooseName(note) {
  say("choose-name-note", note || "");
  show("choose-name");
  element("choose-name-input").focus();
}

function showReady(displayName) {
  element("signed-in-as").textContent = `Signed in as ${displayName}`;
  show("ready");
  element("play").focus();
}

// Where signing in, registering or choosing a name came to.
function settle(result) {
  if (result.state === "signedIn") {
    showReady(result.displayName);
  } else {
    showChooseName();
  }
}

async function submitSignIn(event) {
  event.preventDefault();
  if (authenticating) {
    return;
  }
  const email = element("sign-in-email").value.trim();
  const password = element("sign-in-password").value;
  if (!email || !password) {
    say("sign-in-note", "Enter your email and password.");
    return;
  }
  say("sign-in-note", "");
  try {
    const result = await busy("sign-in-form", () => invoke("sign_in", { email, password }));
    remember(EMAIL_KEY, email);
    element("sign-in-password").value = "";
    settle(result);
  } catch (problem) {
    say("sign-in-note", String(problem));
  }
}

async function submitRegister(event) {
  event.preventDefault();
  if (authenticating) {
    return;
  }
  const displayName = element("register-name").value.trim();
  const email = element("register-email").value.trim();
  const password = element("register-password").value;
  if (!/^[A-Za-z0-9_]{3,16}$/.test(displayName)) {
    say("register-note", "A name is 3 to 16 letters, digits or underscores.");
    return;
  }
  if (!email) {
    say("register-note", "Enter your email address.");
    return;
  }
  if (password.length < 6) {
    say("register-note", "Choose a password of at least 6 characters.");
    return;
  }
  say("register-note", "");
  try {
    const result = await busy("register-form", () => invoke("register", { email, password, displayName }));
    remember(EMAIL_KEY, email);
    element("register-password").value = "";
    if (result.state === "chooseName") {
      // The account exists, but someone took the name a moment ago.
      element("choose-name-input").value = displayName;
      showChooseName("That name is taken. Choose another.");
    } else {
      settle(result);
    }
  } catch (problem) {
    say("register-note", String(problem));
  }
}

async function submitChooseName(event) {
  event.preventDefault();
  if (authenticating) {
    return;
  }
  const displayName = element("choose-name-input").value.trim();
  if (!/^[A-Za-z0-9_]{3,16}$/.test(displayName)) {
    say("choose-name-note", "A name is 3 to 16 letters, digits or underscores.");
    return;
  }
  say("choose-name-note", "");
  try {
    settle(await busy("choose-name-form", () => invoke("choose_name", { displayName })));
  } catch (problem) {
    say("choose-name-note", String(problem));
  }
}

async function forgotPassword() {
  const email = element("sign-in-email").value.trim();
  if (!email) {
    say("sign-in-note", "Enter your email, then choose Forgot password.");
    element("sign-in-email").focus();
    return;
  }
  try {
    await invoke("reset_password", { email });
    say("sign-in-note", `If ${email} has an account, an email to reset its password is on its way.`);
  } catch (problem) {
    say("sign-in-note", String(problem));
  }
}

async function signOut() {
  await invoke("sign_out");
  showSignIn();
}

async function play(account) {
  if (account !== null) {
    if (!account) {
      return;
    }
    remember(ACCOUNT_KEY, account);
  }
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

// The window draws its own title bar (index.html); Close ends the launcher as the frame's did.
const appWindow = window.__TAURI__.window.getCurrentWindow();
element("minimize").addEventListener("click", () => appWindow.minimize());
element("close").addEventListener("click", () => appWindow.close());
element("play").addEventListener("click", () => play(null));
element("dev-play").addEventListener("click", () => play(element("account").value));
element("sign-in-form").addEventListener("submit", submitSignIn);
element("register-form").addEventListener("submit", submitRegister);
element("choose-name-form").addEventListener("submit", submitChooseName);
element("show-register").addEventListener("click", whenIdle(showRegister));
element("show-sign-in").addEventListener("click", whenIdle(showSignIn));
element("choose-name-back").addEventListener("click", whenIdle(signOut));
element("forgot-password").addEventListener("click", whenIdle(forgotPassword));
element("sign-out").addEventListener("click", whenIdle(signOut));
element("change-folder").addEventListener("click", changeFolder);
element("repair").addEventListener("click", () => {
  element("repair").hidden = true;
  install(null, true);
});
window.addEventListener("DOMContentLoaded", () => {
  feature();
  start();
  // Beside the card, and never in its way: neither waits for the other or for the card.
  loadFeed();
  followServers();
  setInterval(followServers, SERVERS_INTERVAL_MS);
  setInterval(renderServers, SERVERS_WORDING_INTERVAL_MS);
});
