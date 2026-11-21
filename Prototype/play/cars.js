"use strict";
// Residents' cars (LIVING_CITY 3.4: CITY-043 to CITY-049, CITY-068). Every car's day is worked out by
// Tools/citysim/traffic.py and packed into city_data.js as D.cars by Tools/worldgen/build_play.py; nothing here decides
// where a car goes, it only places and draws what the data says. A resident who is in their car is not drawn as a person
// (piece kind 3 in D.plan); the car is. Cars out of town (commuters past a town gate) are not on the map.
//
// Loaded before play.js, like conversations.js. Nothing here touches play.js's globals (C, groundBase, pl...) until
// play.js calls in through Cars.draw and Cars.push, by which time they all exist.
//
// The look is a stand-in made of boxes, in the world palette (ART_DIRECTION W4). The real car models come after a look
// test in Tools/blender/ (CITY-067).
var Cars = (() => {
  const D = window.WASTED, CARS = D.cars || [], STEP = D.car_step || .1;
  const LEN = 4.2, WID = 1.8;
  let paint = null;

  function at(c, m) {                                    // {x, z, hx, hz, moving} at minute m, or null when out of town
    let bay = c.home;
    for (const t of c.trips) {
      if (m < t[0]) break;
      if (m < t[1]) {
        const p = t[3], n = p.length / 4, f = Math.min(n - 1, Math.max(0, (m - t[0]) / STEP)), i = Math.min(n - 2, Math.floor(f)), g = f - i;
        if (n < 2) return { x: p[0], z: p[1], hx: p[2], hz: p[3], moving: true };
        const a = i * 4, b = a + 4;
        const moved = Math.hypot(p[b] - p[a], p[b + 1] - p[a + 1]) > .05;
        let hx = p[a + 2] + (p[b + 2] - p[a + 2]) * g, hz = p[a + 3] + (p[b + 3] - p[a + 3]) * g; const l = Math.hypot(hx, hz) || 1;
        return { x: p[a] + (p[b] - p[a]) * g, z: p[a + 1] + (p[b + 1] - p[a + 1]) * g, hx: hx / l, hz: hz / l, moving: moved };
      }
      bay = t[2];
    }
    return bay ? { x: bay[0], z: bay[1], hx: bay[2], hz: bay[3], moving: false } : null;
  }

  // a box turned to the car's heading: a0..a1 along it, w0..w1 across it, y0..y1 up
  function obox(m, c, a0, a1, w0, w1, y0, y1, col, f) {
    const P = (a, w, y) => [c.x + c.hx * a - c.hz * w, y, c.z + c.hz * a + c.hx * w];
    const q = (p0, p1, p2, p3, n) => m.quad(p0, p1, p2, p3, col, f, n);
    q(P(a1, w0, y0), P(a1, w1, y0), P(a1, w1, y1), P(a1, w0, y1), [c.hx, 0, c.hz]);          // front
    q(P(a0, w0, y0), P(a0, w1, y0), P(a0, w1, y1), P(a0, w0, y1), [-c.hx, 0, -c.hz]);         // back
    q(P(a0, w1, y0), P(a1, w1, y0), P(a1, w1, y1), P(a0, w1, y1), [-c.hz, 0, c.hx]);          // right
    q(P(a0, w0, y0), P(a1, w0, y0), P(a1, w0, y1), P(a0, w0, y1), [c.hz, 0, -c.hx]);          // left
    q(P(a0, w0, y1), P(a1, w0, y1), P(a1, w1, y1), P(a0, w1, y1), [0, 1, 0]);                 // top
  }

  function body(m, c, k, y) {                            // a small old-town car: rounded-looking body, cabin, glass, wheels
    if (!paint) paint = [C.teal, C.terracotta, C.cream, C.olive, C.plum, C.indigo, C.ochre, C.blue, C.brown];
    const col = paint[k % paint.length], dark = C.charcoal, glass = [.10, .12, .14], h = LEN / 2, w = WID / 2;
    obox(m, c, -h, h, -w, w, y + .32, y + .86, col, 9);                       // body
    obox(m, c, -h + .12, h - .12, -w + .04, w - .04, y + .86, y + .96, col, 9);   // shoulder, so the top reads soft
    obox(m, c, -h + .95, h - 1.15, -w + .14, w - .14, y + .96, y + 1.5, glass, 1);   // cabin glass
    obox(m, c, -h + 1.0, h - 1.2, -w + .2, w - .2, y + 1.5, y + 1.56, col, 9);       // roof
    for (const a of [-h + .75, h - .75]) for (const s of [-1, 1])            // wheels
      obox(m, c, a - .33, a + .33, s > 0 ? w - .22 : -w - .02, s > 0 ? w + .02 : -w + .22, y, y + .62, dark, 9);
    for (const s of [-1, 1]) obox(m, c, h - .02, h + .03, s * (w - .38) - .14, s * (w - .38) + .14, y + .62, y + .76, C.cream, 9);   // lamps
  }

  function draw(m, minute, near, reach) {                // every car within reach of the viewer, into the dynamic mesh
    let n = 0;
    for (let k = 0; k < CARS.length; k++) {
      const c = at(CARS[k], minute);
      if (!c || Math.hypot(c.x - near[0], c.z - near[1]) > reach + 40) continue;
      body(m, c, k, groundBase(c.x, c.z));
      n++;
    }
    return n;
  }

  function push(p, minute) {                             // keep the player out of cars, as the tram does
    for (const car of CARS) {
      const c = at(car, minute);
      if (!c || Math.abs(c.x - p.x) > 4 || Math.abs(c.z - p.z) > 4 || p.y > groundBase(c.x, c.z) + 1.5) continue;
      const dx = p.x - c.x, dz = p.z - c.z, a = dx * c.hx + dz * c.hz, w = -dx * c.hz + dz * c.hx;
      const ea = LEN / 2 + .3 - Math.abs(a), ew = WID / 2 + .3 - Math.abs(w);
      if (ea <= 0 || ew <= 0) continue;
      if (ew < ea) { const s = Math.sign(w) || 1; p.x += -c.hz * s * ew; p.z += c.hx * s * ew; }
      else { const s = Math.sign(a) || 1; p.x += c.hx * s * ea; p.z += c.hz * s * ea; }
    }
  }

  return { at, draw, push, count: CARS.length };
})();
