"""The furniture outlet as the slice builds it, for the Unreal build's tests (FURN-020, FURN-021; F-FURN-STORE).

Runs the slice's own outlet.js (Prototype/play/) in headless Edge, as play.js runs it for the lot of kind "furniture"
(World/city_01/blocks/b_14_8.json): Outlet.build() (the hall, its windows, sign, door, loading doors, and inside0(): the
route, the room sets' walls, tills, desks, the cafe counter, the racks and lanterns), then furnish()'s Outlet.inside() twice
as play.js does (at load: the places, uses and what is in the way; near the camera: the furniture itself) and the car park
prop's Outlet.carpark(). Each piece of furniture in a room set (kit 17's living rooms, outlet.js furnishSet(); kit 18's
rooms, furnishRoom()) is recorded as the piece it is, where it stands and which way (kind 13: x, y, z, its across axis x and
z, its back axis x and z; Text "piece|line"), for the Unreal build lays the furniture kits' own meshes there as it does in
homes (ADR-0050), where the slice draws boxes; a kit 17 set's stool, trunk, crate or pouffe ("extra": no mesh) stays boxes.
The town's mesh and play.js's helpers (sideOf, fbox, SP.at, SP.n, SP.win, SP.lamp, archHead,
lettering, Cars.parked) are play.js's own, cut from it, with the mesh swapped for a recorder: each call (quad, box, ybox,
obox, solid, use, spot, light, name, occl, a parked car) with its numbers, in order. Writes
  Tests/values/outlet_city_01.json   the calls in runs of 400 (how many and a checksum, every call in full for one run in three
                                     ), the lamps; FURN020_the_outlet_is_built_as_the_slice_builds_it checks BrixWorld's
                                     BrixOutlet against it.
Run it again after changing outlet.js or the outlet's block file.
  python Tools/unreal/outlet_values.py
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from mall_values import ROOT, PLAY, CITY, cut, run_page   # noqa: E402

OUT = os.path.join(ROOT, "Tests", "values", "outlet_city_01.json")
MODELS = os.path.join(ROOT, "Game", "Data", "car_models.json")
RUN = 400

HARNESS = r"""
let REC = [];
const rec = (k, a, c, f, x, t) => REC.push([k, a, c || null, f === undefined ? 0 : f, x || null, t || null]);
const mesh = {
  quad(p0, p1, p2, p3, c, f, hint, uv) { rec(0, [...p0, ...p1, ...p2, ...p3, ...(hint || [0, 0, 0]), hint ? 1 : 0, uv ? 1 : 0], c, f); },
  quadN(p, nr, cs, f) { rec(1, [...p[0], ...p[1], ...p[2], ...p[3], ...nr[0], ...nr[1], ...nr[2], ...nr[3], ...cs[0], ...cs[1], ...cs[2], ...cs[3]], null, f); },
  box(x0, y0, z0, x1, y1, z1, c, f, o) { rec(2, [x0, y0, z0, x1, y1, z1], c, f, o ? Object.keys(o).map(k => o[k] ? [+k, ...o[k][0], o[k][1]] : [+k]) : null); },
  ybox(cx, y0, cz, sx, sy, sz, yaw, c, f) { rec(3, [cx, y0, cz, sx, sy, sz, yaw], c, f); },
  flat(x0, z0, x1, z1, y, c, f) { this.quad([x0, y, z0], [x0, y, z1], [x1, y, z1], [x1, y, z0], c, f, [0, 1, 0]); },   // (Mesh.flat())
  obox(m, cx, cy, cz, sx, sy, sz, c, f) { rec(11, [...m, cx, cy, cz, sx, sy, sz], c, f); },
};
const world = mesh, signs = mesh;
const lights = { push(a) { rec(8, a); } }, occl = { push(a) { rec(10, a); } };
function solid(x0, y0, z0, x1, y1, z1) { rec(4, [x0, y0, z0, x1, y1, z1]); }
function useAt(x, y, z, kind, label, yaw) { rec(5, [x, y, z, yaw], null, 0, null, kind + "|" + label); }
function spotAt(l, x, z, pose, yaw, h) { rec(6, [x, z, pose, yaw, h === undefined ? -1 : h]); }
const detailOf = () => mesh, BLOBS = { push() { } }, QUAD_UV = true;
let CELLS = 0, LAST = "";
function nameCell(name) { LAST = name; return { i: CELLS++, tw: .5 }; }
function lettering(m, ctr, n, h, cell, f) { rec(9, [...ctr, ...n, h], null, 0, null, LAST); }
const Cars = { parked(m, x, z, yaw, seed, y) { const md = MODEL[(h => h < .4 ? "small" : h < .72 ? "estate" : "pickup")(hash(seed, "model"))]; rec(12, [x, z, yaw, y], null, 0, null, seed); return [md[0], md[1]]; } };
const RR = { lamps: { push(a) { rec(7, a); } } };
let PIECE = null, PAT = null;                              // a piece of furniture: where its frame's origin, across and back axes fall (kind 13)
const K17 = { rug: "k17:rug", sofa: "k17:sofa", chair: "k17:armchair", coffee: "k17:coffee", lamp: "k17:floor_lamp", case: "k17:bookcase", bed: "k17:bed", side: "k17:bedside", ward: "k17:wardrobe" };
const lineKey = L => Object.keys(Outlet.LINES).find(k => Outlet.LINES[k] === L) || "plain";
function piece(Q, kind, L) { PIECE = kind + "|" + lineKey(L); Q(0, 0, 0, 0, 0, 0, [0, 0, 0], 77); Q(1, 0, 0, 0, 0, 0, [0, 0, 0], 78); Q(0, 0, 1, 0, 0, 0, [0, 0, 0], 79); }
Furniture.draw = (kind, Q, L) => piece(Q, kind, L);
function furnish(l, side, accent, wall) {                  // play.js furnish() for the outlet: at load (vis null), then near the camera
  for (const vis of [null, mesh]) {
    const F = .2, KEEP = () => false, rr = vis && RR, dm = vis || { ybox() { } };
    const spot = vis ? () => { } : spotAt, use = vis ? () => { } : useAt;
    const hard = (ax0, ay0, az0, ax1, ay1, az1) => { if (ay0 < F + 2 && KEEP()) return; return vis || solid(Math.min(ax0, ax1), ay0, Math.min(az0, az1), Math.max(ax0, ax1), ay1, Math.max(az0, az1)); };
    const B = (cx, y0, cz, sx, sy, sz, c, yaw, f) => {
      if (f >= 77 && f <= 79) { if (!vis) return; if (f === 77) PAT = [cx, F + y0, cz]; else if (f === 78) PAT.push(cx - PAT[0], cz - PAT[2]); else rec(13, [...PAT, cx - PAT[0], cz - PAT[2]], null, 0, null, PIECE); return; }
      if (rr && f === 2) rr.lamps.push([cx, F + y0 + sy / 2, cz, Math.min(.5, .9 * Math.max(sx, sy, sz))]);
      dm.ybox(cx, F + y0, cz, sx, sy, sz, yaw || 0, c, f || 19); };
    Outlet.inside(l, B, hard, spot, use, rr, dm);
  }
}
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
  const l = LOT, side = SIDE;
  l.block = BLOCK.id;
  Outlet.build(l, side);
  for (const p of BLOCK.props || []) if (p.kind === "carpark") Outlet.carpark(p, BLOCK);
  const runs = [];
  for (let i = 0; i < REC.length; i += RUN) {
    const part = REC.slice(i, i + RUN), e = { n: part.length, sum: part.reduce((s, r) => s + sum(r), 0) };
    if ((i / RUN) % 3 === 0) e.calls = part;
    runs.push(e);
  }
  const kinds = Array.from({ length: 14 }, (_, k) => REC.filter(r => r[0] === k).length);
  document.getElementById("out").textContent = "@@" + JSON.stringify({ calls: REC.length, kinds, runs, texts: REC.filter(r => r[5]).map(r => r[5]) }) + "@@";
} catch (e) { document.getElementById("out").textContent = "@@" + JSON.stringify({ error: String(e.stack || e) }) + "@@"; }
"""


def main():
    src = open(os.path.join(PLAY, "play.js"), encoding="utf-8").read()
    city = json.load(open(os.path.join(CITY, "city.json"), encoding="utf-8"))
    lot = blk = None
    for b in city["blocks"]:
        bk = json.load(open(os.path.join(CITY, b["file"]), encoding="utf-8"))
        for l in bk["lots"]:
            if l.get("kind") == "furniture":
                lot, blk = l, bk
    if not lot:
        sys.exit("no furniture outlet in the block files")
    models = json.load(open(MODELS, encoding="utf-8"))["models"]
    dims = {k: [m["box"][1] - m["box"][0], m["box"][3] - m["box"][2]] for k, m in models.items()}
    # the door's side, as play.js building() finds it
    x0, z0, x1, z1 = lot["rect"]
    dx, dz = lot["door"]
    side = "n" if abs(dz - z0) < .01 else "s" if abs(dz - z1) < .01 else "w" if abs(dx - x0) < .01 else "e"
    pre = ("const RUN = %d;\nconst LOT = " % RUN) + json.dumps(lot) + ";\nconst BLOCK = " + json.dumps(blk) + ";\nconst SIDE = " + json.dumps(side) + ";\nconst MODEL = " + json.dumps(dims) + ";\n"
    pre += cut(src, r"^const Y_PAVE = .*?;\n", "Y_FLOOR") + cut(src, r"^const mix = .*?;\n", "mix") + cut(src, r"^const mix3 = .*?;\n", "mix3") + cut(src, r"^const mul3 = .*?;\n", "mul3")
    pre += cut(src, r"^function hash\(\) \{.*?^\}\n", "hash") + cut(src, r"^const pick = .*?;\n", "pick") + cut(src, r"^const M4 = \{.*?^\};\n", "M4")
    pre += cut(src, r"^const C = \{.*?^\};\n", "the palette C") + cut(src, r"^const DOOR_DIR = .*?;\n", "DOOR_DIR") + cut(src, r"^const yawOf = .*?;\n", "yawOf")
    pre += cut(src, r"^function sideOf\(l, s\) \{.*?^\}\n", "sideOf") + cut(src, r"^function fbox\(.*?^\}\n", "fbox") + cut(src, r"^function archHead\(.*?^\}\n", "archHead")
    sp = cut(src, r"^const SP = \{.*?^\};\n", "SP")
    pre += sp
    outlet = open(os.path.join(PLAY, "outlet.js"), encoding="utf-8").read()
    draw = "({ rug, sofa: q => sofa(q, L, 2.0), chair: q => sofa(q, L, .95), coffee, lamp: floorLamp, case: q => bookcase(q, L, seed), bed, side: bedside, ward: wardrobe, extra })[kind](Q, L);"
    if draw not in outlet:
        sys.exit("outlet.js furnishSet() no longer draws its pieces as it did; update outlet_values.py")
    outlet = outlet.replace(draw, "if (K17[kind]) piece(Q, K17[kind], L); else " + draw)   # the kit's own pieces (ADR-0050), the extra as boxes
    code = pre + open(os.path.join(PLAY, "furniture.js"), encoding="utf-8").read() + "\n" + outlet + "\n"
    out = run_page(code + HARNESS)
    values = dict(note="Generated by Tools/unreal/outlet_values.py: outlet.js building the furniture outlet as play.js does (build(), furnish()'s "
                       "inside() at load and near the camera, the car park), for FURN020_the_outlet_is_built_as_the_slice_builds_it. Calls: [kind, "
                       "numbers, colour, material, faces, text]; kinds as BrixMall's (0 quad, 2 box, 3 ybox, 4 solid, 5 use, 6 spot, 7 lamp, 8 light, "
                       "9 name, 10 occl, 11 obox, 12 a parked car, 13 a piece of furniture).",
                  lot=lot["id"], side=side, run=RUN, **out)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        json.dump(values, f, separators=(",", ":"))
    print("wrote %s: %s (door on %s), %d calls in %d runs (%s)" % (os.path.relpath(OUT, ROOT), lot["id"], side, out["calls"], len(out["runs"]), out["kinds"]))


if __name__ == "__main__":
    main()
