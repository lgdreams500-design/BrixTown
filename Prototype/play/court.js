"use strict";
// Eastgate Mall's inside (WORLD_MAP WLD-058 to WLD-061, ADR-0038; the owner, 2026-11-24: "start interiors fresh as this is new
// design"). What is where is data: the lot's `plan` (concept "court") in World/city_01/blocks/b_16_5.json, made by
// Tools/worldgen/mall_inside_01.py. This file only draws it and makes it walkable, in the shell's language and the town's
// materials (ART_DIRECTION 2.2 and 2.6: matte, chunky and soft, the muted palette, lanterns and lit windows):
//
//   the hall      its own wall, open at an entrance under each arch of the shell (shell.js); under the arch a porch of ribbon
//                 walls sweeping in to an arched opening, so the hall's wall is not seen from outside
//   floors        everything the plan's walks cover, less its voids, in 2 m squares; a smooth strip along each void's edge.
//                 Upper floors are slabs you stand on; under each is the ceiling of the floor below
//   parapets      a plaster ribbon along every edge where a floor stops, with a line of light under it
//   shop fronts   every one an outlet: its front, its room and what is sold in it are outlets.js's; where its staff are in
//                 today there is a counter
//   the cascades  bridges across the court and moving stairs (walked up as low steps) from the court floor to each
//   egg halls     a round drum under each egg, open into it; from inside an egg shows the sky through its openings
//   the court     a fountain, gardens with trees, benches, kiosks, hanging lanterns
//
// Everything indoors is drawn in room materials (3 plaster, 16 tiles, 19 plain), so it is lit as a room is; trees and
// benches are this file's own for that reason.
const Court = (() => {
  const T = .5, G = 2, DOOR_H = 5.2, RAIL = 1.1, STAIR_V = .7;   // STAIR_V: how fast the moving stairs run, metres a second along their slope (the shader's material 28 moves the treads as fast)
  const moving = [];                                        // the moving stairs, for carry()
  const lamps = [];                                         // every lantern and shop light [x, y, z, strength], for the pools of light the room light lays under the nearest of them
  const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
  const mul = (c, k) => [c[0] * k, c[1] * k, c[2] * k];
  const inPoly = (p, x, z) => { let h = false; for (let i = 0, j = p.length - 1; i < p.length; j = i++) { const a = p[i], b = p[j]; if ((a[1] > z) !== (b[1] > z) && x < a[0] + (z - a[1]) * (b[0] - a[0]) / (b[1] - a[1])) h = !h; } return h; };
  const edgeDist = (p, x, z) => { let d = 1e9; for (let i = 0, j = p.length - 1; i < p.length; j = i++) { const a = p[j], b = p[i], ex = b[0] - a[0], ez = b[1] - a[1], t = clamp(((x - a[0]) * ex + (z - a[1]) * ez) / (ex * ex + ez * ez || 1), 0, 1); d = Math.min(d, Math.hypot(x - a[0] - ex * t, z - a[1] - ez * t)); } return d; };
  const boxOf = p => { let a = 1e9, b = 1e9, c = -1e9, d = -1e9; for (const q of p) { a = Math.min(a, q[0]); b = Math.min(b, q[1]); c = Math.max(c, q[0]); d = Math.max(d, q[1]); } return [a, b, c, d]; };
  const area = p => { let s = 0; for (let i = 0; i < p.length; i++) { const a = p[i], b = p[(i + 1) % p.length]; s += a[0] * b[1] - b[0] * a[1]; } return s / 2; };
  function outward(p) {                                     // at each corner of a closed line, the way out of it
    const n = p.length, sg = area(p) > 0 ? 1 : -1, out = [];
    for (let i = 0; i < n; i++) { const a = p[(i + n - 1) % n], b = p[(i + 1) % n], dx = b[0] - a[0], dz = b[1] - a[1], L = Math.hypot(dx, dz) || 1; out.push([dz / L * sg, -dx / L * sg]); }
    return out;
  }
  function region(P, k) {                                   // what is floor on level k
    const L = P.levels[k], walks = L.walks.map(p => [p, boxOf(p)]), voids = L.voids.map(p => [p, boxOf(p)]), halls = P.halls;
    const walk = (x, z) => walks.some(([p, b]) => x > b[0] && x < b[2] && z > b[1] && z < b[3] && inPoly(p, x, z));
    const hole = (x, z, r) => voids.some(([p, b]) => x > b[0] - r && x < b[2] + r && z > b[1] - r && z < b[3] + r && (inPoly(p, x, z) || (r > 0 && edgeDist(p, x, z) < r)));
    const inHall = (x, z, r) => halls.some(h => Math.hypot(x - h.pos[0], z - h.pos[1]) < h.r + r);
    const cell = (x, z) => !hole(x, z, 1.4) && !(k && inHall(x, z, 1.0)) && (walk(x, z) || (!(k && inHall(x, z, .5)) && (walk(x - 1, z - 1) || walk(x + 1, z - 1) || walk(x - 1, z + 1) || walk(x + 1, z + 1))));
    return { walk, hole, cell, on: (x, z) => walk(x, z) && !hole(x, z, 0) };
  }
  function raster(rect, cell, emit) {                       // runs of 2 m squares along x, row by row
    const [x0, z0, x1, z1] = rect;
    for (let z = z0; z < z1 - .01; z += G) { let run = null; for (let x = x0; x <= x1 + .01; x += G) { const on = x < x1 - .01 && cell(x + G / 2, z + G / 2); if (on && run === null) run = x; if (!on && run !== null) { for (let a = run, e = Math.min(x, x1); a < e - .01; a += 6) emit(a, z, Math.min(a + 6, e), Math.min(z + G, z1)); run = null; } } }   // in pieces of 6 m at most: across a long thin face the plaster's pattern slips, and each row shows as a step
  }
  function frame(a, b, into) {                              // a wall from a to b that faces `into` (a point, or a test of where the walk is)
    const L = Math.hypot(b[0] - a[0], b[1] - a[1]) || 1, t = [(b[0] - a[0]) / L, (b[1] - a[1]) / L], mx = (a[0] + b[0]) / 2, mz = (a[1] + b[1]) / 2; let n = [-t[1], t[0]];
    const ok = typeof into === "function" ? into(mx + n[0] * .8, mz + n[1] * .8) : (into[0] - mx) * n[0] + (into[1] - mz) * n[1] > 0;
    if (!ok) n = [-n[0], -n[1]];
    return { L, t, n, mx, mz, at: (u, y, d) => [a[0] + t[0] * u + n[0] * d, y, a[1] + t[1] * u + n[1] * d] };
  }

  function hall(o, l) {                                     // the hall's own wall: plinth and plaster outside, plaster inside, open at each entrance
    const m = o.mesh, [x0, z0, x1, z1] = l.rect, H = l.H, P = l.plan, st = P.storey_height, F = o.F, t = .3;
    for (const [s, fix, a0, a1, out] of [["n", z0, x0, x1, -1], ["s", z1, x0, x1, 1], ["w", x0, z0, z1, -1], ["e", x1, z0, z1, 1]]) {
      const ns = s === "n" || s === "s", e = P.entrances.find(q => q.side === s), c = e ? e.at[ns ? 0 : 1] : 0, hw = e ? e.width / 2 : 0, arch = (l.shell.arches || []).find(q => q.side === s);
      const at = (u, y, d) => ns ? [u, y, fix + d * out] : [fix + d * out, y, u], dir = d => ns ? [0, 0, d * out] : [d * out, 0, 0];   // d metres out from the wall's line
      const face = (u0, u1, y0, y1, d, col, f, way, uv) => m.quad(at(u0, y0, d), at(u1, y0, d), at(u1, y1, d), at(u0, y1, d), col, f, dir(way), uv);
      const piece = (u0, u1, y0, y1) => {
        if (u1 - u0 < .01) return;
        if (y0 < .6) face(u0, u1, y0, .6, .3, o.C.stone, 6, 1);
        face(u0, u1, Math.max(y0, .6), y1, .2, o.wall, 0, 1); face(u0, u1, Math.max(y0, F), y1, -.2, o.inside, 3, -1);
        ns ? o.solid(u0, y0, fix - t, u1, y1, fix + t) : o.solid(fix - t, y0, u0, fix + t, y1, u1);
      };
      if (!e) { piece(a0, a1, 0, H); continue; }
      // The way in is a porch in the shell's own language, not the hall's flat front (the owner, 2026-11-24, of that front's windows, name
      // band and doors: "these look so bad, can we remove those and improve the entrance?"). Under the arch two ribbon walls sweep in from
      // where the arch springs to the sides of the opening, so none of the hall's wall shows; the opening has a low arched head, ribbons
      // over it up to the arch, and a line of light round it. No door leaves: it stands open.
      piece(a0, c - hw, 0, H); piece(c + hw, a1, 0, H);
      const sk = l.shell.skin, tone = name => Math.max(0, Shell.TONES.indexOf(name)), kz = tone(sk.colors[0]) + 8 * tone(sk.colors[1] || sk.colors[0]), glow = [.3, .4, .97], SPRING = 3.3, N = 14;
      const openTop = u => SPRING + (DOOR_H + .1 - SPRING) * Math.sqrt(Math.max(0, 1 - 4 * (u - c) * (u - c) / (e.width * e.width)));
      const lift = !arch ? [] : sk.points.filter(q => q[2] > .02).map(q => ns ? [q[0], (q[1] - fix) * out, q[2]] : [q[1], (q[0] - fix) * out, q[2]]).filter(q => q[1] > 0 && Math.abs(q[0] - c) < arch.width / 2 + 6).sort((p, q) => p[0] - q[0]);   // the skin where it is lifted over this arch: along the wall, out from it, how high
      const roofAt = u => { const n = lift.length; if (!n) return H; if (u <= lift[0][0]) return lift[0][2]; if (u >= lift[n - 1][0]) return lift[n - 1][2];
        for (let i = 1; i < n; i++) if (u <= lift[i][0]) { const p = lift[i - 1], q = lift[i]; return p[2] + (q[2] - p[2]) * (u - p[0]) / (q[0] - p[0] || 1); } return H; };
      const up = u => Math.min(H, roofAt(u)) + .05, band = y => [y / 64, 0, kz];                                   // ribbons 1 m deep, level, on every face of the porch
      for (let i = 0; i < N; i++) {                             // over the opening: its arched head, the ribbons above it, the hall's wall inside
        const u0 = c - hw + e.width * i / N, u1 = c - hw + e.width * (i + 1) / N, y0 = openTop(u0), y1 = openTop(u1), r0 = up(u0), r1 = up(u1);
        m.quad(at(u0, y0, -.2), at(u1, y1, -.2), at(u1, H, -.2), at(u0, H, -.2), o.inside, 3, dir(-1));
        m.quadN([at(u0, y0, .3), at(u1, y1, .3), at(u1, r1, .3), at(u0, r0, .3)], [dir(1), dir(1), dir(1), dir(1)], [band(y0), band(y1), band(r1), band(r0)], 26);
        m.quad(at(u0, y0, -.2), at(u1, y1, -.2), at(u1, y1, .3), at(u0, y0, .3), o.C.stoneDark, 3, [0, -1, 0]);
        m.quad(at(u0, y0, .33), at(u1, y1, .33), at(u1, y1 + .22, .33), at(u0, y0 + .22, .33), glow, 1, dir(1));
        const ym = Math.min(y0, y1); ns ? o.solid(u0, ym, fix - t, u1, H, fix + t) : o.solid(fix - t, ym, u0, fix + t, H, u1);
      }
      for (const sg of [-1, 1]) { const u = c + sg * hw, inw = ns ? [-sg, 0, 0] : [0, 0, -sg];                    // the opening's sides, a line of light up each
        m.quad(at(u, 0, -.2), at(u, 0, .3), at(u, SPRING, .3), at(u, SPRING, -.2), o.C.stoneDark, 3, inw);
        m.quad(at(u - sg * .02, F, .06), at(u - sg * .02, F, .28), at(u - sg * .02, SPRING, .28), at(u - sg * .02, SPRING, .06), glow, 1, inw); }
      m.quad(at(c - hw, F, -.2), at(c + hw, F, -.2), at(c + hw, F, .32), at(c - hw, F, .32), o.floor, 16, [0, 1, 0]);
      if (lift.length > 2) for (const sg of [-1, 1]) {          // the two ribbon walls: from the opening's side straight out, then round to where the arch comes down
        const end = sg < 0 ? lift[0] : lift[lift.length - 1], M = 18, pts = [];
        for (let i = 0; i <= M; i++) { const a = Math.PI / 2 * i / M, u = c + sg * hw + (end[0] - c - sg * hw) * (1 - Math.cos(a)); pts.push([u, .3 + (end[1] - .3) * Math.sin(a), Math.max(.05, up(u))]); }
        const nrm = pts.map((_, i) => { const a = pts[Math.max(0, i - 1)], b = pts[Math.min(M, i + 1)], L = Math.hypot(b[0] - a[0], b[1] - a[1]) || 1; let nu = -(b[1] - a[1]) / L, nd = (b[0] - a[0]) / L; if (sg < 0) { nu = -nu; nd = -nd; }   // towards the porch
          return ns ? [nu, 0, nd * out] : [nd * out, 0, nu]; });
        for (let i = 0; i < M; i++) { const p = pts[i], q = pts[i + 1];
          m.quadN([at(p[0], 0, p[1]), at(q[0], 0, q[1]), at(q[0], q[2], q[1]), at(p[0], p[2], p[1])], [nrm[i], nrm[i + 1], nrm[i + 1], nrm[i]], [band(0), band(0), band(q[2]), band(p[2])], 26);
          const k2 = Math.max(1, Math.ceil(Math.hypot(q[0] - p[0], q[1] - p[1]) / 1.0));
          for (let j = 0; j < k2; j++) { const f = (j + .5) / k2, w = at(p[0] + (q[0] - p[0]) * f, 0, p[1] + (q[1] - p[1]) * f), top = p[2] + (q[2] - p[2]) * f; if (top > .4) o.solid(w[0] - .6, 0, w[2] - .6, w[0] + .6, top, w[2] + .6); } }
      }
      for (let fl = 1; fl < l.floors; fl++) for (const sg of [-1, 1]) face(c + sg * 3.4 - 2.6, c + sg * 3.4 + 2.6, fl * st + 1, fl * st + 4.4, -.23, [0, 0, .97], 1, -1, true);   // the end of each upper concourse: tall lit windows
    }
  }

  function draw(o, l) {
    l.outletRooms = [];                                       // each outlet's room, for buying in it (outlets.js Outlets.at, WLD-066)
    const m = o.mesh, P = l.plan, st = P.storey_height, F = o.F, H = l.H, C = o.C, [x0, z0, x1, z1] = l.rect;
    const cream = mul(C.cream, 1.2), plaster = o.tones[4], floorC = mul(o.floor, 1.1), shopCeil = mul(plaster, 1.14), dark = C.stoneDark, wood = C.wood, lit = s => [.3, .4, s];
    const fascia = mul(C.brown, .74), ceilC = mul(C.brown, .98), leafC = [.27, .37, .18];   // the ribbons along every edge and the ceilings are dark, as timber; floors pale and shop fronts bright (the owner's picture, in the town's own tones)
    const flat = (a, b, c, d, y, col, f, up) => m.quad([a[0], y, a[1]], [b[0], y, b[1]], [c[0], y, c[1]], [d[0], y, d[1]], col, f, [0, up, 0]);
    const wallQ = (S, u0, u1, y0, y1, d, col, f, uv) => m.quad(S.at(u0, y0, d), S.at(u1, y0, d), S.at(u1, y1, d), S.at(u0, y1, d), col, f, [S.n[0], 0, S.n[1]], uv);
    const boxQ = (S, u0, u1, y0, y1, d0, d1, col, f) => {   // a block standing against (or in front of) a wall
      wallQ(S, u0, u1, y0, y1, d1, col, f); m.quad(S.at(u0, y1, d0), S.at(u1, y1, d0), S.at(u1, y1, d1), S.at(u0, y1, d1), col, f, [0, 1, 0]);
      m.quad(S.at(u0, y0, d0), S.at(u0, y0, d1), S.at(u0, y1, d1), S.at(u0, y1, d0), col, f, [-S.t[0], 0, -S.t[1]]); m.quad(S.at(u1, y0, d0), S.at(u1, y0, d1), S.at(u1, y1, d1), S.at(u1, y1, d0), col, f, [S.t[0], 0, S.t[1]]);
    };
    const solidAlong = (a, b, y0, y1, r) => { const L = Math.hypot(b[0] - a[0], b[1] - a[1]), n = Math.max(1, Math.ceil(L / 1.1)); for (let i = 0; i < n; i++) { const f = (i + .5) / n, x = a[0] + (b[0] - a[0]) * f, z = a[1] + (b[1] - a[1]) * f; o.solid(x - r, y0, z - r, x + r, y1, z + r); } };
    const lantern = (x, y, z, top, s) => { m.box(x - .04, y + s, z - .04, x + .04, top, z + .04, C.metal, 19); m.box(x - s / 2, y, z - s / 2, x + s / 2, y + s, z + s / 2, C.lantern, 2, { 2: [C.lantern, 2] }); lamps.push([x, y - .1, z, 30]); };   // (no street light from them: the hall has its own light, and a lantern's glow in the air would veil the whole court)
    const prism = (pts, y0, y1, colS, colT, fS, fT) => {       // a closed line pushed up into a solid shape with a flat top
      const out = outward(pts), n = pts.length, cx = pts.reduce((s, p) => s + p[0], 0) / n, cz = pts.reduce((s, p) => s + p[1], 0) / n;
      for (let i = 0; i < n; i++) { const a = pts[i], b = pts[(i + 1) % n]; m.quad([a[0], y0, a[1]], [b[0], y0, b[1]], [b[0], y1, b[1]], [a[0], y1, a[1]], colS, fS, [out[i][0] + out[(i + 1) % n][0], 0, out[i][1] + out[(i + 1) % n][1]]); m.quad([cx, y1, cz], [a[0], y1, a[1]], [b[0], y1, b[1]], [cx, y1, cz], colT, fT, [0, 1, 0]); }
    };
    const round = (x, z, rx, rz, n) => Array.from({ length: n }, (_, i) => [x + rx * Math.cos(6.2832 * i / n), z + rz * Math.sin(6.2832 * i / n)]);
    const blob = (x, y, z, r, col, seed) => {                 // a tree's crown: a faceted ball, each face its own tone
      const A = 7, B = 4;
      for (let j = 0; j < B; j++) for (let i = 0; i < A; i++) {
        const q = (ii, jj) => { const ph = 6.2832 * ii / A + jj * .4, th = Math.PI * jj / B, rr = r * (.86 + .28 * o.hash(seed, ii % A, jj)); return [x + rr * Math.sin(th) * Math.cos(ph), y + r * .85 * Math.cos(th), z + rr * Math.sin(th) * Math.sin(ph)]; };
        const p = [q(i, j), q(i + 1, j), q(i + 1, j + 1), q(i, j + 1)], cx = (p[0][0] + p[2][0]) / 2 - x, cy = (p[0][1] + p[2][1]) / 2 - y, cz = (p[0][2] + p[2][2]) / 2 - z;
        const tone = mul(col, .8 + .4 * o.hash(seed, i, j, "c"));
        j ? m.quad(p[0], p[1], p[2], p[3], tone, 19, [cx, cy, cz]) : m.quad(p[3], p[2], p[1], p[0], tone, 19, [cx, cy, cz]);   // at the top the first two corners are one point: start from the others
      }
    };
    const tree = (x, z, y, species, seed, size) => { const k = size || 1, h = (species === "birch" ? 5.2 : 4.2) * k, r = (species === "birch" ? 2.1 : 2.9) * k, leaf = species === "birch" ? [.33, .4, .2] : [.24, .33, .17];
      m.box(x - .16, y, z - .16, x + .16, y + h, z + .16, species === "birch" ? [.6, .58, .52] : C.brown, 19); o.solid(x - .2, y, z - .2, x + .2, y + h, z + .2);
      blob(x, y + h + r * .35, z, r, leaf, seed); blob(x + r * .5, y + h - .3, z + r * .3, r * .62, leaf, seed + "b"); blob(x - r * .45, y + h - .1, z - r * .35, r * .58, leaf, seed + "c"); };

    // ---- floors, their undersides, the smooth strip and the parapet along each void
    const regions = P.levels.map((_, k) => region(P, k));
    P.levels.forEach((L, k) => {
      const fy = k ? k * st : F, R = regions[k], top = fy + st - (k ? 0 : F) - T;        // top: the underside of the floor above (or the roof)
      raster(l.rect, R.cell, (xa, za, xb, zb) => {
        flat([xa, za], [xa, zb], [xb, zb], [xb, za], fy, floorC, 16, 1);
        if (k) { flat([xa, za], [xa, zb], [xb, zb], [xb, za], fy - T, ceilC, 3, -1); o.solid(xa, fy - T, za, xb, fy, zb); }
      });
      if (k) for (const w of L.walks) if (w.length === 4) {   // where a straight walk runs out into an egg hall as a balcony (the squares stop short of the hall): its own pieces there, straight to its edges
        const la = Math.hypot(w[1][0] - w[0][0], w[1][1] - w[0][1]), lb = Math.hypot(w[3][0] - w[0][0], w[3][1] - w[0][1]), na = Math.ceil(la / 1.2), nb = Math.ceil(lb / 1.2);
        const at = (s, v) => [w[0][0] + (w[1][0] - w[0][0]) * s + (w[3][0] - w[0][0]) * v, w[0][1] + (w[1][1] - w[0][1]) * s + (w[3][1] - w[0][1]) * v];
        for (let i = 0; i < na; i++) for (let j = 0; j < nb; j++) {
          const c = at((i + .5) / na, (j + .5) / nb); if (R.hole(c[0], c[1], 0) || !P.halls.some(h => Math.hypot(c[0] - h.pos[0], c[1] - h.pos[1]) < h.r + 2.4)) continue;
          const p = [at(i / na, j / nb), at((i + 1) / na, j / nb), at((i + 1) / na, (j + 1) / nb), at(i / na, (j + 1) / nb)];
          flat(p[0], p[1], p[2], p[3], fy, floorC, 16, 1); flat(p[0], p[1], p[2], p[3], fy - T, ceilC, 3, -1); o.solid(c[0] - .8, fy - T, c[1] - .8, c[0] + .8, fy, c[1] + .8);
        }
      }
      for (const v of L.voids) {
        const out = outward(v), n = v.length, W = 2.9;
        for (let i = 0; i < n; i++) {
          const j = (i + 1) % n, a = v[i], b = v[j], a2 = [a[0] + out[i][0] * W, a[1] + out[i][1] * W], b2 = [b[0] + out[j][0] * W, b[1] + out[j][1] * W];
          flat(a, b, b2, a2, fy + .006, floorC, 16, 1); flat(a, b, b2, a2, fy - T, ceilC, 3, -1);
          for (const [d, w] of [[1.2, .2], [2.4, .12]]) {           // lines of light in the ceiling, following the edge
            const p = (q, oo, dd) => [q[0] + oo[0] * dd, q[1] + oo[1] * dd]; flat(p(a, out[i], d), p(b, out[j], d), p(b, out[j], d + w), p(a, out[i], d + w), fy - T - .012, lit(.97), 1, -1); }
          for (const f of [.17, .5, .83]) for (const r of [.4, 1.3, 2.2, 3.1]) { const x = a[0] + (b[0] - a[0]) * f + (out[i][0] + out[j][0]) / 2 * r, z = a[1] + (b[1] - a[1]) * f + (out[i][1] + out[j][1]) / 2 * r; o.solid(x - .95, fy - T, z - .95, x + .95, fy, z + .95); }
        }
      }
      const inBridge = (x, z) => L.bridges.some(b => { const q = boxOf(b); return x > q[0] - .2 && x < q[2] + .2 && z > q[1] - .2 && z < q[3] + .2; });
      const parapet = (a, b, voidSide, endA, endB, nA, nB) => {   // a ribbon from under the slab to hand height, a line of light under it; closed all round, as the shade pass sees both sides of a face
        const S = frame(a, b, voidSide), y0 = fy - T - .4, y1 = fy + RAIL, back = [-S.n[0], 0, -S.n[1]];
        const bk = (e, y) => { const nE = e ? nB : nA, p = S.at(e ? S.L : 0, y, 0); if (!nE) return S.at(e ? S.L : 0, y, -.3);   // its back at an end: where it meets the next length's (nA, nB: the way out at that joint)
          const q = .3 / Math.max(.5, nE[0] * S.n[0] + nE[1] * S.n[1]); return [p[0] - nE[0] * q, y, p[2] - nE[1] * q]; };
        const yf = fy + .3;                                    // the dark fascia runs from under the slab to just over the floor line; a pale upstand above it
        m.quad(bk(0, y0), bk(1, y0), bk(1, fy - T), bk(0, fy - T), fascia, 3, back);
        for (const [on, e, sg] of [[endA, 0, -1], [endB, 1, 1]]) if (on) { m.quad(S.at(e * S.L, y0, 0), bk(e, y0), bk(e, yf), S.at(e * S.L, yf, 0), fascia, 3, [S.t[0] * sg, 0, S.t[1] * sg]); m.quad(S.at(e * S.L, yf, 0), bk(e, yf), bk(e, y1), S.at(e * S.L, y1, 0), cream, 3, [S.t[0] * sg, 0, S.t[1] * sg]); }
        wallQ(S, 0, S.L, y0, yf, 0, fascia, 3); wallQ(S, 0, S.L, yf, y1, 0, cream, 3); wallQ(S, 0, S.L, y0 + .06, y0 + .3, .03, lit(.97), 1);
        m.quad(S.at(0, y1, .04), S.at(S.L, y1, .04), bk(1, y1), bk(0, y1), wood, 19, [0, 1, 0]); wallQ(S, 0, S.L, y1 - .07, y1, .04, wood, 19);
        m.quad(bk(0, fy), bk(1, fy), bk(1, y1), bk(0, y1), plaster, 3, back);
        m.quad(S.at(0, y0, 0), S.at(S.L, y0, 0), bk(1, y0), bk(0, y0), fascia, 3, [0, -1, 0]);
        solidAlong([S.mx - S.t[0] * S.L / 2 - S.n[0] * .15, S.mz - S.t[1] * S.L / 2 - S.n[1] * .15], [S.mx + S.t[0] * S.L / 2 - S.n[0] * .15, S.mz + S.t[1] * S.L / 2 - S.n[1] * .15], fy, fy + RAIL, .3);
      };
      for (const rl of L.rails) {
        const n = rl.length - 1, shut = rl[0][0] === rl[n][0] && rl[0][1] === rl[n][1], toVoid = (x, z) => !R.on(x, z);
        const sn = Array.from({ length: n }, (_, i) => frame(rl[i], rl[i + 1], toVoid).n), gone = Array.from({ length: n }, (_, i) => inBridge((rl[i][0] + rl[i + 1][0]) / 2, (rl[i][1] + rl[i + 1][1]) / 2));
        const joint = (i, j) => { if (j < 0 || j >= n) { if (!shut) return null; j = (j + n) % n; } const x = sn[i][0] + sn[j][0], z = sn[i][1] + sn[j][1], q = Math.hypot(x, z); return gone[j] || q < .2 ? null : [x / q, z / q]; };   // the way out at the joint of lengths i and j; null where the ribbon ends
        for (let i = 0; i < n; i++) if (!gone[i]) { const nA = joint(i, i - 1), nB = joint(i, i + 1); parapet(rl[i], rl[i + 1], toVoid, !nA, !nB, nA, nB); }
        if (shut && n > 40) { let run = 3 + 5 * o.hash(l.id, k, "green");   // plants in troughs on the parapet, trailing down over the fascia (the owner's picture)
          for (let i = 0; i < n; i++) { const a = rl[i], b = rl[i + 1], len = Math.hypot(b[0] - a[0], b[1] - a[1]); run -= len; if (run > 0 || gone[i]) continue; run = 6.5 + 5 * o.hash(l.id, k, i, "gap");
            const x = (a[0] + b[0]) / 2 + sn[i][0] * .05, z = (a[1] + b[1]) / 2 + sn[i][1] * .05, tx = (b[0] - a[0]) / len, tz = (b[1] - a[1]) / len, big = o.hash(l.id, k, i, "big");
            for (const u of [-.7, 0, .7]) blob(x + tx * u, fy + RAIL + .22, z + tz * u, .5 + .22 * o.hash(l.id, k, i, u), leafC, l.id + k + "," + i + u);
            for (let s = 0; s < 6; s++) { const u = -1.1 + 2.2 * o.hash(l.id, k, i, s, "u"), drop = .5 + (1.3 + big * 1.4) * o.hash(l.id, k, i, s, "d"), px = x + tx * u + sn[i][0] * .14, pz = z + tz * u + sn[i][1] * .14;
              m.box(px - .09, fy + RAIL - drop, pz - .09, px + .09, fy + RAIL + .1, pz + .09, mul(leafC, .8 + .4 * o.hash(l.id, k, i, s, "c")), 19); } } }
      }
      for (const br of L.bridges) {                             // a bridge: its slab, and a parapet along each side, open where a moving stair lands
        const q = boxOf(br); flat([q[0], q[1]], [q[0], q[3]], [q[2], q[3]], [q[2], q[1]], fy + .008, floorC, 16, 1); flat([q[0], q[1]], [q[0], q[3]], [q[2], q[3]], [q[2], q[1]], fy - T, ceilC, 3, -1); o.solid(q[0], fy - T, q[1], q[2], fy, q[3]);
        for (const [zs, sg] of [[q[1], -1], [q[3], 1]]) {
          const gap = P.ramps.map(r => r.to === k && Math.abs(r.b[1] - zs) < .6 ? [r.b[0], r.width] : r.from === k && Math.abs(r.a[1] - zs) < .6 ? [r.a[0], r.width] : null).find(g => g);
          const parts = gap ? [[q[0], gap[0] - gap[1] / 2 - .1], [gap[0] + gap[1] / 2 + .1, q[2]]] : [[q[0], q[2]]];
          for (const [u0, u1] of parts) parapet([u0, zs], [u1, zs], [(u0 + u1) / 2, zs + sg * 4], true, true);
        }
      }
      // ---- walls and shop fronts
      const open = new Set(), units = {}; L.units.forEach(u => { units[u.id] = u; if (u.outlet && u.stand && (l.objects.work || []).some(w => u.stand.some(s => Math.abs(s[0] - w[0]) < .3 && Math.abs(s[1] - w[1]) < .3))) open.add(u.id); });
      for (const fr of L.fronts) {                             // every front is an outlet of some kind, with its room and what is sold in it (outlets.js, WLD-063)
        const S = frame(fr.a, fr.b, R.walk), u = units[fr.unit];
        Outlets.front({ m, o, C, S, u, fy, top, k, T, lamp: (x, y, z, w) => lamps.push([x, y, z, w]), Lf: S.L, rooms: l.outletRooms, staffed: open.has(u.id), spec: (P.outlets || {})[u.outlet], depth: P.outlet_depth, blob, prism, round, wallQ, boxQ, solidAlong, plaster, dark, wood, shopCeil, floorC });
      }
      for (const w of L.walls) for (let i = 0; i < w.line.length - 1; i++) {
        const a = w.line[i], b = w.line[i + 1], drum = w.kind === "drum", hh = drum ? P.halls.reduce((p, h) => Math.hypot(h.pos[0] - a[0], h.pos[1] - a[1]) < Math.hypot(p.pos[0] - a[0], p.pos[1] - a[1]) ? h : p) : null;
        const S = frame(a, b, drum ? hh.pos : R.walk), y1 = drum ? fy + st - (k ? 0 : F) : top;
        wallQ(S, 0, S.L, fy - (drum && k ? T : 0), y1, 0, drum ? cream : plaster, 3);
        if (drum) { wallQ(S, 0, S.L, fy + 4.3, fy + 4.42, .03, lit(.97), 1); if (!k) wallQ(S, 0, S.L, fy, fy + .5, .05, dark, 3); }
        else if (i % 3 === 1 && S.L > 1.7) wallQ(S, .15, S.L - .15, fy + 1.0, fy + 3.1, .04, [0, 0, .96 + .03 * o.hash(l.id, k, i, a[0])], 1, true);   // a lit display case now and then
        solidAlong(a, b, fy, y1, .45);
      }
    });

    // ---- the roof: solid round the court's opening and the egg halls' eyes, with a rim under the roof light
    const eyes = P.roof.openings.map(e => [e, boxOf(e)]), RIM = 1.5;   // a round skylight over each atrium: a deep dark rim, a ring of light in it
    const roofCell = (x, z) => !eyes.some(([e, b]) => x > b[0] - 1.5 && x < b[2] + 1.5 && z > b[1] - 1.5 && z < b[3] + 1.5 && (inPoly(e, x, z) || edgeDist(e, x, z) < 1.4)) && !P.halls.some(h => Math.hypot(x - h.pos[0], z - h.pos[1]) < h.r + 1.2);
    raster(l.rect, roofCell, (xa, za, xb, zb) => { flat([xa, za], [xa, zb], [xb, zb], [xb, za], H, mul(cream, .86), 0, 1); flat([xa, za], [xa, zb], [xb, zb], [xb, za], H - T, ceilC, 3, -1); });
    for (const [e, hb] of eyes) { const out = outward(e), n = e.length, mid = [(hb[0] + hb[2]) / 2, (hb[1] + hb[3]) / 2];
      for (let i = 0; i < n; i++) { const j = (i + 1) % n, a = e[i], b = e[j], a2 = [a[0] + out[i][0] * 2.9, a[1] + out[i][1] * 2.9], b2 = [b[0] + out[j][0] * 2.9, b[1] + out[j][1] * 2.9], a3 = [a[0] + out[i][0] * .35, a[1] + out[i][1] * .35], b3 = [b[0] + out[j][0] * .35, b[1] + out[j][1] * .35];
        flat(a, b, b2, a2, H - T - .004, ceilC, 3, -1); flat(a, b, b2, a2, H + .004, mul(cream, .86), 0, 1);
        const S = frame(a, b, mid); wallQ(S, 0, S.L, H - T - RIM, H + .3, 0, fascia, 3); wallQ(S, 0, S.L, H - T - RIM + .12, H - T - RIM + .36, .03, lit(.97), 1);
        m.quad([a3[0], H - T - RIM, a3[1]], [b3[0], H - T - RIM, b3[1]], [b3[0], H - T, b3[1]], [a3[0], H - T, a3[1]], fascia, 3, [-S.n[0], 0, -S.n[1]]);   // its back and its underside, so it is closed
        m.quad([a[0], H - T - RIM, a[1]], [b[0], H - T - RIM, b[1]], [b3[0], H - T - RIM, b3[1]], [a3[0], H - T - RIM, a3[1]], fascia, 3, [0, -1, 0]); } }

    // ---- moving stairs: a slope with a parapet each side, walked up as thirty low steps
    for (const r of P.ramps) {
      const ya = r.from ? r.from * st : F, yb = r.to * st, dx = r.b[0] - r.a[0], dz = r.b[1] - r.a[1], Lr = Math.hypot(dx, dz), t = [dx / Lr, dz / Lr], s = [-t[1], t[0]], hw = r.width / 2;
      const pt = (f, side, y) => [r.a[0] + dx * f + s[0] * side, ya + (yb - ya) * f + y, r.a[1] + dz * f + s[1] * side];
      const Ls = Math.hypot(Lr, yb - ya), upN = [0, 1, 0], MID = .15;
      for (const sg of [-1, 1]) {                             // two flights: seen from the foot, the right one runs up and the left one down. Their treads move (material 28: metres along, which way, metres across)
        const s0 = sg > 0 ? MID : -hw + .3, s1 = sg > 0 ? hw - .3 : -MID;
        m.quadN([pt(0, s0, .01), pt(0, s1, .01), pt(1, s1, .01), pt(1, s0, .01)], [upN, upN, upN, upN], [[0, sg, s0], [0, sg, s1], [Ls, sg, s1], [Ls, sg, s0]], 28);
        m.quad(pt(0, sg * MID, 0), pt(1, sg * MID, 0), pt(1, sg * MID, RAIL - .2), pt(0, sg * MID, RAIL - .2), plaster, 3, [s[0] * sg, 0, s[1] * sg]);   // the low wall between them
      }
      m.quad(pt(0, -MID, RAIL - .2), pt(0, MID, RAIL - .2), pt(1, MID, RAIL - .2), pt(1, -MID, RAIL - .2), cream, 3, [0, 1, 0]);
      for (const f of [0, 1]) m.quad(pt(f, -MID, 0), pt(f, MID, 0), pt(f, MID, RAIL - .2), pt(f, -MID, RAIL - .2), cream, 3, [t[0] * (f ? 1 : -1), 0, t[1] * (f ? 1 : -1)]);
      m.quad(pt(0, -hw, -T), pt(0, hw, -T), pt(1, hw, -T), pt(1, -hw, -T), ceilC, 3, [0, -1, 0]);
      moving.push({ a: r.a, t, s, Lr, ya, yb, hw, v: STAIR_V * Lr / Ls });
      for (const sg of [-1, 1]) {
        m.quad(pt(0, sg * hw, -T), pt(1, sg * hw, -T), pt(1, sg * hw, RAIL), pt(0, sg * hw, RAIL), fascia, 3, [s[0] * sg, 0, s[1] * sg]);   // dark outside, as the galleries' fascias: the stairs cross the atrium as two ribbons
        m.quad(pt(0, sg * (hw + .02), -T + .04), pt(1, sg * (hw + .02), -T + .04), pt(1, sg * (hw + .02), -T + .2), pt(0, sg * (hw + .02), -T + .2), lit(.97), 1, [s[0] * sg, 0, s[1] * sg]);
        for (const f of [0, 1]) m.quad(pt(f, sg * hw, -T), pt(f, sg * (hw - .3), -T), pt(f, sg * (hw - .3), RAIL), pt(f, sg * hw, RAIL), fascia, 3, [t[0] * (f ? 1 : -1), 0, t[1] * (f ? 1 : -1)]);
        m.quad(pt(0, sg * (hw - .3), 0), pt(1, sg * (hw - .3), 0), pt(1, sg * (hw - .3), RAIL), pt(0, sg * (hw - .3), RAIL), plaster, 3, [-s[0] * sg, 0, -s[1] * sg]);
        m.quad(pt(0, sg * hw, RAIL), pt(1, sg * hw, RAIL), pt(1, sg * (hw - .3), RAIL), pt(0, sg * (hw - .3), RAIL), wood, 19, [0, 1, 0]);
      }
      const N = 30;
      for (let i = 0; i < N; i++) {
        const p0 = pt(i / N, 0, 0), p1 = pt((i + 1) / N, 0, 0), y1 = ya + (yb - ya) * (i + 1) / N, ex = Math.abs(s[0]) * hw, ez = Math.abs(s[1]) * hw;
        o.solid(Math.min(p0[0], p1[0]) - ex, y1 - T, Math.min(p0[2], p1[2]) - ez, Math.max(p0[0], p1[0]) + ex, y1, Math.max(p0[2], p1[2]) + ez);
        for (const sg of [-1, 1]) { const q = pt((i + .5) / N, sg * (hw - .15), 0); o.solid(q[0] - .25, y1 - T, q[2] - .25, q[0] + .25, y1 + RAIL, q[2] + .25); }
        { const q = pt((i + .5) / N, 0, 0); o.solid(q[0] - .24, y1 - T, q[2] - .24, q[0] + .24, y1 + RAIL - .2, q[2] + .24); }
      }
    }

    // ---- lifts: a round shaft through every floor, a door on each; using it takes you up or down a floor
    for (const q of P.lifts || []) {
      const [x, z] = q.pos, r = q.r, yaw = q.door * Math.PI / 180, fx = Math.sin(yaw), fz = -Math.cos(yaw), rx = -fz, rz = fx, n = P.levels.length;
      prism(round(x, z, r, r, 24), F, H - T, cream, cream, 3, 3); prism(round(x, z, r + .08, r + .08, 24), F, F + .5, dark, dark, 3, 3);
      for (let i = 0; i < 10; i++) { const a = 6.2832 * i / 10; o.solid(x + (r - .5) * Math.cos(a) - .5, F, z + (r - .5) * Math.sin(a) - .5, x + (r - .5) * Math.cos(a) + .5, H, z + (r - .5) * Math.sin(a) + .5); }
      const fyOf = k => k ? k * st : F, to = k => [x + fx * (r + 1.3), fyOf(k), z + fz * (r + 1.3)], floorName = k => k ? "floor " + k : "the ground floor";
      for (let k = 0; k < n; k++) {
        const fy = fyOf(k), c = [x + fx * (r + .17), z + fz * (r + .17)], S = frame([c[0] - rx * .8, c[1] - rz * .8], [c[0] + rx * .8, c[1] + rz * .8], [x + fx * 9, z + fz * 9]);
        m.ybox(x + fx * (r - .12), fy, z + fz * (r - .12), 2.2, 2.75, .56, -yaw, plaster, 3);                            // the door's surround, standing out of the shaft
        wallQ(S, .05, .78, fy + .03, fy + 2.25, 0, mul(C.metal, 1.5), 19); wallQ(S, .82, 1.55, fy + .03, fy + 2.25, 0, mul(C.metal, 1.5), 19);   // two leaves
        wallQ(S, .05, 1.55, fy + 2.32, fy + 2.56, 0, lit(.97), 1);                                                    // a light over them
        prism(round(x, z, r + .03, r + .03, 24), fy + 4.3, fy + 4.42, lit(.97), cream, 1, 3);
        const ux = x + fx * (r + .9), uz = z + fz * (r + .9), back = yaw + Math.PI;
        if (k < n - 1) o.use(ux - rx * .35, fy, uz - rz * .35, "lift", "Lift: up to floor " + (k + 1), back, { to: to(k + 1) });
        if (k > 0) o.use(ux + rx * .35, fy, uz + rz * .35, "lift", "Lift: down to " + floorName(k - 1), back, { to: to(k - 1) });
        if (k === 0 && n > 2) o.use(ux + rx * .35, fy, uz + rz * .35, "lift", "Lift: up to floor " + (n - 1), back, { to: to(n - 1) });
        if (k === n - 1 && n > 2) o.use(ux - rx * .35, fy, uz - rz * .35, "lift", "Lift: down to the ground floor", back, { to: to(0) });
      }
    }

    // ---- the egg halls: the egg seen from inside, lanterns hanging in a ring, a stall where each of its staff stands, a tree in the middle
    const anchors = {}; P.levels[0].units.forEach(u => { if (u.kind === "anchor") anchors[u.id] = u; });
    for (const h of P.halls) {
      const [px, pz] = h.pos;
      Shell.egg(o, l.shell.domes[h.dome], H - .6, true);
      for (let i = 0; i < 6; i++) { const a = 6.2832 * (i + .5) / 6; lantern(px + h.r * .62 * Math.cos(a), 6.4 + (i % 2) * 1.6, pz + h.r * .62 * Math.sin(a), H + 6, .7); }
      for (const [x, z] of (anchors[h.unit] || {}).stand || []) { const dx = px - x, dz = pz - z, d = Math.hypot(dx, dz) || 1, yaw = Math.atan2(dx, -dz), cx = x + dx / d * 1.0, cz = z + dz / d * 1.0;
        m.ybox(cx, F, cz, 2.8, 1.0, .8, -yaw, wood, 19); m.ybox(cx, F + 1.0, cz, 3.0, .08, 1.0, -yaw, C.cream, 19); o.solid(cx - .9, F, cz - .9, cx + .9, F + 1.0, cz + .9); }
      prism(round(px, pz, 3.2, 3.2, 16), F, F + .7, C.stone, [.2, .15, .11], 3, 19); solidAlong([px - 2.4, pz], [px + 2.4, pz], F, F + .7, 2.4); tree(px, pz, F + .7, "linden", h.unit);
    }
    Shell.egg(o, l.shell.roof_light, l.shell.roof_light.base, true);

    // ---- the court floor: the fountain, the gardens and their trees, benches, kiosks, lanterns under the galleries
    for (const f of P.fountains) { const [x, z] = f.pos, r = f.r;
      prism(round(x, z, r, r, 28), F, F + .75, C.stone, C.stone, 3, 3); prism(round(x, z, r - .6, r - .6, 28), F + .75, F + .78, C.water, mul(C.water, 1.5), 19, 19);
      prism(round(x, z, 1.1, 1.1, 12), F + .5, F + 2.6, C.stone, C.stone, 3, 3); prism(round(x, z, 2.6, 2.6, 16), F + 2.6, F + 2.9, C.stone, C.water, 3, 19); prism(round(x, z, .35, .35, 8), F + 2.9, F + 4.6, [.62, .7, .74], [.62, .7, .74], 19, 19);
      for (let i = 0; i < 20; i++) { const a = 6.2832 * i / 20; o.solid(x + r * Math.cos(a) - .8, F, z + r * Math.sin(a) - .8, x + r * Math.cos(a) + .8, F + .75, z + r * Math.sin(a) + .8); } }
    for (const g of P.gardens) { const [x, z] = g.pos;
      prism(round(x, z, g.r[0], g.r[1], 28), F, F + .42, C.stone, [.25, .33, .2], 3, 19);
      g.trees.forEach((t, i) => tree(t[0], t[1], F + .42, t[2], x + "," + z + i));
      if (g.low) for (let i = 0; i < 16; i++) { const a = 6.2832 * o.hash(x, z, i, "a"), d = Math.sqrt(o.hash(x, z, i, "d")) * .8; blob(x + g.r[0] * d * Math.cos(a), F + .42 + .35, z + g.r[1] * d * Math.sin(a), .7 + .5 * o.hash(x, z, i, "r"), leafC, "low" + i); }   // shrubs only, where the stairs cross overhead
      for (let i = 0; i < 28; i++) { const a = 6.2832 * i / 28; o.solid(x + (g.r[0] - .5) * Math.cos(a) - .6, F, z + (g.r[1] - .5) * Math.sin(a) - .6, x + (g.r[0] - .5) * Math.cos(a) + .6, F + (g.low ? 1.2 : .42), z + (g.r[1] - .5) * Math.sin(a) + .6); } }
    for (const q of P.planters || []) { const [x, z] = q.pos, y = q.level * st;   // a round planter on a gallery, a small tree in it
      prism(round(x, z, q.r, q.r, 16), y, y + .6, fascia, [.2, .15, .11], 3, 19); prism(round(x, z, q.r + .12, q.r + .12, 16), y + .5, y + .62, wood, wood, 19, 19);
      for (let i = 0; i < 8; i++) { const a = 6.2832 * i / 8; o.solid(x + q.r * .6 * Math.cos(a) - .75, y, z + q.r * .6 * Math.sin(a) - .75, x + q.r * .6 * Math.cos(a) + .75, y + 1.0, z + q.r * .6 * Math.sin(a) + .75); }
      tree(x, z, y + .6, q.tree, "pl" + x + z, .5); blob(x + .9, y + .85, z - .5, .55, leafC, "pa" + x + z); blob(x - .8, y + .8, z + .7, .5, leafC, "pb" + x + z); }
    for (const q of P.rides || []) Outlets.ride({ m, o, C, F, blob, prism, round }, q);   // the carousel and the coin rides (outlets.js)
    for (const b of P.benches) { const yaw = b.yaw * Math.PI / 180, [x, z] = b.pos, fx = Math.sin(yaw), fz = -Math.cos(yaw);
      m.ybox(x, F + .3, z, 1.9, .16, .55, -yaw, wood, 19); m.ybox(x - fx * .3, F + .46, z - fz * .3, 1.9, .5, .1, -yaw, wood, 19);
      for (const sgn of [-.8, .8]) m.ybox(x + fz * sgn * -1, F, z + fx * sgn, .12, .3, .5, -yaw, C.metal, 19);
      o.solid(x - .6, F, z - .6, x + .6, F + .46, z + .6); o.use(x, F + .47, z, "sit", "Sit on the bench", yaw); }
    for (const u of P.levels[0].units) if (u.kind === "kiosk") { const [a, b, c, d] = u.rect, acc = o.accent[Math.floor(o.hash(u.id, "k") * o.accent.length)];
      m.box(a, F, b, c, F + 1.05, d, wood, 19); m.box(a - .3, F + 2.7, b - .3, c + .3, F + 2.95, d + .3, acc, 19, { 2: [mul(acc, .7), 19] });
      for (const [x, z] of [[a, b], [c, b], [a, d], [c, d]]) m.box(x - .08, F + 1.05, z - .08, x + .08, F + 2.7, z + .08, C.metal, 19);
      m.box((a + c) / 2 - .2, F + 2.2, (b + d) / 2 - .2, (a + c) / 2 + .2, F + 2.6, (b + d) / 2 + .2, C.lantern, 2, { 2: [C.lantern, 2] }); o.solid(a, F, b, c, F + 1.05, d); }
    const under = st - T;                                       // lanterns under the lower gallery, round the court and down each concourse
    for (const e of P.entrances) { const ns = e.side === "n" || e.side === "s", sg = e.side === "n" || e.side === "w" ? 1 : -1;
      for (let d = 10; d < 70; d += 15) lantern(e.at[0] + (ns ? 0 : sg * d), F + 3.9, e.at[1] + (ns ? sg * d : 0), F + under, .55); }
    { const v = P.levels[1].voids[0], out = outward(v); for (let i = 0; i < v.length; i += 11) lantern(v[i][0] + out[i][0] * 7.5, F + 3.9, v[i][1] + out[i][1] * 7.5, F + under, .55); }

    // ---- where the staff stand: a keeper behind the counter of every outlet on every floor, facing the walk (CITY-105); in an egg hall
    // facing its middle; and the guards and janitors of the whole mall at their own places (plan.staff)
    P.levels.forEach((L, k) => { for (const u of L.units) { if (k && !u.outlet) continue;          // (a big store is one unit on every floor: its staff are on the ground)
      for (const s of u.stand || []) {
        const h = u.kind === "anchor" ? P.halls.find(q => q.unit === u.id) : null;
        o.spot(s[0], s[1], 0, h ? Math.atan2(h.pos[0] - s[0], -(h.pos[1] - s[1])) : (u.angle || 0) * Math.PI / 180, k ? k * st : F);
      } } });
    for (const c in P.staff || {}) for (const q of P.staff[c]) o.spot(q.pos[0], q.pos[1], 0, q.yaw * Math.PI / 180, q.level ? q.level * st : F);
  }
  function carry(p, dt) {                                   // someone standing on moving stairs is carried: up on the right-hand flight, down on the left
    for (const r of moving) {
      const qx = p.x - r.a[0], qz = p.z - r.a[1], f = (qx * r.t[0] + qz * r.t[1]) / r.Lr, side = qx * r.s[0] + qz * r.s[1];
      if (f < 0 || f > 1 || Math.abs(side) > r.hw || Math.abs(p.y - (r.ya + (r.yb - r.ya) * f)) > .9) continue;
      const d = (side > 0 ? 1 : -1) * r.v * dt; p.x += r.t[0] * d; p.z += r.t[1] * d; return true;
    }
    return false;
  }
  // what the room light needs to know of a court mall (play.js): where the sky comes in
  const light = l => { const b = boxOf(l.plan.levels[l.plan.levels.length - 1].voids[0]), c = l.plan.court || { lobe: 36, voids: {} }, v = k => { const q = c.voids[k]; return q ? [q.wide, q.narrow, q.long, q.sway] : [0, 0, 1, 0]; };
    return { lamps, k: [(l.rect[0] + l.rect[2]) / 2, (l.rect[1] + l.rect[3]) / 2, c.lobe, l.H], v1: v(1), v2: v(2), eyes: l.plan.roof.openings.slice(0, 5).map(e => { const q = boxOf(e); return [(q[0] + q[2]) / 2, (q[1] + q[3]) / 2, (q[2] - q[0]) / 2, 0]; }), rect: l.rect, eye: [(b[0] + b[2]) / 2, (b[1] + b[3]) / 2, (b[2] - b[0]) / 2, (b[3] - b[1]) / 2], halls: l.plan.halls.slice(0, 4).map(h => [h.pos[0], h.pos[1], h.r, 0]) }; };
  return { can: l => !!(l.plan && l.plan.concept === "court" && l.shell && l.plan.entrances), hall, draw, carry, light };
})();
