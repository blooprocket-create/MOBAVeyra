// A local preview of the built website, close to how Vercel serves it: `node Website/site/serve.mjs [port]` after
// `node Website/site/build.mjs`. It reads vercel.json for clean URLs, the response headers (the content security
// policy among them) and the rewrites, which it forwards to their real destinations. For trying pages out, not
// for production.

import { createServer } from "node:http";
import { readFile, stat } from "node:fs/promises";
import { extname, join, normalize, resolve } from "node:path";
import { REPO_ROOT } from "./build.mjs";

const DIST = join(REPO_ROOT, "Website", "site", "dist");
const VERCEL = JSON.parse(await readFile(join(REPO_ROOT, "vercel.json"), "utf8"));
const PORT = Number(process.argv[2] ?? 4173);
const TYPES = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".svg": "image/svg+xml",
  ".webp": "image/webp",
  ".ttf": "font/ttf",
  ".md": "text/markdown; charset=utf-8",
};

/** A Vercel source pattern as a regular expression: ":name" segments, and groups like "(.*)" as they are. */
function pattern(source) {
  return new RegExp(`^${source.replace(/:([a-z]+)/gi, "(?<$1>[^/]+)")}$`);
}

const rewrites = VERCEL.rewrites.map(({ source, destination }) => ({ match: pattern(source), destination }));
const headerRules = VERCEL.headers.map(({ source, headers }) => ({ match: pattern(source), headers }));

function headersFor(path) {
  const headers = {};
  for (const rule of headerRules.filter(({ match }) => match.test(path))) {
    for (const { key, value } of rule.headers) {
      headers[key] = value;
    }
  }
  return headers;
}

async function file(path) {
  const full = resolve(DIST, `.${normalize(path)}`);
  if (!full.startsWith(DIST)) {
    return null;
  }
  for (const candidate of [full, `${full}.html`, join(full, "index.html")]) {
    try {
      if ((await stat(candidate)).isFile()) {
        return candidate;
      }
    } catch {
      // Try the next.
    }
  }
  return null;
}

createServer(async (request, response) => {
  const url = new URL(request.url, `http://${request.headers.host}`);
  const path = decodeURIComponent(url.pathname);
  for (const { match, destination } of rewrites) {
    const found = match.exec(path);
    if (found) {
      const target = destination.replace(/:([a-z]+)/gi, (_, name) => found.groups?.[name] ?? "");
      const chunks = [];
      for await (const chunk of request) {
        chunks.push(chunk);
      }
      try {
        const upstream = await fetch(target, {
          method: request.method,
          headers: { "content-type": request.headers["content-type"] ?? "application/json", accept: "application/json" },
          body: ["GET", "HEAD"].includes(request.method) ? undefined : Buffer.concat(chunks),
        });
        response.writeHead(upstream.status, { ...headersFor(path), "content-type": upstream.headers.get("content-type") ?? "text/plain" });
        response.end(Buffer.from(await upstream.arrayBuffer()));
      } catch {
        response.writeHead(502, { "content-type": "text/plain" }).end("Bad gateway\n");
      }
      return;
    }
  }
  if (VERCEL.cleanUrls && path.endsWith(".html")) {
    response.writeHead(308, { location: path.replace(/(index)?\.html$/, "") || "/" }).end();
    return;
  }
  const found = await file(path);
  const served = found ?? join(DIST, "404.html");
  response.writeHead(found ? 200 : 404, { ...headersFor(path), "content-type": TYPES[extname(served)] ?? "application/octet-stream" });
  response.end(await readFile(served));
}).listen(PORT, () => console.log(`Previewing Website/site/dist on http://localhost:${PORT}`));
