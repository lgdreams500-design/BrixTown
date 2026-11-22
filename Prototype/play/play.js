/* Wasted: playable slice. Builds the whole city from the block files bundled in city_data.js (see
   Tools/worldgen/build_play.py) and plays one day of everyone's routine. No libraries: plain WebGL.
   The look follows docs/art/ART_DIRECTION.md (rules W1-W8 are named where they are applied); people are in people.js.
   Layout of this file: helpers, mesh, world building, residents, shaders and light, player, camera, HUD, frame loop. */
"use strict";
const D = window.WASTED;
const G = D.city.grid, P = G.pitch, SW = G.street_width, CITY = G.columns * P;
const Y_PAVE = 0.12, Y_YARD = 0.16, Y_GRASS = 0.15, Y_PATH = 0.18, Y_FLOOR = 0.2, GF = 3.2;
const OLD_BASE = 3.55;                                      // the old quarters' stone storey: one floor, plaster above (building_kit_07: 3.4 m)
const PAVE = 4.5;                                        // pavement width round every block
const TRAM_Z = 5 * P - 1.8;                              // the tram runs along Tram Street

// ---------------------------------------------------------------- helpers
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const mix = (a, b, t) => a + (b - a) * t;
const mix3 = (a, b, t) => [mix(a[0], b[0], t), mix(a[1], b[1], t), mix(a[2], b[2], t)];
const smooth = (a, b, v) => { const t = clamp((v - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); };
const mul3 = (a, k) => [a[0] * k, a[1] * k, a[2] * k];
function hash() {                                        // the same inputs always give the same number in 0..1
  let h = 2166136261;
  for (const a of arguments) { const s = String(a); for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } h ^= 124; h = Math.imul(h, 16777619); }
  h ^= h >>> 15; h = Math.imul(h, 2246822519); h ^= h >>> 13; h = Math.imul(h, 3266489917); h ^= h >>> 16;
  return (h >>> 0) / 4294967296;
}
const pick = (list, r) => list[Math.min(list.length - 1, Math.floor(r * list.length))];
const M4 = {
  mul(a, b) { const o = new Array(16); for (let c = 0; c < 4; c++) for (let r = 0; r < 4; r++) o[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] + a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3]; return o; },
  t: (x, y, z) => [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1],
  ry(a) { const c = Math.cos(a), s = Math.sin(a); return [c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1]; },
  rx(a) { const c = Math.cos(a), s = Math.sin(a); return [1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1]; },
  rz(a) { const c = Math.cos(a), s = Math.sin(a); return [c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]; },
  persp(fov, asp, n, f) { const t = 1 / Math.tan(fov / 2); return [t / asp, 0, 0, 0, 0, t, 0, 0, 0, 0, (f + n) / (n - f), -1, 0, 0, 2 * f * n / (n - f), 0]; },
  look(e, f) {                                           // camera at e looking along unit vector f
    let r = [-f[2], 0, f[0]]; const n = Math.hypot(r[0], r[2]) || 1; r = [r[0] / n, 0, r[2] / n];
    const u = [r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]];
    return { m: [r[0], u[0], -f[0], 0, r[1], u[1], -f[1], 0, r[2], u[2], -f[2], 0, -(r[0] * e[0] + r[1] * e[1] + r[2] * e[2]), -(u[0] * e[0] + u[1] * e[1] + u[2] * e[2]), f[0] * e[0] + f[1] * e[1] + f[2] * e[2], 1], r, u };
  },
  pt: (m, x, y, z) => [m[0] * x + m[4] * y + m[8] * z + m[12], m[1] * x + m[5] * y + m[9] * z + m[13], m[2] * x + m[6] * y + m[10] * z + m[14]],
};

// ---------------------------------------------------------------- palette (linear RGB, docs/art/ART_DIRECTION.md §2.3)
const C = {
  stone: [.52, .48, .43], stoneDark: [.37, .34, .32], paving: [.46, .42, .37], pave: [.46, .42, .37], path: [.48, .44, .38], road: [.36, .34, .34], grass: [.3, .35, .2],
  field: [.29, .32, .2], water: [.2, .32, .38], brown: [.24, .16, .11], olive: [.30, .33, .20], plum: [.30, .24, .34], charcoal: [.09, .09, .11],
  indigo: [.10, .14, .24], cream: [.66, .61, .50], ochre: [.52, .40, .20], terracotta: [.44, .24, .18], teal: [.20, .36, .37], blue: [.30, .38, .52],
  lantern: [1, .55, .22], metal: [.17, .18, .21], soil: [.2, .15, .11], wood: [.34, .23, .15],
};
const PLASTER = [[.68, .63, .55], [.70, .58, .46], [.64, .6, .53], [.7, .6, .5], [.66, .56, .46], [.68, .63, .55]];
const ROOF = [[.44, .24, .18], [.38, .21, .16], [.22, .22, .27], [.32, .21, .15], [.47, .28, .2]];
const ACCENT = [C.ochre, C.terracotta, C.teal, C.blue, C.olive, C.plum];
const FLOOR = { home: [.36, .26, .18], shop: [.5, .46, .4], food: [.42, .3, .22], work: [.38, .36, .36], health: [.6, .6, .57], utility: [.3, .3, .31], bank: [.62, .6, .55], dealer: [.6, .6, .58], library: [.38, .26, .17], museum: [.6, .58, .54], cinema: [.3, .14, .12] };
const SKIN = [[.55, .38, .28], [.42, .27, .18], [.30, .19, .13], [.62, .46, .36], [.24, .15, .10], [.5, .33, .22]];
const HAIR = [[.05, .04, .04], [.16, .1, .06], [.09, .07, .06], [.3, .2, .1], [.22, .12, .08]];
const CLOTH = [C.brown, C.olive, C.plum, C.charcoal, C.indigo, [.25, .3, .4], [.33, .29, .25]];
const KIND_CSS = { home: "#b59a7c", shop: "#c79a45", food: "#b0614a", work: "#6f86ab", health: "#d9d3c2", utility: "#4b4b55", garden: "#6f8650", market: "#d8c9a3", stop: "#6fa3a2", bank: "#8c7a3c", dealer: "#5a7f8f", library: "#7a5c8f", museum: "#a08a6a", cinema: "#8f3c4a" };
const KIND_WORD = { home: "Homes", shop: "Shops", food: "Cafés", work: "Workplaces", health: "Clinics and hospital", utility: "Utilities", garden: "Gardens and parks", market: "Market Square", stop: "Tram stops", bank: "Bank", dealer: "Car showroom", library: "Library", museum: "Museum", cinema: "Cinema" };

// ---------------------------------------------------------------- mesh: position 3, normal 3, colour 3, material 1
// materials: 0 plaster (mottled) · 1 lit window · 2 lamp · 3 indoor surface · 4 dark window · 6 stone blocks · 7 paving · 8 light pool · 9 plain · 10 person indoors
//            11 barrel roof tiles · 12 louvred shutter (both shade their own relief, after Tools/blender/building_kit_07.py) · 14 person outdoors
//            20 road setts · 21 cobbled footpath (7 paving also draws the kerb along each pavement's edge)
//            15 floorboards · 16 floor tiles · 17 panelled room wall · 18 beamed ceiling (rooms: warm room light, little haze)
class Mesh {
  constructor(n) { this.a = new Float32Array(n || 1 << 16); this.n = 0; }
  reset() { this.n = 0; }
  quad(p0, p1, p2, p3, c, f, hint, uv) {
    let ax = p1[0] - p0[0], ay = p1[1] - p0[1], az = p1[2] - p0[2], bx = p2[0] - p0[0], by = p2[1] - p0[1], bz = p2[2] - p0[2];
    let nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
    const l = Math.hypot(nx, ny, nz) || 1; nx /= l; ny /= l; nz /= l;
    let sw = false; if (hint && nx * hint[0] + ny * hint[1] + nz * hint[2] < 0) { const t = p1; p1 = p3; p3 = t; nx = -nx; ny = -ny; nz = -nz; sw = true; }
    if (this.n + 60 > this.a.length) { const b = new Float32Array(this.a.length * 2); b.set(this.a); this.a = b; }
    const a = this.a;
    const ps = [p0, p1, p2, p0, p2, p3], ui = sw ? QUV1 : QUV0;
    for (let k = 0; k < 6; k++) { const p = ps[k], i = this.n; a[i] = p[0]; a[i + 1] = p[1]; a[i + 2] = p[2]; a[i + 3] = nx; a[i + 4] = ny; a[i + 5] = nz; if (uv) { a[i + 6] = ui[k] === 1 || ui[k] === 2 ? 1 : 0; a[i + 7] = ui[k] > 1 ? 1 : 0; } else { a[i + 6] = c[0]; a[i + 7] = c[1]; } a[i + 8] = c[2]; a[i + 9] = f; this.n = i + 10; }
  }
  // faces: 0 -x, 1 +x, 2 -y, 3 +y, 4 -z, 5 +z. `o` maps a face to [colour, material] or to null to leave it out. The bottom is left out unless asked for.
  box(x0, y0, z0, x1, y1, z1, c, f, o) {
    const X = [x0, x1], Y = [y0, y1], Z = [z0, z1];
    for (let k = 0; k < 6; k++) {
      let cc = c, ff = f;
      if (o && k in o) { if (!o[k]) continue; cc = o[k][0]; ff = o[k][1]; } else if (k === 2) continue;
      const F = FACES[k];
      this.quad([X[F[0][0]], Y[F[0][1]], Z[F[0][2]]], [X[F[1][0]], Y[F[1][1]], Z[F[1][2]]], [X[F[2][0]], Y[F[2][1]], Z[F[2][2]]], [X[F[3][0]], Y[F[3][1]], Z[F[3][2]]], cc, ff, F[4]);
    }
  }
  obox(m, cx, cy, cz, sx, sy, sz, c, f) {                // a box of size s centred at c, placed by matrix m
    const hx = sx / 2, hy = sy / 2, hz = sz / 2, q = [];
    for (let k = 0; k < 8; k++) q.push(M4.pt(m, cx + (k & 1 ? hx : -hx), cy + (k & 2 ? hy : -hy), cz + (k & 4 ? hz : -hz)));
    const o = M4.pt(m, cx, cy, cz);
    for (const [a, b, c2, d] of [[0, 4, 6, 2], [1, 3, 7, 5], [0, 1, 5, 4], [2, 6, 7, 3], [0, 2, 3, 1], [4, 5, 7, 6]]) {
      const mid = [(q[a][0] + q[c2][0]) / 2 - o[0], (q[a][1] + q[c2][1]) / 2 - o[1], (q[a][2] + q[c2][2]) / 2 - o[2]];
      this.quad(q[a], q[b], q[c2], q[d], c, f, mid);
    }
  }
  ybox(cx, y0, cz, sx, sy, sz, yaw, c, f) { this.obox(M4.mul(M4.t(cx, y0, cz), M4.ry(yaw)), 0, sy / 2, 0, sx, sy, sz, c, f); }
  flat(x0, z0, x1, z1, y, c, f) { this.quad([x0, y, z0], [x0, y, z1], [x1, y, z1], [x1, y, z0], c, f, [0, 1, 0]); }
}
const QUV0 = [0, 1, 2, 0, 2, 3], QUV1 = [0, 3, 2, 0, 2, 1];
const FACES = [
  [[0, 0, 0], [0, 0, 1], [0, 1, 1], [0, 1, 0], [-1, 0, 0]], [[1, 0, 0], [1, 1, 0], [1, 1, 1], [1, 0, 1], [1, 0, 0]], [[0, 0, 0], [1, 0, 0], [1, 0, 1], [0, 0, 1], [0, -1, 0]],
  [[0, 1, 0], [0, 1, 1], [1, 1, 1], [1, 1, 0], [0, 1, 0]], [[0, 0, 0], [0, 1, 0], [1, 1, 0], [1, 0, 0], [0, 0, -1]], [[0, 0, 1], [1, 0, 1], [1, 1, 1], [0, 1, 1], [0, 0, 1]]];

// ---------------------------------------------------------------- things you can bump into and things you can use
const solids = [], cells = new Map(), CELL = 8, uses = [], labels = [], lights = [], occl = [];
function solid(x0, y0, z0, x1, y1, z1) {
  const i = solids.length; solids.push([x0, y0, z0, x1, y1, z1]);
  for (let a = Math.floor(x0 / CELL); a <= Math.floor(x1 / CELL); a++) for (let b = Math.floor(z0 / CELL); b <= Math.floor(z1 / CELL); b++) {
    const k = (a + 512) * 4096 + b + 512; let l = cells.get(k); if (!l) cells.set(k, l = []); l.push(i);
  }
}
function eachSolid(x, z, r, fn) {
  for (let a = Math.floor((x - r) / CELL); a <= Math.floor((x + r) / CELL); a++) for (let b = Math.floor((z - r) / CELL); b <= Math.floor((z + r) / CELL); b++) {
    const l = cells.get((a + 512) * 4096 + b + 512); if (l) for (const i of l) fn(solids[i]);
  }
}
const blockAt = new Map(), LOTS = [];
for (const b of D.blocks) { blockAt.set(b.grid[0] + "," + b.grid[1], b); for (const l of b.lots) { l.block = b; l.spots = []; LOTS.push(l); } }
const isOpen = l => l.kind === "garden" || l.kind === "market" || l.kind === "stop";
function blockOf(x, z) { return blockAt.get(Math.floor(x / P) + "," + Math.floor(z / P)); }
function lotAt(x, z, any) {
  const b = blockOf(x, z); if (!b) return null;
  for (const l of b.lots) if ((any || !isOpen(l)) && x >= l.rect[0] && x <= l.rect[2] && z >= l.rect[1] && z <= l.rect[3]) return l;
  return null;
}
function groundBase(x, z) {                              // height of the ground itself: street, pavement, yard or a building's floor
  if (x < -SW / 2 || z < -SW / 2 || x > CITY + SW / 2 || z > CITY + SW / 2) return 0;
  const u = x - Math.floor(x / P) * P, v = z - Math.floor(z / P) * P, lo = SW / 2 - PAVE, hi = P - SW / 2 + PAVE;
  if (u < lo || u > hi || v < lo || v > hi || x < 0 || z < 0 || x > CITY || z > CITY) return 0;
  if (u < SW / 2 || u > P - SW / 2 || v < SW / 2 || v > P - SW / 2) return Y_PAVE;
  const l = lotAt(x, z); return l && l.enterable ? Y_FLOOR : Y_YARD;
}
const spot = (lot, x, z, pose, yaw, h) => lot.spots.push({ x, z, pose, yaw, h });       // how a resident holds themselves at a place
const use = (x, y, z, type, label, yaw, extra) => uses.push(Object.assign({ x, y, z, type, label, yaw: yaw || 0 }, extra));
const spotAt = (...a) => spot(...a), useAt = (...a) => use(...a);   // furnish() shadows these with no-ops when it only draws
const NULL_MESH = { box() { }, quad() { }, obox() { }, ybox() { }, flat() { } };   // geometry that goes nowhere (the load-time pass of a room)
let ROOM_REC = null;                                     // while a room near the camera is furnished: its lamps and what stands on its floor (nearRooms)

// ---------------------------------------------------------------- building the city from the data
const world = new Mesh(1 << 22), signs = new Mesh(1 << 14);    // painted names are kept apart: they are the only faces that cut holes (discard)
const RANGES = [];                                         // world in pieces: [first float, end float, whole-town?], so each pass draws only the pieces it can see
const DOOR_DIR = { n: [0, -1], s: [0, 1], w: [-1, 0], e: [1, 0] };
const yawOf = (dx, dz) => Math.atan2(dx, -dz);
// Small relief (sills, lintels, shutters, arch stones, corbels, quoins) goes into one mesh per block, drawn only near the camera.
const DETAIL = new Map(), DETAIL_R = 115;
function detailOf(b) { let m = DETAIL.get(b.id); if (!m) { m = new Mesh(1 << 17); m.cx = b.origin[0] + b.size / 2; m.cz = b.origin[1] + b.size / 2; DETAIL.set(b.id, m); } return m; }
function sideOf(l, s) {                                   // a wall of a lot: is it along x, where its face is, which way is out, its span
  const [x0, z0, x1, z1] = l.rect, ns = s === "n" || s === "s";
  return { ns, fix: s === "n" ? z0 : s === "s" ? z1 : s === "w" ? x0 : x1, out: s === "n" || s === "w" ? -1 : 1, a0: ns ? x0 : z0, a1: ns ? x1 : z1 };
}
function fbox(m, S, a0, a1, y0, y1, d0, d1, c, f, under, all) { // a box on a wall face: a along the wall, d out from it; the face against the wall is left out unless `all`
  const p0 = S.fix + S.out * d0, p1 = S.fix + S.out * d1, lo = Math.min(p0, p1), hi = Math.max(p0, p1);
  const o = all ? {} : { [S.ns ? (S.out < 0 ? 5 : 4) : (S.out < 0 ? 1 : 0)]: null }; if (under) o[2] = [mul3(c, .8), f];
  S.ns ? m.box(a0, y0, lo, a1, y1, hi, c, f, o) : m.box(lo, y0, a0, hi, y1, a1, c, f, o);
}
// Painted names: every sign's name is drawn once into a 4 x 32 grid of cells; a lettering quad carries (u, v, cell + share of the cell the name fills).
const TXT = document.createElement("canvas"); TXT.width = TXT.height = 2048; const txt = TXT.getContext("2d"), TXT_CELL = new Map();
function nameCell(name) {
  if (TXT_CELL.has(name)) return TXT_CELL.get(name);
  const i = TXT_CELL.size; if (i >= 128) return null;
  let px = 46; txt.font = `600 ${px}px Georgia, 'Times New Roman', serif`; let tw = txt.measureText(name).width;
  if (tw > 490) { px = Math.floor(px * 490 / tw); txt.font = `600 ${px}px Georgia, 'Times New Roman', serif`; tw = txt.measureText(name).width; }
  txt.fillStyle = "#fff"; txt.textAlign = "center"; txt.textBaseline = "middle"; txt.fillText(name, (i % 4) * 512 + 256, Math.floor(i / 4) * 64 + 34);
  const cell = { i, tw: Math.min(.99, (tw + 8) / 512) }; TXT_CELL.set(name, cell); return cell;
}
function lettering(m, ctr, n, h, cell) {                    // the name on a sign face whose outward normal is n, letters h tall; r is the reader's right, (-n) x up
  const r = [n[2], 0, -n[0]], w = h * 8 * cell.tw * (64 / 46) * .82, hh = h * (64 / 46) * .82;   // r = (-n) x up
  const P = (sr, su) => [ctr[0] + r[0] * sr * w / 2, ctr[1] + su * hh / 2, ctr[2] + r[2] * sr * w / 2];
  m.quad(P(-1, -1), P(1, -1), P(1, 1), P(-1, 1), [0, 0, cell.i + cell.tw], 13, n, true);
  return w;
}
function chimney(x, z, y0, top, wallC) {
  world.box(x - .4, y0, z - .35, x + .4, top + 1, z + .35, wallC, 0); world.box(x - .5, top + 1, z - .45, x + .5, top + 1.15, z + .45, C.stoneDark, 0, { 2: [C.stoneDark, 0] });
  for (const s of [-.17, .17]) world.box(x + s - .09, top + 1.15, z - .09, x + s + .09, top + 1.55, z + .09, C.terracotta, 0);
}
function quoins(l, y0) {                                   // corner stones up the plaster, long and short in turn
  const [x0, z0, x1, z1] = l.rect, dm = detailOf(l.block);
  for (const [cx, sx] of [[x0, -1], [x1, 1]]) for (const [cz, sz] of [[z0, -1], [z1, 1]]) {
    for (let y = y0, k = 0; y < l.H - .3; y += .5, k++) {
      const y1 = Math.min(y + .5, l.H - .05), la = k % 2 ? .55 : .95, lb = k % 2 ? .95 : .55, col = mul3(C.stone, .88 + .2 * hash(l.id, cx, cz, k));
      dm.box(Math.min(cx + sx * .05, cx - sx * la), y, Math.min(cz, cz + sz * .05), Math.max(cx + sx * .05, cx - sx * la), y1, Math.max(cz, cz + sz * .05), col, 0, { [sz < 0 ? 5 : 4]: null });
      dm.box(Math.min(cx, cx + sx * .05), y, Math.min(cz, cz - sz * lb), Math.max(cx, cx + sx * .05), y1, Math.max(cz, cz - sz * lb), col, 0, { [sx < 0 ? 1 : 0]: null });
    }
  }
}
function archHead(dm, S, ca, spring, r, seed, lit) {     // the round head of an arched shop window: glass fan in the far picture, a ring of arch stones near
  const P = (a, y, d) => S.ns ? [a, y, S.fix + S.out * d] : [S.fix + S.out * d, y, a], hint = S.ns ? [0, 0, S.out] : [S.out, 0, 0], N = 12;
  for (let i = 0; i < N; i++) { const t0 = Math.PI * i / N, t1 = Math.PI * (i + 1) / N; world.quad(P(ca, spring, .03), P(ca + r * Math.cos(t0), spring + r * Math.sin(t0), .03), P(ca + r * Math.cos(t1), spring + r * Math.sin(t1), .03), P(ca, spring, .03), [.5, .5, seed], lit ? 1 : 4, hint); }
  const NS = 9, half = Math.PI * (r + .19) / NS / 2 * .94;
  for (let i = 0; i < NS; i++) {
    const th = Math.PI * (i + .5) / NS, key = i === NS >> 1, rr = r + .19 + (key ? .04 : 0), ctr = P(ca + rr * Math.cos(th), spring + rr * Math.sin(th), key ? .05 : .04);
    const m = S.ns ? M4.mul(M4.t(ctr[0], ctr[1], ctr[2]), M4.rz(th - Math.PI / 2)) : M4.mul(M4.mul(M4.t(ctr[0], ctr[1], ctr[2]), M4.ry(-Math.PI / 2)), M4.rz(th - Math.PI / 2));
    dm.obox(m, 0, 0, 0, half * 2, key ? .48 : .38, key ? .1 : .08, key ? C.stoneDark : mul3(C.stone, i % 2 ? .82 : 1), 0);
  }
}
function cafeTable(x, z, alongX) {                         // a round-topped table and two chairs on the pavement
  const g = groundBase(x, z), m = world, W = C.wood;
  m.box(x - .04, g, z - .04, x + .04, g + .72, z + .04, C.metal, 9); m.box(x - .2, g, z - .2, x + .2, g + .03, z + .2, C.metal, 9);
  m.box(x - .38, g + .72, z - .38, x + .38, g + .77, z + .38, W, 0, { 2: [mul3(W, .7), 0] });
  for (const s of [-1, 1]) {
    const cx = x + (alongX ? s * .66 : 0), cz = z + (alongX ? 0 : s * .66), bx = cx + (alongX ? s * .18 : 0), bz = cz + (alongX ? 0 : s * .18);
    m.box(cx - .2, g + .42, cz - .2, cx + .2, g + .47, cz + .2, W, 0, { 2: [mul3(W, .7), 0] });
    m.box(bx - (alongX ? .03 : .2), g + .47, bz - (alongX ? .2 : .03), bx + (alongX ? .03 : .2), g + .92, bz + (alongX ? .2 : .03), W, 0);
    for (const [lx, lz] of [[-.17, -.17], [.17, -.17], [-.17, .17], [.17, .17]]) m.box(cx + lx - .02, g, cz + lz - .02, cx + lx + .02, g + .42, cz + lz + .02, W, 9);
  }
  solid(x - .4, g, z - .4, x + .4, g + .77, z + .4);
}
function awning(l, S, dc, dw, dh, accent, wide) {          // sloped striped canvas with a hanging valance
  const w = wide || dw + 1.4, n = Math.max(4, Math.round(w / .45)), yT = dh + .55, yL = dh + .1, o1 = 1.35;
  const P3 = (a, y, o) => S.ns ? [dc + a, y, S.fix + S.out * o] : [S.fix + S.out * o, y, dc + a], up = S.ns ? [0, 1, S.out * .4] : [S.out * .4, 1, 0], fw = S.ns ? [0, 0, S.out] : [S.out, 0, 0];
  for (let k = 0; k < n; k++) {
    const a0 = -w / 2 + k * w / n, a1 = a0 + w / n, col = k % 2 ? C.cream : accent;
    const q = [P3(a0, yT, .05), P3(a1, yT, .05), P3(a1, yL, o1), P3(a0, yL, o1)], v = [P3(a0, yL, o1), P3(a1, yL, o1), P3(a1, yL - .28, o1), P3(a0, yL - .28, o1)];
    world.quad(q[0], q[1], q[2], q[3], col, 0, up); world.quad(q[0], q[1], q[2], q[3], mul3(col, .7), 0, up.map(x => -x));
    world.quad(v[0], v[1], v[2], v[3], col, 0, fw); world.quad(v[0], v[1], v[2], v[3], mul3(col, .7), 0, fw.map(x => -x));
  }
}

// ---------------------------------------------------------------- the special buildings (Tools/blender/building_kit_08.py)
// Each keeps the ordinary walkable ground floor, door and collisions, and adds its own front, roof and name.
function specialKind(l) {
  const t = l.block.template;
  const tags = l.tags || [];
  if (["bank", "dealer", "library", "museum", "cinema"].includes(l.kind)) return l.kind;   // the places added by Tools/worldgen/add_places_01.py
  if (t === "hospital" || tags.includes("hospital")) return "hospital";
  if (t === "mall" || tags.includes("mall")) return "mall";
  if (t === "hall") return "hall";
  if (t === "market" && l.kind === "work") return "guild";
  if (l.tower) return l.kind === "home" ? "flats" : "office";
  if (l.kind === "work" && t === "peri") return "works";
  if (l.kind === "utility" && l.enterable) return "depot";
  return null;
}
const SP = {                                               // where a point on a face is, and small shapes laid on faces
  at: (S, a, y, d) => S.ns ? [a, y, S.fix + S.out * d] : [S.fix + S.out * d, y, a],
  n: S => S.ns ? [0, 0, S.out] : [S.out, 0, 0],
  win(S, c, y, w, h, d, seed, lit) {                       // a window quad on a face standing d out from the wall
    const a = c - w / 2 - .12, b = c + w / 2 + .12, ya = y - .1, yb = y + h, P = (aa, yy) => SP.at(S, aa, yy, d + .03);
    world.quad(P(a, ya), P(b, ya), P(b, yb), P(a, yb), [0, 0, seed], lit ? 1 : 4, SP.n(S), QUAD_UV);
  },
  disc(m, ctr, n, r, col, f, segs) {                       // a flat round face looking along n
    const rv = [n[2], 0, -n[0]], N = segs || 20, P = a => [ctr[0] + rv[0] * r * Math.cos(a), ctr[1] + r * Math.sin(a), ctr[2] + rv[2] * r * Math.cos(a)];
    for (let i = 0; i < N; i++) m.quad(ctr, P(i * 6.2832 / N), P((i + 1) * 6.2832 / N), ctr, col, f, n);
  },
  clock(S, a, y, d, r) {                                   // stone rim, cream face, iron hands at ten past ten
    const n = SP.n(S);
    SP.disc(world, SP.at(S, a, y, d), n, r + .18, C.stoneDark, 0); SP.disc(world, SP.at(S, a, y, d + .03), n, r, C.cream, 9);
    const hand = (ang, len, w) => { const c = SP.at(S, a + Math.sin(ang) * len / 2, y + Math.cos(ang) * len / 2, d + .07); const m = S.ns ? M4.mul(M4.t(c[0], c[1], c[2]), M4.rz(-ang)) : M4.mul(M4.mul(M4.t(c[0], c[1], c[2]), M4.ry(-Math.PI / 2)), M4.rz(-ang)); world.obox(m, 0, 0, 0, w, len, .04, C.charcoal, 9); };
    hand(-.95, r * .62, .09); hand(1.05, r * .85, .06);
  },
  pyramid(cx, y0, cz, w, h, col) {                         // a tiled pyramid roof
    const q = [[cx - w / 2, y0, cz - w / 2], [cx + w / 2, y0, cz - w / 2], [cx + w / 2, y0, cz + w / 2], [cx - w / 2, y0, cz + w / 2]], ap = [cx, y0 + h, cz];
    for (let i = 0; i < 4; i++) { const p = q[i], r = q[(i + 1) % 4], mid = [(p[0] + r[0]) / 2 - cx, h * .3, (p[2] + r[2]) / 2 - cz]; world.quad(p, r, ap, ap, col, 11, mid); }
  },
  flag(x, y, z, col) { world.box(x - .04, y, z - .04, x + .04, y + 3.2, z + .04, C.metal, 9); world.box(x + .04, y + 2.2, z - .02, x + 1.6, y + 3.1, z + .02, col, 0, { 2: [col, 0] }); },
  gable(S, a0, a1, y, h, col, f) {                         // a triangle standing on the face, with its two sloping tops
    const m = (a0 + a1) / 2, F = d => [SP.at(S, a0, y, d), SP.at(S, a1, y, d), SP.at(S, m, y + h, d)];
    const o = F(.02), i = F(-.4);
    world.quad(o[0], o[1], o[2], o[2], col, f, SP.n(S));
    world.quad(o[0], o[2], i[2], i[0], C.stoneDark, 0, [0, 1, 0]); world.quad(o[2], o[1], i[1], i[2], C.stoneDark, 0, [0, 1, 0]);
  },
  key(m, S, a, y, d, s) {                                  // the guild's gilded key, bow up
    SP.disc(m, SP.at(S, a, y + .32 * s, d + .04), SP.n(S), .3 * s, C.ochre, 0); SP.disc(m, SP.at(S, a, y + .32 * s, d + .05), SP.n(S), .12 * s, C.stone, 0);
    fbox(m, S, a - .06 * s, a + .06 * s, y - .75 * s, y + .05, d, d + .05, C.ochre, 0); fbox(m, S, a, a + .24 * s, y - .72 * s, y - .6 * s, d, d + .05, C.ochre, 0); fbox(m, S, a, a + .18 * s, y - .5 * s, y - .4 * s, d, d + .05, C.ochre, 0);
  },
  lamp(S, a, y) { const p = SP.at(S, a, y, .55); world.box(p[0] - .13, y, p[2] - .13, p[0] + .13, y + .42, p[2] + .13, C.lantern, 2); world.box(p[0] - .17, y + .42, p[2] - .17, p[0] + .17, y + .5, p[2] + .17, C.metal, 9); fbox(world, S, a - .03, a + .03, y + .46, y + .52, 0, .55, C.metal, 9, true); const q = SP.at(S, a, y + .2, 1.0); lights.push([q[0], q[1], q[2], 1, 1]); },
  steps(S, a0, a1, n) { for (let s = 0; s < n; s++) { const p = SP.at(S, a0 - .4 * (n - s), 0, 0), q = SP.at(S, a1 + .4 * (n - s), .16 * (s + 1), 1.4 - .42 * s); const b = [Math.min(p[0], q[0]), 0, Math.min(p[2], q[2]), Math.max(p[0], q[0]), .16 * (s + 1) + Y_PAVE, Math.max(p[2], q[2])]; world.box(b[0], b[1], b[2], b[3], b[4], b[5], C.stoneDark, 6); solid(b[0], b[1], b[2], b[3], b[4], b[5]); } },
};
function special(l, ctx) {
  const { H, DS, dc, dw, dh, wallC, roofC } = ctx, S = DS, dm = detailOf(l.block), cell = nameCell(l.name), nO = SP.n(S), L = S.a1 - S.a0;
  const [x0, z0, x1, z1] = l.rect, depth = S.ns ? z1 - z0 : x1 - x0, seed = k => .96 + .03 * hash(l.id, k);
  const name = (a, y, d, h) => { if (cell) lettering(signs, SP.at(S, a, y, d), nO, h, cell); };
  if (l.sp === "hall") {
    // a blind arcade of lit arches along the front, broad steps to the door, a name panel on the attic, a clock tower behind it
    for (let a = S.a0 + 3; a < S.a1 - 2; a += 4.4) {
      if (Math.abs(a - dc) < dw / 2 + 2.2) continue;
      SP.win(S, a, .35, 2.6, 1.85, 0, seed(a), true); archHead(dm, S, a, 2.1, 1.42, seed(a), true);
      SP.lamp(S, a + 2.2, 2.3);
    }
    SP.steps(S, dc - dw / 2 - 1, dc + dw / 2 + 1, 3);
    fbox(world, S, dc - 6.5, dc + 6.5, H + 1.1, H + 2.9, -.4, .05, C.stone, 6, true, true); fbox(world, S, dc - 6.7, dc + 6.7, H + 2.9, H + 3.1, -.5, .15, C.stoneDark, 0, true);
    name(dc, H + 2.0, .07, .62);
    const T = SP.at(S, dc, 0, -6.5), tw = 6, ty = H + 1.1, top = ty + 10;
    world.box(T[0] - tw / 2, ty, T[2] - tw / 2, T[0] + tw / 2, top, T[2] + tw / 2, C.stone, 6);
    for (const y of [ty + 3.6, top - .3]) world.box(T[0] - tw / 2 - .15, y, T[2] - tw / 2 - .15, T[0] + tw / 2 + .15, y + .3, T[2] + tw / 2 + .15, C.stoneDark, 0, { 2: [C.stoneDark, 0] });
    SP.clock({ ns: S.ns, fix: S.fix - S.out * (6.5 - tw / 2), out: S.out }, dc, ty + 6.6, .05, 1.5);
    for (const [px, pz] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) world.box(T[0] + px * (tw / 2 - .45) - .45, top, T[2] + pz * (tw / 2 - .45) - .45, T[0] + px * (tw / 2 - .45) + .45, top + 3, T[2] + pz * (tw / 2 - .45) + .45, C.stone, 6);
    world.box(T[0] - .7, top, T[2] - .7, T[0] + .7, top + 1.4, T[2] + .7, C.ochre, 0);
    world.box(T[0] - tw / 2 - .2, top + 3, T[2] - tw / 2 - .2, T[0] + tw / 2 + .2, top + 3.4, T[2] + tw / 2 + .2, C.stoneDark, 0, { 2: [C.stoneDark, 0] });
    SP.pyramid(T[0], top + 3.4, T[2], tw + .6, 4.4, roofC); SP.flag(T[0], top + 7.8, T[2], C.terracotta);
  } else if (l.sp === "hospital") {
    // a stone centre bay standing forward with a gable and the name, a canopy over the door on four columns
    const cw = 5, pr = .6, ytop = H + 1.4;
    for (const [p, q] of [[dc - cw, dc - dw / 2], [dc + dw / 2, dc + cw]]) { fbox(world, S, p, q, 0, ytop, 0, pr, C.stone, 6, false, true); const A = SP.at(S, p, 0, 0), B = SP.at(S, q, ytop, pr); solid(Math.min(A[0], B[0]), 0, Math.min(A[2], B[2]), Math.max(A[0], B[0]), ytop, Math.max(A[2], B[2])); }
    fbox(world, S, dc - dw / 2, dc + dw / 2, dh + .7, ytop, 0, pr, C.stone, 6, true, true);
    for (let f = 1; f < l.floors; f++) for (const c of [dc - 3, dc, dc + 3]) SP.win(S, c, f * 3 + 1, 1.15, 1.5, pr, hash(l.id, f, c) * .95, hash(l.id, f, c, 1) < .5);
    fbox(world, S, dc - cw - .15, dc + cw + .15, ytop, ytop + .35, -.2, pr + .2, C.stoneDark, 0, true);
    SP.gable({ ns: S.ns, fix: S.fix + S.out * pr, out: S.out }, dc - cw, dc + cw, ytop + .35, 2.6, C.stone, 0);
    fbox(world, S, dc - .45, dc + .45, ytop + .7, ytop + 1.6, pr, pr + .05, C.teal, 0); fbox(world, S, dc - .08, dc + .08, ytop + .8, ytop + 1.5, pr + .05, pr + .08, C.cream, 0); fbox(world, S, dc - .3, dc + .3, ytop + 1.07, ytop + 1.23, pr + .05, pr + .08, C.cream, 0);
    name(dc, H + .5, pr + .02, .45);
    fbox(world, S, dc - 3.4, dc + 3.4, 3.35, 3.7, pr, 5.2, C.stoneDark, 0, true, true); fbox(world, S, dc - 3.3, dc + 3.3, 3.4, 3.66, 5.2, 5.26, C.teal, 0);
    for (const ca of [dc - 3, dc + 3]) for (const cd of [1.3, 4.8]) { const p = SP.at(S, ca, 0, cd); world.box(p[0] - .2, Y_PAVE, p[2] - .2, p[0] + .2, 3.35, p[2] + .2, C.stone, 0); world.box(p[0] - .3, Y_PAVE, p[2] - .3, p[0] + .3, .45, p[2] + .3, C.stoneDark, 0); solid(p[0] - .25, 0, p[2] - .25, p[0] + .25, 3.35, p[2] + .25); }
    for (const la of [dc - 2, dc + 2]) { const p = SP.at(S, la, 3.0, 3.2); world.box(p[0] - .12, 2.95, p[2] - .12, p[0] + .12, 3.35, p[2] + .12, C.lantern, 2); lights.push([p[0], 2.9, p[2], 1, 1]); }
  } else if (l.sp === "mall") {
    // a great fanlit arch over the door, a gable above the parapet with the name and a clock, a glass vault running in over the middle
    const R = 3.4, sp = dh + .5;
    archHead(dm, S, dc, sp, R, .5, false);
    for (let k = 1; k < 9; k++) { const a = Math.PI * k / 9, c = SP.at(S, dc + Math.cos(a) * R / 2, sp + Math.sin(a) * R / 2, .07); const m = S.ns ? M4.mul(M4.t(c[0], c[1], c[2]), M4.rz(a - Math.PI / 2)) : M4.mul(M4.mul(M4.t(c[0], c[1], c[2]), M4.ry(-Math.PI / 2)), M4.rz(a - Math.PI / 2)); dm.obox(m, 0, 0, 0, .06, R, .05, C.metal, 9); }
    fbox(dm, S, dc - R, dc + R, sp - .08, sp + .04, .03, .09, C.metal, 9, true);
    for (const s of [-1, 1]) fbox(world, S, dc + s * (R + .2) - .35, dc + s * (R + .2) + .35, 0, sp + .2, 0, .25, C.stone, 6);
    const gy = H + 1.24;
    fbox(world, S, dc - 6, dc + 6, H + 1.1, gy + .9, -.4, .06, C.stone, 6, true, true);
    name(dc, gy + .42, .08, .6);
    SP.gable(S, dc - 6, dc + 6, gy + .9, 3.2, C.stone, 6); SP.clock(S, dc, gy + 2.0, .07, .7);
    const vw = 3.5, vh = 2.2, run = Math.min(depth - 3, 36), segs = 6, prof = k => { const a = Math.PI * k / segs; return [Math.cos(a) * vw, Math.sin(a) * vh]; };
    for (let k = 0; k < segs; k++) {
      const [a0, h0] = prof(k), [a1, h1] = prof(k + 1), Q = (aa, hh, d) => SP.at(S, dc + aa, H + .25 + hh, d), out = [ (a0 + a1) / 2, (h0 + h1) / 2 ];
      const nrm = S.ns ? [out[0], out[1], 0] : [0, out[1], out[0]];
      world.quad(Q(a0, h0, -1), Q(a1, h1, -1), Q(a1, h1, -1 - run), Q(a0, h0, -1 - run), [.5, .5, .3], 4, nrm);
    }
    for (let d = -1; d >= -1 - run; d -= 3) for (let k = 0; k < segs; k++) { const [a0, h0] = prof(k), [a1, h1] = prof(k + 1), c = SP.at(S, dc + (a0 + a1) / 2, H + .25 + (h0 + h1) / 2, d), len = Math.hypot(a1 - a0, h1 - h0) * 1.04, ang = Math.atan2(h1 - h0, a1 - a0); const m = S.ns ? M4.mul(M4.t(c[0], c[1], c[2]), M4.rz(ang)) : M4.mul(M4.mul(M4.t(c[0], c[1], c[2]), M4.ry(-Math.PI / 2)), M4.rz(ang)); world.obox(m, 0, 0, 0, len, .12, .12, C.metal, 9); }
  } else if (l.sp === "guild") {
    // two stepped gables over steep roofs whose ridges run back from the street; copings and stone balls on every step;
    // a portal with columns and a hood, the guild's key over it and a key sign hung out
    const nG = L >= 18 ? 2 : 1, gw = L / nG, rise = (gw / 2) * Math.tan(50 * Math.PI / 180), nst = 5, y0 = H + .4, sh = rise / nst;
    for (let g = 0; g < nG; g++) {
      const g0 = S.a0 + g * gw, g1 = g0 + gw, gc = (g0 + g1) / 2, ins = k => (k + .5) * (gw / 2 - .9) / nst;
      for (let k = 0; k < nst; k++) {
        fbox(world, S, g0 + ins(k), g1 - ins(k), y0 + k * sh, y0 + (k + 1) * sh, -.45, .02, C.stone, 6, false, true);
        if (k < nst - 1) for (const sd of [-1, 1]) {
          const outer = sd < 0 ? g0 + ins(k) : g1 - ins(k), inner = sd < 0 ? g0 + ins(k + 1) : g1 - ins(k + 1), a = Math.min(outer, inner) - (sd < 0 ? .07 : 0), b = Math.max(outer, inner) + (sd > 0 ? .07 : 0), yy = y0 + (k + 1) * sh;
          fbox(dm, S, a, b, yy, yy + .14, -.52, .09, C.stoneDark, 0, true, true);
          const bx = (outer + inner) / 2, p = SP.at(S, bx, yy + .14, -.2);
          dm.box(p[0] - .1, yy + .14, p[2] - .1, p[0] + .1, yy + .26, p[2] + .1, C.stone, 0); dm.ybox(p[0], yy + .26, p[2], .2, .2, .2, Math.PI / 4, C.stone, 0);
        }
      }
      fbox(dm, S, g0 + ins(nst - 1) - .07, g1 - ins(nst - 1) + .07, y0 + rise, y0 + rise + .16, -.52, .09, C.stoneDark, 0, true, true);
      const tp = SP.at(S, gc, y0 + rise + .16, -.2); dm.box(tp[0] - .16, y0 + rise + .16, tp[2] - .16, tp[0] + .16, y0 + rise + .6, tp[2] + .16, C.stone, 0); dm.ybox(tp[0], y0 + rise + .6, tp[2], .34, .34, .34, Math.PI / 4, C.ochre, 0);
      for (const [wa, wy, ww] of [[gc - 1.9, y0 + .7, .9], [gc, y0 + .7, .9], [gc + 1.9, y0 + .7, .9], [gc - .9, y0 + 2.9, .8], [gc + .9, y0 + 2.9, .8], [gc, y0 + 4.8, .6]]) if (wy + 1.3 < y0 + rise) SP.win(S, wa, wy, ww, ww < .7 ? .9 : 1.3, 0, hash(l.id, wa, wy) * .95, false);
      const Q = (a, y, d) => SP.at(S, a, y, d), ry = y0 + rise - .5;           // its roof, from just behind the gable to the back wall
      world.quad(Q(g0 - .2, y0 - .2, -.45), Q(gc, ry, -.45), Q(gc, ry, -depth - .3), Q(g0 - .2, y0 - .2, -depth - .3), roofC, 11, [0, 1, 0]);
      world.quad(Q(gc, ry, -.45), Q(g1 + .2, y0 - .2, -.45), Q(g1 + .2, y0 - .2, -depth - .3), Q(gc, ry, -depth - .3), roofC, 11, [0, 1, 0]);
      world.quad(Q(g0, y0, -depth), Q(g1, y0, -depth), Q(gc, ry, -depth), Q(gc, ry, -depth), wallC, 0, mul3(nO, -1));
    }
    for (const s of [-1, 1]) { const p = SP.at(S, dc + s * (dw / 2 + .55), 0, .35); world.box(p[0] - .2, Y_PAVE, p[2] - .2, p[0] + .2, 2.6, p[2] + .2, C.stone, 0); world.box(p[0] - .3, 2.6, p[2] - .3, p[0] + .3, 2.8, p[2] + .3, C.stoneDark, 0); solid(p[0] - .22, 0, p[2] - .22, p[0] + .22, 2.8, p[2] + .22); }
    fbox(world, S, dc - dw / 2 - 1.1, dc + dw / 2 + 1.1, 2.8, 3.1, 0, .7, C.stoneDark, 0, true);
    fbox(world, S, dc - 2.1, dc + 2.1, 3.5, 5.7, 0, .06, C.stone, 6); SP.key(world, S, dc, 5.0, .06, 1.25); name(dc, 3.78, .08, .3);
    const ka = Math.min(S.a1 - 2, dc + 6.5);
    fbox(world, S, ka - .03, ka + .03, 4.55, 4.62, 0, 1.9, C.metal, 9, true); fbox(world, S, ka - .03, ka + .03, 3.6, 4.62, 0, .07, C.metal, 9); SP.key(world, S, ka, 3.8, 1.5, .85);
  } else if (l.sp === "office") {
    // stone piers up every face between the window columns, a cornice over the podium with the name, a clock crown on the roof
    for (const s of "nswe") {
      const F = sideOf(l, s), Lf = F.a1 - F.a0, n = Math.max(1, Math.floor((Lf - 1.6) / 3.1));
      for (let k = 0; k <= n; k++) { const a = clamp(F.a0 + k * Lf / n, F.a0 + .35, F.a1 - .35); fbox(world, F, a - .35, a + .35, 6.6, H, 0, .45, mul3(C.stone, 1.15), 6); }
      for (let f = 2; f < l.floors; f++) for (let k = 0; k < n; k++) {         // dark panels between the floors, so the piers read as ribs
        const a = F.a0 + k * Lf / n + .35, b = F.a0 + (k + 1) * Lf / n - .35; fbox(world, F, a, b, f * 3 + .2, f * 3 + .85, 0, .08, mul3(C.stoneDark, .62), 0, true);
      }
      fbox(world, F, F.a0 - .1, F.a1 + .1, 6.2, 6.6, 0, .4, C.stoneDark, 0, true); fbox(world, F, F.a0, F.a1, 5.65, 6.2, 0, .1, C.stone, 6);
    }
    name(dc, 5.92, .12, .4);
    const T = SP.at(S, dc, 0, -6), tw = 6.5;
    world.box(T[0] - tw / 2, H, T[2] - tw / 2, T[0] + tw / 2, H + 5, T[2] + tw / 2, C.stone, 6); solid(T[0] - tw / 2, H, T[2] - tw / 2, T[0] + tw / 2, H + 5, T[2] + tw / 2);
    world.box(T[0] - tw / 2 - .2, H + 5, T[2] - tw / 2 - .2, T[0] + tw / 2 + .2, H + 5.4, T[2] + tw / 2 + .2, C.stoneDark, 0, { 2: [C.stoneDark, 0] });
    SP.clock({ ns: S.ns, fix: S.fix - S.out * (6 - tw / 2), out: S.out }, dc, H + 2.6, .05, 1.5);
    SP.pyramid(T[0], H + 5.4, T[2], tw + .5, 2.6, roofC); SP.flag(T[0], H + 8, T[2], C.teal);
  } else if (l.sp === "flats") {
    // shutters and balconies come from windows(); here a water tank on legs on the roof, clear of the lift
    const T = SP.at(S, dc + 6, 0, -6);
    for (const [px, pz] of [[-1.2, -1.2], [1.2, -1.2], [-1.2, 1.2], [1.2, 1.2]]) world.box(T[0] + px - .1, H, T[2] + pz - .1, T[0] + px + .1, H + 4, T[2] + pz + .1, C.wood, 9);
    world.box(T[0] - 1.6, H + 4, T[2] - 1.6, T[0] + 1.6, H + 6.2, T[2] + 1.6, [.42, .3, .2], 0, { 2: [C.brown, 0] }); SP.pyramid(T[0], H + 6.2, T[2], 3.6, 1.0, roofC);
    solid(T[0] - 1.4, H, T[2] - 1.4, T[0] + 1.4, H + 6, T[2] + 1.4);
  } else if (l.sp === "works") {
    // a hoist beam, pulley and crate over the loading doors (windows() puts the doors in), a tall lettered chimney at the back
    const hy = H - .5, ha = l.hoistA || dc;
    fbox(world, S, ha - .1, ha + .1, hy, hy + .25, 0, 1.7, C.wood, 0, true); fbox(world, S, ha - .04, ha + .04, hy - .3, hy, 1.4, 1.48, C.metal, 9);
    fbox(world, S, ha - .01, ha + .01, 4.3, hy - .3, 1.43, 1.45, C.charcoal, 9); fbox(world, S, ha - .45, ha + .45, 3.7, 4.3, 1.1, 1.8, [.5, .32, .16], 0, true, true);
    const ca = S.a1 - 3, cd = -(depth - 2.5), P0 = SP.at(S, ca, 0, cd), cH = H + 14;
    world.box(P0[0] - .9, H, P0[2] - .9, P0[0] + .9, H + 5, P0[2] + .9, C.stone, 6); world.box(P0[0] - .7, H + 5, P0[2] - .7, P0[0] + .7, cH, P0[2] + .7, C.stone, 6);
    world.box(P0[0] - .85, cH, P0[2] - .85, P0[0] + .85, cH + .35, P0[2] + .85, C.stoneDark, 0, { 2: [C.stoneDark, 0] });
    const letters = l.name.toUpperCase().replace(/\s+/g, "").slice(0, 12);
    for (let k = 0; k < letters.length; k++) { const c = nameCell(letters[k]); if (c) lettering(signs, SP.at(S, ca, cH - 1.0 - k * .82, cd + .72), nO, .62, c); }
  } else if (l.sp === "depot") {
    // three gables over the front, each with a round window; great boarded doors either side of the entrance; rails running out
    const nb = 3, bw = L / nb;
    for (let i = 0; i < nb; i++) { const b0 = S.a0 + i * bw; SP.gable(S, b0 + .05, b0 + bw - .05, H + 1.24, bw / 2 * .53, C.stone, 6); const oc = SP.at(S, b0 + bw / 2, H + 1.24 + bw * .12, .04); SP.disc(world, oc, nO, .8, C.stoneDark, 0); SP.disc(world, SP.at(S, b0 + bw / 2, H + 1.24 + bw * .12, .07), nO, .58, [.07, .09, .12], 9); }
    for (const s of [-1, 1]) {
      const c = dc + s * bw; if (c - 2.4 < S.a0 || c + 2.4 > S.a1) continue;
      fbox(world, S, c - 2.3, c + 2.3, .02, 3.2, .02, .1, C.teal, 12); archHead(dm, S, c, 3.2, 2.3, .4, false);
      fbox(dm, S, c - .9, c + .9, 1.3, 2.3, .1, .14, C.cream, 0);
      for (const rx of [-.72, .72]) fbox(world, S, c + rx - .04, c + rx + .04, Y_PAVE, Y_PAVE + .04, 0, 9, C.metal, 9);
    }
    for (const rx of [-.72, .72]) fbox(world, S, dc + rx - .04, dc + rx + .04, Y_PAVE, Y_PAVE + .04, 0, 9, C.metal, 9);
    name(dc, 3.6, .06, .42);
  } else if (l.sp === "bank" || l.sp === "museum" || l.sp === "library") {
    // a stone portico: columns on a stepped base, an entablature carrying the name, a pediment; the museum's is wider and hung with banners
    const n = l.sp === "museum" ? 6 : l.sp === "bank" ? 4 : 2, half = l.sp === "museum" ? 7.2 : l.sp === "bank" ? 4.6 : 2.6, ht = l.sp === "library" ? 3.6 : 4.6, dep = 2.2;
    SP.steps(S, dc - half - .3, dc + half + .3, 3);
    for (let k = 0; k < n; k++) {
      const a = dc - half + .4 + k * (2 * half - .8) / (n - 1), p = SP.at(S, a, 0, dep - .4);
      world.box(p[0] - .38, .5, p[2] - .38, p[0] + .38, .78, p[2] + .38, C.stoneDark, 0); world.box(p[0] - .26, .78, p[2] - .26, p[0] + .26, ht - .3, p[2] + .26, C.stone, 6);
      world.box(p[0] - .36, ht - .3, p[2] - .36, p[0] + .36, ht, p[2] + .36, C.stoneDark, 0, { 2: [C.stoneDark, 0] }); solid(p[0] - .3, 0, p[2] - .3, p[0] + .3, ht, p[2] + .3);
    }
    fbox(world, S, dc - half - .2, dc + half + .2, ht, ht + .8, 0, dep, C.stone, 6, true, true); fbox(world, S, dc - half - .35, dc + half + .35, ht + .8, ht + 1.0, -.1, dep + .15, C.stoneDark, 0, true, true);
    name(dc, ht + .4, dep + .02, l.sp === "library" ? .36 : .46);
    SP.gable({ ns: S.ns, fix: S.fix + S.out * dep, out: S.out }, dc - half - .2, dc + half + .2, ht + 1.0, l.sp === "library" ? 1.4 : 2.0, C.stone, 0);
    for (const s of [-1, 1]) { const p = SP.at(S, dc + s * (dw / 2 + .5), 2.4, .3); world.box(p[0] - .13, 2.4, p[2] - .13, p[0] + .13, 2.82, p[2] + .13, C.lantern, 2); lights.push([p[0], 2.6, p[2], 1, 1]); }
    if (l.sp === "museum") for (const s of [-1, 1]) for (const k of [0, 1]) { const a = dc + s * (half + 2.2 + k * 3.2); fbox(world, S, a - .55, a + .55, 3.6, 7.6, .05, .12, k ? C.teal : C.terracotta, 0); fbox(world, S, a - .7, a + .7, 7.6, 7.7, 0, .3, C.metal, 9); fbox(world, S, a - .3, a + .3, 5.2, 6.0, .12, .14, C.cream, 0); }
    if (l.sp === "bank") { fbox(world, S, dc - .9, dc + .9, 1.8, 2.4, .02, .08, [.55, .45, .2], 0); }
  } else if (l.sp === "cinema") {
    // a marquee over the door lit underneath, the name on its front, a tall blade sign spelling it out, lit posters either side
    fbox(world, S, dc - 5.5, dc + 5.5, 3.0, 3.75, 0, 2.6, C.charcoal, 9, true, true); fbox(world, S, dc - 5.5, dc + 5.5, 3.1, 3.65, 2.6, 2.64, [.95, .86, .62], 9);
    for (let a = dc - 5.1; a <= dc + 5.1; a += .6) { const p = SP.at(S, a, 2.98, 2.3); world.box(p[0] - .06, 2.92, p[2] - .06, p[0] + .06, 3.0, p[2] + .06, [1, .8, .5], 2); }
    if (cell) lettering(signs, SP.at(S, dc, 3.38, 2.66), nO, .34, cell);
    if (D.dressing) { const fc = nameCell(D.dressing.film);    // today's film on a board over the marquee (WLD-034)
      fbox(world, S, dc - 4.9, dc + 4.9, 3.78, 4.5, 2.0, 2.12, [.1, .08, .08], 9, true, true); fbox(world, S, dc - 4.9, dc + 4.9, 4.5, 4.56, 2.0, 2.14, [1, .8, .5], 2);
      if (fc) lettering(signs, SP.at(S, dc, 4.14, 2.14), nO, Math.min(.3, 9 / (8 * fc.tw * (64 / 46) * .82)), fc); }
    for (const s of [-1, 1]) { const p = SP.at(S, dc, 2.6, 1.6 * s + 1.3); lights.push([p[0], 2.6, p[2], 1, 1]); }
    const ba = dc + 6.6, word = l.name.split(" ")[0].toUpperCase(), bh = Math.min(H - .6, 3.9 + word.length * .78);
    fbox(world, S, ba - .09, ba + .09, 3.9, bh, .2, 1.3, C.terracotta, 0, true, true); fbox(world, S, ba - .11, ba + .11, 3.9, bh, 1.3, 1.36, [.95, .86, .62], 9);
    for (let k = 0; k < word.length; k++) { const c = nameCell(word[k]); if (!c) continue; for (const sg of [-1, 1]) lettering(signs, SP.at(S, ba + sg * .1, bh - .55 - k * .78, .75), S.ns ? [sg, 0, 0] : [0, 0, sg], .55, c); }
    for (const s of [-1, 1]) for (const k of [0, 1]) { const a = dc + s * (3.0 + k * 1.5); fbox(world, S, a - .6, a + .6, .7, 2.5, 0, .06, C.stoneDark, 0); SP.win(S, a, .85, .95, 1.5, .03, .97, true); }
  } else if (l.sp === "dealer") {
    // a showroom front: tall lit panes the width of the ground floor, the name on a long board above, a forecourt marked in bays
    const n = Math.max(2, Math.floor((L - 4) / 3.4));
    for (let k = 0; k < n; k++) { const a = S.a0 + 2 + (k + .5) * (L - 4) / n; if (Math.abs(a - dc) < dw / 2 + 1.6) continue; SP.win(S, a, .35, 2.8, 2.4, 0, .97, true); fbox(world, S, a - 1.6, a + 1.6, .1, .35, 0, .12, C.stoneDark, 0); }
    fbox(world, S, S.a0 + 1.5, S.a1 - 1.5, 3.05, 3.7, 0, .14, C.teal, 0, true); if (cell) lettering(signs, SP.at(S, dc, 3.37, .16), nO, .42, cell);
    for (let k = 0; k <= 6; k++) { const a = S.a0 + 3 + k * (L - 6) / 6; fbox(world, S, a - .05, a + .05, Y_PAVE + .005, Y_PAVE + .015, 1.2, 5.6, C.cream, 9); }
    fbox(world, S, S.a0 + 3, S.a1 - 3, Y_PAVE + .005, Y_PAVE + .015, 5.5, 5.6, C.cream, 9);
  }
}

function building(l) {
  const [x0, z0, x1, z1] = l.rect, t = 0.35, H = Math.max(l.floors * 3 + 0.4, GF + 0.4), [dx, dz] = l.door;
  const side = Math.abs(dz - z0) < .01 ? "n" : Math.abs(dz - z1) < .01 ? "s" : Math.abs(dx - x0) < .01 ? "w" : "e";
  const big = (x1 - x0) * (z1 - z0) > 800 || l.tower, dw = big ? 3.2 : 1.7, dh = 2.4;
  const wallC = pick(PLASTER, hash(l.id, 1)), roofC = pick(ROOF, hash(l.id, 2)), accent = pick(ACCENT, hash(l.id, 3)), inC = mix3(wallC, [.8, .74, .62], .5);
  l.side = side; l.dw = dw; l.H = H;
  labels.push({ x: dx, y: 3.3, z: dz, text: l.name, lot: l, r: big ? 60 : 30 });
  if (!l.enterable) {
    world.box(x0, 0, z0, x1, H, z1, C.stone, 6); solid(x0, 0, z0, x1, H, z1);
    const d = DOOR_DIR[side]; world.ybox(dx + d[0] * .03, Y_PAVE, dz + d[1] * .03, side === "n" || side === "s" ? 1.6 : .1, 2.3, side === "n" || side === "s" ? .1 : 1.6, 0, C.metal, 9);
    windows(l, wallC); return;
  }
  // ground floor: four stone walls with an open doorway, plaster inside
  const wall = (ax0, az0, ax1, az1, y0, inner) => { world.box(ax0, y0, az0, ax1, GF, az1, C.stone, 6, { [inner]: [inC, 17], 3: null, 2: y0 > 0 ? [C.stoneDark, 9] : null }); solid(ax0, y0, az0, ax1, GF, az1); };
  for (const s of "nswe") {
    const ns = s === "n" || s === "s", inner = { n: 5, s: 4, w: 1, e: 0 }[s];
    const a0 = ns ? x0 : z0 + t, a1 = ns ? x1 : z1 - t, f0 = s === "n" ? z0 : s === "s" ? z1 - t : s === "w" ? x0 : x1 - t;
    const piece = (p, q, y0) => { if (q - p > .01) ns ? wall(p, f0, q, f0 + t, y0, inner) : wall(f0, p, f0 + t, q, y0, inner); };
    if (s !== side) { piece(a0, a1, 0); continue; }
    const dc = ns ? dx : dz;
    piece(a0, dc - dw / 2, 0); piece(dc + dw / 2, a1, 0); arch(ns, s, f0, dc, dw / 2, inC, detailOf(l.block));
    ns ? solid(dc - dw / 2, dh, f0, dc + dw / 2, GF, f0 + t) : solid(f0, dh, dc - dw / 2, f0 + t, GF, dc + dw / 2);
  }
  const fl = ROOM_FLOOR[l.sp] || ROOM_FLOOR[l.kind] || ROOM_FLOOR.work; world.flat(x0 + t, z0 + t, x1 - t, z1 - t, Y_FLOOR, fl[1], fl[0]);
  const flatRoof = big || l.kind === "utility", old = l.block.district !== "Outskirts" && !l.tower && H > 6, ceil = [mix3(inC, [1, 1, 1], .25), 18]; l.old = old;
  l.sp = specialKind(l);
  const stoneUp = l.sp === "office" || (l.block.template === "peri" && hash(l.id, "st") < .15);     // most old houses are plaster over a stone storey, as in the building kit
  const front = (l.kind === "shop" || l.kind === "food") && !l.tower, DSa = sideOf(l, side), aw = front ? Math.min(DSa.a1 - DSa.a0 - 2, 14) : 0; l.aw = aw;
  if (old) world.box(x0, GF, z0, x1, OLD_BASE, z1, C.stone, 6, { 2: ceil, 3: null });
  world.box(x0, old ? OLD_BASE : GF, z0, x1, H, z1, stoneUp ? C.stone : wallC, stoneUp ? 6 : 0, old ? { 3: [C.stoneDark, 0] } : { 2: ceil, 3: [flatRoof ? C.stoneDark : roofC, 0] });
  solid(x0, GF, z0, x1, H, z1);
  const dm = detailOf(l.block), ring = (x, y0, y1, ov) => world.box(x0 - ov, y0, z0 - ov, x1 + ov, y1, z1 + ov, C.stoneDark, 0, { 2: [mul3(C.stoneDark, .8), 0] });
  if (flatRoof) {                                        // a flat roof with a parapet you can walk on, a coping on top and a cornice below
    for (const [a, b, c, d] of [[x0, z0, x1, z0 + .4], [x0, z1 - .4, x1, z1], [x0, z0 + .4, x0 + .4, z1 - .4], [x1 - .4, z0 + .4, x1, z1 - .4]]) { world.box(a, H, b, c, H + 1.1, d, wallC, 0); solid(a, H, b, c, H + 1.1, d); world.box(a - .06, H + 1.1, b - .06, c + .06, H + 1.24, d + .06, C.stoneDark, 0, { 2: [C.stoneDark, 0] }); }
    ring(0, H - .3, H, .12);
  } else if (old) { world.box(x0 - .2, H, z0 - .2, x1 + .2, H + .4, z1 + .2, C.stoneDark, 0, { 2: [C.stoneDark, 0] }); ring(0, H - .24, H, .1); piers(l, dw); }
  else {                                                 // a pitched roof of barrel tiles, ridge along the long side, on corbels, with a chimney
    const alongX = x1 - x0 >= z1 - z0, o = .45, rh = Math.min(5, (alongX ? z1 - z0 : x1 - x0) * .38), up = [0, 1, 0], top = H + rh;
    ring(0, H - .32, H - .12, .07);
    if (alongX) {
      const zc = (z0 + z1) / 2;
      world.quad([x0 - o, H, z0 - o], [x1 + o, H, z0 - o], [x1 + o, top, zc], [x0 - o, top, zc], roofC, 11, up);
      world.quad([x0 - o, H, z1 + o], [x1 + o, H, z1 + o], [x1 + o, top, zc], [x0 - o, top, zc], roofC, 11, up);
      world.quad([x0, H, z0], [x0, H, z1], [x0, top, zc], [x0, top, zc], wallC, 0, [-1, 0, 0]); world.quad([x1, H, z0], [x1, H, z1], [x1, top, zc], [x1, top, zc], wallC, 0, [1, 0, 0]);
      world.box(x0 - o, top - .08, zc - .17, x1 + o, top + .16, zc + .17, mul3(roofC, .9), 11);
      for (const ez of [z0 - o, z1 + o]) world.box(x0 - o, H - .2, ez - .06, x1 + o, H + .02, ez + .06, C.wood, 0, { 2: [C.wood, 0] });
      for (const s of "ns") { const S = sideOf(l, s), n = Math.floor((x1 - x0) / .9); for (let k = 0; k < n; k++) { const c = x0 + (k + .5) * (x1 - x0) / n; fbox(dm, S, c - .09, c + .09, H - .3, H - .02, 0, .34, C.stoneDark, 0, true); } }
      chimney(x0 + 1.2 + (x1 - x0 - 2.4) * hash(l.id, "ch"), zc + .5, H, top, wallC);
    } else {
      const xc = (x0 + x1) / 2;
      world.quad([x0 - o, H, z0 - o], [x0 - o, H, z1 + o], [xc, top, z1 + o], [xc, top, z0 - o], roofC, 11, up);
      world.quad([x1 + o, H, z0 - o], [x1 + o, H, z1 + o], [xc, top, z1 + o], [xc, top, z0 - o], roofC, 11, up);
      world.quad([x0, H, z0], [x1, H, z0], [xc, top, z0], [xc, top, z0], wallC, 0, [0, 0, -1]); world.quad([x0, H, z1], [x1, H, z1], [xc, top, z1], [xc, top, z1], wallC, 0, [0, 0, 1]);
      world.box(xc - .17, top - .08, z0 - o, xc + .17, top + .16, z1 + o, mul3(roofC, .9), 11);
      for (const ex of [x0 - o, x1 + o]) world.box(ex - .06, H - .2, z0 - o, ex + .06, H + .02, z1 + o, C.wood, 0, { 2: [C.wood, 0] });
      for (const s of "we") { const S = sideOf(l, s), n = Math.floor((z1 - z0) / .9); for (let k = 0; k < n; k++) { const c = z0 + (k + .5) * (z1 - z0) / n; fbox(dm, S, c - .09, c + .09, H - .3, H - .02, 0, .34, C.stoneDark, 0, true); } }
      chimney(xc + .5, z0 + 1.2 + (z1 - z0 - 2.4) * hash(l.id, "ch"), H, top, wallC);
    }
  }
  windows(l, wallC);
  if (!stoneUp && !l.tower) quoins(l, old ? OLD_BASE + .25 : l.floors > 1 ? GF + .16 : .45);
  const d = DOOR_DIR[side], ns = side === "n" || side === "s", DS = sideOf(l, side), dc = ns ? dx : dz;
  for (const s of "nswe") {                              // a dark stone plinth round the foot of the walls, open at the door
    const S = sideOf(l, s), segs = s === side ? [[S.a0, dc - dw / 2], [dc + dw / 2, S.a1]] : [[S.a0, S.a1]];
    for (const [p, q] of segs) if (q - p > .05) fbox(world, S, p, q, 0, .45, 0, .08, C.stoneDark, 6);
  }
  const da = DS.a1 - .3;                                 // a drainpipe down the corner of the front
  fbox(world, DS, da - .06, da + .06, .12, H - .2, .06, .18, C.metal, 9); fbox(world, DS, da - .15, da + .15, H - .45, H - .15, .02, .32, C.metal, 9, true); fbox(world, DS, da - .1, da + .1, .12, .3, .06, .42, C.metal, 9);
  if (l.floors > 1) world.box(x0 - .22, old ? OLD_BASE : GF - .1, z0 - .22, x1 + .22, old ? OLD_BASE + .25 : GF + .16, z1 + .22, C.stoneDark, 0, { 2: [C.stoneDark, 0] });
  const shopish = l.kind === "shop" || l.kind === "food" || l.kind === "health";
  if (shopish || big || hash(l.id, "lan") < .4) {              // a wall lantern beside the door
    const o = front ? aw / 2 + .45 : dw / 2 + .55, lx = dx + (ns ? o : d[0] * .2), lz = dz + (ns ? d[1] * .2 : o);
    world.box(lx - .13, 2.5, lz - .13, lx + .13, 2.92, lz + .13, C.lantern, 2); world.box(lx - .17, 2.92, lz - .17, lx + .17, 3, lz + .17, C.metal, 9); world.box(lx - .17, 2.44, lz - .17, lx + .17, 2.5, lz + .17, C.metal, 9);
    lights.push([lx + d[0] * .5, 2.7, lz + d[1] * .5, 1, 1]);
  }
  if ((l.kind === "shop" || l.kind === "food") && hash(l.id, "cr") < .7) crates(dx + (ns ? -dw / 2 - 1.1 : d[0] * .6), dz + (ns ? d[1] * .6 : -dw / 2 - 1.1), l.id);
  const cell = !l.sp && (l.kind === "shop" || l.kind === "food" || l.kind === "health") ? nameCell(l.name) : null;   // special buildings carry their own name
  const at = (a, y, dd) => DS.ns ? [a, y, DS.fix + DS.out * dd] : [DS.fix + DS.out * dd, y, a], nOut = DS.ns ? [0, 0, DS.out] : [DS.out, 0, 0];
  let bw = 0;
  if (cell) {                                            // a name board over the door, cream letters on painted wood (or dark stone for a clinic)
    const y0 = old ? 3.04 : 3.28, y1 = old ? 3.5 : 3.72, lh = (y1 - y0) * .62, tw = lh * 8 * cell.tw * (64 / 46) * .82;
    bw = Math.min(tw + .6, (DS.a1 - DS.a0) - 1);
    fbox(world, DS, dc - bw / 2, dc + bw / 2, y0, y1, 0, .08, l.kind === "food" ? mul3(C.brown, 1.4) : l.kind === "shop" ? C.teal : C.stoneDark, 0, true);
    lettering(signs, at(dc, (y0 + y1) / 2, .09), nOut, lh, cell);
  }
  if (l.kind === "shop" || l.kind === "food") {          // a striped awning over the door and a sign hung out on an iron arm, named on both faces
    if (l.sp === "mall") for (const s of [-1, 1]) awning(l, DS, dc + s * 7.3, dw, dh, accent, 6.4);   // either side of the great arch
    else awning(l, DS, dc, dw, dh, accent, aw);
    if (l.kind === "food") for (const off of [-6.6, -3.5, 3.5, 6.6]) { const ta = dc + off; if (ta > DS.a0 + 1 && ta < DS.a1 - 1) { const p = at(ta, 0, 2.4); cafeTable(p[0], p[2], DS.ns); } }
    const a = dc - Math.max(dw / 2 + .95, bw / 2 + .35);
    if (!l.sp) { fbox(world, DS, a - .03, a + .03, 3.78, 3.86, 0, 1.2, C.metal, 9, true); fbox(world, DS, a - .03, a + .03, 2.95, 3.86, 0, .07, C.metal, 9);
      fbox(world, DS, a - .045, a + .045, 3.0, 3.7, .22, 1.12, C.teal, 0, true); }
    if (cell) { const lh = Math.min(.2, .8 / (8 * cell.tw * (64 / 46) * .82)), na = DS.ns ? [1, 0, 0] : [0, 0, 1];
      for (const sg of [-1, 1]) { const p = at(a + sg * .05, 3.35, .67); lettering(signs, p, na.map(v => v * sg), lh, cell); } }
  } else if (l.kind === "health") {                      // a plaque with a cross beside the door
    const a = dc - dw / 2 - .75;
    fbox(world, DS, a - .3, a + .3, 1.45, 2.05, 0, .05, C.teal, 0, true); fbox(world, DS, a - .05, a + .05, 1.55, 1.95, .05, .08, C.cream, 0); fbox(world, DS, a - .18, a + .18, 1.7, 1.8, .05, .08, C.cream, 0);
  }
  if (l.sp) special(l, { H, DS, dc, dw, dh, wallC, roofC });
  l.furn = [side, accent, inC]; furnish(l, side, accent, inC);     // the room itself is drawn only when someone is near (rooms near the camera, below)
}

function arch(ns, s, f0, dc, r, inC, dm) {                // the wall over a doorway, cut to a round arch (W3), ringed with arch stones and a keystone
  const t = .35, spring = r > 1 ? 1.3 : 1.9, N = 10, first = s === "n" || s === "w", outP = first ? f0 : f0 + t, inP = first ? f0 + t : f0, on = first ? -1 : 1;
  const P3 = (a, y, pl) => ns ? [dc + a, y, pl] : [pl, y, dc + a], nOut = ns ? [0, 0, on] : [on, 0, 0], nIn = [-nOut[0], 0, -nOut[2]];
  for (let i = 0; i < N; i++) {
    const a0 = -r + 2 * r * i / N, a1 = -r + 2 * r * (i + 1) / N, y0 = spring + Math.sqrt(Math.max(0, r * r - a0 * a0)), y1 = spring + Math.sqrt(Math.max(0, r * r - a1 * a1)), am = (a0 + a1) / 2;
    world.quad(P3(a0, y0, outP), P3(a1, y1, outP), P3(a1, GF, outP), P3(a0, GF, outP), C.stone, 6, nOut);
    world.quad(P3(a0, y0, inP), P3(a1, y1, inP), P3(a1, GF, inP), P3(a0, GF, inP), inC, 3, nIn);
    world.quad(P3(a0, y0, outP), P3(a1, y1, outP), P3(a1, y1, inP), P3(a0, y0, inP), C.stoneDark, 0, ns ? [-am, spring - (y0 + y1) / 2, 0] : [0, spring - (y0 + y1) / 2, -am]);
  }
  if (!dm) return;
  const NS = Math.max(7, Math.round(Math.PI * (r + .21) / .36)) | 1, half = Math.PI * (r + .21) / NS / 2 * .94;
  for (let i = 0; i < NS; i++) {
    const th = Math.PI * (i + .5) / NS, key = i === NS >> 1, rr = r + .21 + (key ? .05 : 0), ca = rr * Math.cos(th), cy = spring + rr * Math.sin(th), pl = outP + on * (key ? .05 : .035);
    const m = ns ? M4.mul(M4.t(dc + ca, cy, pl), M4.rz(th - Math.PI / 2)) : M4.mul(M4.mul(M4.t(pl, cy, dc + ca), M4.ry(-Math.PI / 2)), M4.rz(th - Math.PI / 2));
    dm.obox(m, 0, 0, 0, half * 2, key ? .52 : .42, key ? .1 : .07, key ? C.stoneDark : mul3(C.stone, i % 2 ? .82 : 1), 0);
  }
}
function piers(l, dw) {                                    // dark stone piers every five metres along the base
  const [x0, z0, x1, z1] = l.rect;
  for (const s of "nswe") {
    const ns = s === "n" || s === "s", L = ns ? x1 - x0 : z1 - z0, n = Math.max(2, Math.round(L / 5)), a0 = ns ? x0 : z0, fix = s === "n" ? z0 : s === "s" ? z1 : s === "w" ? x0 : x1, out = s === "n" || s === "w" ? -1 : 1;
    for (let k = 0; k <= n; k++) {
      const c = clamp(a0 + k * L / n, a0 + .3, a0 + L - .3); if (s === l.side && (l.aw || Math.abs(c - (ns ? l.door[0] : l.door[1])) < dw / 2 + .75)) continue;
      const p0 = Math.min(fix, fix + out * .28), p1 = Math.max(fix, fix + out * .28);
      ns ? world.box(c - .3, 0, p0, c + .3, OLD_BASE, p1, C.stoneDark, 6) : world.box(p0, 0, c - .3, p1, OLD_BASE, c + .3, C.stoneDark, 6);
    }
  }
}
function crates(x, z, seed) {                             // goods stacked by a door, in the accent colours
  const g = groundBase(x, z), a = .38 + .1 * hash(seed, "c1"), col = k => pick([C.ochre, C.terracotta, [.58, .5, .38], C.ochre], hash(seed, "cc", k));
  world.box(x - a, g, z - a, x + a, g + 2 * a, z + a, col(0), 0); solid(x - a, g, z - a, x + a, g + 2 * a, z + a);
  if (hash(seed, "c2") < .7) { const b = a * .72, ox = (hash(seed, "c3") - .5) * .2; world.box(x + ox - b, g + 2 * a, z - b, x + ox + b, g + 2 * a + 2 * b, z + b, col(1), 0); }
  if (hash(seed, "c4") < .5) { const b = .3, ox = a + .42; world.box(x + ox - b, g, z - b, x + ox + b, g + 2 * b, z + b, col(2), 0); solid(x + ox - b, g, z - b, x + ox + b, g + 2 * b, z + b); }
}
function banner(x0, z0, x1, z1, seed, y) {                // cloth strung across a street between two buildings
  y = y || 5.6; const  ax = z0 === z1, L = ax ? x1 - x0 : z1 - z0, n = Math.max(4, Math.round(L / 2.4)), cols = [C.cream, C.terracotta, C.blue, C.cream, C.ochre, C.teal];
  ax ? world.box(x0, y, z0 - .02, x1, y + .04, z0 + .02, C.brown, 9) : world.box(x0 - .02, y, z0, x0 + .02, y + .04, z1, C.brown, 9);
  for (let k = 0; k < n; k++) {
    const c = (ax ? x0 : z0) + (k + .5) * L / n, w = .55, h = 1 + .3 * hash(seed, k), col = cols[(k + Math.floor(hash(seed) * 6)) % 6];
    const p = (a, yy) => ax ? [a, yy, z0] : [x0, yy, a], q = [p(c - w, y), p(c + w, y), p(c + w, y - h), p(c - w, y - h)];
    world.quad(q[0], q[1], q[2], q[3], col, 0, ax ? [0, 0, 1] : [1, 0, 0]); world.quad(q[0], q[1], q[2], q[3], col, 0, ax ? [0, 0, -1] : [-1, 0, 0]);
  }
}
const QUAD_UV = true;
function windows(l) {                                      // each window is one quad that draws its own frame, sill, reveal and panes
  const [x0, z0, x1, z1] = l.rect, lit = { home: .5, food: .45, shop: .3, work: .16, health: .5, utility: .1, bank: .2, dealer: .3, library: .4, museum: .2, cinema: .1 }[l.kind] || .3;
  const flats = l.sp === "flats", old = l.old, w = flats ? 1.2 : l.tower ? 1.7 : old ? 1.25 : 1.15, gap = l.tower ? 3.1 : old ? 3.6 : 2.9, tall = old ? 2 : 1.5;
  const shut = (!l.tower || flats) && (l.kind === "home" || l.kind === "food" || l.kind === "shop") && (flats || hash(l.id, "sh") < .75);
  const sc = old ? pick([C.teal, [.42, .18, .14], C.ochre, C.blue], hash(l.id, "shc")) : pick(ACCENT, hash(l.id, "shc")), dm = detailOf(l.block), plants = [[.26, .38, .2], [.4, .48, .24]];
  const frameC = pick([C.cream, [.3, .2, .13], C.teal, C.cream], hash(l.id, "fr")), balF = 1, balOdds = l.kind === "food" ? .9 : l.kind === "home" ? .35 : 0;
  for (const s of "nswe") {
    const ns = s === "n" || s === "s", L = ns ? x1 - x0 : z1 - z0, n = Math.max(1, Math.floor((L - 1.6) / gap)), a0 = ns ? x0 : z0;
    const fix = s === "n" ? z0 : s === "s" ? z1 : s === "w" ? x0 : x1, out = s === "n" || s === "w" ? -1 : 1, hint = ns ? [0, 0, out] : [out, 0, 0], S = { ns, fix, out };
    const doorA = ns ? l.door[0] : l.door[1], kDoor = clamp(Math.round((doorA - a0) / (L / n) - .5), 0, n - 1);
    for (let f = 0; f < l.floors; f++) for (let k = 0; k < n; k++) {
      const c = a0 + (k + .5) * L / n, shopW = f === 0 && s === l.side && l.aw > 0;     // a shop or cafe front: wide arched windows, lit inside by day
      if (f === 0 && s === l.side && Math.abs(c - doorA) < l.dw / 2 + (shopW ? 1.35 : 1.5)) continue;
      if (f === 0 && s !== l.side && l.block.template === "peri") continue;      // alley and yard walls of the old courts are plain stone at street level
      if (shopW && Math.abs(c - doorA) > l.aw / 2) continue;
      // the special buildings: no windows where an arcade, a centre bay or a name plaque stands; loading doors up a works
      if (f === 0 && s === l.side && (l.sp === "hall" || l.sp === "depot")) continue;
      if (l.sp === "hospital" && s === l.side && Math.abs(c - doorA) < 5.8) continue;
      if (l.sp === "mall" && s === l.side && Math.abs(c - doorA) < 4.4) continue;
      if ((l.sp === "dealer" || l.sp === "cinema") && s === l.side && f === 0) continue;   // showroom panes and cinema posters stand there
      if ((l.sp === "bank" || l.sp === "museum" || l.sp === "library") && s === l.side && Math.abs(c - doorA) < (l.sp === "museum" ? 8.2 : l.sp === "bank" ? 5.6 : 3.6) && f < 2) continue;   // behind the portico
      if (l.sp === "cinema" && s === l.side && Math.abs(c - doorA - 6.6) < 1.4) continue;   // the blade sign
      if (l.sp === "depot" && s === l.side && f === 1 && Math.min(Math.abs(c - doorA - L / 3), Math.abs(c - doorA + L / 3)) < 2.9) continue;
      if (l.sp === "guild" && s === l.side && f === 1 && Math.abs(c - doorA) < 2.7) continue;
      if (l.sp === "works" && s === l.side && k === kDoor && f > 0) {
        const z = old && f === 1 ? OLD_BASE + .3 : f * 3 + .15, wd = [.42, .28, .18];
        fbox(world, S, c - .8, c + .8, z, z + 2.2, .02, .08, wd, 0);
        for (let q = 1; q < 6; q++) fbox(dm, S, c - .8 + q * .267 - .015, c - .8 + q * .267 + .015, z, z + 2.2, .08, .095, mul3(wd, .6), 0);
        fbox(dm, S, c - 1, c + 1, z - .13, z, 0, .22, C.stoneDark, 0, true); fbox(dm, S, c - 1, c + 1, z + 2.2, z + 2.42, 0, .14, C.stoneDark, 0, true);
        fbox(dm, S, c - .75, c + .75, z + .85, z + .9, .08, .14, C.metal, 9);
        l.hoistA = c; continue;
      }
      const balc = s === l.side && f === balF && k === kDoor && hash(l.id, "bal") < balOdds || flats && f > 1 && (k + f) % 2 === 0;
      const ww = shopW ? 2.0 : w;
      let y = shopW ? .55 : f * 3 + (old ? (f ? 1.05 : .7) : 1), tall0 = shopW ? 2.35 : old && f ? 1.72 : tall; const yTop = y + tall0; if (balc) y = old ? OLD_BASE + .4 : f * 3 + .25;
      if (Dress.covers(l, s, c, y, yTop)) continue;          // a painted advert or a hoarding is there (dressing.js)
      const on = shopW || hash(l.id, s, f, k) < lit, seed = shopW ? .96 + .03 * hash(l.id, k) : hash(l.id, s, f, k, 2) * .95, p = fix + out * .03, a = c - ww / 2 - .12, b = c + ww / 2 + .12, ya = y - .1;
      const arched = shopW && old, yb = arched ? yTop - ww / 2 : yTop;
      const P = (aa, yy, pp) => ns ? [aa, yy, pp] : [pp, yy, aa];
      world.quad(P(a, ya, p), P(b, ya, p), P(b, yb, p), P(a, yb, p), [0, 0, seed], on ? 1 : 4, hint, QUAD_UV);
      if (f === 0 && l.enterable && ya < GF - .3) {        // the same window from inside the room, and a note of it so tall furniture keeps clear
        const pi = fix - out * .36, yi = Math.min(yb, GF - .25); world.quad(P(a, ya, pi), P(b, ya, pi), P(b, yi, pi), P(a, yi, pi), [0, 0, .998], 1, [-hint[0], 0, -hint[2]], QUAD_UV);
        (l.inWin = l.inWin || []).push([s, a - .3, b + .3]);
      }
      if (arched) archHead(dm, S, c, yb, ww / 2 + .12, seed, true);
      // louvred shutters folded back against the wall, mostly in the building's own colour; the far picture keeps a flat copy
      const plainSp = l.sp && l.sp !== "guild" && l.sp !== "flats";          // civic halls, offices, works and the depot have no shutters
      const hasSh = !shopW && !plainSp && (old ? hash(l.id, s, f, k, 5) < .8 : shut && f > 0), scol = hash(l.id, s, f, k, 6) < .9 ? sc : pick(ACCENT, hash(l.id, s, f, k, 7)), lw = ww * .5;
      const leaves = [[a - .17 - lw, a - .17], [b + .17, b + .17 + lw]];
      if (hasSh) for (const [sa, sb] of leaves) world.quad(P(sa, ya + .1, p + out * .02), P(sb, ya + .1, p + out * .02), P(sb, yb - .06, p + out * .02), P(sa, yb - .06, p + out * .02), scol, 12, hint);
      // the relief, near the camera only: a deep reveal (sill, jambs and lintel stand 0.25 m proud of the glass), a real frame with bars,
      // keystone, shutters standing proud, now and then a flower box or a balcony
      if (!balc) fbox(dm, S, a - .14, b + .14, ya - .16, ya + .02, 0, .3, C.stoneDark, 0, true);
      if (l.tower && !flats) continue;
      fbox(dm, S, a - .17, a + .04, ya + .02, yb, 0, .25, C.stoneDark, 0); fbox(dm, S, b - .04, b + .17, ya + .02, yb, 0, .25, C.stoneDark, 0);
      if (!arched) { fbox(dm, S, a - .24, b + .24, yb - .04, yb + .22, 0, .27, C.stoneDark, 0, true); if (old || hash(l.id, "ks") < .5) fbox(dm, S, c - .13, c + .13, yb - .05, yb + .3, 0, .31, C.stone, 0, true); }
      const g0 = a + .16, g1 = b - .16, gy0 = ya + .1, gy1 = yb - .08, fm = shopW ? [.3, .2, .13] : frameC;
      fbox(dm, S, g0 - .02, g0 + .06, gy0, gy1, .03, .09, fm, 0); fbox(dm, S, g1 - .06, g1 + .02, gy0, gy1, .03, .09, fm, 0);
      fbox(dm, S, g0, g1, gy0 - .02, gy0 + .06, .03, .09, fm, 0, true); fbox(dm, S, g0, g1, gy1 - .06, gy1 + .02, .03, .09, fm, 0, true);
      fbox(dm, S, c - .03, c + .03, gy0, gy1, .03, .08, fm, 0); const ty = gy0 + (gy1 - gy0) * .66; fbox(dm, S, g0, g1, ty - .03, ty + .03, .03, .08, fm, 0, true);
      if (shopW) { fbox(dm, S, a + .04, b - .04, .12, ya, 0, .12, [.3, .2, .13], 0); if (l.kind === "food") { fbox(dm, S, a, b, ya + .02, ya + .26, .1, .42, C.terracotta, 0, true); for (let q = 0; q < 3; q++) { const pa = a + .1 + q * (b - a - .2) / 3; fbox(dm, S, pa, pa + (b - a - .2) / 3, ya + .26, ya + .5, .14, .38, plants[q % 2], 0); } } }
      if (hasSh) for (const [sa, sb] of leaves) fbox(dm, S, sa, sb, ya + .1, yb - .06, .02, .09, scol, 12);
      if (balc) {                                          // a stone slab on corbels with an iron rail, the window let down to a door
        const h2 = (b - a) / 2 + .55;
        fbox(dm, S, c - h2, c + h2, ya - .2, ya - .02, 0, .9, C.stoneDark, 0, true);
        for (const cx of [c - h2 + .25, c + h2 - .25]) fbox(dm, S, cx - .09, cx + .09, ya - .55, ya - .2, 0, .62, C.stoneDark, 0, true);
        fbox(dm, S, c - h2 + .03, c + h2 - .03, ya + .86, ya + .92, .8, .86, C.metal, 9, true); fbox(dm, S, c - h2 + .03, c + h2 - .03, ya - .02, ya + .04, .8, .86, C.metal, 9);
        for (const sx of [c - h2 + .03, c + h2 - .07]) { fbox(dm, S, sx, sx + .04, ya + .86, ya + .92, .02, .86, C.metal, 9, true); for (let q = 1; q < 6; q++) fbox(dm, S, sx + .01, sx + .03, ya, ya + .88, q * .15 - .01, q * .15 + .01, C.metal, 9); }
        for (let x = c - h2 + .15; x < c + h2 - .1; x += .13) fbox(dm, S, x - .012, x + .012, ya, ya + .88, .81, .85, C.metal, 9);
        for (const dx of [-h2 + .35, h2 - .35]) { fbox(dm, S, c + dx - .16, c + dx + .16, ya - .02, ya + .3, .4, .7, C.terracotta, 0, true); fbox(dm, S, c + dx - .2, c + dx + .2, ya + .3, ya + .6, .36, .74, plants[0], 0); }
      } else if (f > 0 && l.kind !== "health" && l.kind !== "work" && hash(l.id, s, f, k, 8) < (l.window_boxes ? .8 : .2)) {   // window boxes (WLD-030)
        fbox(dm, S, a, b, ya - .42, ya - .16, .06, .36, C.terracotta, 0, true);
        if (l.window_boxes && D.flowers.size > .2) for (let q = 0; q < 6; q++) { const pa = a + .12 + q * (b - a - .24) / 6; fbox(dm, S, pa, pa + .1, ya + .04, ya + .16 + .08 * hash(l.id, k, q, 4), .12, .26, D.flowers.colors[(q + k + f) % D.flowers.colors.length], 0); }
        for (let q = 0; q < 3; q++) { const pa = a + .05 + q * (b - a - .1) / 3; fbox(dm, S, pa, pa + (b - a - .1) / 3, ya - .16, ya + .06 + .1 * hash(l.id, k, q), .1, .32, plants[q % 2], 0); }
      }
    }
  }
}

function furnish(l, side, accent, inC, vis) {     // at load (no vis): places, uses and collisions; near the camera (vis = a mesh): the furniture itself
  const [x0, z0, x1, z1] = l.rect, o = l.objects, d = DOOR_DIR[side], ns = side === "n" || side === "s", F = Y_FLOOR;
  const toDoor = yawOf(d[0], d[1]), fromDoor = yawOf(-d[0], -d[1]);
  // Every place residents and the player use stays where it was (same heights, same collisions); what stands there is real
  // furniture now, drawn in the block's near-only mesh. Then the room is dressed for what the building is (see room()).
  const dm = vis || NULL_MESH, occ = [], R = ROOM_STYLE[l.sp] || ROOM_STYLE[l.kind] || ROOM_STYLE.work, fab = pick(R.fabric, hash(l.id, "fab"));
  const RR = vis && ROOM_REC;                             // what the room's light needs: its lamps and where things stand on the floor
  const spot = vis ? () => { } : spotAt, use = vis ? () => { } : useAt, hard = (ax0, ay0, az0, ax1, ay1, az1) => { if (RR && ay0 < F + 1.2) RR.foot.push([(ax0 + ax1) / 2, (az0 + az1) / 2, Math.abs(ax1 - ax0) / 2, Math.abs(az1 - az0) / 2, Math.min(1, 1 - (ay0 - F) / 1.4)]); return vis || solid(Math.min(ax0, ax1), ay0, Math.min(az0, az1), Math.max(ax0, ax1), ay1, Math.max(az0, az1)); };
  const B = (cx, y0, cz, sx, sy, sz, c, yaw, f) => {
    if (RR) { const cy = Math.abs(Math.cos(yaw || 0)), sn = Math.abs(Math.sin(yaw || 0));
      if (y0 < 1.1) RR.foot.push([cx, cz, (cy * sx + sn * sz) / 2, (sn * sx + cy * sz) / 2, 1 - y0 / 1.4]);   // seats and table tops shade the floor too, less the higher they are
      if (f === 2) RR.lamps.push([cx, F + y0 + sy / 2, cz, Math.min(.5, .9 * Math.max(sx, sy, sz))]); }   // small lit things: sconces, cases, signs
    dm.ybox(cx, F + y0, cz, sx, sy, sz, yaw || 0, c, f || 19); };
  const chair = (x, z, yaw, c, arm) => {                  // a chair facing along yaw; the back stands behind the sitter
    const fx2 = Math.sin(yaw), fz2 = -Math.cos(yaw), w = arm ? .72 : .44;
    B(x, .4, z, w, .07, w, c, -yaw);
    for (const [u, v] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) { const px = Math.cos(yaw) * u * (w / 2 - .04) + fx2 * v * (w / 2 - .04), pz = Math.sin(yaw) * u * (w / 2 - .04) + fz2 * v * (w / 2 - .04); B(x + px, 0, z + pz, .045, .4, .045, ROOM_WOOD, -yaw); }
    B(x - fx2 * (w / 2 - .03), .47, z - fz2 * (w / 2 - .03), w, arm ? .5 : .48, .06, c, -yaw);
    if (arm) for (const u of [-1, 1]) B(x + Math.cos(yaw) * u * (w / 2 - .05), .47, z + Math.sin(yaw) * u * (w / 2 - .05), .1, .2, w, c, -yaw);
  };
  const table = (x, z, size, h, round) => {               // a table: a round top on a pedestal, or a square one on four legs
    B(x, h - .04, z, size, .045, size, ROOM_WOOD, 0); if (round) { B(x, h - .04, z, size, .045, size, ROOM_WOOD, Math.PI / 4); B(x, 0, z, .09, h - .04, .09, C.metal); B(x, 0, z, .45, .04, .45, C.metal, Math.PI / 4); }
    else for (const [u, v] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) B(x + u * (size / 2 - .06), 0, z + v * (size / 2 - .06), .06, h - .04, .06, ROOM_WOOD);
  };
  for (const [x, z] of o.bed || []) {                     // a framed bed with a headboard, blanket and pillow, a nightstand and its lamp
    const bl = pick([C.cream, C.blue, C.olive, C.plum, C.terracotta], hash(l.id, x, z));
    B(x, 0, z, 1.0, .32, 2.0, ROOM_WOOD); B(x, .32, z, .94, .2, 1.92, [.78, .74, .66]); B(x, .5, z - .3, .98, .1, 1.36, bl); B(x, .52, z + .68, .62, .14, .34, [.85, .82, .76]);
    B(x, 0, z + 1.02, 1.06, .98, .08, ROOM_WOOD); B(x, 0, z - 1.02, 1.06, .5, .07, ROOM_WOOD);
    if (hash(l.id, x, z, "ns") < .7) { const sx = x + (hash(l.id, z, "s") < .5 ? -.82 : .82); B(sx, 0, z + .75, .42, .5, .38, ROOM_WOOD); B(sx, .5, z + .75, .12, .22, .12, [.9, .8, .6], 0, 2); }
    hard(x - .5, F, z - 1, x + .5, F + .5, z + 1); occ.push([x, z, 1.5]);
    spot(l, x, z, 3, 0, F + .62); use(x, F + .62, z, "lie", "Lie down", 0);
  }
  (o.room || []).forEach(([x, z], k) => { const yaw = Math.floor(hash(l.id, k, "r") * 4) * Math.PI / 2; chair(x, z, yaw, fab, l.kind === "home"); occ.push([x, z, 1.0]); spot(l, x, z, 2, yaw, F + .45); use(x, F + .45, z, "sit", "Sit down", yaw); });
  (o.chair || []).forEach(([x, z]) => { chair(x, z, toDoor, R.chair || C.cream); occ.push([x, z, .9]); spot(l, x, z, 2, toDoor, F + .45); use(x, F + .45, z, "sit", "Sit down", toDoor); });
  (o.seat || []).forEach(([x, z], k) => {
    if (l.kind === "cinema" || l.kind === "museum") {     // a cinema seat facing the screen at the back; a museum bench facing the exhibits
      const cin = l.kind === "cinema", red = [.42, .1, .1];
      if (cin) { B(x, 0, z, .5, .42, .5, C.charcoal, -fromDoor); B(x, .42, z, .5, .1, .48, red, -fromDoor); B(x - Math.sin(fromDoor) * .24, .42, z + Math.cos(fromDoor) * .24, .52, .55, .1, red, -fromDoor); }
      else { B(x, .4, z, 1.5, .07, .45, ROOM_WOOD, -fromDoor); for (const u of [-.65, .65]) B(x + Math.cos(fromDoor) * u, 0, z + Math.sin(fromDoor) * u, .07, .4, .4, ROOM_WOOD, -fromDoor); }
      occ.push([x, z, cin ? .45 : .9]); spot(l, x, z, 2, fromDoor, F + .45); use(x, F + .45, z, "sit", cin ? "Take a seat" : "Sit on the bench", fromDoor); return;
    }
    const sgn = k % 2 ? -1 : 1; chair(x, z, yawOf(sgn, 0), R.chair || ROOM_WOOD); table(x + sgn * .7, z, .64, .74, l.kind === "food");
    if (l.kind === "food" || l.sp === "mall") chair(x + sgn * 1.4, z, yawOf(-sgn, 0), R.chair || ROOM_WOOD);   // a table for two: a second chair across it
    hard(x + sgn * .7 - .31, F, z - .31, x + sgn * .7 + .31, F + .74, z + .31); occ.push([x + sgn * .35, z, 1.2]);
    spot(l, x, z, 2, yawOf(sgn, 0), F + .45); use(x, F + .45, z, "sit", "Sit down", yawOf(sgn, 0));
  });
  for (const [x, z] of o.work || []) {                    // a desk with a lamp and papers, or a counter with what that trade keeps on it
    const desk = l.kind === "work" && l.sp !== "works", cx = x + d[0] * .8, cz = z + d[1] * .8, w = desk ? 1.3 : 1.7, dep = desk ? .65 : .55, h = desk ? .76 : .98;
    const sx = ns ? w : dep, sz = ns ? dep : w;
    if (desk) {
      B(cx, h - .04, cz, sx, .04, sz, ROOM_WOOD); B(cx + (ns ? w / 2 - .22 : 0), 0, cz + (ns ? 0 : w / 2 - .22), ns ? .4 : dep - .04, h - .04, ns ? dep - .04 : .4, ROOM_WOOD);
      B(cx - (ns ? w / 2 - .04 : 0), 0, cz - (ns ? 0 : w / 2 - .04), ns ? .05 : dep - .06, h - .04, ns ? dep - .06 : .05, ROOM_WOOD);
      B(cx - (ns ? .2 : 0), h, cz - (ns ? 0 : .2), .32, .02, .24, [.88, .86, .8], .2); B(cx + (ns ? .4 : .1), h, cz + (ns ? .1 : .4), .1, .38, .1, C.metal); B(cx + (ns ? .4 : .1), h + .34, cz + (ns ? .1 : .4), .2, .08, .2, R.lamp || C.teal);
      chair(x, z, toDoor, R.chair || ROOM_WOOD); use(x, F + .45, z, "sit", "Sit at the desk", toDoor);
    } else if (l.kind !== "food") {                       // (a cafe's staff stand behind one long coffee bar, built in room())
      B(cx, 0, cz, sx, h - .05, sz, R.counter || ROOM_WOOD); B(cx, h - .05, cz, sx + .06, .05, sz + .06, R.top || accent);
      for (let q = 0; q < 3; q++) { const u = (q - 1) * w / 3; B(cx + (ns ? u : d[0] * -.29), .12, cz + (ns ? d[1] * -.29 : u), ns ? w / 3 - .12 : .02, h - .3, ns ? .02 : w / 3 - .12, mul3(R.counter || ROOM_WOOD, .8)); }
      const tool = R.onCounter; if (tool) tool(B, cx, cz, h, ns, l);
    }
    hard(cx - sx / 2, F, cz - sz / 2, cx + sx / 2, F + h, cz + sz / 2); occ.push([cx, cz, 1.2], [x, z, .8]);
    spot(l, x, z, desk ? 2 : 0, toDoor, desk ? F + .45 : F);
  }
  for (const [x, z] of o.customer || []) { spot(l, x, z, 0, fromDoor, F); occ.push([x, z, .7]); }
  room(l, side, F, dm, occ, R, accent, fab, chair, table, B, hard);
  if (l.tower && !vis) {                                       // a lift in the back corner, up to the roof terrace
    const bx = side === "e" ? x0 + 1.5 : side === "w" ? x1 - 1.5 : x0 + 1.5, bz = side === "s" ? z0 + 1.5 : side === "n" ? z1 - 1.5 : z0 + 1.5;
    const ix = bx < (x0 + x1) / 2 ? 1 : -1, iz = bz < (z0 + z1) / 2 ? 1 : -1, top = l.H;
    for (const y of [F, top]) {
      world.box(bx - 1.1, y, bz - 1.1, bx + 1.1, y + 2.7, bz + 1.1, C.metal, y === F ? 3 : 9); solid(bx - 1.1, y, bz - 1.1, bx + 1.1, y + 2.7, bz + 1.1);
      world.box(bx + ix * 1.1 - .04, y + .1, bz - .7, bx + ix * 1.1 + .04, y + 2.2, bz + .7, C.ochre, y === F ? 3 : 9); world.box(bx + ix * 1.16 - .03, y + 1.2, bz + iz * .95 - .08, bx + ix * 1.16 + .03, y + 1.36, bz + iz * .95 + .08, C.lantern, 2);
    }
    use(bx + ix * 1.9, F, bz, "lift", "Take the lift to the roof", 0, { to: [bx + ix * 2.2, top, bz + iz * 1.2] });
    use(bx + ix * 1.9, top, bz, "lift", "Take the lift down", 0, { to: [bx + ix * 2.2, F, bz + iz * 1.2] });
    labels.push({ x: bx, y: 2.9, z: bz, text: "Lift", lot: l, r: 14, indoor: true });
  }
}

// ---------------------------------------------------------------- rooms: what each kind of building is furnished with
const ROOM_WOOD = [.36, .24, .15], ROOM_GOODS = [C.ochre, C.terracotta, C.olive, C.cream, [.6, .2, .16], [.42, .5, .24], [.3, .38, .52], [.55, .45, .3]];
const ROOM_FLOOR = {                                         // [material, colour]: 15 boards, 16 tiles
  home: [15, [.4, .27, .17]], food: [16, [.56, .34, .24]], shop: [16, [.58, .52, .44]], work: [15, [.36, .26, .18]], works: [15, [.3, .24, .19]],
  bank: [16, [.74, .7, .62]], library: [15, [.4, .27, .17]], museum: [16, [.66, .62, .56]], cinema: [16, [.34, .14, .12]], dealer: [16, [.72, .72, .7]],
  health: [16, [.66, .66, .63]], hospital: [16, [.66, .66, .63]], utility: [16, [.4, .4, .4]], mall: [16, [.62, .56, .46]], hall: [16, [.5, .4, .32]], guild: [15, [.38, .25, .16]], office: [15, [.36, .26, .18]], flats: [16, [.58, .5, .42]],
};
const espresso = (B, cx, cz, h, ns) => { B(cx - (ns ? .45 : 0), h, cz - (ns ? 0 : .45), .5, .42, .36, C.metal); B(cx - (ns ? .45 : 0), h + .42, cz - (ns ? 0 : .45), .3, .08, .3, [.6, .58, .55]); for (let k = 0; k < 4; k++) B(cx + (ns ? .1 + k * .12 : 0), h, cz + (ns ? 0 : .1 + k * .12), .07, .08, .07, [.86, .84, .8]); B(cx + (ns ? .62 : 0), h, cz + (ns ? 0 : .62), .3, .26, .3, [.95, .82, .55], 0, 2); };
const till = (B, cx, cz, h, ns) => { B(cx - (ns ? .5 : 0), h, cz - (ns ? 0 : .5), .36, .2, .3, C.charcoal); B(cx - (ns ? .5 : 0), h + .2, cz - (ns ? 0 : .5), .26, .14, .04, [.2, .3, .3]); B(cx + (ns ? .35 : 0), h, cz + (ns ? 0 : .35), .5, .14, .34, C.ochre); };
const vise = (B, cx, cz, h, ns) => { B(cx - (ns ? .5 : 0), h, cz - (ns ? 0 : .5), .22, .18, .2, C.metal); B(cx - (ns ? .5 : 0), h + .18, cz - (ns ? 0 : .5), .04, .04, .34, C.metal); B(cx + (ns ? .2 : 0), h, cz + (ns ? 0 : .2), .5, .03, .08, [.5, .32, .18]); B(cx + (ns ? .4 : 0), h, cz + (ns ? 0 : .4), .06, .05, .3, C.metal); };
const ledger = (B, cx, cz, h, ns) => { B(cx, h, cz, .3, .04, .22, [.86, .84, .78]); B(cx + (ns ? .5 : 0), h, cz + (ns ? 0 : .5), .1, .3, .1, C.metal); };
const ROOM_STYLE = {
  // tall/low: pieces for the walls (tall ones keep off windows and the door wall); mid: things for open floor; pictures; lamp shades
  home: { tall: ["wardrobe", "bookcase", "dresser", "fireplace"], low: ["sideboard", "plant", "dresser"], mid: ["dining", "rug"], pictures: "art", fabric: [C.olive, C.plum, [.55, .35, .24], C.blue], lamp: C.cream },
  food: { tall: [], low: ["plant"], mid: [], pictures: "menu", fabric: [C.cream], chair: [.42, .3, .2], counter: [.3, .2, .13], top: [.62, .58, .52], onCounter: espresso, lamp: C.ochre },
  shop: { tall: [], low: ["plant"], mid: [], pictures: "price", fabric: [C.cream], counter: [.32, .22, .14], top: C.teal, onCounter: till, lamp: C.cream },
  work: { tall: ["bookcase", "filing"], low: ["plant", "sideboard"], mid: ["rug"], pictures: "art", fabric: [C.blue, C.olive], onCounter: ledger, lamp: C.teal },
  office: { tall: ["bookcase", "filing"], low: ["plant", "sofa"], mid: ["reception", "rug"], pictures: "art", fabric: [C.blue, C.charcoal], onCounter: ledger, lamp: C.teal },
  guild: { tall: ["bookcase", "fireplace"], low: ["sideboard", "plant"], mid: ["long", "rug"], pictures: "art", fabric: [C.terracotta], onCounter: ledger, lamp: C.ochre },
  hall: { tall: ["bookcase", "filing"], low: ["bench", "plant"], mid: ["reception", "benches"], pictures: "art", fabric: [C.terracotta], onCounter: ledger, lamp: C.cream },
  works: { tall: ["lockers"], low: ["workbench", "barrels", "crates"], mid: ["crates"], pictures: "tools", fabric: [C.charcoal], onCounter: vise, counter: [.36, .26, .18], top: [.42, .3, .2], lamp: C.metal },
  health: { tall: ["medcab"], low: ["sink", "bench", "plant"], mid: ["reception", "screens"], pictures: "notice", fabric: [C.teal], chair: [.6, .62, .62], counter: [.8, .8, .76], top: C.teal, onCounter: ledger, lamp: C.cream },
  hospital: { tall: ["medcab"], low: ["sink", "bench", "plant"], mid: ["reception", "screens", "beds"], pictures: "notice", fabric: [C.teal], chair: [.6, .62, .62], counter: [.8, .8, .76], top: C.teal, onCounter: ledger, lamp: C.cream },
  utility: { tall: ["lockers"], low: ["workbench", "crates", "barrels"], mid: ["crates"], pictures: "tools", fabric: [C.charcoal], onCounter: ledger, lamp: C.metal },
  mall: { tall: [], low: ["plant", "bench"], mid: ["kiosk", "plant"], pictures: "price", fabric: [C.cream], counter: [.32, .22, .14], top: C.ochre, onCounter: till, lamp: C.ochre },
  flats: { tall: [], low: ["sofa", "plant"], mid: ["reception", "rug"], pictures: "art", fabric: [C.olive], onCounter: ledger, lamp: C.cream },
  bank: { tall: ["filing"], low: ["plant", "bench"], mid: [], pictures: "art", fabric: [C.teal], chair: [.42, .3, .2], counter: [.3, .2, .13], top: [.72, .7, .64], onCounter: ledger, lamp: C.cream },
  library: { tall: ["bookcase", "bookcase", "filing"], low: ["plant", "bench"], mid: [], pictures: "art", fabric: [C.olive], chair: [.36, .24, .15], counter: [.3, .2, .13], top: [.24, .34, .26], onCounter: ledger, lamp: C.olive },
  museum: { tall: [], low: ["bench", "plant"], mid: [], pictures: "art", fabric: [C.terracotta], counter: [.3, .2, .13], top: [.62, .58, .52], onCounter: ledger, lamp: C.cream },
  cinema: { tall: [], low: [], mid: [], pictures: "art", fabric: [[.42, .1, .1]], counter: [.42, .1, .1], top: [.62, .5, .3], onCounter: till, lamp: [.42, .1, .1], dim: true },
  dealer: { tall: [], low: ["plant", "sofa"], mid: [], pictures: "price", fabric: [C.charcoal], counter: [.32, .34, .36], top: [.8, .8, .78], onCounter: ledger, lamp: C.cream },
};
// The arrangement of each kind of building, laid out from the door: `a` runs along the door wall, `i` inward from it.
// Everything is placed round the data's own places (staff behind counters at the back, seats and customers in front).
function layout(l, side, F, dm, occ, R, accent, fab, chair, table, B, hard, clear, walls, box, P) {
  const [x0, z0, x1, z1] = l.rect, t = .35, o = DOOR_DIR[side], [dx, dz] = l.door, dw = l.dw, FR = Math.atan2(-o[0], -o[1]);
  const h = (...k) => hash(l.id, "lay", ...k), WD = ROOM_WOOD, DARK = [.3, .2, .13], TOP = [.62, .58, .52], WHITE = [.84, .85, .83];
  const wp = (a, i) => [dx - o[0] * i - o[1] * a, dz - o[1] * i + o[0] * a];
  const rel = ([x, z]) => [-(x - dx) * o[1] + (z - dz) * o[0], -((x - dx) * o[0] + (z - dz) * o[1])];
  const cs = [[x0, z0], [x1, z0], [x0, z1], [x1, z1]].map(rel), aMin = Math.min(...cs.map(c => c[0])) + t, aMax = Math.max(...cs.map(c => c[0])) - t, iMax = Math.max(...cs.map(c => c[1])) - t, iMin = t;
  const FB = (a, i, y0, sa, sy, si, c, f, yaw) => { const p = wp(a, i); B(p[0], y0, p[1], sa, sy, si, c, FR + (yaw || 0), f); };
  const FH = (a0, a1, i0, i1, ht) => { const p = wp(a0, i0), q = wp(a1, i1); hard(p[0], F, p[1], q[0], F + ht, q[1]); };
  const FO = (a, i, r) => { const p = wp(a, i); occ.push([p[0], p[1], r]); };
  const FF = (a, i, r) => { const p = wp(a, i); return clear(p[0], p[1], r) && a - r > aMin - .35 && a + r < aMax + .35 && i - r > iMin - .35 && i + r < iMax + .35; };   // (pieces may stand against a wall)
  const FFrun = (a, i0, i1, r) => { for (let i = i0; i <= i1; i += .8) if (!FF(a, i, r)) return false; return true; };
  const ob = k => ((l.objects || {})[k] || []).map(rel), pv = [-o[1], 0, o[0]];
  const lamp = (a, i, drop, c) => { const p = wp(a, i); if (ROOM_REC && dm !== NULL_MESH) ROOM_REC.lamps.push([p[0], GF - drop - .3, p[1], 1.0]); dm.box(p[0] - .01, GF - drop, p[1] - .01, p[0] + .01, GF, p[1] + .01, C.charcoal, 19); dm.ybox(p[0], GF - drop - .2, p[1], .32, .2, .32, 0, c || C.ochre, 19); dm.box(p[0] - .07, GF - drop - .27, p[1] - .07, p[0] + .07, GF - drop - .19, p[1] + .07, [1, .78, .5], 2); };
  const glass = (a, i0, i1, y0, y1, facing) => { const P1 = (ii, y) => { const p = wp(a, ii); return [p[0], F + y, p[1]]; }; dm.quad(P1(i0, y0), P1(i1, y0), P1(i1, y1), P1(i0, y1), [0, 0, .998], 1, facing, true); };
  const key = ["bank", "dealer", "library", "museum", "cinema"].includes(l.kind) ? l.kind : l.sp === "mall" ? "mall" : l.kind === "food" ? "cafe" : l.kind === "shop" ? "grocer" : l.kind === "home" ? "home" : l.sp === "works" ? "works"
    : l.kind === "work" ? (l.sp === "hall" ? "hall" : l.sp === "guild" ? "guild" : "office") : l.kind === "health" ? "clinic" : l.kind === "utility" ? "depot" : null;
  const Wk = ob("work"), Cu = ob("customer"), Ch = ob("chair");

  if (key === "cafe") {
    // one coffee bar across the back joining the baristas' places: espresso machines, grinders, cups, a lit cake case
    if (Wk.length) {
      const wi = Math.max(...Wk.map(w => w[1])), ci = wi - .8, a0 = Math.max(aMin + .4, Math.min(...Wk.map(w => w[0])) - 2.6), a1 = Math.min(aMax - .4, Math.max(...Wk.map(w => w[0])) + 2.6), am = (a0 + a1) / 2, L = a1 - a0;
      FB(am, ci, 0, L, 1.0, .62, DARK); FB(am, ci, 1.0, L + .1, .06, .8, TOP);
      for (let k = 0; k < Math.floor(L / .9); k++) FB(a0 + .45 + k * .9, ci - .32, .15, .78, .72, .02, [.24, .16, .1]);
      FB(am, ci - .45, .18, L, .04, .04, C.metal); FH(a0, a1, ci - .4, ci + .4, 1.06);
      for (const [wa] of Wk) {
        FB(wa - .25, ci + .1, 1.06, .56, .46, .42, C.metal); FB(wa - .25, ci + .1, 1.52, .4, .06, .32, [.6, .58, .55]); FB(wa - .32, ci - .14, 1.18, .05, .12, .05, [.2, .2, .22]); FB(wa - .18, ci - .14, 1.18, .05, .12, .05, [.2, .2, .22]);
        FB(wa + .4, ci + .15, 1.06, .18, .4, .18, [.2, .2, .22]); FB(wa + .4, ci + .15, 1.46, .2, .12, .2, [.5, .3, .18]);
        for (let k = 0; k < 4; k++) FB(wa + .75 + (k % 2) * .11, ci - .05, 1.06 + Math.floor(k / 2) * .09, .08, .085, .08, [.9, .88, .84]);
      }
      FB(a1 - .75, ci, 1.06, 1.2, .48, .58, [.95, .86, .64], 2); FB(a1 - .75, ci, 1.54, 1.24, .03, .62, C.metal);
      for (let k = 0; k < 4; k++) FB(a1 - 1.2 + k * .3, ci, 1.08, .22, .11, .22, pick([C.ochre, [.55, .3, .18], C.cream, [.6, .2, .16]], h(k, "cake")));
      FB(a0 + .35, ci, 1.06, .16, .22, .16, [.75, .8, .82]);
      const bi = iMax - .32;                                // the back counter, shelves of cups and bottles, chalk menus over the baristas
      if (bi - ci > 1.2) {
        FB(am, bi, 0, L, .9, .6, DARK); FB(am, bi, .9, L + .05, .04, .64, TOP); FH(a0, a1, bi - .3, iMax, .95);
        for (let s = 0; s < 2; s++) { FB(am, iMax - .14, 1.45 + s * .42, L, .04, .28, WD); for (let k = 0; k < Math.floor(L / .24); k++) FB(a0 + .15 + k * .24, iMax - .14, 1.49 + s * .42, .1, s ? .27 : .11, .1, s ? pick([[.16, .3, .18], [.38, .2, .1], [.6, .52, .3], [.5, .14, .12]], h(k, s)) : [.9, .88, .84]); }
        for (let k = 0; k < 3; k++) { const ma = am + (k - 1) * L / 3.2; FB(ma, iMax - .03, 2.25, 1.1, .72, .03, [.1, .1, .1]); FB(ma, iMax - .05, 2.86, .7, .05, .02, [.9, .86, .74]); for (let q = 0; q < 4; q++) FB(ma - .3 + .12 * h(k, q), iMax - .05, 2.72 - q * .14, .4 + .3 * h(q, k), .03, .02, [.86, .82, .72]); }
        for (let a = a0 + .7; a < a1; a += 1.5) FO(a, bi, .8);
      }
      for (let k = 0; k < Math.max(2, Math.round(L / 2.4)); k++) lamp(a0 + (k + .5) * L / Math.max(2, Math.round(L / 2.4)), ci, .85, C.ochre);
      for (let a = a0 + .5; a < a1; a += 1.2) FO(a, ci, .7);
    }
    // a window bar with stools under the front windows, either side of the door
    for (const sg of [-1, 1]) {
      const b0 = sg < 0 ? aMin + .5 : dw / 2 + 1.6, b1 = sg < 0 ? -dw / 2 - 1.6 : aMax - .5;
      if (b1 - b0 < 1.8 || !FF((b0 + b1) / 2, .9, .2)) continue;
      let ok = true; for (let a = b0 + .3; a < b1; a += .8) if (!FF(a, .95, .3)) { ok = false; break; } if (!ok) continue;
      FB((b0 + b1) / 2, .6, 1.02, b1 - b0, .05, .44, TOP); for (let a = b0 + .3; a < b1; a += 1.5) FB(a, .55, 0, .06, 1.02, .06, C.metal); FH(b0, b1, .38, .82, 1.05);
      for (let a = b0 + .45; a < b1 - .2; a += .9) { FB(a, 1.1, .74, .36, .05, .36, DARK); FB(a, 1.1, 0, .05, .74, .05, C.metal); FB(a, 1.1, .25, .3, .03, .3, C.metal, Math.PI / 4); }
      for (let a = b0; a <= b1; a += 1) FO(a, .95, .55);
    }
    const ka = dw / 2 + .9;                                // a coat stand by the door
    if (FF(ka, 1.0, .3)) { FB(ka, 1.0, 0, .05, 1.8, .05, WD); FB(ka, 1.0, 0, .46, .04, .46, WD, Math.PI / 4); FB(ka, 1.0, 1.62, .5, .05, .05, WD); FB(ka, 1.0, 1.62, .05, .05, .5, WD); FB(ka + .18, 1.0, 1.2, .2, .45, .12, C.olive); FO(ka, 1.0, .35); }
  }

  if (key === "grocer") {
    // checkouts at the staff places: a belt, a sweets rack in front, a stack of baskets by the door
    const wiMin = Wk.length ? Math.min(...Wk.map(w => w[1])) : iMax - 2, ci = wiMin - .8;
    for (const [wa, wiK] of Wk) {
      FB(wa, wiK - .8, .98, 1.5, .02, .32, [.12, .12, .13]); FB(wa - .55, wiK - .8, .98, .3, .2, .3, [.2, .2, .2]);
      FB(wa + .35, wiK - 1.3, 0, .9, 1.1, .26, WD); for (let s = 0; s < 3; s++) for (let k = 0; k < 4; k++) FB(wa + .02 + k * .22, wiK - 1.38, .18 + s * .3, .16, .13, .1, pick(ROOM_GOODS, h(wa, s, k)));
      FH(wa - .1, wa + .8, wiK - 1.45, wiK - 1.15, 1.1); FO(wa + .35, wiK - 1.3, .5);
      const p = wp(wa, wiK - .8); dm.box(p[0] - .01, GF - .55, p[1] - .01, p[0] + .01, GF, p[1] + .01, C.charcoal, 19); FB(wa, wiK - .8, GF - F - .8, .9, .25, .04, C.teal); FB(wa, wiK - .82, GF - F - .74, .5, .12, .02, C.cream);
    }
    const ba = -(dw / 2 + 1.1); if (FF(ba, 1.0, .3)) { for (let k = 0; k < 5; k++) FB(ba, 1.0, k * .2, .5, .18, .36, [.62, .16, .12]); FO(ba, 1.0, .4); }
    // fridges with lit glass doors down one side wall; produce stands down the other
    const sideRun = (aw, sg, build) => { for (let i = 2.0; i + 1.3 < iMax - .3; i += 1.4) build(aw + sg * .45, i + .65, sg, i); };
    sideRun(aMin, 1, (a, i, sg, i0) => {
      if (!FF(a, i, .5)) return;
      FB(a, i, 0, .86, 2.05, 1.34, WHITE); FB(a, i, 2.05, .92, .28, 1.36, C.teal); FB(a + sg * .44, i, .0, .02, .15, 1.3, [.2, .2, .22]);
      glass(a + sg * .44, i0 + .05, i0 + .64, .2, 1.95, sg > 0 ? pv : pv.map(v => -v)); glass(a + sg * .44, i0 + .68, i0 + 1.27, .2, 1.95, sg > 0 ? pv : pv.map(v => -v));
      for (let s = 0; s < 4; s++) for (let k = 0; k < 4; k++) FB(a + sg * .2, i0 + .15 + k * .32, .3 + s * .42, .2, .2, .16, pick([[.85, .85, .8], [.3, .45, .6], [.7, .2, .15], [.85, .7, .3]], h(i0, s, k)));
      FH(a - .45, a + .45, i0, i0 + 1.34, 2.3); FO(a, i, .8);
    });
    sideRun(aMax, -1, (a, i, sg, i0) => {
      if (!FF(a, i, .5)) return;
      FB(a, i, 0, .86, .62, 1.3, WD); FB(a - sg * .1, i, .62, .66, .2, 1.26, WD); FB(a + sg * .2, i, .62, .4, .45, 1.26, WD);
      for (let k = 0; k < 8; k++) FB(a + (k % 2 ? sg * .2 : -sg * .1), i0 + .2 + Math.floor(k / 2) * .3, (k % 2 ? 1.07 : .82), .26, .1, .24, pick([[.6, .2, .16], [.42, .5, .24], C.ochre, [.55, .3, .45], [.3, .45, .2]], h(i0, k)));
      FH(a - .45, a + .45, i0, i0 + 1.3, 1.1); FO(a, i, .8);
    });
    // aisles of stocked shelving between the customers' places, running in from the door; signs hang over them
    const cas = Cu.map(c => c[0]).sort((p, q) => p - q), ais = [];
    for (let k = 0; k + 1 < cas.length; k++) ais.push((cas[k] + cas[k + 1]) / 2);
    if (cas.length) ais.push(cas[0] - 2.6, cas[cas.length - 1] + 2.6);
    const i0 = 2.2, i1 = Math.min(ci - 2.1, iMax - 1.5);
    for (const a of ais) {
      if (Math.abs(a) < dw / 2 + 1.3 || a < aMin + 2.0 || a > aMax - 2.0 || i1 - i0 < 1.6 || !FFrun(a, i0, i1, .55)) continue;
      const im = (i0 + i1) / 2, Lg = i1 - i0;
      FB(a, im, 0, 1.0, .12, Lg, [.3, .3, .32]); FB(a, im, .12, .06, 1.5, Lg, WHITE);
      for (let s = 0; s < 4; s++) {
        const y = .22 + s * .38; FB(a, im, y, 1.0, .03, Lg, WHITE);
        for (const sd of [-1, 1]) for (let g = 0; g < Math.floor(Lg / .3); g++) FB(a + sd * .28, i0 + .15 + g * .3, y + .03, .34, .12 + .16 * h(a, s, g, sd), .25, pick(ROOM_GOODS, h(a, g, s, sd)));
      }
      FB(a, i0, 0, 1.04, 1.62, .06, C.teal); FB(a, i0 - .04, 1.2, .8, .3, .02, C.cream);
      const p = wp(a, im); dm.box(p[0] - .01, GF - .5, p[1] - .01, p[0] + .01, GF, p[1] + .01, C.charcoal, 19); FB(a, im, GF - F - .78, .05, .28, 1.3, C.teal); FB(a, im, GF - F - .72, .07, .14, 1.0, C.cream);
      FH(a - .5, a + .5, i0, i1, 1.65); for (let i = i0; i <= i1; i += .9) FO(a, i, .6);
    }
    // stocked shelving along the back wall behind the checkouts
    if (iMax - ci > 1.6) for (let a = aMin + 1.2; a < aMax - 1.0; a += 2.0) { if (!FF(a, iMax - .3, .4)) continue; FB(a, iMax - .25, 0, 1.9, 1.9, .45, WD); for (let s = 0; s < 4; s++) for (let k = 0; k < 6; k++) FB(a - .8 + k * .32, iMax - .45, .2 + s * .44, .24, .12 + .14 * h(a, s, k), .1, pick(ROOM_GOODS, h(k, s, a))); FH(a - .95, a + .95, iMax - .5, iMax, 1.9); FO(a, iMax - .3, .7); }
  }

  if (key === "home") {
    // one home round each bed: a wardrobe at the back wall, a rug and lamp by the armchair, a kitchenette and a small table
    const beds = ob("bed"), rooms = ob("room");
    beds.forEach(([ba, bi], k) => {
      const wa = ba + 1.5;
      if (bi > iMax - 2.8 && FF(wa, iMax - .35, .55)) { FB(wa, iMax - .32, 0, 1.1, 2.0, .58, WD); FB(wa, iMax - .62, .1, .02, 1.85, .02, mul3(WD, .6)); FB(wa, iMax - .32, 2.0, 1.16, .07, .62, mul3(WD, .8)); FB(wa - .1, iMax - .64, 1.0, .04, .15, .04, C.ochre); FB(wa + .1, iMax - .64, 1.0, .04, .15, .04, C.ochre); FH(wa - .55, wa + .55, iMax - .62, iMax, 2.0); FO(wa, iMax - .35, .7); }
    });
    rooms.forEach(([ra, ri], k) => {
      const p = wp(ra, ri); dm.flat(p[0] - 1.25, p[1] - 1.0, p[0] + 1.25, p[1] + 1.0, F + .004, mul3(accent, .7), 19); dm.flat(p[0] - 1.1, p[1] - .85, p[0] + 1.1, p[1] + .85, F + .006, mul3(pick(ROOM_GOODS, h(k, "rug")), .85), 19);
      const ta = ra + 1.0; if (FF(ta, ri, .35)) { FB(ta, ri, .38, .72, .05, .5, WD); for (const [u, v] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) FB(ta + u * .3, ri + v * .2, 0, .04, .38, .04, WD); FB(ta, ri, .43, .26, .04, .18, pick(ROOM_GOODS, h(k, "bk"))); FB(ta + .2, ri + .1, .43, .08, .1, .08, [.9, .88, .84]); FH(ta - .36, ta + .36, ri - .25, ri + .25, .43); FO(ta, ri, .45); }
      const la = ra - .8; if (FF(la, ri + .4, .2)) { FB(la, ri + .4, 0, .28, .03, .28, C.metal); FB(la, ri + .4, 0, .03, 1.45, .03, C.metal); FB(la, ri + .4, 1.4, .38, .32, .38, C.cream); FB(la, ri + .4, 1.46, .14, .12, .14, [1, .8, .55], 2); FO(la, ri + .4, .3); }
    });
    beds.forEach(([ba], k) => {
      let ka = ba; if (Math.abs(ka) < dw / 2 + 1.4) ka += ka >= 0 ? 2.4 : -2.4;
      if (!FF(ka, .65, .85)) return;
      FB(ka, .62, 0, 1.8, .88, .58, [.8, .78, .72]); FB(ka, .62, .88, 1.84, .05, .62, [.4, .36, .32]); FB(ka - .45, .62, .93, .5, .02, .36, C.metal); FB(ka - .45, .45, .93, .04, .3, .04, C.metal);
      FB(ka + .45, .62, .93, .56, .03, .48, [.15, .15, .16]); for (const u of [-.13, .13]) FB(ka + .45 + u, .62, .96, .16, .02, .16, [.3, .3, .3]);
      for (const u of [-.6, 0, .6]) FB(ka + u, .34, .15, .02, .65, .02, mul3([.8, .78, .72], .7));
      FB(ka, .5, 1.5, 1.8, .6, .32, WD); FH(ka - .9, ka + .9, .33, .92, .93); FO(ka, .65, .95);
      if (FF(ka, 2.0, .9)) { table(...wp(ka, 2.0), .8, .74, false); const q = wp(ka, 2.0); hard(q[0] - .4, F, q[1] - .4, q[0] + .4, F + .74, q[1] + .4); for (const sg of [-1, 1]) { const c = wp(ka + sg * .62, 2.0); chair(c[0], c[1], yawOf(-sg * pv[0], -sg * pv[2]), fab); } FO(ka, 2.0, 1.0); }
    });
    const bs = beds.map(b => b[0]).sort((p, q) => p - q);
    for (let k = 0; k + 1 < bs.length; k++) if (bs[k + 1] - bs[k] > 5.5) {
      const ma = (bs[k] + bs[k + 1]) / 2, di0 = Math.max(iMin + 2.5, iMax - 6.5), dim = (di0 + iMax) / 2;
      if (!FFrun(ma, di0, iMax - .2, .1)) continue;
      FB(ma, dim, 0, .1, 1.2, iMax - di0, mul3(WD, .9)); FB(ma, dim, 1.2, .16, .05, iMax - di0, WD); FH(ma - .06, ma + .06, di0, iMax, 1.25);
    }
  }

  if (key === "works") {
    // a hoist rail under the beams; timber racks down one side wall, bar stock and sheets down the other; machines between the benches
    FB((aMin + aMax) / 2, (iMin + iMax) / 2, GF - .5, aMax - aMin - 1, .16, .14, C.metal);
    FB((aMin + aMax) / 2, (iMin + iMax) / 2, GF - 1.4, .03, .9, .03, C.charcoal); FB((aMin + aMax) / 2, (iMin + iMax) / 2, GF - 1.5, .1, .12, .06, C.metal);
    for (const [aw, sg] of [[aMin, 1], [aMax, -1]]) for (let i = 2.0; i < iMax - 1.6; i += 2.4) {
      const a = aw + sg * .45; if (!FF(a, i + 1.1, .5)) continue;
      for (const u of [0, 2.2]) FB(a, i + u, 0, .1, 2.0, .1, C.metal);
      for (let s = 0; s < 3; s++) { FB(a, i + 1.1, .4 + s * .6, .8, .04, 2.3, C.metal); for (let k = 0; k < 4; k++) FB(a + (k - 1.5) * .18, i + 1.1, .44 + s * .6, .14, .1, 2.2, sg > 0 ? mul3(WD, .9 + .2 * h(i, s, k)) : [.4, .42, .44]); }
      FH(a - .45, a + .45, i, i + 2.2, 2.0); FO(a, i + 1.1, 1.1);
    }
    const rows = {}; for (const [wa, wi] of Wk) (rows[wi.toFixed(1)] = rows[wi.toFixed(1)] || []).push(wa);
    for (const [wi, as] of Object.entries(rows)) { as.sort((p, q) => p - q); for (let k = 0; k + 1 < as.length; k++) { const ma = (as[k] + as[k + 1]) / 2, mi = +wi - .4; if (!FF(ma, mi, .9)) continue; FB(ma, mi, 0, 1.4, .95, .8, [.3, .34, .32]); FB(ma - .3, mi, .95, .5, .55, .6, [.3, .34, .32]); FB(ma + .4, mi, .95, .5, .2, .5, C.metal); FB(ma + .55, mi - .3, 1.0, .06, .35, .2, C.ochre); FH(ma - .7, ma + .7, mi - .4, mi + .4, 1.5); FO(ma, mi, 1.0); } }
  }

  if (key === "office" || key === "hall" || key === "guild") {
    // a typewriter and papers on every desk; the town hall gets a public counter and benches, the guild banners, the offices a water cooler
    for (const [wa, wi] of Wk) { FB(wa - .15, wi - .8, .76, .42, .1, .3, [.15, .15, .16]); FB(wa - .15, wi - .68, .86, .38, .14, .03, [.9, .9, .86]); FB(wa + .35, wi - .85, .76, .2, .08, .26, [.86, .84, .78]); }
    if (key === "hall" && Wk.length) {
      const ci = Math.min(...Wk.map(w => w[1])) - 2.6;
      for (const [s0, s1] of [[aMin + 1.5, -dw / 2 - 1.2], [dw / 2 + 1.2, aMax - 1.5]]) if (s1 - s0 > 2 && FFrun((s0 + s1) / 2, ci - .3, ci + .3, .2)) { FB((s0 + s1) / 2, ci, 0, s1 - s0, 1.05, .6, DARK); FB((s0 + s1) / 2, ci, 1.05, s1 - s0 + .1, .05, .72, TOP); for (let a = s0 + .8; a < s1; a += 2.2) { FB(a, ci, 1.1, .05, .5, .05, C.metal); FB(a, ci + .05, 1.4, .3, .22, .03, C.cream); } FH(s0, s1, ci - .3, ci + .3, 1.1); for (let a = s0; a < s1; a += 1) FO(a, ci, .6); }
      for (const sg of [-1, 1]) for (let r = 0; r < 2; r++) { const a = sg * (dw / 2 + 3.5), i = 1.8 + r * 1.6; if (!FF(a, i, 1.3)) continue; FB(a, i, .42, 2.4, .06, .45, WD); FB(a, i + .2, .48, 2.4, .4, .06, WD); for (const u of [-1.1, 1.1]) FB(a + u, i, 0, .08, .42, .4, WD); FH(a - 1.2, a + 1.2, i - .25, i + .25, .5); FO(a, i, 1.3); }
      for (const sg of [-1, 1]) { const a = sg * 3.2, i = iMax - .5; if (!FF(a, i, .3)) continue; FB(a, i, 0, .3, .1, .3, C.stoneDark); FB(a, i, 0, .04, 2.6, .04, C.metal); FB(a + .3, i, 1.6, .6, .9, .02, sg < 0 ? C.terracotta : C.teal); FO(a, i, .5); }
      FB(0, iMax - .03, 1.5, 1.3, 1.1, .05, [.3, .2, .12]); FB(0, iMax - .06, 1.58, 1.1, .94, .02, [.42, .34, .3]);
    }
    if (key === "guild") for (const [aw, sg] of [[aMin, 1], [aMax, -1]]) for (let i = 2.5; i < iMax - 1; i += 3.0) { FB(aw + sg * .02, i, 1.0, .03, 1.6, .7, pick([C.terracotta, C.teal, C.ochre], h(i, sg))); FB(aw + sg * .04, i, 2.55, .05, .06, .85, C.metal); FB(aw + sg * .04, i, 1.5, .02, .3, .3, C.ochre); }
    if (key === "office") { const a = aMax - .6, i = 1.6; if (FF(a, i, .35)) { FB(a, i, 0, .4, .95, .4, WHITE); FB(a, i, .95, .3, .45, .3, [.55, .7, .82]); FB(a, i - .21, .8, .06, .06, .04, C.metal); FO(a, i, .4); } }
  }

  if (key === "clinic") {
    // a low table with magazines before the waiting chairs; an examination bay with a couch, screens, a sink and a cabinet at the back
    if (Ch.length) { const ma = Ch.reduce((s, c) => s + c[0], 0) / Ch.length, mi = Math.min(...Ch.map(c => c[1])) - 1.0; if (FF(ma, mi, .6)) { FB(ma, mi, .36, 1.2, .05, .55, WD); for (const [u, v] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) FB(ma + u * .55, mi + v * .22, 0, .04, .36, .04, WD); for (let k = 0; k < 3; k++) FB(ma - .35 + k * .3, mi, .41, .22, .02, .3, pick([C.teal, C.terracotta, C.cream], h(k, "mag"))); FH(ma - .6, ma + .6, mi - .28, mi + .28, .41); FO(ma, mi, .8); } }
    const recA = Wk.length ? Wk[0][0] : 0, ea = recA > 0 ? aMin + 2.0 : aMax - 2.0, ei = iMax - 1.4;
    if (FF(ea, ei, 1.2)) {
      FB(ea, ei, 0, 1.95, .6, .7, WHITE); FB(ea, ei, .6, 1.9, .12, .66, [.92, .92, .9]); FB(ea + .7, ei, .72, .45, .1, .32, [.95, .95, .93]);
      for (let k = 0; k < 3; k++) FB(ea - .9 + k * .9, ei - 1.0, 0, .86, 1.75, .04, [.82, .8, .72], 0, k === 1 ? 0 : (k - 1) * .3);
      FB(ea - 1.6, iMax - .3, 0, .7, .85, .5, WHITE); FB(ea - 1.6, iMax - .3, .85, .45, .04, .35, C.metal); FB(ea + 1.5, iMax - .25, 0, .9, 1.85, .4, WHITE); FB(ea + 1.5, iMax - .46, 1.45, .08, .3, .02, C.teal); FB(ea + 1.5, iMax - .46, 1.56, .3, .08, .02, C.teal);
      FH(ea - 1.0, ea + 1.0, ei - .35, ei + .35, .75); FO(ea, ei, 1.6);
    }
  }

  if (key === "depot") {
    // a tram bay down one side: rails, a pit, a tram standing over it
    const ta = aMin + 2.0, ti0 = 1.6, ti1 = iMax - 1.2;
    if (ti1 - ti0 > 6 && FFrun(ta, ti0 + 1, ti1 - 1, 1.2)) {
      for (const u of [-.72, .72]) FB(ta + u, (ti0 + ti1) / 2, 0, .08, .05, ti1 - ti0, C.metal);
      const tl = Math.min(11, ti1 - ti0 - 1), tm = (ti0 + ti1) / 2;
      FB(ta, tm, .35, 2.4, 1.0, tl, C.terracotta); FB(ta, tm, 1.35, 2.4, 1.4, tl, C.cream); FB(ta, tm, 2.75, 2.5, .2, tl + .1, C.metal);
      for (let k = 0; k < Math.floor(tl / 1.9); k++) for (const sg of [-1, 1]) glass(ta + sg * 1.21, tm - tl / 2 + .4 + k * 1.9, tm - tl / 2 + 1.8 + k * 1.9, 1.5, 2.55, sg > 0 ? pv : pv.map(v => -v));
      FH(ta - 1.25, ta + 1.25, tm - tl / 2, tm + tl / 2, 2.95); for (let i = ti0; i < ti1; i += 1.5) FO(ta, i, 1.6);
    }
  }

  if (key === "mall") {
    // a shop booth at each trader's place: shelves behind, a canopy and a name board over the counter; a fountain in the court
    for (const [wa, wi] of Wk) {
      const col = pick([C.teal, C.terracotta, C.ochre, C.blue], h(wa, "bo"));
      FB(wa, wi + .6, 0, 3.2, 2.2, .45, WD); for (let s = 0; s < 4; s++) for (let k = 0; k < 9; k++) FB(wa - 1.4 + k * .35, wi + .38, .25 + s * .5, .28, .14 + .14 * h(wa, s, k), .12, pick(ROOM_GOODS, h(k, s, wa)));
      for (const u of [-1.6, 1.6]) for (const v of [-1.2, .8]) FB(wa + u, wi + v, 0, .08, 2.6, .08, C.metal);
      FB(wa, wi - .2, 2.6, 3.5, .12, 2.2, col); FB(wa, wi - 1.32, 2.2, 3.4, .4, .06, col); FB(wa, wi - 1.36, 2.28, 2.2, .22, .02, C.cream);
      FH(wa - 1.6, wa + 1.6, wi + .35, wi + .85, 2.2); FO(wa, wi + .6, 1.6);
    }
    const fa = 0, fi = (Math.min(...(Cu.length ? Cu : [[0, iMax / 2]]).map(c => c[1])) + 2.0) / 2 + .5;
    if (FF(fa, fi, 1.8)) { FB(fa, fi, 0, 3.0, .45, 3.0, C.stone, 0, Math.PI / 8); FB(fa, fi, 0, 3.0, .45, 3.0, C.stone, 0, -Math.PI / 8 + Math.PI / 4); FB(fa, fi, .38, 2.5, .04, 2.5, C.water, 9); FB(fa, fi, 0, .3, 1.4, .3, C.stone); FB(fa, fi, 1.4, .9, .12, .9, C.stoneDark, 0, Math.PI / 4); FH(fa - 1.5, fa + 1.5, fi - 1.5, fi + 1.5, .45); FO(fa, fi, 2.0); }
  }

  const backN = [o[0], 0, o[1]], onBack = (a, y, d) => { const p = wp(a, iMax - (d || .02)); return [p[0], F + y, p[1]]; };
  const counterRow = (bodyC, topC, ht) => {               // one long counter across the back joining the staff places; returns [a0, a1, ci]
    const wi = Math.max(...Wk.map(w => w[1])), ci = wi - .8, a0 = Math.max(aMin + .4, Math.min(...Wk.map(w => w[0])) - 1.6), a1 = Math.min(aMax - .4, Math.max(...Wk.map(w => w[0])) + 1.6), am = (a0 + a1) / 2, L = a1 - a0;
    FB(am, ci, 0, L, ht, .6, bodyC); FB(am, ci, ht, L + .08, .05, .74, topC);
    for (let k = 0; k < Math.floor(L / 1.0); k++) FB(a0 + .5 + k, ci - .31, .12, .86, ht - .25, .02, mul3(bodyC, .8));
    FH(a0, a1, ci - .35, ci + .35, ht + .05); for (let a = a0 + .5; a < a1; a += 1.2) FO(a, ci, .7);
    return [a0, a1, ci];
  };

  if (key === "bank" && Wk.length) {
    // the tellers' counter with glass screens and a brass tray at each window; the vault door and a clock on the back wall;
    // queue posts with ropes before the counter; writing desks by the door
    const [a0, a1, ci] = counterRow(DARK, [.74, .72, .66], 1.05);
    FB((a0 + a1) / 2, ci, 1.1, a1 - a0, .85, .03, [.6, .7, .76]); FB((a0 + a1) / 2, ci, 1.95, a1 - a0 + .1, .25, .14, DARK);
    for (const [wa] of Wk) { FB(wa - .6, ci, 1.1, .06, .85, .08, [.55, .45, .2]); FB(wa, ci - .05, 1.1, .45, .05, .14, [.55, .45, .2]); FB(wa, ci - .04, 1.12, .3, .12, .02, [.12, .12, .12]); }
    const va = (a0 + a1) / 2;
    if (iMax - ci > 1.1) { SP.disc(dm, onBack(va, 1.25, .04), backN, 1.15, [.32, .34, .36], 9, 28); SP.disc(dm, onBack(va, 1.25, .07), backN, .9, [.42, .44, .46], 9, 28); SP.disc(dm, onBack(va, 1.25, .1), backN, .16, [.55, .45, .2], 9, 12); for (let k = 0; k < 4; k++) FB(va, iMax - .12, 1.21, .06, .06, .02, [.55, .45, .2], 0, k * Math.PI / 4); }
    SP.disc(dm, onBack(va + 3.5, 2.5, .04), backN, .42, DARK, 19); SP.disc(dm, onBack(va + 3.5, 2.5, .06), backN, .34, [.9, .88, .82], 19);
    const qi = ci - 3.0; if (qi > 2) for (let a = -6; a <= 6; a += 1.5) { if (Math.abs(a) < dw / 2 + .4 || !FF(a, qi, .15)) continue; FB(a, qi, 0, .22, .04, .22, [.55, .45, .2]); FB(a, qi, 0, .05, .95, .05, [.55, .45, .2]); if (a + 1.5 <= 6 && Math.abs(a + .75) >= dw / 2 + .4) FB(a + .75, qi, .82, 1.4, .05, .05, [.5, .1, .1]); FO(a, qi, .3); }
    for (const sg of [-1, 1]) { const a = sg * (dw / 2 + 3.0), i = 2.2; if (!FF(a, i, .7)) continue; for (const u of [-.4, .4]) FB(a + u, i, 0, .06, 1.0, .06, C.metal); FB(a, i, 1.0, 1.1, .05, .55, [.3, .2, .13], 0); for (let k = 0; k < 3; k++) FB(a - .3 + k * .3, i, 1.05, .2, .01, .28, [.9, .88, .82]); FH(a - .55, a + .55, i - .28, i + .28, 1.05); FO(a, i, .8); }
  }

  if (key === "library") {
    // stacks of double-sided bookcases between the readers' tables and the browsers; green lamps on every table;
    // a card catalogue and a globe by the door (the issue desk is the counter at the librarians' places)
    const Se = ob("seat"), si = Se.length ? Math.max(...Se.map(s => s[1])) : 6, cuI = Cu.length ? Math.min(...Cu.map(c => c[1])) : iMax - 4, sti = (si + cuI) / 2;
    if (cuI - si > 2.4) for (let a = aMin + 1.7; a + 2.6 < aMax - 1.0; a += 3.6) {
      const m = a + 1.3; if (Math.abs(m) < dw / 2 + 1.6 || !FF(m, sti, .45) || !FF(a + .2, sti, .3) || !FF(a + 2.4, sti, .3)) continue;
      FB(m, sti, 0, 2.6, 2.1, .7, WD); FB(m, sti, 2.1, 2.7, .06, .74, mul3(WD, .8));
      for (let s = 0; s < 5; s++) for (const sd of [-1, 1]) { FB(m, sti + sd * .31, .1 + s * .42, 2.5, .03, .1, mul3(WD, .9)); for (let k = 0; k < 14; k++) { const bh = .24 + .1 * h(m, s, k, sd); FB(a + .15 + k * .17, sti + sd * .3, .13 + s * .42, .13, bh, .12, pick(ROOM_GOODS, h(k, s, m, sd))); } }
      FH(a, a + 2.6, sti - .35, sti + .35, 2.1); FO(m, sti, 1.5);
    }
    (l.objects.seat || []).forEach(([x, z], k) => { const [ta, ti] = rel([x + (k % 2 ? -1 : 1) * .7, z]); FB(ta, ti, .74, .14, .03, .14, [.55, .45, .2]); FB(ta, ti, .74, .02, .3, .02, [.55, .45, .2]); FB(ta, ti, 1.02, .28, .1, .14, [.16, .36, .22]); FB(ta, ti, .98, .1, .05, .06, [1, .8, .55], 2); });
    const ca = dw / 2 + 2.2; if (FF(ca, 1.2, .7)) { FB(ca, 1.2, 0, 1.2, 1.15, .5, WD); for (let r = 0; r < 4; r++) for (let q = 0; q < 4; q++) FB(ca - .42 + q * .28, 1.2 - .26, .15 + r * .25, .22, .18, .02, mul3(WD, .75)); FH(ca - .6, ca + .6, .95, 1.45, 1.15); FO(ca, 1.2, .8); }
    const ga = -(dw / 2 + 2.2); if (FF(ga, 1.4, .5)) { FB(ga, 1.4, 0, .5, .04, .5, WD, 0, Math.PI / 4); FB(ga, 1.4, 0, .05, .85, .05, WD); FB(ga, 1.4, .85, .5, .5, .5, C.blue, 0, Math.PI / 4); FB(ga, 1.4, .92, .36, .36, .36, C.olive, 0, Math.PI / 8); FO(ga, 1.4, .45); }
  }

  if (key === "museum") {
    // an exhibit before each visitor's place (a statue on a plinth, an urn, a glass case of finds) behind rope posts; a great
    // painting across the back wall
    const stone = [.72, .7, .66];
    Cu.forEach(([ca, ci], k) => {
      const ei = ci + 1.4; if (!FF(ca, ei, .32)) return; const t = k % 3;
      FB(ca, ei, 0, .8, 1.0, .8, stone);
      if (t === 0) { FB(ca, ei, 1.0, .5, .1, .4, stone); FB(ca - .1, ei, 1.1, .14, .7, .16, stone); FB(ca + .1, ei, 1.1, .14, .7, .16, stone); FB(ca, ei, 1.8, .42, .62, .26, stone); FB(ca, ei, 2.44, .2, .26, .22, stone); FB(ca + .3, ei, 1.9, .1, .5, .12, stone, 0, .4); }
      else if (t === 1) { FB(ca, ei, 1.0, .3, .1, .3, C.terracotta, 0, Math.PI / 4); FB(ca, ei, 1.1, .5, .38, .5, C.terracotta, 0, Math.PI / 4); FB(ca, ei, 1.48, .3, .22, .3, C.terracotta, 0, Math.PI / 4); FB(ca, ei, 1.7, .4, .06, .4, C.terracotta, 0, Math.PI / 4); FB(ca, ei, 1.3, .54, .06, .54, C.charcoal, 0, Math.PI / 4); }
      else { FB(ca, ei, 1.0, .74, .55, .74, [.62, .72, .76]); for (let q = 0; q < 3; q++) FB(ca - .2 + q * .2, ei, 1.02, .12, .05 + .06 * q, .16, pick([C.ochre, [.55, .45, .2], C.teal], h(k, q))); }
      for (const u of [-.75, .75]) { FB(ca + u, ei - .8, 0, .2, .04, .2, [.55, .45, .2]); FB(ca + u, ei - .8, 0, .05, .9, .05, [.55, .45, .2]); } FB(ca, ei - .8, .8, 1.5, .05, .05, [.5, .1, .1]);
      FH(ca - .45, ca + .45, ei - .45, ei + .45, 1.6); FO(ca, ei, .9);
    });
    const pw = Math.min(6, aMax - aMin - 6); FB(0, iMax - .05, .9, pw + .3, 2.1, .06, [.42, .3, .14]); FB(0, iMax - .1, 1.05, pw, 1.8, .03, [.36, .42, .5]); FB(-pw * .2, iMax - .12, 1.2, pw * .4, .7, .02, [.5, .44, .3]); FB(pw * .15, iMax - .12, 1.6, pw * .35, .9, .02, [.3, .36, .24]);
  }

  if (key === "cinema") {
    // the screen on the back wall between red curtains, a stage before it, a red runner up the aisle, wall lights low and
    // warm; ticket and sweets counters by the door (the counters at the staff places), a popcorn machine on one
    const sw = Math.min(aMax - aMin - 4, 14), red = [.5, .1, .1];
    dm.quad(onBack(-sw / 2, .95, .06), onBack(sw / 2, .95, .06), onBack(sw / 2, 2.95, .06), onBack(-sw / 2, 2.95, .06), [.3, .3, .9995], 1, backN);   // (across and up kept off the glazing bars)
    for (const s of [-1, 1]) FB(s * (sw / 2 + .55), iMax - .2, 0, 1.1, 3.0, .26, red); FB(0, iMax - .2, 2.95, sw + 2.2, .25, .26, red);
    FB(0, iMax - 1.0, 0, sw + 1, .42, 1.5, DARK); FH(-(sw + 1) / 2, (sw + 1) / 2, iMax - 1.75, iMax - .25, .42); FO(0, iMax - 1.0, 2.5);
    { const p0 = wp(-.55, 3.0), p1 = wp(.55, iMax - 1.8); dm.flat(Math.min(p0[0], p1[0]), Math.min(p0[1], p1[1]), Math.max(p0[0], p1[0]), Math.max(p0[1], p1[1]), F + .005, [.36, .08, .08], 19); }
    for (const [aw, sg] of [[aMin, 1], [aMax, -1]]) for (let i = 3; i < iMax - 1; i += 3.0) { FB(aw + sg * .08, i, 2.1, .14, .24, .24, [1, .72, .42], 2); FB(aw + sg * .05, i, 2.0, .08, .1, .3, [.55, .45, .2]); }
    if (Wk.length) { const [wa, wi] = Wk[0]; FB(wa + .45, wi - .8, .98, .5, .62, .42, [.95, .82, .5], 2); FB(wa + .45, wi - .8, 1.6, .54, .06, .46, red); }
  }

  if (key === "dealer") {
    // cars on show between the buyers' places, each with a price board on a stand; tyres and oil by the back wall; a brochure rack
    const colors = [C.teal, C.terracotta, C.cream, C.blue, C.olive, [.52, .16, .14]];
    const car = (a, i, col) => {
      FB(a, i, .32, 1.8, .5, 4.2, col); FB(a, i - .15, .82, 1.62, .48, 2.2, col); FB(a, i - .15, .86, 1.64, .36, 2.0, [.32, .42, .48]);
      FB(a, i + 1.8, .6, 1.7, .14, .5, col); for (const u of [-1, 1]) for (const v of [-1.35, 1.35]) FB(a + u * .82, i + v, 0, .26, .64, .64, C.charcoal);
      for (const v of [-2.12, 2.12]) FB(a, i + v, .34, 1.74, .14, .1, C.metal); for (const u of [-.6, .6]) FB(a + u, i - 2.16, .55, .26, .14, .04, [1, .9, .7], 2);
      FB(a + 1.4, i - 1.6, 0, .04, 1.0, .04, C.metal); FB(a + 1.4, i - 1.6, 1.0, .5, .36, .04, C.cream);
      FH(a - .9, a + .9, i - 2.1, i + 2.1, 1.3); FO(a, i, 2.3);
    };
    const cas = Cu.map(c => c[0]).sort((p, q) => p - q), ci = Cu.length ? Cu[0][1] : 6;
    let n = 0;
    for (let k = 0; k + 1 < cas.length; k++) { const m = (cas[k] + cas[k + 1]) / 2; if (Math.abs(m) < dw / 2 + 1.4 || !FF(m, ci + .6, 1.0) || !FF(m, ci - 1.2, .8) || !FF(m, ci + 2.4, .8)) continue; car(m, ci + .6, colors[n++ % colors.length]); }
    for (let a = aMin + 2.5; a < aMax - 2; a += 5.5) { if (Math.abs(a) > dw / 2 + 1.4 && FF(a, ci + 5.2, 1.0) && FF(a, ci + 3.4, .8) && FF(a, ci + 7.0, .8)) car(a, ci + 5.2, colors[n++ % colors.length]); }
    for (let a = aMin + 1.0; a < aMax - 1; a += 4.0) { if (!FF(a, iMax - .5, .5)) continue; for (let k = 0; k < 4; k++) FB(a, iMax - .5, k * .24, .62, .22, .62, C.charcoal, 0, k * .3); FB(a + .7, iMax - .4, 0, .3, .45, .3, C.ochre); FO(a, iMax - .5, .7); }
    const ra = -(dw / 2 + 1.8); if (FF(ra, 1.2, .5)) { FB(ra, 1.2, 0, .7, 1.4, .3, WD); for (let r = 0; r < 3; r++) for (let q = 0; q < 3; q++) FB(ra - .22 + q * .22, 1.04, .3 + r * .38, .18, .26, .02, pick(colors, h(r, q, "br"))); FO(ra, 1.2, .5); }
  }
}

function room(l, side, F, dm, occ, R, accent, fab, chair, table, B, hard) {
  const [x0, z0, x1, z1] = l.rect, t = .35, rx0 = x0 + t, rx1 = x1 - t, rz0 = z0 + t, rz1 = z1 - t, d = DOOR_DIR[side], [dx, dz] = l.door, dw = l.dw;
  for (let s = 0; s < 5; s++) occ.push([dx - d[0] * s * 1.2, dz - d[1] * s * 1.2, dw / 2 + .7]);        // keep the way in clear
  if (l.tower) { const bx = side === "e" ? x0 + 1.5 : side === "w" ? x1 - 1.5 : x0 + 1.5, bz = side === "s" ? z0 + 1.5 : side === "n" ? z1 - 1.5 : z0 + 1.5; occ.push([bx, bz, 3.2]); }
  const clear = (x, z, r) => occ.every(([ox, oz, or]) => Math.hypot(x - ox, z - oz) > r + or);   // nothing placed or used there
  const free = (x, z, r) => x > rx0 + r * .7 && x < rx1 - r * .7 && z > rz0 + r * .7 && z < rz1 - r * .7 && occ.every(([ox, oz, or]) => Math.hypot(x - ox, z - oz) > r + or);
  const walls = { n: { ns: true, fix: rz0, out: 1 }, s: { ns: true, fix: rz1, out: -1 }, w: { ns: false, fix: rx0, out: 1 }, e: { ns: false, fix: rx1, out: -1 } };
  const W = ROOM_WOOD, h = (...k) => hash(l.id, "room", ...k); let menuDone = 0;
  const box = (S, a, y0, y1, d0, d1, w, c, f) => fbox(dm, S, a - w / 2, a + w / 2, F + y0, F + y1, d0, d1, c, f || 19, false, true);
  const solidOn = (S, a, w, dep, ht) => { const p = SP.at(S, a - w / 2, 0, 0), q = SP.at(S, a + w / 2, 0, dep); hard(p[0], F, p[2], q[0], F + ht, q[2]); };
  // wall pieces: [width, depth, height, build(S, a)]
  const P = {
    wardrobe: [1.2, .6, 2.1, (S, a) => { box(S, a, 0, 2.1, 0, .6, 1.2, W); box(S, a, .1, 2.0, .6, .61, .02, mul3(W, .6)); for (const u of [-.1, .1]) box(S, a + u, 1.0, 1.15, .6, .64, .04, C.ochre); box(S, a, 2.1, 2.18, -.02, .64, 1.26, mul3(W, .8)); }],
    bookcase: [1.3, .34, 2.0, (S, a) => { box(S, a, 0, 2.0, 0, .05, 1.3, W); for (const u of [-.63, .63]) box(S, a + u, 0, 2.0, 0, .34, .04, W); for (let k = 0; k < 5; k++) { box(S, a, .05 + k * .46, .09 + k * .46, 0, .34, 1.22, W); if (k < 4) for (let b = 0, u = -.56; u < .52; b++) { const bw = .05 + .05 * h(a, k, b), bh = .24 + .12 * h(b, k, a); box(S, a + u + bw / 2, .09 + k * .46, .09 + k * .46 + bh, .04, .28, bw, pick(ROOM_GOODS, h(a, b, k))); u += bw + .01; } } }],
    dresser: [1.0, .5, 1.0, (S, a) => { box(S, a, 0, .95, 0, .5, 1.0, W); for (let k = 0; k < 3; k++) box(S, a, .12 + k * .28, .14 + k * .28, .5, .51, .92, mul3(W, .6)); box(S, a, 1.3, 2.0, 0, .04, .7, mul3(W, .7)); box(S, a, 1.36, 1.94, .04, .05, .58, [.55, .62, .66]); }],
    fireplace: [1.8, .45, 1.25, (S, a) => { box(S, a, 0, 1.15, 0, .4, 1.8, C.stone); box(S, a, .1, .8, .4, .41, .9, [.08, .06, .05]); box(S, a, .12, .3, .2, .38, .5, [1, .5, .2], 2); box(S, a, 1.15, 1.25, -.02, .5, 2.0, C.stoneDark); box(S, a, 1.6, 2.2, 0, .04, .9, mul3(W, .7)); box(S, a, 1.66, 2.14, .04, .05, .78, pick(ROOM_GOODS, h(a, "f"))); }],
    sideboard: [1.6, .45, .85, (S, a) => { box(S, a, 0, .85, 0, .45, 1.6, W); for (const u of [-.4, .4]) box(S, a + u, .1, .75, .45, .46, .7, mul3(W, .7)); box(S, a - .4, .85, 1.15, .15, .3, .16, pick([C.teal, C.ochre, C.terracotta], h(a, "v"))); box(S, a + .3, .85, .9, .1, .35, .4, [.86, .84, .78]); }],
    plant: [.6, .6, 1.3, (S, a) => { box(S, a, 0, .45, .1, .5, .4, C.terracotta); for (let k = 0; k < 5; k++) box(S, a + (h(a, k) - .5) * .3, .45 + k * .16, .75 + k * .16, .08 + h(k, a) * .2, .32 + h(k, a) * .2, .32, pick([[.26, .38, .2], [.36, .46, .24]], h(a, k, "p")), 19); }],
    bottles: [1.6, .3, 1.9, (S, a) => { box(S, a, 0, 1.9, 0, .04, 1.6, W); for (let k = 0; k < 4; k++) { box(S, a, .3 + k * .42, .33 + k * .42, 0, .3, 1.6, W); for (let b = 0; b < 9; b++) box(S, a - .7 + b * .17, .33 + k * .42, .33 + k * .42 + .24 + .06 * h(a, k, b), .1, .18, .06, pick([[.16, .3, .18], [.38, .2, .1], [.6, .52, .3], [.5, .14, .12]], h(b, k, a))); } }],
    pastry: [1.4, .6, 1.05, (S, a) => { box(S, a, 0, .8, 0, .6, 1.4, W); box(S, a, .8, 1.05, .05, .55, 1.3, [.95, .85, .62], 2); for (let b = 0; b < 5; b++) box(S, a - .5 + b * .25, .82, .9, .15, .45, .16, pick([C.ochre, [.55, .3, .18], C.cream], h(a, b))); }],
    crates: [1.2, .8, 1.1, (S, a) => { for (let k = 0; k < 2; k++) { box(S, a - .3 + k * .6, 0, .5, .05, .7, .56, W); box(S, a - .3 + k * .6, .5, .62, .1, .65, .46, pick(ROOM_GOODS, h(a, k))); } box(S, a, .62, 1.05, .1, .6, .55, mul3(W, .85)); }],
    workbench: [2.2, .7, .9, (S, a) => { box(S, a, .82, .9, 0, .7, 2.2, W); for (const u of [-1, 1]) box(S, a + u * 1.0, 0, .82, .05, .65, .08, W); box(S, a, .25, .3, .05, .65, 2.0, W); box(S, a + .8, .9, 1.05, .45, .65, .2, C.metal); box(S, a, 1.2, 2.2, 0, .03, 2.0, [.3, .22, .16]); for (let k = 0; k < 7; k++) box(S, a - .8 + k * .27, 1.4 + .3 * h(a, k), 1.75 + .3 * h(a, k), .03, .06, .05, C.metal); }],
    machine: [1.3, .9, 1.4, (S, a) => { box(S, a, 0, .9, .05, .85, 1.3, [.3, .34, .32]); box(S, a - .3, .9, 1.4, .2, .7, .5, [.3, .34, .32]); box(S, a + .35, .95, 1.1, .3, .6, .5, C.metal); box(S, a + .55, 1.0, 1.35, .2, .3, .06, C.ochre); }],
    barrels: [1.3, .65, .95, (S, a) => { for (const u of [-.32, .32]) { box(S, a + u, 0, .95, .05, .62, .56, W); for (const y of [.2, .75]) box(S, a + u, y, y + .05, .04, .63, .58, C.metal); } }],
    filing: [1.2, .6, 1.35, (S, a) => { for (const u of [-.3, .3]) { box(S, a + u, 0, 1.35, 0, .6, .56, [.42, .44, .44]); for (let k = 0; k < 4; k++) box(S, a + u, .15 + k * .32, .18 + k * .32, .6, .61, .3, C.metal); } }],
    medcab: [1.0, .45, 1.9, (S, a) => { box(S, a, 0, 1.9, 0, .45, 1.0, [.86, .86, .82]); box(S, a, .9, 1.8, .45, .46, .86, [.55, .62, .66]); box(S, a, 1.45, 1.75, .46, .47, .08, C.teal); box(S, a, 1.56, 1.64, .46, .47, .3, C.teal); }],
    sink: [1.0, .55, .9, (S, a) => { box(S, a, 0, .85, 0, .55, 1.0, [.86, .86, .82]); box(S, a, .85, .9, .05, .5, .6, C.metal); box(S, a, .9, 1.15, .05, .1, .05, C.metal); box(S, a, 1.2, 1.9, 0, .03, .7, [.55, .62, .66]); }],
    bench: [1.8, .5, .9, (S, a) => { box(S, a, .42, .48, 0, .45, 1.8, R.chair || W); box(S, a, .48, .9, 0, .06, 1.8, R.chair || W); for (const u of [-.8, .8]) box(S, a + u, 0, .42, .05, .42, .06, C.metal); }],
    lockers: [1.4, .5, 1.85, (S, a) => { box(S, a, 0, 1.85, 0, .5, 1.4, [.32, .4, .4]); for (let k = 1; k < 4; k++) box(S, a - .7 + k * .35, .05, 1.8, .5, .51, .02, [.2, .26, .26]); for (let k = 0; k < 4; k++) box(S, a - .52 + k * .35, 1.5, 1.6, .5, .52, .18, [.2, .26, .26]); }],
    sofa: [2.0, .85, .85, (S, a) => { box(S, a, 0, .42, .05, .85, 2.0, fab); box(S, a, .42, .85, .05, .3, 2.0, fab); for (const u of [-.92, .92]) box(S, a + u, .42, .62, .05, .85, .16, fab); for (const u of [-.5, .5]) box(S, a + u, .42, .52, .3, .8, .9, mul3(fab, 1.12)); }],
  };
  layout(l, side, F, dm, occ, R, accent, fab, chair, table, B, hard, clear, walls, box, P);
  // along each wall: tall pieces where there is no window and it is not the door wall, low ones elsewhere; pictures above the low ones
  const winAt = (s, a, w) => (l.inWin || []).some(([ws, w0, w1]) => ws === s && a + w / 2 > w0 && a - w / 2 < w1);
  let placed = 0;
  for (const s of "nswe") {
    const S = walls[s], A0 = S.ns ? rx0 : rz0, A1 = S.ns ? rx1 : rz1, doorA = S.ns ? dx : dz, onDoor = s === side;
    for (let a = A0 + 1.3, k = 0; a < A1 - 1.0 && placed < 60; a += 2.7, k++) {
      if (onDoor && Math.abs(a - doorA) < dw / 2 + 1.6) continue;
      const tall = !onDoor && !winAt(s, a, 1.5) && R.tall.length && h(s, k) < .55, list = tall ? R.tall : R.low;
      if (!list.length || h(s, k, "skip") < .18) continue;
      const name = pick(list, h(s, k, "pick")), [w, dep, ht, build] = P[name], c = SP.at(S, a, 0, dep / 2 + .05);
      if (a - w / 2 < A0 + .2 || a + w / 2 > A1 - .2 || !clear(c[0], c[2], Math.max(w, dep) / 2)) continue;
      build(S, a); if (name !== "plant") solidOn(S, a, w, dep, ht); occ.push([c[0], c[2], Math.max(w, dep) / 2 + .3]); placed++;
      if (ht < 1.3 && name !== "fireplace" && !winAt(s, a, 1.0) && R.pictures) picture(S, a, k);
    }
  }
  function picture(S, a, k) {                             // something on the wall: a painting, the menu, price boards, a notice, tools
    const kind = R.pictures, y0 = F + 1.45;
    if (kind === "menu" && !menuDone) { menuDone = 1; fbox(dm, S, a - .6, a + .6, y0, y0 + .85, 0, .04, [.1, .1, .1], 19); for (let q = 0; q < 5; q++) fbox(dm, S, a - .45, a - .45 + .5 + .35 * h(a, q), y0 + .65 - q * .13, y0 + .69 - q * .13, .04, .05, [.85, .82, .74], 19); return; }
    if (kind === "tools") { fbox(dm, S, a - .5, a + .5, y0, y0 + .8, 0, .03, [.3, .22, .16], 19); for (let q = 0; q < 4; q++) fbox(dm, S, a - .38 + q * .25, a - .33 + q * .25, y0 + .15, y0 + .55 + .1 * h(a, q), .03, .07, C.metal, 19); return; }
    const w = kind === "notice" || kind === "price" ? .55 : .6 + .4 * h(a, k, "w"), ht = kind === "price" ? .4 : .5 + .3 * h(k, a, "h");
    fbox(dm, S, a - w / 2, a + w / 2, y0, y0 + ht, 0, .04, kind === "art" ? [.3, .2, .12] : [.2, .16, .12], 19);
    fbox(dm, S, a - w / 2 + .05, a + w / 2 - .05, y0 + .05, y0 + ht - .05, .04, .045, kind === "notice" ? [.86, .84, .78] : kind === "price" ? [.12, .12, .12] : pick(ROOM_GOODS, h(a, k, "c")), 19);
  }
  // open floor: what this kind of building has standing in the room
  const cx = (rx0 + rx1) / 2, cz = (rz0 + rz1) / 2, spots = [];
  for (let gx = rx0 + 2; gx < rx1 - 1.5; gx += 2.5) for (let gz = rz0 + 2; gz < rz1 - 1.5; gz += 2.5) spots.push([gx, gz, Math.hypot(gx - cx, gz - cz) + 3 * h(gx, gz)]);
  spots.sort((p, q) => p[2] - q[2]);
  const want = { dining: 1, rug: 1, produce: 4, kiosk: 6, crates: 2, machine: 2, reception: 1, screens: 2, beds: 6, benches: 3, long: 1, plant: 2 };
  for (const m of R.mid) {
    let n = Math.round((want[m] || 1) * Math.max(1, (rx1 - rx0) * (rz1 - rz0) / 160));     // bigger rooms hold more
    for (const [px, pz] of spots) {
      if (n <= 0) break;
      if (m === "rug") { if (!free(px, pz, .3)) continue; dm.flat(px - 1.4, pz - 1.0, px + 1.4, pz + 1.0, F + .004, mul3(accent, .8), 19); dm.flat(px - 1.25, pz - .85, px + 1.25, pz + .85, F + .006, mul3(pick(ROOM_GOODS, h(px, pz)), .9), 19); n--; continue; }
      const r = { dining: 1.9, produce: 1.4, kiosk: 2.2, crates: .9, machine: 1.0, reception: 1.6, screens: 1.2, beds: 1.4, benches: 1.6, long: 2.4, plant: .5 }[m];
      if (!free(px, pz, r)) continue;
      if (m === "dining") { table(px, pz, 1.1, .76, false); hard(px - .55, F, pz - .55, px + .55, F + .76, pz + .55); for (const [u, v] of [[0, -1], [0, 1], [-1, 0], [1, 0]]) chair(px + u * .8, pz + v * .8, yawOf(-u, -v), fab); }
      else if (m === "produce") { B(px, 0, pz, 2.2, .7, 1.0, ROOM_WOOD); for (let k = 0; k < 6; k++) { const u = -.75 + (k % 3) * .75, v = k < 3 ? -.25 : .25; B(px + u, .7, pz + v, .66, .12, .44, ROOM_WOOD); B(px + u, .82, pz + v, .58, .1, .38, pick(ROOM_GOODS, h(px, k))); } hard(px - 1.1, F, pz - .5, px + 1.1, F + .9, pz + .5); }
      else if (m === "kiosk") { for (const [u, v] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) B(px + u * .95, 0, pz + v * .95, .08, 2.3, .08, C.metal); B(px, 2.3, pz, 2.2, .12, 2.2, pick([C.teal, C.terracotta, C.ochre], h(px, pz))); B(px, 0, pz, 1.7, 1.0, 1.7, ROOM_WOOD); B(px, 1.0, pz, 1.8, .05, 1.8, [.62, .58, .52]); for (let k = 0; k < 4; k++) B(px - .5 + k * .33, 1.05, pz, .22, .2, .3, pick(ROOM_GOODS, h(px, k))); hard(px - .9, F, pz - .9, px + .9, F + 1.05, pz + .9); }
      else if (m === "crates") { for (let k = 0; k < 3; k++) B(px + (k - 1) * .62, 0, pz, .58, .55 + (k === 1 ? .55 : 0), .58, mul3(ROOM_WOOD, .85 + .2 * h(px, k))); hard(px - .9, F, pz - .3, px + .9, F + 1.1, pz + .3); }
      else if (m === "machine") { B(px, 0, pz, 1.4, .95, .8, [.3, .34, .32]); B(px - .3, .95, pz, .5, .55, .6, [.3, .34, .32]); B(px + .4, .95, pz, .5, .2, .5, C.metal); hard(px - .7, F, pz - .4, px + .7, F + 1.5, pz + .4); }
      else if (m === "reception") { B(px, 0, pz, 2.4, 1.05, .7, R.counter || ROOM_WOOD); B(px, 1.05, pz, 2.5, .05, .8, R.top || C.teal); B(px + .7, 1.1, pz, .14, .35, .14, C.metal); B(px + .7, 1.42, pz, .26, .1, .26, R.lamp || C.cream); B(px - .5, 1.1, pz, .4, .03, .3, [.86, .84, .78]); hard(px - 1.2, F, pz - .35, px + 1.2, F + 1.1, pz + .35); }
      else if (m === "screens") { for (let k = 0; k < 3; k++) { const yaw = (k - 1) * .5; B(px + Math.cos(yaw) * (k - 1) * .62, 0, pz + Math.sin(yaw) * (k - 1) * .62, .62, 1.75, .04, [.82, .8, .72], -yaw); } B(px, 0, pz + .9, .9, .55, 1.9, [.86, .86, .82]); B(px, .55, pz + .9, .86, .1, 1.8, [.92, .92, .9]); hard(px - .45, F, pz - .05, px + .45, F + .6, pz + 1.85); }
      else if (m === "beds") { B(px, 0, pz, .95, .55, 2.0, [.86, .86, .82]); B(px, .55, pz, .9, .12, 1.9, [.92, .92, .9]); B(px, .67, pz + .65, .6, .1, .3, [.95, .95, .93]); B(px, 0, pz - 1.05, .95, .95, .06, C.metal); hard(px - .48, F, pz - 1, px + .48, F + .67, pz + 1); }
      else if (m === "benches" || m === "long") { const L2 = m === "long" ? 3.4 : 2.4; B(px, .42, pz, L2, .06, .45, ROOM_WOOD); B(px, .48, pz + .2, L2, .4, .06, ROOM_WOOD); for (const u of [-1, 1]) B(px + u * (L2 / 2 - .1), 0, pz, .08, .42, .4, ROOM_WOOD); if (m === "long") { B(px, .74, pz - 1.1, L2, .05, 1.0, ROOM_WOOD); for (const u of [-1, 1]) B(px + u * (L2 / 2 - .2), 0, pz - 1.1, .1, .74, .9, ROOM_WOOD); hard(px - L2 / 2, F, pz - 1.6, px + L2 / 2, F + .78, pz - .6); } hard(px - L2 / 2, F, pz - .25, px + L2 / 2, F + .5, pz + .25); }
      else if (m === "plant") { B(px, 0, pz, .45, .45, .45, C.terracotta); for (let k = 0; k < 4; k++) B(px + (h(px, k) - .5) * .3, .45 + k * .2, pz + (h(k, pz) - .5) * .3, .5 - k * .07, .32, .5 - k * .07, [.28, .4, .21]); }
      occ.push([px, pz, r + .3]); n--;
    }
  }
  // lamps hanging from the beams
  const nx = Math.max(1, Math.round((rx1 - rx0) / 4.4)), nz = Math.max(1, Math.round((rz1 - rz0) / 4.4));
  if (!R.dim) for (let i = 0; i < nx; i++) for (let j = 0; j < nz; j++) {   // (a cinema stays dark)
    const lx = rx0 + (i + .5) * (rx1 - rx0) / nx, lz = rz0 + (j + .5) * (rz1 - rz0) / nz, top = GF - .02;
    if (ROOM_REC && dm !== NULL_MESH) ROOM_REC.lamps.push([lx, top - .84, lz, 1.4]);
    dm.box(lx - .012, top - .55, lz - .012, lx + .012, top, lz + .012, C.charcoal, 19);
    dm.ybox(lx, top - .78, lz, .42, .24, .42, Math.PI / 4, R.lamp || C.cream, 19); dm.box(lx - .09, top - .86, lz - .09, lx + .09, top - .76, lz + .09, [1, .78, .5], 2);
  }
}

function openLot(l) {
  const [x0, z0, x1, z1] = l.rect, o = l.objects, cx = (x0 + x1) / 2, cz = (z0 + z1) / 2, b = l.block;
  if (l.kind === "garden") {
    world.flat(x0, z0, x1, z1, Y_GRASS + .02, mul3(C.grass, 1.08), 0);
    for (const [x, z] of o.bench || []) spot(l, x, z, 2, Math.abs(z - cz) > Math.abs(x - cx) ? yawOf(0, cz - z) : yawOf(cx - x, 0), Y_YARD + .46);
    for (const [x, z] of o.rim || []) spot(l, x, z, 2, yawOf(x - cx, z - cz), Y_YARD + .5);
    for (const [x, z] of o.flowerbed || []) spot(l, x, z, 0, yawOf(0, -1), Y_YARD);
    labels.push({ x: cx, y: 3, z: cz, text: l.name, lot: l, r: 46 });
  } else if (l.kind === "market") {
    world.flat(x0, z0, x1, z1, Y_PATH, C.path, 7);
    for (const [x, z] of o.stall || []) spot(l, x, z, 0, yawOf(0, z < cz ? 1 : -1), Y_YARD);
    for (const [x, z] of o.customer || []) spot(l, x, z, 0, yawOf(0, z < cz ? -1 : 1), Y_YARD);
    labels.push({ x: cx, y: 5, z: cz, text: l.name, lot: l, r: 70 });
  } else {
    world.box(x0, Y_PAVE, z0, x1, Y_PAVE + .06, z1, C.cream, 7);
    for (const [x, z] of o.wait || []) spot(l, x, z, 0, yawOf(0, 1), Y_PAVE + .06);
    labels.push({ x: cx, y: 3.2, z: cz, text: "Tram stop", lot: l, r: 30 });
  }
}

function prop(p, b) {
  const [x, z] = p.pos, g = groundBase(x, z);
  if (p.set === "dress" && Dress.prop(p, b)) return;      // street dressing (dressing.js, WLD-028)
  if (p.kind === "lantern") {                              // a wall lantern: glass on an iron arm, and the light it gives (watts as in the Blender scene)
    const o = p.out, lx = x + o[0] * .32, lz = z + o[1] * .32, h = p.height;
    world.box(Math.min(x, lx) - .03, h + .28, Math.min(z, lz) - .03, Math.max(x, lx) + .03, h + .33, Math.max(z, lz) + .03, C.metal, 9);
    world.box(lx - .11, h - .12, lz - .11, lx + .11, h + .26, lz + .11, C.lantern, 2); world.box(lx - .15, h + .26, lz - .15, lx + .15, h + .31, lz + .15, C.metal, 9);
    lights.push([x + o[0] * .5, h, z + o[1] * .5, p.power / 120, 1]);
  } else if (p.kind === "arch") {                          // a stone arch across an alley, wall to wall, as in the reference alley
    const ns = p.span === "x", r = p.width / 2, t = p.thick / 2, spring = 2.3, top = spring + r + 1.6, N = 12, P3 = (a, y, o) => ns ? [x + a, y, z + o] : [x + o, y, z + a];
    for (let i = 0; i < N; i++) {
      const a0 = -r + 2 * r * i / N, a1 = -r + 2 * r * (i + 1) / N, y0 = spring + Math.sqrt(Math.max(0, r * r - a0 * a0)), y1 = spring + Math.sqrt(Math.max(0, r * r - a1 * a1)), am = (a0 + a1) / 2;
      for (const o of [-t, t]) world.quad(P3(a0, y0, o), P3(a1, y1, o), P3(a1, top, o), P3(a0, top, o), C.stone, 6, ns ? [0, 0, o] : [o, 0, 0]);
      world.quad(P3(a0, y0, -t), P3(a1, y1, -t), P3(a1, y1, t), P3(a0, y0, t), mul3(C.stone, .92), 0, ns ? [-am, spring - (y0 + y1) / 2, 0] : [0, spring - (y0 + y1) / 2, -am]);
    }
    world.quad(P3(-r, top, -t), P3(r, top, -t), P3(r, top, t), P3(-r, top, t), C.stoneDark, 0, [0, 1, 0]);
    const e = .42 / r, V3 = (a, y, o) => P3(a * (1 + e), spring + (y - spring) * (1 + e), o);   // the ring of arch stones, standing a little proud of the wall
    for (let i = 0; i < N; i++) {
      const a0 = -r + 2 * r * i / N, a1 = -r + 2 * r * (i + 1) / N, y0 = spring + Math.sqrt(Math.max(0, r * r - a0 * a0)), y1 = spring + Math.sqrt(Math.max(0, r * r - a1 * a1)), col = mul3(C.stone, .78 + .3 * hash(x, z, i));
      for (const o of [-t - .08, t + .08]) world.quad(P3(a0, y0, o), P3(a1, y1, o), V3(a1, y1, o), V3(a0, y0, o), col, 0, ns ? [0, 0, o] : [o, 0, 0]);
    }
    for (const sd of [-1, 1]) { const a = P3(sd * r, 0, -t - .16), b2 = P3(sd * (r - .5), spring, t + .16); world.box(Math.min(a[0], b2[0]), 0, Math.min(a[2], b2[2]), Math.max(a[0], b2[0]), spring + .12, Math.max(a[2], b2[2]), C.stone, 6); }
    const q0 = P3(-r, 0, -t), q1 = P3(r, 0, t), bx = [Math.min(q0[0], q1[0]), Math.min(q0[2], q1[2]), Math.max(q0[0], q1[0]), Math.max(q0[2], q1[2])];
    solid(bx[0], spring + r * .7, bx[1], bx[2], top, bx[3]); occl.push([bx[0], bx[1], bx[2], bx[3], top, 1]);
    const loc = ns ? z - b.origin[1] : x - b.origin[0];
    if (loc < 2 || loc > b.size - 2) {                     // this arch is on the street: roof the first 7 m behind it
      const dir = loc < 2 ? 1 : -1, c0 = P3(-r, 0, dir * t), c1 = P3(r, 0, dir * (t + 7)), cb = [Math.min(c0[0], c1[0]), Math.min(c0[2], c1[2]), Math.max(c0[0], c1[0]), Math.max(c0[2], c1[2])];
      const ub = [Math.min(bx[0], cb[0]), Math.min(bx[1], cb[1]), Math.max(bx[2], cb[2]), Math.max(bx[3], cb[3])];      // a room bridges the alley over the gateway
      world.box(cb[0], top - .5, cb[1], cb[2], top, cb[3], C.stoneDark, 0, { 2: [mul3(C.brown, 1.5), 0] }); solid(cb[0], top - .5, cb[1], cb[2], top, cb[3]);
      world.box(ub[0], top, ub[1], ub[2], top + 2.5, ub[3], PLASTER[1], 0); world.box(ub[0] - .15, top + 2.5, ub[1] - .15, ub[2] + .15, top + 2.75, ub[3] + .15, C.stoneDark, 0, { 2: [C.stoneDark, 0] }); occl.push([ub[0], ub[1], ub[2], ub[3], top + 2.75, 1]);
      for (let kb = 0; kb < 5; kb++) { const o = dir * (t + .9 + kb * 1.45), e0 = P3(-r, 0, o - .11), e1 = P3(r, 0, o + .11); world.box(Math.min(e0[0], e1[0]), top - .78, Math.min(e0[2], e1[2]), Math.max(e0[0], e1[0]), top - .5, Math.max(e0[2], e1[2]), C.brown, 0, { 2: [C.brown, 0] }); }
    }
    const hk = hash(x, z, "al");
    if (hk > .35) { const c = P3(r - .55, 0, (hk > .7 ? 1 : -1) * (t + 1.3)); crates(c[0], c[2], "ar" + x + z); }
  } else if (p.kind === "lamp") {                          // a lantern on a post; its light is real (see the lamps in the shader)
    world.box(x - .17, g, z - .17, x + .17, g + .55, z + .17, C.stoneDark, 6); world.box(x - .08, g + .55, z - .08, x + .08, g + 3.3, z + .08, C.metal, 9);
    world.box(x - .2, g + 3.3, z - .2, x + .2, g + 3.78, z + .2, C.lantern, 2); world.box(x - .27, g + 3.78, z - .27, x + .27, g + 3.88, z + .27, C.metal, 9);
    solid(x - .17, g, z - .17, x + .17, g + 3.3, z + .17); lights.push([x, g + 3.5, z, 1.3]);
  } else if (p.kind === "bench") {                         // a wooden slab on two stone blocks, as in the reference alley
    const ax = p.along === "x", gl = b.lots.find(l => l.kind === "garden" && x >= l.rect[0] && x <= l.rect[2] && z >= l.rect[1] && z <= l.rect[3]);
    let fx = 0, fz = 1; if (gl) { const cx = (gl.rect[0] + gl.rect[2]) / 2, cz = (gl.rect[1] + gl.rect[3]) / 2; ax ? fz = Math.sign(cz - z) || 1 : (fx = Math.sign(cx - x) || 1, fz = 0); } else if (!ax) { fx = 1; fz = 0; }
    world.box(x - (ax ? 1 : .27), g + .36, z - (ax ? .27 : 1), x + (ax ? 1 : .27), g + .47, z + (ax ? .27 : 1), [.5, .32, .16], 0, { 2: [C.brown, 9] });
    for (const s of [-.68, .68]) world.box(x + (ax ? s : 0) - (ax ? .15 : .22), g, z + (ax ? 0 : s) - (ax ? .22 : .15), x + (ax ? s : 0) + (ax ? .15 : .22), g + .36, z + (ax ? 0 : s) + (ax ? .22 : .15), C.stone, 6);
    for (const s of [-.5, .5]) use(x + (ax ? s : 0), g + .47, z + (ax ? 0 : s), "sit", "Sit on the bench", yawOf(fx, fz));
  } else if (p.kind === "fountain") {                      // an eight-sided basin with a bowl on a column
    const R = p.r * 1.36, r2 = R - .42, pt = (k, rr, y) => [x + Math.cos(k * Math.PI / 4 + .3927) * rr, y, z + Math.sin(k * Math.PI / 4 + .3927) * rr];
    for (let k = 0; k < 8; k++) {
      const out = [Math.cos((k + .5) * Math.PI / 4 + .3927), 0, Math.sin((k + .5) * Math.PI / 4 + .3927)];
      world.quad(pt(k, R, g), pt(k + 1, R, g), pt(k + 1, R, g + .55), pt(k, R, g + .55), C.stone, 6, out);
      world.quad(pt(k, R, g + .55), pt(k + 1, R, g + .55), pt(k + 1, r2, g + .55), pt(k, r2, g + .55), C.stone, 0, [0, 1, 0]);
      world.quad(pt(k, r2, g + .55), pt(k + 1, r2, g + .55), pt(k + 1, r2, g + .38), pt(k, r2, g + .38), C.stoneDark, 0, [-out[0], 0, -out[2]]);
      world.quad(pt(k, r2, g + .4), pt(k + 1, r2, g + .4), [x, g + .4, z], [x, g + .4, z], C.water, 9, [0, 1, 0]);
    }
    world.box(x - .28, g + .4, z - .28, x + .28, g + 1.5, z + .28, C.stone, 6); world.box(x - .75, g + 1.5, z - .75, x + .75, g + 1.72, z + .75, C.stone, 0, { 2: [C.stoneDark, 0] }); world.box(x - .16, g + 1.72, z - .16, x + .16, g + 2.05, z + .16, C.stone, 0);
    solid(x - R * .92, g, z - R * .92, x + R * .92, g + .55, z + R * .92);
    if (p.grand) {                                         // the grand one (WLD-029): a second bowl higher up, and a bronze figure pouring water
      world.box(x - .2, g + 2.05, z - .2, x + .2, g + 2.9, z + .2, C.stone, 6); world.box(x - .5, g + 2.9, z - .5, x + .5, g + 3.05, z + .5, C.stone, 0, { 2: [C.stoneDark, 0] });
      world.flat(x - .42, z - .42, x + .42, z + .42, g + 3.0, C.water, 9); world.flat(x - .7, z - .7, x + .7, z + .7, g + 1.68, C.water, 9);
      const Bz = [.3, .42, .36]; world.box(x - .14, g + 3.05, z - .1, x + .14, g + 3.55, z + .1, Bz, 9); world.box(x - .18, g + 3.5, z - .13, x + .18, g + 3.95, z + .13, Bz, 9); world.ybox(x, g + 3.95, z, .17, .2, .17, Math.PI / 4, Bz, 9);
      world.ybox(x + .26, g + 3.75, z, .22, .2, .18, .3, [.55, .45, .2], 9);
      for (let k = 0; k < 8; k++) { const a = k * Math.PI / 4; world.box(x + Math.cos(a) * .66 - .015, g + 1.7, z + Math.sin(a) * .66 - .015, x + Math.cos(a) * .7 + .015, g + 2.05, z + Math.sin(a) * .7 + .015, [.62, .72, .78], 9); }
    }
  } else if (p.kind === "flowerbed") {
    world.box(x - 1.3, g, z - 1.3, x + 1.3, g + .3, z + 1.3, C.stone, 6); world.flat(x - 1.08, z - 1.08, x + 1.08, z + 1.08, g + .32, C.soil, 0);
    const s = D.flowers.size, col = D.flowers.colors[p.color % D.flowers.colors.length];
    if (s > 0) for (let k = 0; k < 16; k++) { const fx = x - .82 + (k % 4) * .55 + (hash(x, z, k) - .5) * .25, fz = z - .82 + Math.floor(k / 4) * .55 + (hash(z, x, k) - .5) * .25, hh = .2 + .25 * s * hash(k, x); world.box(fx - .13, g + .32, fz - .13, fx + .13, g + .32 + hh, fz + .13, mul3(C.olive, .9 + .3 * hash(k, z)), 0); if (D.flowers.bloom > 0) world.box(fx - .1, g + .32 + hh, fz - .1, fx + .1, g + .44 + hh, fz + .1, mul3(col, .62), 0); }
  } else if (p.kind === "stall") {
    const f = p.face, col = pick(ACCENT, (p.color % 6) / 6 + .01), tz = z + f * 1.1;
    world.box(x - 1.2, g, tz - .4, x + 1.2, g + .9, tz + .4, [.5, .32, .16], 0); solid(x - 1.2, g, tz - .4, x + 1.2, g + .9, tz + .4);
    for (let k = 0; k < 5; k++) world.box(x - 1.05 + k * .45, g + .9, tz - .25, x - .75 + k * .45, g + 1.04 + .12 * hash(x, k), tz + .25, pick([C.ochre, C.terracotta, C.olive, C.cream, C.plum], hash(x, z, k)), 0);
    for (const sx of [-1.4, 1.4]) for (const sz of [-.7, 1.9]) world.box(x + sx - .06, g, z + f * sz - .06, x + sx + .06, g + 2.5, z + f * sz + .06, C.brown, 9);
    world.box(x - 1.65, g + 2.5, Math.min(z - f * .95, z + f * 2.15), x + 1.65, g + 2.64, Math.max(z - f * .95, z + f * 2.15), col, 0, { 2: [mul3(col, .7), 0] });
    crates(x + 1.9, tz, "st" + x + z);
  } else if (p.kind === "shelter") {
    for (const sx of [-6, -2, 2, 6]) world.box(x + sx - .08, g, z - 1.26, x + sx + .08, g + 2.6, z - 1.1, C.brown, 9);
    world.box(x - 6.4, g + 2.6, z - 1.4, x + 6.4, g + 2.74, z + 1.3, C.teal, 0, { 2: [mul3(C.teal, .7), 0] }); world.box(x - 6, g + .9, z - 1.24, x + 6, g + 2.2, z - 1.14, C.cream, 0);
  } else if (p.kind === "watertower") {
    for (const sx of [-2, 2]) for (const sz of [-2, 2]) { world.box(x + sx - .2, g, z + sz - .2, x + sx + .2, g + 9, z + sz + .2, C.brown, 9); solid(x + sx - .2, g, z + sz - .2, x + sx + .2, g + 9, z + sz + .2); }
    world.box(x - 2.8, g + 9, z - 2.8, x + 2.8, g + 13, z + 2.8, [.5, .32, .16], 0, { 2: [C.brown, 9] }); world.box(x - 3, g + 13, z - 3, x + 3, g + 13.3, z + 3, C.metal, 9);
  }
}

function tree(t) {                                         // a pale trunk and a faceted crown, like the tree in the reference square
  const [x, z, r, leaf, cr, cg, cb, blossom, fallen] = t, g = groundBase(x, z), w = clamp(.09 + r * .03, .11, .25), h = 1.3 + r * .75, bark = [.46, .38, .3];
  if (fallen) { world.ybox(x + h / 2, g, z, h, w * 2, w * 2, hash(x, z) * 6.28, bark, 0); return; }
  world.box(x - w, g, z - w, x + w, g + h + r * .3, z + w, bark, 0); solid(x - w, g, z - w, x + w, g + h, z + w);
  if (leaf <= .05) return;
  const base = blossom ? [.7, .52, .55] : mix3([cr, cg, cb], [.32, .42, .22], .5), R = r * (.8 + .2 * leaf), cy = g + h + R * .5, N = 7, M = 5, ring = [];
  for (let k = 0; k <= M; k++) { const f = -1.2 + (Math.PI / 2 + 1.2) * k / M, row = []; for (let i = 0; i < N; i++) { const a = i / N * 6.2832 + k * .45, rr = R * (.8 + .4 * hash(x, z, k, i)) * (k === M ? 0 : Math.cos(f)); row.push([x + Math.cos(a) * rr, cy + Math.sin(f) * R * .92 * (k === M ? 1 : .85 + .3 * hash(z, x, k, i)), z + Math.sin(a) * rr]); } ring.push(row); }
  for (let k = 0; k < M; k++) for (let i = 0; i < N; i++) {
    const j = (i + 1) % N, a = ring[k][i], b2 = ring[k][j], c = ring[k + 1][j], d = ring[k + 1][i], mid = [(a[0] + c[0]) / 2 - x, (a[1] + c[1]) / 2 - cy, (a[2] + c[2]) / 2 - z], col = mul3(base, .72 + .55 * hash(x, k, i) + .06 * k);
    world.quad(a, b2, c, c, col, 0, mid); if (k < M - 1) world.quad(a, c, d, d, col, 0, mid);
  }
  for (let i = 0; i < N; i++) world.quad(ring[0][i], ring[0][(i + 1) % N], [x, cy - R * .9, z], [x, cy - R * .9, z], mul3(base, .6), 0, [0, -1, 0]);
}

function buildWorld() {
  const E = SW / 2;
  const A = -E - 2, B = CITY + E + 2, far = 2500;             // fields all round the town, never under it
  world.flat(-far, -far, CITY + far, A, -.05, C.field, 0); world.flat(-far, B, CITY + far, CITY + far, -.05, C.field, 0);
  world.flat(-far, A, A, B, -.05, C.field, 0); world.flat(B, A, CITY + far, B, -.05, C.field, 0);
  world.flat(A, A, B, B, 0, C.road, 20);
  // the town wall, with a gate where the tram comes through
  const W0 = -E - 1.5, W1 = CITY + E + 1.5, th = 3, gh = 7.5;
  const wallBox = (a, b, c, d, y0) => { world.box(a, y0 || 0, b, c, gh, d, C.stone, 6, y0 ? { 2: [C.stoneDark, 9] } : null); solid(a, y0 || 0, b, c, gh, d); occl.push([a, b, c, d, gh]); };
  wallBox(W0 - th, W0 - th, W1 + th, W0); wallBox(W0 - th, W1, W1 + th, W1 + th);
  for (const [a, c] of [[W0 - th, W0], [W1, W1 + th]]) { wallBox(a, W0, c, TRAM_Z - 3.4); wallBox(a, TRAM_Z + 3.4, c, W1); wallBox(a, TRAM_Z - 3.4, c, TRAM_Z + 3.4, 4.6); solid(a + 1, 0, TRAM_Z - 3.4, c - 1, 4.6, TRAM_Z + 3.4); }
  for (const cx of [W0 - th / 2, W1 + th / 2]) for (const cz of [W0 - th / 2, W1 + th / 2]) world.box(cx - 3.5, 0, cz - 3.5, cx + 3.5, gh + 3, cz + 3.5, C.stone, 6);
  for (const s of [-.72, .72]) world.box(-E - 40, 0, TRAM_Z + s - .05, CITY + E + 40, .04, TRAM_Z + s + .05, C.metal, 9);
  RANGES.push([0, world.n, true]);                        // fields, town paving, wall and rails: laid first, under everything
  for (const b of D.blocks) {
    const [bx, bz] = b.origin, S = b.size, b0 = world.n;
    world.box(bx - PAVE, 0, bz - PAVE, bx + S + PAVE, Y_PAVE, bz + S + PAVE, C.pave, 7);
    if (b.template === "rows") world.flat(bx + 2, bz + 16.5, bx + S - 2, bz + S - 16.5, Y_GRASS, C.grass, 0);
    if (b.template === "park") world.flat(bx + 1, bz + 1, bx + S - 1, bz + S - 1, Y_GRASS, C.grass, 0);
    for (const [p, q] of b.paths) {                      // footpaths, kept inside the block and its pavement
      const lo = [bx - PAVE, bz - PAVE], hi = [bx + S + PAVE, bz + S + PAVE];
      const a = [clamp(Math.min(p[0], q[0]), lo[0], hi[0]), clamp(Math.min(p[1], q[1]), lo[1], hi[1])], c = [clamp(Math.max(p[0], q[0]), lo[0], hi[0]), clamp(Math.max(p[1], q[1]), lo[1], hi[1])];
      world.flat(a[0] - 1, a[1] - 1, c[0] + 1, c[1] + 1, Y_PATH + (p[0] === q[0] ? .005 : 0), C.path, 21);
    }
    for (const l of b.lots) isOpen(l) ? openLot(l) : building(l);
    if (b.template === "peri") {                           // cloth strung across the streets of the old quarters
      const up = blockAt.get(b.grid[0] + "," + (b.grid[1] - 1)), left = blockAt.get((b.grid[0] - 1) + "," + b.grid[1]);
      if (up && up.template === "peri") for (const u of [20, 60]) if (hash(b.id, "bn", u) < .55) banner(bx + u, bz - SW, bx + u, bz, b.id + u);
      if (left && left.template === "peri") for (const v of [25, 55]) if (hash(b.id, "bw", v) < .55) banner(bx - SW, bz + v, bx, bz + v, b.id + v);
      for (const [u, v, ax] of [[8, 37.8, 0], [72, 37.8, 0], [37.8, 8, 1], [37.8, 72, 1]]) if (hash(b.id, "ba", u, v) < .7) ax ? banner(bx + u, bz + v, bx + u + 4.4, bz + v, b.id + u + v, 4.35) : banner(bx + u, bz + v, bx + u, bz + v + 4.4, b.id + u + v, 4.35);   // and across the alleys, between the arches
    }
    for (const p of b.props) prop(p, b);
    RANGES.push([b0, world.n, false]);
  }
  const byCell = new Map();                                // trees touch nothing they could tie with, so they are grouped by block square
  for (const t of D.trees) { const k = Math.floor(t[0] / P) + "," + Math.floor(t[1] / P); if (!byCell.has(k)) byCell.set(k, []); byCell.get(k).push(t); }
  for (const ts of byCell.values()) { const t0 = world.n; ts.forEach(tree); RANGES.push([t0, world.n, false]); }
}

// ---------------------------------------------------------------- people
const looks = D.residents.map((r, i) => People.look(i, D.roles[r.r], D.role_sig[D.roles[r.r]]));
let playerLook = People.player();                  // the designed character replaces this (life.js, CHR-001)

function pieceAt(row, m) { let lo = 0, hi = row.length - 1; while (lo < hi) { const mid = (lo + hi + 1) >> 1; if (row[mid][0] <= m) lo = mid; else hi = mid - 1; } return lo; }
function along(p, d) {
  const c = p.c; let k = 1; while (k < c.length - 1 && c[k] < d) k++;
  const f = c[k] > c[k - 1] ? (d - c[k - 1]) / (c[k] - c[k - 1]) : 0, i = 3 + (k - 1) * 2;
  return [mix(p[i], p[i + 2], f), mix(p[i + 1], p[i + 3], f)];
}
const STAND = { pose: 0 };
function resident(i, m) {                                // where resident i is at minute m, or null when they are away
  const row = D.plan[i], k = pieceAt(row, m), p = row[k];
  if (p[1] === 2 || p[1] === 3) return null;                 // away by tram or out of town, or in their car (cars.js draws the car)
  if (p[1] === 0) {
    if (!p.s) { const lot = LOTS[p[7]]; p.s = STAND; if (p[5] >= 0) for (const s of lot.spots) if (Math.abs(s.x - p[2]) < .2 && Math.abs(s.z - p[3]) < .2) { p.s = s; break; } }
    const s = p.s;
    return { x: p[2], z: p[3], y: s === STAND ? groundBase(p[2], p[3]) : s.h, yaw: s === STAND ? hash(i, k) * 6.28 : s.yaw, pose: s.pose, act: p[4], inside: p[6], lot: LOTS[p[7]] };
  }
  if (!p.c) { p.c = [0]; for (let j = 5; j < p.length; j += 2) p.c.push(p.c[p.c.length - 1] + Math.hypot(p[j] - p[j - 2], p[j + 1] - p[j - 1])); }
  const t1 = k + 1 < row.length ? row[k + 1][0] : 1440, n = p.c.length, L = p.c[n - 1], d = clamp((m - p[0]) / Math.max(t1 - p[0], 1e-6), 0, 1) * L;
  const a = along(p, d), b = along(p, Math.min(L, d + 2)), c = along(p, Math.max(0, d - 2));
  let dx = b[0] - c[0], dz = b[1] - c[1]; const len = Math.hypot(dx, dz) || 1; dx /= len; dz /= len;
  const lane = n >= 4 ? (hash(i, "lane") * 2 - 1) * 2.3 * clamp(Math.min(d - p.c[1], p.c[n - 2] - d) / 5, 0, 1) : 0;   // keep to one side of the street, never at a door
  const x = a[0] - dz * lane, z = a[1] + dx * lane;
  return { x, z, y: groundBase(x, z), yaw: yawOf(dx, dz), pose: 1, act: p[2], inside: 0, walking: true, ph: (d / People.stepLength(looks[i].sig, looks[i].S)) % 1 };
}
const DOING = { read: "reading at the library", view: "looking round the museum", watch: "at the pictures", bank: "at the bank", browse: "looking at cars", sleep: "asleep", home: "at home", work: "at work", eat: "eating", shop: "shopping", rest: "resting", sell: "selling at the market", wait: "waiting for the tram", visit: "at the clinic", tend: "tending the garden", deliver: "making a delivery", stroll: "out for a stroll", arrive: "back from town", eat_home: "eating at home", check: "on a round", inspect: "on a round", away: "waiting for the tram" };
const GOING = { read: "going to the library", view: "going to the museum", watch: "going to the pictures", bank: "going to the bank", browse: "going to look at cars", sleep: "heading home", home: "heading home", eat_home: "heading home", work: "on the way to work", eat: "going to eat", shop: "going shopping", rest: "going for a sit down", sell: "on the way to the market", wait: "heading for the tram", away: "heading for the tram", visit: "going to the clinic", tend: "going to the garden", deliver: "out on a delivery", stroll: "out for a stroll" };

function tramAt(m) {                                     // every tram on the map now: [x, stopped]
  const tr = D.tram, t0 = (+tr.first.slice(0, 2)) * 60 + (+tr.first.slice(3)), tl = (+tr.last.slice(0, 2)) * 60 + (+tr.last.slice(3)), v = tr.speed * D.pace, dw = tr.dwell, out = [];
  const stops = [P / 2, 3 * P + P / 2, 7 * P + P / 2], ref = stops[1];
  for (let k = Math.floor((m - t0) / tr.every) - 2; k <= Math.floor((m - t0) / tr.every) + 2; k++) {
    const T = t0 + k * tr.every; if (T < t0 || T > tl) continue;
    let t = m - (T - (ref - stops[0]) / v - dw - (stops[0] + 60) / v), x = -60;       // minutes since this tram entered the map
    for (const s of stops) { const run = (s - x) / v; if (t < run) { x += t * v; t = -1; break; } t -= run; x = s; if (t < dw) { t = -2; break; } t -= dw; }
    if (t >= 0) x += t * v;
    if (m - T > -40 && x > -60 && x < CITY + 60) out.push([x, t === -2, T]);   // T names the run (cars.js seats its riders)
  }
  return out;
}

// ---------------------------------------------------------------- WebGL
const cv = document.getElementById("gl"), ui = document.getElementById("ui"), g2 = ui.getContext("2d");
const gl = cv.getContext("webgl", { antialias: true, alpha: false });
function fail(msg) { const f = document.getElementById("fail"); f.style.display = "flex"; f.textContent = msg; document.getElementById("start").style.display = "none"; throw new Error(msg); }
if (!gl) fail("This browser could not start WebGL, which the slice needs to draw the city. Try a current version of Edge or Chrome.");
function program(vs, fs) {
  const p = gl.createProgram();
  for (const [type, src] of [[gl.VERTEX_SHADER, vs], [gl.FRAGMENT_SHADER, fs]]) { const s = gl.createShader(type); gl.shaderSource(s, src); gl.compileShader(s); if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) fail("Shader error: " + gl.getShaderInfoLog(s)); gl.attachShader(p, s); }
  gl.linkProgram(p); if (!gl.getProgramParameter(p, gl.LINK_STATUS)) fail("Shader link error: " + gl.getProgramInfoLog(p));
  const u = {}; for (let i = 0; i < gl.getProgramParameter(p, gl.ACTIVE_UNIFORMS); i++) { const n = gl.getActiveUniform(p, i).name; u[n] = gl.getUniformLocation(p, n); }
  return { p, u };
}
const NOISE = `float h3(vec3 p){ return fract(sin(dot(p, vec3(12.9898,78.233,37.719)))*43758.5453); }
float ns(vec3 p){ vec3 i=floor(p), f=fract(p); f=f*f*(3.-2.*f);
  return mix(mix(mix(h3(i),h3(i+vec3(1,0,0)),f.x), mix(h3(i+vec3(0,1,0)),h3(i+vec3(1,1,0)),f.x), f.y), mix(mix(h3(i+vec3(0,0,1)),h3(i+vec3(1,0,1)),f.x), mix(h3(i+vec3(0,1,1)),h3(i+vec3(1,1,1)),f.x), f.y), f.z); }`;
// Light, haze, materials and tone follow the numbers in Tools/blender/style_test_02.py, the script behind the reference image.
const TONE = `vec3 tone(vec3 v){                         // AgX with the "Medium High Contrast" look, as the renders use (fitted curve)
  v=mat3(.842479062253094,.0423282422610123,.0423756549057051,.0784335999999992,.878468636469772,.0784336,.0792237451477643,.0791661274605434,.879142973793104)*v;
  v=clamp(log(max(v,vec3(1e-10)))*1.442695,-12.47393,4.026069); v=(v+12.47393)/16.5;
  vec3 x2=v*v, x4=x2*x2; v=15.5*x4*x2-40.14*x4*v+31.96*x4-6.868*x2*v+.4298*x2+.1191*v-.00232;
  float l=dot(v,vec3(.2126,.7152,.0722)); v=pow(max(v,0.),vec3(1.2)); v=l+1.12*(v-l);
  v=mat3(1.19687900512017,-.0528968517574562,-.0529716355144438,-.0980208811401368,1.15190312990417,-.0980434501171241,-.0990297440797205,-.0989611768448433,1.15107367264116)*v;
  return clamp(v,0.,1.); }
float haze(float c){ return .64/(12.566*pow(1.36-1.2*c,1.5)); }`;   // how strongly haze scatters sunlight towards the eye (anisotropy 0.6)
const MAIN_VS = `attribute vec3 aP,aN,aC; attribute float aF; uniform mat4 uVP,uSM; varying vec3 vP,vN,vC,vS; varying float vF; invariant gl_Position;
void main(){ vP=aP; vN=aN; vC=aC; vF=aF; vec4 s=uSM*vec4(aP+aN*.08,1.); vS=s.xyz*.5+.5; gl_Position=uVP*vec4(aP,1.); }`;   // invariant: the depth-only pass and the lit pass land on the same depths
const MAIN_FS = `precision highp float; varying vec3 vP,vN,vC,vS; varying float vF;
uniform vec3 uSun,uSunCol,uSky,uGround,uFog,uCam,uAOm; uniform float uNight,uFogD,uExp,uShOn; uniform sampler2D uSh,uAO,uTxt; uniform vec4 uLamp[12]; uniform int uLampN; uniform mat4 uSM;
uniform vec4 uRoomL[16], uRoomR; uniform int uRoomLN; uniform sampler2D uRoomAO;
${NOISE}
${TONE}
// Light indoors: daylight in through the windows and door (sky and bounce, gone at night), the room's own lamps with distance and
// facing, and contact shade from the map of what stands on the floor (uRoomAO) and from the walls, floor and ceiling meeting.
// The room the camera is in gets all of that; other rooms an even lamplight, as before.
vec3 roomLight(vec3 n,float person){
  float day=1.-uNight, y=vP.y-${Y_FLOOR.toFixed(2)};
  vec3 hemi=mix(uGround,uSky,n.y*.5+.5);
  vec3 amb=(hemi*.42*day+uSunCol*.025*day+vec3(.16,.13,.1)*(.5+.5*day))*(.8+.2*n.y)*(.82+.18*smoothstep(.0,1.6,y));
  bool here=vP.x>uRoomR.x-.05&&vP.x<uRoomR.z+.05&&vP.z>uRoomR.y-.05&&vP.z<uRoomR.w+.05;
  vec3 lamps=vec3(0.); float ao=1.;
  if(here){
    for(int i=0;i<16;i++){ if(i>=uRoomLN) break; vec3 L=uRoomL[i].xyz-vP; float d2=dot(L,L), ndl=dot(n,L*inversesqrt(d2));
      lamps+=uRoomL[i].w*(max(ndl,0.)*.86+.14*(.5+.5*ndl))/(d2*(.35+.06*d2)+.6); }   // a pool under each lamp, falling off faster than 1/d2 past a few metres
    vec2 uv=(vP.xz-uRoomR.xy)/(uRoomR.zw-uRoomR.xy); float o=texture2D(uRoomAO,uv).r;
    float low=1.-smoothstep(0.,.95,y);                                     // contact shade: strong on the floor, fading up the sides of furniture
    ao*=1.-o*(n.y>.6?.62*low:.45*low)*(1.-.4*person);
    float e=min(min(vP.x-uRoomR.x,uRoomR.z-vP.x),min(vP.z-uRoomR.y,uRoomR.w-vP.z));
    ao*=n.y>.6?.72+.28*smoothstep(.25,1.3,e):1.;                           // the floor darkens towards the walls
    ao*=abs(n.y)<.5?(.7+.3*smoothstep(0.,.7,y))*(.82+.18*smoothstep(0.,.6,${GF.toFixed(1)}-vP.y)):1.;   // walls darken where they meet floor and ceiling
    ao*=n.y<-.6?.8+.2*smoothstep(.25,1.,e):1.;
  } else lamps=vec3(.55)*(.82+.16*n.y);
  return (amb+lamps*vec3(1.,.74,.48)*.85)*ao;
}
float shadow(){                                   // soft shadow from the sun's (or moon's) point of view
  if(uShOn<.5) return 1.; vec3 s=vS; if(s.x<0.||s.x>1.||s.y<0.||s.y>1.||s.z>1.) return 1.;
  vec2 p=s.xy*2048.-.5, b0=floor(p), fr=p-b0; float lit=0.;
  for(int i=0;i<4;i++){ float wx=i==0?1.-fr.x:(i==3?fr.x:1.); for(int j=0;j<4;j++){ float wy=j==0?1.-fr.y:(j==3?fr.y:1.);
    lit+=wx*wy*(texture2D(uSh,(b0+vec2(float(i),float(j))-.5)/2048.).r+.0005>s.z?1.:0.); } }
  vec2 b=min(s.xy,1.-s.xy); return mix(1.,lit/9.,smoothstep(0.,.06,min(b.x,b.y))); }
float airLit(vec3 dir,float dist){                // how much of the air between the eye and this point the sun reaches: light shafts (W5)
  if(uShOn<.5) return 1.; float L=min(dist,60.), lit=0., jit=fract(52.9829189*fract(dot(gl_FragCoord.xy,vec2(.06711056,.00583715))));
  for(int i=0;i<10;i++){ vec3 s=(uSM*vec4(uCam+dir*L*((float(i)+jit)/10.),1.)).xyz*.5+.5;
    lit+=(s.x<0.||s.x>1.||s.y<0.||s.y>1.||s.z>1.)?1.:step(s.z,texture2D(uSh,s.xy).r+.0005); }
  return mix(lit/10.,1.,clamp((dist-60.)/120.,0.,1.)); }
vec3 paint(vec3 c2,vec3 r,float v){               // the painted() material: dark tone, a mid tone and a warm highlight blotched together (W1)
  vec3 c1=c2*r, mid=(c1+c2)*vec3(.48,.465,.53), hi=c2*vec3(1.12,1.04,.92);
  return v<.45?mix(c1,mid,v/.45):(v<.86?mix(mid,hi,(v-.45)/.41):mix(hi,c2,(v-.86)/.14)); }
void main(){
  float f=vF; vec3 c=vC; vec3 n=normalize(vN); vec3 tc=uCam-vP; float d=length(tc); vec3 vd=-tc/d;
  float hh=45., dy=vP.y-uCam.y; float k=abs(dy)>.05?(exp(-uCam.y/hh)-exp(-vP.y/hh))/(dy/hh):exp(-uCam.y/hh);
  float fog=1.-exp(-d*uFogD*max(k,0.));
  float blot=clamp((ns(vP*.9)*.6+ns(vP*2.7+7.)*.4-.5)*2.2+.5,0.,1.), nearK=1.-min(1.,d/70.);
  vec3 col; bool emis=false; float inside=0.;
  if((f>.5&&f<1.5)||(f>3.5&&f<4.5)){                                      // a window: vC is (across, up, seed)
    vec2 uv=c.xy; float bx=min(uv.x,1.-uv.x);
    if(bx<.09||uv.y<.07||uv.y>.93){ c=paint(vec3(.37,.34,.32),vec3(.49,.53,.69),blot)*(uv.y<.07?1.2:1.); f=9.; }   // stone frame and sill
    else { vec2 g=(uv-vec2(.09,.07))/vec2(.82,.86);
      float reveal=(1.-.6*smoothstep(.72,1.,g.y))*(1.-.4*smoothstep(.22,0.,g.x));
      vec3 glass=(vec3(.07,.09,.12)*(uSky*1.5+uGround*2.)+uSky*.04)*reveal, warm=mix(vec3(1.,.5,.2),vec3(1.,.68,.36),c.z)*(1.+1.5*g.y);
      float shop=step(.955,c.z);                                          // shop and cafe windows glow by day too
      col=mix(glass,warm*(1.-.5*shop*(1.-uNight)),f<1.5?max(uNight,shop*.8):0.);
      if(c.z>.995) col=c.z>.9988?vec3(.82,.86,.94)*1.5:(uFog*1.3+uSky*1.9+uSunCol*.05)*(.7+.5*g.y)*(1.-.85*uNight);   // the same window seen from inside: daylight beyond (.999: a lit cinema screen)
      if(abs(g.x-.5)<.035||abs(g.y-.6)<.03) col=vec3(.3,.2,.13)*(uSky+uSunCol*.03+.004);   // glazing bars
      emis=true; }
  }
#ifdef LETTERS
  if(f>12.5&&f<13.5){                                                     // painted lettering: vC is (across, up, cell + share of the cell)
    float id=floor(c.z), tw=fract(c.z); vec2 uv=vec2((mod(id,4.)+.5+(c.x-.5)*tw)/4.,(floor(id/4.)+1.-c.y)/32.);
    if(texture2D(uTxt,uv).a<.5) discard; c=vec3(.70,.64,.52); f=0.; }
#endif
  if(!emis){
    if(f<.5) c=paint(c,vec3(.69,.69,.73),blot)*(.92+.16*ns(vP*.13));
    else if(f>5.5&&f<6.5){ float u=abs(n.x)>.5?vP.z:vP.x; float row=floor(vP.y/.5); float uu=u+.5*mod(row,2.); float t1=h3(vec3(floor(uu),row,3.));
      float fy=fract(vP.y/.5), fx=fract(uu), wall=abs(1.-abs(n.y));
      float j=max(smoothstep(.945,.965,fy),smoothstep(.972,.984,fx))*wall;                 // blocks 1 m by 0.5 m with thin dark joints (W2)
      float bev=(.12*smoothstep(.78,.93,fy)*step(fy,.95)-.14*smoothstep(.13,0.,fy)+.06*smoothstep(.9,.965,fx)*step(fx,.975)-.07*smoothstep(.04,0.,fx))*wall*nearK;
      vec3 c1=c*vec3(.52,.54,.70); c=mix(paint(c,vec3(.52,.54,.70),blot),(c1+c)*.5,t1*.8)*(1.+bev); c=mix(c,c1*vec3(.7,.7,.75),j*(1.-min(1.,d/120.))); }
    else if(f>6.5&&f<7.5){ vec2 q=vec2(vP.x/.83,vP.z/.42); q.x+=.5*mod(floor(q.y),2.); float t1=h3(vec3(floor(q),5.)); vec2 fr=fract(q);
      float j=max(smoothstep(.97,.985,fr.x),smoothstep(.94,.965,fr.y)); float bev=(.07*smoothstep(.75,.93,fr.y)*step(fr.y,.95)-.08*smoothstep(.12,0.,fr.y))*nearK;
      vec3 c1=c*vec3(.48,.5,.68); c=mix(paint(c,vec3(.48,.5,.68),blot),(c1+c)*.5,t1*.8)*(1.+bev); c=mix(c,c1*vec3(.7,.7,.75),j*(1.-min(1.,d/50.)));
      if(vP.y<${(Y_PAVE + .01).toFixed(3)}){ vec2 u2=mod(vP.xz,${P.toFixed(1)}), e2=min(u2-${(SW / 2 - PAVE).toFixed(3)},${(P - SW / 2 + PAVE).toFixed(3)}-u2); float e=min(e2.x,e2.y);   // kerb: long pale stones along the pavement's edge
        if(e<.32){ float al=e2.x<e2.y?vP.z:vP.x, t2=h3(vec3(floor(al/1.1),11.,0.)), k2=1.-min(1.,d/50.);
          vec3 kc=paint(c1*vec3(2.4,2.3,1.75),vec3(.62,.62,.68),blot)*(.9+.16*t2)*(1.+.12*smoothstep(.06,0.,e)*step(.5,n.y)*k2);
          c=e<.3?mix(kc,kc*.55,smoothstep(.975,.99,fract(al/1.1))*k2):c1*.6; } } }
    else if(f>19.5&&f<20.5){ vec2 u2=mod(vP.xz,${P.toFixed(1)}), e2=min(u2,${P.toFixed(1)}-u2); bool ns=e2.x<e2.y;   // the road: small setts in courses across the street
      vec2 q=ns?vec2(vP.x/.24,vP.z/.15):vec2(vP.z/.24,vP.x/.15); q.x+=.5*mod(floor(q.y),2.)+.3*h3(vec3(floor(q.y),0.,13.));
      float t1=h3(vec3(floor(q),13.)), k2=1.-min(1.,d/40.), k3=1.-min(1.,d/90.); vec2 fr=fract(q);
      float j=max(smoothstep(.9,.96,fr.x),smoothstep(.86,.94,fr.y)), dome=(1.-pow(abs(fr.x*2.-1.),4.))*(1.-pow(abs(fr.y*2.-1.),4.));
      float g=ns?e2.x:e2.y, gut=smoothstep(${(SW / 2 - PAVE - .55).toFixed(3)},${(SW / 2 - PAVE - .45).toFixed(3)},g);   // a darker gutter row along each kerb
      vec3 c1=c*vec3(.62,.64,.78); c=mix(paint(c,vec3(.62,.64,.78),blot),(c1+c)*.5,t1*.7*k3)*(.9+.16*dome*k2)*(1.-.16*gut); c=mix(c,c1*.55,j*k2); }
    else if(f>20.5&&f<21.5){ vec2 q=vP.xz/.3; q.x+=.5*mod(floor(q.y),2.); vec2 id=floor(q), fr=fract(q)-.5;   // footpaths: round cobbles
      float t1=h3(vec3(id,17.)), k2=1.-min(1.,d/30.); fr+=vec2(h3(vec3(id,18.)),h3(vec3(id,19.)))*.16-.08; float r=length(fr)*2.;
      vec3 c1=c*vec3(.6,.6,.72); c=mix(paint(c,vec3(.6,.6,.72),blot),c1,t1*.55*k2)*mix(1.,(1.06-.2*fr.y)*(1.-.55*smoothstep(.72,1.,r)),k2); }
    else if(f>21.5&&f<22.5) c=paint(c,vec3(.74,.72,.8),blot)*(.95+.1*ns(vP*3.1));   // car paint: matte and mottled like everything else (W1, V2; cars.js)
    else if(f>10.5&&f<11.5){ vec3 tg=normalize(vec3(-n.z,0.,n.x)+vec3(1e-3,0.,0.)); float u=dot(vP,tg)/.24, cu=fract(u)*2.-1., row=fract(vP.y/.17);   // barrel tiles laid in courses
      float tube=sqrt(max(1.-cu*cu,0.)), t1=h3(vec3(floor(u),floor(vP.y/.17),9.)), k2=1.-min(1.,d/90.);
      c=paint(c,vec3(.62,.6,.66),blot)*mix(1.,(.5+.5*tube)*(.86+.28*t1)*(1.-.32*smoothstep(.72,1.,row)),k2); n=normalize(n+tg*cu*.6*k2); }
    else if(f>11.5&&f<12.5){ float s=fract(vP.y/.1), k2=1.-min(1.,d/60.);                                            // louvred shutter slats
      c=paint(c,vec3(.7,.7,.74),blot)*mix(1.,mix(.42,1.08,smoothstep(0.,.3,s))*(1.-.45*smoothstep(.82,1.,s)),k2); n=normalize(n+vec3(0.,(.5-s)*.9*k2,0.)); }
    else if(f>14.5&&f<15.5){ float pz=vP.z/.19, pi=floor(pz), fz=fract(pz), px=vP.x/2.1+h3(vec3(pi,1.,7.)), fx=fract(px);   // floorboards
      float t1=h3(vec3(pi,floor(px),8.)); c=c*(.8+.34*t1)*(.93+.14*ns(vec3(vP.x*2.5,pi*.7,1.)));
      c*=1.-.5*max(smoothstep(.9,.99,fz),smoothstep(.985,.999,fx))*nearK; }
    else if(f>15.5&&f<16.5){ vec2 q=vP.xz/.42, fr=fract(q); float ck=mod(floor(q.x)+floor(q.y),2.), t1=h3(vec3(floor(q),4.));   // tiles in two tones
      c=mix(c,c*vec3(.8,.8,.84),ck)*(.95+.1*t1); c=mix(c,c*.62,max(smoothstep(.955,.985,fr.x),smoothstep(.955,.985,fr.y))*nearK); }
    else if(f>16.5&&f<17.5){ float y=vP.y-.2, u=abs(n.x)>.5?vP.z:vP.x, pu=fract(u/.85);   // a room wall: wooden panelling with a rail below, plaster above, a dark skirting
      vec3 wood=mix(c,vec3(.34,.22,.14),.72);
      if(y<.13) c=wood*.55;
      else if(y<.92) c=wood*(.86+.14*smoothstep(.06,.12,pu)*smoothstep(.94,.88,pu))*(1.-.25*smoothstep(.13,.2,y)*smoothstep(.2,.13,y))*(.95+.1*ns(vP*3.));
      else if(y<1.0) c=wood*1.1;
      else c=paint(c,vec3(.86),blot); }
    else if(f>17.5&&f<18.5){ float bx=fract(vP.x/1.4);                                // a ceiling: dark timber beams across plaster
      c=bx<.16?vec3(.24,.16,.1)*(.85+.3*ns(vP*2.)):paint(c,vec3(.88),blot); }
    else if(f>18.5&&f<19.5) c*=.93+.14*ns(vP*5.);                         // room furniture: plain, a little grain
    if(f>1.5&&f<2.5){ col=vec3(1.,.55,.22)*6.; emis=true; }                // lantern glass: emission strength 6
    else {
      vec3 hemi=mix(uGround,uSky,n.y*.5+.5), light;
      if(f>2.5&&f<3.5){ light=roomLight(n,0.); c=paint(c,vec3(.76),blot); inside=1.; }
      else if(f>14.5&&f<19.5){ light=roomLight(n,0.); inside=1.; }   // room surfaces and furniture (roomLight above)
      else if(f>9.5&&f<10.5){ float up=n.y*.5+.5;                       // a person indoors: the room's light, a little more from above so the body keeps its form
        light=roomLight(n,1.)*(.72+.42*up*up); inside=1.; }
      else {
        vec2 a=texture2D(uAO,(vP.xz+n.xz-uAOm.x)*uAOm.y).rg; float vis=mix(a.r,1.,clamp((vP.y-.3)/max(a.g*85.,.6),0.,1.));   // how much sky this spot sees
        if(abs(n.y)<.5&&!(f>21.5&&f<22.5)) c*=.66+.34*smoothstep(.1,1.4,vP.y);   // grime at the foot of walls (not on cars)
        float ndl=max(dot(n,uSun),0.); light=uSunCol*(ndl>0.?ndl*shadow():0.);
        if((f>13.5&&f<14.5)||(f>21.5&&f<22.5)){ float up=n.y*.5+.5, bw=dot(n,normalize(vec3(-uSun.x,-.45,-uSun.z)))*.5+.5;   // a person or a car: sky from above, bounce from the sunlit side away from the sun, so shade still models the body
          if(f<14.5){ float ks=dot(n,normalize(vec3(uSun.x,.9,uSun.z)))*.5+.5, dn=max(-n.y,0.);   // a person in shade: the sky is brightest round the sun, so that side of the body is lit more, and what faces down is darker
            light+=uSky*(.14+.6*up*up+.62*ks*ks*ks)*vis*vis*(1.-.4*dn)+uGround*(1.-.5*vis)*(.3+1.3*bw*bw); }
          else light+=uSky*(.28+.82*up*up)*vis*vis+uGround*(1.-.5*vis)*(.38+1.25*bw*bw); }
        else light+=uSky*(n.y*.35+.65)*vis*vis+uGround*(1.-.5*vis);   // sun, sky seen, and light bounced off sunlit stone
        for(int i=0;i<12;i++){ if(i>=uLampN) break; vec3 L=uLamp[i].xyz-vP; float d2=dot(L,L); light+=vec3(1.,.62,.32)*(uLamp[i].w/(d2+.25)*smoothstep(420.,60.,d2)*(max(dot(n,L*inversesqrt(d2)),0.)*.85+.15)); } }   // lanterns (W6)
      col=c*light; }
  }
  col=mix(col,uFog+uSunCol*(4.4*haze(dot(vd,uSun)))*airLit(vd,d),fog*(emis?.5:1.)*(1.-.85*inside));   // haze glows where the sun reaches it; little haze indoors
  float glow=0.; for(int i=0;i<12;i++){ if(i>=uLampN) break; vec3 q=uLamp[i].xyz-uCam; float b=dot(q,vd), h=sqrt(max(dot(q,q)-b*b,.02)); glow+=uLamp[i].w*(atan((d-b)/h)+atan(b/h))/h; }
  col+=vec3(1.,.62,.32)*(glow*uFogD*1.6);                                 // every lantern glows in the haze around it
  gl_FragColor=vec4(tone(col*uExp),1.);
}`;
const main = program(MAIN_VS, MAIN_FS), signProg = program(MAIN_VS, "#define LETTERS\n" + MAIN_FS);   // only the painted names cut holes, so the town's shader keeps the early depth test
const pre = program(MAIN_VS, `precision mediump float; void main(){ gl_FragColor=vec4(1.); }`);       // depth only, laid first so each pixel is lit once
const sky = program(`attribute vec2 aQ; varying vec2 vQ; void main(){ vQ=aQ; gl_Position=vec4(aQ,.9999,1.); }`,
`precision highp float; varying vec2 vQ; uniform vec3 uR,uU,uF,uSun,uZen,uHor,uSunCol; uniform float uNight,uExp;
${NOISE}
${TONE}
void main(){ vec3 d=normalize(uF+vQ.x*uR+vQ.y*uU); float s=dot(d,uSun), p=haze(s); vec3 c=mix(uHor+uSunCol*(4.4*p),uZen+uSunCol*(.9*p),pow(clamp(d.y,0.,1.),.5));
  c+=uSunCol*pow(max(s,0.),2500.)*30.*step(0.,d.y); c+=step(.9982,h3(floor(d*170.)))*uNight*smoothstep(.03,.3,d.y)*.05;
  gl_FragColor=vec4(tone(c*uExp),1.); }`);
const depthProg = program(`attribute vec3 aP; uniform mat4 uVP; void main(){ gl_Position=uVP*vec4(aP,1.); }`, `precision mediump float; void main(){ gl_FragColor=vec4(1.); }`);
const SH = 2048; let shFbo = null, shTex = null;
if (gl.getExtension("WEBGL_depth_texture")) {                     // a depth picture of the scene from the sun, for shadows
  shTex = gl.createTexture(); gl.bindTexture(gl.TEXTURE_2D, shTex); gl.texImage2D(gl.TEXTURE_2D, 0, gl.DEPTH_COMPONENT, SH, SH, 0, gl.DEPTH_COMPONENT, gl.UNSIGNED_INT, null);
  const colTex = gl.createTexture(), par = t => { for (const [k, v] of [[gl.TEXTURE_MIN_FILTER, gl.NEAREST], [gl.TEXTURE_MAG_FILTER, gl.NEAREST], [gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE], [gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE]]) gl.texParameteri(gl.TEXTURE_2D, k, v); };
  par(); gl.bindTexture(gl.TEXTURE_2D, colTex); gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, SH, SH, 0, gl.RGBA, gl.UNSIGNED_BYTE, null); par();
  shFbo = gl.createFramebuffer(); gl.bindFramebuffer(gl.FRAMEBUFFER, shFbo);
  gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, colTex, 0); gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.TEXTURE_2D, shTex, 0);
  if (gl.checkFramebufferStatus(gl.FRAMEBUFFER) !== gl.FRAMEBUFFER_COMPLETE) shFbo = null;
  gl.bindFramebuffer(gl.FRAMEBUFFER, null);
}
function bakeSky() {                                       // a map of how much sky each spot on the ground sees, worked out once from the heights around it
  const N = 1024, lo = -SW / 2 - 14, size = CITY + SW + 28, k = N / size, H = new Float32Array(N * N); let V = new Float32Array(N * N).fill(-1); const hn = new Float32Array(N * N);
  const put = (x0, z0, x1, z1, h) => { const i0 = Math.max(0, Math.round((x0 - lo) * k)), i1 = Math.min(N, Math.max(Math.round((x1 - lo) * k), i0 + 1)), j0 = Math.max(0, Math.round((z0 - lo) * k)), j1 = Math.min(N, Math.max(Math.round((z1 - lo) * k), j0 + 1)); for (let j = j0; j < j1; j++) for (let i = i0; i < i1; i++) if (h > H[j * N + i]) H[j * N + i] = h; };
  for (const l of LOTS) if (!isOpen(l)) put(l.rect[0], l.rect[1], l.rect[2], l.rect[3], l.H + 1);
  for (const o of occl) put(o[0], o[1], o[2], o[3], o[4]);
  const covered = new Uint8Array(N * N); for (const o of occl) if (o[5]) for (let j = Math.max(0, Math.floor((o[1] - lo) * k)); j <= Math.min(N - 1, Math.ceil((o[3] - lo) * k)); j++) for (let i = Math.max(0, Math.floor((o[0] - lo) * k)); i <= Math.min(N - 1, Math.ceil((o[2] - lo) * k)); i++) covered[j * N + i] = 1;
  const steps = [1, 2.2, 4, 7, 11, 16], cs = []; for (let a = 0; a < 8; a++) cs.push([Math.cos(a * Math.PI / 4 + .2), Math.sin(a * Math.PI / 4 + .2)]);
  for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
    const q = j * N + i; if (H[q] > .5) { hn[q] = H[q]; continue; }
    let v = 0, top = 0;
    for (const [cx, cz] of cs) { let mx = 0; for (const st of steps) { const ii = Math.round(i + cx * st * k), jj = Math.round(j + cz * st * k); if (ii < 0 || jj < 0 || ii >= N || jj >= N) continue; const h = H[jj * N + ii]; if (h > top) top = h; if (h / st > mx) mx = h / st; } v += 1 - mx / Math.sqrt(1 + mx * mx); }
    V[q] = v / 8; hn[q] = top;
  }
  for (let pass = 0; pass < 2; pass++) {                   // spread ground values two cells into the footprints, so a wall reads the ground beside it
    const V2 = V.slice();
    for (let j = 1; j < N - 1; j++) for (let i = 1; i < N - 1; i++) { const q = j * N + i; if (V[q] >= 0) continue; let m = 2; for (let dj = -1; dj <= 1; dj++) for (let di = -1; di <= 1; di++) { const w = V[q + dj * N + di]; if (w >= 0 && w < m) m = w; } if (m < 2) V2[q] = m; }
    V = V2;
  }
  const out = new Uint8Array(N * N * 4); for (let q = 0; q < N * N; q++) { out[q * 4] = (covered[q] ? .07 : V[q] < 0 ? 1 : V[q]) * 255; out[q * 4 + 1] = Math.min(255, hn[q] * 3); out[q * 4 + 3] = 255; }
  const tex = gl.createTexture(); gl.bindTexture(gl.TEXTURE_2D, tex); gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, N, N, 0, gl.RGBA, gl.UNSIGNED_BYTE, out);
  for (const [kk, vv] of [[gl.TEXTURE_MIN_FILTER, gl.LINEAR], [gl.TEXTURE_MAG_FILTER, gl.LINEAR], [gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE], [gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE]]) gl.texParameteri(gl.TEXTURE_2D, kk, vv);
  return { tex, lo, size };
}
function drawDepth(buf, count) { gl.bindBuffer(gl.ARRAY_BUFFER, buf); const loc = gl.getAttribLocation(depthProg.p, "aP"); gl.enableVertexAttribArray(loc); gl.vertexAttribPointer(loc, 3, gl.FLOAT, false, 40, 0); gl.drawArrays(gl.TRIANGLES, 0, count); }
function buffer(mesh, usage) { const b = gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER, b); gl.bufferData(gl.ARRAY_BUFFER, mesh.a.subarray(0, mesh.n), usage); return b; }
function attribs(pr, buf) {                                 // point a program of the main vertex layout at a buffer
  gl.bindBuffer(gl.ARRAY_BUFFER, buf);
  if (!pr.loc) pr.loc = ["aP", "aN", "aC", "aF"].map(n => gl.getAttribLocation(pr.p, n));
  pr.loc.forEach((loc, k) => { if (loc < 0) return; gl.enableVertexAttribArray(loc); gl.vertexAttribPointer(loc, k < 3 ? 3 : 1, gl.FLOAT, false, 40, k * 12); });
}
function drawMesh(buf, count, pr) { attribs(pr || main, buf); gl.drawArrays(gl.TRIANGLES, 0, count); }
function boxOf(a, s, e) { const b = [1e9, 1e9, 1e9, -1e9, -1e9, -1e9]; for (let i = s; i < e; i += 10) for (let k = 0; k < 3; k++) { const v = a[i + k]; if (v < b[k]) b[k] = v; if (v > b[k + 3]) b[k + 3] = v; } return b; }
function outside(M, b) {                                     // is the box wholly beyond one side of the view M (its 8 corners all past the same plane)?
  let all = 63;
  for (let k = 0; k < 8; k++) {
    const x = b[k & 1 ? 3 : 0], y = b[k & 2 ? 4 : 1], z = b[k & 4 ? 5 : 2];
    const cx = M[0] * x + M[4] * y + M[8] * z + M[12], cy = M[1] * x + M[5] * y + M[9] * z + M[13], cz = M[2] * x + M[6] * y + M[10] * z + M[14], cw = M[3] * x + M[7] * y + M[11] * z + M[15];
    all &= (cx < -cw ? 1 : 0) | (cx > cw ? 2 : 0) | (cy < -cw ? 4 : 0) | (cy > cw ? 8 : 0) | (cz < -cw ? 16 : 0) | (cz > cw ? 32 : 0);
    if (!all) return false;
  }
  return true;
}
const boxD2 = (b, p) => { const dx = Math.max(b[0] - p[0], 0, p[0] - b[3]), dy = Math.max(b[1] - p[1], 0, p[1] - b[4]), dz = Math.max(b[2] - p[2], 0, p[2] - b[5]); return dx * dx + dy * dy + dz * dz; };

function lighting(m) {                                       // the reference recipe (sun 14 W/m2, sky 0.35, haze) carried through the day
  const rise = D.day.sunrise * 60, set = D.day.sunset * 60, day = smooth(0, 1, Math.min((m - (rise - 30)) / 60, ((set + 30) - m) / 60));
  const a = clamp((m - rise) / (set - rise), 0, 1) * Math.PI, unit = v => { const n = Math.hypot(v[0], v[1], v[2]); return v.map(q => q / n); };
  const sun = unit([Math.cos(a) * .85, Math.sin(a) * .8 + .08, .42 * Math.sin(a) + .12]), moon = unit([-.42, .74, -.52]), low = smooth(.05, .5, sun[1]), isDay = day > .03;
  const fogD = mix(.014, .009, low) * (.6 + .4 * day), through = Math.exp(-fogD * 14 / Math.max(sun[1], .12));      // sunlight left after crossing the haze
  const sunCol = isDay ? mul3(mix3([1, .42, .22], [1, .86, .68], low), 14 / Math.PI * through * day) : [.04, .055, .1];
  const sky = mix3([.012, .018, .045], mix3([.036, .06, .18], [.147, .193, .287], low), day);
  return { day, night: 1 - day, dir: isDay ? sun : moon, sunCol, sky, fogD,
    ground: [.004 + sunCol[0] * .1, .004 + sunCol[1] * .068, .005 + sunCol[2] * .042],
    fog: mul3(sky, 1.25), zen: mul3(sky, 1.5), exp: mix(1.7, 1.23, day) };
}

// ---------------------------------------------------------------- the player
const spawn = D.city.spawn;
const pl = { x: spawn.pos[0], y: 0, z: spawn.pos[1], vy: 0, yaw: yawOf(spawn.facing[0], spawn.facing[1]), pose: 0, ph: 0, moving: false, seat: null, onGround: true };
const cam = { yaw: pl.yaw, pitch: -.04, dist: 4.4, fp: false, eye: [0, 0, 0], dir: [0, 0, -1] };
const clock = { m: spawn.time ? (+spawn.time.slice(0, 2)) * 60 + (+spawn.time.slice(3)) : 8 * 60, rate: 1, day: 0 };   // day: game days since the character arrived (the bank settles by them, BANK-008)
const keys = {}; let started = false, locked = false, showMap = false, bodies = 0, fps = 60, target = null, quality = 1, slow = 0;
const R = 0.32, TALL = 1.75;

function pushOut(p) {
  eachSolid(p.x, p.z, R, s => {
    if (s[4] <= p.y + .5 || s[1] >= p.y + TALL - .05) return;
    const cx = clamp(p.x, s[0], s[3]), cz = clamp(p.z, s[2], s[5]), dx = p.x - cx, dz = p.z - cz, d2 = dx * dx + dz * dz;
    if (d2 >= R * R) return;
    if (d2 > 1e-9) { const d = Math.sqrt(d2); p.x = cx + dx / d * R; p.z = cz + dz / d * R; return; }
    const a = [p.x - s[0], s[3] - p.x, p.z - s[2], s[5] - p.z], k = a.indexOf(Math.min(...a));
    if (k === 0) p.x = s[0] - R; else if (k === 1) p.x = s[3] + R; else if (k === 2) p.z = s[2] - R; else p.z = s[5] + R;
  });
}
function floorAt(x, z, y) { let g = groundBase(x, z); eachSolid(x, z, .2, s => { if (x > s[0] - .15 && x < s[3] + .15 && z > s[2] - .15 && z < s[5] + .15 && s[4] <= y + .5 && s[4] > g) g = s[4]; }); return g; }
function ceilAt(x, z, y) { let c = 1e9; eachSolid(x, z, .2, s => { if (x > s[0] && x < s[3] && z > s[2] && z < s[5] && s[1] >= y + TALL - .3 && s[1] < c) c = s[1]; }); return c; }
function blocked(q) { if (q[1] < groundBase(q[0], q[2]) + .15) return true; let hit = false; eachSolid(q[0], q[2], .2, s => { if (q[0] > s[0] - .15 && q[0] < s[3] + .15 && q[1] > s[1] - .15 && q[1] < s[4] + .15 && q[2] > s[2] - .15 && q[2] < s[5] + .15) hit = true; }); return hit; }

function stepPlayer(dt) {
  if (pl.pose === 4) return Cars.drive(dt, keys, pl, cam, clock.m);   // driving (cars.js)
  const f = (keys.KeyW || keys.ArrowUp ? 1 : 0) - (keys.KeyS || keys.ArrowDown ? 1 : 0), s = (keys.KeyD || keys.ArrowRight ? 1 : 0) - (keys.KeyA || keys.ArrowLeft ? 1 : 0);
  if (pl.pose > 1) { if (f || s || keys.Space) { Sound.act("stand"); pl.pose = 0; pl.y = floorAt(pl.x, pl.z, pl.y); if (pl.seat) { pl.x = pl.seat.ox; pl.z = pl.seat.oz; } pl.seat = null; } else return; }
  const run = keys.ShiftLeft || keys.ShiftRight, sp = run ? 4.2 : 1.6, sy = Math.sin(cam.yaw), cy = Math.cos(cam.yaw);
  let vx = sy * f + cy * s, vz = -cy * f + sy * s; const l = Math.hypot(vx, vz);
  pl.moving = l > 0;
  if (l > 0) {
    vx *= sp / l; vz *= sp / l;
    const n = Math.max(1, Math.ceil(sp * dt / .15));
    for (let k = 0; k < n; k++) { pl.x += vx * dt / n; pl.z += vz * dt / n; pushOut(pl); pushOut(pl); }
    let dy = yawOf(vx, vz) - pl.yaw; dy = Math.atan2(Math.sin(dy), Math.cos(dy)); pl.yaw += dy * Math.min(1, dt * 12);
    pl.run = run; pl.ph = (pl.ph + sp * dt / People.stepLength(run ? People.SIG.Run : playerLook.sig, playerLook.S)) % 1;
  }
  if (cam.fp) pl.yaw = cam.yaw;
  for (const [x, stopped] of tramAt(clock.m)) if (Math.abs(pl.x - x) < 6.4 && Math.abs(pl.z - TRAM_Z) < 1.6 && pl.y < 3) pl.z = TRAM_Z + (pl.z < TRAM_Z ? -1.6 : 1.6);
  Cars.push(pl, clock.m);                                  // nor through a car (cars.js)
  const g = floorAt(pl.x, pl.z, pl.y);
  if (keys.Space && pl.onGround) { pl.vy = 4.8; pl.onGround = false; Sound.act("jump"); }
  if (!pl.onGround || pl.y > g + .01) {
    pl.vy -= 14 * dt; pl.y += pl.vy * dt;
    const c = ceilAt(pl.x, pl.z, pl.y - pl.vy * dt); if (pl.vy > 0 && pl.y + TALL > c) { pl.y = c - TALL; pl.vy = 0; }
    if (pl.y <= g) { pl.y = g; pl.vy = 0; pl.onGround = true; } else pl.onGround = false;
  } else { pl.y = mix(pl.y, g, Math.min(1, dt * 18)); pl.onGround = true; }
}
function interact() {
  if (pl.pose === 4) { Sound.car("out"); return Cars.leave(pl, cam); }   // out of the car (cars.js)
  if (pl.pose > 1) { keys.Space = true; stepPlayer(0); keys.Space = false; return; }
  if (!target || Talk.on) return;
  if (target.type === "talk") return Talk.open(target);
  if (target.type === "car") { Sound.car("in"); return Cars.enter(target, pl, cam, clock.m); }   // into a parked car (cars.js)
  if (target.type === "lift") {
    const fd = document.getElementById("fade"); fd.style.opacity = 1; Sound.act("lift");
    setTimeout(() => { pl.x = target.to[0]; pl.y = target.to[1]; pl.z = target.to[2]; pl.vy = 0; fd.style.opacity = 0; }, 240);
  } else { pl.seat = { ox: pl.x, oz: pl.z }; pl.x = target.x; pl.z = target.z; pl.y = target.y; pl.yaw = target.yaw; pl.pose = target.type === "lie" ? 3 : 2; pl.vy = 0; Sound.act(target.type === "lie" ? "lie" : "sit"); if (cam.fp) cam.yaw = target.yaw; }
}
let lastTarget = null;
function findTarget() {
  findTarget0(); if (target && target !== lastTarget && (!lastTarget || target.type !== lastTarget.type || target.x !== lastTarget.x)) Sound.act("prompt"); lastTarget = target;
}
function findTarget0() {
  target = null; let best = 1.7;
  if (pl.pose > 1) return;
  for (const u of uses) { if (Math.abs(u.x - pl.x) > 2 || Math.abs(u.z - pl.z) > 2 || Math.abs(u.y - pl.y) > 1.2) continue; const d = Math.hypot(u.x - pl.x, u.z - pl.z); if (d < best) { best = d; target = u; } }
  if (!target) target = Cars.target(pl, clock.m);        // a parked car to drive (cars.js)
}
function stepCamera(dt) {
  const cp = Math.cos(cam.pitch); cam.dir = [Math.sin(cam.yaw) * cp, Math.sin(cam.pitch), -Math.cos(cam.yaw) * cp];
  if (!started && Life.designing) return Life.camera();  // the character designer turns round the player (life.js)
  if (!started) {                                        // before play: a slow turn above Market Square
    const a = performance.now() / 16000, c = [3 * P + P / 2, 0, 4 * P + P / 2]; cam.eye = [c[0] + Math.sin(a) * 105, 46, c[2] + Math.cos(a) * 105];
    const d = [c[0] - cam.eye[0], 6 - cam.eye[1], c[2] - cam.eye[2]], n = Math.hypot(d[0], d[1], d[2]); cam.dir = d.map(v => v / n); return;
  }
  const eyeH = pl.pose === 2 ? .72 : pl.pose === 3 ? .3 : 1.51 * playerLook.S;
  cam.close = false; if (cam.fp) { cam.eye = [pl.x + Math.sin(cam.yaw) * .1, pl.y + eyeH, pl.z - Math.cos(cam.yaw) * .1]; return; }
  const t = [pl.x, pl.y + (pl.pose > 1 ? eyeH + .3 : 1.5), pl.z]; let d = .3;
  for (; d < cam.dist; d += .12) if (blocked([t[0] - cam.dir[0] * d, t[1] - cam.dir[1] * d, t[2] - cam.dir[2] * d])) break;
  d = Math.max(.35, d - .3); cam.close = d < 1.3;      // with a wall right behind, look through the player's eyes instead of at the back of their head
  cam.eye = cam.close ? [pl.x, pl.y + eyeH, pl.z] : [t[0] - cam.dir[0] * d, t[1] - cam.dir[1] * d, t[2] - cam.dir[2] * d];
}

// ---------------------------------------------------------------- HUD, labels and maps
const $ = id => document.getElementById(id);
const hhmm = m => String(Math.floor(m / 60) % 24).padStart(2, "0") + ":" + String(Math.floor(m % 60)).padStart(2, "0");
const base = document.createElement("canvas"); base.width = base.height = 900;
function drawBase() {
  const c = base.getContext("2d"), k = 900 / (CITY + 40), o = 20;
  c.fillStyle = "#3a3d2c"; c.fillRect(0, 0, 900, 900); c.fillStyle = "#4b4745"; c.fillRect((o - SW / 2) * k, (o - SW / 2) * k, (CITY + SW) * k, (CITY + SW) * k);
  for (const b of D.blocks) {
    c.fillStyle = b.template === "park" ? "#5d7343" : "#77705f"; c.fillRect((b.origin[0] + o - 2) * k, (b.origin[1] + o - 2) * k, (b.size + 4) * k, (b.size + 4) * k);
    for (const l of b.lots) { c.fillStyle = KIND_CSS[l.kind]; c.fillRect((l.rect[0] + o) * k, (l.rect[1] + o) * k, (l.rect[2] - l.rect[0]) * k, (l.rect[3] - l.rect[1]) * k); }
  }
  c.strokeStyle = "#2a2a30"; c.lineWidth = 1.5; c.beginPath(); c.moveTo((o - 8) * k, (TRAM_Z + o) * k); c.lineTo((CITY + o + 8) * k, (TRAM_Z + o) * k); c.stroke();
  return { k, o };
}
let mapScale, people = [];
function drawMaps() {
  const { k, o } = mapScale, mc = $("mini").getContext("2d"), S = 408, span = 230, z = S / span;
  mc.save(); mc.clearRect(0, 0, S, S); mc.beginPath(); mc.arc(S / 2, S / 2, S / 2, 0, 7); mc.clip();
  mc.fillStyle = "#3a3d2c"; mc.fillRect(0, 0, S, S);
  mc.drawImage(base, (pl.x + o - span / 2) * k, (pl.z + o - span / 2) * k, span * k, span * k, 0, 0, S, S);
  mc.fillStyle = "#17171c"; for (const q of people) { const x = (q[0] - pl.x) * z + S / 2, y = (q[1] - pl.z) * z + S / 2; if (x > 0 && x < S && y > 0 && y < S) mc.fillRect(x - 2, y - 2, 4, 4); }
  const marks = Life.marks();                             // quest people and places (life.js, QST-005)
  for (const [x0, z0, kind] of marks) { const x = clamp((x0 - pl.x) * z + S / 2, 12, S - 12), y = clamp((z0 - pl.z) * z + S / 2, 12, S - 12); mc.fillStyle = kind ? "#f0c35a" : "#9fd1c4"; mc.strokeStyle = "#17171c"; mc.lineWidth = 2; mc.beginPath(); mc.arc(x, y, 7, 0, 7); mc.fill(); mc.stroke(); }
  mc.translate(S / 2, S / 2); mc.rotate(cam.yaw); mc.fillStyle = "#f0a35a"; mc.strokeStyle = "#17171c"; mc.lineWidth = 2; mc.beginPath(); mc.moveTo(0, -13); mc.lineTo(9, 10); mc.lineTo(0, 5); mc.lineTo(-9, 10); mc.closePath(); mc.fill(); mc.stroke(); mc.restore();
  if (!showMap) return;
  const bc = $("big").getContext("2d"); bc.drawImage(base, 0, 0);
  bc.fillStyle = "#17171c"; for (const q of people) bc.fillRect((q[0] + o) * k - 1.5, (q[1] + o) * k - 1.5, 3, 3);
  for (const [x, z0, kind] of marks) { bc.fillStyle = kind ? "#f0c35a" : "#9fd1c4"; bc.strokeStyle = "#17171c"; bc.lineWidth = 2; bc.beginPath(); bc.arc((x + o) * k, (z0 + o) * k, 8, 0, 7); bc.fill(); bc.stroke(); }
  bc.fillStyle = "#f0a35a"; bc.strokeStyle = "#17171c"; bc.lineWidth = 2; bc.beginPath(); bc.arc((pl.x + o) * k, (pl.z + o) * k, 7, 0, 7); bc.fill(); bc.stroke();
}
function placeName() {
  const l = lotAt(pl.x, pl.z, true), b = blockOf(pl.x, pl.z);
  if (pl.y > 6 && l) return [l.name, "On the roof, " + Math.round(pl.y) + " m up"];
  if (l && !isOpen(l)) return [l.name, "Inside"];
  if (l && l.kind !== "stop") return [l.name, b.district];
  if (pl.x < -SW / 2 || pl.z < -SW / 2 || pl.x > CITY + SW / 2 || pl.z > CITY + SW / 2) return ["Outside the walls", ""];
  const i = Math.floor(pl.x / P), j = Math.floor(pl.z / P), u = pl.x - i * P, v = pl.z - j * P, ns = D.city.streets.north_south, ew = D.city.streets.east_west;
  const du = Math.min(u, P - u), dv = Math.min(v, P - v);
  if (Math.min(du, dv) < SW / 2 + PAVE) return [(du < dv ? ns[u < P / 2 ? i : i + 1] : ew[v < P / 2 ? j : j + 1]) + " Street", b ? b.district : ""];
  return [b ? b.district : D.city.name, b && b.template === "rows" ? "Back gardens" : "Yard"];
}
function hud(VP, W, H, tags) {
  $("time").textContent = hhmm(clock.m);
  $("date").textContent = (Life.ready ? "Day " + (clock.day + 1) + " · " : "") + D.day.weekday + " · " + D.day.season + (D.day.market ? " · market day" : "") + (clock.rate !== 1 ? "  (clock ×" + clock.rate + ")" : "");
  const [a, b] = placeName(); $("place").textContent = a; $("sub").textContent = b;
  $("stats").innerHTML = bodies + " of " + D.residents.length + " residents have bodies<br>" + Math.round(fps) + " frames a second" + (quality < 1 ? " · soft picture" : "");
  const pr = $("prompt"); if (started && (target || pl.pose > 1)) { pr.style.display = "block"; pr.innerHTML = "<kbd>E</kbd>" + (pl.pose === 4 ? Cars.label() : pl.pose > 1 ? "Get up" : target.label); } else pr.style.display = "none";
  $("dot").style.display = started && cam.fp ? "block" : "none";
  g2.clearRect(0, 0, W, H); if (!started) return;
  const inLot = lotAt(pl.x, pl.z), sc = W / ui.clientWidth;
  const put = (x, y, z, text, alpha, size, font) => {
    const w = VP[3] * x + VP[7] * y + VP[11] * z + VP[15]; if (w < .3) return;
    const sx = ((VP[0] * x + VP[4] * y + VP[8] * z + VP[12]) / w * .5 + .5) * W, sy = (1 - ((VP[1] * x + VP[5] * y + VP[9] * z + VP[13]) / w * .5 + .5)) * H;
    g2.globalAlpha = alpha; g2.font = font + " " + Math.round(size * sc) + "px " + (font === "600" ? "Bahnschrift, 'Segoe UI', sans-serif" : "Constantia, 'Palatino Linotype', Georgia, serif");
    g2.lineWidth = 4 * sc; g2.strokeStyle = "rgba(23,23,28,.85)"; g2.strokeText(text, sx, sy); g2.fillStyle = "#e6dcc3"; g2.fillText(text, sx, sy);
  };
  g2.textAlign = "center"; g2.lineJoin = "round";
  for (const L of labels) {
    const d = Math.hypot(L.x - pl.x, L.z - pl.z); if (d > L.r) continue;
    if (L.indoor ? inLot !== L.lot : (inLot && pl.y < 6)) continue;
    put(L.x, L.y, L.z, L.text, clamp((L.r - d) / (L.r * .35), 0, 1), d < 14 ? 19 : 16, "400");
  }
  for (const t of tags) put(t.x, t.y, t.z, t.text, t.a !== undefined ? t.a : clamp((9 - t.d) / 3, 0, 1), t.size || 14, "600");
  g2.globalAlpha = 1;
}

// ---------------------------------------------------------------- start up
buildWorld(); Dress.init();                              // which play spot is on which swing or slide (dressing.js)
if (RANGES[RANGES.length - 1][1] !== world.n) fail("World pieces do not cover the whole town mesh.");
const worldBuf = buffer(world, gl.STATIC_DRAW), worldN = world.n / 10, skyMap = bakeSky();
const pieces = RANGES.filter(r => r[1] > r[0]).map(([s, e, town]) => ({ first: s / 10, count: (e - s) / 10, town, box: boxOf(world.a, s, e) }));
world.a = null;
const chunks = [...DETAIL.values()].map(m => { const c = { buf: buffer(m, gl.STATIC_DRAW), n: m.n / 10, cx: m.cx, cz: m.cz, box: boxOf(m.a, 0, m.n) }; m.a = null; return c; });
const signBuf = buffer(signs, gl.STATIC_DRAW), signN = signs.n / 10; signs.a = null;
const detailN = chunks.reduce((s, c) => s + c.n, 0);
const txtTex = gl.createTexture(); gl.bindTexture(gl.TEXTURE_2D, txtTex); gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, TXT); gl.generateMipmap(gl.TEXTURE_2D);
for (const [kk, vv] of [[gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR], [gl.TEXTURE_MAG_FILTER, gl.LINEAR], [gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE], [gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE]]) gl.texParameteri(gl.TEXTURE_2D, kk, vv);
const dyn = new Mesh(1 << 18), dynBuf = gl.createBuffer(), skyBuf = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, skyBuf); gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);
mapScale = drawBase();
$("lede").textContent = D.city.name + ", a " + D.day.season.toLowerCase() + " " + ({ Mon: "Monday", Tue: "Tuesday", Wed: "Wednesday", Thu: "Thursday", Fri: "Friday", Sat: "Saturday", Sun: "Sunday" })[D.day.weekday] + (D.day.market ? ", market day" : "") + ". " +
  D.blocks.length + " blocks, " + LOTS.filter(l => !isOpen(l) && l.enterable).length + " buildings you can walk into and " + D.residents.length + " residents going about their day.";
$("legend").innerHTML = Object.keys(KIND_WORD).map(k => `<div><i style="background:${KIND_CSS[k]}"></i>${KIND_WORD[k]}</div>`).join("") + `<div><i style="background:#17171c;width:5px;height:5px"></i>Residents</div><div><i style="background:#f0a35a;border-radius:50%"></i>You</div>`;

function begin() { Sound.start(); started = true; $("start").style.display = "none"; $("keys").style.display = "block"; if (cv.requestPointerLock) { const r = cv.requestPointerLock(); if (r && r.catch) r.catch(() => { }); } }
$("go").addEventListener("click", () => Life.go(begin));   // carry on, or the character designer first (life.js)
cv.addEventListener("click", () => { if (started && !locked && !Talk.on && !Life.on && cv.requestPointerLock) { const r = cv.requestPointerLock(); if (r && r.catch) r.catch(() => { }); } });
document.addEventListener("pointerlockchange", () => { locked = document.pointerLockElement === cv; });
let drag = false; cv.addEventListener("mousedown", () => drag = true); addEventListener("mouseup", () => drag = false);
addEventListener("mousemove", e => { if (Life.designing && drag) return Life.spin(e.movementX); if (!started || !(locked || drag)) return; cam.yaw += e.movementX * .0024; cam.pitch = clamp(cam.pitch - e.movementY * .0024, -1.35, 1.35); });
addEventListener("wheel", e => { cam.dist = clamp(cam.dist + Math.sign(e.deltaY) * .6, 2.2, 14); });
const RATES = [1, 5, 20, 100];
addEventListener("keydown", e => {
  if (e.code === "Space" || e.code.startsWith("Arrow")) e.preventDefault();
  Sound.start();                                         // browsers allow sound only after a key or a click
  if (!started) { if (e.code === "Enter" && !Life.designing && document.activeElement === document.body) Life.go(begin); return; }
  if (Life.on) { Life.key(e); return; }                  // a counter or the quest log is open: Esc closes it (life.js)
  if (Talk.on) { Talk.key(e); return; }                  // number keys answer, Esc leaves
  if (e.repeat) return; keys[e.code] = true;
  if (e.code === "KeyV") cam.fp = !cam.fp;
  else if (e.code === "KeyE") interact();
  else if (e.code === "KeyN") Sound.toggle();
  else if (e.code === "KeyB" && pl.pose !== 4) Life.counter();   // buy, sell, bank or stall where you stand (life.js)
  else if (e.code === "KeyI") Life.panel("wallet");     // money and what you carry
  else if (e.code === "KeyJ") Life.panel("quests");     // the quest log
  else if (e.code === "KeyF" && pl.pose === 4) Sound.horn(true);
  else if (e.code === "KeyR" && pl.pose === 4) Sound.radio();
  else if (e.code === "KeyM") { Sound.act("map"); showMap = !showMap; $("bigmap").style.display = showMap ? "flex" : "none"; }
  else if (e.code === "KeyH") $("keys").style.display = $("keys").style.display === "none" ? "block" : "none";
  else if (e.code.startsWith("Bracket")) Sound.act("clock");
  if (e.code === "BracketRight") clock.rate = RATES[Math.min(RATES.length - 1, RATES.indexOf(clock.rate) + 1)];
  else if (e.code === "BracketLeft") clock.rate = RATES[Math.max(0, RATES.indexOf(clock.rate) - 1)];
  else if (e.code === "KeyT") { if (clock.m + 60 >= 1440) clock.day++; clock.m = (clock.m + 60) % 1440; Sound.act("hour"); }
  else if (e.code === "KeyQ") quality = quality > .75 ? .5 : 1;
});
addEventListener("keyup", e => { keys[e.code] = false; if (e.code === "KeyF") Sound.horn(false); });
addEventListener("pointerdown", () => Sound.start());
addEventListener("blur", () => { for (const k in keys) keys[k] = false; });

// Rooms are furnished only near the camera: built when a building comes within 40 m (two a frame at most), dropped past 70 m.
const ROOMS = new Map();
// A room's light: its lamps, and a map seen from above of how much of the floor round each spot is covered by furniture, blurred,
// so floors darken round and under what stands on them and furniture darkens at its foot (contact shade, as baked in Blender).
const ROOM_AO_CELL = .2;
function roomAO(l, foot) {
  const [x0, z0, x1, z1] = l.rect, w = Math.max(2, Math.ceil((x1 - x0) / ROOM_AO_CELL)), hgt = Math.max(2, Math.ceil((z1 - z0) / ROOM_AO_CELL)), g = new Float32Array(w * hgt);
  for (const [cx, cz, hx, hz, wt] of foot) {
    const i0 = Math.max(0, Math.floor((cx - hx - x0) / ROOM_AO_CELL)), i1 = Math.min(w - 1, Math.floor((cx + hx - x0) / ROOM_AO_CELL));
    const j0 = Math.max(0, Math.floor((cz - hz - z0) / ROOM_AO_CELL)), j1 = Math.min(hgt - 1, Math.floor((cz + hz - z0) / ROOM_AO_CELL));
    for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) g[j * w + i] = Math.max(g[j * w + i], wt);
  }
  const blur = (src, r) => { const t = new Float32Array(w * hgt), o = new Float32Array(w * hgt), n = 2 * r + 1;     // a box blur, across then down
    for (let j = 0; j < hgt; j++) for (let i = 0; i < w; i++) { let s = 0; for (let k = -r; k <= r; k++) s += src[j * w + Math.min(w - 1, Math.max(0, i + k))]; t[j * w + i] = s / n; }
    for (let j = 0; j < hgt; j++) for (let i = 0; i < w; i++) { let s = 0; for (let k = -r; k <= r; k++) s += t[Math.min(hgt - 1, Math.max(0, j + k)) * w + i]; o[j * w + i] = s / n; }
    return o; };
  const a = blur(blur(g, 2), 1), px = new Uint8Array(w * hgt);
  for (let k = 0; k < w * hgt; k++) px[k] = Math.round(255 * Math.min(1, Math.max(g[k] * .8, a[k] * 1.8)));
  const tex = gl.createTexture(); gl.bindTexture(gl.TEXTURE_2D, tex); gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.LUMINANCE, w, hgt, 0, gl.LUMINANCE, gl.UNSIGNED_BYTE, px); gl.pixelStorei(gl.UNPACK_ALIGNMENT, 4);
  for (const [k, v] of [[gl.TEXTURE_MIN_FILTER, gl.LINEAR], [gl.TEXTURE_MAG_FILTER, gl.LINEAR], [gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE], [gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE]]) gl.texParameteri(gl.TEXTURE_2D, k, v);
  gl.bindTexture(gl.TEXTURE_2D, null); return tex;
}
function nearRooms(eye) {
  let built = 0;
  for (const l of LOTS) {
    if (!l.furn) continue;
    const [a0, b0, a1, b1] = l.rect, dd = Math.hypot((a0 + a1) / 2 - eye[0], (b0 + b1) / 2 - eye[2]) - Math.hypot(a1 - a0, b1 - b0) / 2, have = ROOMS.get(l);
    if (!have && dd < 40 && built < 2) {
      const m = new Mesh(1 << 15); ROOM_REC = { lamps: [], foot: [] }; furnish(l, l.furn[0], l.furn[1], l.furn[2], m); built++;
      const rec = ROOM_REC; ROOM_REC = null;
      ROOMS.set(l, m.n ? { buf: buffer(m, gl.STATIC_DRAW), n: m.n / 10, box: boxOf(m.a, 0, m.n), lot: l, lamps: rec.lamps, ao: roomAO(l, rec.foot) } : { n: 0, lot: l, lamps: [] });
    } else if (have && dd > 70) { if (have.buf) gl.deleteBuffer(have.buf); if (have.ao) gl.deleteTexture(have.ao); ROOMS.delete(l); }
  }
  const out = []; for (const r of ROOMS.values()) if (r.n) out.push(r); return out;
}
let last = performance.now(); const stat = { detail: 0 }; const PIN_Q = new URLSearchParams(location.search).has("quality");   // ?quality=1 or 0.5 holds the picture for timing
if (PIN_Q) quality = clamp(+new URLSearchParams(location.search).get("quality") || 1, .5, 1);
function frame(now) {
  const dt = Math.min(.05, (now - last) / 1000); last = now; fps = mix(fps, 1 / Math.max(dt, 1e-3), .05);
  if (fps < 27 && quality > .5 && !PIN_Q) { slow += dt; if (slow > 3) { quality = Math.max(.5, quality - .25); slow = 0; } } else slow = 0;      // a slow machine gets a softer picture, not a slow game
  const dpr = Math.min(window.devicePixelRatio || 1, 1.5) * quality, W = Math.round(cv.clientWidth * dpr), H = Math.round(cv.clientHeight * dpr);
  if (cv.width !== W || cv.height !== H) { cv.width = ui.width = W; cv.height = ui.height = H; }
  const m0 = clock.m; clock.m = (clock.m + dt / D.pace * clock.rate) % 1440; if (clock.m < m0) clock.day++;
  if (started && !Talk.on && !Life.on) { stepPlayer(dt); findTarget(); } else if (started && Talk.on) Talk.step(dt);   // a counter or the quest log (life.js) also holds the player still   // talking (conversations.js) holds the player still
  stepCamera(dt);

  // everyone's place right now; bodies only for those near the player
  Dodge.frame(now / 1000, pl);                             // is the player's car bearing down on anyone (dodge.js, CITY-070)
  dyn.reset(); people = []; bodies = 0; const heard = [], tags = [], near = started ? [pl.x, pl.z] : [cam.eye[0] + cam.dir[0] * 60, cam.eye[2] + cam.dir[2] * 60], reach = started ? 85 : 130;
  for (let i = 0; i < D.plan.length; i++) {
    const r = Dress.apply(i, Dodge.apply(i, Talk.where(i, clock.m)), now / 1000); if (!r) continue;   // children on the swings (dressing.js)   // out of the way of the player's car (dodge.js)
    people.push([r.x, r.z]);
    const d = Math.hypot(r.x - near[0], r.z - near[1]); if (d > reach) continue;
    if (d < 20) heard.push({ i, x: r.x, z: r.z, act: D.acts[r.act], walking: r.walking, pose: r.pose, ph: r.ph, inside: r.inside });
    bodies++;
    const L = looks[i], dc = Math.hypot(r.x - cam.eye[0], r.z - cam.eye[2]); let lk = null;
    if (started && r.pose !== 3 && d < 5.5 && Math.abs(r.y - pl.y) < 2) {          // someone near turns to look: eyes first, head part of the way (M4)
      let da = yawOf(pl.x - r.x, pl.z - r.z) - r.yaw; da = Math.atan2(Math.sin(da), Math.cos(da));
      if (Math.abs(da) < 1.9) { const w = smooth(5.5, 3.2, d); lk = [clamp(-da * .6, -.85, .85) * w, 0]; }
    }
    People.draw(dyn, r.x, r.y, r.z, r.yaw, L, r.pose, { ph: r.ph || 0, t: now / 1000, lod: dc < 13 ? 0 : dc < 40 ? 1 : 2, fl: r.inside ? 10 : 14, look: lk, sleep: r.pose === 3, face: Talk.face(i), mod: r.mod, sig: r.sig });
    if (started && d < 9 && Math.abs(r.y - pl.y) < 3) tags.push({ x: r.x, y: r.y + (r.pose === 2 ? 1.0 : r.pose === 3 ? .55 : 1.88) * L.S, z: r.z, d, text: D.residents[i].n + ", " + D.roles[D.residents[i].r] + " · " + (r.walking ? GOING[D.acts[r.act]] || "walking" : DOING[D.acts[r.act]] || D.acts[r.act]) });
    if (started && r.shout && d < D.dodge.tag_m) tags.push({ x: r.x, y: r.y + 2.18 * L.S, z: r.z, d, a: clamp((D.dodge.tag_m - d) / 4, 0, 1), size: 18, text: r.shout });   // telling the driver off (CITY-071)
    if (started) Talk.consider(i, r, d);
    if (started) Life.mark(i, r, d, L, tags);             // someone with a quest or a job for the player (life.js, QST-005)
  }
  if (started) Talk.pick();                               // the nearest resident in reach can be talked to (E)
  if (started) Life.frame(dt);                            // money, quests and jobs move on with the clock (life.js)
  if ((started || Life.designing) && !cam.fp && !cam.close && pl.pose !== 4) {   // not while in a car; and in the designer, to see who you are making
    let da = cam.yaw - pl.yaw; da = Math.atan2(Math.sin(da), Math.cos(da));
    People.draw(dyn, pl.x, pl.y, pl.z, pl.yaw, playerLook, pl.pose > 1 ? pl.pose : pl.moving ? 1 : 0, { ph: pl.ph, t: now / 1000, lod: 0, fl: lotAt(pl.x, pl.z) && pl.y < 6 ? 10 : 14,
      look: pl.pose === 3 ? null : [clamp(-da, -1, 1) * .7, clamp(-cam.pitch, -.5, .5) * .5], sig: pl.moving && pl.run ? People.SIG.Run : null });
  }
  for (const [x, , T] of tramAt(clock.m)) Cars.tram(dyn, x, TRAM_Z, T, clock.m);   // the tram and the commuters on it (cars.js)
  stat.cars = Cars.draw(dyn, clock.m, near, reach);
  Dress.draw(dyn, cam.eye, now / 1000);                   // swing seats and seesaws (dressing.js)       // residents' cars, parked and driving (cars.js)
  if (started) {                                          // what the listener hears (sound.js): where they are and what is near
    const lot = pl.y < 6 ? lotAt(pl.x, pl.z) : null, drv = pl.pose === 4;
    Sound.frame({ m: clock.m, dt, x: pl.x, y: pl.y, z: pl.z, yaw: cam.yaw, lot, talk: Talk.on, residents: heard, trams: tramAt(clock.m), tramZ: TRAM_Z,
      cars: Cars.near ? Cars.near(clock.m, pl.x, pl.z, 75) : [],
      walk: drv ? null : { moving: pl.moving && pl.pose < 2, ph: pl.ph, run: pl.run, onGround: pl.onGround, vy: pl.vy, surface: lot ? "wood" : (lotAt(pl.x, pl.z, true) || {}).kind === "garden" ? "grass" : "stone" },
      drive: drv ? { v: Cars.speed ? Cars.speed() : 0, gas: !!(keys.KeyW || keys.ArrowUp), brake: !!(keys.Space || ((keys.KeyS || keys.ArrowDown) && (Cars.speed ? Cars.speed() : 0) > 0)) } : null });
  }

  const Lt = lighting(clock.m), dials = window.wasted.dials; Lt.sunCol = mul3(Lt.sunCol, dials.sun); Lt.sky = mul3(Lt.sky, dials.sky); Lt.ground = mul3(Lt.ground, dials.bounce); Lt.fogD *= dials.haze; Lt.exp *= dials.exposure;
  const wm = dials.warm || 0; Lt.sunCol = [Lt.sunCol[0], Lt.sunCol[1] * (1 - .1 * wm), Lt.sunCol[2] * (1 - .26 * wm)]; Lt.sky = [Lt.sky[0] * (1 - .08 * wm), Lt.sky[1] * (1 - .04 * wm), Lt.sky[2]];   // a more golden sun against a deeper sky
  const view = M4.look(cam.eye, cam.dir), fov = .9, asp = W / H, VP = M4.mul(M4.persp(fov, asp, .25, 3200), view.m), th = Math.tan(fov / 2);
  gl.bindBuffer(gl.ARRAY_BUFFER, dynBuf); gl.bufferData(gl.ARRAY_BUFFER, dyn.a.subarray(0, dyn.n), gl.DYNAMIC_DRAW);
  let SM = M4.t(0, 0, 0), shOn = 0;
  if (shFbo && quality > .6) {                              // shadows: draw the town from the light, around where the player looks
    const R = 70, tex = 2 * R / SH, f = [-Lt.dir[0], -Lt.dir[1], -Lt.dir[2]], lv0 = M4.look([0, 0, 0], f), dot3 = (p, q) => p[0] * q[0] + p[1] * q[1] + p[2] * q[2];
    let foc = [near[0] + cam.dir[0] * 28, 0, near[1] + cam.dir[2] * 28]; const ox = dot3(foc, lv0.r) % tex, oy = dot3(foc, lv0.u) % tex;
    foc = [foc[0] - lv0.r[0] * ox - lv0.u[0] * oy, foc[1] - lv0.r[1] * ox - lv0.u[1] * oy, foc[2] - lv0.r[2] * ox - lv0.u[2] * oy];   // move in whole texels so shadows do not crawl
    const lv = M4.look([foc[0] + Lt.dir[0] * 260, foc[1] + Lt.dir[1] * 260, foc[2] + Lt.dir[2] * 260], f), nn = 1, ff = 520;
    SM = M4.mul([1 / R, 0, 0, 0, 0, 1 / R, 0, 0, 0, 0, -2 / (ff - nn), 0, 0, 0, -(ff + nn) / (ff - nn), 1], lv.m); shOn = 1;
    gl.bindFramebuffer(gl.FRAMEBUFFER, shFbo); gl.viewport(0, 0, SH, SH); gl.colorMask(false, false, false, false); gl.enable(gl.DEPTH_TEST); gl.depthFunc(gl.LESS); gl.depthMask(true); gl.clear(gl.DEPTH_BUFFER_BIT);
    gl.disable(gl.CULL_FACE); gl.enable(gl.POLYGON_OFFSET_FILL); gl.polygonOffset(1.5, 3);
    gl.useProgram(depthProg.p); gl.uniformMatrix4fv(depthProg.u.uVP, false, SM);
    drawDepth(worldBuf, 0); for (const pc of pieces) if (!outside(SM, pc.box)) gl.drawArrays(gl.TRIANGLES, pc.first, pc.count);   // only the pieces inside the sun's box
    drawDepth(dynBuf, dyn.n / 10); drawDepth(signBuf, signN);
    for (const ch of chunks) if (Math.hypot(ch.cx - foc[0], ch.cz - foc[2]) < R + 25 && !outside(SM, ch.box)) drawDepth(ch.buf, ch.n);
    gl.disable(gl.POLYGON_OFFSET_FILL); gl.colorMask(true, true, true, true); gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }
  // the 12 lanterns nearest the camera light the scene; the rest only glow
  const lamp = new Float32Array(48), pickL = [];
  for (const l of lights) { if (!l[4] && Lt.night < .02) continue; const d2 = (l[0] - cam.eye[0]) ** 2 + (l[2] - cam.eye[2]) ** 2; if (d2 < 5600) pickL.push([d2, l]); }   // wall lanterns in the shade stay lit by day, as in the reference
  pickL.sort((p, q) => p[0] - q[0]); const lampN = Math.min(12, pickL.length);
  for (let k = 0; k < Math.min(12, pickL.length); k++) { const [d2, l] = pickL[k]; lamp.set([l[0], l[1], l[2], l[3] * 3 * (l[4] ? 1 + Lt.night : 2 * Lt.night) * dials.lanterns * smooth(5600, 3000, d2)], k * 4); }
  gl.viewport(0, 0, W, H); gl.clear(gl.DEPTH_BUFFER_BIT);
  gl.disable(gl.DEPTH_TEST); gl.disable(gl.CULL_FACE); gl.depthMask(false);
  gl.useProgram(sky.p); gl.bindBuffer(gl.ARRAY_BUFFER, skyBuf); const aq = gl.getAttribLocation(sky.p, "aQ"); gl.enableVertexAttribArray(aq); gl.vertexAttribPointer(aq, 2, gl.FLOAT, false, 0, 0);
  gl.uniform3fv(sky.u.uR, mul3(view.r, th * asp)); gl.uniform3fv(sky.u.uU, mul3(view.u, th)); gl.uniform3fv(sky.u.uF, cam.dir); gl.uniform3fv(sky.u.uSun, Lt.dir);
  gl.uniform3fv(sky.u.uZen, Lt.zen); gl.uniform3fv(sky.u.uHor, Lt.fog); gl.uniform3fv(sky.u.uSunCol, Lt.sunCol); gl.uniform1f(sky.u.uNight, Lt.night); gl.uniform1f(sky.u.uExp, Lt.exp);
  gl.drawArrays(gl.TRIANGLES, 0, 3);
  gl.enable(gl.DEPTH_TEST); gl.depthFunc(gl.LEQUAL); gl.enable(gl.CULL_FACE); gl.depthMask(true);   // on a tie the layer laid later (the upper one) wins
  const seen = pieces.filter(pc => !outside(VP, pc.box)), nearD = chunks.filter(ch => Math.hypot(ch.cx - cam.eye[0], ch.cz - cam.eye[2]) < DETAIL_R && !outside(VP, ch.box));
  const rooms = nearRooms(cam.eye); for (const rm of rooms) if (!outside(VP, rm.box)) nearD.push(rm);
  // the room the camera is in (or the nearest one in reach) gets its own lamps and contact shade; other rooms an even lamplight
  let inRoom = null, best = 10;
  for (const rm of rooms) { if (!rm.ao) continue; const [a0, b0, a1, b1] = rm.lot.rect, e = Math.hypot(Math.max(a0 - cam.eye[0], 0, cam.eye[0] - a1), Math.max(b0 - cam.eye[2], 0, cam.eye[2] - b1)) + (cam.eye[1] > GF + 1 ? 99 : 0); if (e < best) { best = e; inRoom = rm; } }
  const roomL = new Float32Array(64); let roomLN = 0;
  if (inRoom) { const ls = inRoom.lamps.map(q => [(q[0] - cam.eye[0]) ** 2 + (q[2] - cam.eye[2]) ** 2, q]).sort((p, q) => p[0] - q[0]);
    for (; roomLN < Math.min(16, ls.length); roomLN++) { const q = ls[roomLN][1]; roomL.set([q[0], q[1], q[2], q[3] * dials.lanterns * (.75 + .5 * Lt.night)], roomLN * 4); } }   // furnished rooms of the buildings close by   // near relief: blocks within reach and in view
  for (const pc of seen) pc.d2 = boxD2(pc.box, cam.eye); for (const ch of nearD) ch.d2 = boxD2(ch.box, cam.eye);
  const front = seen.slice().sort((p, q) => p.d2 - q.d2); nearD.sort((p, q) => p.d2 - q.d2);
  // depth first, nearest pieces first: the costly light below then runs once for each pixel instead of for every hidden wall and room behind it
  gl.colorMask(false, false, false, false); gl.useProgram(pre.p); gl.uniformMatrix4fv(pre.u.uVP, false, VP); if (pre.u.uSM) gl.uniformMatrix4fv(pre.u.uSM, false, SM);
  attribs(pre, worldBuf); for (const pc of front) gl.drawArrays(gl.TRIANGLES, pc.first, pc.count);
  for (const ch of nearD) drawMesh(ch.buf, ch.n, pre);
  gl.colorMask(true, true, true, true); gl.depthMask(false);
  const lit = pr => {
    const u = pr.u; gl.useProgram(pr.p); gl.uniformMatrix4fv(u.uVP, false, VP); gl.uniformMatrix4fv(u.uSM, false, SM); gl.uniform1f(u.uShOn, shOn);
    gl.uniform1i(u.uSh, 0); gl.uniform1i(u.uAO, 1); gl.uniform1i(u.uTxt, 2); gl.uniform4fv(u["uLamp[0]"], lamp); gl.uniform1i(u.uLampN, lampN); gl.uniform3f(u.uAOm, skyMap.lo, 1 / skyMap.size, 0);
    gl.uniform3fv(u.uSun, Lt.dir); gl.uniform3fv(u.uSunCol, Lt.sunCol); gl.uniform3fv(u.uSky, Lt.sky); gl.uniform3fv(u.uGround, Lt.ground); gl.uniform3fv(u.uFog, Lt.fog);
    gl.uniform3fv(u.uCam, cam.eye); gl.uniform1f(u.uNight, Lt.night); gl.uniform1f(u.uFogD, Lt.fogD); gl.uniform1f(u.uExp, Lt.exp);
    gl.uniform1i(u.uRoomAO, 3); gl.uniform4fv(u["uRoomL[0]"], roomL); gl.uniform1i(u.uRoomLN, roomLN);
    if (inRoom) gl.uniform4fv(u.uRoomR, inRoom.lot.rect); else gl.uniform4f(u.uRoomR, 0, 0, -1, -1);
  };
  gl.activeTexture(gl.TEXTURE3); gl.bindTexture(gl.TEXTURE_2D, inRoom ? inRoom.ao : skyMap.tex);
  gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, shTex); gl.activeTexture(gl.TEXTURE1); gl.bindTexture(gl.TEXTURE_2D, skyMap.tex);
  gl.activeTexture(gl.TEXTURE2); gl.bindTexture(gl.TEXTURE_2D, txtTex); gl.activeTexture(gl.TEXTURE0);
  lit(main); attribs(main, worldBuf); for (const pc of seen) gl.drawArrays(gl.TRIANGLES, pc.first, pc.count);   // in the order laid, so ties fall as before
  let drawnDetail = 0; for (const ch of nearD) { drawMesh(ch.buf, ch.n); drawnDetail += ch.n; }
  stat.detail = drawnDetail; stat.pieces = seen.length;
  gl.depthMask(true); lit(signProg); drawMesh(signBuf, signN, signProg); gl.useProgram(main.p);
  gl.disable(gl.CULL_FACE); drawMesh(dynBuf, dyn.n / 10);   // people are closed soft forms; either side of a face may show
  hud(VP, W, H, tags); drawMaps();
  requestAnimationFrame(frame);
}
// for tests and screenshots: index.html?auto=1&x=..&z=..&yaw=..&pitch=..&t=..&fp=1
const q = new URLSearchParams(location.search);
if (q.has("auto")) { started = true; $("start").style.display = "none"; $("keys").style.display = "block"; }
for (const [k, o, key] of [["x", pl, "x"], ["z", pl, "z"], ["y", pl, "y"], ["yaw", cam, "yaw"], ["pitch", cam, "pitch"], ["t", clock, "m"], ["dist", cam, "dist"]]) if (q.has(k)) o[key] = +q.get(k);
if (q.has("fp")) cam.fp = true; if (q.has("yaw")) pl.yaw = cam.yaw; if (q.has("rate")) clock.rate = +q.get("rate");
// Light presets. "kit" is the daylight of ADR-0007: the softer, warmer light of the building kit renders, and the default.
// "reference" is the earlier light of the reference alley image (ADR-0004), kept for comparison: L switches, ?light=reference starts with it.
const LIGHTS = { reference: { sun: 1, sky: 1, bounce: 1, lanterns: 1, haze: 1, exposure: 1, warm: 0 }, kit: { sun: .62, sky: .9, bounce: 1.5, lanterns: 1, haze: .55, exposure: .7, warm: 1 } };
let lightName = q.get("light") === "reference" ? "reference" : "kit";
addEventListener("keydown", e => { if (e.code === "KeyL" && started && !e.repeat) { lightName = lightName === "kit" ? "reference" : "kit"; Object.assign(window.wasted.dials, LIGHTS[lightName]); } });
window.wasted = { dials: Object.assign({}, LIGHTS[lightName]), light: () => lightName, pl, cam, clock, keys, uses, solids, labels, interact, resident, groundBase, lotAt, tramAt, begin, setLook: L => { playerLook = L; }, step: secs => { for (let k = 0; k < secs * 60; k++) stepPlayer(1 / 60); findTarget(); }, stats: () => ({ verts: worldN, detailVerts: detailN, detailDrawn: stat.detail, rooms: ROOMS.size, pieces: pieces.length, piecesDrawn: stat.pieces, solids: solids.length, uses: uses.length, bodies, cars: stat.cars, fps, people: people.length, target: target && target.label }) };
// sound (sound.js): which lots play music, where the fountains and gardens are
Sound.init(LOTS, D.blocks.flatMap(b => b.props.filter(p => p.kind === "fountain").map(p => p.pos)),
  LOTS.filter(l => l.kind === "garden").map(l => [(l.rect[0] + l.rect[2]) / 2, (l.rect[1] + l.rect[3]) / 2]));
window.wasted.sound = Sound;
requestAnimationFrame(frame);
