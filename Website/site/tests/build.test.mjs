// The website's build and its agreements with the rest of the repository: `node --test Website/site/tests/`.

import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { after, before, describe, test } from "node:test";
import { artUrl, assemblePage, build, makeConfig, parseHues, parseVanguardYaml, readLauncherPublic, REPO_ROOT, validateSite } from "../build.mjs";
import { ROUTES } from "../src/js/backend.js";
import { describeServerHours } from "../src/js/hours.js";

const readJson = (path) => JSON.parse(readFileSync(join(REPO_ROOT, path), "utf8"));
const site = readJson("Website/site/site.json");
const vercel = readJson("vercel.json");

describe("site.json", () => {
  test("is valid", () => {
    assert.deepEqual(validateSite(site), []);
  });

  test("rejects what it must", () => {
    const broken = (change) => validateSite({ ...structuredClone(site), ...change });
    assert.match(broken({ extra: 1 }).join(), /unknown field "extra"/);
    assert.match(broken({ schemaVersion: 2 }).join(), /schemaVersion/);
    assert.match(broken({ featuredVanguards: [] }).join(), /at least one/);
    assert.match(broken({ featuredVanguards: [{ id: "kade", focus: "50% 50%" }, { id: "kade", focus: "50% 50%" }] }).join(), /twice/);
    assert.match(broken({ featuredVanguards: [{ id: "kade", focus: "middle" }] }).join(), /focus/);
    assert.match(broken({ heroRotationSeconds: 0 }).join(), /heroRotationSeconds/);
    assert.match(broken({ account: { ...site.account, displayName: { minLength: 5, maxLength: 3, pattern: "^a$" } } }).join(), /maxLength/);
    assert.match(broken({ account: { ...site.account, displayName: { ...site.account.displayName, pattern: "[a-z]+" } } }).join(), /anchored/);
    assert.match(broken({ support: { email: "not an email", links: [] } }).join(), /support\.email/);
    assert.match(broken({ support: { email: null, links: [{ label: "Chat", url: "http://insecure.example" }] } }).join(), /https/);
    assert.match(broken({ serverHours: { ...site.serverHours, timeZone: "Eastern" } }).join(), /IANA time zone/);
    assert.match(broken({ serverHours: { ...site.serverHours, timeZone: "Mars/Olympus_Mons" } }).join(), /IANA time zone/);
    assert.match(broken({ serverHours: { ...site.serverHours, opens: "10pm" } }).join(), /serverHours\.opens/);
    assert.match(broken({ serverHours: { ...site.serverHours, closes: "24:00" } }).join(), /serverHours\.closes/);
    assert.match(broken({ serverHours: { ...site.serverHours, closes: site.serverHours.opens } }).join(), /must differ/);
    assert.match(broken({ serverHours: { ...site.serverHours, days: "weekends" } }).join(), /unknown field "days"/);
  });
});

describe("the display-name rules", () => {
  // The backend owns them (ADR-038 §4); the website and the launcher check them first.
  test("match the backend's", () => {
    const identity = readFileSync(join(REPO_ROOT, "Backend/internal/identity/identity.go"), "utf8");
    assert.equal(Number(/MinDisplayNameLength = (\d+)/.exec(identity)[1]), site.account.displayName.minLength);
    assert.equal(Number(/MaxDisplayNameLength = (\d+)/.exec(identity)[1]), site.account.displayName.maxLength);
    assert.equal(/displayNamePattern = regexp\.MustCompile\(`([^`]+)`\)/.exec(identity)[1], site.account.displayName.pattern);
  });

  test("match the launcher's", () => {
    const player = readFileSync(join(REPO_ROOT, "Launcher/core/src/player.rs"), "utf8");
    assert.equal(Number(/MIN_DISPLAY_NAME_LENGTH: usize = (\d+)/.exec(player)[1]), site.account.displayName.minLength);
    assert.equal(Number(/MAX_DISPLAY_NAME_LENGTH: usize = (\d+)/.exec(player)[1]), site.account.displayName.maxLength);
  });
});

describe("the generated configuration", () => {
  const { config } = makeConfig();

  test("uses the public launcher's Firebase project and release channel", () => {
    const launcher = readJson("Launcher/config/public.json");
    assert.deepEqual(config.firebase, launcher.playerLogin.firebase);
    assert.deepEqual(config.releases, { url: launcher.game.install.releasesUrl, channel: launcher.game.install.channel });
  });

  test("names each featured Vanguard from canon", () => {
    assert.deepEqual(config.vanguards.map(({ id }) => id), site.featuredVanguards.map(({ id }) => id));
    const kade = config.vanguards.find(({ id }) => id === "kade");
    assert.deepEqual({ name: kade.name, title: kade.title, region: kade.region, roles: kade.roles }, { name: "Kade", title: "Dead Reckoning", region: "Iron March", roles: ["Marksman", "Utility carry"] });
    // A per-Vanguard colour wins over the region's.
    const marek = config.vanguards.find(({ id }) => id === "marek");
    assert.equal(marek.hue, "#9b4dd6");
    for (const vanguard of config.vanguards) {
      assert.match(vanguard.hue, /^#[0-9a-f]{6}$/);
      assert.match(vanguard.art, new RegExp(`^/art/${vanguard.id}\\.[0-9a-f]{10}\\.webp$`));
    }
  });

  test("names each piece of art after its bytes, so redrawn art reaches every visitor", () => {
    const source = join(REPO_ROOT, "ConceptArt", "Vanguards", "kade", "hero.webp");
    const root = mkdtempSync(join(tmpdir(), "veyra-art-"));
    try {
      const copy = join(root, "hero.webp");
      writeFileSync(copy, readFileSync(source));
      assert.equal(artUrl("kade", copy), artUrl("kade", source));
      writeFileSync(copy, Buffer.concat([readFileSync(source), Buffer.from([0])]));
      assert.notEqual(artUrl("kade", copy), artUrl("kade", source));
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });

  test("reads YAML scalars and lists, and the hue tables", () => {
    assert.deepEqual(parseVanguardYaml("id: kade\nname: Kade\nterrain_affinity: []\nrole_tags:\n  - marksman\n  - utility_carry\nabilities:\n  q: Throughline\n"), {
      id: "kade",
      name: "Kade",
      terrain_affinity: [],
      role_tags: ["marksman", "utility_carry"],
      abilities: [],
    });
    const hues = parseHues('HUE = {\n    "Merrin": "#C8536F",\n}\n\nHUE_BY_ID = {\n    # a comment\n    "patch": "#ff2d55",\n}\n');
    assert.deepEqual(hues, { byRegion: { Merrin: "#c8536f" }, byId: { patch: "#ff2d55" } });
  });

  test("refuses a launcher configuration without Firebase", () => {
    const root = mkdtempSync(join(tmpdir(), "veyra-launcher-"));
    try {
      const launcher = readJson("Launcher/config/public.json");
      delete launcher.playerLogin;
      mkdirSync(join(root, "Launcher", "config"), { recursive: true });
      writeFileSync(join(root, "Launcher", "config", "public.json"), JSON.stringify(launcher));
      assert.throws(() => readLauncherPublic(root), /playerLogin\.firebase/);
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });
});

describe("the built site", () => {
  let out;
  let pages;
  before(() => {
    out = mkdtempSync(join(tmpdir(), "veyra-site-"));
    build(REPO_ROOT, out);
    pages = readdirSync(out).filter((name) => name.endsWith(".html"));
  });
  after(() => rmSync(out, { recursive: true, force: true }));

  test("has every page, with its partials put in", () => {
    assert.deepEqual(pages.sort(), ["404.html", "account.html", "download.html", "index.html", "login.html", "signup.html", "support.html"]);
    for (const page of pages) {
      const html = readFileSync(join(out, page), "utf8");
      assert.doesNotMatch(html, /<!--#/, page);
      assert.match(html, /class="site-header"/, page);
      assert.match(html, /class="site-footer"/, page);
    }
    assert.ok(!existsSync(join(out, "partials")));
  });

  test("marks the current page in the header", () => {
    const html = readFileSync(join(out, "download.html"), "utf8");
    assert.match(html, /href="\/download" data-nav="download" aria-current="page"/);
    assert.doesNotMatch(html, /data-nav="home" aria-current/);
    assert.equal(assemblePage('<body data-page="b"><!--#x-->', { x: '<a data-nav="a"></a><a data-nav="b"></a>' }), '<body data-page="b"><a data-nav="a"></a><a data-nav="b" aria-current="page"></a>');
  });

  test("puts partials inside partials, but never one inside itself", () => {
    assert.equal(assemblePage('<body data-page="b"><!--#x-->', { x: "<i><!--#y--></i>", y: '<a data-nav="b"></a>' }), '<body data-page="b"><i><a data-nav="b" aria-current="page"></a></i>');
    assert.throws(() => assemblePage("<!--#x-->", { x: "<!--#y-->", y: "<!--#x-->" }), /x\.html includes itself, through x → y/);
  });

  test("names the servers' hours wherever a page shows them", () => {
    const hours = describeServerHours(site.serverHours).host;
    assert.match(hours, /^\d{1,2}:\d{2}.+\d{1,2}:\d{2} [AP]M \S/);
    for (const page of pages) {
      const html = readFileSync(join(out, page), "utf8");
      const shown = [...html.matchAll(/data-server-hours>([^<]*)</g)].map(([, text]) => text);
      // Every page has the footer's; the pages that talk about the servers have their own as well.
      assert.ok(shown.length >= 1, page);
      assert.deepEqual(new Set(shown), new Set([hours]), page);
    }
  });

  test("links only to files it has, clean URLs included", () => {
    const missing = [];
    for (const page of pages) {
      const html = readFileSync(join(out, page), "utf8");
      for (const [, url] of html.matchAll(/(?:href|src)="(\/[^"#?]*)/g)) {
        const path = url === "/" ? "index.html" : url.slice(1);
        if (!existsSync(join(out, path)) && !existsSync(join(out, `${path}.html`))) {
          missing.push(`${page}: ${url}`);
        }
      }
    }
    assert.deepEqual(missing, []);
  });

  test("keeps the content security policy: no inline script or style", () => {
    for (const page of pages) {
      const html = readFileSync(join(out, page), "utf8");
      assert.doesNotMatch(html, /\sstyle="/, page);
      assert.doesNotMatch(html, /<style/, page);
      assert.doesNotMatch(html, /\son[a-z]+="/, page);
      for (const [script] of html.matchAll(/<script[^>]*>/g)) {
        assert.match(script, /\ssrc="\/js\//, `${page}: ${script}`);
      }
    }
  });

  test("never lets a form put a password in a URL, even without its script", () => {
    for (const page of pages) {
      const html = readFileSync(join(out, page), "utf8");
      for (const [form] of html.matchAll(/<form[^>]*>/g)) {
        assert.match(form, /\smethod="post"/, `${page}: ${form}`);
      }
      for (const [input] of html.matchAll(/<input[^>]*type="password"[^>]*>/g)) {
        assert.doesNotMatch(input, /\sname=/, `${page}: ${input}`);
      }
    }
  });

  test("writes the configuration, the fonts and the art", () => {
    assert.match(readFileSync(join(out, "js", "site-config.js"), "utf8"), /^\/\/ Generated by Website\/site\/build\.mjs/);
    assert.ok(existsSync(join(out, "fonts", "Roboto-Black.ttf")));
    assert.ok(existsSync(join(out, "fonts", "NOTICE.md")));
    const { config } = makeConfig();
    for (const { id, art } of config.vanguards) {
      assert.ok(existsSync(join(out, art)), id);
    }
  });

  test("points the pages only at art named after its bytes", () => {
    for (const page of pages) {
      const html = readFileSync(join(out, page), "utf8");
      for (const [url] of html.matchAll(/\/art\/[^"]+/g)) {
        assert.match(url, /^\/art\/[a-z]+\.[0-9a-f]{10}\.webp$/, page);
      }
    }
  });
});

describe("vercel.json", () => {
  const launcher = readJson("Launcher/config/public.json");
  /** Where Vercel sends a path: the first rewrite whose source matches, with its :names filled in. */
  const rewrite = (path) => {
    for (const { source, destination } of vercel.rewrites) {
      const match = new RegExp(`^${source.replace(/:([a-z]+)/g, "(?<$1>[^/]+)")}$`).exec(path);
      if (match) {
        return destination.replace(/:([a-z]+)/g, (_, name) => match.groups[name]);
      }
    }
    return null;
  };

  test("builds the site and serves its output", () => {
    assert.equal(vercel.buildCommand, "node Website/site/build.mjs");
    assert.equal(vercel.outputDirectory, "Website/site/dist");
    assert.equal(vercel.cleanUrls, true);
  });

  test("sends the website's routes to the public launcher's backend and release store", () => {
    assert.equal(rewrite(ROUTES.status), `${launcher.backend.baseUrl}/readyz`);
    assert.equal(rewrite(ROUTES.register), `${launcher.backend.baseUrl}/v1/register`);
    assert.equal(rewrite(ROUTES.launcherChannel("public")), `${launcher.game.install.releasesUrl}/launcher/public.json`);
    // Nothing else of the backend's is reachable through the website, and never /v1/login (ADR-070 §4).
    assert.equal(vercel.rewrites.length, 3);
    assert.ok(!vercel.rewrites.some(({ destination }) => destination.includes("/v1/login")));
  });

  test("lets the pages reach Firebase and nothing else off-site", () => {
    const csp = vercel.headers.find(({ source }) => source === "/(.*)").headers.find(({ key }) => key === "Content-Security-Policy").value;
    const connect = /connect-src ([^;]+)/.exec(csp)[1].split(" ");
    assert.deepEqual(connect, ["'self'", launcher.playerLogin.firebase.authUrl]);
    assert.match(csp, /script-src 'self';/);
    assert.match(csp, /frame-ancestors 'none'/);
  });

  test("lets browsers keep the art and fonts", () => {
    const cache = (path) =>
      vercel.headers.filter(({ source }) => new RegExp(`^${source}$`).test(path)).flatMap(({ headers }) => headers).find(({ key }) => key === "Cache-Control")?.value;
    // Art is named after its bytes, so a copy never goes stale; fonts keep their names and only revalidate.
    assert.equal(cache("/art/kade.0123456789.webp"), "public, max-age=31536000, immutable");
    assert.match(cache("/fonts/Roboto-Black.ttf"), /stale-while-revalidate/);
  });

  test("rebuilds whenever anything the build reads changes", () => {
    for (const input of ["vercel.json", "Website/site", "Launcher/config/public.json", "Launcher/ui/fonts", "Docs/Design/Vanguards", "ConceptArt/Vanguards"]) {
      assert.ok(vercel.ignoreCommand.split(" ").includes(input), input);
    }
  });
});
