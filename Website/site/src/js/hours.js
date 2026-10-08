// The servers' daily hours (site.json "serverHours"): the public test servers start by themselves at `opens` and
// stop at `closes`, every day, on the host's clock (`timeZone`). They can still fail to start, so the hours say when
// to look, and the live status says whether they came up.
// No dependencies: Intl only, so the build and the browser word the hours alike.

const LOCALE = "en-US";
const MINUTES_PER_DAY = 24 * 60;
const MS_PER_MINUTE = 60 * 1000;

/** "22:00" → minutes after midnight. */
export function minutesOf(clock) {
  const [hours, minutes] = clock.split(":").map(Number);
  return hours * 60 + minutes;
}

function partsIn(timeZone, instant, options) {
  const parts = new Intl.DateTimeFormat(LOCALE, { timeZone, ...options }).formatToParts(instant);
  return Object.fromEntries(parts.map(({ type, value }) => [type, value]));
}

/** How far `timeZone`'s clock is ahead of UTC at `instant`, in minutes. */
function offsetMinutes(timeZone, instant) {
  const p = partsIn(timeZone, instant, { hourCycle: "h23", year: "numeric", month: "numeric", day: "numeric", hour: "numeric", minute: "numeric", second: "numeric" });
  const asUtc = Date.UTC(Number(p.year), Number(p.month) - 1, Number(p.day), Number(p.hour), Number(p.minute), Number(p.second));
  return Math.round((asUtc - instant.getTime()) / MS_PER_MINUTE);
}

/**
 * The instant `timeZone`'s clock reads `minutes` after midnight on the given day (days and minutes may overflow).
 * Where the clock changes that day, a time it skips reads as the same time after the jump (2:30 becomes 3:30), as
 * schedulers run it, and a time it repeats reads as its first.
 */
function instantAt(timeZone, { year, month, day }, minutes) {
  const wall = Date.UTC(year, month - 1, day, 0, minutes);
  // The offsets in force a day either side: the same unless the clock changes in between.
  const before = offsetMinutes(timeZone, new Date(wall - MINUTES_PER_DAY * MS_PER_MINUTE));
  const after = offsetMinutes(timeZone, new Date(wall + MINUTES_PER_DAY * MS_PER_MINUTE));
  // Each offset gives a candidate; it is real if the clock reads `wall` there. Two are real where the time repeats.
  const real = [before, after]
    .map((offset) => wall - offset * MS_PER_MINUTE)
    .filter((instant) => instant + offsetMinutes(timeZone, new Date(instant)) * MS_PER_MINUTE === wall);
  // None is real where the time is skipped: read it on the clock from before the change, which lands after the jump.
  return new Date(real.length > 0 ? Math.min(...real) : wall - before * MS_PER_MINUTE);
}

/** The session running at `now`, or the next one: { opensAt, closesAt, live }. Hours may run past midnight. */
export function sessionAt(hours, now = new Date()) {
  const p = partsIn(hours.timeZone, now, { year: "numeric", month: "numeric", day: "numeric" });
  const today = { year: Number(p.year), month: Number(p.month), day: Number(p.day) };
  const opens = minutesOf(hours.opens);
  let closes = minutesOf(hours.closes);
  if (closes <= opens) {
    closes += MINUTES_PER_DAY;
  }
  // Yesterday's session may still be running past midnight; otherwise today's, or tomorrow's.
  for (const days of [-1, 0, 1]) {
    const day = { ...today, day: today.day + days };
    const opensAt = instantAt(hours.timeZone, day, opens);
    const closesAt = instantAt(hours.timeZone, day, closes);
    if (now < closesAt) {
      return { opensAt, closesAt, live: now >= opensAt };
    }
  }
  throw new Error("unreachable: tomorrow's session always closes after now");
}

/** "10:00 PM", with a plain space whatever the engine's ICU puts before the day period. */
function clockParts(instant, timeZone) {
  const p = partsIn(timeZone, instant, { hour: "numeric", minute: "2-digit" });
  return { time: `${p.hour}:${p.minute}`, period: p.dayPeriod ?? "" };
}

export function clockIn(instant, timeZone) {
  const { time, period } = clockParts(instant, timeZone);
  return period ? `${time} ${period}` : time;
}

/** "10:00–11:30 PM", or "11:00 PM–12:30 AM" across a change of day period. */
export function rangeIn(opensAt, closesAt, timeZone) {
  const opens = clockParts(opensAt, timeZone);
  const closes = clockParts(closesAt, timeZone);
  const start = opens.period === closes.period ? opens.time : clockIn(opensAt, timeZone);
  return `${start}–${clockIn(closesAt, timeZone)}`;
}

/** "Eastern Time" where the engine knows the generic name, else the short one ("EDT"). */
export function zoneName(timeZone, instant = new Date()) {
  try {
    return partsIn(timeZone, instant, { timeZoneName: "longGeneric" }).timeZoneName;
  } catch {
    return partsIn(timeZone, instant, { timeZoneName: "short" }).timeZoneName;
  }
}

/**
 * The hours in words: `host` on the host's clock ("10:00–11:30 PM Eastern Time"), and `local` on the viewer's when
 * that reads differently ("7:00–8:30 PM your time"), else null.
 */
export function describeServerHours(hours, viewerZone = hours.timeZone, now = new Date()) {
  const { opensAt, closesAt } = sessionAt(hours, now);
  const host = rangeIn(opensAt, closesAt, hours.timeZone);
  const local = viewerZone ? rangeIn(opensAt, closesAt, viewerZone) : host;
  return { host: `${host} ${zoneName(hours.timeZone, opensAt)}`, local: local === host ? null : `${local} your time` };
}

/** The viewer's time zone, or null where the browser won't say. */
export function viewerTimeZone() {
  try {
    return Intl.DateTimeFormat().resolvedOptions().timeZone ?? null;
  } catch {
    return null;
  }
}

/** The hours as one phrase: "10:00–11:30 PM Eastern Time (7:00–8:30 PM your time)". */
export function serverHoursPhrase(hours, viewerZone = viewerTimeZone(), now = new Date()) {
  const { host, local } = describeServerHours(hours, viewerZone, now);
  return local ? `${host} (${local})` : host;
}

/** "3 h 20 min", "45 min": rounded up, so it never says 0. */
export function formatWait(ms) {
  const minutes = Math.max(1, Math.ceil(ms / MS_PER_MINUTE));
  const hours = Math.floor(minutes / 60);
  const rest = minutes % 60;
  if (hours === 0) {
    return `${rest} min`;
  }
  return rest === 0 ? `${hours} h` : `${hours} h ${rest} min`;
}

/**
 * A line to go with the servers' live state, or "" when there's nothing to add:
 * - offline during the hours: today's start hasn't come up, and why that happens;
 * - offline outside them: when the next session opens;
 * - online during them: when today's session ends.
 */
export function serverNote(hours, state, now = new Date()) {
  const session = sessionAt(hours, now);
  const zone = zoneName(hours.timeZone, session.opensAt);
  if (state === "offline" && session.live) {
    return `Today's session starts at ${clockIn(session.opensAt, hours.timeZone)} ${zone} and takes a few minutes to come up. ` +
      "If the servers stay offline after that, the host computer is off or the automation that starts them has run out of usage, so there's no session today.";
  }
  if (state === "offline") {
    return `The next session opens in ${formatWait(session.opensAt - now)}.`;
  }
  if (state === "online" && session.live) {
    return `Today's session runs until ${clockIn(session.closesAt, hours.timeZone)} ${zone}.`;
  }
  return "";
}
