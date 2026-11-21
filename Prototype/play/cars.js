"use strict";
// Cars in the slice (LIVING_CITY 3.4: CITY-040, CITY-043 to CITY-049, CITY-068, CITY-069). Every vehicle's day (residents'
// cars, delivery vans, cars passing through) is worked out by Tools/citysim/traffic.py and packed into city_data.js as
// D.cars by Tools/worldgen/build_play.py; nothing here decides where those go, it only places and draws them. A resident
// who is in their car is not drawn as a person (piece kind 3 in D.plan); the car is.
//
// The player can get into any parked car or van (E), drive it, and leave it anywhere. A car the player has moved stays
// where it was left for the rest of the visit and no longer follows its owner's day. This is a stand-in for CITY-060 to
// CITY-063 (buying your own car comes with money; resident cars do not yet stop for the player, CITY-049).
//
// Loaded before play.js, like conversations.js. Nothing here touches play.js's globals (C, groundBase, blocked, yawOf,
// pl, cam...) until play.js calls in through Cars.draw, push, target, enter, leave and drive, by which time they exist.
// The look is a stand-in made of boxes in the world palette (ART_DIRECTION W4) until a car look test (CITY-067).
var Cars = (() => {
  const D = window.WASTED, CARS = D.cars || [], STEP = D.car_step || .1;
  const SIZE = [[4.2, 1.8], [5.0, 2.0]];                // length and width: car, van
  const taken = new Map();                               // car -> where the player left it
  let drv = null, saved = null, paint = null;

  function at(k, m) {                                    // {x, z, hx, hz, moving} of car k at minute m, or null when out of town
    if (drv && drv.k === k) return { x: drv.x, z: drv.z, hx: Math.cos(drv.a), hz: Math.sin(drv.a), moving: Math.abs(drv.v) > .2 };
    if (taken.has(k)) return taken.get(k);
    const c = CARS[k]; let bay = c.home;
    for (const t of c.trips) {
      if (m < t[0]) break;
      if (m < t[1]) {
        const p = t[3], n = p.length / 4;
        if (n < 2) return { x: p[0], z: p[1], hx: p[2], hz: p[3], moving: true };
        const f = Math.min(n - 1, Math.max(0, (m - t[0]) / STEP)), i = Math.min(n - 2, Math.floor(f)), g = f - i, a = i * 4, b = a + 4;
        let hx = p[a + 2] + (p[b + 2] - p[a + 2]) * g, hz = p[a + 3] + (p[b + 3] - p[a + 3]) * g; const l = Math.hypot(hx, hz) || 1;
        return { x: p[a] + (p[b] - p[a]) * g, z: p[a + 1] + (p[b + 1] - p[a + 1]) * g, hx: hx / l, hz: hz / l, moving: Math.hypot(p[b] - p[a], p[b + 1] - p[a + 1]) > .05 };
      }
      bay = t[2];
    }
    return bay ? { x: bay[0], z: bay[1], hx: bay[2], hz: bay[3], moving: false } : null;
  }
  const local = (c, x, z) => { const dx = x - c.x, dz = z - c.z; return [dx * c.hx + dz * c.hz, -dx * c.hz + dz * c.hx]; };   // along, across

  // a box turned to the car's heading: a0..a1 along it, w0..w1 across it, y0..y1 up
  function obox(m, c, a0, a1, w0, w1, y0, y1, col, f) {
    const P = (a, w, y) => [c.x + c.hx * a - c.hz * w, y, c.z + c.hz * a + c.hx * w];
    const q = (p0, p1, p2, p3, n) => m.quad(p0, p1, p2, p3, col, f, n);
    q(P(a1, w0, y0), P(a1, w1, y0), P(a1, w1, y1), P(a1, w0, y1), [c.hx, 0, c.hz]);
    q(P(a0, w0, y0), P(a0, w1, y0), P(a0, w1, y1), P(a0, w0, y1), [-c.hx, 0, -c.hz]);
    q(P(a0, w1, y0), P(a1, w1, y0), P(a1, w1, y1), P(a0, w1, y1), [-c.hz, 0, c.hx]);
    q(P(a0, w0, y0), P(a1, w0, y0), P(a1, w0, y1), P(a0, w0, y1), [c.hz, 0, -c.hx]);
    q(P(a0, w0, y1), P(a1, w0, y1), P(a1, w1, y1), P(a0, w1, y1), [0, 1, 0]);
  }
  function body(m, c, k, y) {
    if (!paint) paint = [C.teal, C.terracotta, C.cream, C.olive, C.plum, C.indigo, C.ochre, C.blue, C.brown];
    const van = CARS[k].k === 1, [L, W] = SIZE[van ? 1 : 0], h = L / 2, w = W / 2, dark = C.charcoal, glass = [.10, .12, .14];
    const col = van ? (k % 2 ? C.cream : C.teal) : paint[k % paint.length];
    if (van) {                                           // a cab in front of a tall load box
      obox(m, c, -h, h, -w, w, y + .35, y + .95, col, 9);
      obox(m, c, -h, h - 1.35, -w, w, y + .95, y + 2.3, col, 9);
      obox(m, c, h - 1.35, h - .25, -w + .08, w - .08, y + .95, y + 1.65, glass, 1);
      obox(m, c, h - 1.35, h - .3, -w + .1, w - .1, y + 1.65, y + 1.75, col, 9);
    } else {
      obox(m, c, -h, h, -w, w, y + .32, y + .86, col, 9);
      obox(m, c, -h + .12, h - .12, -w + .04, w - .04, y + .86, y + .96, col, 9);
      obox(m, c, -h + .95, h - 1.15, -w + .14, w - .14, y + .96, y + 1.5, glass, 1);
      obox(m, c, -h + 1.0, h - 1.2, -w + .2, w - .2, y + 1.5, y + 1.56, col, 9);
    }
    for (const a of [-h + .8, h - .8]) for (const s of [-1, 1]) obox(m, c, a - .33, a + .33, s > 0 ? w - .22 : -w - .02, s > 0 ? w + .02 : -w + .22, y, y + .62, dark, 9);
    for (const s of [-1, 1]) obox(m, c, h - .02, h + .03, s * (w - .38) - .14, s * (w - .38) + .14, y + .62, y + .76, C.cream, 9);
  }

  function draw(m, minute, near, reach) {                // every car within reach of the viewer, into the dynamic mesh
    let n = 0;
    for (let k = 0; k < CARS.length; k++) {
      const c = at(k, minute);
      if (!c || Math.hypot(c.x - near[0], c.z - near[1]) > reach + 40) continue;
      body(m, c, k, groundBase(c.x, c.z));
      n++;
    }
    return n;
  }

  function hits(c, k, x, z, pad) {                       // is the point (x, z) inside car k at c, grown by pad?
    const [L, W] = SIZE[CARS[k].k === 1 ? 1 : 0], [a, w] = local(c, x, z);
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

  // -- the player driving --
  function target(p, minute) {                           // the nearest parked car the player can get into, or null
    if (drv || p.pose > 1) return null;
    let best = null;
    for (let k = 0; k < CARS.length; k++) {
      const c = at(k, minute);
      if (!c || c.moving || Math.abs(c.x - p.x) > 5 || Math.abs(c.z - p.z) > 5 || Math.abs(p.y - groundBase(c.x, c.z)) > 1.2) continue;
      const h = hits(c, k, p.x, p.z, 1.1); if (!h) continue;
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
    const c = at(drv.k, 0), [L, W] = SIZE[CARS[drv.k].k === 1 ? 1 : 0];
    taken.set(drv.k, { x: c.x, z: c.z, hx: c.hx, hz: c.hz, moving: false });
    const y = groundBase(c.x, c.z) + .9;                 // out by the driver's door, else the other side, else the back
    const spots = [[0, -(W / 2 + .7)], [0, W / 2 + .7], [-(L / 2 + .8), 0], [L / 2 + .8, 0]].map(([a, w]) => [c.x + c.hx * a - c.hz * w, c.z + c.hz * a + c.hx * w]);
    const spot = spots.find(q => !blocked([q[0], y, q[1]])) || spots[0];
    p.x = spot[0]; p.z = spot[1]; p.y = groundBase(p.x, p.z); p.pose = 0; p.vy = 0; p.yaw = yawOf(c.hx, c.hz);
    if (saved) { cam.dist = saved.dist; cam.fp = saved.fp; }
    drv = null;
  }
  function free(x, z, a, minute) {                       // could the car stand here: no wall, no other car?
    const [L, W] = SIZE[CARS[drv.k].k === 1 ? 1 : 0], hx = Math.cos(a), hz = Math.sin(a), y = groundBase(x, z) + .7;
    for (const [al, w] of [[L / 2, -W / 2], [L / 2, W / 2], [-L / 2, -W / 2], [-L / 2, W / 2], [L / 2, 0], [-L / 2, 0], [0, -W / 2], [0, W / 2]]) {
      const px = x + hx * al - hz * w, pz = z + hz * al + hx * w;
      if (blocked([px, y, pz])) return false;
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
    if (up) v += (v < 0 ? 8 : 3.2) * dt;
    else if (down) v -= (v > 0 ? 7 : 2.5) * dt;
    else v -= Math.sign(v) * Math.min(Math.abs(v), 1.2 * dt);
    if (keys.Space) v -= Math.sign(v) * Math.min(Math.abs(v), 9 * dt);
    v = Math.max(-4, Math.min(14, v));
    const turn = steer * .5 * (1 - .5 * Math.min(1, Math.abs(v) / 14)), a = drv.a + v / 2.6 * Math.tan(turn) * dt;
    const x = drv.x + Math.cos(a) * v * dt, z = drv.z + Math.sin(a) * v * dt;
    if (free(x, z, a, minute)) { drv.x = x; drv.z = z; drv.a = a; drv.v = v; }
    else drv.v = -v * .2;                                 // a bump: stop, and roll back a little
    p.x = drv.x; p.z = drv.z; p.y = groundBase(drv.x, drv.z); p.yaw = yawOf(Math.cos(drv.a), Math.sin(drv.a)); p.moving = false;
    if (Math.abs(drv.v) > .5) {                          // the camera swings round behind the car
      let d = yawOf(Math.cos(drv.a) * Math.sign(drv.v), Math.sin(drv.a) * Math.sign(drv.v)) - cam.yaw; d = Math.atan2(Math.sin(d), Math.cos(d));
      cam.yaw += d * Math.min(1, dt * 2.2);
    }
  }
  const label = () => "Get out · " + Math.round(Math.abs(drv ? drv.v : 0) * 3.6) + " km/h";

  return { at, draw, push, target, enter, leave, drive, label, driving: () => !!drv, count: CARS.length,
    debugFree: (x, z, a, m) => drv ? free(x, z, a, m) : null };
})();
