// The launcher's window. It shows what the launcher's core reports and asks it to launch; the core,
// in Rust, does all the work (Launcher/app/src/main.rs).
"use strict";

const invoke = window.__TAURI__.core.invoke;

// How often the window asks how a launch is going, in milliseconds.
const STATUS_INTERVAL_MS = 200;
// Where the window remembers the last account chosen, on this machine only.
const ACCOUNT_KEY = "veyra.launcher.account";

const sections = ["loading", "sign-in", "progress", "problem"];
const element = (id) => document.getElementById(id);

function show(id) {
  for (const section of sections) {
    element(section).hidden = section !== id;
  }
}

function showProblem(text, retry) {
  element("problem-text").textContent = text;
  element("retry").onclick = retry;
  show("problem");
}

async function start() {
  show("loading");
  const startup = await invoke("startup");
  element("build").textContent = startup.buildVersion ? `Build ${startup.buildVersion}` : "";
  if (startup.problem) {
    showProblem(startup.problem, start);
    return;
  }
  const select = element("account");
  select.replaceChildren(
    ...startup.accounts.map((name) => {
      const option = document.createElement("option");
      option.value = name;
      option.textContent = name;
      return option;
    }),
  );
  const remembered = localStorage.getItem(ACCOUNT_KEY);
  if (startup.accounts.includes(remembered)) {
    select.value = remembered;
  }
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
    showProblem(String(problem), () => show("sign-in"));
    return;
  }
  show("progress");
  follow();
}

async function follow() {
  const status = await invoke("launch_status");
  element("stage").textContent = `${status.stage}…`;
  if (status.problem) {
    showProblem(status.problem, () => show("sign-in"));
  } else if (status.running) {
    setTimeout(follow, STATUS_INTERVAL_MS);
  }
}

element("play").addEventListener("click", play);
window.addEventListener("DOMContentLoaded", start);
