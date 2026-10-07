// The support page: the answers filter as the player types, a link to one opens it, the display-name rules come
// from the site's configuration, and the contact card lists whatever channels site.json names.

import config from "../site-config.js";
import { startChrome } from "../chrome.js";
import { describeDisplayName } from "../rules.js";

const normalise = (text) => text.toLowerCase().replace(/\s+/g, " ").trim();

function setUpSearch() {
  const input = document.querySelector("[data-faq-search]");
  const empty = document.querySelector("[data-faq-empty]");
  const groups = [...document.querySelectorAll("[data-faq-group]")];
  const answers = groups.flatMap((group) => [...group.querySelectorAll("details.faq")]).map((details) => ({ details, text: normalise(details.textContent) }));
  const wasOpen = new Map();

  input.addEventListener("input", () => {
    const words = normalise(input.value).split(" ").filter(Boolean);
    let shown = 0;
    for (const { details, text } of answers) {
      if (!wasOpen.has(details)) {
        wasOpen.set(details, details.open);
      }
      const match = words.every((word) => text.includes(word));
      details.hidden = !match;
      // While searching, matches open so their answers show; clearing the search restores what was open.
      details.open = words.length > 0 ? match : wasOpen.get(details);
      shown += match ? 1 : 0;
    }
    if (words.length === 0) {
      wasOpen.clear();
    }
    groups.forEach((group) => (group.hidden = !group.querySelector("details.faq:not([hidden])")));
    empty.hidden = shown > 0;
  });
}

/** A link to an answer (support#faq-…) opens it. */
function openFromHash() {
  const target = location.hash ? document.getElementById(decodeURIComponent(location.hash.slice(1))) : null;
  if (target instanceof HTMLDetailsElement) {
    target.open = true;
    target.querySelector("summary")?.focus({ preventScroll: true });
  }
}

function renderContact({ email, links }) {
  const list = document.querySelector("[data-contact-list]");
  const items = [];
  if (email) {
    items.push({ label: `Email ${email}`, url: `mailto:${email}` });
  }
  items.push(...links);
  for (const item of items) {
    const entry = document.createElement("li");
    const link = document.createElement("a");
    link.className = "btn btn--ghost btn--small btn--block";
    link.href = item.url;
    link.textContent = item.label;
    if (item.url.startsWith("https://")) {
      link.rel = "noopener";
      link.target = "_blank";
    }
    entry.append(link);
    list.append(entry);
  }
  const none = items.length === 0;
  list.hidden = none;
  document.querySelector("[data-contact-intro]").hidden = none;
  document.querySelector("[data-contact-none]").hidden = !none;
}

document.querySelectorAll("[data-name-rules]").forEach((element) => (element.textContent = describeDisplayName(config.account.displayName)));
renderContact(config.support);
setUpSearch();
startChrome();
openFromHash();
addEventListener("hashchange", openFromHash);
