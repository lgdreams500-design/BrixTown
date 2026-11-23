/* The row houses and the court flats furnished from their approved plans (ADR-0034; FURN-060 to FURN-063), as houses.js
   furnishes the designed houses. Each planned home names its plan (World/rules/home_plans.json) in `interior` and how the
   plan lies in it in `interior_frame` (Tools/worldgen/add_homes_01.py): a row house on its lot, a flat on its entry in
   lot.flats. The building's outside, its outer walls, floors, stair (storeys.js; a row house's is the plan's straight stair)
   and a flat's own walls (Storeys.partitions) stay as they are; this draws and makes solid what is inside: the plan's
   partitions with their doorways, each room's floor, and the plan's pieces at the household's furnishing level (FURN-063,
   furnishing.js), with a bed for every resident whatever the level. Pieces taller than the sill in front of one of the
   building's real windows (play.js windows(): l.inWin, l.inWinUp), and wall pieces hung over one, are left out.
   play.js furnish() hands a planned home's floor here: Homes.floor(l, f, mesh) for a row house, Homes.flats(l, f, mesh) for
   the planned flats on floor f of a court (and Homes.others() for the places that are not theirs). With no mesh it makes
   the solids, the places residents use (spotAt) and the player's uses (useAt); with a mesh it draws. Runs inside play.js's
   world; load it before play.js. */
"use strict";
window.Homes = (() => {
  const PLANS = (window.WASTED || {}).home_plans || {}, SPOTS = ((PLANS.house_12x9 || {}).house || {}).spots || {};
  const BED_KINDS = new Set(["k17:bed", "c_bunk", "r_single"]), SEAT_KINDS = new Set(["k17:sofa", "k17:dining", "k17:armchair"]);
  const PART = .12, DOOR_H = 2.15, DH = .45, SILL_TALL = 1.05;   // the plans' own rule: nothing over sill + 0.15 m in front of a window
  // pieces hung on a wall or from the ceiling: their box in the piece's frame [x0, x1, y0, y1, z0, z1], for the camera only (as houses.js)
  const CAM_ONLY = { k_wall: [-.6, .6, -.24, .17, 1.35, 2.21], d_pendant: [-.3, .3, -.3, .3, 2.0, 3.0], h_hooks: [-.61, .57, -.28, .08, 1.5, 1.91],
                     h_mirror: [-.36, .36, -.14, .05, 1.0, 2.12], b_mirror: [-.3, .3, -.14, .08, 1.1, 2.01], o_painting: [-.51, .51, -.02, .05, 1.2, 1.94],
                     s_pinboard: [-.55, .55, -.04, .03, .9, 1.85], o_clock: [-.21, .21, -.07, .03, 1.6, 2.21], o_wallshelf: [-.5, .5, -.21, .01, 1.3, 1.72] };
  const PLASTER = [.8, .76, .68], FRAME = [.36, .24, .15], BOARDS = [.42, .29, .18], TILES = [.7, .68, .64];
  const mapA = (x, o, n) => { if (x <= o[0]) return n[0] + x - o[0]; for (let i = 0; i + 1 < o.length; i++) if (x <= o[i + 1]) return x - o[i] <= o[i + 1] - x ? n[i] + x - o[i] : n[i + 1] - (o[i + 1] - x); return n[n.length - 1] + x - o[o.length - 1]; };
  const cache = new Map(), stats = { homes: 0, dropped: 0, droppedKinds: {} };

  // ---------------------------------------------------------------- a planned home: its plan, frame and pieces
  function home(l, q) {
    const key = q || l; if (cache.has(key)) return cache.get(key);
    const src = q || l, plan = PLANS[src.interior], Fr = src.interior_frame;
    if (!plan || !Fr) { cache.set(key, null); return null; }
    const P = plan.house, mu = x => mapA(x, Fr.u[0], Fr.u[1]), mv = y => mapA(y, Fr.v[0], Fr.v[1]);
    const W = (x, y) => [Fr.o[0] + x * Fr.x[0] + y * Fr.y[0], Fr.o[1] + x * Fr.x[1] + y * Fr.y[1]];   // plan (in the slice's walls) to world x, z
    const Dv = (px, py) => [px * Fr.x[0] + py * Fr.y[0], px * Fr.x[1] + py * Fr.y[1]];                  // a direction
    const floorOf = storey => q ? q.floor : P.floors.indexOf(storey);
    const items = plan.items.map((it, i) => ({ i, f: floorOf(it.storey), kind: it.piece, x: mu(it.x), y: mv(it.y), th: it.yaw, lift: it.lift || 0, level: it.level, it }));
    const order = q ? l.flats.slice().sort((a, b) => a.floor - b.floor || l.flats.indexOf(a) - l.flats.indexOf(b)) : null;   // the lot's units in floor order
    const H = { l, q, id: q ? q.id : l.id, plan, P, mu, mv, W, Dv, floorOf, items, line: src.furniture,
                beds: q ? q.beds.length : ((l.objects || {}).bed || []).length, flatIx: q ? order.indexOf(q) : null };
    H.drop = dropped(H);
    cache.set(key, H); stats.homes++;
    return H;
  }
  // a piece's footprint in the world: [x0, z0, x1, z1] of its box [a0, a1, b0, b1] in its own frame
  function foot(H, it, b) {
    const c = Math.cos(it.th), s = Math.sin(it.th), xs = [], zs = [];
    for (const [a, bb] of [[b[0], b[2]], [b[1], b[2]], [b[1], b[3]], [b[0], b[3]]]) { const p = H.W(it.x + a * c - bb * s, it.y + a * s + bb * c); xs.push(p[0]); zs.push(p[1]); }
    return [Math.min(...xs), Math.min(...zs), Math.max(...xs), Math.max(...zs)];
  }
  // the pieces that would stand in front of, or hang over, one of the building's real windows (the plan's checked windows are
  // its own; the slice's outside has its windows from the building's design)
  function dropped(H) {
    const l = H.l, [x0, z0, x1, z1] = l.rect, t = .35, out = new Set();
    const faces = { n: z0 + t, s: z1 - t, w: x0 + t, e: x1 - t };
    for (const it of H.items) {
      const sz = Furniture.SIZE[it.kind], cam = CAM_ONLY[it.kind], tall = sz && sz[2] > SILL_TALL && it.kind !== "b_toilet";
      if (!(tall || (cam && it.kind !== "d_pendant")) || BED_KINDS.has(it.kind)) continue;   // a bed stays: every resident has one
      const fp = foot(H, it, sz ? [-sz[0] / 2, sz[0] / 2, -sz[1] / 2, sz[1] / 2] : cam);
      const wins = it.f === 0 ? (l.inWin || []) : (l.inWinUp || []).filter(w => w[3] === it.f);
      for (const [s, a0, a1] of wins) {
        const ns = s === "n" || s === "s", near = ns ? Math.min(Math.abs(fp[1] - faces[s]), Math.abs(fp[3] - faces[s])) : Math.min(Math.abs(fp[0] - faces[s]), Math.abs(fp[2] - faces[s]));
        const p0 = ns ? fp[0] : fp[1], p1 = ns ? fp[2] : fp[3];
        if (near < .35 && p1 > a0 + .3 && p0 < a1 - .3) { out.add(it.i); stats.dropped++; stats.droppedKinds[it.kind] = (stats.droppedKinds[it.kind] || 0) + 1; break; }
      }
    }
    return out;
  }
  function level(H) {                                        // the household's furnishing level today (FURN-063)
    if (window.Bench && Bench.level && Bench.lot === H.l.id) return Bench.level;
    if (!window.Furnishing) return 4;
    if (H.q && Furnishing.unitOf(H.l.id, H.flatIx) !== null) return Furnishing.level(H.l.id, H.flatIx);
    return Furnishing.level(H.l.id, null);                   // a court that is one unit in the market: its flats share its level
  }
  function pieces(H, f) {                                     // what stands on floor f: its level and below, a bed for each resident
    const L = level(H); let need = H.beds; const keep = new Set();
    for (const it of H.items) if (BED_KINDS.has(it.kind) && need > 0) { keep.add(it); need -= (SPOTS[it.kind] || []).length; }
    return H.items.filter(it => it.f === f && !H.drop.has(it.i) && (it.level <= L || keep.has(it)));
  }
  // where people lie and sit on floor f: [x, z, height above the floor, yaw, "lie" | "sit"]
  function spots(H, f) {
    const out = [];
    for (const it of pieces(H, f)) {
      const lie = BED_KINDS.has(it.kind); if (!lie && !SEAT_KINDS.has(it.kind)) continue;
      const c = Math.cos(it.th), s = Math.sin(it.th), it0 = it.it;
      for (const [x, y, z, fx, fy] of SPOTS[it.kind]) {
        const p = H.W(H.mu(it0.x + x * c - y * s), H.mv(it0.y + x * s + y * c)), d = H.Dv(fx * c - fy * s, fx * s + fy * c);
        out.push([p[0], p[1], z, lie ? yawOf(-d[0], -d[1]) : yawOf(d[0], d[1]), lie ? "lie" : "sit"]);
      }
    }
    return out;
  }
  // the plan's partitions on floor f, cut at their doorways: [x0, z0, x1, z1, door] in the world
  function walls(H, f) {
    const out = [];
    for (const [storey, list] of Object.entries(H.P.partitions)) {
      if (H.floorOf(storey) !== f) continue;
      const doors = H.P.doors[storey] || [];
      for (const [ax, at, a0, a1] of list) {
        const cuts = doors.filter(d => d[0] === ax && Math.abs(d[1] - at) < 1e-3 && d[2] > a0 && d[2] < a1).map(d => d[2]).sort((p, q) => p - q);
        const seg = (p, q, door) => {
          if (q - p < .02) return;
          const A = ax === "x" ? H.W(H.mu(at) - PART / 2, H.mv(p)) : H.W(H.mu(p), H.mv(at) - PART / 2), B = ax === "x" ? H.W(H.mu(at) + PART / 2, H.mv(q)) : H.W(H.mu(q), H.mv(at) + PART / 2);
          out.push([Math.min(A[0], B[0]), Math.min(A[1], B[1]), Math.max(A[0], B[0]), Math.max(A[1], B[1]), door]);
        };
        let a = a0;
        for (const c of cuts) { seg(a, c - DH, false); seg(c - DH, c + DH, true); a = c + DH; }
        seg(a, a1, false);
      }
    }
    return out;
  }
  function rooms(H, f) {                                      // [name, x0, z0, x1, z1] in the world
    const out = [];
    for (const [storey, rs] of Object.entries(H.P.rooms)) if (H.floorOf(storey) === f)
      for (const [name, r] of Object.entries(rs)) { const a = H.W(H.mu(r[0]), H.mv(r[2])), b = H.W(H.mu(r[1]), H.mv(r[3])); out.push([name, Math.min(a[0], b[0]), Math.min(a[1], b[1]), Math.max(a[0], b[0]), Math.max(a[1], b[1])]); }
    return out;
  }
  const without = (r, h) => {                                 // a rectangle less a hole, as up to four strips
    if (!h || h[2] <= r[0] || h[0] >= r[2] || h[3] <= r[1] || h[1] >= r[3]) return [r];
    return [[r[0], r[1], r[2], h[1]], [r[0], h[3], r[2], r[3]], [r[0], Math.max(r[1], h[1]), h[0], Math.min(r[3], h[3])], [h[2], Math.max(r[1], h[1]), r[2], Math.min(r[3], h[3])]].filter(q => q[2] - q[0] > .01 && q[3] - q[1] > .01);
  };

  // ---------------------------------------------------------------- one home's floor f: at load (no mesh) or drawn (mesh m)
  function one(H, f, m) {
    const l = H.l, F = Storeys.floorY(f), CY = Storeys.ceilY(f), here = pieces(H, f), rec = m && ROOM_REC;
    if (!m) {
      for (const [a, b, c, d, door] of walls(H, f)) solid(a, door ? F + DOOR_H : F, b, c, CY, d);
      for (const it of here) {
        const sz = Furniture.SIZE[it.kind];
        if (sz && !it.lift) { const [a, b, c, d] = foot(H, it, [-sz[0] / 2, sz[0] / 2, -sz[1] / 2, sz[1] / 2]); solid(a + .05, F, b + .05, c - .05, F + Math.max(sz[2], .9), d - .05); }
        const cb = CAM_ONLY[it.kind];
        if (cb) { const [a, b, c, d] = foot(H, it, cb); solid(a, F + cb[4], b, c, F + cb[5], d); solids[solids.length - 1].cam = true; }
      }
      // the residents' places on this floor (lot.objects, add_homes_01.py) sit on the nearest spot of the plan's pieces
      const S = spots(H, f); if (!S.length) return;
      const pts = H.q ? [...H.q.beds.map(i => ["bed", i]), ...H.q.rooms.map(i => ["room", i])].filter(([k, i]) => (l.levels || {})[k] ? l.levels[k][i] === f : f === 0).map(([k, i]) => [k, l.objects[k][i]])
                      : ["bed", "room"].flatMap(k => (l.objects[k] || []).filter((_, i) => ((l.levels || {})[k] || [])[i] === f || (!(l.levels || {})[k] && f === 0)).map(p => [k, p]));
      for (const [k, [x, z]] of pts) {
        const lie = k === "bed", s = S.filter(p => (p[4] === "lie") === lie).reduce((b, p) => !b || Math.hypot(p[0] - x, p[1] - z) < Math.hypot(b[0] - x, b[1] - z) ? p : b, null);
        if (!s) continue;
        spotAt(l, x, z, lie ? 3 : 2, s[3], F + s[2]); useAt(x, F + s[2], z, lie ? "lie" : "sit", lie ? "Lie down" : "Sit down", s[3]);
      }
      return;
    }
    // floors: boards, tiles in the wet rooms; on a row house's upper floors the landing goes round the stair well
    const G = !H.q && Storeys.stairOf(l), hole = G && f > 0 ? G.hole(f) : null;
    let hr = null; if (hole) { const a = G.P(hole[0], hole[2], 0), b = G.P(hole[1], hole[3], 0); hr = [Math.min(a[0], b[0]), Math.min(a[2], b[2]), Math.max(a[0], b[0]), Math.max(a[2], b[2])]; }
    for (const [name, a, b, c, d] of rooms(H, f)) {
      const wet = /Kitchen|Bathroom|WC/.test(name);
      for (const r of without([a, b, c, d], hr)) m.flat(r[0], r[1], r[2], r[3], F + .006, wet ? TILES : BOARDS, wet ? 16 : 15);
      if (rec) rec.lamps.push([(a + c) / 2, CY - .4, (b + d) / 2, 1.1]);
    }
    // the partitions, plastered, with their doorways' frames
    for (const [a, b, c, d, door] of walls(H, f)) {
      if (!door) { m.box(a, F, b, c, CY, d, PLASTER, 17); continue; }
      m.box(a, F + DOOR_H, b, c, CY, d, PLASTER, 17);
      const alongX = c - a > d - b;
      for (const e of alongX ? [a, c] : [b, d]) alongX ? m.box(e - .05, F, b - .02, e + .05, F + DOOR_H + .08, d + .02, FRAME, 19) : m.box(a - .02, F, e - .05, c + .02, F + DOOR_H + .08, e + .05, FRAME, 19);
    }
    // the pieces, in the home's line (furniture.js); the plan is mirrored where the home is, so are its pieces
    const Lf = (window.Outlet && (Outlet.LINES[H.line] || Outlet.LINES.oldtown)) || null;
    for (const it of here) {
      const c = Math.cos(it.th), s = Math.sin(it.th), y0 = F + it.lift;
      const Q = (a, yy, b, sa, sy, sb, col, fm, yaw) => {
        const ph = it.th + (yaw || 0), p = H.W(it.x + a * c - b * s, it.y + a * s + b * c), ex = H.Dv(Math.cos(ph), Math.sin(ph)), ez = H.Dv(-Math.sin(ph), Math.cos(ph));
        m.obox([ex[0], 0, ex[1], 0, 0, 1, 0, 0, ez[0], 0, ez[1], 0, p[0], y0 + yy, p[1], 1], 0, sy / 2, 0, sa, sy, sb, col, fm || 19);
      };
      Furniture.draw(it.kind, Q, Lf, hash(H.id, "furn", it.i));
      const sz = Furniture.SIZE[it.kind];
      if (rec && sz && it.lift < 1.1) { const [a, b, cc, d] = foot(H, it, [-sz[0] / 2, sz[0] / 2, -sz[1] / 2, sz[1] / 2]); rec.foot.push([(a + cc) / 2, (b + d) / 2, (cc - a) / 2, (d - b) / 2, Math.min(1, 1 - it.lift / 1.4)]); }
    }
  }

  // ---------------------------------------------------------------- what play.js calls
  const has = l => !!(l && l.interior && l.interior_frame && PLANS[l.interior]);
  function floor(l, f, m) { const H = home(l); if (H) one(H, f, m); }
  const planned = (l, f) => (l.flats || []).filter(q => q.floor === f && q.interior && PLANS[q.interior]).map(q => home(l, q)).filter(Boolean);
  function flats(l, f, m) { for (const H of planned(l, f)) one(H, f, m); }
  function others(l, f, objs) {                               // floor f's places that are not in a planned flat (those are furnished here)
    const mine = new Set(); for (const H of planned(l, f)) { for (const i of H.q.beds) mine.add("bed:" + l.objects.bed[i]); for (const i of H.q.rooms) mine.add("room:" + l.objects.room[i]); }
    if (!mine.size) return objs;
    const out = Object.assign({}, objs); for (const k of ["bed", "room"]) if (out[k]) out[k] = out[k].filter(p => !mine.has(k + ":" + p));
    return out;
  }
  // the home the player stands in, for the cutaway (play.js cutState): its world rect and the way in from its door side
  function at(l, x, z, f) {
    if (has(l)) { const H = home(l); return H && { H, rect: l.rect, front: [-H.l.interior_frame.y[0], -H.l.interior_frame.y[1]] }; }
    for (const H of planned(l, f)) { const r = H.q.rect; if (x > r[0] && x < r[2] && z > r[1] && z < r[3]) return { H, rect: r, front: [-H.q.interior_frame.y[0], -H.q.interior_frame.y[1]] }; }
    return null;
  }
  return { has, floor, flats, others, at, home, pieces, spots, stats, CAM_ONLY };
})();
