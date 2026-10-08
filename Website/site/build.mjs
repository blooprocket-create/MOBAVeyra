// Builds Veyra's website (ADR-070) into Website/site/dist, which Vercel serves: `node Website/site/build.mjs`.
//
// The site keeps no copy of what another part of the repository owns. The build reads:
// - Launcher/config/public.json: the Firebase project and the release store and channel the public launcher uses,
//   so a player's website account and download are the same ones the launcher signs in to and installs from;
// - Docs/Design/Vanguards/<nn>-<id>.yaml: each featured Vanguard's name, title, region and roles;
// - Docs/Design/Vanguards/render_sheet.py: their signature colours (Art Direction, "Signature colours");
// - ConceptArt/Vanguards/<id>/hero.webp: their approved hero art, for them and for any page that shows one;
// - Launcher/ui/fonts: Roboto, the game's and the launcher's type.
// Website/site/site.json holds only what is the website's own. No dependencies: Node's standard library only.

import { createHash } from "node:crypto";
import { cpSync, existsSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { describeServerHours } from "./src/js/hours.js";

const HERE = dirname(fileURLToPath(import.meta.url));
export const REPO_ROOT = resolve(HERE, "..", "..");
const SITE_SCHEMA_VERSION = 1;
const LAUNCHER_SCHEMA_VERSION = 2;
// How many hex digits of an art file's SHA-256 its published name carries: enough that two drawings never share one.
const ART_HASH_LENGTH = 10;

class ConfigError extends Error {}

function fail(problems) {
  throw new ConfigError("Website/site/site.json and its sources have problems:\n- " + problems.join("\n- "));
}

function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8"));
}

function onlyKeys(object, allowed, where, problems) {
  for (const key of Object.keys(object ?? {})) {
    if (!allowed.includes(key)) {
      problems.push(`${where}: unknown field "${key}"`);
    }
  }
}

const isInteger = (value, min) => Number.isInteger(value) && value >= min;
const isHttpsUrl = (value) => typeof value === "string" && /^https:\/\/[^\s/]+(\/[^\s]*)?$/.test(value) && !value.endsWith("/");

/** The website's own data, checked strictly: every field is required and an unknown one is an error. */
export function validateSite(site) {
  const problems = [];
  onlyKeys(site, ["schemaVersion", "featuredVanguards", "heroRotationSeconds", "account", "support", "serverHours"], "site", problems);
  if (site.schemaVersion !== SITE_SCHEMA_VERSION) {
    problems.push(`schemaVersion must be ${SITE_SCHEMA_VERSION}`);
  }
  const featured = site.featuredVanguards;
  if (!Array.isArray(featured) || featured.length === 0) {
    problems.push("featuredVanguards must list at least one Vanguard");
  } else {
    const seen = new Set();
    featured.forEach((entry, index) => {
      onlyKeys(entry, ["id", "focus"], `featuredVanguards[${index}]`, problems);
      if (typeof entry.id !== "string" || !/^[a-z]+$/.test(entry.id)) {
        problems.push(`featuredVanguards[${index}].id must be a Vanguard id`);
      } else if (seen.has(entry.id)) {
        problems.push(`featuredVanguards lists ${entry.id} twice`);
      }
      seen.add(entry.id);
      if (typeof entry.focus !== "string" || !/^(100|[1-9]?\d)% (100|[1-9]?\d)%$/.test(entry.focus)) {
        problems.push(`featuredVanguards[${index}].focus must be "<x>% <y>%"`);
      }
    });
  }
  if (typeof site.heroRotationSeconds !== "number" || !(site.heroRotationSeconds > 0)) {
    problems.push("heroRotationSeconds must be a positive number");
  }
  const account = site.account ?? {};
  onlyKeys(account, ["displayName", "passwordMinLength"], "account", problems);
  const name = account.displayName ?? {};
  onlyKeys(name, ["minLength", "maxLength", "pattern"], "account.displayName", problems);
  if (!isInteger(name.minLength, 1) || !isInteger(name.maxLength, 1) || name.maxLength < name.minLength) {
    problems.push("account.displayName needs minLength and maxLength, 1 or more, with maxLength at least minLength");
  }
  try {
    if (typeof name.pattern !== "string" || !name.pattern.startsWith("^") || !name.pattern.endsWith("$")) {
      throw new Error();
    }
    new RegExp(name.pattern);
  } catch {
    problems.push("account.displayName.pattern must be an anchored regular expression");
  }
  if (!isInteger(account.passwordMinLength, 1)) {
    problems.push("account.passwordMinLength must be a whole number, 1 or more");
  }
  const support = site.support ?? {};
  onlyKeys(support, ["email", "links"], "support", problems);
  if (support.email !== null && (typeof support.email !== "string" || !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(support.email))) {
    problems.push("support.email must be an email address or null");
  }
  if (!Array.isArray(support.links)) {
    problems.push("support.links must be a list");
  } else {
    support.links.forEach((link, index) => {
      onlyKeys(link, ["label", "url"], `support.links[${index}]`, problems);
      if (typeof link.label !== "string" || link.label.trim() === "" || !isHttpsUrl(link.url)) {
        problems.push(`support.links[${index}] needs a label and an https url`);
      }
    });
  }
  const hours = site.serverHours ?? {};
  onlyKeys(hours, ["timeZone", "opens", "closes"], "serverHours", problems);
  if (!isTimeZone(hours.timeZone)) {
    problems.push("serverHours.timeZone must be an IANA time zone, such as America/New_York");
  }
  for (const key of ["opens", "closes"]) {
    if (typeof hours[key] !== "string" || !/^([01]\d|2[0-3]):[0-5]\d$/.test(hours[key])) {
      problems.push(`serverHours.${key} must be a 24-hour time, "HH:MM"`);
    }
  }
  if (hours.opens === hours.closes) {
    problems.push("serverHours.opens and serverHours.closes must differ");
  }
  return problems;
}

function isTimeZone(value) {
  if (typeof value !== "string" || !value.includes("/")) {
    return false;
  }
  try {
    new Intl.DateTimeFormat("en-US", { timeZone: value });
    return true;
  } catch {
    return false;
  }
}

/** What the public launcher's configuration says about the Firebase project and the release store. */
export function readLauncherPublic(root = REPO_ROOT) {
  const launcher = readJson(join(root, "Launcher", "config", "public.json"));
  const problems = [];
  if (launcher.schemaVersion !== LAUNCHER_SCHEMA_VERSION) {
    problems.push(`Launcher/config/public.json: expected schemaVersion ${LAUNCHER_SCHEMA_VERSION}`);
  }
  const firebase = launcher.playerLogin?.firebase;
  if (!isHttpsUrl(firebase?.authUrl) || typeof firebase?.apiKey !== "string" || firebase.apiKey === "") {
    problems.push("Launcher/config/public.json: playerLogin.firebase needs authUrl and apiKey");
  }
  const install = launcher.game?.install;
  if (!isHttpsUrl(install?.releasesUrl) || typeof install?.channel !== "string" || !/^[a-z0-9-]+$/.test(install.channel)) {
    problems.push("Launcher/config/public.json: game.install needs an https releasesUrl and a channel");
  }
  if (!isHttpsUrl(launcher.backend?.baseUrl)) {
    problems.push("Launcher/config/public.json: backend.baseUrl must be an https base URL");
  }
  if (problems.length > 0) {
    fail(problems);
  }
  return {
    backendUrl: launcher.backend.baseUrl,
    firebase: { authUrl: firebase.authUrl, apiKey: firebase.apiKey },
    releases: { url: install.releasesUrl, channel: install.channel },
  };
}

/** The top-level scalars and lists of a Vanguard's structural YAML: all the website reads. */
export function parseVanguardYaml(text) {
  const fields = {};
  let list = null;
  for (const line of text.split(/\r?\n/)) {
    const item = /^ {2}- (.+?)\s*$/.exec(line);
    if (item && list) {
      list.push(unquote(item[1]));
      continue;
    }
    const field = /^([a-z_]+):\s*(.*?)\s*$/.exec(line);
    if (field) {
      list = null;
      if (field[2] === "") {
        list = fields[field[1]] = [];
      } else {
        fields[field[1]] = field[2] === "[]" ? [] : unquote(field[2]);
      }
    } else if (!/^\s/.test(line)) {
      list = null;
    }
  }
  return fields;
}

function unquote(value) {
  return value.replace(/^(["'])(.*)\1$/, "$2");
}

/** The signature colours by region and the per-Vanguard overrides, from the sheet renderer's two tables. */
export function parseHues(source) {
  const table = (name) => {
    const block = new RegExp(`^${name} = \\{([\\s\\S]*?)^\\}`, "m").exec(source);
    if (!block) {
      fail([`Docs/Design/Vanguards/render_sheet.py: no ${name} table`]);
    }
    const entries = {};
    for (const match of block[1].matchAll(/^\s*"([^"]+)":\s*"(#[0-9a-fA-F]{6})"/gm)) {
      entries[match[1]] = match[2].toLowerCase();
    }
    return entries;
  };
  return { byRegion: table("HUE"), byId: table("HUE_BY_ID") };
}

function roleLabel(tag) {
  const words = tag.replace(/_/g, " ");
  return words.charAt(0).toUpperCase() + words.slice(1);
}

const heroArt = (id, root) => join(root, "ConceptArt", "Vanguards", id, "hero.webp");

/**
 * Where a Vanguard's art is published: named after its bytes, so redrawn art gets a new address that no browser or
 * cache can answer with the old picture, and an unchanged picture can be kept for good.
 */
export function artUrl(id, source) {
  const hash = createHash("sha256").update(readFileSync(source)).digest("hex").slice(0, ART_HASH_LENGTH);
  return `/art/${id}.${hash}.webp`;
}

/** Each featured Vanguard, from canon: name, title, region, roles, colour and art. */
export function readVanguards(featured, root = REPO_ROOT) {
  const dataDir = join(root, "Docs", "Design", "Vanguards");
  const files = readdirSync(dataDir).filter((name) => /^\d{2}-[a-z]+\.yaml$/.test(name));
  const hues = parseHues(readFileSync(join(dataDir, "render_sheet.py"), "utf8"));
  const problems = [];
  const vanguards = featured.map(({ id, focus }) => {
    const file = files.find((name) => name.endsWith(`-${id}.yaml`));
    if (!file) {
      problems.push(`no Docs/Design/Vanguards/<nn>-${id}.yaml`);
      return null;
    }
    const fields = parseVanguardYaml(readFileSync(join(dataDir, file), "utf8"));
    const art = heroArt(id, root);
    if (fields.id !== id || !fields.name || !fields.title || !fields.origin_region || !Array.isArray(fields.role_tags)) {
      problems.push(`${file}: needs id, name, title, origin_region and role_tags`);
    }
    if (!existsSync(art)) {
      problems.push(`no hero art at ConceptArt/Vanguards/${id}/hero.webp`);
    }
    const hue = hues.byId[id] ?? hues.byRegion[fields.origin_region] ?? hues.byRegion.Unknown;
    if (!hue) {
      problems.push(`${id}: no signature colour for region "${fields.origin_region}"`);
    }
    return {
      id,
      name: fields.name,
      title: fields.title,
      region: fields.origin_region,
      roles: (fields.role_tags ?? []).map(roleLabel),
      hue,
      focus,
      art: existsSync(art) ? artUrl(id, art) : null,
      source: art,
    };
  });
  if (problems.length > 0) {
    fail(problems);
  }
  return vanguards;
}

/** Everything the pages read at run time, and where each featured Vanguard's art comes from. */
export function makeConfig(root = REPO_ROOT) {
  const site = readJson(join(root, "Website", "site", "site.json"));
  const problems = validateSite(site);
  if (problems.length > 0) {
    fail(problems);
  }
  const launcher = readLauncherPublic(root);
  const vanguards = readVanguards(site.featuredVanguards, root);
  return {
    config: {
      firebase: launcher.firebase,
      releases: launcher.releases,
      heroRotationSeconds: site.heroRotationSeconds,
      account: site.account,
      support: site.support,
      serverHours: site.serverHours,
      vanguards: vanguards.map(({ source, ...vanguard }) => vanguard),
    },
    art: vanguards.map(({ id, art, source }) => ({ id, url: art, source })),
  };
}

/**
 * A page with its partials put in: each <!--#name--> becomes src/partials/name.html, or one the build generates. In
 * the header, the link whose data-nav names the page (its body's data-page) is marked as the current page.
 */
export function assemblePage(html, partials) {
  const page = /<body[^>]*\sdata-page="([a-z-]+)"/.exec(html)?.[1];
  const markCurrent = (text) => text.replace(/ data-nav="([a-z-]+)"/g, (attribute, nav) => (nav === page ? `${attribute} aria-current="page"` : attribute));
  // A partial may include others; `within` is the chain being filled, so one that includes itself fails.
  const fill = (text, within) =>
    text.replace(/<!--#([a-z-]+)-->/g, (marker, name) => {
      if (!(name in partials)) {
        fail([`a page includes ${marker}, but there is no src/partials/${name}.html`]);
      }
      if (within.includes(name)) {
        fail([`src/partials/${name}.html includes itself, through ${within.join(" → ")}`]);
      }
      return fill(markCurrent(partials[name].trimEnd()), [...within, name]);
    });
  return fill(html, []);
}

/**
 * Art the pages name by hand, as /art/<id>.webp, besides the featured Vanguards': each Vanguard's published file,
 * so the page can be pointed at it.
 */
function pageArt(pages, featured, root) {
  const ids = new Set(pages.flatMap((html) => [...html.matchAll(/\/art\/([a-z]+)\.webp"/g)].map(([, id]) => id)));
  const known = new Map(featured.map((entry) => [entry.id, entry]));
  const missing = [...ids].filter((id) => !known.has(id) && !existsSync(heroArt(id, root)));
  if (missing.length > 0) {
    fail(missing.map((id) => `a page shows /art/${id}.webp, but there is no hero art at ConceptArt/Vanguards/${id}/hero.webp`));
  }
  for (const id of ids) {
    if (!known.has(id)) {
      known.set(id, { id, url: artUrl(id, heroArt(id, root)), source: heroArt(id, root) });
    }
  }
  return [...known.values()];
}

/** Builds the site into outDir: the pages, the generated configuration, the fonts and the art. */
export function build(root = REPO_ROOT, outDir = join(root, "Website", "site", "dist")) {
  const { config, art: featuredArt } = makeConfig(root);
  const source = join(root, "Website", "site", "src");
  const partialsDir = join(source, "partials");
  const partials = Object.fromEntries(
    readdirSync(partialsDir)
      .filter((name) => name.endsWith(".html"))
      .map((name) => [name.slice(0, -".html".length), readFileSync(join(partialsDir, name), "utf8")]),
  );
  // The servers' hours on the host's clock, so a page reads right before its script adds the viewer's own.
  partials["server-hours"] = describeServerHours(config.serverHours).host;
  rmSync(outDir, { recursive: true, force: true });
  cpSync(source, outDir, { recursive: true, filter: (path) => path !== partialsDir });
  const pageNames = readdirSync(outDir).filter((file) => file.endsWith(".html"));
  const pages = new Map(pageNames.map((name) => [name, assemblePage(readFileSync(join(outDir, name), "utf8"), partials)]));
  const art = pageArt([...pages.values()], featuredArt, root);
  const urls = new Map(art.map(({ id, url }) => [id, url]));
  for (const [name, html] of pages) {
    writeFileSync(join(outDir, name), html.replace(/\/art\/([a-z]+)\.webp"/g, (_, id) => `${urls.get(id)}"`));
  }
  const fonts = join(root, "Launcher", "ui", "fonts");
  mkdirSync(join(outDir, "fonts"), { recursive: true });
  for (const name of readdirSync(fonts).filter((file) => /\.(ttf|md)$/.test(file))) {
    cpSync(join(fonts, name), join(outDir, "fonts", name));
  }
  mkdirSync(join(outDir, "art"), { recursive: true });
  for (const { url, source } of art) {
    cpSync(source, join(outDir, url));
  }
  const banner = "// Generated by Website/site/build.mjs from site.json and Launcher/config/public.json. Do not edit.\n";
  writeFileSync(join(outDir, "js", "site-config.js"), `${banner}export default Object.freeze(${JSON.stringify(config, null, 2)});\n`);
  return { outDir, config };
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    const { outDir, config } = build();
    console.log(`Built the website into ${outDir}: ${config.vanguards.length} featured Vanguards, channel "${config.releases.channel}".`);
  } catch (error) {
    console.error(error instanceof ConfigError ? error.message : error);
    process.exit(1);
  }
}
