"use strict";
// Crash damage in the slice (LIVING_CITY CITY-072 to CITY-076; F-VEH-CRASH level 2). Rules from World/rules/crash.json
// (window.WASTED.crash), packed by Tools/worldgen/build_play.py.
//
// While the town is built, every street thing that can break (lamps, benches, bins, planters, bollards, post and phone
// boxes, boards, racks, flowerbeds, stalls, crates, café tables, trees) is wrapped by Smash.wrap: its slice of the static
// town mesh, its solids, its light and its seats are noted. A fast hit by the player's car (Cars.drive) breaks it: the
// slice is collapsed on the graphics card, its solids, light and seats switch off, and a copy made from the same
// triangles topples, or bursts into pieces in its own colours that bounce and settle as rubble. Hitting a building
// corner fast chips it. Nearby residents turn and shout (Smash.apply, as CITY-071 does for near misses).
//
// Damage is stored state (LIVING_CITY §1, CITY-074): a list of what broke, where and when, kept in the browser here (on
// the server in the game). When each thing is mended is worked out from that list by the same rule as
// Tools/citysim/repairs.py (CITY-075, proven there by test_repairs): a crew from a depot comes in working hours, works
// a while at the spot, and the thing is whole again. Nothing else is stored; debris and poses are only presented.
//
// Loaded before play.js; touches play.js's globals (world, worldBuf, gl, solids, lights, uses, LOTS, groundBase, clock,
// cam, pl, People, hash, yawOf) only once play.js calls in.
var Smash = (() => {
  let R = (window.WASTED && window.WASTED.crash) || null;
  const KEY = "wasted.damage.v1";
  const list = [], byId = new Map(), grid = new Map(), G = 4;
  const effects = new Map();                             // damage id -> what is drawn for it: a toppled copy, pieces, a chip
  const recent = [];                                     // smashes residents may still be reacting to: {x, z, t}
  let depth = 0, ready = false, damage = [], plan = {}, last = 0, crewLook = null;
  const kindOf = k => R ? (R.kinds[k] ? k : (R.aliases || {})[k] || null) : null;
  const now = () => clock.day * 1440 + clock.m;
  const rnd = (a, b) => { const s = Math.sin(a * 12.9898 + b * 78.233) * 43758.5453; return s - Math.floor(s); };
  const sm = (a, b, v) => { const t = Math.max(0, Math.min(1, (v - a) / (b - a))); return t * t * (3 - 2 * t); };

  // ---- while the town is built ----
  function wrap(kind, x, z, fn) {
    const k = depth === 0 ? kindOf(kind) : null;           // inside another breakable (crates on a stall): part of it
    if (!k) { fn(); return; }
    depth++;
    const s = world.n, so = solids.length, li = lights.length, us = uses.length;
    try { fn(); } finally { depth--; }
    if (world.n <= s) return;
    let id = k + "@" + x.toFixed(1) + "," + z.toFixed(1), n = 2;
    while (byId.has(id)) id = k + "@" + x.toFixed(1) + "," + z.toFixed(1) + "#" + n++;
    const b = { id, kind: k, x, z, s, e: world.n, sol: [so, solids.length], li: [li, lights.length], us: [us, uses.length], broken: false };
    list.push(b); byId.set(id, b);
  }

  // ---- once the town is built, before play.js lets go of the mesh: copy each breakable's triangles ----
  let prepared = false;
  function prepare() {
    prepared = true;
    if (!R) return;
    for (const b of list) {
      const A = world.a, g = groundBase(b.x, b.z);
      let x0 = 1e9, x1 = -1e9, z0 = 1e9, z1 = -1e9, top = g;
      for (let i = b.s; i < b.e; i += 10) top = Math.max(top, A[i + 1]);
      for (let pass = 0; pass < 2 && x0 > x1; pass++)       // its footprint near the ground (a tree's trunk, not its crown)
        for (let i = b.s; i < b.e; i += 10) if (pass || A[i + 1] < g + 1.3) { x0 = Math.min(x0, A[i]); x1 = Math.max(x1, A[i]); z0 = Math.min(z0, A[i + 2]); z1 = Math.max(z1, A[i + 2]); }
      const orig = A.slice(b.s, b.e), gone = orig.slice();
      for (let i = 0; i < gone.length; i += 10) { gone[i] = b.x; gone[i + 1] = -60; gone[i + 2] = b.z; }   // every triangle shrunk to a point under the ground
      Object.assign(b, { g, top, box: [x0, z0, x1, z1], orig, gone });
      for (let a = Math.floor(x0 / G); a <= Math.floor(x1 / G); a++) for (let c = Math.floor(z0 / G); c <= Math.floor(z1 / G); c++) {
        const key = a + "," + c; if (!grid.has(key)) grid.set(key, []); grid.get(key).push(b);
      }
    }
  }

  // ---- on the first frame (the clock is restored by then): the saved damage ----
  function init() {
    ready = true;
    if (!R) return;
    if (!prepared) { console.warn("Smash.prepare() was not called before the town mesh was let go: nothing can break."); R = null; return; }
    try { damage = JSON.parse(localStorage.getItem(KEY) || "[]"); } catch (e) { damage = []; }
    const t = now();
    for (const d of damage) if (d.at > t) d.at = t;      // a new life started since: nothing broke in its future
    replan();
    for (const d of damage) {
      if (plan[d.id] && plan[d.id].done <= t) continue;
      const b = byId.get(d.id);
      if (b) { off(b); effects.set(d.id, rest(b, d)); }
      else if (d.kind === "corner") effects.set(d.id, chipRest(d));
    }
    tidy();
  }

  // ---- the repair rule: the same as Tools/citysim/repairs.py ----
  function replan() {
    const r = R.repair, hm = s => (+s.slice(0, 2)) * 60 + (+s.slice(3)), start = hm(r.start), end = hm(r.end), speed = r.walk_mps * window.WASTED.pace;
    const into = t => { const day = Math.floor(t / 1440), tod = t - day * 1440; return tod < start ? day * 1440 + start : tod >= end ? (day + 1) * 1440 + start : t; };
    const crews = [];
    LOTS.filter(l => l.kind === "utility" && l.enterable).map(l => [l.id, l.door]).sort((a, b) => a[0] < b[0] ? -1 : 1)
      .forEach(([id, door]) => { for (let n = 0; n < r.crews_per_depot; n++) crews.push({ id: id + "/" + n, free: -1e18, pos: door }); });
    plan = {};
    if (!crews.length) return;
    for (const d of [...damage].sort((a, b) => a.at - b.at || (a.id < b.id ? -1 : 1))) {
      const work = (R.kinds[d.kind] || {}).work_min || 30;
      let best = null;
      for (const c of crews) {
        const walk = Math.hypot(c.pos[0] - d.x, c.pos[1] - d.z) * r.detour / speed;
        let leave = into(Math.max(c.free, d.at + r.report_min));
        if (leave % 1440 + walk + work > end) leave = (Math.floor(leave / 1440) + 1) * 1440 + start;
        const done = leave + walk + work;
        if (!best || done < best.done - 1e-9) best = { done, c, leave, walk };
      }
      plan[d.id] = { crew: best.c.id, leave: best.leave, arrive: best.leave + best.walk, done: best.done };
      best.c.free = best.done; best.c.pos = [d.x, d.z];
    }
  }
  function save() { try { localStorage.setItem(KEY, JSON.stringify(damage)); } catch (e) { } }

  // ---- switching a thing off and on ----
  function upload(b, data) { gl.bindBuffer(gl.ARRAY_BUFFER, worldBuf); gl.bufferSubData(gl.ARRAY_BUFFER, b.s * 4, data); }
  function off(b) {
    if (b.broken) return;
    b.broken = true;
    upload(b, b.gone);
    b.solids = []; for (let i = b.sol[0]; i < b.sol[1]; i++) { b.solids.push(solids[i].slice()); solids[i][1] = solids[i][4] = -1e9; }
    b.lights = []; for (let i = b.li[0]; i < b.li[1]; i++) { b.lights.push(lights[i][3]); lights[i][3] = 0; }
    for (let i = b.us[0]; i < b.us[1]; i++) uses[i].y += 1000;
  }
  function on(b) {
    if (!b.broken) return;
    b.broken = false;
    upload(b, b.orig);
    for (let i = b.sol[0]; i < b.sol[1]; i++) solids[i].splice(0, 6, ...b.solids[i - b.sol[0]]);
    for (let i = b.li[0]; i < b.li[1]; i++) lights[i][3] = b.lights[i - b.li[0]];
    for (let i = b.us[0]; i < b.us[1]; i++) uses[i].y -= 1000;
  }

  // ---- what a broken thing looks like ----
  function colours(b, n, seed) {                          // n colours taken from the thing's own triangles
    const A = b.orig, tris = A.length / 30, out = [];
    for (let k = 0; k < n; k++) { const j = Math.floor(rnd(seed, k) * tris) * 30; out.push([A[j + 6], A[j + 7], A[j + 8]]); }
    return out;
  }
  function burst(b, x, y, z, vx, vz, size, cols, n) {     // pieces that fly, bounce and settle
    const P = [];
    for (let k = 0; k < n; k++) {
      const r1 = Math.random(), r2 = Math.random(), r3 = Math.random(), s = size * (.45 + .7 * Math.random());
      P.push({ x: x + (r1 - .5) * size * 2, y: y + r2 * size * 2, z: z + (r3 - .5) * size * 2, vx: vx * (.35 + .5 * r1) + (r2 - .5) * 3, vy: 2 + 4 * r3,
        vz: vz * (.35 + .5 * r2) + (r1 - .5) * 3, s, h: s * (.5 + .6 * r2), yaw: r3 * 6.28, spin: (r1 - .5) * 14, c: cols[k % cols.length], rest: false });
    }
    return P;
  }
  function rubble(b, x, z, size, cols, seed) {            // pieces at rest, the same every time (a damage loaded from the save)
    const P = [], n = R.debris.rest_pieces;
    for (let k = 0; k < n; k++) {
      const a = rnd(seed, k) * 6.28, d = .3 + 1.4 * rnd(seed, k + 50), s = size * (.5 + .6 * rnd(seed, k + 90)), px = x + Math.cos(a) * d, pz = z + Math.sin(a) * d;
      P.push({ x: px, z: pz, y: groundBase(px, pz) + s * .25, s, h: s * .5, yaw: a * 3, c: cols[k % cols.length], rest: true });
    }
    return P;
  }
  function topple(b, d, t0) { return { how: "topple", b, dir: d.dir || [1, 0], t0 }; }
  function rest(b, d) {                                    // the look of damage from a save: fallen, or rubble
    const k = R.kinds[b.kind];
    if (k.how === "topple") return topple(b, d, -1e9);
    const size = Math.max(.12, Math.min(.35, Math.max(b.box[2] - b.box[0], b.box[3] - b.box[1]) / 3));
    return { how: "shatter", P: rubble(b, b.x, b.z, size, colours(b, 6, b.x + b.z), b.x * 3 + b.z) };
  }
  function chipRest(d) {
    const stone = [[.37, .34, .32], [.52, .48, .43], [.3, .28, .27]];
    return { how: "chip", d, P: rubble(null, d.x + d.out[0] * .8, d.z + d.out[1] * .8, .24, stone, d.x + d.z * 7) };
  }

  // ---- the car hitting things (called from cars.js) ----
  function probe(x, y, z) {                               // the breakable standing at this point, or null
    if (!R) return null;
    if (!ready) init();
    const l = grid.get(Math.floor(x / G) + "," + Math.floor(z / G)); if (!l) return null;
    const M = .2;                                        // wider than the .15 m that play.js keeps round every solid, so the thing is met before its solid
    for (const b of l) if (!b.broken && x >= b.box[0] - M && x <= b.box[2] + M && z >= b.box[1] - M && z <= b.box[3] + M && y < b.top + .2) return b;
    return null;
  }
  const breakSpeed = () => R ? R.break_kmh / 3.6 : 1e9;
  function smash(b, vx, vz) {                              // returns how much of its speed the car keeps
    if (b.broken) return 1;
    const sp = Math.hypot(vx, vz) || 1, dir = [vx / sp, vz / sp], d = { id: b.id, kind: b.kind, at: now(), x: b.x, z: b.z, dir };
    off(b);
    const k = R.kinds[b.kind], t = performance.now() / 1000;
    if (k.how === "topple") effects.set(b.id, topple(b, d, t));
    else {
      const size = Math.max(.12, Math.min(.35, Math.max(b.box[2] - b.box[0], b.box[3] - b.box[1]) / 3));
      effects.set(b.id, { how: "shatter", P: burst(b, b.x, b.g + .2, b.z, vx, vz, size, colours(b, 8, t), R.debris.pieces), t0: t });
    }
    damage = damage.filter(o => o.id !== b.id).concat([d]); replan(); save();
    recent.push({ x: b.x, z: b.z, t });
    if (window.Sound && Sound.act) try { Sound.act("crash"); } catch (e) { }
    return k.slow;
  }
  function chip(x, z, vx, vz) {                            // a fast hit on a building wall: chip the corner if it is one
    if (!R) return;
    if (!ready) init();
    let best = null;
    for (const l of LOTS) {
      if (l.kind === "garden" || l.kind === "market" || l.kind === "stop") continue;
      const [a0, c0, a1, c1] = l.rect;
      if (x < a0 - 2 || x > a1 + 2 || z < c0 - 2 || z > c1 + 2) continue;
      [[a0, c0], [a1, c0], [a1, c1], [a0, c1]].forEach((p, ci) => { const dd = Math.hypot(p[0] - x, p[1] - z); if (dd < R.corner_m && (!best || dd < best.dd)) best = { l, p, ci, dd }; });
    }
    if (!best) return;
    const id = "corner@" + best.l.id + ":" + best.ci;
    if (damage.some(o => o.id === id)) return;
    const [a0, c0, a1, c1] = best.l.rect, out = [Math.sign(best.p[0] - (a0 + a1) / 2), Math.sign(best.p[1] - (c0 + c1) / 2)];
    const d = { id, kind: "corner", at: now(), x: best.p[0], z: best.p[1], out, dir: [vx, vz] };
    damage = damage.concat([d]); replan(); save();
    const t = performance.now() / 1000, stone = [[.37, .34, .32], [.52, .48, .43], [.3, .28, .27]];
    const e = chipRest(d); e.P = burst(null, d.x + out[0] * .3, groundBase(d.x, d.z) + .7, d.z + out[1] * .3, -vx * .3 + out[0] * 3, -vz * .3 + out[1] * 3, .14, stone, 10); e.t0 = t;
    effects.set(id, e);
    recent.push({ x: d.x, z: d.z, t });
    if (window.Sound && Sound.act) try { Sound.act("crash"); } catch (e2) { }
  }

  // ---- drawing ----
  function emit(m, b, rot) {                               // the thing's own triangles, turned about its foot
    const A = b.orig, n = A.length;
    if (m.n + n > m.a.length) { const nb = new Float32Array(Math.max(m.a.length * 2, m.n + n + 60)); nb.set(m.a); m.a = nb; }
    const O = m.a; let o = m.n;
    for (let i = 0; i < n; i += 10) {
      const p = rot(A[i] - b.x, A[i + 1] - b.g, A[i + 2] - b.z), q = rot(A[i + 3], A[i + 4], A[i + 5]);
      O[o] = b.x + p[0]; O[o + 1] = b.g + p[1]; O[o + 2] = b.z + p[2]; O[o + 3] = q[0]; O[o + 4] = q[1]; O[o + 5] = q[2];
      O[o + 6] = A[i + 6]; O[o + 7] = A[i + 7]; O[o + 8] = A[i + 8]; O[o + 9] = A[i + 9]; o += 10;
    }
    m.n = o;
  }
  function rodrigues(k, th) {                              // rotation by th about the horizontal axis k
    const c = Math.cos(th), s = Math.sin(th);
    return (x, y, z) => { const kx = k[0], kz = k[2], dot = kx * x + kz * z, cx = -kz * y, cy = kz * x - kx * z, cz = kx * y;   // k x v with k = (kx, 0, kz)
      return [x * c + cx * s + kx * dot * (1 - c), y * c + cy * s, z * c + cz * s + kz * dot * (1 - c)]; };
  }
  function pieces(m, e, dt) {
    for (const p of e.P) {
      if (!p.rest) {
        p.vy -= R.debris.gravity * dt; p.x += p.vx * dt; p.y += p.vy * dt; p.z += p.vz * dt; p.yaw += p.spin * dt;
        const g = groundBase(p.x, p.z) + p.h / 2;
        if (p.y < g) { p.y = g; p.vy = -p.vy * R.debris.bounce; p.vx *= .6; p.vz *= .6; p.spin *= .6; if (Math.abs(p.vy) < .6 && Math.hypot(p.vx, p.vz) < .4) p.rest = true; }
      }
      m.ybox(p.x, p.y - p.h / 2, p.z, p.s, p.h, p.s * .8, p.yaw, p.c, 0);
    }
    if (e.t0 && performance.now() / 1000 - e.t0 > R.debris.life_s) { e.P = e.P.slice(0, R.debris.rest_pieces); for (const p of e.P) { p.rest = true; p.y = groundBase(p.x, p.z) + p.h / 2; } e.t0 = 0; }
  }
  function worker(m, x, z, toward, t, k) {                // a crew member at work: crouched over it, arms busy
    if (!crewLook) crewLook = [0, 1].map(i => People.look(310000 + i * 17, "utility", (window.WASTED.role_sig && window.WASTED.role_sig.utility) || "Easy"));
    const L = crewLook[k % 2], y = groundBase(x, z), w = Math.sin(t * 5 + k) * 20;
    People.draw(m, x, y, z, yawOf(toward[0] - x, toward[1] - z), L, 0, { ph: 0, t, lod: 1, fl: 14, mod: { arms: [[1, 60 + w, 20, 70], [1, 60 - w, 20, 70]], chest: [24, 0] } });
  }

  function draw(m, tags, tSec) {
    if (!ready) init();
    if (!R) return 0;
    const t = now(), dt = Math.min(.05, Math.max(0, tSec - last)); last = tSec;
    let fixed = false;
    for (const d of damage) if (plan[d.id] && plan[d.id].done <= t) {   // mended
      const b = byId.get(d.id); if (b) on(b);
      effects.delete(d.id); fixed = true;
    }
    if (fixed) { damage = damage.filter(d => !(plan[d.id] && plan[d.id].done <= t)); replan(); save(); }
    let n = 0;
    for (const d of damage) {
      const e = effects.get(d.id); if (!e) continue;
      if (Math.hypot(d.x - cam.eye[0], d.z - cam.eye[2]) > 140) continue;
      n++;
      if (e.how === "topple") {
        const T = R.topple_s, u = e.t0 < 0 ? 1 : Math.min(1, (tSec - e.t0) / T), fall = Math.PI / 2 - .06;
        const th = fall * u * u - (u >= 1 && e.t0 > 0 ? .07 * Math.abs(Math.sin((tSec - e.t0 - T) * 9)) * Math.exp(-(tSec - e.t0 - T) * 4) : 0);
        const dir = e.dir, k = [dir[1], 0, -dir[0]];
        emit(m, e.b, rodrigues(k, th));
      } else if (e.how === "shatter") pieces(m, e, dt);
      else if (e.how === "chip") {                       // the corner's rough core showing, and stones at its foot
        const dd = e.d, g = groundBase(dd.x, dd.z), core = [.19, .17, .16], broken = [.6, .56, .5], ox = dd.out[0], oz = dd.out[1];
        for (let i = 0; i < 5; i++) {                      // the rough core where stones broke away, standing just proud of both faces
          const y0 = .15 + i * .26 + rnd(dd.x, i) * .08, s = (.34 + rnd(dd.z, i) * .22) * (1 - i * .12), h = .22 + rnd(dd.x + 1, i) * .12;
          m.box(dd.x - s * .5 + ox * s * .5, g + y0, dd.z - s * .5 + oz * s * .5, dd.x + s * .5 + ox * s * .5 + ox * .06, g + y0 + h, dd.z + s * .5 + oz * s * .5 + oz * .06, core, 0);
          if (i % 2 === 0) m.ybox(dd.x + ox * (s * .5 + .08), g + y0 + h, dd.z + oz * (s * .5 + .08), .14, .08, .1, rnd(dd.z, i) * 6, broken, 0);   // a broken edge stone
        }
        pieces(m, e, dt);
      }
      const p = plan[d.id];
      if (p && t >= p.arrive && t < p.done) worker(m, d.x + (d.out ? d.out[0] : 1) * 1.1, d.z + (d.out ? d.out[1] : .3) * 1.1, [d.x, d.z], tSec, d.at | 0);
      const pd = Math.hypot(d.x - pl.x, d.z - pl.z);
      if (tags && p && pd < 12) {
        const hh = q => String(Math.floor((q % 1440) / 60)).padStart(2, "0") + ":" + String(Math.floor(q % 60)).padStart(2, "0");
        const text = t >= p.arrive ? "The repair crew is at work · done at " + hh(p.done) : "Broken · the repair crew comes at " + hh(p.arrive) + (Math.floor(p.arrive / 1440) > Math.floor(t / 1440) ? " tomorrow" : "");
        tags.push({ x: d.x, y: groundBase(d.x, d.z) + 1.4, z: d.z, d: pd, a: Math.max(0, Math.min(1, (12 - pd) / 3)), size: 15, text });
      }
    }
    tidy();
    return n;
  }
  function tidy() { const t = performance.now() / 1000; while (recent.length && t - recent[0].t > R.react_s) recent.shift(); }

  // ---- residents who saw it (CITY-076) ----
  function apply(i, r) {
    if (!R || !r || !recent.length || r.inside || r.pose === 2 || r.pose === 3 || r.dodge) return r;
    const t = performance.now() / 1000;
    let s = null;
    for (const e of recent) if (Math.hypot(e.x - r.x, e.z - r.z) < R.react_m && t - e.t < R.react_s) s = e;
    if (!s) return r;
    const f = (t - s.t) / R.react_s, k = sm(0, .1, f) * (1 - sm(.85, 1, f)), shake = Math.sin(t * 15 + i) * 9 * k;
    const line = R.shouts[Math.floor(rnd(i, Math.floor(s.t)) * R.shouts.length)];
    return Object.assign({}, r, { pose: 0, walking: false, yaw: yawOf(s.x - r.x, s.z - r.z),
      mod: { arms: [[k, 140 + shake, 38, 25], [k, 140 - shake, 38, 25]], chest: [-5 * k, 0] }, shout: f < .85 ? line : null });
  }

  function clear() { if (!ready) init(); damage = []; save(); for (const b of list) on(b); effects.clear(); replan(); }   // for testing: everything whole

  return { wrap, prepare, probe, smash, chip, draw, apply, breakSpeed, clear, on: () => !!R, count: () => list.length,
    damage: () => damage.map(d => Object.assign({}, d, plan[d.id] || {})) };
})();
