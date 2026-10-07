// The website's motion, in the match's effect language (ADR-068 §1): Flux motes drifting behind the page, panels
// cut in as they scroll into view, and a burst where a button is pressed. Presentation only. Nothing runs when
// the player asks for reduced motion, and the motes stop while the tab is hidden.

export const reducedMotion = () => globalThis.matchMedia?.("(prefers-reduced-motion: reduce)").matches ?? false;

// How the motes look and move. Presentation values, not gameplay tuning.
const MOTES = Object.freeze({
  perPixel: 1 / 16000, // one mote per this many square pixels of window
  max: 90,
  minRadius: 0.6,
  maxRadius: 2.4,
  riseMin: 8, // pixels a second
  riseMax: 30,
  sway: 14, // pixels either side
  hotShare: 0.12, // motes with a white-hot core
  brassShare: 0.22, // motes in brass rather than Flux teal
});

const TEAL = [79, 216, 207];
const BRASS = [214, 181, 110];

function makeMote(width, height, fresh) {
  const radius = MOTES.minRadius + Math.random() ** 2 * (MOTES.maxRadius - MOTES.minRadius);
  return {
    x: Math.random() * width,
    y: fresh ? Math.random() * height : height + 10,
    radius,
    rise: MOTES.riseMin + Math.random() * (MOTES.riseMax - MOTES.riseMin),
    phase: Math.random() * Math.PI * 2,
    swaySpeed: 0.2 + Math.random() * 0.6,
    colour: Math.random() < MOTES.brassShare ? BRASS : TEAL,
    hot: Math.random() < MOTES.hotShare,
    flicker: Math.random() * Math.PI * 2,
  };
}

/** Flux motes on a canvas behind the page. */
export function startMotes(canvas) {
  if (!canvas || reducedMotion() || !canvas.getContext) {
    return;
  }
  const context = canvas.getContext("2d");
  let width = 0;
  let height = 0;
  let motes = [];
  let last = 0;
  let frame = 0;

  function resize() {
    const ratio = Math.min(globalThis.devicePixelRatio || 1, 2);
    width = canvas.clientWidth;
    height = canvas.clientHeight;
    canvas.width = Math.round(width * ratio);
    canvas.height = Math.round(height * ratio);
    context.setTransform(ratio, 0, 0, ratio, 0, 0);
    const count = Math.min(MOTES.max, Math.round(width * height * MOTES.perPixel));
    motes = Array.from({ length: count }, () => makeMote(width, height, true));
  }

  function draw(time) {
    const seconds = Math.min((time - last) / 1000, 0.1);
    last = time;
    context.clearRect(0, 0, width, height);
    context.globalCompositeOperation = "lighter";
    for (const mote of motes) {
      mote.y -= mote.rise * seconds;
      mote.phase += mote.swaySpeed * seconds;
      mote.flicker += seconds * 3;
      if (mote.y < -10) {
        Object.assign(mote, makeMote(width, height, false));
      }
      const x = mote.x + Math.sin(mote.phase) * MOTES.sway;
      // Fade in from the foot of the window and out toward its head.
      const life = Math.min(1, (height - mote.y) / (height * 0.2)) * Math.min(1, mote.y / (height * 0.25));
      const alpha = Math.max(0, life) * (0.55 + 0.45 * Math.sin(mote.flicker));
      const [r, g, b] = mote.colour;
      const glow = context.createRadialGradient(x, mote.y, 0, x, mote.y, mote.radius * 5);
      glow.addColorStop(0, `rgba(${r}, ${g}, ${b}, ${0.5 * alpha})`);
      glow.addColorStop(1, `rgba(${r}, ${g}, ${b}, 0)`);
      context.fillStyle = glow;
      context.beginPath();
      context.arc(x, mote.y, mote.radius * 5, 0, Math.PI * 2);
      context.fill();
      context.fillStyle = mote.hot ? `rgba(255, 255, 255, ${alpha})` : `rgba(${r}, ${g}, ${b}, ${alpha})`;
      context.beginPath();
      context.arc(x, mote.y, mote.radius, 0, Math.PI * 2);
      context.fill();
    }
    frame = requestAnimationFrame(draw);
  }

  function start() {
    cancelAnimationFrame(frame);
    last = performance.now();
    frame = requestAnimationFrame(draw);
  }

  resize();
  start();
  let resizing = 0;
  addEventListener("resize", () => {
    clearTimeout(resizing);
    resizing = setTimeout(resize, 150);
  });
  document.addEventListener("visibilitychange", () => (document.hidden ? cancelAnimationFrame(frame) : start()));
}

/** Panels marked data-reveal cut in as they scroll into view, once. */
export function revealOnScroll(root = document) {
  const targets = [...root.querySelectorAll("[data-reveal]:not(.is-in)")];
  // Siblings that arrive together are staggered by their order (the stylesheet reads --i).
  for (const target of targets) {
    const siblings = [...(target.parentElement?.children ?? [])].filter((element) => element.hasAttribute("data-reveal"));
    target.style.setProperty("--i", String(Math.max(0, siblings.indexOf(target))));
  }
  if (reducedMotion() || !("IntersectionObserver" in globalThis)) {
    targets.forEach((target) => target.classList.add("is-in"));
    return;
  }
  const observer = new IntersectionObserver(
    (entries) => {
      for (const entry of entries) {
        if (entry.isIntersecting) {
          entry.target.classList.add("is-in");
          observer.unobserve(entry.target);
        }
      }
    },
    { rootMargin: "0px 0px -8% 0px", threshold: 0.12 },
  );
  targets.forEach((target) => observer.observe(target));
}

/** A burst at a point inside an element: a ring, a white-hot core and four slashes. */
export function burstAt(host, x, y) {
  if (reducedMotion()) {
    return;
  }
  const burst = document.createElement("span");
  burst.className = "burst";
  burst.setAttribute("aria-hidden", "true");
  burst.style.left = `${x}px`;
  burst.style.top = `${y}px`;
  const base = Math.random() * 90;
  for (let index = 0; index < 4; index += 1) {
    const slash = document.createElement("span");
    slash.className = "burst__slash";
    slash.style.setProperty("--angle", `${base + index * 90 + (Math.random() * 24 - 12)}deg`);
    burst.append(slash);
  }
  host.append(burst);
  setTimeout(() => burst.remove(), 600);
}

/** Every button and button-styled link bursts where it is pressed. */
export function burstOnPress(root = document) {
  root.addEventListener("pointerdown", (event) => {
    const button = event.target.closest?.(".btn");
    if (!button || button.disabled || button.getAttribute("aria-disabled") === "true") {
      return;
    }
    const box = button.getBoundingClientRect();
    burstAt(button, event.clientX - box.left, event.clientY - box.top);
  });
}
