#!/usr/bin/env python3
"""Render a Vanguard character sheet as HTML from canon.

    python3 Docs/Design/Vanguards/render_sheet.py bryn
    python3 Docs/Design/Vanguards/render_sheet.py --all

Text is read at render time from the Character Bible and the Vanguard YAML.
Nothing is duplicated into a third place and nothing is typed by hand, so a
canon change is picked up by re-running this rather than by re-rendering
artwork and re-proofreading it.

Artwork is deliberately NOT part of the output. Each image region renders as a
labelled slot naming what belongs there and at what aspect, so the sheet
doubles as the brief for the art that fills it.
"""
from __future__ import annotations

import html
import re
import sys
import pathlib
import datetime

try:
    import yaml
except ImportError:
    sys.exit("PyYAML is required:  pip install pyyaml")

HERE = pathlib.Path(__file__).resolve().parent
DESIGN = HERE.parent
OUT = HERE / "sheets"


def newest(stem: str) -> pathlib.Path:
    def version(p: pathlib.Path) -> tuple[int, int]:
        m = re.search(r"_v(\d+)\.(\d+)\.md$", p.name)
        return (int(m.group(1)), int(m.group(2))) if m else (-1, -1)
    found = sorted(DESIGN.glob(f"{stem}_v*.md"), key=version)
    return found[-1] if found else DESIGN / f"{stem}_MISSING.md"


BIBLE = newest("Veyra_Initial_Roster_Character_Bible")

# One saturated signature hue per region, per the house style. Neighbouring
# regions are kept apart so no two sheets collide in a draft-screen grid.
HUE = {
    "Iron March": "#c4552b",
    "Drowned Cantons": "#2e8b9e",
    "Merrin": "#c8536f",
    "Buried Riverlands": "#c19a4b",
    "Shatterdeep": "#6fc3d4",
    "Ember Basin": "#ff7a1a",
    "Bramblehollow": "#7fa650",
    "Wildwood": "#a8c64a",
    "Reed Provinces": "#c0392b",
    "Ancient Flux Network": "#4a8fd4",
    "Ancient Fluxborn system": "#5e7fd4",
    "Unknown": "#8a6bbf",
}

SLOTS = [
    ("hero", "Hero illustration", "3:4", "Full-figure character art, signature key light, atmospheric region background held well behind the subject."),
    ("front", "Turnaround — front", "1:2", None),
    ("back", "Turnaround — back", "1:2", None),
    ("side", "Turnaround — side", "1:2", None),
    ("scale", "Scale silhouette", "1:2", None),
]

DETAIL_SLOTS = ["Portrait", "Weapon / focus", "Material detail", "Signature element"]
VIEW_SLOTS = ["Idle", "Move", "Cast", "Ultimate"]


def bible_section(number: int) -> str:
    text = BIBLE.read_text(encoding="utf-8")
    m = re.search(rf"^# {number}\. .+?$(.*?)(?=^# \d+\. |\Z)", text, re.S | re.M)
    return m.group(1) if m else ""


def parse_section(raw: str) -> dict:
    """Split a bible section into header fields, prose, and abilities.

    Handles all three heading generations in the bible: `## X — Name`,
    `### X — Name`, and header fields run together on one line.
    """
    ability_re = re.compile(r"^#{2,4}\s+(Passive|Q|W|E|R)\s+—\s+(.+?)\s*$", re.M)

    first = ability_re.search(raw)
    head = raw[: first.start()] if first else raw

    fields: dict[str, str] = {}
    for key, val in re.findall(r"\*\*([A-Z][^:*]{2,40}):\*\*\s*([^\n]*)", head):
        fields.setdefault(key.strip(), val.strip().rstrip("  "))

    prose = [
        p.strip()
        for p in head.split("\n\n")
        if p.strip() and not p.strip().startswith(("**Origin", "**Version", ">"))
    ]

    abilities = []
    for m in ability_re.finditer(raw):
        nxt = ability_re.search(raw, m.end())
        body = raw[m.end(): nxt.start() if nxt else len(raw)]
        body = re.split(r"^\*\*(?:Play|Relationships|Roster|Mastery|Identity)", body, maxsplit=1, flags=re.M)[0]
        paras = [p.strip() for p in body.split("\n\n") if p.strip() and not p.strip().startswith("#")]
        abilities.append({"slot": m.group(1), "name": m.group(2), "body": paras})
    return {"fields": fields, "prose": prose, "abilities": abilities}


def md(text: str) -> str:
    """Minimal inline markdown → HTML. Escapes first; no raw HTML passes through."""
    t = html.escape(text)
    t = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", t)
    t = re.sub(r"(?<!\w)\*(?!\s)(.+?)(?<!\s)\*(?!\w)", r"<em>\1</em>", t)
    t = re.sub(r"`(.+?)`", r"<code>\1</code>", t)
    return t


def bullets(items: list[str]) -> str:
    if not items:
        return '<span class="none">none</span>'
    return "".join(f"<span class='chip'>{html.escape(i.replace('_', ' '))}</span>" for i in items)


def slot(label: str, note: str | None, cls: str) -> str:
    sub = f"<span class='slot-note'>{html.escape(note)}</span>" if note else ""
    return f"<div class='slot {cls}'><span class='slot-label'>{html.escape(label)}</span>{sub}</div>"


def render(vid: str) -> pathlib.Path:
    src = next(HERE.glob(f"*-{vid}.yaml"), None)
    if src is None:
        sys.exit(f"no Vanguard data file for {vid!r}")
    d = yaml.safe_load(src.read_text(encoding="utf-8"))

    sec = parse_section(bible_section(d["roster_number"]))
    hue = HUE.get(d["origin_region"], "#9aa4ad")
    title = d.get("title")

    lore = [p for p in sec["prose"] if not re.match(r"^\*\*[A-Z][^:*]{2,40}:\*\*", p)][:4]
    visual = sec["fields"].get("Visual language") or sec["fields"].get("Visual") or ""

    abil = "".join(
        f"""<article class="ability">
              <div class="ability-head"><span class="key">{html.escape(a['slot'])}</span>
              <h4>{md(a['name'])}</h4></div>
              {''.join(f'<p>{md(p)}</p>' for p in a['body'][:3])}
            </article>"""
        for a in sec["abilities"]
    )

    ents = d.get("owned_entities") or []
    ent_html = "".join(
        f"<li><strong>{html.escape(e['id'].replace('_',' '))}</strong> — {html.escape(e['category'].replace('_',' '))}"
        f"{' · destructible' if e.get('destructible') else ''}</li>" for e in ents
    ) or "<li class='none'>none</li>"

    guards = "".join(f"<li>{md(g)}</li>" for g in (d.get("guards") or []))
    vision = "".join(
        f"<li><strong>{html.escape(v['ability'].upper())}</strong> — {md(v['effect'])}</li>"
        for v in (d.get("vision_touchpoints") or [])
    ) or "<li class='none'>no vision interaction</li>"

    body = f"""<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<title>{html.escape(d['name'])} — Vanguard sheet</title>
<style>
:root {{ --hue: {hue}; --bg:#0c0d10; --panel:#141619; --line:#24272c;
         --ink:#e8e6e3; --dim:#9aa0a8; --faint:#6a7078; }}
* {{ box-sizing:border-box; }}
body {{ margin:0; background:var(--bg); color:var(--ink);
        font:15px/1.55 "Inter","Helvetica Neue",Arial,sans-serif; }}
.sheet {{ max-width:1220px; margin:0 auto; padding:34px; }}
h1,h2,h3,h4 {{ font-family:"Iowan Old Style","Palatino Linotype",Georgia,serif;
               margin:0; font-weight:600; }}
h1 {{ font-size:58px; letter-spacing:.5px; line-height:1; }}
.title {{ color:var(--hue); font-size:19px; letter-spacing:3.5px;
          text-transform:uppercase; margin-top:9px; }}
.grid {{ display:grid; gap:16px; }}
.panel {{ background:var(--panel); border:1px solid var(--line); border-radius:3px; padding:18px 20px; }}
.label {{ font-size:10px; letter-spacing:2.4px; text-transform:uppercase;
          color:var(--faint); margin-bottom:11px; }}
.top {{ grid-template-columns:1.08fr 1fr; align-items:start; }}
.meta {{ display:grid; grid-template-columns:repeat(3,1fr); gap:0; }}
.meta div {{ padding:0 14px; border-left:1px solid var(--line); }}
.meta div:first-child {{ padding-left:0; border-left:0; }}
.meta b {{ display:block; font-size:10px; letter-spacing:2px; color:var(--faint);
           text-transform:uppercase; margin-bottom:5px; }}
.meta span {{ font-family:Georgia,serif; font-size:17px; }}
.slot {{ border:1px dashed #333840; border-radius:3px; display:flex; flex-direction:column;
         align-items:center; justify-content:center; text-align:center; gap:5px;
         color:var(--faint); background:
           repeating-linear-gradient(135deg,transparent,transparent 9px,#121418 9px,#121418 18px); }}
.slot-label {{ font-size:11px; letter-spacing:1.6px; text-transform:uppercase; color:#79808a; }}
.slot-note {{ font-size:11px; color:#565c65; max-width:88%; line-height:1.4; }}
.hero {{ aspect-ratio:3/4; }}
.turn {{ display:grid; grid-template-columns:repeat(4,1fr); gap:9px; }}
.turn .slot {{ aspect-ratio:1/2; }}
.views, .details {{ display:grid; grid-template-columns:repeat(4,1fr); gap:9px; }}
.views .slot, .details .slot {{ aspect-ratio:1/1; }}
.mid {{ grid-template-columns:1.5fr 1fr; }}
.abilities {{ display:grid; gap:13px; }}
.ability {{ border-left:2px solid var(--hue); padding-left:13px; }}
.ability-head {{ display:flex; align-items:baseline; gap:9px; margin-bottom:5px; }}
.key {{ background:var(--hue); color:#0b0c0e; font-size:11px; font-weight:700;
        padding:2px 7px; border-radius:2px; letter-spacing:1px; }}
.ability h4 {{ font-size:17px; }}
.ability p {{ margin:5px 0; color:var(--dim); font-size:13.5px; }}
.chip {{ display:inline-block; background:#1c1f24; border:1px solid var(--line);
         color:var(--dim); font-size:11.5px; padding:3px 9px; border-radius:2px;
         margin:0 5px 5px 0; letter-spacing:.3px; }}
.profile dt {{ font-size:10px; letter-spacing:2px; text-transform:uppercase;
               color:var(--faint); margin-top:13px; }}
.profile dt:first-child {{ margin-top:0; }}
.profile dd {{ margin:5px 0 0; }}
ul {{ margin:0; padding-left:17px; }}
li {{ margin:6px 0; color:var(--dim); font-size:13px; }}
.none {{ color:#4e545c; font-style:italic; }}
blockquote {{ margin:0 0 20px; font-family:Georgia,serif; font-size:20px;
              font-style:italic; color:var(--dim); border-left:2px solid var(--hue); padding-left:16px; }}
.lore p {{ color:var(--dim); font-size:13.5px; margin:0 0 10px; }}
.prov {{ margin-top:22px; padding-top:14px; border-top:1px solid var(--line);
         color:#565c65; font-size:11.5px; line-height:1.7; }}
.prov strong {{ color:#79808a; font-weight:600; }}
.warn {{ border-left:2px solid var(--hue); padding-left:12px; color:#9aa0a8; font-size:12.5px; }}
</style></head><body><div class="sheet">

<div class="grid top">
  <div>
    <h1>{html.escape(d['name'])}</h1>
    {f'<div class="title">{html.escape(title)}</div>' if title else ''}
    <div style="margin-top:18px">{slot(*SLOTS[0][1:3], 'hero') if False else slot('Hero illustration','Full figure, signature key light, region background held behind the subject. 3:4.','hero')}</div>
  </div>
  <div class="grid" style="gap:16px">
    <div class="panel"><div class="meta">
      <div><b>Role</b><span>{html.escape(' / '.join(t.replace('_',' ').title() for t in d['role_tags']))}</span></div>
      <div><b>Nature</b><span>{html.escape(d['nature'].replace('_',' ').title())}</span></div>
      <div><b>Region</b><span>{html.escape(d['origin_region'])}</span></div>
    </div></div>
    <div class="panel lore"><div class="label">Lore</div>
      {''.join(f'<p>{md(p)}</p>' for p in lore)}
    </div>
    <div class="panel"><div class="label">Visual exploration</div>
      <div class="turn">{slot('Front',None,'')}{slot('Back',None,'')}{slot('Side',None,'')}{slot('Scale',None,'')}</div>
      {f'<p style="color:var(--dim);font-size:12.5px;margin:13px 0 0">{md(visual)}</p>' if visual else ''}
    </div>
  </div>
</div>

<div class="grid mid" style="margin-top:16px">
  <div class="panel"><div class="label">Ability kit</div><div class="abilities">{abil}</div></div>
  <div class="grid" style="gap:16px">
    <div class="panel"><div class="label">Profile</div><dl class="profile">
      <dt>Damage</dt><dd>{bullets(d['damage_profile'])}</dd>
      <dt>Basic attack</dt><dd>{bullets([d['basic_attack']['damage_type'], d['basic_attack']['range_class']])}</dd>
      <dt>Resource</dt><dd>{bullets([d['resource']])}</dd>
      <dt>Crowd control</dt><dd>{bullets(d.get('cc') or [])}</dd>
      <dt>Mobility</dt><dd>{bullets(d.get('mobility') or [])}</dd>
      <dt>Grants</dt><dd>{bullets(d.get('grants') or [])}</dd>
      <dt>Stealth</dt><dd>{bullets(d.get('stealth') or [])}</dd>
    </dl></div>
    <div class="panel"><div class="label">Owned field entities</div><ul>{ent_html}</ul></div>
    <div class="panel"><div class="label">Vision interaction</div><ul>{vision}</ul></div>
  </div>
</div>

<div class="grid" style="grid-template-columns:1fr 1fr; margin-top:16px">
  <div class="panel"><div class="label">In-game views</div><div class="views">
    {''.join(slot(v,None,'') for v in VIEW_SLOTS)}</div></div>
  <div class="panel"><div class="label">Details</div><div class="details">
    {''.join(slot(v,None,'') for v in DETAIL_SLOTS)}</div></div>
</div>

<div class="panel" style="margin-top:16px"><div class="label">Implementation guards — must survive to code</div><ul>{guards}</ul></div>

<div class="prov">
  <strong>Rendered from canon.</strong>
  Structure: <code>Docs/Design/Vanguards/{html.escape(src.name)}</code>.
  Narrative and abilities: <code>{html.escape(BIBLE.name)}</code> §{d['roster_number']}.
  Generated {datetime.date.today().isoformat()} by <code>render_sheet.py</code>.<br>
  No text on this sheet was typed by hand or copied into a third location, and no
  tuning value appears on it — the data layer rejects both. Re-run the renderer after a
  canon change; the artwork that fills the slots is unaffected.
</div>
</div></body></html>"""

    OUT.mkdir(exist_ok=True)
    dest = OUT / f"{d['roster_number']:02d}-{vid}.html"
    dest.write_text(body, encoding="utf-8")
    return dest


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 1
    ids = ([p.stem.split("-", 1)[1] for p in sorted(HERE.glob("*.yaml"))]
           if args[0] == "--all" else args)
    for vid in ids:
        print(f"  {render(vid)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
