// The Veyra Setup a player downloads: the launcher's release on the public channel, read live from the release
// store (ADR-022 §11). The store names Setup by version, SHA-256 and size; the website shows all three, so a
// player can check the unsigned file is ours before running it.

import { ROUTES } from "./backend.js";

// The launcher channel format's limits (Launcher/core/src/release.rs and manifest.rs). Protocol limits, not tuning.
export const LAUNCHER_CHANNEL_SCHEMA_VERSION = 1;
export const MAX_SETUP_BYTES = 256 * 1024 * 1024;
const MAX_VERSION_LENGTH = 64;

/** A launcher channel file, checked as the launcher checks it; null when it is not one. */
export function parseLauncherChannel(answer) {
  if (answer === null || typeof answer !== "object") {
    return null;
  }
  const { schemaVersion, version, setup, size } = answer;
  const keys = Object.keys(answer).sort().join(",");
  if (keys !== "schemaVersion,setup,size,version" || schemaVersion !== LAUNCHER_CHANNEL_SCHEMA_VERSION) {
    return null;
  }
  if (typeof version !== "string" || version.length === 0 || version.length > MAX_VERSION_LENGTH || !/^[A-Za-z0-9._+-]+$/.test(version)) {
    return null;
  }
  if (typeof setup !== "string" || !/^[0-9a-f]{64}$/.test(setup) || !Number.isInteger(size) || size < 1 || size > MAX_SETUP_BYTES) {
    return null;
  }
  return { version, sha256: setup, size };
}

/** Where a channel's Setup is kept under the name a person downloads (release.rs, setup_download_object). */
export function setupDownloadUrl(releasesUrl, version, channel) {
  return `${releasesUrl.replace(/\/+$/, "")}/setup/VeyraSetup-${version}-${channel}.exe`;
}

export function setupFileName(version, channel) {
  return `VeyraSetup-${version}-${channel}.exe`;
}

/** A size for people: 12.4 MB. */
export function formatBytes(bytes) {
  const units = ["bytes", "KB", "MB", "GB"];
  let value = bytes;
  let unit = 0;
  while (value >= 1000 && unit < units.length - 1) {
    value /= 1000;
    unit += 1;
  }
  return unit === 0 ? `${value} bytes` : `${value.toFixed(value >= 100 ? 0 : 1)} ${units[unit]}`;
}

/**
 * The current Setup: { state: "ready", version, sha256, size, url, fileName }, or { state: "offline" } while
 * the release store cannot be reached, or { state: "unavailable" } when it holds no valid Setup for the channel.
 */
export async function latestSetup({ url, channel }, fetchImpl = (...args) => globalThis.fetch(...args)) {
  let response;
  try {
    response = await fetchImpl(ROUTES.launcherChannel(channel), { headers: { accept: "application/json" }, cache: "no-store" });
  } catch {
    return { state: "offline" };
  }
  if ([502, 503, 504].includes(response.status)) {
    return { state: "offline" };
  }
  if (!response.ok) {
    return { state: "unavailable" };
  }
  let release = null;
  try {
    release = parseLauncherChannel(await response.json());
  } catch {
    // Not JSON: unavailable.
  }
  if (!release) {
    return { state: "unavailable" };
  }
  return {
    state: "ready",
    ...release,
    url: setupDownloadUrl(url, release.version, channel),
    fileName: setupFileName(release.version, channel),
  };
}
