"use strict";
// A mall's sculpted shell (WORLD_MAP WLD-057, ADR-0037; the owner, 2026-11-24, from a photograph of a mall of flowing white
// bands and golden pierced domes). Where it stands, how high and where it is lifted is data: the lot's `shell` in the block
// file (World/city_01/blocks/b_16_5.json, made by Tools/worldgen/mall_shell_01.py). This file only draws it, in the town's
// own materials (ART_DIRECTION 2.2: matte and mottled, chunky and soft, the muted palette, lanterns at dusk):
//
//   the skin     a wall of plaster ribbons standing on the skin's line, swelling out a little half way up and curling in
//                at the top; its top rises and falls, dips round each egg and lifts off the ground as round arches. The
//                joints between the ribbons are dark by day and lit at night (material 26)
//   the eggs     an ochre egg over each corner store and a cream half egg over the court, pierced by a lattice of diamond
//                openings: dark reveals by day, lanterns behind them at night (material 25)
//   the arches   under each, the underside of the skin and lanterns hanging from it; where the mall has its inside built, a
//                porch of ribbon walls sweeping in to the way in (court.js), otherwise the hall's own front (play.js mallShell)
//
// The hall inside keeps its box, its door and its walls you cannot walk through; the skin adds its own.
const Shell = (() => {
  const TONES = ["ochre", "cream", "terracotta", "teal", "plaster"];   // the colours a shell may name; play.js holds them in this order for the shader
  const tone = name => Math.max(0, TONES.indexOf(name));
  const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
  const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
  const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const neg = v => [-v[0], -v[1], -v[2]];
  const norm = v => { const l = Math.hypot(v[0], v[1], v[2]) || 1; return [v[0] / l, v[1] / l, v[2] / l]; };
  const SEC = 12;                                          // steps up the skin
  const OUT = { n: [0, -1], s: [0, 1], w: [-1, 0], e: [1, 0] };

  function skin(o, l, sh) {
    const sk = sh.skin, pts = sk.points, n = pts.length, [x0, z0, x1, z1] = l.rect, m = o.mesh;
    const kz = tone(sk.colors[0]) + 8 * tone(sk.colors[1] || sk.colors[0]), base = o.tones[tone(sk.colors[0])];
    const grid = [], hall = [];
    for (let i = 0; i < n; i++) {                           // the wall's section at each point of the line: straight up with a belly, then a quarter turn in towards the hall
      const [x, z, B, T] = pts[i], qx = clamp(x, x0, x1), qz = clamp(z, z0, z1), c = Math.hypot(qx - x, qz - z) || 1, ex = (qx - x) / c, ez = (qz - z) / c;
      const h = T - B, roll = Math.min(sk.roll, .75 * c, .8 * h), L1 = h - roll, L2 = roll * Math.PI / 2, row = [];
      for (let j = 0; j <= SEC; j++) {
        const a = j / SEC * (L1 + L2); let y, off;
        if (a <= L1) { const s = Math.sin(Math.PI * a / L1); y = B + a; off = -sk.belly * s * s * Math.min(1, h / 12); }
        else { const th = (a - L1) / roll; y = B + L1 + roll * Math.sin(th); off = roll * (1 - Math.cos(th)); }
        row.push([x + ex * off, y, z + ez * off]);
      }
      grid.push(row); hall.push([qx, qz]);
    }
    const nrm = grid.map((row, i) => row.map((p, j) => {    // smooth: each point's normal from its neighbours, facing away from the hall
      const v = norm(cross(sub(row[Math.min(SEC, j + 1)], row[Math.max(0, j - 1)]), sub(grid[(i + 1) % n][j], grid[(i + n - 1) % n][j])));
      return v[0] * (pts[i][0] - hall[i][0]) + v[2] * (pts[i][1] - hall[i][1]) + v[1] * 4 < 0 ? [-v[0], -v[1], -v[2]] : v;
    }));
    const soffit = [base[0] * .78, base[1] * .78, base[2] * .78], lid = [base[0] * .86, base[1] * .86, base[2] * .86], H = l.H + .35;
    for (let i = 0; i < n; i++) {
      const i1 = (i + 1) % n, A = pts[i], B = pts[i1], qa = hall[i], qb = hall[i1];
      for (let j = 0; j < SEC; j++) {
        const ca = [j / SEC * sk.bands / 64, 0, kz], cb = [(j + 1) / SEC * sk.bands / 64, 0, kz];
        m.quadN([grid[i][j], grid[i1][j], grid[i1][j + 1], grid[i][j + 1]], [nrm[i][j], nrm[i1][j], nrm[i1][j + 1], nrm[i][j + 1]], [ca, ca, cb, cb], 26);
        m.quadN([grid[i][j], grid[i1][j], grid[i1][j + 1], grid[i][j + 1]], [neg(nrm[i][j]), neg(nrm[i1][j]), neg(nrm[i1][j + 1]), neg(nrm[i][j + 1])], [soffit, soffit, soffit, soffit], 0);   // its back, plain plaster: the space between the skin and the hall is not for walking in, but nothing may be seen through from there
      }
      m.quad(grid[i][SEC], grid[i1][SEC], [qb[0], H, qb[1]], [qa[0], H, qa[1]], lid, 0, [0, 1, 0]);   // from the skin's top edge back to the hall's roof
      m.quad(grid[i][SEC], grid[i1][SEC], [qb[0], H, qb[1]], [qa[0], H, qa[1]], soffit, 0, [0, -1, 0]);   // and that lid's underside
      if (A[2] > .02 || B[2] > .02) {                       // under an arch: the underside, from the skin's lifted edge back to the hall's wall (seen from either side)
        const p = [[A[0], A[2], A[1]], [B[0], B[2], B[1]], [qb[0], B[2], qb[1]], [qa[0], A[2], qa[1]]];
        m.quad(p[0], p[1], p[2], p[3], soffit, 0); m.quad(p[3], p[2], p[1], p[0], soffit, 0);
      }
      const len = Math.hypot(B[0] - A[0], B[1] - A[1]), k = Math.max(1, Math.ceil(len / 1.2));   // the skin cannot be walked through, nor the low ends of an arch
      for (let s = 0; s < k; s++) {
        const f = (s + .5) / k, x = A[0] + (B[0] - A[0]) * f, z = A[1] + (B[1] - A[1]) * f, b = A[2] + (B[2] - A[2]) * f, t = A[3] + (B[3] - A[3]) * f;
        if (b > 2.6) continue;
        o.solid(x - .7, 0, z - .7, x + .7, t, z + .7);        // from the ground up, also where the skin has begun to lift: a car is tested 0.7 m up, and would slip under
        if (b > .02) { const qx = clamp(x, x0, x1), qz = clamp(z, z0, z1), d = Math.hypot(qx - x, qz - z); for (let r = 1.2; r < d; r += 1.2) o.solid(x + (qx - x) * r / d - .7, 0, z + (qz - z) * r / d - .7, x + (qx - x) * r / d + .7, t, z + (qz - z) * r / d + .7); }
      }
    }
    const xs = pts.map(p => p[0]), zs = pts.map(p => p[1]);
    o.occl.push([Math.min(...xs) + 3, Math.min(...zs) + 3, Math.max(...xs) - 3, Math.max(...zs) - 3, l.H + 1, 0]);   // the ground at its foot sees less sky
  }

  function egg(o, d, base, inside) {                        // an egg from its crown down to `base`: rings of a stretched sphere, the lattice counted round and down it; `inside`: as seen from within (court.js)
    const m = o.mesh, [rx, ry, rz] = d.radii, [px, pz] = d.pos, cy = d.centre === undefined ? base : d.centre, kz = d.cells + 1000 * tone(d.color);
    const end = Math.acos(clamp((base - cy) / ry, -1, 1)), RINGS = Math.max(10, Math.round(30 * end / Math.PI * 1.6)), SEG = 80, cell = Math.PI * (rx + rz) / d.cells, rows = [];
    let arc = 0;
    for (let r = 0; r <= RINGS; r++) {
      const a = end * r / RINGS, sp = Math.sin(a), cp = Math.cos(a);
      if (r) { const a0 = end * (r - 1) / RINGS; arc += Math.hypot((rx + rz) / 2 * (sp - Math.sin(a0)), ry * (cp - Math.cos(a0))); }
      rows.push([sp, cp, arc / cell / d.cells]);
    }
    for (let r = 0; r < RINGS; r++) for (let s = 0; s < SEG; s++) {
      const P = [], N = [], K = [];
      for (const [rr, ss] of [[r + 1, s], [r + 1, s + 1], [r, s + 1], [r, s]]) {
        const [sp, cp, v] = rows[rr], ph = 2 * Math.PI * ss / SEG, c = Math.cos(ph), sn = Math.sin(ph);
        const nn = norm([sp * c / rx, cp / ry, sp * sn / rz]);
        P.push([px + rx * sp * c, cy + ry * cp, pz + rz * sp * sn]); N.push(inside ? [-nn[0], -nn[1], -nn[2]] : nn); K.push([ss / SEG, v, kz]);
      }
      m.quadN(P, N, K, inside ? 27 : 25);
    }
  }

  function lanterns(o, l, sh) {                             // three lanterns hang under each arch, between the skin and the hall's front (W6)
    for (const a of sh.arches) {
      const out = OUT[a.side], along = [Math.abs(out[1]), Math.abs(out[0])];
      const way = ((l.plan || {}).entrances || []).find(q => q.side === a.side), k = way ? .3 * way.width / a.width : .27;   // where there is a porch (court.js) they hang inside it, before the opening
      for (const f of [-k, 0, k]) {
        const x = a.at[0] + out[0] * 7 + along[0] * f * a.width, z = a.at[1] + out[1] * 7 + along[1] * f * a.width, top = a.height * Math.sqrt(1 - 4 * f * f), y = Math.max(4.2, top - 2.6);
        o.mesh.box(x - .05, y + .7, z - .05, x + .05, top, z + .05, o.C.metal, 9);
        o.mesh.box(x - .34, y, z - .34, x + .34, y + .7, z + .34, o.C.lantern, 2);
        o.mesh.box(x - .42, y + .7, z - .42, x + .42, y + .82, z + .42, o.C.metal, 9);
        o.lights.push([x, y - .2, z, 2.4, 1]);
      }
    }
  }

  return {
    TONES, egg,
    can: sh => sh.style === "ribbon" && sh.skin && sh.skin.points.length > 8,
    draw(o, l) {
      const sh = l.shell;
      skin(o, l, sh);
      for (const d of sh.domes || []) egg(o, d, 0);
      if (sh.roof_light) egg(o, sh.roof_light, sh.roof_light.base);
      lanterns(o, l, sh);
    },
  };
})();
