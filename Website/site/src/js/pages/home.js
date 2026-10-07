// The home page: the featured Vanguards cut in one after another behind the title, and the roster strip.

import config from "../site-config.js";
import { startChrome } from "../chrome.js";
import { reducedMotion } from "../motion.js";

// How long the cut between two Vanguards takes; the stylesheet's smear and speed lines run as long.
const CUT_MS = 640;
// The cut's edge, in percent of the slide's width: its top and bottom start off the right and end off the left.
const CUT_EDGE = Object.freeze({ top: [155, -30], bottom: [130, -55] });
// How far the art leans toward the pointer, in pixels. A presentation value.
const PARALLAX_PX = 12;

/** A CSS cubic-bezier easing as a function of time, 0 to 1 (the stylesheet's --ease-smear). */
function cubicBezier(x1, y1, x2, y2) {
  const at = (a, b, s) => 3 * a * (1 - s) ** 2 * s + 3 * b * (1 - s) * s ** 2 + s ** 3;
  return (time) => {
    let low = 0;
    let high = 1;
    for (let step = 0; step < 24; step += 1) {
      const middle = (low + high) / 2;
      if (at(x1, x2, middle) < time) {
        low = middle;
      } else {
        high = middle;
      }
    }
    return at(y1, y2, (low + high) / 2);
  };
}

const easeSmear = cubicBezier(0.7, 0, 0.2, 1);

/** Runs the cut on slide: its clip and the slash both follow one progress value, frame by frame. */
function runCut(hero, slide) {
  // The edge leans by the top's and bottom's difference across the slide's height; the slash leans with it.
  const box = slide.getBoundingClientRect();
  const lean = ((CUT_EDGE.top[0] - CUT_EDGE.bottom[0]) / 100) * box.width;
  hero.style.setProperty("--cut-slant", `${(-Math.atan2(lean, box.height) * 180) / Math.PI}deg`);
  return new Promise((resolve) => {
    const start = performance.now();
    const frame = (now) => {
      const progress = easeSmear(Math.min(1, (now - start) / CUT_MS));
      const top = CUT_EDGE.top[0] + (CUT_EDGE.top[1] - CUT_EDGE.top[0]) * progress;
      const bottom = CUT_EDGE.bottom[0] + (CUT_EDGE.bottom[1] - CUT_EDGE.bottom[0]) * progress;
      slide.style.setProperty("clip-path", `polygon(${top}% 0, ${CUT_EDGE.top[0]}% 0, ${CUT_EDGE.bottom[0]}% 100%, ${bottom}% 100%)`);
      hero.style.setProperty("--cut", progress.toFixed(4));
      if (now - start < CUT_MS) {
        requestAnimationFrame(frame);
      } else {
        slide.style.removeProperty("clip-path");
        resolve();
      }
    };
    requestAnimationFrame(frame);
  });
}

function element(tag, className, text) {
  const node = document.createElement(tag);
  if (className) {
    node.className = className;
  }
  if (text !== undefined) {
    node.textContent = text;
  }
  return node;
}

function startHero(hero, vanguards, holdSeconds) {
  if (!hero || vanguards.length === 0) {
    return;
  }
  const stage = hero.querySelector("[data-hero-stage]");
  const ticks = hero.querySelector("[data-hero-ticks]");
  const plate = hero.querySelector("[data-hero-plate]");
  const holdMs = holdSeconds * 1000;
  const still = reducedMotion();
  hero.style.setProperty("--hold", `${holdSeconds}s`);

  const slides = vanguards.map((vanguard, index) => {
    const slide = element("div", "hero__slide");
    slide.style.setProperty("--focus", vanguard.focus);
    const image = element("img");
    image.src = vanguard.art;
    image.alt = "";
    image.decoding = "async";
    if (index === 0) {
      image.fetchPriority = "high";
    } else {
      image.loading = "lazy";
    }
    slide.append(image);
    stage.append(slide);
    return { slide, image };
  });

  const buttons = vanguards.map((vanguard, index) => {
    const button = element("button", "hero__tick");
    button.type = "button";
    button.setAttribute("aria-label", `Show ${vanguard.name}`);
    button.setAttribute("aria-pressed", "false");
    button.addEventListener("click", () => {
      show(index);
      // A choice holds: the rotation waits while the pointer or focus stays on the hero.
    });
    ticks.append(button);
    return button;
  });

  let current = -1;
  let cutting = false;
  // Choices made while the art loads or a cut runs: only the latest is shown, after the cut in progress.
  let latest = 0;
  let queued = null;
  let timer = 0;
  let startedAt = 0;
  let remaining = holdMs;
  let paused = false;

  function setPlate(index) {
    const vanguard = vanguards[index];
    hero.style.setProperty("--hue", vanguard.hue);
    plate.style.setProperty("--hue", vanguard.hue);
    plate.querySelector("[data-plate-index]").textContent = `${String(index + 1).padStart(2, "0")} / ${String(vanguards.length).padStart(2, "0")}`;
    plate.querySelector("[data-plate-name]").textContent = vanguard.name;
    plate.querySelector("[data-plate-title]").textContent = vanguard.title;
    plate.querySelector("[data-plate-meta]").textContent = [vanguard.region, ...vanguard.roles].join(" · ");
    plate.classList.remove("is-changing");
    void plate.offsetWidth;
    plate.classList.add("is-changing");
    buttons.forEach((button, i) => button.setAttribute("aria-pressed", String(i === index)));
  }

  function schedule(delay) {
    clearTimeout(timer);
    if (still || paused || vanguards.length < 2) {
      return;
    }
    startedAt = performance.now();
    remaining = delay;
    timer = setTimeout(() => show((current + 1) % vanguards.length), delay);
  }

  async function show(index) {
    if (cutting) {
      queued = index;
      return;
    }
    const ticket = ++latest;
    const next = slides[index];
    // Wait for the art, so the cut never reveals an empty frame.
    next.image.loading = "eager";
    try {
      await next.image.decode();
    } catch {
      // Shown anyway: a broken image leaves the stage's own gradient.
    }
    // A later choice, or a cut that began meanwhile, takes precedence over this one.
    if (ticket !== latest) {
      return;
    }
    if (cutting) {
      queued = index;
      return;
    }
    if (index === current) {
      return;
    }
    const previous = slides[current];
    current = index;
    setPlate(index);
    if (!previous || still) {
      previous?.slide.classList.remove("is-current");
      next.slide.classList.add("is-current");
      schedule(holdMs);
      return;
    }
    cutting = true;
    schedule(holdMs);
    hero.style.setProperty("--cut", "0");
    next.slide.style.setProperty("clip-path", "polygon(0 0, 0 0, 0 0)");
    next.slide.classList.add("is-entering");
    hero.classList.remove("is-cutting");
    void hero.offsetWidth;
    hero.classList.add("is-cutting");
    await runCut(hero, next.slide);
    previous.slide.classList.remove("is-current");
    next.slide.classList.remove("is-entering");
    next.slide.classList.add("is-current");
    hero.classList.remove("is-cutting");
    cutting = false;
    if (queued !== null) {
      const waiting = queued;
      queued = null;
      show(waiting);
    }
  }

  function pause() {
    if (paused) {
      return;
    }
    paused = true;
    hero.classList.add("is-paused");
    clearTimeout(timer);
    remaining = Math.max(0, remaining - (performance.now() - startedAt));
  }

  function resume() {
    if (!paused) {
      return;
    }
    paused = false;
    hero.classList.remove("is-paused");
    schedule(remaining);
  }

  // The rotation waits while someone reads the plate or uses the controls, and while the tab is hidden.
  for (const zone of [plate, ticks]) {
    zone.addEventListener("pointerenter", pause);
    zone.addEventListener("pointerleave", resume);
  }
  hero.addEventListener("focusin", pause);
  hero.addEventListener("focusout", (event) => {
    if (!hero.contains(event.relatedTarget)) {
      resume();
    }
  });
  document.addEventListener("visibilitychange", () => (document.hidden ? pause() : resume()));

  // The art leans a little toward the pointer, like a camera following the eye.
  if (!still && matchMedia("(pointer: fine)").matches) {
    let frame = 0;
    hero.addEventListener("pointermove", (event) => {
      cancelAnimationFrame(frame);
      frame = requestAnimationFrame(() => {
        const box = hero.getBoundingClientRect();
        const x = ((event.clientX - box.left) / box.width - 0.5) * -2 * PARALLAX_PX;
        const y = ((event.clientY - box.top) / box.height - 0.5) * -2 * PARALLAX_PX;
        for (const { slide } of slides) {
          slide.style.setProperty("--px", `${x.toFixed(1)}px`);
          slide.style.setProperty("--py", `${y.toFixed(1)}px`);
        }
      });
    });
  }

  show(0);
}

function renderRoster(list, vanguards) {
  if (!list) {
    return;
  }
  for (const vanguard of vanguards) {
    const card = element("article", "vanguard-card");
    card.setAttribute("role", "listitem");
    card.setAttribute("data-reveal", "");
    card.style.setProperty("--hue", vanguard.hue);
    card.style.setProperty("--focus", vanguard.focus);
    const image = element("img");
    image.src = vanguard.art;
    image.alt = "";
    image.loading = "lazy";
    image.decoding = "async";
    const body = element("div", "vanguard-card__body");
    body.append(
      element("span", "vanguard-card__region", vanguard.region),
      element("h3", "vanguard-card__name", vanguard.name),
      element("p", "vanguard-card__title", `${vanguard.title} · ${vanguard.roles.join(", ")}`),
    );
    card.append(image, body);
    list.append(card);
  }
}

renderRoster(document.querySelector("[data-roster]"), config.vanguards);
startChrome();
startHero(document.querySelector("[data-hero]"), config.vanguards, config.heroRotationSeconds);
