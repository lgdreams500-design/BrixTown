/* Every floor of every building (WORLD_MAP WLD-050 to WLD-053, F-BLD-INSIDE level 3, ADR-0021).
   The block files give each building of two or more storeys a stairwell (lot.stair), the towers a lift (lot.lift), what each
   floor is for (lot.storeys) and the floor of each upstairs place (lot.levels); World/rules/storeys.json the sizes (D.storeys).
   Storeys.shell() makes a building hollow from the first floor up, with the same outside: walls, floor slabs with a hole over
   each flight, a switchback stair (two flights a storey, a spine wall between them, a landing at each end), a rail at each hole
   and a lift in the towers; all of it solid, so the player climbs it. Storeys.build() draws one floor's slabs, stair and lift
   when the camera is in that building (play.js nearRooms); furnish() lays out the floor through Storeys.proxy().
   Heights: the ground floor at 0.2 with its ceiling at 3.2; floor f at 3f + 0.4 under a 0.2 slab; the top ceiling meets the
   roof at floors x 3 + 0.4, the height the building has from outside. Runs inside play.js's world; load it before play.js. */
"use strict";
window.Storeys = (() => {
  const floorY = f => f === 0 ? .2 : 3 * f + .4, ceilY = f => 3 * f + 3.2;
  const USE_AS = {                                          // what an upper floor is furnished as: the kind and special name play.js knows
    bedrooms: { kind: "home", sp: null }, flat: { kind: "home", sp: "flats" }, store: { kind: "shop", sp: null }, rooms: { kind: "home", sp: null },
    office: { kind: "work", sp: "office" }, wards: { kind: "health", sp: null }, stacks: { kind: "library", sp: null }, gallery: { kind: "museum", sp: null },
    plant: { kind: "utility", sp: "depot" },
  };
  const STEP = .19, WOOD = [.36, .24, .15], RAIL = [.2, .16, .12];
  const geo = new Map();

  function levelAt(l, y) {                                   // which floor of building l a foot at height y is on
    if (!l || l.floors < 2 || y < 2.6) return 0;
    return Math.max(0, Math.min(l.floors - 1, Math.floor((y - .4 + .6) / 3)));
  }
  function objectsOn(l, f) {                                // the places on floor f (the same order as in the block file)
    const lv = l.levels || {}, out = {};
    for (const k in l.objects || {}) { const v = l.objects[k], ls = lv[k]; out[k] = ls ? v.filter((_, i) => ls[i] === f) : (f === 0 ? v : []); }
    return out;
  }
  function proxy(l, f) {                                    // the lot as furnish() should see floor f: its places, what the floor is for, the stair foot as its way in
    const p = Object.create(l); p.objects = objectsOn(l, f);
    if (f > 0) {
      const as = USE_AS[(l.storeys || [])[f]] || USE_AS.rooms; p.kind = as.kind; p.sp = as.sp; p.id = l.id + "/" + f; p.enterable = true;
      const G = stairOf(l); if (G) p.door = G.footAt(f); p.inWin = (l.inWinUp || []).filter(w => w[3] === f).map(w => w.slice(0, 3));
      p.levelUse = (l.storeys || [])[f];
    }
    p.level = f; return p;
  }

  // ------------------------------------------------------------ the stairwell, worked out once per building
  function stairOf(l) {
    if (!l.stair) return null; if (geo.has(l)) return geo.get(l);
    const s = l.stair, S = D.storeys.stair, [rx0, rz0, rx1, rz1] = s.rect, ax = s.axis === "x";
    const u0 = ax ? rx0 : rz0, u1 = ax ? rx1 : rz1, v0 = ax ? rz0 : rx0, v1 = ax ? rz1 : rx1;
    const wallLow = ax ? s.wall === "n" : s.wall === "w";   // is lane 0 (against the wall) at the low v end?
    const w = S.width, sp = S.spine, lane0 = wallLow ? [v0, v0 + w] : [v1 - w, v1], lane1 = wallLow ? [v0 + w + sp, v1] : [v0, v1 - w - sp], spine = wallLow ? [v0 + w, v0 + w + sp] : [v1 - w - sp, v1 - w];
    const uLo = u0 + S.landing, uHi = u1 - S.landing;
    const P = (u, v, y) => ax ? [u, y, v] : [v, y, u];       // (along, across) to world
    const box = (m, ua, ub, va, vb, y0, y1, c, f, o) => { const a = P(Math.min(ua, ub), Math.min(va, vb), y0), b = P(Math.max(ua, ub), Math.max(va, vb), y1); m.box(a[0], y0, a[2], b[0], y1, b[2], c, f, o); };
    const sol = (ua, ub, va, vb, y0, y1) => { const a = P(Math.min(ua, ub), Math.min(va, vb), y0), b = P(Math.max(ua, ub), Math.max(va, vb), y1); solid(Math.min(a[0], b[0]), y0, Math.min(a[2], b[2]), Math.max(a[0], b[0]), y1, Math.max(a[2], b[2])); };
    const flight = f => { const lane = f % 2 ? lane1 : lane0, dir = (f % 2 ? -1 : 1) * s.up0, ub = dir > 0 ? uLo : uHi, ut = dir > 0 ? uHi : uLo; return { lane, dir, ub, ut }; };
    const g = {
      ax, u0, u1, v0, v1, uLo, uHi, lane0, lane1, spine, P, box, sol, flight,
      footAt(f) { const F = flight(f), v = (F.lane[0] + F.lane[1]) / 2, u = F.ub - F.dir * .6; const p = P(u, v, 0); return [p[0], p[2]]; },   // where you step on to go up from floor f
      hole(f) { if (f < 1) return null; const F = flight(f - 1); return [uLo, uHi, F.lane[0], F.lane[1]]; },                                     // the opening in floor f's slab
    };
    geo.set(l, g); return g;
  }

  // a slab over the whole floor of a building, with a rectangular hole: drawn as the four strips round the hole
  function slab(l, y0, y1, hole, draw) {
    const [x0, z0, x1, z1] = l.rect, t = D.storeys.wall, X0 = x0 + t, Z0 = z0 + t, X1 = x1 - t, Z1 = z1 - t;
    if (!hole) return draw(X0, Z0, X1, Z1);
    const G = stairOf(l), a = G.P(hole[0], hole[2], 0), b = G.P(hole[1], hole[3], 0);
    const hx0 = Math.min(a[0], b[0]), hx1 = Math.max(a[0], b[0]), hz0 = Math.min(a[2], b[2]), hz1 = Math.max(a[2], b[2]);
    for (const r of [[X0, Z0, X1, hz0], [X0, hz1, X1, Z1], [X0, hz0, hx0, hz1], [hx1, hz0, X1, hz1]]) if (r[2] - r[0] > .01 && r[3] - r[1] > .01) draw(...r);
  }

  // ------------------------------------------------------------ the shell, at load: walls from the first floor up, slabs, stairs, lift
  function shell(l, o) {
    const [x0, z0, x1, z1] = l.rect, t = D.storeys.wall, H = o.H, n = l.floors, G = stairOf(l);
    // the upper walls: four slabs, the outside as it was, plaster within (faces: 0 -x, 1 +x, 2 under, 3 top, 4 -z, 5 +z)
    const bands = o.old ? [[GF, OLD_BASE, o.stoneC, o.stoneF], [OLD_BASE, H, o.upC, o.upF]] : [[GF, H, o.upC, o.upF]];
    const inner = [o.inC, 17];
    for (const [y0, y1, c, f] of bands) {
      world.box(x0, y0, z0, x1, y1, z0 + t, c, f, { 5: inner, 2: null, 3: null });
      world.box(x0, y0, z1 - t, x1, y1, z1, c, f, { 4: inner, 2: null, 3: null });
      world.box(x0, y0, z0 + t, x0 + t, y1, z1 - t, c, f, { 1: inner, 4: null, 5: null, 2: null, 3: null });
      world.box(x1 - t, y0, z0 + t, x1, y1, z1 - t, c, f, { 0: inner, 4: null, 5: null, 2: null, 3: null });
    }
    solid(x0, GF, z0, x1, H, z0 + t); solid(x0, GF, z1 - t, x1, H, z1); solid(x0, GF, z0, x0 + t, H, z1); solid(x1 - t, GF, z0, x1, H, z1);
    world.box(x0, H - .2, z0, x1, H, z1, o.capC, 0, { 0: null, 1: null, 2: null, 4: null, 5: null, 3: [o.capC, 0] });   // the roof's underside is drawn from inside (build)
    solid(x0, H - .2, z0, x1, H, z1);
    // floor slabs 1 .. n-1, open over the flight that comes up through them
    for (let f = 1; f < n; f++) slab(l, floorY(f) - .2, floorY(f), G && G.hole(f), (a, b, c, d) => solid(a, floorY(f) - .2, b, c, floorY(f), d));
    if (!G) return;
    // the flights: steps you can climb, solid down to the floor they stand on; the spine wall between the flights; a rail at each hole
    for (let f = 0; f < n - 1; f++) {
      const F = G.flight(f), y0 = floorY(f), rise = floorY(f + 1) - y0, N = Math.ceil(rise / STEP), tr = (G.uHi - G.uLo) / N;
      for (let k = 0; k < N; k++) { const ua = F.ub + F.dir * k * tr, ub = F.ub + F.dir * (k + 1) * tr; G.sol(ua, ub, F.lane[0], F.lane[1], y0, y0 + (k + 1) * rise / N); }
    }
    for (let f = 0; f < n; f++) G.sol(G.uLo, G.uHi, G.spine[0], G.spine[1], floorY(f), ceilY(f));
    for (let f = 1; f < n; f++) { const F = G.flight(f - 1), u = F.ub; G.sol(u - .04, u + .04, F.lane[0], F.lane[1], floorY(f), floorY(f) + 1.0); }
    // the lift: a shaft from the ground to the top floor, a door and a call button on each floor; up and down a floor, or all the way
    if (l.lift) {
      const [bx, bz] = l.lift, ix = bx < (x0 + x1) / 2 ? 1 : -1;
      solid(bx - 1.1, .2, bz - 1.1, bx + 1.1, ceilY(n - 1), bz + 1.1);
      const at = f => [bx + ix * 1.9, floorY(f), bz], to = f => [bx + ix * 2.2, floorY(f), bz + .6];
      for (let f = 0; f < n; f++) {
        const [ux, uy, uz] = at(f);
        if (f < n - 1) use(ux, uy, uz - .35, "lift", f === 0 ? "Take the lift up a floor" : "Lift: up a floor (to " + (f + 1) + ")", 0, { to: to(f + 1) });
        if (f > 0) use(ux, uy, uz + .35, "lift", "Lift: down a floor (to " + (f - 1 || "ground") + ")", 0, { to: to(f - 1) });
        if (f === 0) use(ux, uy, uz + .35, "lift", "Take the lift to the top floor", 0, { to: to(n - 1) });
        if (f === n - 1) { use(ux, uy, uz + .7, "lift", "Lift: down to the ground", 0, { to: to(0) }); use(ux, uy, uz - .7, "lift", "Take the lift to the roof", 0, { to: [bx + ix * 2.2, H, bz + .6] }); }
        if (f > 0 && f < n - 1) use(ux, uy, uz + .7, "lift", "Lift: down to the ground", 0, { to: to(0) });
      }
      // the lift house on the roof, as before
      world.box(bx - 1.1, H, bz - 1.1, bx + 1.1, H + 2.7, bz + 1.1, C.metal, 9); solid(bx - 1.1, H, bz - 1.1, bx + 1.1, H + 2.7, bz + 1.1);
      world.box(bx + ix * 1.1 - .04, H + .1, bz - .7, bx + ix * 1.1 + .04, H + 2.2, bz + .7, C.ochre, 9);
      use(bx + ix * 1.9, H, bz, "lift", "Take the lift down to the top floor", 0, { to: to(n - 1) });
      labels.push({ x: bx, y: 2.9, z: bz, text: "Lift", lot: l, r: 14, indoor: true });
    }
  }

  // ------------------------------------------------------------ one floor's slabs, stair and lift, drawn near the camera
  function build(l, f, m, floorMat) {
    const n = l.floors, G = stairOf(l), y = floorY(f), cy = ceilY(f), ceilC = [.78, .74, .66];
    if (f > 0) slab(l, y - .2, y, G && G.hole(f), (a, b, c, d) => m.box(a, y - .2, b, c, y, d, floorMat[1], floorMat[0], { 2: null, 0: null, 1: null, 4: null, 5: null }));
    slab(l, cy, cy + .2, f + 1 < n && G ? G.hole(f + 1) : null, (a, b, c, d) => m.box(a, cy, b, c, cy + .02, d, ceilC, 18, { 2: [ceilC, 18], 3: null, 0: null, 1: null, 4: null, 5: null }));
    if (G) {
      if (f < n - 1) {                                       // the flight up from this floor, its treads and risers, a handrail on the open side
        const F = G.flight(f), rise = floorY(f + 1) - y, N = Math.ceil(rise / STEP), tr = (G.uHi - G.uLo) / N;
        for (let k = 0; k < N; k++) { const ua = F.ub + F.dir * k * tr, ub = F.ub + F.dir * (k + 1) * tr; G.box(m, ua, ub, F.lane[0], F.lane[1], y, y + (k + 1) * rise / N, k % 2 ? WOOD : [.4, .27, .17], 19, { 2: null }); }   // each step built down to the floor: a solid flight
        G.box(m, F.ub, F.ut, F.lane[0] - .03, F.lane[0] + .03, y, y + .25, RAIL, 19);
      }
      G.box(m, G.uLo, G.uHi, G.spine[0], G.spine[1], y, cy, [.78, .73, .64], 17);   // the spine wall between the flights
      if (f > 0) {                                           // a rail where the floor stops at the hole
        const F = G.flight(f - 1), u = F.ub;
        G.box(m, u - .03, u + .03, F.lane[0], F.lane[1], y + .92, y + 1.0, RAIL, 19);
        for (let v = F.lane[0] + .1; v < F.lane[1]; v += .25) G.box(m, u - .015, u + .015, v - .015, v + .015, y, y + .92, RAIL, 19);
      }
    }
    if (l.lift) {                                            // the shaft's front on this floor: steel doors, a lit button
      const [bx, bz] = l.lift, ix = bx < (l.rect[0] + l.rect[2]) / 2 ? 1 : -1, steel = [.42, .44, .46];
      m.box(bx - 1.1, y, bz - 1.1, bx + 1.1, cy, bz + 1.1, [.62, .6, .56], 17);
      m.box(bx + ix * 1.1 - .03, y + .05, bz - .55, bx + ix * 1.1 + ix * .03, y + 2.2, bz - .01, steel, 19); m.box(bx + ix * 1.1 - .03, y + .05, bz + .01, bx + ix * 1.1 + ix * .03, y + 2.2, bz + .55, steel, 19);
      m.box(bx + ix * 1.12 - .02, y + 1.15, bz + .7, bx + ix * 1.12 + .02, y + 1.3, bz + .8, [1, .8, .5], 2);
      m.box(bx + ix * 1.1 - .03, y + 2.3, bz - .3, bx + ix * 1.1 + ix * .04, y + 2.45, bz + .3, [.1, .1, .1], 19);
    }
  }

  // ------------------------------------------------------------ floors whose use has no layout of its own: filled in rows, clear of the stair
  // (called from furnish(): at load for the collisions, near the camera for the look; B and hard are furnish()'s)
  const GOODS = [[.55, .45, .3], [.5, .4, .26], [.62, .55, .4], [.42, .5, .24], [.6, .2, .16], [.3, .38, .52]];
  function fill(l, f, B, hard, occ) {
    const use = (l.storeys || [])[f], F = floorY(f), [x0, z0, x1, z1] = l.rect, t = D.storeys.wall + .9;
    const item = { store: [1.0, 2.2, 3.0], wards: [1.0, 2.0, 2.6], plant: [1.4, 1.4, 3.2], gallery: [.8, .8, 3.0], stacks: [.45, 2.6, 2.2] }[use];
    if (!item) return;
    const [w, d, gap] = item, alongX = x1 - x0 >= z1 - z0, h = (...k) => hash(l.id, f, "fill", ...k);
    const clear = (x, z, r) => occ.every(([ox, oz, or]) => Math.hypot(x - ox, z - oz) > r + or);
    let k = 0;
    for (let a = (alongX ? x0 : z0) + t + w; a < (alongX ? x1 : z1) - t - w / 2; a += w + gap) for (let b = (alongX ? z0 : x0) + t + d / 2; b < (alongX ? z1 : x1) - t - d / 2; b += d + 1.6) {
      const x = alongX ? a : b, z = alongX ? b : a, r = Math.max(w, d) / 2 + .2; if (!clear(x, z, r)) continue; k++;
      const sx = alongX ? w : d, sz = alongX ? d : w;
      if (use === "store") {                                  // a rack of shelves stacked with boxes and sacks
        for (const y of [0, .62, 1.24, 1.86]) B(x, y, z, sx, .05, sz, [.42, .3, .2]);
        for (const u of [-1, 1]) for (const v of [-1, 1]) B(x + u * (sx / 2 - .03), 0, z + v * (sz / 2 - .03), .05, 2.0, .05, [.3, .22, .15]);
        for (let q = 0; q < 9; q++) { const ly = [0, .62, 1.24][q % 3] + .05, off = (Math.floor(q / 3) - 1) * (alongX ? sz : sx) * .3, bw = .3 + .2 * h(k, q);
          B(x + (alongX ? 0 : off), ly, z + (alongX ? off : 0), Math.min(sx - .1, bw + .1), .25 + .25 * h(q, k), Math.min(sz - .1, bw), pick(GOODS, h(k, q, 2))); }
      } else if (use === "wards") {                           // a hospital bed with a rail, a curtain on one side, a locker
        B(x, 0, z, sx * .9, .5, sz * .9, [.72, .72, .7]); B(x, .5, z, sx * .86, .12, sz * .86, [.9, .9, .88]); B(x, .62, z, sx * .6, .1, sz * .3, [.85, .85, .82]);
        B(x + (alongX ? sx * .62 : 0), 0, z + (alongX ? 0 : sx * .62), alongX ? .04 : sz, 2.0, alongX ? sz : .04, [.62, .72, .7]);
      } else if (use === "plant") {                           // pumps and switch cabinets, a pipe up to the ceiling
        B(x, 0, z, sx, 1.4, sz, [.36, .42, .44]); B(x, 1.4, z, sx * .6, .3, sz * .6, [.3, .32, .34]); B(x + sx * .3, 1.7, z, .14, ceilY(f) - F - 1.7, .14, C.metal);
        B(x - sx * .3, .5, z + sz / 2 + .01, .3, .2, .02, [1, .7, .3], 0, 2);
      } else if (use === "gallery") {                         // a plinth with something on it
        B(x, 0, z, .7, 1.0, .7, [.82, .8, .76]); B(x, 1.0, z, .3 + .2 * h(k), .3 + .5 * h(k, 1), .3, pick([C.terracotta, [.62, .72, .76], [.72, .7, .66]], h(k, 2)));
      } else if (use === "stacks") {                          // a double-sided bookcase
        B(x, 0, z, sx, 2.0, sz, [.36, .24, .15]); for (let q = 0; q < 4; q++) B(x, .1 + q * .48, z, sx + .02, .3, sz + .02, pick(GOODS, h(k, q)));
      }
      hard(x - sx / 2, F, z - sz / 2, x + sx / 2, F + (use === "gallery" ? 1.0 : 1.9), z + sz / 2); occ.push([x, z, r]);
    }
  }

  // ------------------------------------------------------------ what furniture must leave clear on a floor: the stairwell and 0.6 m round
  // it, and on the ground floor the way from the door to the foot of the stair. keepOut(l, f) gives a test of a footprint.
  function keepOut(l, f) {
    const G = stairOf(l); if (!G) return () => false;
    const a = G.P(G.u0, Math.min(G.v0, G.v1), 0), b = G.P(G.u1, Math.max(G.v0, G.v1), 0), e = .6;
    const wx0 = Math.min(a[0], b[0]) - e, wx1 = Math.max(a[0], b[0]) + e, wz0 = Math.min(a[2], b[2]) - e, wz1 = Math.max(a[2], b[2]) + e;
    const segs = [];
    if (f === 0) { const d = DOOR_DIR[l.side] || [0, 0], p0 = [l.door[0] - d[0] * .4, l.door[1] - d[1] * .4], p1 = [l.door[0] - d[0] * 1.6, l.door[1] - d[1] * 1.6], ft = G.footAt(0); segs.push([p0, p1], [p1, ft]); }
    const nearSeg = (x, z, [p, q], r) => { const vx = q[0] - p[0], vz = q[1] - p[1], L2 = vx * vx + vz * vz || 1, t = Math.max(0, Math.min(1, ((x - p[0]) * vx + (z - p[1]) * vz) / L2)); return Math.hypot(x - p[0] - vx * t, z - p[1] - vz * t) < r; };
    return (x0, z0, x1, z1) => {
      if (x1 > wx0 && x0 < wx1 && z1 > wz0 && z0 < wz1) return true;
      const cx = Math.max(x0, Math.min((x0 + x1) / 2, x1)), cz = (z0 + z1) / 2, r = Math.hypot(x1 - x0, z1 - z0) / 2 + .55;
      return segs.some(sg => nearSeg(cx, cz, sg, r));
    };
  }

  return { floorY, ceilY, levelAt, objectsOn, proxy, stairOf, shell, build, fill, keepOut, USE_AS };
})();
