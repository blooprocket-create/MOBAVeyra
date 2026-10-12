"""Builds the Crucible Spatial Plan review page from plan.svg, current.svg and metrics.json."""
import json
import re
from pathlib import Path

D = Path(__file__).resolve().parent
m = json.loads((D / "metrics.json").read_text(encoding="utf-8"))


def svg(name, label):
    text = (D / name).read_text(encoding="utf-8")
    text = re.sub(r"^<svg ", f'<svg class="mapsvg" aria-label="{label}" ', text, count=1)
    return text


r = m["fountain_routes"]
cur = m["current"]
lanes = m["lanes"]
cur_side = 2 * (cur["halfExtent"] - 800)
new_side = m["playableSide"]
area_gain = round(((new_side / cur_side) ** 2 - 1) * 100)


def sec(units):
    return f"{units / 330:.0f}&nbsp;s"


routes_rows = [
    ("Miremother", r["miremother"]), ("Stonehorn", r["stonehorn"]), ("Gloomwing", r["gloomwing"]),
    ("Razorback", r["razorback"]), ("Skittermaw", r["skittermaw"]), ("Ashfang", r["ashfang"]),
    ("New camp, bottom strip", r["bot-outer glade"]), ("New camp, top corner", r["top elbow pocket"]),
    ("Bot crossroads", r["bot crossroads"]), ("Top crossroads", r["top crossroads"]),
    ("South Flux Well", r["south Well"]), ("North Flux Well", r["north Well"]),
    ("Top outer ford", r["top outer ford"]), ("Bottom outer ford", r["bottom outer ford"]),
]
routes_html = "\n".join(
    f'<tr><th scope="row">{n}</th><td class="num">{v["units"]:,}</td><td class="num">{v["seconds"]:.1f}</td></tr>'
    for n, v in routes_rows)

interior = round(m["blocked_interior_share"] * 100)
edge = round(m["blocked_edge_rim_share"] * 100)
open_share = round(m["jungle_open_share"] * 100)

page = f"""<title>Crucible Spatial Plan</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Roboto+Condensed:wght@600;700&family=Roboto:wght@400;500;700&family=Roboto+Mono:wght@400;500&display=swap">
<style>
/* Layout: a field report. The map leads at full width; findings, numbers and decisions follow in one reading column. */
:root {{
  --ink: #0a0d12; --raised: #111720; --sunk: #070a0e; --line: rgba(236,238,242,.11); --line-strong: rgba(236,238,242,.2);
  --text: #eceef2; --muted: #95a0af; --flux: #4fd8cf; --flux-soft: rgba(79,216,207,.14); --gold: #d6b56e; --gold-soft: rgba(214,181,110,.14);
  --display: "Roboto Condensed", "Arial Narrow", "Segoe UI", sans-serif;
  --body: "Roboto", "Segoe UI", system-ui, sans-serif;
  --mono: "Roboto Mono", ui-monospace, "Cascadia Mono", Consolas, monospace;
  --cut: 12px;
  color-scheme: dark;
}}
@media (prefers-color-scheme: light) {{ :root:not([data-theme="dark"]) {{
  --ink: #eef1f4; --raised: #ffffff; --sunk: #e3e8ed; --line: rgba(16,21,29,.12); --line-strong: rgba(16,21,29,.24);
  --text: #121821; --muted: #556070; --flux: #0d7f79; --flux-soft: rgba(13,127,121,.1); --gold: #87661c; --gold-soft: rgba(135,102,28,.1);
  color-scheme: light; }} }}
:root[data-theme="light"] {{
  --ink: #eef1f4; --raised: #ffffff; --sunk: #e3e8ed; --line: rgba(16,21,29,.12); --line-strong: rgba(16,21,29,.24);
  --text: #121821; --muted: #556070; --flux: #0d7f79; --flux-soft: rgba(13,127,121,.1); --gold: #87661c; --gold-soft: rgba(135,102,28,.1);
  color-scheme: light; }}
* {{ box-sizing: border-box; }}
body {{ background: var(--ink); color: var(--text); font: 400 15px/1.6 var(--body); padding-inline: clamp(16px, 4vw, 40px); padding-block: 28px 64px; }}
.wrap {{ max-width: 1120px; margin-inline: auto; display: grid; gap: 40px; }}
header {{ display: grid; gap: 14px; max-width: 760px; }}
.eyebrow {{ font: 600 12px/1 var(--display); letter-spacing: .14em; text-transform: uppercase; color: var(--flux); }}
h1 {{ font: 700 clamp(34px, 6vw, 54px)/1 var(--display); letter-spacing: .01em; margin: 0; text-wrap: balance; }}
h2 {{ font: 700 24px/1.15 var(--display); margin: 0; text-wrap: balance; letter-spacing: .01em; }}
h3 {{ font: 600 16px/1.3 var(--display); margin: 0; letter-spacing: .02em; }}
p {{ margin: 0; }}
.lede {{ color: var(--muted); font-size: 16.5px; max-width: 64ch; }}
.chips {{ display: flex; flex-wrap: wrap; gap: 8px; }}
.chip {{ font: 500 12.5px/1 var(--body); padding: 7px 10px; border: 1px solid var(--line-strong); color: var(--muted); }}
.chip.warn {{ color: var(--gold); border-color: var(--gold); background: var(--gold-soft); }}
.mapcard {{ display: grid; grid-template-columns: minmax(0, 1fr) 280px; gap: 0; background: var(--raised); border: 1px solid var(--line);
  clip-path: polygon(var(--cut) 0, 100% 0, 100% calc(100% - var(--cut)), calc(100% - var(--cut)) 100%, 0 100%, 0 var(--cut)); }}
.mapview {{ background: #1d221d; padding: 14px; min-width: 0; }}
.mapsvg {{ width: 100%; height: auto; display: block; }}
.side {{ border-left: 1px solid var(--line); padding: 18px; display: grid; gap: 18px; align-content: start; min-width: 0; }}
.toggle {{ display: grid; grid-template-columns: 1fr 1fr; border: 1px solid var(--line-strong); }}
.toggle button {{ font: 600 13px/1 var(--display); letter-spacing: .06em; text-transform: uppercase; padding: 10px 8px; background: transparent; color: var(--muted); border: 0; cursor: pointer; }}
.toggle button[aria-pressed="true"] {{ background: var(--flux-soft); color: var(--flux); box-shadow: inset 0 -2px 0 var(--flux); }}
.toggle button:focus-visible, a:focus-visible {{ outline: 2px solid var(--flux); outline-offset: 2px; }}
.legend {{ list-style: none; margin: 0; padding: 0; display: grid; gap: 7px; font-size: 13px; color: var(--muted); }}
.legend li {{ display: grid; grid-template-columns: 22px 1fr; gap: 9px; align-items: center; }}
.sw {{ width: 22px; height: 14px; border: 1px solid rgba(0,0,0,.35); }}
.sw.round {{ width: 14px; height: 14px; border-radius: 50%; justify-self: center; }}
.sw.dia {{ width: 12px; height: 12px; transform: rotate(45deg); justify-self: center; }}
.note {{ font-size: 12.5px; color: var(--muted); }}
section {{ display: grid; gap: 16px; }}
.cols {{ display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 16px 28px; }}
.answer {{ display: grid; gap: 6px; padding-top: 12px; border-top: 1px solid var(--line); min-width: 0; }}
.answer .said {{ font: 500 13px/1.4 var(--body); color: var(--gold); }}
.answer p {{ color: var(--text); }}
.tablewrap {{ overflow-x: auto; border: 1px solid var(--line); background: var(--raised); }}
table {{ border-collapse: collapse; width: 100%; font-size: 14px; min-width: 520px; }}
th, td {{ text-align: left; padding: 10px 14px; border-bottom: 1px solid var(--line); vertical-align: top; }}
thead th {{ font: 600 12px/1.2 var(--display); letter-spacing: .1em; text-transform: uppercase; color: var(--muted); background: var(--sunk); }}
tbody th {{ font-weight: 500; }}
tr:last-child th, tr:last-child td {{ border-bottom: 0; }}
.num {{ font-family: var(--mono); font-variant-numeric: tabular-nums; text-align: right; white-space: nowrap; }}
td.new {{ color: var(--flux); }}
.twocol {{ display: grid; grid-template-columns: minmax(0, 1.2fr) minmax(0, 1fr); gap: 28px; align-items: start; }}
.facts {{ margin: 0; padding: 0; list-style: none; display: grid; gap: 10px; }}
.facts li {{ padding-left: 16px; position: relative; max-width: 68ch; }}
.facts li::before {{ content: ""; position: absolute; left: 0; top: .72em; width: 6px; height: 6px; background: var(--flux); }}
.decisions {{ display: grid; gap: 12px; counter-reset: none; }}
.decision {{ display: grid; grid-template-columns: 52px minmax(0, 1fr); gap: 14px; padding: 14px 16px; background: var(--raised); border: 1px solid var(--line); }}
.decision .id {{ font: 700 22px/1 var(--display); color: var(--gold); }}
.decision h3 {{ margin-bottom: 4px; }}
.decision p {{ color: var(--muted); }}
.decision p strong {{ color: var(--text); font-weight: 500; }}
ol.steps {{ margin: 0; padding-left: 22px; display: grid; gap: 8px; max-width: 72ch; }}
code {{ font-family: var(--mono); font-size: .9em; background: var(--sunk); padding: 1px 5px; }}
footer {{ color: var(--muted); font-size: 13px; border-top: 1px solid var(--line); padding-top: 16px; max-width: 80ch; }}
@media (max-width: 860px) {{
  .mapcard {{ grid-template-columns: minmax(0, 1fr); }}
  .side {{ border-left: 0; border-top: 1px solid var(--line); }}
  .cols, .twocol {{ grid-template-columns: minmax(0, 1fr); }}
}}
@media (prefers-reduced-motion: no-preference) {{ .toggle button {{ transition: background .15s, color .15s; }} }}
</style>

<div class="wrap">
<header>
  <div class="eyebrow">Crucible redesign · design proposal for review</div>
  <h1>Crucible Spatial Plan</h1>
  <p class="lede">A whole-map plan to agree before any terrain is generated. It places the playable spaces first: lanes, walled keeps, camp clearings, jungle routes, the river and its Flux Wells. Whatever is left over becomes rock and forest. Team B's half is Team A's turned half a turn, so every route, camp and objective is equally fair to both sides.</p>
  <div class="chips">
    <span class="chip warn">Proposal only. World.json changes after your approval</span>
    <span class="chip">Revision 2 · 12 Oct 2026</span>
    <span class="chip">Units are Unreal centimetres · 330 per second walking</span>
  </div>
</header>

<section>
  <h2>Revisions from your notes</h2>
  <div class="cols">
    <div class="answer"><span class="said">Room for more jungle camps</span><p>Two more camps per team, ringed on the map: one in the top-corner pocket by the outer ford, which the other team can contest from its outer jungle, and one in the bottom outer strip with its own lane mouth. Each team now has 8 camps: 4 inner and 4 outer.</p></div>
    <div class="answer"><span class="said">The river just flows off the map</span><p>The walkable river now ends 700 past each outer ford at a rock lip, then drops through a gorge of falls to the map edge. Nobody can wade out of the map.</p></div>
    <div class="answer"><span class="said">Space the middle turrets further apart</span><p>Mid Spires now stand 800, 3,000 and 5,200 from the inhibitor (gaps of 2,200, was 1,900). I read "middle turrets" as the mid lane Spires; if you meant the two base towers by the Prime Well, I&#39;ll spread those instead.</p></div>
    <div class="answer"><span class="said">Spires shouldn&#39;t sit in the middle of the lane</span><p>Each Spire now stands 450 from the lane&#39;s centre line, on the shoulder beside the road, alternating sides along the lane. Waves walk past it on the road. Sides are measured from each owner&#39;s walking direction, so both teams&#39; towers stay equivalent.</p></div>
    <div class="answer"><span class="said">More Dense Fog, including lane edges</span><p>{m["fog_bank_count_per_team"]} banks per team, up from 12. Nine are ribbons that hug the outside of a lane shoulder at lane mouths, mid doors and elbow insides, for flanks and escapes. Still none on a road or shoulder, and every lobe stays at least {m["fog_min_clearance_to_structures"]} from a structure, so towers are never hidden.</p></div>
  </div>
</section>

<section aria-label="Map">
  <div class="mapcard">
    <div class="mapview">
      <div id="view-plan">{svg("plan.svg", "Proposed Crucible layout")}</div>
      <div id="view-current" hidden>{svg("current.svg", "Current Crucible layout on main")}</div>
    </div>
    <div class="side">
      <div class="toggle" role="group" aria-label="Map shown">
        <button type="button" id="btn-plan" aria-pressed="true">Proposal</button>
        <button type="button" id="btn-current" aria-pressed="false">Current main</button>
      </div>
      <p class="note" id="map-note">Proposal: playable square {new_side:,} across (half extent {m['halfExtent']:,}). Team A's keep is bottom left.</p>
      <ul class="legend">
        <li><span class="sw" style="background:#d8c597"></span>Lane road, with its shoulder band</li>
        <li><span class="sw" style="background:#6f8f4e"></span>Jungle open ground: clearings and routes</li>
        <li><span class="sw" style="background:#33503a"></span>Rock and forest mass (derived)</li>
        <li><span class="sw" style="background:#3f8f9c"></span>River, wadeable, with banks</li>
        <li><span class="sw" style="background:#a7a59c;border-color:#3b3a36;border-width:3px"></span>Keep floor; dark edge is the wall, gaps are gates</li>
        <li><span class="sw" style="background:#eef1f3"></span>Dense Fog bank</li>
        <li><span class="sw round" style="background:#f0c04a"></span>Wildlife camp</li>
        <li><span class="sw round" style="background:#f0c04a;box-shadow:0 0 0 3px #fff6d6"></span>New camp (revision 2)</li>
        <li><span class="sw" style="background:#5b4f3f"></span>Falls: the river&#39;s walkable end</li>
        <li><span class="sw dia" style="background:#c9a2ff"></span>Flux Well on its island</li>
        <li><span class="sw round" style="background:#4f7fd6"></span>Team A Spires and base towers (B in red)</li>
        <li><span class="sw" style="background:#4f7fd6;width:14px;height:14px;justify-self:center"></span>Inhibitor, just inside each gate</li>
      </ul>
      <p class="note">Both views use the same frame, so the current map appears smaller: its playable square is {cur_side:,} across.</p>
    </div>
  </div>
</section>

<section>
  <h2>What the plan answers</h2>
  <div class="cols">
    <div class="answer"><span class="said">The layout barely changed</span><p>Every space is new. Side lanes bend through rounded elbows, keeps are walled, the outer jungle is twice as deep, and the jungle is a network of clearings and routes instead of an open field with strips.</p></div>
    <div class="answer"><span class="said">Walls consumed playable jungle</span><p>Routes and clearings are placed first and keep their full width. Interior masses cover {interior}% of the jungle, against 28% on main today, as solid forms at least 300 thick. Rims framing the outer routes against the boundary cliffs add {edge}%.</p></div>
    <div class="answer"><span class="said">No Dense Fog in lanes</span><p>{m["fog_bank_count_per_team"]} fog banks per team ({m["fog_lobes_per_team"]} lobes) sit at jungle mouths, river entrances, outer bends and along lane edges. None overlaps a lane road or shoulder.</p></div>
    <div class="answer"><span class="said">Bases need walls and real entrances</span><p>Each keep has a stone perimeter with three {m['keep']['gate_width']}-wide gates, one per lane. Inhibitors stand just inside the gates, equally far from the Prime Well. The well sits on a central plaza, with the fountain in the back corner.</p></div>
    <div class="answer"><span class="said">One section can't fix the whole map</span><p>This is the whole map at once. Nothing gets art or detail until the composition is agreed.</p></div>
    <div class="answer"><span class="said">Should we use Mesh Terrain?</span><p>The plan doesn't depend on it. Landscape plus cliff meshes can build every form here. Mesh Terrain remains a separate evaluation that would need its own ADR.</p></div>
  </div>
</section>

<section>
  <h2>Proportions</h2>
  <div class="tablewrap"><table>
    <thead><tr><th scope="col">Measure</th><th scope="col">Current main</th><th scope="col">Proposal</th></tr></thead>
    <tbody>
      <tr><th scope="row">Half extent</th><td class="num">{cur['halfExtent']:,}</td><td class="num new">{m['halfExtent']:,}</td></tr>
      <tr><th scope="row">Playable area</th><td class="num">{cur_side:,}²</td><td class="num new">{new_side:,}² (+{area_gain}%)</td></tr>
      <tr><th scope="row">Outer jungle beside top and bot</th><td class="num">{cur['outer_jungle_depth']:,} deep</td><td class="num new">{m['outer_jungle_depth']:,} deep</td></tr>
      <tr><th scope="row">Base</th><td>Open pad, radius {cur['base_pad_radius']:,}, no walls</td><td class="new">Walled keep, {m['keep']['depth_from_corner']:,} deep, 3 gates</td></tr>
      <tr><th scope="row">Top and bot lanes</th><td class="num">{cur['lane_lengths']['Top']:,} · {sec(cur['lane_lengths']['Top'])}</td><td class="num new">{lanes['Top']['length']:,} · {sec(lanes['Top']['length'])}</td></tr>
      <tr><th scope="row">Mid lane</th><td class="num">{cur['lane_lengths']['Mid']:,} · {sec(cur['lane_lengths']['Mid'])}</td><td class="num new">{lanes['Mid']['length']:,} · {sec(lanes['Mid']['length'])}</td></tr>
      <tr><th scope="row">Lane corners</th><td>Square</td><td class="new">Rounded elbows, radius 3,800</td></tr>
      <tr><th scope="row">Jungle that is open ground</th><td class="num">72%</td><td class="num new">{open_share}%</td></tr>
      <tr><th scope="row">Interior masses</th><td>{cur['walls']} wall strips, 28%</td><td class="new">{m['blocked_mass_count']} masses incl. rims, {interior}% interior</td></tr>
      <tr><th scope="row">Camps per team</th><td class="num">6</td><td class="num new">8: 4 inner, 4 outer</td></tr>
      <tr><th scope="row">Dense Fog</th><td>{cur['fog_lobes']} lobes in total</td><td class="new">{m['fog_bank_count_per_team']} banks per team, none on a lane</td></tr>
      <tr><th scope="row">Spire placement</th><td>On the lane centre line</td><td class="new">450 to the side, alternating</td></tr>
      <tr><th scope="row">Mid Spires from the inhibitor</th><td class="num">800 · 2,700 · 4,600</td><td class="num new">800 · 3,000 · 5,200</td></tr>
      <tr><th scope="row">River ends</th><td>Runs off the map</td><td class="new">Falls 700 past each outer ford</td></tr>
    </tbody>
  </table></div>
  <p class="note">Main's 72% open is mostly open field between strips. The proposal's {open_share}% is clearings and routes on purpose.</p>
</section>

<section class="twocol">
  <div style="display:grid;gap:14px;min-width:0">
    <h2>How the jungle is laid out</h2>
    <ul class="facts">
      <li><strong>Three route widths, not uniform tubes.</strong> Rotation routes are 1,000 wide (crossroads to camps and the river). Lane approaches and back doors are 700. The wraparound outer routes are 900.</li>
      <li><strong>Clearings sized for fights.</strong> Camps have radius 800 (Skittermaw's pack 900). Crossroads, glades and river mouths are 800 to 900. Well arenas are 1,350 around a 700 island. The mid ford is 1,100.</li>
      <li><strong>Masses are what's left.</strong> Any leftover thinner than 300 or smaller than 25&nbsp;m² becomes open ground, so no raised strips survive.</li>
      <li><strong>Readable lanes.</strong> Each camp has 3 to 5 entrances. Each team's lane legs get 3 inner and 3 outer mouths.</li>
      <li><strong>Top and bot feel different.</strong> The top inner jungle is a crossroads hub with a loop by the river. The bot side runs a low path along the lane between its two camps. The left outer strip loops around a central mass; the bottom one is a single long route of glades.</li>
      <li><strong>Four camps outside, four inside.</strong> Each outer strip holds two camps, so a second roamer has real farm without taking the inner jungle&#39;s.</li>
      <li><strong>Outer fords.</strong> Where the river leaves the map's corners, each team's outer route reaches a ford into the other side's outer jungle: the wraparound path for flank ganks and counter-jungling.</li>
    </ul>
  </div>
  <div style="display:grid;gap:14px;min-width:0">
    <h2>From Team A's fountain</h2>
    <div class="tablewrap"><table style="min-width:0">
      <thead><tr><th scope="col">Destination</th><th scope="col" class="num">Units</th><th scope="col" class="num">Seconds</th></tr></thead>
      <tbody>{routes_html}</tbody>
    </table></div>
    <p class="note">Shortest walking routes at 330 per second. Team B's are identical by the half turn.</p>
  </div>
</section>

<section>
  <h2>What the data model needs</h2>
  <p style="max-width:72ch">Today's <code>World.json</code> authors walls as variable-width splines, which the generator raises as ridges. That model produced the strips you rejected, and it has no way to say "keep this clearing at full size". This plan needs <code>World.json</code> to author the open spaces instead: clearings, routes with width classes, and keep perimeters with gates. The generator would then derive the rock and forest masses from what's left, under the thickness and area rules above. Runtime collision and sight would read those derived masses, either as polygons or broken into the boxes ADR-043 already uses. This changes ADR-043 §1 and ADR-040 §3, so it needs a new record, ADR-073, before implementation.</p>
</section>

<section>
  <h2>Decisions for you</h2>
  <div class="decisions">
    <div class="decision"><div class="id">D1</div><div><h3>Map size</h3><p><strong>Half extent 10,000, +{area_gain}% playable area,</strong> so the outer jungle can be {m['outer_jungle_depth']:,} deep with a camp, loops and lane mouths. The alternative keeps 9,000 and pulls the side lanes in 900, which shrinks both the outer and inner jungle.</p></div></div>
    <div class="decision"><div class="id">D2</div><div><h3>Keep entrances</h3><p><strong>Three lane gates only.</strong> The alternative adds one narrow side door per keep into each inner jungle, which gives defenders a flank but weakens the wall's purpose.</p></div></div>
    <div class="decision"><div class="id">D3</div><div><h3>Mid lane length</h3><p><strong>Mid walks in {sec(lanes['Mid']['length'])} instead of {sec(cur['lane_lengths']['Mid'])}</strong>, because the keeps sit deeper in the corners. Side lanes barely change. Smaller keeps (walls at 5,000) would bring mid to about 44 s.</p></div></div>
    <div class="decision"><div class="id">D4</div><div><h3>Flux Well lean</h3><p><strong>Each team reaches its bot-side Well {r['north Well']['seconds'] - r['south Well']['seconds']:.1f} s sooner than its top-side one</strong> (Team A: south {r['south Well']['seconds']:.1f} s, north {r['north Well']['seconds']:.1f} s; Team B mirrored). Keep it as a deliberate lean toward cross-map trades, or even it out.</p></div></div>
    <div class="decision"><div class="id">D5</div><div><h3>Jungle density</h3><p><strong>Interior masses at {interior}% of the jungle.</strong> Raise it toward 30 to 35% for more cover and sight-blocking, or lower it for more open fighting room.</p></div></div>
    <div class="decision"><div class="id">D6</div><div><h3>New camps&#39; wildlife</h3><p><strong>The two new camps per team need species.</strong> Reuse two of the six existing ones (camps keep fixed identities, so a second Ashfang den is allowed), or add new Veyran wildlife to the Battleground Bible.</p></div></div>
    <div class="decision"><div class="id">D7</div><div><h3>Data model</h3><p><strong>Write ADR-073: author the playable spaces, derive the masses.</strong> This comes before any generation pass.</p></div></div>
  </div>
</section>

<section>
  <h2>After you approve</h2>
  <ol class="steps">
    <li>ADR-073 and the <code>World.json</code> schema change, with validation for route widths, mass thickness, fog clearance and half-turn fairness.</li>
    <li>Generator: derive the masses, keep walls and gates, river and Well arenas from the new data, then regenerate the map.</li>
    <li>Carry over the reusable parts of Pass14: the validator's probe fix, the rule keeping map fog off lanes with its test, and the 30-route navigation suite.</li>
    <li>Validate navigation for both teams, sight and fairness; capture gameplay-camera views and an honest whole-map overhead for your review.</li>
    <li>Retune Spire distances and wave timing for the new lane lengths, as data only.</li>
  </ol>
</section>

<footer>
  <p><strong>Branch housekeeping.</strong> Codex's Pass14 is saved as local commit <code>0e96ba97</code> on <code>codex/crucible-jungle-layout</code>, not pushed, with nothing deleted. Its untracked review captures (about 133&nbsp;MB) stay in place. The checkout also holds Launcher edits (Cargo files, <code>build.rs</code>, launcher art) that aren't part of the map work; I left them untouched. The reference sheets you gave Codex informed principles only (bounded keeps, camp pockets, interlocking routes). Nothing here is traced from them.</p>
</footer>
</div>

<script>
(() => {{
  const views = {{ plan: document.getElementById('view-plan'), current: document.getElementById('view-current') }};
  const buttons = {{ plan: document.getElementById('btn-plan'), current: document.getElementById('btn-current') }};
  const note = document.getElementById('map-note');
  const notes = {{
    plan: 'Proposal: playable square {new_side:,} across (half extent {m["halfExtent"]:,}). Team A\\'s keep is bottom left.',
    current: 'Current main: playable square {cur_side:,} across (half extent {cur["halfExtent"]:,}), from World.json on main.'
  }};
  function show(which) {{
    for (const k of Object.keys(views)) {{ views[k].hidden = k !== which; buttons[k].setAttribute('aria-pressed', String(k === which)); }}
    note.textContent = notes[which];
  }}
  buttons.plan.addEventListener('click', () => show('plan'));
  buttons.current.addEventListener('click', () => show('current'));
  if (location.hash === '#current') show('current');
}})();
</script>
"""
(D / "Crucible-Spatial-Plan.html").write_text(page, encoding="utf-8")
print("written", len(page))
