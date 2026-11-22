/* The independent houses of the communities (Tools/worldgen/add_houses_01.py; the owner's requests of 2026-11-22): a designed
   front and roof in one of many variants, and inside, rooms on two floors round a stair that rises up the middle of the hall to a
   half landing and branches to both sides.
   Everything is laid out in the house's own frame (u across the front, v from the front at -5 to the back at +5, metres from its
   centre) and turned by lot.angle. Houses.outside(l) runs at load: the exterior, the walls and floors you cannot walk through, and
   the places residents and the player use. Houses.inside(l, m) runs near the camera: the rooms' walls, floors, ceilings, the stair
   and the furniture, into the room mesh m. Runs inside play.js's world; load it before play.js. */
"use strict";
window.Houses = (() => {
  const F0 = .2, F1 = 3.4, C0 = 3.2, C1 = 6.2, H = 6.4, T = .3, PART = .12, DOOR_H = 2.15;
  const SH = { teal: [.2, .36, .37], blue: [.3, .38, .52], olive: [.3, .33, .2], terracotta: [.44, .24, .18], ochre: [.52, .4, .2], plum: [.3, .24, .34] };
  const BRICK = [.5, .3, .22], GLASS = [.1, .12, .14], STONE_L = [.62, .58, .52];

  function frame(l) {                                         // the house's own frame: T(u, y, v) -> world, and its matrix for boxes
    const [x0, z0, x1, z1] = l.rect, cx = (x0 + x1) / 2, cz = (z0 + z1) / 2, t = (l.angle || 0) * Math.PI / 180, c = Math.cos(t), s = Math.sin(t);
    const M = M4.mul(M4.t(cx, 0, cz), M4.ry(-t));
    return { cx, cz, c, s, M, P: (u, y, v) => [cx + u * c - v * s, y, cz + u * s + v * c], N: (nu, nv) => [nu * c - nv * s, 0, nu * s + nv * c] };
  }
  const bx = (m, Fr, u0, y0, v0, u1, y1, v1, col, f) => m.obox(Fr.M, (u0 + u1) / 2, (y0 + y1) / 2, (v0 + v1) / 2, Math.abs(u1 - u0), Math.abs(y1 - y0), Math.abs(v1 - v0), col, f);
  const yb = (m, Fr, u, y0, v, su, sy, sv, yaw, col, f) => m.obox(M4.mul(Fr.M, M4.mul(M4.t(u, y0, v), M4.ry(yaw || 0))), 0, sy / 2, 0, su, sy, sv, col, f);
  const qd = (m, Fr, a, b, c, d, col, f, nu, nv) => m.quad(Fr.P(...a), Fr.P(...b), Fr.P(...c), Fr.P(...d), col, f, nu === undefined ? [0, 1, 0] : nu === "down" ? [0, -1, 0] : Fr.N(nu, nv));
  function solidAt(Fr, u, v, y0, y1, r) { const p = Fr.P(u, 0, v); solid(p[0] - r, y0, p[2] - r, p[0] + r, y1, p[2] + r); }
  function chain(Fr, u0, v0, u1, v1, y0, y1, gaps, r) {        // a wall you cannot walk through: small blocks along it, but for its doorways
    r = r || .3;                                             // thin walls (r .1) where the way between them is narrow: the side flights
    const L = Math.hypot(u1 - u0, v1 - v0), n = Math.max(1, Math.round(L / (r * 1.8)));
    for (let k = 0; k <= n; k++) {
      const u = u0 + (u1 - u0) * k / n, v = v0 + (v1 - v0) * k / n;
      if ((gaps || []).some(g => Math.hypot(u - g[0], v - g[1]) < .5 + r)) continue;
      solidAt(Fr, u, v, y0, y1, r);
    }
  }
  function grid(Fr, u0, v0, u1, v1, y0, y1, cell) {            // a floor or a stair step: blocks over a rectangle (a turned house's blocks
    cell = cell || 1.1;                                      // stand square to the world, so they are kept small where they could reach into a stair)
    const nu = Math.max(1, Math.ceil((u1 - u0) / cell)), nv = Math.max(1, Math.ceil((v1 - v0) / cell));
    for (let i = 0; i < nu; i++) for (let j = 0; j < nv; j++) {
      const a = u0 + (u1 - u0) * i / nu, b = u0 + (u1 - u0) * (i + 1) / nu, c = v0 + (v1 - v0) * j / nv, d = v0 + (v1 - v0) * (j + 1) / nv;
      const p = [Fr.P(a, 0, c), Fr.P(b, 0, c), Fr.P(b, 0, d), Fr.P(a, 0, d)];
      solid(Math.min(...p.map(q => q[0])) + .02, y0, Math.min(...p.map(q => q[2])) + .02, Math.max(...p.map(q => q[0])) - .02, y1, Math.max(...p.map(q => q[2])) - .02);
    }
  }
  const sg = l => l.house.mirror ? -1 : 1;
  // ---- the stair, in steps: [u0, v0, u1, v1, top] (the central flight, the half landing, the two side flights and their landings)
  function stairSteps(l) {
    if (l._steps) return l._steps;
    const out = [], n = 9, half = (F0 + F1) / 2;
    for (let k = 0; k < n; k++) { const v0 = -2.3 + 3.5 * k / n, v1 = -2.3 + 3.5 * (k + 1) / n; out.push([-.75, v0, .75, v1, F0 + (half - F0) * (k + 1) / n]); }
    out.push([-1.5, 1.2, 1.5, 2.6, half]);
    for (const s of [-1, 1]) {
      for (let k = 0; k < n; k++) { const a = 1.5 + 3.5 * k / n, b = 1.5 + 3.5 * (k + 1) / n; out.push([s > 0 ? a : -b, 1.25, s > 0 ? b : -a, 2.25, half + (F1 - half) * (k + 1) / n]); }
      out.push([s > 0 ? 5 : -6.5, 1.2, s > 0 ? 6.5 : -5, 2.3, F1]);
    }
    return (l._steps = out);
  }
  // the inner walls of one floor, from the rooms: [u0, v0, u1, v1, doors on it]
  function innerWalls(l, f) {
    const key = "_w" + f; if (l[key]) return l[key];
    const rooms = l.house.rooms.filter(r => r.floor === f).map(r => r.name === "Hall" ? [...r.rect, "hall"] : r.rect);   // the hall opens on to its stair at the back
    const doors = l.house.doors.filter(d => d.floor === f).map(d => d.at), seen = new Set(), out = [];
    for (const [u0, v0, u1, v1, tag] of rooms) for (const e of (tag === "hall" ? [[u0, v0, u1, v0], [u0, v0, u0, v1], [u1, v0, u1, v1]] : [[u0, v0, u1, v0], [u0, v1, u1, v1], [u0, v0, u0, v1], [u1, v0, u1, v1]])) {
      const outer = (e[0] === e[2] && Math.abs(e[0]) > 6.4) || (e[1] === e[3] && Math.abs(e[1]) > 4.9); if (outer) continue;
      const k = e.map(x => x.toFixed(2)).join(","); if (seen.has(k)) continue; seen.add(k);
      out.push([...e, doors.filter(d => (e[0] === e[2] ? Math.abs(d[0] - e[0]) < .05 && d[1] > Math.min(e[1], e[3]) - .01 && d[1] < Math.max(e[1], e[3]) + .01 : Math.abs(d[1] - e[1]) < .05 && d[0] > Math.min(e[0], e[2]) - .01 && d[0] < Math.max(e[0], e[2]) + .01))]);
    }
    return (l[key] = out);
  }
  const flip = (l, u) => u * sg(l);

  // ---------------------------------------------------------------- outside, at load
  function outside(l) {
    if (l.house.plan === "house_01") return outside01(l);
    const Fr = frame(l), Hs = l.house, tr = Hs.trim, s = sg(l), m = world;
    const plaster = PLASTER[tr.plaster % PLASTER.length], roofC = ROOF[tr.roof_tone % ROOF.length], shut = SH[tr.shutter_color] || C.teal;
    const bp = SURF.patterns.indexOf("brick_running"), sp = SURF.patterns.indexOf("ashlar");
    const [wallC, wallF] = Hs.walls === "brick" ? [BRICK, bp >= 0 ? 30 + bp : 6] : Hs.walls === "stone" ? [C.stone, sp >= 0 ? 30 + sp : 6] : [plaster, 0];
    l.H = H;
    const back = Hs.doors[1].at[0];                              // the back door's u (mirrored with the plan)
    // walls, as four slabs with the two doorways cut
    const wall = (u0, v0, u1, v1, y0, y1) => bx(m, Fr, u0, y0, v0, u1, y1, v1, wallC, wallF);
    wall(-6.5, -5, -.6, -5 + T, 0, H); wall(.6, -5, 6.5, -5 + T, 0, H); wall(-.6, -5, .6, -5 + T, 2.6, H);
    wall(-6.5, 5 - T, back - .6, 5, 0, H); wall(back + .6, 5 - T, 6.5, 5, 0, H); wall(back - .6, 5 - T, back + .6, 5, 2.4, H);
    wall(-6.5, -5 + T, -6.5 + T, 5 - T, 0, H); wall(6.5 - T, -5 + T, 6.5, 5 - T, 0, H);
    chain(Fr, -6.5, -5, 6.5, -5, 0, H, [[0, -5]]); chain(Fr, -6.5, 5, 6.5, 5, 0, H, [[back, 5]]); chain(Fr, -6.5, -5, -6.5, 5, 0, H); chain(Fr, 6.5, -5, 6.5, 5, 0, H);
    // plinth, string course, quoins
    for (const [u0, v0, u1, v1] of [[-6.62, -5.12, 6.62, -5], [-6.62, 5, 6.62, 5.12], [-6.62, -5, -6.5, 5], [6.5, -5, 6.62, 5]]) {
      bx(m, Fr, u0, 0, v0, u1, .55, v1, C.stoneDark, 6); bx(m, Fr, u0, 3.28, v0, u1, 3.42, v1, STONE_L, 6);
    }
    if (Hs.walls === "plaster") for (const [u, v] of [[-6.5, -5], [6.5, -5], [-6.5, 5], [6.5, 5]]) bx(m, Fr, u - .16 * Math.sign(u) * 0, .55, v, u + .001, H, v + .001, STONE_L, 6), bx(m, Fr, u - .22, .55, v - .22, u + .22, H, v + .22, STONE_L, 6);
    // windows: dark glass outside and in, a stone sill, a keystone or plain head, shutters, window boxes
    const win = (u, v, y0, h, w, face, opts) => {
      const o = face * .02, vv = v + face * (T / 2 + .01), out = [u - w / 2, y0, vv + o], A = (uu, yy) => [uu, yy, vv + o];
      if (Math.abs(v) < 6) {
        qd(m, Fr, A(u - w / 2, y0), A(u + w / 2, y0), A(u + w / 2, y0 + h), A(u - w / 2, y0 + h), GLASS, 4, 0, face);
        const iv = v - face * (T / 2 + .01); qd(m, Fr, [u - w / 2, y0, iv], [u + w / 2, y0, iv], [u + w / 2, y0 + h, iv], [u - w / 2, y0 + h, iv], GLASS, 4, 0, -face);
        bx(m, Fr, u - w / 2 - .12, y0 - .12, vv - .02, u + w / 2 + .12, y0, vv + face * .16, STONE_L, 6);
        if (tr.keystones) bx(m, Fr, u - .16, y0 + h, vv - .02, u + .16, y0 + h + .32, vv + face * .08, STONE_L, 6); else bx(m, Fr, u - w / 2 - .1, y0 + h, vv - .02, u + w / 2 + .1, y0 + h + .14, vv + face * .06, STONE_L, 6);
        if (tr.shutters && !(opts && opts.noShut)) for (const sd of [-1, 1]) bx(m, Fr, u + sd * (w / 2 + .02), y0, vv, u + sd * (w / 2 + w / 2 + .02), y0 + h, vv + face * .05, shut, 12);
        if (tr.window_boxes && opts && opts.box) { bx(m, Fr, u - w / 2, y0 - .32, vv, u + w / 2, y0 - .02, vv + face * .3, C.brown, 9); for (let k = 0; k < 5; k++) yb(m, Fr, u - w / 2 + .15 + k * (w - .3) / 4, y0 - .02, vv + face * .15, .2, .18, .2, 0, pick([C.terracotta, C.ochre, C.olive, C.plum, C.cream], hash(l.id, "fl", u, k)), 9); }
      }
      void out;
    };
    const winSide = (u, v, y0, h, w, face) => {                    // a window in a side wall (constant u)
      const uu = u + face * (T / 2 + .03), iu = u - face * (T / 2 + .03);
      qd(m, Fr, [uu, y0, v - w / 2], [uu, y0, v + w / 2], [uu, y0 + h, v + w / 2], [uu, y0 + h, v - w / 2], GLASS, 4, face, 0);
      qd(m, Fr, [iu, y0, v - w / 2], [iu, y0, v + w / 2], [iu, y0 + h, v + w / 2], [iu, y0 + h, v - w / 2], GLASS, 4, -face, 0);
      bx(m, Fr, u, y0 - .12, v - w / 2 - .12, u + face * .16, y0, v + w / 2 + .12, STONE_L, 6);
    };
    const bay = (Hs.extras || []).find(e => e.kind === "bay");
    for (const u of [-4, 4]) {
      const living = u * s < 0;
      if (!(bay && living)) win(u, -5, 1.0, 1.8, 1.4, -1, {});
      win(u, -5, 4.1, 1.6, 1.3, -1, { box: true });
      win(u, 5, 1.0, 1.6, 1.3, 1, {}); win(u, 5, 4.1, 1.6, 1.3, 1, {});
    }
    for (const u of [-6.5, 6.5]) for (const [v, y0] of [[-2, 1.0], [-2, 4.1], [3.65, 1.1], [3.65, 4.1]]) winSide(u, v, y0, 1.5, 1.1, Math.sign(u));
    // the tall arched window over the door, lighting the hall's stair
    { const vv = -5 - T / 2 - .03, iv = -5 + T + .02;
      qd(m, Fr, [-.75, 3.8, vv], [.75, 3.8, vv], [.75, 5.4, vv], [-.75, 5.4, vv], GLASS, 4, 0, -1);
      qd(m, Fr, [-.75, 3.8, iv], [.75, 3.8, iv], [.75, 5.4, iv], [-.75, 5.4, iv], GLASS, 4, 0, 1);
      for (let k = 0; k < 6; k++) { const a0 = Math.PI * k / 6, a1 = Math.PI * (k + 1) / 6; qd(m, Fr, [0, 5.4, vv], [0, 5.4, vv], [.75 * Math.cos(a1), 5.4 + .75 * Math.sin(a1), vv], [.75 * Math.cos(a0), 5.4 + .75 * Math.sin(a0), vv], GLASS, 4, 0, -1); }
      bx(m, Fr, -.95, 3.66, -5.2, .95, 3.8, -5, STONE_L, 6); }
    // the front door, open into the hall, and the back door
    yb(m, Fr, -.55 + .5, 0, -5 + T + .5, .08, 2.55, 1.0, Math.PI / 2 - 1.2, C.brown, 9);
    yb(m, Fr, back - .55, 0, 5 - T - .45, 1.0, 2.35, .08, 1.2, C.brown, 9);
    bx(m, Fr, -.7, 0, -5.45, .7, .2, -5, STONE_L, 6);                                           // the step
    // the porch
    if (Hs.porch === "columns") {
      for (const u of [-1.25, 1.25]) { yb(m, Fr, u, .2, -6.1, .32, 2.75, .32, 0, C.cream, 6); }
      bx(m, Fr, -1.6, 2.95, -6.45, 1.6, 3.2, -5, STONE_L, 6);
      qd(m, Fr, [-1.7, 3.2, -6.46], [1.7, 3.2, -6.46], [0, 3.85, -6.46], [0, 3.85, -6.46], STONE_L, 6, 0, -1);
      qd(m, Fr, [-1.7, 3.2, -6.46], [0, 3.85, -6.46], [0, 3.85, -5], [-1.7, 3.2, -5], roofC, 11);
      qd(m, Fr, [0, 3.85, -6.46], [1.7, 3.2, -6.46], [1.7, 3.2, -5], [0, 3.85, -5], roofC, 11);
      bx(m, Fr, -1.7, 0, -6.5, 1.7, .2, -5, STONE_L, 6);
      for (const u of [-1.25, 1.25]) solidAt(Fr, u, -6.1, 0, 3, .2);
    } else if (Hs.porch === "canopy") {
      bx(m, Fr, -1.5, 2.85, -6.1, 1.5, 3.0, -5, C.stoneDark, 9);
      for (const u of [-1.3, 1.3]) bx(m, Fr, u - .06, 2.35, -5.6, u + .06, 2.85, -5, C.charcoal, 9);
    } else {                                                                                      // an arched stone doorway
      for (const u of [-1.0, 1.0]) bx(m, Fr, u - .2, 0, -5.25, u + .2, 2.7, -5, STONE_L, 6);
      for (let k = 0; k < 7; k++) { const a = Math.PI * (k + .5) / 7; yb(m, Fr, Math.cos(a) * .95, 2.55 + Math.sin(a) * .7, -5.12, .32, .3, .25, 0, STONE_L, 6); }
    }
    // extras
    for (const ex of Hs.extras || []) {
      const [u0, v0, u1, v1] = ex.rect;
      if (ex.kind === "bay") {                                     // a bay window on the living room
        bx(m, Fr, u0, 0, v0, u1, .55, v1, C.stoneDark, 6); bx(m, Fr, u0, 2.9, v0, u1, 3.15, v1, C.stoneDark, 9);
        qd(m, Fr, [u0, 3.15, v0], [u1, 3.15, v0], [u1, 3.15, v1], [u0, 3.15, v1], roofC, 11);
        qd(m, Fr, [u0 + .1, .6, v0 - .01], [u1 - .1, .6, v0 - .01], [u1 - .1, 2.8, v0 - .01], [u0 + .1, 2.8, v0 - .01], GLASS, 4, 0, -1);
        for (const u of [u0, u1]) bx(m, Fr, u - .05, .55, v0, u + .05, 2.9, v1, wallC, wallF);
        chain(Fr, u0, v0, u1, v0, 0, 3); chain(Fr, u0, v0, u0, v1, 0, 3); chain(Fr, u1, v0, u1, v1, 0, 3);
      } else if (ex.kind === "balcony") {                        // over the porch, at the first floor
        bx(m, Fr, -1.5, 3.25, -6.2, 1.5, 3.4, -5, STONE_L, 6);
        bx(m, Fr, -1.5, 3.4, -6.2, 1.5, 3.48, -6.12, C.charcoal, 9); bx(m, Fr, -1.5, 4.4, -6.2, 1.5, 4.46, -6.12, C.charcoal, 9);
        for (let k = 0; k <= 10; k++) bx(m, Fr, -1.5 + k * .3 - .02, 3.4, -6.18, -1.5 + k * .3 + .02, 4.4, -6.14, C.charcoal, 9);
      } else if (ex.kind === "garage") {                         // a garage at the side, its door to the front
        bx(m, Fr, u0, 0, v0, u1, 2.9, v1, wallC, wallF); bx(m, Fr, u0 - .15, 2.9, v0 - .15, u1 + .15, 3.1, v1 + .15, C.stoneDark, 9);
        qd(m, Fr, [u0 + .3, .05, v0 - .02], [u1 - .3, .05, v0 - .02], [u1 - .3, 2.4, v0 - .02], [u0 + .3, 2.4, v0 - .02], pick([C.teal, C.olive, C.blue, C.cream], hash(l.id, "gd")), 9, 0, -1);
        chain(Fr, u0, v0, u1, v0, 0, 2.9); chain(Fr, u0, v1, u1, v1, 0, 2.9); chain(Fr, u0, v0, u0, v1, 0, 2.9); chain(Fr, u1, v0, u1, v1, 0, 2.9);
      } else if (ex.kind === "conservatory") {                   // glass room at the back
        bx(m, Fr, u0, 0, v0, u1, .5, v1, C.stoneDark, 6);
        for (const [a, b] of [[[u0, v1], [u1, v1]], [[u0, v0], [u0, v1]], [[u1, v0], [u1, v1]]]) {
          const n = a[0] === b[0] ? [Math.sign(a[0] - (u0 + u1) / 2), 0] : [0, 1];
          qd(m, Fr, [a[0], .5, a[1]], [b[0], .5, b[1]], [b[0], 2.7, b[1]], [a[0], 2.7, a[1]], [.55, .64, .7], 4, n[0], n[1]);
        }
        for (const [u, v] of [[u0, v1], [u1, v1], [u0, v0], [u1, v0]]) bx(m, Fr, u - .06, .5, v - .06, u + .06, 2.75, v + .06, C.cream, 9);
        qd(m, Fr, [u0, 2.75, v0], [u1, 2.75, v0], [u1, 2.35, v1], [u0, 2.35, v1], [.6, .68, .72], 4);
        chain(Fr, u0, v1, u1, v1, 0, 2.7); chain(Fr, u0, v0, u0, v1, 0, 2.7); chain(Fr, u1, v0, u1, v1, 0, 2.7);
      }
    }
    // the roof: hipped, gabled or mansard with dormers; and a chimney at each end
    const o = .5, R = 3.0, E = H;
    if (Hs.roof === "hip") {
      qd(m, Fr, [-6.5 - o, E, -5 - o], [6.5 + o, E, -5 - o], [3.2, E + R, 0], [-3.2, E + R, 0], roofC, 11);
      qd(m, Fr, [-3.2, E + R, 0], [3.2, E + R, 0], [6.5 + o, E, 5 + o], [-6.5 - o, E, 5 + o], roofC, 11);
      qd(m, Fr, [-6.5 - o, E, 5 + o], [-6.5 - o, E, -5 - o], [-3.2, E + R, 0], [-3.2, E + R, 0], roofC, 11);
      qd(m, Fr, [6.5 + o, E, -5 - o], [6.5 + o, E, 5 + o], [3.2, E + R, 0], [3.2, E + R, 0], roofC, 11);
    } else if (Hs.roof === "gable") {
      qd(m, Fr, [-6.5 - o, E, -5 - o], [6.5 + o, E, -5 - o], [6.5 + o, E + R, 0], [-6.5 - o, E + R, 0], roofC, 11);
      qd(m, Fr, [-6.5 - o, E + R, 0], [6.5 + o, E + R, 0], [6.5 + o, E, 5 + o], [-6.5 - o, E, 5 + o], roofC, 11);
      for (const u of [-6.5, 6.5]) qd(m, Fr, [u, E, -5], [u, E, 5], [u, E + R * 5 / (5 + o), 0], [u, E + R * 5 / (5 + o), 0], wallC, wallF, Math.sign(u), 0);
    } else {                                                                                    // mansard: a steep lower slope, a low top
      const i = 1.3, Ru = 2.4, Rt = .9;
      qd(m, Fr, [-6.5 - o, E, -5 - o], [6.5 + o, E, -5 - o], [6.5 - i, E + Ru, -5 + i], [-6.5 + i, E + Ru, -5 + i], roofC, 11);
      qd(m, Fr, [6.5 + o, E, 5 + o], [-6.5 - o, E, 5 + o], [-6.5 + i, E + Ru, 5 - i], [6.5 - i, E + Ru, 5 - i], roofC, 11);
      qd(m, Fr, [-6.5 - o, E, 5 + o], [-6.5 - o, E, -5 - o], [-6.5 + i, E + Ru, -5 + i], [-6.5 + i, E + Ru, 5 - i], roofC, 11);
      qd(m, Fr, [6.5 + o, E, -5 - o], [6.5 + o, E, 5 + o], [6.5 - i, E + Ru, 5 - i], [6.5 - i, E + Ru, -5 + i], roofC, 11);
      qd(m, Fr, [-6.5 + i, E + Ru, -5 + i], [6.5 - i, E + Ru, -5 + i], [0, E + Ru + Rt, 0], [0, E + Ru + Rt, 0], mul3(roofC, .85), 11);
      qd(m, Fr, [6.5 - i, E + Ru, 5 - i], [-6.5 + i, E + Ru, 5 - i], [0, E + Ru + Rt, 0], [0, E + Ru + Rt, 0], mul3(roofC, .85), 11);
      for (const u of [-3.4, 3.4]) {                                                          // dormers on the front
        bx(m, Fr, u - .8, E + .2, -5.2, u + .8, E + 1.9, -4.2, wallC, wallF);
        qd(m, Fr, [u - .55, E + .5, -5.22], [u + .55, E + .5, -5.22], [u + .55, E + 1.6, -5.22], [u - .55, E + 1.6, -5.22], GLASS, 4, 0, -1);
        qd(m, Fr, [u - 1.0, E + 1.9, -5.35], [u + 1.0, E + 1.9, -5.35], [u + 1.0, E + 1.9, -4.1], [u - 1.0, E + 1.9, -4.1], roofC, 11);
      }
    }
    for (const u of [-5.6, 5.6]) { bx(m, Fr, u - .45, E - 1, -.4, u + .45, E + R + 1.1, .4, Hs.walls === "plaster" ? STONE_L : wallC, Hs.walls === "plaster" ? 6 : wallF); bx(m, Fr, u - .55, E + R + 1.1, -.5, u + .55, E + R + 1.25, .5, C.stoneDark, 9); }
    // what stands inside and cannot be walked through: inner walls, the first floor, the stair, the rails at the half landing
    for (const f of [0, 1]) for (const [u0, v0, u1, v1, ds] of innerWalls(l, f)) chain(Fr, u0, v0, u1, v1, f ? F1 : 0, f ? C1 : C0 - 1.05, ds, f ? .1 : .3);
    for (const r of Hs.rooms.filter(r => r.floor === 1)) {     // the first floor: kept 0.3 m back from the side flights' edges
      const [u0, v0, u1, v1] = r.rect; grid(Fr, u0, Math.abs(v0 - 2.3) < .01 ? v0 + .3 : v0, u1, Math.abs(v1 - 1.2) < .01 ? v1 - .3 : v1, C0, F1, .7);
    }
    for (const [u0, v0, u1, v1, top] of stairSteps(l)) grid(Fr, u0, v0, u1, v1, top - .2, top);
    for (const st of stairSteps(l)) { const [u0, v0, u1, v1, top] = st; if (top < F1 - .01 || Math.abs(u1 - u0) < 1.2) grid(Fr, u0, v0, u1, v1, 0, top - .19); }   // solid under the steps; the side landings stand over the passages
    chain(Fr, -1.5, 1.2, -.85, 1.2, 1.8, 2.8, null, .08); chain(Fr, .85, 1.2, 1.5, 1.2, 1.8, 2.8, null, .08);   // a rail where the half landing meets the hall
    // the places residents and the player use: a bed each upstairs, a seat each in the living room (as the block file has them)
    places(l, Fr);
    l.furn = ["house"];
    occl.push([Math.min(l.rect[0], l.rect[2]) - 2, Math.min(l.rect[1], l.rect[3]) - 2, Math.max(l.rect[0], l.rect[2]) + 2, Math.max(l.rect[1], l.rect[3]) + 2, H]);
  }

  function places(l, Fr) {
    const o = l.objects || {}, t = (l.angle || 0) * Math.PI / 180;
    (o.bed || []).forEach(([x, z]) => {                                       // lying along u, the head to the side wall
      const [cx, cz] = [x - Fr.cx, z - Fr.cz], u = cx * Fr.c + cz * Fr.s, head = Math.sign(u) || 1;
      const dir = Fr.N(head, 0), yaw = yawOf(-dir[0], -dir[2]);
      spotAt(l, x, z, 3, yaw, F1 + .62); useAt(x, F1 + .62, z, "lie", "Lie down", yaw);
    });
    (o.room || []).forEach(([x, z]) => {
      const [cx, cz] = [x - Fr.cx, z - Fr.cz], u = cx * Fr.c + cz * Fr.s, v = -cx * Fr.s + cz * Fr.c;
      const lc = [-4 * sg(l), -1.9], d = Fr.N(lc[0] - u, lc[1] - v), yaw = yawOf(d[0], d[2]);
      spotAt(l, x, z, 2, yaw, F0 + .45); useAt(x, F0 + .45, z, "sit", "Sit down", yaw);
    });
    void t;
  }

  // ---------------------------------------------------------------- inside, near the camera
  function inside(l, m, rec) {
    if (l.house.plan === "house_01") return inside01(l, m, rec);
    const Fr = frame(l), Hs = l.house, s = sg(l), wood = ROOM_WOOD, plasterIn = [.82, .78, .7], boards = [.42, .29, .18], tiles = [.7, .68, .64];
    const B = (u, y0, v, su, sy, sv, col, yaw, f) => yb(m, Fr, u, y0, v, su, sy, sv, yaw || 0, col, f || 19);
    // floors and ceilings
    for (const r of Hs.rooms) {
      const [u0, v0, u1, v1] = r.rect, wet = /Bath|Utility|Kitchen|Pantry/.test(r.name), y = r.floor ? F1 : F0;
      qd(m, Fr, [u0, y, v0], [u1, y, v0], [u1, y, v1], [u0, y, v1], wet ? tiles : boards, wet ? 16 : 15);
      if (r.name !== "Hall") { const cy = r.floor ? C1 : C0; qd(m, Fr, [u0, cy, v1], [u1, cy, v1], [u1, cy, v0], [u0, cy, v0], [.8, .76, .68], 18, "down"); }
    }
    qd(m, Fr, [-1.5, C1, 1.2], [1.5, C1, 1.2], [1.5, C1, -5], [-1.5, C1, -5], [.8, .76, .68], 18, "down");   // the hall's ceiling, two storeys up
    qd(m, Fr, [-1.5, C1, 2.6], [1.5, C1, 2.6], [1.5, C1, 1.2], [-1.5, C1, 1.2], [.8, .76, .68], 18, "down");
    bx(m, Fr, -1.5, C0, 2.6 - PART / 2, 1.5, C1, 2.6 + PART / 2, plasterIn, 17);            // the closed space over the pantry: its wall to the stair
    qd(m, Fr, [-1.5, C1, 5], [1.5, C1, 5], [1.5, C1, 2.6], [-1.5, C1, 2.6], [.8, .76, .68], 18, "down");
    for (const sd of [-1, 1]) { const a = sd > 0 ? 5 : -6.5, b = sd > 0 ? 6.5 : -5; qd(m, Fr, [a, F1, 1.2], [b, F1, 1.2], [b, F1, 2.3], [a, F1, 2.3], boards, 15); }
    // inner walls with their doorways: plaster both sides, a door frame and a door leaf standing open
    for (const f of [0, 1]) {
      const y0 = f ? F1 : F0, y1 = f ? C1 : C0;
      for (const [u0, v0, u1, v1, ds] of innerWalls(l, f)) {
        const along = u0 === u1 ? "v" : "u", a0 = along === "u" ? Math.min(u0, u1) : Math.min(v0, v1), a1 = along === "u" ? Math.max(u0, u1) : Math.max(v0, v1);
        const cuts = ds.map(d => along === "u" ? d[0] : d[1]).sort((p, q) => p - q);
        let at = a0;
        const seg = (p, q, ya, yb2) => along === "u" ? bx(m, Fr, p, ya, v0 - PART / 2, q, yb2, v0 + PART / 2, plasterIn, 17) : bx(m, Fr, u0 - PART / 2, ya, p, u0 + PART / 2, yb2, q, plasterIn, 17);
        for (const c of cuts) {
          seg(at, c - .5, y0, y1); seg(c - .5, c + .5, y0 + DOOR_H, y1); at = c + .5;
          for (const e of [c - .5, c + .5]) along === "u" ? bx(m, Fr, e - .05, y0, v0 - .08, e + .05, y0 + DOOR_H + .08, v0 + .08, wood, 19) : bx(m, Fr, u0 - .08, y0, e - .05, u0 + .08, y0 + DOOR_H + .08, e + .05, wood, 19);
        }
        if (a1 - at > .02) seg(at, a1, y0, y1);
      }
    }
    // the stair: treads and risers in wood, the landing, a handrail along the central flight
    for (const [u0, v0, u1, v1, top] of stairSteps(l)) {
      bx(m, Fr, u0, top - .04, v0, u1, top, v1, [.4, .27, .17], 15);
      if (top < F1 - .01 || Math.abs(u1 - u0) < 1.2) bx(m, Fr, u0, F0, v0, u1, top - .04, v1, [.74, .7, .62], 17);
      else bx(m, Fr, u0, top - .24, v0, u1, top - .04, v1, [.74, .7, .62], 17);              // a side landing: a slab over the passage
    }
    for (const u of [-.8, .8]) { bx(m, Fr, u - .03, F0, -2.3, u + .03, F0 + 2.6, -2.25, wood, 19); bx(m, Fr, u - .03, 1.8 + .9, 1.15, u + .03, 1.8 + .95, 1.2, wood, 19); }
    for (const u of [-.8, .8]) for (let k = 0; k <= 8; k++) { const v = -2.3 + k * 3.5 / 8, y = F0 + 1.6 * k / 8; bx(m, Fr, u - .02, y, v - .02, u + .02, y + .9, v + .02, wood, 19); }
    for (const [ua, ub] of [[-1.5, -.85], [.85, 1.5]]) { bx(m, Fr, ua, 1.8 + .9, 1.18, ub, 1.8 + .96, 1.24, wood, 19); }
    // furniture, room by room (in the frame as drawn: s turns it with a mirrored plan)
    const U = u => u * s;
    // living room: sofa and armchairs at the residents' seats, a fireplace on the side wall, a rug, a bookcase
    B(U(-6.3), F0, -1.9, .4, 1.2, 1.8, C.stoneDark, 0, 19); B(U(-6.25), F0 + .1, -1.9, .3, .6, 1.0, [.15, .12, .1], 0, 19);
    B(U(-4.1), F0, -1.9, 3.0, .02, 2.4, pick([C.terracotta, C.blue, C.olive], hash(l.id, "rug")), 0, 19);
    B(U(-2.0), F0, -4.6, 1.4, 2.0, .35, wood, 0, 19);
    // kitchen: a counter run along the side wall with a cooker and sink, a dining table and chairs
    B(U(6.15), F0, -2.0, .6, .9, 5.6, wood, 0, 19); B(U(6.15), F0 + .9, -2.0, .64, .04, 5.64, tiles, 0, 19);
    B(U(6.15), F0 + .94, -3.4, .5, .04, .6, C.charcoal, 0, 19); B(U(6.15), F0 + .94, -.8, .5, .06, .7, [.7, .72, .74], 0, 19);
    B(U(3.6), F0 + .72, -2.0, .9, .05, 1.8, wood, 0, 19); for (const v of [-2.6, -1.4]) for (const u of [2.8, 4.4]) B(U(u), F0, v, .42, .45, .42, wood, 0, 19);
    // study: a desk, a chair, shelves; utility: washer and coats; pantry: shelves
    B(U(-5.6), F0, 3.6, 1.4, .76, .7, wood, Math.PI / 2, 19); B(U(-4.8), F0, 3.6, .45, .45, .45, C.olive, 0, 19); B(U(-2.0), F0, 3.6, .35, 2.0, 1.6, wood, 0, 19);
    B(U(6.1), F0, 3.9, .6, .85, .6, [.85, .85, .83], 0, 19); B(U(6.1), F0, 3.2, .6, .85, .6, [.85, .85, .83], 0, 19);
    B(U(.0), F0, 4.6, 2.6, 2.0, .45, wood, 0, 19);
    // bedrooms: the beds the residents sleep in (a double in the main bedroom), wardrobes; the bathroom
    const beds = (l.objects.bed || []).map(([x, z]) => { const dx = x - Fr.cx, dz = z - Fr.cz; return [dx * Fr.c + dz * Fr.s, -dx * Fr.s + dz * Fr.c]; });
    const main = beds.filter(([u, v]) => u * s < 0), others = beds.filter(([u, v]) => u * s > 0);
    if (main.length) { const v = main.reduce((a, b) => a + b[1], 0) / main.length, u = U(-5.3); B(u, F1, v, 2.1, .45, main.length > 1 ? 1.8 : 1.1, wood, 0, 19); B(u, F1 + .45, v, 2.0, .14, main.length > 1 ? 1.7 : 1.0, [.82, .78, .7], 0, 19); B(u + .4 * s, F1 + .59, v, 1.2, .06, main.length > 1 ? 1.72 : 1.02, pick([C.blue, C.olive, C.plum, C.terracotta], hash(l.id, "bl")), 0, 19); B(U(-6.35), F1, v, .1, 1.1, main.length > 1 ? 1.9 : 1.2, wood, 0, 19); }
    for (const [u, v] of others) { B(u, F1, v, 2.0, .45, 1.0, wood, 0, 19); B(u, F1 + .45, v, 1.9, .14, .94, [.82, .78, .7], 0, 19); B(u - .3 * s, F1 + .59, v, 1.2, .06, .96, pick([C.cream, C.blue, C.olive], hash(l.id, u, v)), 0, 19); }
    B(U(-2.0), F1, -4.4, .6, 2.0, 1.6, wood, 0, 19); B(U(2.0), F1, -4.4, .6, 2.0, 1.4, wood, 0, 19);
    B(U(-5.4), F1, 4.6, 1.7, .55, .75, [.9, .9, .88], 0, 19); B(U(-2.0), F1, 4.6, .5, .85, .45, [.9, .9, .88], 0, 19); B(U(-3.2), F1, 4.7, .4, .42, .55, [.9, .9, .88], 0, 19);
    // lamps for the room light (play.js roomLight)
    if (rec) for (const [u, v, f] of [[-4, -1.9, 0], [4, -2, 0], [0, -3.5, 0], [-4, 3.6, 0], [4, 3.6, 0], [-4, -1.9, 1], [4, -1.9, 1], [-4, 3.6, 1], [4, 3.6, 1]]) { const p = Fr.P(U(u), (f ? C1 : C0) - .4, v); rec.lamps.push([p[0], p[1], p[2], 1.1]); }
  }


  // The slice's world turns the other way from Blender's (seen from the lane, +u would run to the left), so house_01 is drawn
  // through these: u as in Blender's x, the living room on the left as you face the front door.
  const bx1 = (m, Fr, u0, y0, v0, u1, y1, v1, col, f) => bx(m, Fr, -u0, y0, v0, -u1, y1, v1, col, f);
  const yb1 = (m, Fr, u, y0, v, su, sy, sv, yaw, col, f) => yb(m, Fr, -u, y0, v, su, sy, sv, -(yaw || 0), col, f);
  const qd1 = (m, Fr, a, b, c, d, col, f, nu, nv) => qd(m, Fr, [-a[0], a[1], a[2]], [-b[0], b[1], b[2]], [-c[0], c[1], c[2]], [-d[0], d[1], d[2]], col, f, typeof nu === "number" ? -nu : nu, nv);
  const chain1 = (Fr, u0, v0, u1, v1, y0, y1, gaps, r) => chain(Fr, -u0, v0, -u1, v1, y0, y1, gaps && gaps.map(g => [-g[0], g[1]]), r);
  const grid1 = (Fr, u0, v0, u1, v1, y0, y1, cell) => grid(Fr, -Math.max(u0, u1), v0, -Math.min(u0, u1), v1, y0, y1, cell);
  const solidAt1 = (Fr, u, v, y0, y1, r) => solidAt(Fr, -u, v, y0, y1, r);
  // ================================================================ plan "house_01": the house approved in Blender (Tools/blender/house_01.py,
  // owner 2026-11-22 "House looks good"). Same frame (u across the front, v front -5 .. back +5); Blender x = u + 6.5, y = v + 5.
  // Heights as there: a 0.45 m plinth, 3.5 m storeys, walls 7.45 m to the eaves, a gabled roof at 30 degrees, ridge along the front.
  const P = .45, FH = 3.5, G0 = P, G1 = P + FH, HH = P + 2 * FH, HALF = P + FH / 2, K0 = G1 - .2, K1 = HH - .05, WT = .32;
  const WOOD_RED = [.45, .17, .12], CREAM_F = [.78, .74, .64], STONE_Q = [.66, .63, .57];
  // windows and doors on each face, as drawn (u or v of the middle, bottom, top, width); the same numbers as the Blender settings
  const H01 = {
    front: [[-5.2, G0 + .9, G0 + 3.1, 1.2, "box"], [-3.4, G0 + .9, G0 + 3.1, 1.2, "box"], [3.4, G0 + .9, G0 + 3.1, 1.2, "box"], [5.2, G0 + .9, G0 + 3.1, 1.2, "box"],
            [-4.4, G1 + 1.2, G1 + 2.6, 1.0], [3.4, G1 + .8, G1 + 2.8, 1.2, "box"], [5.2, G1 + .8, G1 + 2.8, 1.2, "box"],
            [0, G1 + .25, G1 + 2.85, 1.6, "noshut"]],                                                  // the tall hall window over the porch
    frontDoor: [0, G0, G0 + 2.4, 1.2],
    back: [[0, G0 + 2.0, G0 + 4.0, 1.4], [-5.39, G1 + .02, G1 + 2.32, 1.4, "door"], [5.39, G1 + .02, G1 + 2.32, 1.4, "door"]],   // the stair window; the balcony doors
    backDoor: [5.8, G0, G0 + 2.3, 1.0],
    // the furniture as house_01.py's FURNITURE: [floor (+ a lift), piece, Blender x, y, yaw]; x = u + 6.5, y = v + 5
    furniture: [
      [0, "l_fireplace", .54, 4.0, Math.PI / 2], [0, "o_painting", .34, 4.0, Math.PI / 2], [0, "k17:rug", 2.3, 4.0, Math.PI / 2],
      [0, "k17:sofa", 3.85, 4.0, -Math.PI / 2], [0, "l_sidetable", 3.95, 5.45, 0], [0, "k17:coffee", 2.3, 4.0, Math.PI / 2],
      [0, "k17:armchair", 1.9, 2.15, Math.PI], [0, "k17:armchair", 1.9, 5.85, 0], [0, "l_piano", 2.0, 7.6, 0],
      [0, "k17:floor_lamp", 3.95, 7.35, 0], [0, "o_floorplant", .75, .75, 0],
      [0, "h_console", 4.64, 3.4, Math.PI / 2], [0, "h_mirror", 4.42, 3.4, Math.PI / 2], [0, "h_shoebench", 8.36, 3.4, -Math.PI / 2],
      [0, "h_hooks", 8.58, 3.4, -Math.PI / 2], [0, "h_coatstand", 8.15, .75, 0], [0, "h_umbrella", 4.8, .75, 0], [0, "k17:rug", 6.5, 2.3, 0],
      [0, "k_counter", 12.36, 1.3, -Math.PI / 2], [0, "k_sink", 12.36, 2.75, -Math.PI / 2], [0, "k_stove", 12.37, 3.75, -Math.PI / 2],
      [0, "k_wall", 12.53, 3.75, -Math.PI / 2], [0, "k_counter", 12.36, 5.0, -Math.PI / 2], [0, "k_fridge", 9.45, 7.5, 0],
      [0, "k_potshelf", 8.84, 5.6, Math.PI / 2], [0, "k17:dining", 10.3, 3.0, Math.PI / 2], [.35, "d_pendant", 10.3, 3.0, 0],
      [1, "b_bath", 1.3, .77, Math.PI], [1, "b_toilet", 4.02, 1.15, -Math.PI / 2], [1, "b_basin", 1.3, 2.24, 0],
      [1, "b_mirror", 1.3, 2.52, 0], [1, "b_basket", 3.95, 2.15, 0],
      [1, "k17:rug", 2.3, 5.0, Math.PI / 2], [1, "k17:bed", 3.26, 5.0, -Math.PI / 2], [1, "k17:bedside", 4.1, 3.85, -Math.PI / 2],
      [1, "k17:bedside", 4.1, 6.15, -Math.PI / 2], [1, "k17:wardrobe", 2.7, 7.57, 0], [1, "r_chest", .55, 3.4, Math.PI / 2],
      [1, "k17:rug", 10.6, 4.0, 0], [1, "r_single", 11.68, 4.0, -Math.PI / 2], [1, "k17:bedside", 12.45, 4.9, -Math.PI / 2],
      [1, "k17:wardrobe", 8.97, 3.0, Math.PI / 2], [1, "s_desk", 10.6, .67, Math.PI], [1, "s_chair", 10.6, 1.3, 0],
      [1, "r_chest", 9.9, 7.57, 0],
      [1, "g_bistro", 1.6, 10.65, 0], [1, "g_bistro", 11.4, 10.65, 0],
    ],
    left: [[1.0, G0 + .9, G0 + 2.9, 1.1], [-3.0, G0 + .9, G0 + 2.9, 1.1], [.2, G1 + .8, G1 + 2.7, 1.1], [-3.7, G1 + 1.2, G1 + 2.5, .9]],
    right: [[-3.0, G0 + .9, G0 + 2.9, 1.1], [1.0, G0 + .9, G0 + 2.9, 1.1], [-3.0, G1 + .8, G1 + 2.7, 1.1], [1.0, G1 + .8, G1 + 2.7, 1.1]],
  };
  // the furniture of one house: the list above, with a second and third single bed in bedroom 2 for a fourth and fifth resident
  // (the fifth's takes the desk chair's place)
  function furn01(l) {
    const n = ((l.objects || {}).bed || []).length, out = H01.furniture.filter(p => !(n >= 5 && p[1] === "s_chair"));
    if (n >= 4) out.push([1, "r_single", 11.68, 6.0, -Math.PI / 2]);
    if (n >= 5) out.push([1, "r_single", 11.68, 2.0, -Math.PI / 2]);
    return out;
  }
  function steps01(l) {                                       // [u0, v0, u1, v1, top] in the frame as drawn
    if (l._steps) return l._steps;
    const out = [], n = 10;
    for (let k = 0; k < n; k++) out.push([-1.3, -.05 + 3 * k / n, 1.3, -.05 + 3 * (k + 1) / n, G0 + (HALF - G0) * (k + 1) / n]);
    out.push([-2.1, 2.95, 2.1, 4.68, HALF]);                                                         // the half landing, against the back wall
    for (const sd of [-1, 1]) {
      for (let k = 0; k < n; k++) { const a = 2.1 + 2.5 * k / n, b = 2.1 + 2.5 * (k + 1) / n; out.push([sd > 0 ? a : -b, 2.98, sd > 0 ? b : -a, 4.66, HALF + (G1 - HALF) * (k + 1) / n]); }
      out.push([sd > 0 ? 4.6 : -6.18, 2.85, sd > 0 ? 6.18 : -4.6, 4.68, G1]);                        // the corner landing
    }
    return (l._steps = out);
  }

  function outside01(l) {
    const Fr = frame(l), Hs = l.house, tr = Hs.trim, s = sg(l), m = world, dm = detailOf(l.block), U = u => u * s;   // dm: the small parts, drawn only near
    const wallC = PLASTER[tr.plaster % PLASTER.length], roofC = ROOF[tr.roof_tone % ROOF.length], shut = SH[tr.shutter_color] || C.teal;
    l.H = HH + 3.3;
    // a box on a wall face: a = along the wall, c = across it (axis "u": a is u and c is v; axis "v": the other way round)
    const wb = (ax, a0, z0, c0, a1, z1, c1, col, f, mm) => ax === "u" ? bx1(mm || m, Fr, a0, z0, c0, a1, z1, c1, col, f) : bx1(mm || m, Fr, c0, z0, a0, c1, z1, a1, col, f);
    function holed(ax, a0, a1, c0, c1, holes) {                 // a wall from 0 to the eaves with rectangular holes [a0, a1, z0, z1]
      const xs = [...new Set([a0, a1, ...holes.flatMap(h => [Math.max(a0, Math.min(a1, h[0])), Math.max(a0, Math.min(a1, h[1]))])])].sort((p, q) => p - q);
      for (let i = 0; i + 1 < xs.length; i++) {
        const p = xs[i], q = xs[i + 1], mid = (p + q) / 2; if (q - p < .005) continue;
        let z = 0;
        for (const [h0, h1] of holes.filter(h => h[0] < mid && h[1] > mid).map(h => [h[2], h[3]]).sort((x, y) => x[0] - y[0])) { if (h0 > z + .005) wb(ax, p, z, c0, q, h0, c1, wallC, 0); z = Math.max(z, h1); }
        if (HH > z + .005) wb(ax, p, z, c0, q, HH, c1, wallC, 0);
      }
    }
    // a window (or glazed door) in a wall whose outer face is at c = line, facing outward `face` (+1 or -1)
    function opening(ax, line, face, a, z0, z1, w, kind) {
      const out = d => line + face * d, h = z1 - z0;
      wb(ax, a - w / 2, z0, out(-.13), a + w / 2, z1, out(-.11), GLASS, 4);                                  // the glass
      for (const [p, q] of [[a - w / 2, a - w / 2 + .07], [a + w / 2 - .07, a + w / 2], [a - .03, a + .03]]) wb(ax, p, z0, out(-.16), q, z1, out(-.08), CREAM_F, 9, dm);
      wb(ax, a - w / 2, z0, out(-.16), a + w / 2, z0 + .07, out(-.08), CREAM_F, 9, dm); wb(ax, a - w / 2, z1 - .07, out(-.16), a + w / 2, z1, out(-.08), CREAM_F, 9, dm);
      if (kind !== "door") wb(ax, a - w / 2, z0 + h * .7, out(-.16), a + w / 2, z0 + h * .7 + .05, out(-.08), CREAM_F, 9, dm);   // the transom
      if (kind !== "door") wb(ax, a - w / 2 - .1, z0 - .1, out(-.1), a + w / 2 + .1, z0, out(.08), STONE_L, 6);              // the sill
      wb(ax, a - w / 2 - .12, z1, out(-.02), a + w / 2 + .12, z1 + .18, out(.04), STONE_L, 6);                             // the head
      if (tr.shutters && kind !== "noshut") for (const sd of [-1, 1]) {
        const e = a + sd * (w / 2 + .02), e0 = Math.min(e, e + sd * w / 2), e1 = Math.max(e, e + sd * w / 2);
        wb(ax, e0, z0, out(0), e1, z1, out(.05), shut, 12);
        for (let k = 1; k < 6; k++) wb(ax, e0 + .04, z0 + h * k / 6 - .015, out(.05), e1 - .04, z0 + h * k / 6 + .015, out(.065), mul3(shut, .7), 12, dm);
      }
      if (tr.window_boxes && kind === "box") {
        wb(ax, a - w / 2, z0 - .42, out(0), a + w / 2, z0 - .12, out(.3), C.brown, 9, dm);
        for (let k = 0; k < 5; k++) { const pa = a - w / 2 + .15 + k * (w - .3) / 4, pc = out(.15), uu = ax === "u" ? pa : pc, vv = ax === "u" ? pc : pa; yb1(dm, Fr, uu, z0 - .12, vv, .2, .18, .2, 0, pick([C.terracotta, C.ochre, C.olive, C.plum, C.cream], hash(l.id, "fl", a, k)), 9); }
      }
    }
    const hole = (a, z0, z1, w) => [a - w / 2, a + w / 2, z0, z1];
    // the four walls (front and back along u, the sides along v), with their holes
    const fr = H01.front.map(([a, z0, z1, w, k]) => [U(a), z0, z1, w, k]), bk = H01.back.map(([a, z0, z1, w, k]) => [U(a), z0, z1, w, k]);
    const fd = H01.frontDoor, bd = [U(H01.backDoor[0]), ...H01.backDoor.slice(1)];
    const sides = [[U(-6.5), -s, H01.left], [U(6.5), s, H01.right]];
    holed("u", -6.5, 6.5, -5, -5 + WT, [...fr.map(([a, z0, z1, w]) => hole(a, z0, z1, w)), hole(fd[0], fd[1], fd[2], fd[3])]);
    holed("u", -6.5, 6.5, 5 - WT, 5, [...bk.map(([a, z0, z1, w]) => hole(a, z0, z1, w)), hole(bd[0], bd[1], bd[2], bd[3])]);
    for (const [line, face, list] of sides) holed("v", -5 + WT, 5 - WT, Math.min(line, line - face * WT), Math.max(line, line - face * WT), list.map(([a, z0, z1, w]) => hole(a, z0, z1, w)));
    for (const [a, z0, z1, w, k] of fr) opening("u", -5, -1, a, z0, z1, w, k);
    for (const [a, z0, z1, w, k] of bk) opening("u", 5, 1, a, z0, z1, w, k);
    for (const [line, face, list] of sides) for (const [a, z0, z1, w, k] of list) opening("v", line, face, a, z0, z1, w, k);
    // the doors: the front door open into the hall, the back door shut; their stone surrounds
    bx1(m, Fr, -.6, G0, -5 + WT, -.52, G0 + 2.4, -5 + WT + 1.18, WOOD_RED, 9);   // hinged on the left jamb, swung open flat into the hall
    for (const z of [G0 + .3, G0 + 2.05]) bx1(dm, Fr, -.62, z, -5 + WT - .02, -.5, z + .1, -5 + WT + .06, C.charcoal, 9);   // its hinges
    bx1(m, Fr, bd[0] - .5, G0, 5 - .2, bd[0] + .5, G0 + 2.3, 5 - .12, WOOD_RED, 9);
    for (const [a, z1, w, c] of [[0, G0 + 2.4, 1.2, -5], [bd[0], G0 + 2.3, 1.0, 5]]) {
      const o = Math.sign(c);
      for (const e of [a - w / 2 - .2, a + w / 2]) bx1(m, Fr, e, G0, c - o * .05, e + .2, z1, c + o * .07, C.stoneDark, 6);
      bx1(m, Fr, a - w / 2 - .28, z1, c - o * .05, a + w / 2 + .28, z1 + .3, c + o * .1, C.stoneDark, 6);
    }
    // plinth, string course at the first floor, quoins at the corners
    for (const [u0, v0, u1, v1] of [[-6.55, -5.05, 6.55, -5], [-6.55, 5, 6.55, 5.05], [-6.55, -5, -6.5, 5], [6.5, -5, 6.55, 5]]) {
      bx1(m, Fr, u0, 0, v0, u1, P, v1, C.stoneDark, 6);
      bx1(m, Fr, u0, G1 - .16, v0, u1, G1 - .02, v1, STONE_L, 6);
    }
    for (const [cu, cv] of [[-6.5, -5], [6.5, -5], [-6.5, 5], [6.5, 5]]) for (let k = 0; P + k * .45 + .4 < HH; k++) {
      const z0 = P + k * .45, la = k % 2 ? .3 : .55, lb = k % 2 ? .55 : .3, su = Math.sign(cu), sv = Math.sign(cv);
      bx1(dm, Fr, cu - su * la, z0, cv, cu + su * .03, z0 + .4, cv + sv * .03, STONE_Q, 6);
      bx1(dm, Fr, cu, z0, cv - sv * lb, cu + su * .03, z0 + .4, cv + sv * .03, STONE_Q, 6);
    }
    // the porch: a stone floor at the plinth's height, two columns, a beam and a small pediment; steps down to the path
    bx1(m, Fr, -1.85, 0, -6.75, 1.85, P, -5, C.stoneDark, 6);
    for (let k = 0; k < 3; k++) bx1(m, Fr, -.9, 0, -6.75 - (k + 1) * .32, .9, P - (k + 1) * P / 3.4, -6.75 - k * .32, C.stoneDark, 6);
    for (const u of [-1.45, 1.45]) { yb1(m, Fr, u, P, -6.35, .34, 2.7, .34, 0, C.cream, 6); yb1(dm, Fr, u, P, -6.35, .46, .12, .46, 0, STONE_L, 6); yb1(dm, Fr, u, P + 2.58, -6.35, .46, .12, .46, 0, STONE_L, 6); }
    bx1(m, Fr, -1.7, P + 2.7, -6.6, 1.7, P + 3.0, -5, C.stoneDark, 6);
    qd1(m, Fr, [-1.8, P + 3.0, -6.62], [1.8, P + 3.0, -6.62], [0, P + 3.65, -6.62], [0, P + 3.65, -6.62], wallC, 0, 0, -1);
    qd1(m, Fr, [-1.95, P + 2.95, -6.75], [0, P + 3.7, -6.75], [0, P + 3.7, -5], [-1.95, P + 2.95, -5], roofC, 11);
    qd1(m, Fr, [0, P + 3.7, -6.75], [1.95, P + 2.95, -6.75], [1.95, P + 2.95, -5], [0, P + 3.7, -5], roofC, 11);
    grid1(Fr, -1.85, -6.75, 1.85, -5, 0, P); for (let k = 0; k < 3; k++) grid1(Fr, -.9, -6.75 - (k + 1) * .32, .9, -6.75 - k * .32, 0, P - (k + 1) * P / 3.4);
    for (const u of [-1.45, 1.45]) solidAt1(Fr, u, -6.35, P, P + 2.7, .2);
    bx1(m, Fr, bd[0] - .7, 0, 5, bd[0] + .7, .25, 5.45, C.stoneDark, 6); grid1(Fr, bd[0] - .7, 5, bd[0] + .7, 5.45, 0, .25);   // a step at the back door
    // the balconies at the back corners: a stone slab level with the first floor, two brackets, an iron railing
    for (const ex of (Hs.extras || []).filter(e => e.kind === "balcony")) {
      const a0 = Math.min(U(ex.rect[0]), U(ex.rect[2])), a1 = Math.max(U(ex.rect[0]), U(ex.rect[2])), v1 = ex.rect[3];
      bx1(m, Fr, a0, G1 - .18, 5, a1, G1, v1, C.stoneDark, 6);
      for (const a of [a0 + .45, a1 - .45]) for (let k = 0; k < 4; k++) bx1(dm, Fr, a - .12, G1 - .9 + k * .18, 5, a + .12, G1 - .72 + k * .18, 5 + (v1 - 5 - .2) * (k + 1) / 4, C.stoneDark, 6);
      bx1(m, Fr, a0 + .03, G1 + .95, v1 - .07, a1 - .03, G1 + 1.0, v1 - .02, C.charcoal, 9);
      for (const a of [a0 + .03, a1 - .08]) bx1(m, Fr, a, G1 + .95, 5, a + .05, G1 + 1.0, v1 - .02, C.charcoal, 9);
      const n = Math.round((a1 - a0) / .14);
      for (let i = 0; i <= n; i++) { const a = a0 + .05 + (a1 - a0 - .1) * i / n; bx1(dm, Fr, a - .012, G1, v1 - .06, a + .012, G1 + .95, v1 - .035, C.charcoal, 9); }
      for (const a of [a0 + .055, a1 - .055]) for (let i = 1; i <= 8; i++) { const v = 5 + (v1 - 5 - .05) * i / 9; bx1(dm, Fr, a - .012, G1, v - .012, a + .012, G1 + .95, v + .012, C.charcoal, 9); }
      grid1(Fr, a0, 5, a1, v1, G1 - .18, G1, .7);
      chain1(Fr, a0, v1, a1, v1, G1, G1 + 1.0, null, .08); chain1(Fr, a0, 5.3, a0, v1, G1, G1 + 1.0, null, .08); chain1(Fr, a1, 5.3, a1, v1, G1, G1 + 1.0, null, .08);
    }
    // the roof: gabled, the ridge along the front, 30 degrees, 0.55 m eaves; plastered gable ends; a chimney on each gable
    const o = .55, R = (5 + o) * Math.tan(Math.PI / 6), E = HH, GR = 5 * Math.tan(Math.PI / 6);
    qd1(m, Fr, [-6.5 - o, E, -5 - o], [6.5 + o, E, -5 - o], [6.5 + o, E + R, 0], [-6.5 - o, E + R, 0], roofC, 11);
    qd1(m, Fr, [-6.5 - o, E + R, 0], [6.5 + o, E + R, 0], [6.5 + o, E, 5 + o], [-6.5 - o, E, 5 + o], roofC, 11);
    qd1(m, Fr, [-6.5 - o, E - .01, -5 - o], [6.5 + o, E - .01, -5 - o], [6.5 + o, E - .01, -5], [-6.5 - o, E - .01, -5], mul3(roofC, .6), 11, "down");   // the eaves' undersides
    qd1(m, Fr, [-6.5 - o, E - .01, 5], [6.5 + o, E - .01, 5], [6.5 + o, E - .01, 5 + o], [-6.5 - o, E - .01, 5 + o], mul3(roofC, .6), 11, "down");
    for (const u of [-6.5, 6.5]) qd1(m, Fr, [u, E, -5], [u, E, 5], [u, E + GR, 0], [u, E + GR, 0], wallC, 0, Math.sign(u), 0);
    bx1(m, Fr, -6.5 - o, E + R - .02, -.1, 6.5 + o, E + R + .1, .1, mul3(roofC, .8), 11);                                     // the ridge tiles
    for (const u of [-5.5, 5.5]) { bx1(m, Fr, u - .4, E + 1, -.35, u + .4, E + R + 1.0, .35, wallC, 0); bx1(m, Fr, u - .48, E + R + 1.0, -.43, u + .48, E + R + 1.12, .43, STONE_L, 6); for (const d of [-.15, .15]) yb1(dm, Fr, u + d, E + R + 1.12, 0, .14, .28, .14, 0, C.terracotta, 9); }
    // what cannot be walked through: the outer walls (gaps at the doors), inner walls, the first floor, the stair, the rails
    chain1(Fr, -6.5, -5, 6.5, -5, 0, HH, [[0, -5]]);
    chain1(Fr, -6.5, 5, 6.5, 5, 0, G1 - .2, [[bd[0], 5]]); chain1(Fr, -6.5, 5, 6.5, 5, G1 + .05, HH, [[U(-5.39), 5], [U(5.39), 5]]);
    chain1(Fr, -6.5, -5, -6.5, 5, 0, HH); chain1(Fr, 6.5, -5, 6.5, 5, 0, HH);
    grid1(Fr, -6.18, -4.68, 6.18, 4.68, 0, G0);
    for (const f of [0, 1]) for (const [u0, v0, u1, v1, ds] of innerWalls(l, f)) chain1(Fr, u0, v0, u1, v1, f ? G1 + .05 : G0, f ? K1 : K0 - .3, ds, .12);
    for (const r of Hs.rooms.filter(r => r.floor === 1)) { const [u0, v0, u1, v1] = r.rect; grid1(Fr, u0, v0, u1, v1, K0, G1, .7); }
    for (const [u0, v0, u1, v1, top] of steps01(l)) { const a = U(u0), b = U(u1); grid1(Fr, Math.min(a, b), v0, Math.max(a, b), v1, top >= G1 - .01 && Math.abs(u1 - u0) < 2 ? top - .2 : 0, top, .6); }
    chain1(Fr, -2.1, 2.95, -1.3, 2.95, HALF, HALF + 1, null, .06); chain1(Fr, 1.3, 2.95, 2.1, 2.95, HALF, HALF + 1, null, .06);   // a rail where the half landing meets the hall
    // the furniture is solid: a block over each piece's footprint (pieces stand square to the house, a quarter turn swaps width and
    // depth), at least 0.9 m high so a low table stops you rather than being stepped on
    furn01(l).forEach(([f, kind, x, y, th]) => {
      const sz = Furniture.SIZE[kind]; if (!sz) return;
      const q = Math.round(th / (Math.PI / 2)) % 2 !== 0, w = q ? sz[1] : sz[0], d = q ? sz[0] : sz[1], y0 = Math.floor(f) ? G1 : G0;
      const pu = x - 6.5, pv = y - 5, a0 = U(pu - w / 2), a1 = U(pu + w / 2);
      grid1(Fr, Math.min(a0, a1) + .05, pv - d / 2 + .05, Math.max(a0, a1) - .05, pv + d / 2 - .05, y0, y0 + Math.max(sz[2], .9), .6);
    });
    places01(l, Fr);
    l.furn = ["house"];
    occl.push([Math.min(l.rect[0], l.rect[2]) - 2, Math.min(l.rect[1], l.rect[3]) - 2, Math.max(l.rect[0], l.rect[2]) + 2, Math.max(l.rect[1], l.rect[3]) + 2, HH]);
  }

  function places01(l, Fr) {
    const o = l.objects || {}, s = sg(l), loc = (x, z) => { const dx = x - Fr.cx, dz = z - Fr.cz; return [-(dx * Fr.c + dz * Fr.s), -dx * Fr.s + dz * Fr.c]; };
    (o.bed || []).forEach(([x, z]) => {                                       // lying along u, the head to +u (as the beds stand)
      const d = Fr.N(-1, 0), yaw = yawOf(-d[0], -d[2]);
      spotAt(l, x, z, 3, yaw, G1 + .62); useAt(x, G1 + .62, z, "lie", "Lie down", yaw);
    });
    (o.room || []).forEach(([x, z]) => {                                      // facing the fireplace on the living room's side wall
      const [u, v] = loc(x, z), d = Fr.N(-(-6 * s - u), -1 - v), yaw = yawOf(d[0], d[2]);
      spotAt(l, x, z, 2, yaw, G0 + .45); useAt(x, G0 + .45, z, "sit", "Sit down", yaw);
    });
  }

  function inside01(l, m, rec) {
    const Fr = frame(l), Hs = l.house, s = sg(l), U = u => u * s, wood = ROOM_WOOD, plasterIn = [.82, .78, .7], boards = [.42, .29, .18], tiles = [.7, .68, .64], ceil = [.8, .76, .68];
    const R = (u0, y0, v0, u1, y1, v1, col, f) => bx1(m, Fr, U(u0), y0, v0, U(u1), y1, v1, col, f || 19);
    // floors, ceilings, the first floor's slabs; one ceiling over the whole house under the roof (the hall is two storeys high)
    for (const r of Hs.rooms) {
      const [u0, v0, u1, v1] = r.rect, wet = /Bath|En-suite|Kitchen/.test(r.name), y = r.floor ? G1 : G0;
      qd1(m, Fr, [u0, y + .005, v0], [u1, y + .005, v0], [u1, y + .005, v1], [u0, y + .005, v1], wet ? tiles : boards, wet ? 16 : 15);
      if (r.floor === 1) bx1(m, Fr, u0 + .01, K0, v0 + .01, u1 - .01, G1, v1 - .01, plasterIn, 17);
      else if (r.name !== "Hall") qd1(m, Fr, [u0, K0 - .005, v1], [u1, K0 - .005, v1], [u1, K0 - .005, v0], [u0, K0 - .005, v0], ceil, 18, "down");
    }
    qd1(m, Fr, [-6.5, K1, 5], [6.5, K1, 5], [6.5, K1, -5], [-6.5, K1, -5], ceil, 18, "down");
    // the outer walls' inner faces, in room plaster, with the same holes as outside (the outside walls are lit as outdoors)
    {
      const hole = ([a, z0, z1, w]) => [a - w / 2, a + w / 2, z0, z1], I = .02;
      const skin = (ax, a0, a1, c0, c1, holes) => {
        const xs = [...new Set([a0, a1, ...holes.flatMap(h => [Math.max(a0, Math.min(a1, h[0])), Math.max(a0, Math.min(a1, h[1]))])])].sort((p, q) => p - q);
        for (let i = 0; i + 1 < xs.length; i++) {
          const p = xs[i], q = xs[i + 1], mid = (p + q) / 2; if (q - p < .005) continue;
          let z = G0;
          const put = (za, zb) => ax === "u" ? bx1(m, Fr, p, za, c0, q, zb, c1, plasterIn, 17) : bx1(m, Fr, c0, za, p, c1, zb, q, plasterIn, 17);
          for (const [h0, h1] of holes.filter(h => h[0] < mid && h[1] > mid).map(h => [h[2], h[3]]).sort((x, y) => x[0] - y[0])) { if (h0 > z + .005) put(z, h0); z = Math.max(z, h1); }
          if (K1 > z + .005) put(z, K1);
        }
      };
      const Ua = ([a, ...r]) => [U(a), ...r];
      skin("u", -6.18, 6.18, -5 + WT, -5 + WT + I, [...H01.front.map(Ua), H01.frontDoor].map(hole));
      skin("u", -6.18, 6.18, 5 - WT - I, 5 - WT, [...H01.back.map(Ua), Ua(H01.backDoor)].map(hole));
      for (const [u, list] of [[U(-6.5 + WT), H01.left], [U(6.5 - WT), H01.right]]) skin("v", -5 + WT, 5 - WT, Math.min(u, u + Math.sign(u) * -I), Math.max(u, u + Math.sign(u) * -I), list.map(hole));
    }
    // inner walls with their doorways: plaster both sides, a frame round each doorway
    for (const f of [0, 1]) {
      const y0 = f ? G1 : G0, y1 = f ? K1 : K0;
      for (const [u0, v0, u1, v1, ds] of innerWalls(l, f)) {
        const along = u0 === u1 ? "v" : "u", a0 = along === "u" ? Math.min(u0, u1) : Math.min(v0, v1), a1 = along === "u" ? Math.max(u0, u1) : Math.max(v0, v1);
        const lo = along === "u" ? -6.18 : -4.68, hi = -lo, cuts = ds.map(d => along === "u" ? d[0] : d[1]).sort((p, q) => p - q);
        const seg = (p, q, ya, yb2) => { p = Math.max(p, lo); q = Math.min(q, hi); if (q - p < .02) return; along === "u" ? bx1(m, Fr, p, ya, v0 - PART / 2, q, yb2, v0 + PART / 2, plasterIn, 17) : bx1(m, Fr, u0 - PART / 2, ya, p, u0 + PART / 2, yb2, q, plasterIn, 17); };
        let at = a0;
        for (const c of cuts) {
          seg(at, c - .5, y0, y1); seg(c - .5, c + .5, y0 + DOOR_H, y1); at = c + .5;
          for (const e of [c - .5, c + .5]) along === "u" ? bx1(m, Fr, e - .05, y0, v0 - .08, e + .05, y0 + DOOR_H + .08, v0 + .08, wood, 19) : bx1(m, Fr, u0 - .08, y0, e - .05, u0 + .08, y0 + DOOR_H + .08, e + .05, wood, 19);
        }
        seg(at, a1, y0, y1);
      }
    }
    // the stair: wooden treads, plastered sides down to the floor (the corner landings stand over the space below)
    for (const [u0, v0, u1, v1, top] of steps01(l)) {
      R(u0, top - .05, v0, u1, top, v1, [.4, .27, .17], 15);
      const landing = top >= G1 - .01 && Math.abs(u1 - u0) < 2;
      R(u0, landing ? top - .25 : G0, v0, u1, top - .05, v1, [.74, .7, .62], 17);
    }
    for (const u of [-1.38, 1.38]) for (let k = 0; k <= 10; k++) { const v = -.05 + k * .3, y = G0 + (HALF - G0) * k / 10; R(u - .02, y, v - .02, u + .02, y + .9, v + .02, wood); }
    for (const u of [-1.38, 1.38]) for (let k = 0; k < 10; k++) { const v = -.05 + k * .3, y = G0 + (HALF - G0) * (k + .5) / 10; R(u - .035, y + .88, v, u + .035, y + .95, v + .3, wood); }
    for (const [a, b] of [[-2.1, -1.3], [1.3, 2.1]]) { R(a, HALF + .9, 2.93, b, HALF + .96, 2.99, wood); for (let k = 0; k <= 4; k++) { const u = a + (b - a) * k / 4; R(u - .02, HALF, 2.94, u + .02, HALF + .9, 2.98, wood); } }
    // the furniture: kits 17 and 18 in the house's line, as placed in Blender (house_01.py FURNITURE), drawn by furniture.js
    const Lf = Outlet.LINES[Hs.furniture] || Outlet.LINES.oldtown;
    furn01(l).forEach(([f, kind, x, y, th], i) => {
      const lift = f - Math.floor(f), y0 = (Math.floor(f) ? G1 : G0) + lift, pu = x - 6.5, pv = y - 5, c = Math.cos(th), sn = Math.sin(th);
      const Q = (a, yy, b, sa, sy, sb, col, fm, yaw) => yb1(m, Fr, U(pu + a * c - b * sn), y0 + yy, pv + a * sn + b * c, sa, sy, sb, -(th + (yaw || 0)), col, fm || 19);
      Furniture.draw(kind, Q, Lf, hash(l.id, "furn", i));
    });
    // lamps for the room light (play.js roomLight)
    if (rec) for (const [u, v, f] of [[-4.3, -1, 0], [4.3, -1, 0], [0, -2.5, 0], [0, 3.8, 0], [-4.3, -3.6, 1], [-4.3, .3, 1], [4.3, -1, 1], [-5.4, 3.8, 1], [5.4, 3.8, 1]]) { const p = Fr.P(-U(u), (f ? K1 : K0) - .4, v); rec.lamps.push([p[0], p[1], p[2], 1.1]); }
  }

  return { outside, inside, frame };
})();
