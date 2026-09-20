#!/usr/bin/env python3
"""Render a Vanguard character sheet as HTML from canon.

    python3 Docs/Design/Vanguards/render_sheet.py bryn          # render one sheet
    python3 Docs/Design/Vanguards/render_sheet.py --all         # render all 25
    python3 Docs/Design/Vanguards/render_sheet.py --prompts bryn  # art prompts for one
    python3 Docs/Design/Vanguards/render_sheet.py --missing       # what art is outstanding
    python3 Docs/Design/Vanguards/render_sheet.py --audit         # appearance text that splits badly

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
# art work order. `--prompts` emits a ready-to-run prompt per slot.
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
# cannot answer. They stay empty until there is a build to capture from, so
# `--prompts` will not emit a prompt for them and `--missing` counts them apart
# from the art that is genuinely outstanding.
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

    lore = [p for p in sec["prose"] if not re.match(r"^\*\*[A-Z][^:*]{2,40}:\*\*", p)][:4]
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


# The house style, split by what each slot can actually use. HOUSE_CORE is the idiom and
# holds everywhere. HOUSE_LIT is dramatic lighting and a full environment, which suits an
# illustration and directly contradicts a turnaround — those ask for flat even light on a
# plain background, and emitting both at once told the model to do two opposite things.
HOUSE_CORE = (
    "Heavily rendered illustration with physically believable materials: creatures, machines, "
    "armour, cloth, stone, water and foliage all read physically. Human characters are painterly-"
    "realistic: realistic proportions and weathered skin. The whole image is "
    "built around one saturated signature colour, which drives "
    "the light and the accents while everything else stays desaturated so that single hue "
    "carries the picture, and the silhouette stays readable at thumbnail size."
)

# Heroes the author has flagged for replacement. These five came back in an anime idiom
# rather than the painterly realism the rest of the roster uses, and are being regenerated
# once the first pass of 25 is complete.
#
# This is a work-order fact, not a canon one. Their appearance paragraphs are already
# reconciled with the art that exists and stay that way; what waits is the other eight
# slots, because every one of those is generated FROM the hero as reference. Producing
# them now would build eight images against a reference that is about to be replaced.
PROVISIONAL_HERO = {"kade", "tavi", "vera", "marek", "neris"}

HOUSE_LIT = (
    " Strong directional key in the signature colour with an opposing rim light, deep shadows "
    "and high contrast. The value key follows the character rather than a fixed rule: bright "
    "open daylight for some, deep night for others. The environment is a full, detailed scene "
    "held in depth behind the subject, so it enriches the frame while the silhouette still "
    "reads first."
)

# Both model families we target warn against negative phrasing — FLUX rejects it outright
# and inverts it in practice, and the Nano Banana guide lists "heavy use of negative
# phrasing" under what to avoid. The bible's guardrails are deliberately written as
# "is X, not Y" because that is the right form for a human art director, so the prompt
# layer has to do the conversion rather than the canon bending to suit a model.
#
# We split the appearance paragraph on that boundary: affirmative sentences become the bulk
# of the prompt, and the guardrail sentences are collected into one short trailing
# constraint. A single delimited constraint is what the guides tolerate; a prompt that is
# one-fifth negation scattered throughout is what they warn about.
# Detecting negation by listing the words that follow it does not work: the list is
# never finished, and a miss is silent — the sentence stays in the affirmative body
# and reaches the model in exactly the form the split exists to prevent. An earlier
# curated version leaked "not background decoration" (Vera), "does not confer"
# (Mimzi) and "not another miniature mage" (Celandrine) for precisely that reason.
# So match the negation itself and let the object of it be anything.
# Deliberately NOT matched: "rather than" and "instead of". They read as mild
# comparatives that both model families handle inline, and in these paragraphs they
# almost always sit mid-sentence after the description that matters — "carried like
# a service weapon rather than a hunting bow", "She rides a living wave rather than
# standing on the ground". Routing them would drag Vera's whole physical
# description and Neris's defining wave into the constraint block. Measured across
# the roster: 7 sentences would move, and every one is a net loss.
GUARDRAIL = re.compile(
    r"\bnot\b"          # covers is/are/does not, and bare "not X" appositives
    r"|\bnever\b"
    r"|\bn't\b"
    r"|\bno\s+\w"       # "no claws", "no skin and no flesh"
    r"|\bnothing\b"
    r"|\bavoid\b"
    r"|\bwithout\b",
    re.I,
)

# Rendering constraints that apply to every slot. Kept separate from the character's own
# guardrails so a sheet never picks up UI furniture, and so the two can be read apart.
NO_FURNITURE = (
    "The image carries no UI, watermark, artist signature, stat bar, health bar, ability icon "
    "or tuning number anywhere in the frame. Lettering exists only where it is part of the "
    "object itself, such as a unit designation stencilled on a machine's plating."
)




def _tidy(fragment: str) -> str:
    """Make a rejoined run of clauses read as a sentence again."""
    s = re.sub(r"^(?:and|but|or)\s+", "", fragment.strip(), flags=re.I)
    s = re.sub(r"[\s,;:—-]+$", "", s)
    if not s:
        return ""
    return s[0].upper() + s[1:] + ("" if s[-1] in ".!?" else ".")


def split_guardrails(look: str) -> tuple[str, str]:
    """Separate an appearance paragraph into affirmative description and guardrails.

    Partitioning whole sentences loses too much: the bible often states a
    character's defining feature and the guardrail against misreading it in one
    breath, so routing the sentence by its negation took Vera's spectral firing
    ranks, Torr's floating gaps and Relay's banner out of the description
    entirely. Clauses are the right unit — the affirmative half stays in the
    subject, the negative half moves to the constraint, and neither is lost.
    """
    body: list[str] = []
    rails: list[str] = []
    for sentence in re.split(r"(?<=[.!?])\s+", look.strip()):
        if not sentence:
            continue
        hit = GUARDRAIL.search(sentence)
        if not hit:
            body.append(sentence)
            continue
        # A negation governs everything after it, across commas: "not welded or
        # bolted — never draw him as a solid statue, a suit of armour, or a
        # seamless rock body" is one constraint, not a constraint followed by two
        # descriptions. Routing those trailing items separately asserted exactly
        # what the guardrail forbids, so the cut is made AT the first negation and
        # the whole tail goes with it.
        head, tail = sentence[:hit.start()], sentence[hit.start():]
        # Cut only where the negation begins a clause. A negation buried inside
        # one is doing descriptive work ("survived a war he was never built
        # for"), and splitting there leaves a mangled fragment on both sides —
        # that exact sentence put "survived a war he was." into Relay's prompt.
        # Where the cut is unsafe the whole sentence goes to the constraint:
        # affirmative content may move, but nothing is ever severed mid-clause.
        at_clause_start = re.search(r"[,;:—-]\s*$", head) is not None
        # A head too short to stand alone ("His own face" before "never becomes
        # frightening") is part of the constraint, not a description of its own.
        if at_clause_start and len(head.split()) >= 5:
            body.append(_tidy(head))
            rails.append(_tidy(tail))
        else:
            rails.append(sentence)
    return " ".join(filter(None, body)), " ".join(filter(None, rails))

# Art direction for the four antagonists. The note is the point: none of them is
# cruel, and lighting them like a villain would misread the writing.
ANTAGONIST = {
    "angeru": "Composed, methodical, unhurried; his expression stays flat whatever his body "
              "is doing.",
    "marek": "Delighted, curious, entirely unbothered. Whatever is behind or beside him should read "
             "as worse than he does.",
    "gorraveth": "Indifferent rather than enraged. He is not hunting the viewer; the viewer is simply "
                 "inside the contract area.",
    "tavi": "Bright, warm, genuinely happy. Nothing in the lighting or framing signals danger. "
            "That is the point — do not make her sinister.",
}


def prompts(vid: str) -> None:
    """Emit a ready-to-run art prompt per slot, with this Vanguard's canon injected."""
    src = next(HERE.glob(f"*-{vid}.yaml"), None)
    if src is None:
        sys.exit(f"no Vanguard data file for {vid!r}")
    d = yaml.safe_load(src.read_text(encoding="utf-8"))
    sec = parse_section(bible_section(d["roster_number"]))
    look = (sec["fields"].get("Visual language") or sec["fields"].get("Visual")
            or "No appearance paragraph in the bible — write one before generating art.")
    look = re.sub(r"\*\*(.+?)\*\*", r"\1", look)
    hue = HUE_BY_ID.get(vid) or HUE.get(d["origin_region"], "#9aa4ad")
    who = f"{d['name']}" + (f", {d['title']}" if d.get("title") else "")

    print(f"# Art prompts — {who}")
    print(f"# Region {d['origin_region']} · signature colour {hue} · nature {d['nature']}")
    print(f"# Save each result to ConceptArt/Vanguards/{vid}/<stem>.webp, then re-render the sheet.\n")
    print("# WORKFLOW: generate `hero` first and choose one of its variations. Feed that image")
    print("# back as a reference for the eight generated slots below — that is what holds")
    print("# the character consistent. Without it you get eight different people.")
    print("# The four in-game views are captured from the running game, not generated.")
    print("# Models: text-to-image for hero; a reference-driven model for the rest.\n")
    if vid in ANTAGONIST:
        print(f"# ANTAGONIST DIRECTION: {ANTAGONIST[vid]}\n")

    described, rails = split_guardrails(look)
    if vid in ANTAGONIST:
        a_keep, a_rails = split_guardrails(ANTAGONIST[vid])
        described = f"{described} {a_keep}".strip()
        rails = f"{rails} {a_rails}".strip()

    for stem, label, asp, grp, desc in SLOTS:
        print(f"--- {stem}.png  ({label}, {asp}) " + "-" * max(0, 46 - len(stem) - len(label)))
        if stem in CAPTURED:
            print("CAPTURED IN ENGINE — no prompt. This view is a screenshot of the running")
            print("game, taken once there is a build to take it from. Generating a picture of")
            print("a gameplay camera would answer the one question this slot exists to ask")
            print("(does the silhouette read at that distance?) with a guess.\n")
            continue
        if stem != "hero":
            print("Reference 1 is the approved hero image, and provides this character's identity,")
            print("proportions, materials, palette and equipment. Preserve all of those exactly.")
        # "Region environment" is meaningless to a model that has never heard of Veyra,
        # so the region and locality are substituted in rather than referred to.
        region, local = d["origin_region"], d.get("origin_locality")
        # A few Vanguards have a deliberately unplaced origin; naming it twice reads as
        # a bug in the prompt rather than as canon.
        where = local if region.lower() == "unknown" and local else region
        if local and where != local:
            where = f"{local}, {region}"
        shot = desc.replace("Region environment", f"The {where} environment is")
        # Subject first, then the shot, then style, then one delimited constraint —
        # the order both model guides ask for, and the reason the style block is no
        # longer a trailing keyword dump receiving the least attention weight.
        print(f"{who}. {described}\n\n{shot}\n\n"
              f"Style: {HOUSE_CORE}{'' if grp == 'turn' else HOUSE_LIT} "
              f"The signature colour is hex {hue}.\n\n"
              f"Critically: {rails} {NO_FURNITURE}\n\nAspect ratio {asp}.\n")


def missing(ids: list[str]) -> None:
    """List the art each Vanguard still needs, as a work order.

    Generated artwork and in-engine captures are counted separately: only the
    former is a commission anyone can act on today. Vanguards whose hero is being
    replaced are held apart again, because every other slot is generated FROM the
    hero and would be built against a reference that is about to change.
    """
    gen_total = cap_total = held = 0
    generate_n = len([s for s, *_ in SLOTS if s not in CAPTURED])
    for vid in ids:
        gaps = [stem for stem, *_ in SLOTS
                if not any((ART / vid / f"{stem}{e}").exists() for e in (".png", ".jpg", ".jpeg", ".webp"))]
        gen = [s for s in gaps if s not in CAPTURED]
        cap = [s for s in gaps if s in CAPTURED]
        gen_total += len(gen)
        cap_total += len(cap)
        state = "art complete" if not gen else f"{len(gen)}/{generate_n} to generate: " + " ".join(gen)
        if cap:
            state += f"   (+{len(cap)} awaiting capture)"
        if vid in PROVISIONAL_HERO:
            held += len(gen)
            state += "   [HOLD — hero is being replaced]"
        print(f"  {vid:<12} {state}")
    print(f"\n  {gen_total} images to generate across {len(ids)} Vanguards.")
    if held:
        print(f"  Of those, {held} are on hold behind a hero that is being replaced "
              f"({', '.join(sorted(PROVISIONAL_HERO))}).")
        print(f"  {gen_total - held} can be commissioned today.")
    print(f"  {cap_total} in-game views awaiting a build to capture from.")

    # Which heroes are the author's rather than the machine's. Derived from the files
    # rather than kept as a list in prose: the same count was maintained by hand in
    # Art_Direction_v0.1.md and drifted twice in a day, which is the exact failure that
    # document tells everyone else to avoid.
    authored = sorted(v for v in ids if (ART / v / "superseded_gen1_a.webp").exists())
    if authored:
        print(f"\n  {len(authored)}/{len(ids)} heroes are authored replacements, their appearance "
              f"paragraphs reconciled with the art:")
        print("    " + " ".join(authored))


def audit(ids: list[str]) -> int:
    """Report appearance sentences that lose description to the constraint block.

    `split_guardrails` only splits a sentence where the negation begins a clause and
    the affirmative head can stand alone. Everywhere else the whole sentence goes to
    the constraint, which is safe but silent: a sentence like "He is a living riverbed
    in motion and never resolves into a face" puts its best description into a block
    the prompt reads as a list of prohibitions.

    Nothing here is a bug in the splitter. It is a writing rule the bible already
    states — put the negation at the start of a clause — and this is what surfaces
    breaches of it. Advisory only: it reports candidates and leaves the judgement to
    a reader, because it cannot tell description from the subject of a constraint.
    """
    flagged = 0
    for vid in ids:
        src = next(HERE.glob(f"*-{vid}.yaml"), None)
        if src is None:
            continue
        d = yaml.safe_load(src.read_text(encoding="utf-8"))
        sec = parse_section(bible_section(d["roster_number"]))
        look = re.sub(r"\*\*(.+?)\*\*", r"\1", sec["fields"].get("Visual language") or "")
        for sentence in re.split(r"(?<=[.!?])\s+", look.strip()):
            hit = GUARDRAIL.search(sentence)
            if not hit:
                continue
            head = sentence[:hit.start()]
            if re.search(r"[,;:—-]\s*$", head) and len(head.split()) >= 5:
                continue
            # A head this short is the subject of a purely negative statement, which
            # belongs in the constraint whole. Longer means description went with it.
            if len(head.split()) < 5:
                continue
            flagged += 1
            print(f"  {vid}: {len(head.split())} words went to the constraint block with the negation")
            print(f"      \"{head.strip()}\" …")
            print()
    if flagged:
        print(f"  {flagged} sentence(s) to review. This flags candidates, it does not judge them:")
        print("  a long head can be lost description (Silt's \"a living riverbed in motion\") or")
        print("  simply the subject of a constraint (Oriel's \"Fragment arrangements and limbs")
        print("  should…\"), which belongs in the constraint whole. Read it and decide. Where it")
        print("  is description, end the sentence before the negation.")
    else:
        print("  Clean: every appearance paragraph splits without losing description.")
    return 0


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 1
    everything = [p.stem.split("-", 1)[1] for p in sorted(HERE.glob("*.yaml"))]

    if args[0] == "--prompts":
        for vid in (everything if args[1:2] == ["--all"] else args[1:]):
            prompts(vid)
        return 0
    if args[0] == "--missing":
        missing(everything if len(args) == 1 else args[1:])
        return 0
    if args[0] == "--audit":
        return audit(everything if len(args) == 1 else args[1:])

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
