"use strict";
// Cars in the slice (LIVING_CITY 3.4: CITY-040, CITY-043 to CITY-049, CITY-068, CITY-069). Every vehicle's day (residents'
// cars, delivery vans, cars passing through) is worked out by Tools/citysim/traffic.py and packed into city_data.js as
// D.cars by Tools/worldgen/build_play.py; nothing here decides where those go, it only places and draws them.
//
// People ride where you can see them: a resident in their car (piece kind 3 in D.plan) sits at its wheel, vans and cars
// passing through have their own driver (and sometimes a passenger), the player sits in the car they drive, and the
// commuters who boarded the tram sit in it until it leaves the map (and on the way back, from the edge to their stop).
// Cabins are open frames, no glass, so the people inside show.
//
// The player can get into any parked car or van (E), drive it, and leave it anywhere. A car the player has moved stays
// where it was left for the rest of the visit and no longer follows its owner's day. This is a stand-in for CITY-060 to
// CITY-063 (buying your own car comes with money; resident cars do not yet stop for the player, CITY-049).
//
// Loaded before play.js, like conversations.js. Nothing here touches play.js's globals (C, groundBase, blocked, yawOf,
// hash, looks, playerLook, cam, pl, LOTS, tramAt...) until play.js calls in, by which time they all exist.
//
// The look: small, round, slightly toy-like cars in the soft style of the people (ART_DIRECTION §3, "Soft"): stacked
// soft forms with no thin edges (W3), big round wheels and lamps, two muted accents from the palette (W4). Five models
// for variety. Vehicles are "not started" in ART_DIRECTION §5: these stand-ins come before a look test in Tools/blender/
// (CITY-067) and need the owner's eye.
var Cars = (() => {
  const D = window.WASTED, CARS = D.cars || [], STEP = D.car_step || .1;
  const TOP = (D.car_top_kmh || 50) / 3.6;                  // the player's car: top speed from World/rules/traffic.json, in m/s
  // model: length, width, driver seat [along, across], back-seat row (along) or null, kind of roof
  const MODELS = [
    { name: "bubble", L: 3.0, W: 1.56, seat: [-.3, -.32], back: null },
    { name: "hatch", L: 3.6, W: 1.66, seat: [-.15, -.34], back: -.95 },
    { name: "estate", L: 4.1, W: 1.70, seat: [.05, -.35], back: -.8 },
    { name: "pickup", L: 4.0, W: 1.70, seat: [.35, -.35], back: null },
    { name: "van", L: 4.6, W: 1.90, seat: [1.25, -.42], back: null },
  ];
  const taken = new Map();                               // car -> where the player left it
  let drv = null, saved = null, PAINT = null, ROOF = null, byCar = null, riders = null;
  const people = new Map();                              // looks for people who are not residents: van drivers, visitors

  // The chosen look (ADR-0015, ART_DIRECTION 2.5): the "Soft box" cars of Tools/blender/car_kit_09.py, exported by
  // car_export_11.py into car_models.js as low-poly meshes, near and far. Without that file, the box cars above stand in.
  const CM = window.CAR_MODELS, MESH = {};
  if (CM) for (const [key, md] of Object.entries(CM.models)) {
    const [a0, a1, w0, w1] = md.box;
    MESH[key] = { name: key, L: a1 - a0, W: w1 - w0, ca: (a0 + a1) / 2, seat: md.seat, seatY: md.seat_y, back: md.back,
      lods: md.lods.map(l => ({ p: Float32Array.from(l.p), n: Float32Array.from(l.n, v => v / 100), t: Uint8Array.from(l.t) })) };
  }
  const model = k => {
    if (CM) return MESH[CARS[k].k === 1 ? "van" : (h => h < .4 ? "small" : h < .72 ? "estate" : "pickup")(hash(k, "model"))];
    return CARS[k].k === 1 ? MODELS[4] : MODELS[Math.floor(hash(k, "model") * 4)];
  };
  const dims = k => { const md = model(k); return [md.L, md.W, md.ca || 0]; };

  function at(k, m) {                                    // {x, z, hx, hz, moving} of car k at minute m, or null when out of town
    if (drv && drv.k === k) return { x: drv.x, z: drv.z, hx: Math.cos(drv.a), hz: Math.sin(drv.a), moving: Math.abs(drv.v) > .2 };
    if (taken.has(k)) return taken.get(k);
    const c = CARS[k]; let bay = c.home;
    for (const t of c.trips) {
      if (m < t[0]) break;
      if (m < t[1]) {
        const p = t[3], n = p.length / 4;
        if (n < 2) return { x: p[0], z: p[1], hx: p[2], hz: p[3], moving: true, trip: true };
        const f = Math.min(n - 1, Math.max(0, (m - t[0]) / STEP)), i = Math.min(n - 2, Math.floor(f)), g = f - i, a = i * 4, b = a + 4;
        let hx = p[a + 2] + (p[b + 2] - p[a + 2]) * g, hz = p[a + 3] + (p[b + 3] - p[a + 3]) * g; const l = Math.hypot(hx, hz) || 1;
        return { x: p[a] + (p[b] - p[a]) * g, z: p[a + 1] + (p[b + 1] - p[a + 1]) * g, hx: hx / l, hz: hz / l, moving: Math.hypot(p[b] - p[a], p[b + 1] - p[a + 1]) > .05, trip: true };
      }
      bay = t[2];
    }
    return bay ? { x: bay[0], z: bay[1], hx: bay[2], hz: bay[3], moving: false } : null;
  }
  const local = (c, x, z) => { const dx = x - c.x, dz = z - c.z; return [dx * c.hx + dz * c.hz, -dx * c.hz + dz * c.hx]; };   // along, across

  // ---- shapes, in the car's own frame: a along it (front +), w across it (right +), y up ----
  function frame(m, c) {
    const P = (a, w, y) => [c.x + c.hx * a - c.hz * w, y, c.z + c.hz * a + c.hx * w];
    const N = (na, nw, ny) => [c.hx * na - c.hz * nw, ny, c.hz * na + c.hx * nw];
    return { P, N, m };
  }
  function box(F, a0, a1, w0, w1, y0, y1, col, f = 9, bottom = false) {
    const { P, N, m } = F;
    m.quad(P(a1, w0, y0), P(a1, w1, y0), P(a1, w1, y1), P(a1, w0, y1), col, f, N(1, 0, 0));
    m.quad(P(a0, w0, y0), P(a0, w1, y0), P(a0, w1, y1), P(a0, w0, y1), col, f, N(-1, 0, 0));
    m.quad(P(a0, w1, y0), P(a1, w1, y0), P(a1, w1, y1), P(a0, w1, y1), col, f, N(0, 1, 0));
    m.quad(P(a0, w0, y0), P(a1, w0, y0), P(a1, w0, y1), P(a0, w0, y1), col, f, N(0, -1, 0));
    m.quad(P(a0, w0, y1), P(a1, w0, y1), P(a1, w1, y1), P(a0, w1, y1), col, f, N(0, 0, 1));
    if (bottom) m.quad(P(a0, w0, y0), P(a1, w0, y0), P(a1, w1, y0), P(a0, w1, y0), col, f, N(0, 0, -1));
  }
  function soft(F, a0, a1, w0, w1, y0, y1, col, r, f) { // a box with its top and bottom edges taken off: reads round (W3)
    box(F, a0, a1, w0, w1, y0 + r, y1 - r, col, f);
    box(F, a0 + r, a1 - r, w0 + r, w1 - r, y1 - r, y1, col, f);
    box(F, a0 + r * .6, a1 - r * .6, w0 + r * .6, w1 - r * .6, y0, y0 + r, col, f, true);
  }
  function disc(F, ca, cy, cw, r, depth, axis, col, f = 9) {   // an eight-sided round: a wheel (axis "w") or a lamp (axis "a")
    const { P, N, m } = F, n = 8, pts = [];
    for (let i = 0; i < n; i++) { const t = (i + .5) / n * Math.PI * 2; pts.push([Math.cos(t) * r, Math.sin(t) * r]); }
    const at3 = (u, v, d) => axis === "w" ? P(ca + u, cw + d, cy + v) : P(ca + d, cw + u, cy + v);
    const nn = (u, v, d) => axis === "w" ? N(u, d, v) : N(d, u, v);
    for (let i = 0; i < n; i++) {
      const [u0, v0] = pts[i], [u1, v1] = pts[(i + 1) % n];
      m.quad(at3(u0, v0, -depth / 2), at3(u1, v1, -depth / 2), at3(u1, v1, depth / 2), at3(u0, v0, depth / 2), col, f, nn(u0 + u1, v0 + v1, 0));
      for (const s of [-1, 1]) m.quad(at3(0, 0, s * depth / 2), at3(u0, v0, s * depth / 2), at3(u1, v1, s * depth / 2), at3(0, 0, s * depth / 2), col, f, nn(0, 0, s));
    }
  }
  function wheels(F, y, L, W, r, axles) {
    for (const a of axles) for (const s of [-1, 1]) {
      disc(F, a, y + r, s * (W / 2 - .1), r, .26, "w", C.charcoal);
      disc(F, a, y + r, s * (W / 2 + .035), r * .45, .04, "w", [.55, .53, .5]);      // a pale hub, so the wheel reads round
    }
  }
  function lamps(F, a, y, ws, r, col) { for (const w of ws) disc(F, a, y, w, r, .07, "a", col); }
  function pillars(F, a0, a1, w, y0, y1, col) {          // the posts of an open cabin: thick enough not to read as thin planes
    for (const a of [a0, a1]) for (const s of [-1, 1]) box(F, a - .06, a + .06, s * w - .07, s * w + .07, y0, y1, col);
  }
  function seats(F, a, w, y, col, two) {                 // seat backs, so an empty car still reads as a car
    for (const s of two ? [-1, 1] : [-1]) box(F, a - .32, a - .2, s * Math.abs(w) - .24, s * Math.abs(w) + .24, y, y + .55, col);
  }

  function body(m, c, k, y) {
    if (CM) return meshCar(m, c, k, y, model(k));
    if (!PAINT) { PAINT = [C.teal, C.terracotta, C.ochre, C.blue, C.olive, C.cream, C.plum, [.55, .40, .34], [.42, .47, .40]]; ROOF = [C.cream, [.74, .70, .62]]; }
    const md = model(k), r = (t) => hash(k, t), F = frame(m, c), L = md.L, W = md.W, h = L / 2, w = W / 2;
    const col = CARS[k].k === 1 ? [C.cream, C.teal, C.ochre, [.55, .40, .34]][k % 4] : PAINT[Math.floor(r("col") * PAINT.length)];
    const roof = r("two") < .55 ? ROOF[Math.floor(r("roof") * 2)] : col, trim = [.30, .29, .29], inside = [.24, .17, .12];
    const light = C.cream, rear = [.55, .25, .2];
    if (md.name === "bubble") {                          // a round little two-seater
      soft(F, -h, h, -w, w, y + .26, y + .82, col, .14);
      soft(F, -h + .25, h - .45, -w + .06, w - .06, y + .82, y + .94, col, .06);
      pillars(F, .55, -1.05, w - .12, y + .9, y + 1.58, roof);
      soft(F, -1.2, .7, -w + .06, w - .06, y + 1.56, y + 1.76, roof, .07);
      box(F, .42, .62, -w + .15, w - .15, y + .86, y + .98, inside);
      seats(F, md.seat[0], md.seat[1], y + .66, inside, true);
      lamps(F, h - .02, y + .62, [-w + .32, w - .32], .14, light); lamps(F, -h + .02, y + .6, [-w + .3, w - .3], .08, rear);
      box(F, h - .08, h + .06, -w + .1, w - .1, y + .3, y + .42, trim); box(F, -h - .06, -h + .08, -w + .1, w - .1, y + .3, y + .42, trim);
      if (r("spare") < .5) disc(F, -h - .14, y + .72, 0, .26, .2, "a", C.charcoal);
      wheels(F, y, L, W, .3, [h - .62, -h + .62]);
    } else if (md.name === "hatch" || md.name === "estate") {
      const est = md.name === "estate", front = est ? .95 : .8, back = est ? -1.85 : -1.5;
      soft(F, -h, h, -w, w, y + .28, y + .84, col, .13);
      if (r("belt") < .5) box(F, -h + .02, h - .02, -w - .01, w + .01, y + .62, y + .7, roof === col ? C.cream : roof);   // a belt stripe
      soft(F, -h + .15, h - .2, -w + .05, w - .05, y + .84, y + .95, col, .05);
      pillars(F, front, back, w - .12, y + .92, y + 1.6, roof);
      if (est) pillars(F, -.45, -.45, w - .12, y + .92, y + 1.6, roof);
      soft(F, back - .12, front + .1, -w + .05, w - .05, y + 1.58, y + 1.75, roof, .07);
      box(F, front - .2, front + .05, -w + .15, w - .15, y + .88, y + 1.0, inside);
      seats(F, md.seat[0], md.seat[1], y + .66, inside, true); seats(F, md.back, md.seat[1], y + .66, inside, true);
      if (est && r("rack") < .7) {                       // a roof rack with luggage
        for (const a of [-1.2, .2]) box(F, a - .04, a + .04, -w + .12, w - .12, y + 1.75, y + 1.81, trim);
        box(F, -1.4, -.5, -w + .25, .1, y + 1.81, y + 2.13, [C.brown, C.ochre, C.terracotta][Math.floor(r("bag") * 3)], 9);
        if (r("bag2") < .6) box(F, -.4, .25, -.1, w - .3, y + 1.81, y + 2.03, C.cream, 9);
      }
      lamps(F, h - .02, y + .64, [-w + .36, w - .36], .13, light); lamps(F, -h + .02, y + .62, [-w + .3, w - .3], .09, rear);
      box(F, h - .08, h + .07, -w + .08, w - .08, y + .3, y + .44, trim); box(F, -h - .07, -h + .08, -w + .08, w - .08, y + .3, y + .44, trim);
      if (!est && r("spare") < .35) disc(F, -h - .14, y + .76, 0, .27, .2, "a", C.charcoal);
      wheels(F, y, L, W, .31, [h - .72, -h + .72]);
    } else if (md.name === "pickup") {                   // a cab and an open bed with crates
      soft(F, -h, h, -w, w, y + .3, y + .82, col, .12);
      soft(F, .1, h - .25, -w + .05, w - .05, y + .82, y + .93, col, .05);
      pillars(F, .95, -.2, w - .12, y + .9, y + 1.6, roof);
      soft(F, -.32, 1.05, -w + .05, w - .05, y + 1.58, y + 1.75, roof, .07);
      box(F, -.32, -.2, -w + .08, w - .08, y + .82, y + 1.6, roof);                         // back of the cab
      box(F, .75, .98, -w + .15, w - .15, y + .86, y + .98, inside);
      seats(F, md.seat[0], md.seat[1], y + .66, inside, true);
      for (const s of [-1, 1]) box(F, -h, -.32, s * (w - .07) - .07, s * (w - .07) + .07, y + .82, y + 1.05, col);
      box(F, -h - .02, -h + .1, -w + .07, w - .07, y + .82, y + 1.05, col);
      const crates = 1 + Math.floor(r("crates") * 3);
      for (let i = 0; i < crates; i++) box(F, -h + .25 + i * .55, -h + .7 + i * .55, -w + .25 + (i % 2) * .5, -w + .75 + (i % 2) * .5, y + .82, y + 1.22, [C.wood, C.ochre, C.cream][i % 3]);
      lamps(F, h - .02, y + .62, [-w + .36, w - .36], .13, light); lamps(F, -h + .02, y + .6, [-w + .28, w - .28], .08, rear);
      box(F, h - .08, h + .07, -w + .08, w - .08, y + .3, y + .44, trim);
      wheels(F, y, L, W, .32, [h - .72, -h + .78]);
    } else {                                             // the delivery van: a tall load box behind an open cab
      soft(F, -h, h, -w, w, y + .32, y + 1.0, col, .14);
      soft(F, -h, .9, -w, w, y + .95, y + 2.35, col, .14);
      box(F, -h + .3, .7, -w - .012, w + .012, y + 1.5, y + 1.72, col === C.cream ? C.teal : C.cream);   // a painted band
      pillars(F, 2.05, 2.05, w - .14, y + .98, y + 1.72, col);
      soft(F, .85, 2.15, -w + .06, w - .06, y + 1.7, y + 1.86, col, .07);
      box(F, 1.75, 2.0, -w + .15, w - .15, y + .98, y + 1.12, inside);
      seats(F, md.seat[0], md.seat[1], y + .72, inside, true);
      lamps(F, h - .02, y + .7, [-w + .38, w - .38], .15, light); lamps(F, -h + .02, y + .66, [-w + .3, w - .3], .09, rear);
      box(F, h - .08, h + .07, -w + .08, w - .08, y + .32, y + .46, trim); box(F, -h - .07, -h + .08, -w + .08, w - .08, y + .32, y + .46, trim);
      wheels(F, y, L, W, .34, [h - .8, -h + .85]);
    }
  }

  function meshCar(m, c, k, y, md) {                    // one classic car from its exported mesh, in this car's colours
    if (!PAINT) { PAINT = [C.teal, C.terracotta, C.ochre, C.blue, C.olive, C.cream, C.plum, [.55, .40, .34], [.42, .47, .40]]; ROOF = [C.cream, [.74, .70, .62]]; }
    const r = t => hash(k, t), van = CARS[k].k === 1;
    const body = van ? [C.cream, C.teal, C.ochre, [.55, .40, .34]][k % 4] : PAINT[Math.floor(r("col") * PAINT.length)];
    const roof = r("two") < .75 ? ROOF[Math.floor(r("roof") * 2)] : body;
    const col = { body, roof, band: body === C.cream ? C.teal : C.cream, tyre: [.07, .065, .065], hub: [.58, .56, .52], leather: [.30, .19, .12],
      lamp: [.86, .80, .64], cream: C.cream, iron: [.10, .10, .11], wood: C.wood, luggage: C.terracotta, bag: C.cream, rail: [.10, .10, .11], spare: [.07, .065, .065], cargo: C.wood };
    const rack = r("rack") < .65, show = { luggage: rack, bag: rack, rail: rack, spare: r("spare") < .5 };   // a roof load comes as one
    const paints = CM.parts.map(name => show[name] === false ? null : col[name] || body);
    const dc = Math.hypot(c.x - cam.eye[0], c.z - cam.eye[2]), L = md.lods[dc < 45 ? 0 : 1];
    const P = L.p, N = L.n, T = L.t, need = T.length * 30;
    if (m.n + need > m.a.length) { const b = new Float32Array(Math.max(m.a.length * 2, m.n + need + 60)); b.set(m.a); m.a = b; }
    const A = m.a, hx = c.hx, hz = c.hz;
    let i = m.n;
    for (let t = 0; t < T.length; t++) {
      const pc = paints[T[t]]; if (!pc) continue;
      for (let v = 0; v < 3; v++) {
        const j = (t * 3 + v) * 3, a = P[j], w = P[j + 1], na = N[j], nw = N[j + 1];
        A[i] = c.x + hx * a - hz * w; A[i + 1] = y + P[j + 2]; A[i + 2] = c.z + hz * a + hx * w;
        A[i + 3] = hx * na - hz * nw; A[i + 4] = N[j + 2]; A[i + 5] = hz * na + hx * nw;
        A[i + 6] = pc[0]; A[i + 7] = pc[1]; A[i + 8] = pc[2]; A[i + 9] = 22;   // 22: car paint and its light (play.js)
        i += 10;
      }
    }
    m.n = i;
  }

  // ---- who sits in which car ----
  function drivers() {                                   // car -> [[from, to, resident]] from the residents' days
    byCar = new Map();
    D.plan.forEach((row, i) => row.forEach((p, j) => {
      if (p[1] !== 3) return;
      const t1 = j + 1 < row.length ? row[j + 1][0] : 1440;
      if (!byCar.has(p[2])) byCar.set(p[2], []);
      byCar.get(p[2]).push([p[0], t1, i]);
    }));
  }
  function stranger(key, role) {                         // someone from out of town, the same person every time (C7: one body)
    if (!people.has(key)) people.set(key, People.look(200000 + people.size * 7 + Math.floor(hash(key, "who") * 5), role, D.role_sig[role] || "Easy"));
    return people.get(key);
  }
  function occupants(k, c, minute) {                     // [[look, along, across]] of the people in car k now
    const md = model(k), out = [];
    if (drv && drv.k === k) return [[playerLook, md.seat[0], md.seat[1]]];
    if (taken.has(k)) return out;
    if (!byCar) drivers();
    const iv = (byCar.get(k) || []).find(v => minute >= v[0] && minute < v[1]);
    if (iv) { out.push([looks[iv[2]], md.seat[0], md.seat[1]]); return out; }
    if (CARS[k].home && CARS[k].k !== 1) return out;     // a resident's car with nobody in it
    if (!c.trip) return out;                             // a van parked at a stop: the driver is out delivering
    out.push([stranger("drv" + k, CARS[k].k === 1 ? "courier" : "commuter"), md.seat[0], md.seat[1]]);
    if (CARS[k].k !== 1 && hash(k, "pass") < .45) out.push([stranger("pas" + k, "elder"), md.seat[0], -md.seat[1]]);
    return out;
  }
  function seat(m, look, x, y, z, yaw, t) {              // one seated person; level of detail by distance to the camera
    const dc = Math.hypot(x - cam.eye[0], z - cam.eye[2]);
    if (dc > 70) return;
    People.draw(m, x, y, z, yaw, look, 2, { ph: 0, t, lod: dc < 13 ? 0 : dc < 40 ? 1 : 2, fl: 14 });
  }

  function draw(m, minute, near, reach) {                // every car within reach of the viewer, and whoever is in it
    let n = 0; const t = performance.now() / 1000;
    for (let k = 0; k < CARS.length; k++) {
      const c = at(k, minute);
      if (!c || Math.hypot(c.x - near[0], c.z - near[1]) > reach + 40) continue;
      const y = groundBase(c.x, c.z);
      body(m, c, k, y);
      const md = model(k), yaw = yawOf(c.hx, c.hz), sy = y + (md.seatY || (CARS[k].k === 1 ? .72 : .66));
      for (const [look, a, w] of occupants(k, c, minute)) seat(m, look, c.x + c.hx * a - c.hz * w, sy, c.z + c.hz * a + c.hx * w, yaw, t);
      n++;
    }
    return n;
  }

  // ---- the tram: open windows, and the commuters who are on it ----
  function tramRiders() {                                // residents' tram rides: [resident, run going out, run coming back, from, to]
    riders = [];
    D.plan.forEach((row, i) => row.forEach((p, j) => {
      if (p[1] !== 2 || j === 0) return;
      const before = row[j - 1], after = row[j + 1];
      const rideOut = before[1] === 0 && LOTS[before[7]] && LOTS[before[7]].kind === "stop";
      const rideIn = after && after[1] === 0 && LOTS[after[7]] && LOTS[after[7]].kind === "stop";
      const run = (m, x) => { let best = null; for (const tr of tramAt(m)) if (Math.abs(tr[0] - x) < 25 && (!best || Math.abs(tr[0] - x) < Math.abs(best[0] - x))) best = tr; return best ? best[2] : null; };
      const t1 = after ? after[0] : 1440;
      riders.push({ i, t0: p[0], t1, out: rideOut ? run(p[0], before[2]) : null, back: rideIn ? run(t1, after[2]) : null });
    }));
  }
  function tram(m, x, z, T, minute) {                    // the tram body (as before, but with open windows) and its riders
    const cream = C.cream;
    m.box(x - 6, .35, z - 1.2, x + 6, 1.5, z + 1.2, C.terracotta, 9);
    m.box(x - 6, 2.55, z - 1.2, x + 6, 2.75, z + 1.2, cream, 9);
    m.box(x - 6.1, 2.75, z - 1.25, x + 6.1, 2.95, z + 1.25, C.metal, 9);
    for (const e of [-1, 1]) m.box(x + e * 6 - (e > 0 ? .25 : 0), 1.5, z - 1.2, x + e * 6 + (e < 0 ? .25 : 0), 2.55, z + 1.2, cream, 9);   // the ends
    for (let k = 1; k <= 6; k++) {                       // posts between the six windows, both sides
      const a = x - 5.525 + k * 1.85;
      m.box(a - .225, 1.5, z - 1.2, a + .225, 2.55, z - 1.0, cream, 9);
      m.box(a - .225, 1.5, z + 1.0, a + .225, 2.55, z + 1.2, cream, 9);
    }
    for (const s of [-1, 1]) m.box(x - 5.75, 1.5, z + s * 1.1 - .1, x - 5.3, 2.55, z + s * 1.1 + .1, cream, 9);   // and before the first
    for (const s of [-1, 1]) m.box(x - 5.3 + .4, 1.1, z + s * .55 - .3, x + 5.4, 1.15, z + s * .55 + .3, [.24, .17, .12], 9);   // bench seats
    m.box(x + 6, 1.1, z - .25, x + 6.06, 1.4, z + .25, C.lantern, 2);
    if (!riders) tramRiders();
    const t = performance.now() / 1000; let slot = 0;
    for (const r of riders) {
      const going = r.out === T && minute >= r.t0 && minute < r.t0 + 40, coming = r.back === T && minute < r.t1 && minute > r.t1 - 40;
      if (!going && !coming) continue;
      if (slot >= 12) break;
      const row = slot % 6, side = slot < 6 ? -1 : 1; slot++;
      seat(m, looks[r.i], x - 4.7 + row * 1.85, 1.15, z + side * .55, yawOf(1, 0), t);
    }
  }

  // ---- collisions and the player driving ----
  function hits(c, k, x, z, pad) {                       // is the point (x, z) inside car k at c, grown by pad?
    const [L, W, ca] = dims(k), [a0, w] = local(c, x, z), a = a0 - ca;
    return Math.abs(a) < L / 2 + pad && Math.abs(w) < W / 2 + pad ? [a, w, L / 2 + pad - Math.abs(a), W / 2 + pad - Math.abs(w)] : null;
  }
  function push(p, minute) {                             // keep the player (on foot) out of cars, as the tram does
    if (drv) return;
    for (let k = 0; k < CARS.length; k++) {
      const c = at(k, minute);
      if (!c || Math.abs(c.x - p.x) > 4 || Math.abs(c.z - p.z) > 4 || p.y > groundBase(c.x, c.z) + 1.5) continue;
      const h = hits(c, k, p.x, p.z, .3); if (!h) continue;
      const [a, w, ea, ew] = h;
      if (ew < ea) { const s = Math.sign(w) || 1; p.x += -c.hz * s * ew; p.z += c.hx * s * ew; }
      else { const s = Math.sign(a) || 1; p.x += c.hx * s * ea; p.z += c.hz * s * ea; }
    }
  }
  function target(p, minute) {                           // the nearest parked car the player can get into, or null
    if (drv || p.pose > 1) return null;
    let best = null;
    for (let k = 0; k < CARS.length; k++) {
      const c = at(k, minute);
      if (!c || c.moving || Math.abs(c.x - p.x) > 5 || Math.abs(c.z - p.z) > 5 || Math.abs(p.y - groundBase(c.x, c.z)) > 1.2) continue;
      const h = hits(c, k, p.x, p.z, 1.5); if (!h) continue;   // within arm's reach of the door
      const d = Math.hypot(c.x - p.x, c.z - p.z);
      if (!best || d < best.d) best = { type: "car", label: CARS[k].k === 1 ? "Drive this van" : "Drive this car", k, d, x: c.x, z: c.z, y: groundBase(c.x, c.z) };
    }
    return best;
  }
  function enter(t, p, cam, minute) {
    const c = at(t.k, minute);
    drv = { k: t.k, x: c.x, z: c.z, a: Math.atan2(c.hz, c.hx), v: 0 };
    // out of the bay onto the road first, as residents' cars do: a sideways step down off the pavement, if there is room
    const g = groundBase(c.x, c.z);
    out: for (const d of [2.85, 2.2, 1.6]) for (const s of [-1, 1]) {
      const x = c.x - c.hz * s * d, z = c.z + c.hx * s * d;
      if (groundBase(x, z) < g - .05 && free(x, z, drv.a, minute)) { drv.x = x; drv.z = z; break out; }
    }
    saved = { dist: cam.dist, fp: cam.fp };
    cam.dist = Math.max(cam.dist, 8.5); cam.fp = false;
    p.pose = 4; p.x = drv.x; p.z = drv.z; p.y = groundBase(drv.x, drv.z); p.vy = 0;
  }
  function leave(p, cam) {
    if (!drv) return;
    const c = at(drv.k, 0), [L, W] = dims(drv.k);
    taken.set(drv.k, { x: c.x, z: c.z, hx: c.hx, hz: c.hz, moving: false });
    const y = groundBase(c.x, c.z) + .9;                 // out by the driver's door, else the other side, else the back
    const spots = [[0, -(W / 2 + .7)], [0, W / 2 + .7], [-(L / 2 + .8), 0], [L / 2 + .8, 0]].map(([a, w]) => [c.x + c.hx * a - c.hz * w, c.z + c.hz * a + c.hx * w]);
    const spot = spots.find(q => !blocked([q[0], y, q[1]])) || spots[0];
    p.x = spot[0]; p.z = spot[1]; p.y = groundBase(p.x, p.z); p.pose = 0; p.vy = 0; p.yaw = yawOf(c.hx, c.hz);
    if (saved) { cam.dist = saved.dist; cam.fp = saved.fp; }
    drv = null;
  }
  function free(x, z, a, minute, speed = 0, hit = null) {   // could the car stand here: no wall, no other car? (hit: what a fast car breaks)
    const [L, W, ca] = dims(drv.k), hx = Math.cos(a), hz = Math.sin(a), y = groundBase(x, z) + .7;
    const fast = hit && speed >= Smash.breakSpeed();
    for (const [al0, w] of [[L / 2, -W / 2], [L / 2, W / 2], [-L / 2, -W / 2], [-L / 2, W / 2], [L / 2, 0], [-L / 2, 0], [0, -W / 2], [0, W / 2]]) {
      const al = al0 + ca, px = x + hx * al - hz * w, pz = z + hz * al + hx * w;
      const b = hit ? Smash.probe(px, y - .5, pz) : null;   // a street thing or a tree (smash.js, CITY-072)
      if (b) { if (fast) { if (!hit.includes(b)) hit.push(b); continue; } return false; }
      if (blocked([px, y, pz])) { if (fast) hit.wall = [px, pz]; return false; }
      for (let k = 0; k < CARS.length; k++) {
        if (k === drv.k) continue;
        const c = at(k, minute);
        if (c && Math.abs(c.x - px) < 4 && Math.abs(c.z - pz) < 4 && hits(c, k, px, pz, .05)) return false;
      }
    }
    return true;
  }
  function drive(dt, keys, p, cam, minute) {             // one step of the player's car: W/S speed and brake, A/D steer, Space handbrake
    const up = keys.KeyW || keys.ArrowUp, down = keys.KeyS || keys.ArrowDown, steer = (keys.KeyD || keys.ArrowRight ? 1 : 0) - (keys.KeyA || keys.ArrowLeft ? 1 : 0);
    let v = drv.v;
    if (up) v += (v < 0 ? 8 : 3.2 + 2.4 * Math.min(1, TOP / 28)) * dt;   // a faster car pulls harder
    else if (down) v -= (v > 0 ? 7 : 2.5) * dt;
    else v -= Math.sign(v) * Math.min(Math.abs(v), 1.2 * dt);
    if (keys.Space) v -= Math.sign(v) * Math.min(Math.abs(v), 9 * dt);
    v = Math.max(-4, Math.min(TOP, v));
    const turn = steer * .5 * (1 - .5 * Math.min(1, Math.abs(v) / TOP)), a = drv.a + v / 2.6 * Math.tan(turn) * dt;
    const x = drv.x + Math.cos(a) * v * dt, z = drv.z + Math.sin(a) * v * dt;
    const hit = [];
    if (free(x, z, a, minute, Math.abs(v), hit)) {
      drv.x = x; drv.z = z; drv.a = a;
      for (const b of hit) v *= Smash.smash(b, Math.cos(a) * v, Math.sin(a) * v);   // broken, and the car loses speed (CITY-073)
      drv.v = v;
    } else {
      if (hit.wall) Smash.chip(hit.wall[0], hit.wall[1], Math.cos(a) * v, Math.sin(a) * v);   // a fast hit chips a building corner
      drv.v = -v * .2;                                    // a bump: stop, and roll back a little
    }
    p.x = drv.x; p.z = drv.z; p.y = groundBase(drv.x, drv.z); p.yaw = yawOf(Math.cos(drv.a), Math.sin(drv.a)); p.moving = false;
    if (Math.abs(drv.v) > .5) {                          // the camera swings round behind the car
      let d = yawOf(Math.cos(drv.a) * Math.sign(drv.v), Math.sin(drv.a) * Math.sign(drv.v)) - cam.yaw; d = Math.atan2(Math.sin(d), Math.cos(d));
      cam.yaw += d * Math.min(1, dt * 2.2);
    }
  }
  function near(minute, x, z, r) {                       // cars on a trip within r of (x, z), with their speed in m/s, for the sound (sound.js)
    const out = [], dm = .02;
    for (let k = 0; k < CARS.length; k++) {
      if (drv && drv.k === k) continue;
      const c = at(k, minute); if (!c || !c.trip || Math.abs(c.x - x) > r || Math.abs(c.z - z) > r) continue;
      const c2 = at(k, minute + dm), v = c2 ? Math.hypot(c2.x - c.x, c2.z - c.z) / (dm * D.pace) : 0;
      out.push({ x: c.x, z: c.z, v, van: CARS[k].k === 1 });
    }
    return out;
  }
  const label = () => "Get out · " + Math.round(Math.abs(drv ? drv.v : 0) * 3.6) + " km/h";

  return { at, draw, tram, push, target, enter, leave, drive, label, near, speed: () => drv ? drv.v : 0, driving: () => !!drv, count: CARS.length, model: k => model(k).name,
    debugFree: (x, z, a, m) => drv ? free(x, z, a, m) : null,
    debugPlace: (x, z, a) => { if (drv) Object.assign(drv, { x, z, a, v: 0 }); } };   // for tests: put the player's car somewhere
})();
