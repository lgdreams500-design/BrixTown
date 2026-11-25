"""Eastgate Mall's inside as the slice draws it, for the Unreal build's tests (WLD-058 to WLD-068, CITY-105, CITY-106).

Runs the slice's own court.js, shell.js and outlets.js (Prototype/play/) in headless Edge (this machine has no Node), as
play.js mallShell() runs them for the one mall whose inside is built (Court.can(): the lot with a plan of concept "court"
and its entrances, World/city_01/blocks/b_16_5.json), with the town's mesh and the lot's hooks swapped for a recorder: each
call they make (quad, quadN, box, ybox, solid, use, spot, light, name, occl) with its numbers, in order. Writes
  Tests/values/mall_city_01.json   the calls in runs of 400: how many and a checksum of all their numbers, every call in
                                   full for one run in twenty; the outlets' rooms (Outlets.at), the lamps (Court.light()),
                                   and where the moving stairs carry someone (Court.carry()) at a few points.
WLD058_eastgate_is_built_as_the_slice_builds_it checks BrixWorld's BrixMall against it. The goods for the home an outlet
sells (goods.js, FURN-065) are left out here, as the slice leaves them out without its life data: the windows show their
stand-ins. Run it again after changing court.js, shell.js, outlets.js or the mall's block file.
  python Tools/unreal/mall_values.py
"""
import html
import json
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
PLAY = os.path.join(ROOT, "Prototype", "play")
CITY = os.path.join(ROOT, "World", "city_01")
OUT = os.path.join(ROOT, "Tests", "values", "mall_city_01.json")
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
RUN = 400

HARNESS = r"""
let REC = [];
const rec = (k, a, c, f, x, t) => REC.push([k, a, c || null, f === undefined ? 0 : f, x || null, t || null]);
const mesh = {
  quad(p0, p1, p2, p3, c, f, hint, uv) { rec(0, [...p0, ...p1, ...p2, ...p3, ...(hint || [0, 0, 0]), hint ? 1 : 0, uv ? 1 : 0], c, f); },
  quadN(p, nr, cs, f) { rec(1, [...p[0], ...p[1], ...p[2], ...p[3], ...nr[0], ...nr[1], ...nr[2], ...nr[3], ...cs[0], ...cs[1], ...cs[2], ...cs[3]], null, f); },
  box(x0, y0, z0, x1, y1, z1, c, f, o) { rec(2, [x0, y0, z0, x1, y1, z1], c, f, o ? Object.keys(o).map(k => o[k] ? [+k, ...o[k][0], o[k][1]] : [+k]) : null); },
  ybox(cx, y0, cz, sx, sy, sz, yaw, c, f) { rec(3, [cx, y0, cz, sx, sy, sz, yaw], c, f); },
};
const lights = { push(a) { rec(8, a); } }, occl = { push(a) { rec(10, a); } };
function solid(x0, y0, z0, x1, y1, z1) { rec(4, [x0, y0, z0, x1, y1, z1]); }
function use(x, y, z, kind, label, yaw, extra) { rec(5, [x, y, z, yaw, ...((extra && extra.to) || [])], null, 0, null, kind + "|" + label); }
function sum(r) {   // the same sum as BrixWorld's BrixMallChecksum()
  let s = (r[0] + 1) * 1000;
  r[1].forEach((a, j) => { s += a * (1 + (j % 5) * .1); });
  if (r[2]) s += r[2][0] * 3 + r[2][1] * 5 + r[2][2] * 7;
  s += r[3] * 11;
  if (r[4]) for (const q of r[4]) { s += q[0] * 13; if (q.length > 1) s += q[1] * 3 + q[2] * 5 + q[3] * 7 + q[4] * 11; else s += 17; }
  if (r[5]) s += r[5].length * 19;
  return s;
}
try {
  const l = LOT, spots = [];
  l.H = (l.plan.storey_height || 6) * (l.floors || 3);
  if (!Court.can(l) || !Shell.can(l.shell)) throw new Error("the lot has no court inside or no shell");
  const o = { mesh, solid, lights, occl, C, tones: SHELL_TONES, accent: ACCENT, hash, F: Y_FLOOR, wall: pick(PLASTER, hash(l.id, "mall")), inside: [.74, .7, .62], floor: [.74, .71, .64],
    spot: (x, z, pose, yaw, h) => rec(6, [x, z, pose, yaw, h === undefined ? -1 : h]), use, name: (text, ctr, n, h) => rec(9, [...ctr, ...n, h], null, 0, null, text) };
  const light = Court.light(l);
  Court.hall(o, l); Shell.draw(o, l); Court.draw(o, l);
  const runs = [];
  for (let i = 0; i < REC.length; i += RUN) {
    const part = REC.slice(i, i + RUN), e = { n: part.length, sum: part.reduce((s, r) => s + sum(r), 0), kinds: Array.from({ length: 11 }, (_, k) => part.filter(r => r[0] === k).length) };
    if ((i / RUN) % 20 === 0) e.calls = part;
    runs.push(e);
  }
  const kinds = Array.from({ length: 11 }, (_, k) => REC.filter(r => r[0] === k).length);
  const texts = REC.filter(r => r[5]).map(r => r[5]);
  const carry = [];   // someone on the moving stairs, and off them
  for (const r of l.plan.ramps) for (const f of [-.1, .05, .3, .6, .95, 1.1]) for (const side of [-1.2, -.5, .5, 1.2]) {
    const ya = r.from ? r.from * l.plan.storey_height : Y_FLOOR, yb = r.to * l.plan.storey_height, dx = r.b[0] - r.a[0], dz = r.b[1] - r.a[1], L = Math.hypot(dx, dz);
    for (const dy of [0, .5, 1.2]) { const p = { x: r.a[0] + dx * f - dz / L * side, y: ya + (yb - ya) * f + dy, z: r.a[1] + dz * f + dx / L * side }, q = [p.x, p.y, p.z];
      const on = Court.carry(p, .25); carry.push([...q, on ? 1 : 0, p.x, p.z]); }
  }
  const rooms = (l.outletRooms || []).map(r => [r.id, r.name, r.kind, r.level, r.fy, r.x, r.z, ...r.t, ...r.n, r.a0, r.a1, r.D]);
  document.getElementById("out").textContent = "@@" + JSON.stringify({ calls: REC.length, kinds, runs, texts, rooms, carry,
    lamps: light.lamps, light: { k: light.k, v1: light.v1, v2: light.v2, eyes: light.eyes, eye: light.eye, halls: light.halls, rect: light.rect } }) + "@@";
} catch (e) { document.getElementById("out").textContent = "@@" + JSON.stringify({ error: String(e.stack || e) }) + "@@"; }
"""


def cut(src, pattern, what):
    m = re.search(pattern, src, re.S | re.M)
    if not m:
        sys.exit("play.js: " + what + " not found; update mall_values.py")
    return m.group(0)


def run_page(code):
    """The page's script run in headless Edge; what it wrote into <pre id=out>."""
    page = "<!doctype html><meta charset=utf-8><pre id=out></pre><script>\n" + code + "\n</script>"
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as tmp:
        path = os.path.join(tmp, "mall_values.html")
        open(path, "w", encoding="utf-8").write(page)
        dump = os.path.join(tmp, "dom.txt")
        args = ",".join("'%s'" % a for a in ("--headless=new", "--user-data-dir=" + os.path.join(tmp, "edge"), "--no-first-run",
                                              "--virtual-time-budget=60000", "--dump-dom", "file:///" + path.replace("\\", "/")))
        # Edge through PowerShell's Start-Process (since Edge 158 nothing comes back on a pipe; see people_values.py)
        subprocess.run(["powershell", "-NoProfile", "-Command",
                        "Start-Process -FilePath '%s' -ArgumentList %s -RedirectStandardOutput '%s' -NoNewWindow -Wait" % (EDGE, args, dump)],
                       stdin=subprocess.DEVNULL, capture_output=True, timeout=900)
        with open(dump, encoding="utf-8", errors="replace") as f:
            text = f.read()
    m = re.search(r"<pre id=\"out\">@@(.*?)@@</pre>", text, re.S)
    if not m:
        sys.exit("Edge gave no values:\n" + text[-2000:])
    out = json.loads(html.unescape(m.group(1)))
    if "error" in out:
        sys.exit("the slice's code failed in Edge: " + out["error"])
    return out


def mall_lot():
    city = json.load(open(os.path.join(CITY, "city.json"), encoding="utf-8"))
    for b in city["blocks"]:
        blk = json.load(open(os.path.join(CITY, b["file"]), encoding="utf-8"))
        for l in blk["lots"]:
            p = l.get("plan") or {}
            if p.get("concept") == "court" and p.get("entrances") and l.get("shell"):
                return l
    sys.exit("no mall with its inside built (a plan of concept court) in the block files")


def main():
    src = open(os.path.join(PLAY, "play.js"), encoding="utf-8").read()
    lot = mall_lot()
    pre = ("const RUN = %d;\nconst LOT = " % RUN) + json.dumps(lot) + ";\n"
    pre += cut(src, r"^const Y_PAVE = .*?;\n", "Y_FLOOR") + cut(src, r"^function hash\(\) \{.*?^\}\n", "hash") + cut(src, r"^const pick = .*?;\n", "pick")
    pre += cut(src, r"^const C = \{.*?^\};\n", "the palette C") + cut(src, r"^const PLASTER = .*?;\n", "PLASTER") + cut(src, r"^const ACCENT = .*?;\n", "ACCENT")
    code = pre + "".join(open(os.path.join(PLAY, f), encoding="utf-8").read() + "\n" for f in ("shell.js", "outlets.js", "court.js"))
    code += cut(src, r"^const SHELL_TONES = .*?;", "SHELL_TONES") + "\n"
    out = run_page(code + HARNESS)
    values = dict(note="Generated by Tools/unreal/mall_values.py: court.js, shell.js and outlets.js drawing Eastgate Mall's inside as play.js "
                       "mallShell() does, for WLD058_eastgate_is_built_as_the_slice_builds_it. Calls: [kind, numbers, colour, material, "
                       "faces, text]; kinds 0 quad, 1 quadN, 2 box, 3 ybox, 4 solid, 5 use, 6 spot, 8 light, 9 name, 10 occl.",
                  lot=lot["id"], run=RUN, **out)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        json.dump(values, f, separators=(",", ":"))
    print("wrote %s: %s, %d calls in %d runs (%s), %d outlet rooms, %d lamps" % (
          os.path.relpath(OUT, ROOT), lot["id"], out["calls"], len(out["runs"]), out["kinds"], len(out["rooms"]), len(out["lamps"])))


if __name__ == "__main__":
    main()
