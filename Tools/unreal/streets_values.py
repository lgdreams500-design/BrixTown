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
    page = "<!doctype html><meta charset=utf-8><pre id=out></pre><script>\n" + code + HARNESS + "\n</script>"
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
        sys.exit("play.js failed in Edge: " + out["error"])
    values = dict(note="Generated by Tools/unreal/streets_values.py: play.js plant(), lawn(), wildProp() and greenGround() for every plant, wild "
                       "thing, garden and block of open ground, for CITY017_every_plant_is_drawn_as_the_slice_draws_it and "
                       "WLD056_the_open_ground_is_drawn_as_the_slice_draws_it. Calls: [kind, numbers, colour, points, text]; kinds as FBrixShape.",
                  day=streets["day"], props=summary(out["props"]), lawns=summary(out["lawns"], 5), ground=summary(out["ground"], 3))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        json.dump(values, f, separators=(",", ":"))
    print("wrote %s: %d things, %d lawns, %d blocks of open ground" % (os.path.relpath(OUT, ROOT), len(out["props"]), len(out["lawns"]), len(out["ground"])))


if __name__ == "__main__":
    main()
