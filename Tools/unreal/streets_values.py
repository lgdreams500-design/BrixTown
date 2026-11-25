"""The slice's gardens, lawns and open ground for the Unreal build's tests (CITY-017, WLD-040, WLD-056, ART-001).

Runs the slice's own drawing functions (Prototype/play/play.js plant(), heads(), plantClump(), lawn(), wildProp(),
greenGround()) in headless Edge (this machine has no Node), cut from play.js as they are, with the town's mesh swapped for a
recorder: each call they make (box, ybox, flat, quad, dome, ribbon, disc, the lettering, solid) with its numbers. They draw
every plant and wild thing in the block files, every garden's lawn and every block of open ground, with today's plants
(Game/Data/streets.json, written by Tools/worldgen/build_play.py). Writes
  Tests/values/streets_city_01.json   per thing: how many calls and a checksum of all their numbers, and every call in
                                      full for one thing in twenty. CITY017_every_plant_is_drawn_as_the_slice_draws_it and
                                      WLD056_the_open_ground_is_drawn_as_the_slice_draws_it check BrixWorld's BrixStreets
                                      against it.
Also play.js tree() for every tree today and roads.js for the road network (TREES_HARNESS), for
WLD056_every_tree_is_drawn_as_the_slice_draws_it and CITY043_the_road_markings_and_signals_are_drawn_as_the_slice_draws_them.
The heights the functions read are the Unreal town's (the paving 0.12 m, an open lot 0.16 m, the open ground -0.03 m), set
the same way by the tests. Run it again after changing play.js, the block files or the vegetation rules (after build_play.py).
  python Tools/unreal/streets_values.py
"""
import glob
import html
import json
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
PLAY_JS = os.path.join(ROOT, "Prototype", "play", "play.js")
STREETS = os.path.join(ROOT, "Game", "Data", "streets.json")
CITY = os.path.join(ROOT, "World", "city_01")
OUT = os.path.join(ROOT, "Tests", "values", "streets_city_01.json")
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
FUNCS = ["heads", "plantClump", "plant", "lawn", "wildProp", "greenGround"]
CONSTS = ["clamp", "mix", "mix3", "mul3", "LAWN", "PLANTS", "BOARD", "CROP", "GREEN_KINDS"]
DRESS_JS = os.path.join(ROOT, "Prototype", "play", "dressing.js")
DAY = os.path.join(ROOT, "Game", "Data", "day.json")
ROADS_JS = os.path.join(ROOT, "Prototype", "play", "roads.js")

# play.js tree() and treeRing() for every tree today, and roads.js for the road network (WLD-056, CITY-043, CITY-045,
# CITY-091): each block square's zebras, lay-bys, bus stops and traffic-light poles, each head's lamp, and every signal's
# phase over a day. The ground under a tree or an out-of-town shelter is the Unreal town's rule, as the test sets it: the
# open ground -0.03 inside a block of open ground, the paving 0.12 inside any other block, the street 0.
TREES_HARNESS = r"""
let REC = [];
const rec = (k, a, c) => REC.push([k, a, c || null]);
const world = {
  box(x0, y0, z0, x1, y1, z1, c) { rec(0, [x0, y0, z0, x1, y1, z1], c); },
  ybox(cx, y0, cz, w, h, d, yaw, c) { rec(1, [cx, y0, cz, w, h, d, yaw], c); },
  flat(x0, z0, x1, z1, y, c) { rec(2, [x0, z0, x1, z1, y], c); },
  quad(p0, p1, p2, p3, c, f, hint) { rec(4, [...p0, ...p1, ...p2, ...p3, ...hint], c); },
};
function solid(x0, y0, z0, x1, y1, z1) { rec(8, [x0, y0, z0, x1, y1, z1], null); }
const Y_WILD = -.03, Y_GRASS = .15, P = D.pitch, LAWNS = [], blockAt = new Map();
for (const b of D.blocks) {
  const [ci, cj] = b.span || [1, 1]; b.ext = [b.size + (ci - 1) * P, b.size + (cj - 1) * P];
  for (let di = 0; di < ci; di++) for (let dj = 0; dj < cj; dj++) blockAt.set((b.grid[0] + di) + "," + (b.grid[1] + dj), b);
}
const blockOf = (x, z) => blockAt.get(Math.floor(x / P) + "," + Math.floor(z / P));
function groundBase(x, z) {
  for (const b of D.blocks) { const [bx, bz] = b.origin; if (x > bx && z > bz && x < bx + b.ext[0] && z < bz + b.ext[1]) return GREEN_KINDS[b.template] ? Y_WILD : .12; }
  return 0;
}
try {
for (const b of D.blocks) {   // the lawns in drawBase()'s order (lanes: none of the open ground has them)
  const [bx, bz] = b.origin, S = b.size, [SX, SZ] = b.ext;
  if (GREEN_KINDS[b.template] && b.template !== "woods") LAWNS.push([bx, bz, bx + SX, bz + SZ, Y_WILD]);
  if (b.template === "rows") LAWNS.push([bx + 2, bz + 16.5, bx + S - 2, bz + S - 16.5, Y_GRASS]);
  if (b.template === "park") LAWNS.push([bx + 1, bz + 1, bx + SX - 1, bz + SZ - 1, Y_GRASS]);
  for (const l of b.lots) if (l.kind === "garden") LAWNS.push([...l.rect, Y_GRASS + .02]);
}
const out = { trees: {}, roads: {}, lamps: [], phase: [] };
D.trees.forEach((t, i) => { const bk = blockOf(t[0], t[1]); REC = []; tree(t, !!(bk && bk.template === "woods")); out.trees[i] = REC; });
for (const k of Roads.cells()) { REC = []; Roads.build(world, .12, k); out.roads[k] = REC; }
REC = []; out.lit = Roads.draw(world, 600, null, 0); out.lamps = REC;
for (let s = 0; s < Roads.data.signals.length; s++) for (let m = 0; m < 1440; m += 0.37) out.phase.push([s, m, Roads.phase(s, "ew", m), Roads.phase(s, "ns", m)]);
document.getElementById("out").textContent = "@@" + JSON.stringify(out) + "@@";
} catch (e) { document.getElementById("out").textContent = "@@" + JSON.stringify({ error: String(e.stack || e) }) + "@@"; }
"""

# dressing.js (WLD-032) with the play.js globals it reads stubbed: every playground piece's still parts drawn, the play
# places put on their pieces (init), a child at every place near each playground at four moments (apply), and the moving
# seats and planks drawn after (draw); people walking through a slide or a climbing frame kept out (keepOut)
DRESS_STUBS = r"""
let REC = [];
const rec = (k, a, c) => REC.push([k, a, c || null]);
const world = {
  box(x0, y0, z0, x1, y1, z1, c, f, o) { rec(0, [x0, y0, z0, x1, y1, z1], o && o[3] ? o[3][0] : c); },
  ybox(cx, y0, cz, w, h, d, yaw, c) { rec(1, [cx, y0, cz, w, h, d, yaw], c); },
  flat(x0, z0, x1, z1, y, c) { rec(2, [x0, z0, x1, z1, y], c); },
  quad(p0, p1, p2, p3, c, f, hint) { rec(4, [...p0, ...p1, ...p2, ...p3, ...hint], c); },
  obox(m, cx, cy, cz, sx, sy, sz, c) { rec(9, [...m, cx, cy, cz, sx, sy, sz], c); },
};
function solid(x0, y0, z0, x1, y1, z1) { rec(8, [x0, y0, z0, x1, y1, z1], null); }
let G_HERE = .12; const groundBase = () => G_HERE; const Y_PAVE = .12, OLD_BASE = 3.55;   // (play.js)
const USES = [], use = (x, y, z, type, label, yaw, extra) => USES.push({ x, y, z, type, label, yaw, sp: extra && extra.sp }), DOING = {}, GOING = {}, signs = {}, LOTS = [], BLOBS = [], occl = [], Smash = { wrap: (k, x, z, fn) => fn() };
const SP = { at: (S, a, y, d) => S.ns ? [a, y, S.fix + S.out * d] : [S.fix + S.out * d, y, a], n: S => S.ns ? [0, 0, S.out] : [S.out, 0, 0] };
function lettering(m, ctr, n, h, cell) { REC.push([7, [ctr[0], ctr[1], ctr[2], n[0], n[2], h], null, null, cell.t]); }
function nameCell(t) { return { t, tw: Math.min(.99, (t.length * 24 + 8) / 512) }; }   // (the slice measures it on a canvas: estimated, as BrixStreets does)
const People = { SIG: { Easy: { name: "Easy" }, Careful: { name: "Careful" } }, stepLength: (sig, S) => 1.37 * S };
const looks = new Proxy({}, { get: () => ({ sig: People.SIG.Easy, S: 1 }) });
"""
DRESS_HARNESS = r"""
try {
const PLAYK = { playground: 1, swing: 1, slide: 1, seesaw: 1, sandpit: 1, climber: 1 }, OPEN = { garden: 1, market: 1, stop: 1 }, OLD = ["Old Town", "Centre"];
const out = { dress: {}, apply: [], draw: [], keep: [] }, parks = [];
for (const b of D.blocks) {
  for (const l of b.lots) { l.block = b; LOTS.push(l); l.old = OLD.includes(b.district); l.H = Math.max(l.floors * 3 + .4, 3.6); }   // (the Unreal town's: BrixBoardWallOf)
  b.props.forEach((p, i) => {
    if (p.set !== "dress" && p.kind !== "arch") return;
    const [x, z] = p.pos; G_HERE = b.lots.some(l => OPEN[l.kind] && x >= l.rect[0] && x <= l.rect[2] && z >= l.rect[1] && z <= l.rect[3]) ? .16 : .12;
    REC = []; if (p.kind === "arch") arch(p, b); else if (!Dress.prop(p, b)) return; out.dress[b.id + "/" + i] = REC;
    if (p.kind === "playground") parks.push(p);
  });
}
Dress.init();
out.read = [];   // readers at a notice board or the kiosk turn to it (apply(): within 6 m)
for (const b of D.blocks) for (const p of b.props) if (p.set === "dress" && (p.kind === "noticeboard" || p.kind === "kiosk"))
  [[1.2, .3], [-2, 1], [4, 3.9], [0, -5.5], [7, 0]].forEach(([dx, dz], q) => {
    const x = p.pos[0] + dx, z = p.pos[1] + dz, act = D.acts.indexOf(q % 2 ? "paper" : "notices");
    const r = Dress.apply(0, { x, z, act, walking: false, yaw: 99 }, 1); out.read.push([x, z, q % 2, r.yaw]); });
const U = ["swing", "slide", "seesaw", "sandpit", "climb"];
let n = 0;
const v = q => q === undefined ? null : q;
for (const park of parks) for (const t of [1.3, 4.2, 6.9, 11.5]) {
  for (const l of LOTS) for (const u of U) ((l.objects || {})[u] || []).forEach(([sx, sz], k) => {
    if (Math.hypot(sx - park.pos[0], sz - park.pos[1]) > 40) return;
    const i = n++ % 500, x = sx + .07, z = sz - .05, act = D.acts.indexOf(u);
    const r = Dress.apply(i, { x, z, act, walking: false }, t);
    out.apply.push([i, U.indexOf(u), x, z, t, r.x, r.z, v(r.y), v(r.yaw), v(r.pose), r.ph || 0, !!r.walking, r.sig ? r.sig.name : null,
      r.mod && r.mod.straddle ? 1 : 0, r.mod && r.mod.arms ? r.mod.arms : null]);
  });
  REC = []; Dress.draw(world, [park.pos[0], 0, park.pos[1]], t); out.draw.push([park.id, t, REC]);
}
for (const b of D.blocks) for (const p of b.props) if (p.set === "dress" && (p.kind === "slide" || p.kind === "climber"))
  for (const [dx, dz] of [[.3, .1], [-.9, .25], [1.6, -.2], [.8, .9], [-3, 0]]) {
    const r = Dress.apply(0, { x: p.pos[0] + dx, z: p.pos[1] + dz, walking: true }, 1); out.keep.push([p.pos[0] + dx, p.pos[1] + dz, r.x, r.z]); }
out.player = [];   // the player's go at each play place (WLD-041): the place, then where they are through a go, and where they step off
const pv = r => [r.x, r.z, r.y, r.yaw, r.pose, r.ph === undefined ? null : r.ph, !!r.done, r.sig ? r.sig.name : null, r.mod && r.mod.straddle ? 1 : 0, r.mod && r.mod.arms ? r.mod.arms : null];
for (const u of USES) if (u.type === "play") {
  const st = { sp: u.sp, t0: 10 };
  out.player.push([u.x, u.z, u.y, u.yaw, u.label, [.2, 1.1, 2.0, 2.9, 3.4, 4.1, 5.6, 9.3].map(e => pv(Dress.playerAt(st, 10 + e))), Dress.playerOff(st)]);
}
document.getElementById("out").textContent = "@@" + JSON.stringify(out) + "@@";
} catch (e) { document.getElementById("out").textContent = "@@" + JSON.stringify({ error: String(e.stack || e) }) + "@@"; }
"""

HARNESS = r"""
let REC = [];
const rec = (k, a, c) => REC.push([k, a, c || null]);
const world = {
  box(x0, y0, z0, x1, y1, z1, c, f, o) { rec(0, [x0, y0, z0, x1, y1, z1], o && o[3] ? o[3][0] : c); },
  ybox(cx, y0, cz, w, h, d, yaw, c) { rec(1, [cx, y0, cz, w, h, d, yaw], c); },
  flat(x0, z0, x1, z1, y, c) { rec(2, [x0, z0, x1, z1, y], c); },
  quad(p0, p1, p2, p3, c, f, hint) { rec(4, [...p0, ...p1, ...p2, ...p3, ...hint], c); },
};
function dome(x, y0, z, R, H, col, seed) { rec(3, [x, y0, z, R, H, seed], col); return y0 + H * .5; }
function ribbon(m, pts, w, y, c) { REC.push([5, [w, y], c, pts]); }
function disc(m, p, r, y, c) { rec(6, [p[0], p[1], r, y], c); }
function nameCell(t) { return { t }; }
function lettering(m, ctr, n, h, cell) { REC.push([7, [ctr[0], ctr[1], ctr[2], n[0], n[2], h], null, null, cell.t]); }
function solid(x0, y0, z0, x1, y1, z1) { rec(8, [x0, y0, z0, x1, y1, z1], null); }
const BLOBS = [], LAWNS = [], signs = {};
let G_HERE = .12, Y_WILD = -.03; const Y_GRASS = .15, P = D.pitch;
const groundBase = () => G_HERE;
try {
const out = { props: {}, lawns: {}, ground: {} };
const OPEN = { garden: 1, market: 1, stop: 1 };
for (const b of D.blocks) {
  const [ci, cj] = b.span || [1, 1]; b.ext = [b.size + (ci - 1) * P, b.size + (cj - 1) * P];
  if (GREEN_KINDS[b.template]) { REC = []; greenGround(b); out.ground[b.id] = REC; }
  for (const l of b.lots) if (l.kind === "garden") { const [x0, z0, x1, z1] = l.rect; REC = []; lawn(x0, z0, x1, z1, Y_GRASS + .02, mul3(C.grass, 1.08), b, true); out.lawns[l.id] = REC; }
  b.props.forEach((p, i) => {
    const [x, z] = p.pos, open = b.lots.some(l => OPEN[l.kind] && x >= l.rect[0] && x <= l.rect[2] && z >= l.rect[1] && z <= l.rect[3]);
    G_HERE = p.set === "wild" ? Y_WILD : open ? .16 : .12;
    const key = b.id + "/" + i;
    if (p.set === "wild") { REC = []; if (wildProp(p)) out.props[key] = REC; }
    if (p.kind === "plant") { REC = []; plant(p); out.props[key] = REC; }
  });
}
document.getElementById("out").textContent = "@@" + JSON.stringify(out) + "@@";
} catch (e) { document.getElementById("out").textContent = "@@" + JSON.stringify({ error: String(e.stack || e) }) + "@@"; }
"""


def cut(src, name):
    """A top-level function or constant of play.js, as it is."""
    m = re.search(r"^function " + name + r"\(.*?^\}\n", src, re.S | re.M) or re.search(r"^const " + name + r" = .*?;\n", src, re.M | re.S)
    if not m:
        m = re.search(r"^const (?:\w+ = [^\n]*?, )*?" + name + r" = ", src, re.M)   # one of several on a line: take the line
        if m:
            line = src[m.start():src.index("\n", m.start()) + 1]
            return line
        sys.exit("play.js: " + name + " not found; update streets_values.py")
    return m.group(0)


def checksum(rec):
    """The same sum as BrixWorld's BrixShapesChecksum(): each call's kind, its numbers by place, its colour, its points."""
    s = 0.0
    for r in rec:
        s += (r[0] + 1) * 1000.0
        for j, a in enumerate(r[1]):
            s += a * (1 + (j % 5) * .1)
        if r[2]:
            s += r[2][0] * 3 + r[2][1] * 5 + r[2][2] * 7
        if len(r) > 3 and r[3]:
            for p in r[3]:
                s += p[0] * .3 + p[1] * .7
    return s


def summary(table, every=20):
    out = {}
    for n, (k, rec) in enumerate(sorted(table.items())):
        e = dict(n=len(rec), sum=checksum(rec), kinds=[sum(1 for r in rec if r[0] == q) for q in range(9)])
        if n % every == 0:
            e["calls"] = rec[:300]   # (its first 300 calls: the rest are in the checksum)
        out[k] = e
    return out


def run_page(code):
    """The page's script run in headless Edge; what it wrote into <pre id=out>."""
    page = "<!doctype html><meta charset=utf-8><pre id=out></pre><script>\n" + code + "\n</script>"
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as tmp:
        path = os.path.join(tmp, "streets_values.html")
        open(path, "w", encoding="utf-8").write(page)
        dump = os.path.join(tmp, "dom.txt")
        args = ",".join("'%s'" % a for a in ("--headless=new", "--user-data-dir=" + os.path.join(tmp, "edge"), "--no-first-run",
                                              "--virtual-time-budget=20000", "--dump-dom", "file:///" + path.replace("\\", "/")))
        # Edge through PowerShell's Start-Process (since Edge 158 nothing comes back on a pipe; see people_values.py)
        subprocess.run(["powershell", "-NoProfile", "-Command",
                        "Start-Process -FilePath '%s' -ArgumentList %s -RedirectStandardOutput '%s' -NoNewWindow -Wait" % (EDGE, args, dump)],
                       stdin=subprocess.DEVNULL, capture_output=True, timeout=600)
        with open(dump, encoding="utf-8", errors="replace") as f:
            text = f.read()
    m = re.search(r"<pre id=\"out\">@@(.*?)@@</pre>", text, re.S)
    if not m:
        sys.exit("Edge gave no values:\n" + text[-2000:])
    out = json.loads(html.unescape(m.group(1)))
    if "error" in out:
        sys.exit("the slice's code failed in Edge: " + out["error"])
    return out


def main():
    src = open(PLAY_JS, encoding="utf-8").read()
    city = json.load(open(os.path.join(CITY, "city.json"), encoding="utf-8"))
    blocks = []
    for name in city["blocks"]:
        blocks.append(json.load(open(os.path.join(CITY, name["file"]), encoding="utf-8")))
    streets = json.load(open(STREETS, encoding="utf-8"))
    # play.js's palette: its C literal, one block of lines
    m = re.search(r"^const C = \{.*?^\};\n", src, re.S | re.M)
    if not m:
        sys.exit("play.js: the palette C not found; update streets_values.py")
    h = re.search(r"^function hash\(\) \{.*?^\}\n", src, re.S | re.M)
    pre = "const D = " + json.dumps(dict(blocks=blocks, pitch=city["grid"]["pitch"], plants=streets["plants"], lawn=streets["lawn"])) + ";\n"
    code = pre + h.group(0) + m.group(0) + "".join(cut(src, c) for c in CONSTS) + "".join(cut(src, f) for f in FUNCS)
    out = run_page(code + HARNESS)
    m4 = re.search(r"^const M4 = \{.*?^\};\n", src, re.S | re.M)
    if not m4:
        sys.exit("play.js: M4 not found; update streets_values.py")
    day = json.load(open(DAY, encoding="utf-8"))
    a0 = src.index('  } else if (p.kind === "arch") {'); a1 = src.index('  } else if (p.kind === "lamp") {', a0)
    arch_fn = "function arch(p, b) {\n  const [x, z] = p.pos, g = groundBase(x, z);\n  {" + src[a0 + len('  } else if (p.kind === "arch") {'):a1] + "  }\n}\n"
    pre2 = "const D = " + json.dumps(dict(blocks=blocks, acts=day["acts"], dressing=streets["dressing"], flowers=streets["flowers"])) + ";\n"
    dress = run_page(pre2 + h.group(0) + m.group(0) + "".join(cut(src, c) for c in ["clamp", "mix", "mix3", "mul3", "pick", "yawOf"]) + m4.group(0)
                     + cut(src, "PLASTER") + cut(src, "fbox") + cut(src, "crates") + cut(src, "cratesNow") + arch_fn + DRESS_STUBS + open(DRESS_JS, encoding="utf-8").read() + DRESS_HARNESS)
    wasted = dict(roads=streets["roads"], city=dict(grid=city["grid"]), day=dict(index=streets["day"]), pace=streets["pace"])
    pre3 = ("window.WASTED = " + json.dumps(wasted) + ";\nconst D = " + json.dumps(dict(blocks=blocks, pitch=city["grid"]["pitch"], trees=streets["trees"])) + ";\n"
            + "const LAWN = " + json.dumps(streets["lawn"]) + ";\n")
    roads = run_page(pre3 + h.group(0) + "".join(cut(src, c) for c in ["clamp", "mix", "mix3", "mul3", "GREEN_KINDS"]) + cut(src, "treeRing") + cut(src, "tree")
                     + open(ROADS_JS, encoding="utf-8").read() + TREES_HARNESS)   # (roads.js stop() finds the harness's groundBase())
    values = dict(note="Generated by Tools/unreal/streets_values.py: play.js plant(), lawn(), wildProp() and greenGround() for every plant, wild "
                       "thing, garden and block of open ground, for CITY017_every_plant_is_drawn_as_the_slice_draws_it and "
                       "WLD056_the_open_ground_is_drawn_as_the_slice_draws_it. Calls: [kind, numbers, colour, points, text]; kinds as FBrixShape.",
                  day=streets["day"], props=summary(out["props"]), lawns=summary(out["lawns"], 5), ground=summary(out["ground"], 3),
                  dress=summary(dress["dress"], 1), apply=dress["apply"], draw=dress["draw"], keep=dress["keep"], read=dress["read"], player=dress["player"],
                  trees=summary(roads["trees"], 25), roads=summary(roads["roads"], 1), lamps=roads["lamps"], phase=roads["phase"])
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        json.dump(values, f, separators=(",", ":"))
    print("wrote %s: %d things, %d lawns, %d blocks of open ground; %d pieces of street dressing, %d children, %d draws; %d trees, %d road squares, %d signal heads" % (
          os.path.relpath(OUT, ROOT), len(out["props"]), len(out["lawns"]), len(out["ground"]), len(dress["dress"]), len(dress["apply"]), len(dress["draw"]),
          len(roads["trees"]), len(roads["roads"]), len(roads["lamps"])))


if __name__ == "__main__":
    main()
