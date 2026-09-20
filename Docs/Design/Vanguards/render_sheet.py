#!/usr/bin/env python3
"""Render a Vanguard character sheet as HTML from canon.

    python3 Docs/Design/Vanguards/render_sheet.py bryn          # render one sheet
    python3 Docs/Design/Vanguards/render_sheet.py --all         # render all 25
    python3 Docs/Design/Vanguards/render_sheet.py --missing      # which slots have no art yet

Text is read at render time from the Character Bible and the Vanguard YAML.
Nothing is duplicated into a third place and nothing is typed by hand, so a
canon change is picked up by re-running this rather than by re-rendering
artwork and re-proofreading it.

Artwork is authored elsewhere and dropped into ConceptArt/Vanguards/<id>/. A
slot with a file shows it; a slot without one renders as a labelled placeholder
naming what belongs there and at what aspect, so the sheet doubles as the brief
for the art that will fill it.

This tool never bakes text into an image and never generates one. Both halves
matter: text baked into artwork cannot be diffed, validated or corrected, which
is exactly how the superseded sheets under ConceptArt/Characters/ drifted from
canon.
"""
from __future__ import annotations

import html
import os
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
ART = DESIGN.parent.parent / "ConceptArt" / "Vanguards"


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

# A Vanguard whose approved art diverges from their regional hue overrides it here.
# Approved art is canon (see Art_Direction_v0.1.md), so the region map is the default
# rather than the last word. Keep overrides visually separable from their neighbours
# — that separation is the only thing the hue table is for.
HUE_BY_ID = {
    # Patch keys off region "Unknown" (violet). His art is a hot luminous crimson,
    # so he takes one that stays clear of Reed Provinces and Iron March: measured
    # CIE76 distance 27 from #c0392b and 38 from #c4552b.
    "patch": "#ff2d55",
    # Korruk keys off Shatterdeep (cyan). Cyan appears nowhere in his design and
    # his own canon already says crimson is his default spine colour, so here the
    # text and the art agree against the region map. #ff1a1a is the best-separated
    # usable red left in a crowded band: measured CIE76 distance 29 from Patch
    # #ff2d55 and 34 from Reed Provinces #c0392b.
    "korruk": "#ff1a1a",
    # Marek keys off Drowned Cantons (teal, hue 190). His art is violet end to end and
    # his own canon already says violet eyes, a violet coat and violet behind Nix's mask.
    # The palette's violet #8a6bbf belongs to region "Unknown", so he takes a separated
    # one instead of squatting on it: measured CIE76 distance 32 from #8a6bbf and 45 from
    # Fluxborn #5e7fd4.
    "marek": "#9b4dd6",
    # Moro keys off Wildwood (yellow-green, hue 75) and his art measures 240 — a 165
    # degree gap, the largest in the set and twice the threshold the other overrides sit
    # at. #4040e0 is the best-separated indigo in that band: measured CIE76 distance 26
    # from Marek #9b4dd6, 48 from Unknown #8a6bbf and 53 from Fluxborn #5e7fd4.
    "moro": "#4040e0",
}


# Every image the sheet can hold: file stem, label, aspect, and what it must show.
# The renderer fills a slot when ConceptArt/Vanguards/<id>/<stem>.webp exists and
# renders a labelled placeholder when it does not, so the sheet doubles as the
# art work order: each empty slot states on the sheet what belongs in it.
SLOTS = [
    ("hero",    "Hero illustration", "16:9", "hero",
     "Full-figure hero illustration. The character occupies the frame, lit by the signature "
     "key light with an opposing rim. Region environment built out in depth behind them — a real "
     "place, not a backdrop — while the character stays the clear focal subject."),
    ("front",   "Turnaround — front", "1:2", "turn",
     "Full-body orthographic front view, neutral A-pose, even flat lighting, plain mid-grey "
     "background, no environment, no dramatic shadow. This is a modelling reference, not an illustration."),
    ("back",    "Turnaround — back", "1:2", "turn",
     "Full-body orthographic back view, neutral A-pose, even flat lighting, plain mid-grey background. "
     "Show how equipment, hair, cloth and any carried weapon sit from behind."),
    ("side",    "Turnaround — side", "1:2", "turn",
     "Full-body orthographic side profile, neutral A-pose, even flat lighting, plain mid-grey background."),
    ("scale",   "Scale silhouette", "1:2", "turn",
     "Flat black silhouette of the character beside a flat grey 1.8m human silhouette for scale, "
     "plain light background, no detail inside either shape."),
    ("idle",    "In-game — idle", "1:1", "view",
     "Three-quarter top-down gameplay camera, as seen in a MOBA. Character standing idle on "
     "battlefield ground. Small in frame. Readable silhouette is the priority."),
    ("move",    "In-game — move", "1:1", "view",
     "Three-quarter top-down gameplay camera. Character moving, mid-stride or mid-traversal."),
    ("cast",    "In-game — cast", "1:1", "view",
     "Three-quarter top-down gameplay camera. Character casting a basic ability, effect visible "
     "in the signature colour. The ability's real shape and area must be readable."),
    ("ult",     "In-game — ultimate", "1:1", "view",
     "Three-quarter top-down gameplay camera. Character using their ultimate, at full effect scale."),
    ("portrait", "Detail — portrait", "1:1", "detail",
     "Tight head-and-shoulders crop. Face or head structure clearly readable."),
    ("weapon",  "Detail — weapon / focus", "1:1", "detail",
     "Close crop of the weapon or focus object alone, three-quarter view, on a neutral background."),
    ("material", "Detail — material", "1:1", "detail",
     "Extreme close crop of the character's defining surface material, filling the frame."),
    ("signature", "Detail — signature element", "1:1", "detail",
     "Close crop of the single element that identifies this character at a glance."),
]


# The four in-game views are screen captures from the running game, not generated
# artwork. A generated picture of a gameplay camera is a guess about a build that
# does not exist yet, and the one thing these slots are for — whether the
# silhouette actually reads at gameplay distance — is the one thing such a guess
# cannot answer. They stay empty until there is a build to capture from, which is
# why `--missing` counts them apart from art that could be made today.
CAPTURED = {"idle", "move", "cast", "ult"}


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


def slot(vid: str, stem: str, label: str, note: str | None, cls: str) -> str:
    """An image when the art exists, a labelled work order when it does not."""
    for ext in (".png", ".jpg", ".jpeg", ".webp"):
        f = ART / vid / f"{stem}{ext}"
        if f.exists():
            rel = os.path.relpath(f, OUT)
            return (f"<div class='slot filled {cls}'>"
                    f"<img src='{html.escape(rel)}' alt='{html.escape(label)}'></div>")
    sub = f"<span class='slot-note'>{html.escape(note)}</span>" if note else ""
    if stem in CAPTURED:
        sub += "<span class='slot-note'>Captured in engine, not generated.</span>"
    return (f"<div class='slot {cls}'><span class='slot-label'>{html.escape(label)}</span>{sub}"
            f"<span class='slot-file'>{html.escape(stem)}.png</span></div>")


def render(vid: str) -> pathlib.Path:
    src = next(HERE.glob(f"*-{vid}.yaml"), None)
    if src is None:
        sys.exit(f"no Vanguard data file for {vid!r}")
    d = yaml.safe_load(src.read_text(encoding="utf-8"))

    sec = parse_section(bible_section(d["roster_number"]))
    hue = HUE_BY_ID.get(vid) or HUE.get(d["origin_region"], "#9aa4ad")
    title = d.get("title")

    # Most entries tell their story in free prose. The later ones (Mimzi, Celandrine,
    # Aurelisse, Eudora) are written as bolded fields instead, including a **Lore:** field,
    # and the prose filter below found nothing in them — so four sheets rendered an empty
    # lore column while claiming to be rendered from canon. Fall back to the field.
    lore = [p for p in sec["prose"] if not re.match(r"^\*\*[A-Z][^:*]{2,40}:\*\*", p)][:4]
    if not lore and sec["fields"].get("Lore"):
        lore = [sec["fields"]["Lore"]]
    visual = sec["fields"].get("Visual language") or sec["fields"].get("Visual") or ""
    # Physical size, where a Vanguard has one recorded. Optional: most are human-scaled
    # and say nothing. It renders beside the lore rather than as a meta chip because the
    # field carries its caveats with it, and a size still owed is worth as much to read
    # as one already settled.
    scale = sec["fields"].get("Scale") or ""
    scale_block = (f'\n      <div class="label" style="margin-top:20px">Scale</div>'
                   f'<p>{md(scale)}</p>') if scale else ""


    abil = "".join(
        f"""<article class="ability">
              <div class="ability-head"><span class="key">{html.escape(a['slot'])}</span>
              <h4>{md(a['name'])}</h4></div>
              {''.join(f'<p>{md(p)}</p>' for p in a['body'][:3])}
            </article>"""
        for a in sec["abilities"]
    )

    byslot = {k: (k, lab, asp, grp, desc) for k, lab, asp, grp, desc in SLOTS}

    def S(stem: str) -> str:
        k, lab, asp, grp, desc = byslot[stem]
        return slot(vid, k, lab, desc if grp == "hero" else None, grp)

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
.slot-file {{ font-size:10px; color:#41464d; font-family:ui-monospace,Menlo,monospace; }}
.slot.filled {{ border:1px solid var(--line); background:#0f1114; overflow:hidden; padding:0; }}
.slot.filled img {{ width:100%; height:100%; object-fit:contain; display:block;
                    background:#0b0c0e; }}
.slot-note {{ font-size:11px; color:#565c65; max-width:88%; line-height:1.4; }}
.hero {{ aspect-ratio:16/9; }}
/* An EMPTY hero keeps a shape so the work order has somewhere to sit. A FILLED
   one takes its aspect from the image instead: generators do not reliably honour
   a requested ratio, and forcing one either crops the art or letterboxes it. */
.hero.filled {{ aspect-ratio:auto; }}
.hero.filled img {{ height:auto; }}
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
  </div>
  <div class="panel"><div class="meta">
    <div><b>Role</b><span>{html.escape(' / '.join(t.replace('_',' ').title() for t in d['role_tags']))}</span></div>
    <div><b>Nature</b><span>{html.escape(d['nature'].replace('_',' ').title())}</span></div>
    <div><b>Region</b><span>{html.escape(d['origin_region'])}</span></div>
  </div></div>
</div>

<div style="margin-top:16px">{S('hero')}</div>

<div class="grid mid" style="margin-top:16px">
    <div class="panel lore"><div class="label">Lore</div>
      {''.join(f'<p>{md(p)}</p>' for p in lore)}{scale_block}
    </div>
    <div class="panel"><div class="label">Visual exploration</div>
      <div class="turn">{''.join(S(k) for k in ('front','back','side','scale'))}</div>
      {f'<p style="color:var(--dim);font-size:12.5px;margin:13px 0 0">{md(visual)}</p>' if visual else ''}
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
    {''.join(S(k) for k in ('idle','move','cast','ult'))}</div></div>
  <div class="panel"><div class="label">Details</div><div class="details">
    {''.join(S(k) for k in ('portrait','weapon','material','signature'))}</div></div>
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


def missing(ids: list[str]) -> None:
    """List which sheet slots each Vanguard still has no art for.

    Authored artwork and in-engine captures are counted separately: the second
    group cannot be filled at all until there is a build to capture from, so
    counting them together would overstate what is outstanding today.
    """
    gen_total = cap_total = 0
    generate_n = len([s for s, *_ in SLOTS if s not in CAPTURED])
    for vid in ids:
        gaps = [stem for stem, *_ in SLOTS
                if not any((ART / vid / f"{stem}{e}").exists() for e in (".png", ".jpg", ".jpeg", ".webp"))]
        gen = [s for s in gaps if s not in CAPTURED]
        cap = [s for s in gaps if s in CAPTURED]
        gen_total += len(gen)
        cap_total += len(cap)
        state = "art complete" if not gen else f"{len(gen)}/{generate_n} outstanding: " + " ".join(gen)
        if cap:
            state += f"   (+{len(cap)} awaiting capture)"
        print(f"  {vid:<12} {state}")
    print(f"\n  {gen_total} images outstanding across {len(ids)} Vanguards.")
    print(f"  {cap_total} in-game views awaiting a build to capture from.")

    # Which heroes carry authored replacement art. Derived from the files
    # rather than kept as a list in prose: the same count was maintained by hand in
    # Art_Direction_v0.1.md and drifted twice in a day, which is the exact failure that
    # document tells everyone else to avoid.
    authored = sorted(v for v in ids if (ART / v / "superseded_gen1_a.webp").exists())
    if authored:
        print(f"\n  {len(authored)}/{len(ids)} heroes are authored replacements, their appearance "
              f"paragraphs reconciled with the art:")
        print("    " + " ".join(authored))


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 1
    everything = [p.stem.split("-", 1)[1] for p in sorted(HERE.glob("*.yaml"))]

    if args[0] == "--missing":
        missing(everything if len(args) == 1 else args[1:])
        return 0

    for vid in (everything if args[0] == "--all" else args):
        print(f"  {render(vid)}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except BrokenPipeError:
        # Piping to head/less closes stdout early. Without this the tool prints a
        # traceback over the output the reader actually wanted.
        os._exit(0)
