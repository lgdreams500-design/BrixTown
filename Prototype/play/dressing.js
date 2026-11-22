/* Street dressing in the playable slice (WORLD_MAP WLD-028 to WLD-034, F-MAP-DRESS level 2).
   The props come from the block files (Tools/worldgen/add_dressing_01.py); what the boards show today and whether the bunting
   is up come worked out from the day (D.dressing, written by build_play.py from World/rules/dressing.json).
   Dress.prop() draws the still parts once, with the town; Dress.apply() moves children with what they play on and turns readers
   to their board; Dress.draw() draws the moving parts (swing seats, seesaw planks) near the camera every frame.
   It runs inside play.js's world (world, solid, groundBase, C, hash, lettering ... are play.js's), so play.js must be loaded after. */
"use strict";
window.Dress = (() => {
  const PLAY = ["swing", "slide", "seesaw", "sandpit", "climb"];
  const RUBBER = [.5, .28, .22], SAND = [.72, .62, .44], WOOD = [.5, .32, .16], IRON = [.17, .18, .21], RED = [.58, .12, .1], GREEN = [.16, .3, .22];
  const BRONZE = [.3, .42, .36], PAINT = () => [C.teal, C.ochre, C.terracotta, C.blue];
  const pieces = [], bySpot = new Map(), boards = [];
  let motion = null, busy = new Set();
  const key = (x, z) => Math.round(x * 5) + "," + Math.round(z * 5);
  const dir = p => p.out ? p.out : p.along === "z" ? [1, 0] : [0, 1];
  const sideOf = o => o[1] < 0 ? "n" : o[1] > 0 ? "s" : o[0] < 0 ? "w" : "e";
  const post = (x, g, z, h, w, c, f) => world.box(x - w, g, z - w, x + w, g + h, z + w, c, f === undefined ? 9 : f);

  // ------------------------------------------------------------ the boards on walls: where they stand (windows() keeps clear)
  function board(l, p) {
    const s = sideOf(p.out), ns = s === "n" || s === "s", [x, z] = p.pos, c = ns ? x : z, hw = p.w / 2;
    if (p.kind === "hoarding") return { s, a0: c - hw, a1: c + hw, y0: Y_PAVE + .1, y1: 2.9 };
    const y0 = l.old ? OLD_BASE + .55 : 3.85, y1 = Math.min(y0 + 2.6, (l.H || 7) - .5);
    return { s, a0: c - hw, a1: c + hw, y0, y1 };
  }
  function covers(l, s, c, ya, yb) {                         // does a board on this wall cover a window centred at c, from ya to yb?
    if (!l.boards) { l.boards = (l.block.props || []).filter(p => p.lot === l.id && (p.kind === "advert" || p.kind === "hoarding")).map(p => board(l, p)); }
    return l.boards.some(B => B.s === s && c > B.a0 - .9 && c < B.a1 + .9 && yb > B.y0 - .2 && ya < B.y1 + .2);
  }
  function advertOf(p, k) {                                  // today's advert on board p (WLD-034); k picks the next ones on a long hoarding
    const d = D.dressing; if (!d) return null; const list = d.advert_list;
    return list[((d.adverts[p.id] || 0) + (k || 0) * 3) % list.length];
  }
  function paintBoard(S, a0, a1, y0, y1, ad, d0) {           // a painted panel with today's words on it
    fbox(world, S, a0, a1, y0, y1, d0, d0 + .04, ad.ground, 0, true, true);
    fbox(world, S, a0 + .12, a1 - .12, y0 + .12, y0 + .2, d0 + .04, d0 + .05, ad.ink, 0);
    fbox(world, S, a0 + .12, a1 - .12, y1 - .2, y1 - .12, d0 + .04, d0 + .05, ad.ink, 0);
    const cell = nameCell(ad.text); if (!cell) return;
    const n = S.ns ? [0, 0, S.out] : [S.out, 0, 0], fit = Math.min((y1 - y0) * .42, (a1 - a0 - .6) / (8 * cell.tw * (64 / 46) * .82));
    lettering(signs, SP.at(S, (a0 + a1) / 2, (y0 + y1) / 2, d0 + .06), n, fit, cell);
  }

  // ------------------------------------------------------------ the still parts, built once with the town
  function prop(p, b) {
    const [x, z] = p.pos, g = groundBase(x, z), k = p.kind;
    if (k === "playground") {                                 // soft red ground with a pale edge
      const hw = p.w / 2, hd = p.d / 2; world.flat(x - hw - .15, z - hd - .15, x + hw + .15, z + hd + .15, g + .015, C.cream, 0); world.flat(x - hw, z - hd, x + hw, z + hd, g + .025, RUBBER, 0);
    } else if (k === "swing") {                               // an A-frame at each end and a beam; the seats hang from it (draw)
      const n = p.seats, half = n * 1.1 / 2 + .3, col = pick(PAINT(), hash(p.id));
      for (const s of [-1, 1]) for (const v of [-.75, .75]) world.ybox(x + s * half, g, z + v * .5, .1, 2.45, .1, 0, col, 0);
      for (const s of [-1, 1]) world.box(x + s * half - .06, g + .9, z - .45, x + s * half + .06, g + .98, z + .45, col, 0);
      world.box(x - half - .05, g + 2.32, z - .06, x + half + .05, g + 2.44, z + .06, col, 0, { 2: [mul3(col, .7), 0] });
      for (const s of [-1, 1]) solid(x + s * half - .1, g, z - .5, x + s * half + .1, g + 2.4, z + .5);
      pieces.push({ p, kind: "swing", x, z, g, n });
    } else if (k === "slide") {                               // ladder up to a railed top, a shiny chute down to the ground
      const col = pick(PAINT(), hash(p.id, "c"));
      for (const v of [-.3, .3]) { world.box(x - 1.95, g, z + v - .04, x - 1.85, g + 2.3, z + v + .04, IRON, 9); world.box(x - 1.25, g, z + v - .04, x - 1.15, g + 2.3, z + v + .04, IRON, 9); }
      for (let r = .3; r < 1.6; r += .3) world.box(x - 1.95, g + r, z - .3, x - 1.85, g + r + .04, z + .3, IRON, 9);
      world.box(x - 1.95, g + 1.5, z - .36, x - 1.15, g + 1.58, z + .36, col, 0, { 2: [mul3(col, .7), 0] });
      world.box(x - 1.95, g + 2.2, z - .32, x - 1.15, g + 2.26, z + .32, IRON, 9);
      const top = [x - 1.15, g + 1.55], bot = [x + 2.1, g + .32], L = Math.hypot(bot[0] - top[0], bot[1] - top[1]), ang = Math.atan2(top[1] - bot[1], bot[0] - top[0]);
      const m = M4.mul(M4.mul(M4.t((top[0] + bot[0]) / 2, (top[1] + bot[1]) / 2, z), M4.ry(0)), M4.rz(-ang));
      world.obox(m, 0, 0, 0, L, .05, .5, [.72, .72, .74], 9);
      for (const v of [-.27, .27]) world.obox(m, 0, .1, v, L, .2, .04, col, 0);
      world.box(bot[0], g, z - .3, bot[0] + .45, g + .3, z + .3, [.72, .72, .74], 9);
      solid(x - 1.95, g, z - .36, x - 1.15, g + 2.3, z + .36);
      pieces.push({ p, kind: "slide", x, z, g });
    } else if (k === "seesaw") {                              // the plank rocks (draw); here the pivot
      world.box(x - .2, g, z - .25, x + .2, g + .42, z + .25, IRON, 9); world.box(x - .05, g + .42, z - .3, x + .05, g + .52, z + .3, IRON, 9);
      pieces.push({ p, kind: "seesaw", x, z, g });
    } else if (k === "sandpit") {                             // a wooden kerb round the sand, a bucket and a spade
      const h = (p.w || 3.4) / 2;
      for (const s of [-1, 1]) { world.box(x - h, g, z + s * h - .12, x + h, g + .28, z + s * h + .12, WOOD, 0, { 2: [mul3(WOOD, 1.2), 0] }); world.box(x + s * h - .12, g, z - h, x + s * h + .12, g + .28, z + h, WOOD, 0, { 2: [mul3(WOOD, 1.2), 0] }); }
      world.flat(x - h + .12, z - h + .12, x + h - .12, z + h - .12, g + .2, SAND, 0);
      world.box(x + .5, g + .2, z - .3, x + .72, g + .42, z - .08, C.blue, 0); world.ybox(x - .6, g + .2, z + .4, .08, .04, .5, .6, C.ochre, 0);
      for (let q = 0; q < 3; q++) world.ybox(x - .9 + q * .7, g + .2, z - .8 + .5 * hash(p.id, q), .4 + .2 * hash(q, p.id), .12, .3, hash(p.id, q, 3) * 3, mul3(SAND, .92), 0);
      pieces.push({ p, kind: "sandpit", x, z, g });
    } else if (k === "climber") {                             // a frame of painted bars with a deck halfway up
      const col = pick(PAINT(), hash(p.id, "cl")), h = 1.0;
      for (const sx of [-h, h]) for (const sz of [-h, h]) world.box(x + sx - .04, g, z + sz - .04, x + sx + .04, g + 1.9, z + sz + .04, col, 0);
      for (const y of [.5, 1.1, 1.85]) { for (const s of [-h, h]) { world.box(x - h, g + y, z + s - .03, x + h, g + y + .06, z + s + .03, col, 0); world.box(x + s - .03, g + y, z - h, x + s + .03, g + y + .06, z + h, col, 0); } }
      world.box(x - h, g + .84, z - h, x + h, g + .9, z + h, WOOD, 0, { 2: [mul3(WOOD, .7), 0] });
      solid(x - h, g, z - h, x + h, g + .9, z + h);
      pieces.push({ p, kind: "climb", x, z, g });
    } else if (k === "bandstand") {                           // an eight-sided stage with iron posts and a pointed roof
      const R = p.r, pt = (q, rr, y) => [x + Math.cos(q * Math.PI / 4) * rr, y, z + Math.sin(q * Math.PI / 4) * rr];
      for (let q = 0; q < 8; q++) {
        const o = [Math.cos((q + .5) * Math.PI / 4), 0, Math.sin((q + .5) * Math.PI / 4)];
        world.quad(pt(q, R, g), pt(q + 1, R, g), pt(q + 1, R, g + .7), pt(q, R, g + .7), C.stone, 6, o);
        world.quad(pt(q, R, g + .7), pt(q + 1, R, g + .7), [x, g + .7, z], [x, g + .7, z], WOOD, 0, [0, 1, 0]);
        const c = pt(q, R - .25, g + .7); world.box(c[0] - .07, g + .7, c[2] - .07, c[0] + .07, g + 3.4, c[2] + .07, C.cream, 0);
        const c2 = pt(q + 1, R - .25, g + 1.6); if (q !== 2) world.box(Math.min(c[0], c2[0]) - .03, g + 1.55, Math.min(c[2], c2[2]) - .03, Math.max(c[0], c2[0]) + .03, g + 1.62, Math.max(c[2], c2[2]) + .03, IRON, 9);
        world.quad(pt(q, R + .4, g + 3.4), pt(q + 1, R + .4, g + 3.4), [x, g + 5.0, z], [x, g + 5.0, z], C.teal, 0, [o[0], .8, o[2]]);
        world.quad(pt(q + 1, R + .4, g + 3.4), pt(q, R + .4, g + 3.4), [x, g + 3.4, z], [x, g + 3.4, z], C.cream, 0, [0, -1, 0]);
      }
      world.box(x - .1, g + 5.0, z - .1, x + .1, g + 5.5, z + .1, C.ochre, 0);
      for (let q = 0; q < 3; q++) { const c = pt(2.3 + q * .2, R + .25 + q * .45, 0); world.box(c[0] - .7, g + .47 * (2 - q) * .5, c[2] - .7, c[0] + .7, g + .7 - q * .23, c[2] + .7, C.stone, 6); }
      solid(x - R * .9, g, z - R * .9, x + R * .9, g + .7, z + R * .9);
    } else if (k === "noticeboard") {                         // two posts, a little roof, papers pinned up
      const ax = p.face === "x", s = p.sign || 1, f = ax ? [s, 0] : [0, s], r = ax ? [0, 1] : [1, 0];
      for (const u of [-.75, .75]) post(x + r[0] * u, g, z + r[1] * u, 2.1, .05, WOOD, 0);
      world.box(x - r[0] * .8 - f[0] * .02 - .02 * r[1], g + .9, z - r[1] * .8 - f[1] * .02 - .02 * r[0], x + r[0] * .8 + f[0] * .04 + .02 * r[1], g + 1.95, z + r[1] * .8 + f[1] * .04 + .02 * r[0], GREEN, 0);
      world.box(x - r[0] * .95 - .18 * Math.abs(f[0]), g + 2.0, z - r[1] * .95 - .18 * Math.abs(f[1]), x + r[0] * .95 + .18 * Math.abs(f[0]), g + 2.1, z + r[1] * .95 + .18 * Math.abs(f[1]), C.terracotta, 0, { 2: [mul3(C.terracotta, .7), 0] });
      for (let q = 0; q < 6; q++) { const u = -.6 + (q % 3) * .55 + .08 * hash(p.id, q), v = q < 3 ? 1.55 : 1.1, w = .18 + .08 * hash(q, p.id), c = pick([C.cream, [.86, .82, .7], [.8, .7, .5], [.7, .78, .8]], hash(p.id, q, 2));
        const cx = x + r[0] * u + f[0] * .05, cz = z + r[1] * u + f[1] * .05; world.box(cx - w * r[0] - .005, g + v, cz - w * r[1] - .005, cx + w * r[0] + .005, g + v + .3 + .1 * hash(q), cz + w * r[1] + .005, c, 0); }
      solid(x - .8 * r[0] - .06, g, z - .8 * r[1] - .06, x + .8 * r[0] + .06, g + 2.1, z + .8 * r[1] + .06);
      boards.push({ x, z, kind: "notice" });
    } else if (k === "sundial") {
      world.box(x - .5, g, z - .5, x + .5, g + .2, z + .5, C.stone, 6); world.ybox(x, g + .2, z, .4, .8, .4, Math.PI / 4, C.stone, 6);
      world.box(x - .38, g + 1.0, z - .38, x + .38, g + 1.05, z + .38, [.55, .45, .2], 9); world.box(x - .02, g + 1.05, z - .25, x + .02, g + 1.3, z + .1, [.55, .45, .2], 9);
      solid(x - .3, g, z - .3, x + .3, g + 1.05, z + .3);
    } else if (k === "well") {                                // a round stone well with a little roof, a rope and a bucket
      const R = .85, pt = (q, rr, y) => [x + Math.cos(q * Math.PI / 4) * rr, y, z + Math.sin(q * Math.PI / 4) * rr];
      for (let q = 0; q < 8; q++) { const o = [Math.cos((q + .5) * Math.PI / 4), 0, Math.sin((q + .5) * Math.PI / 4)];
        world.quad(pt(q, R, g), pt(q + 1, R, g), pt(q + 1, R, g + .75), pt(q, R, g + .75), C.stone, 6, o); world.quad(pt(q, R, g + .75), pt(q + 1, R, g + .75), pt(q + 1, R - .2, g + .75), pt(q, R - .2, g + .75), C.stone, 0, [0, 1, 0]);
        world.quad(pt(q, R - .2, g + .75), pt(q + 1, R - .2, g + .75), [x, g + .3, z], [x, g + .3, z], [.05, .06, .07], 9, [0, 1, 0]); }
      for (const s of [-1, 1]) post(x + s * .8, g + .7, z, 1.5, .06, WOOD, 0);
      world.box(x - .85, g + 1.75, z - .03, x + .85, g + 1.81, z + .03, WOOD, 0); world.box(x - .02, g + .95, z - .02, x + .02, g + 1.75, z + .02, C.cream, 0); world.box(x - .14, g + .8, z - .14, x + .14, g + 1.0, z + .14, WOOD, 0);
      for (const s of [-1, 1]) world.quad([x - 1.1, g + 2.2, z], [x + 1.1, g + 2.2, z], [x + 1.1, g + 1.9, z + s * .75], [x - 1.1, g + 1.9, z + s * .75], C.terracotta, 0, [0, .8, s]);
      solid(x - R, g, z - R, x + R, g + .75, z + R);
    } else if (k === "pergola") {                             // four posts, beams across, a vine over the top
      for (const sx of [-1.4, 1.4]) for (const sz of [-1.4, 1.4]) { post(x + sx, g, z + sz, 2.45, .09, C.cream, 0); solid(x + sx - .1, g, z + sz - .1, x + sx + .1, g + 2.4, z + sz + .1); }
      for (const s of [-1.4, 1.4]) world.box(x - 1.7, g + 2.4, z + s - .07, x + 1.7, g + 2.56, z + s + .07, C.cream, 0);
      for (let q = 0; q < 6; q++) world.box(x - 1.6 + q * .64 - .04, g + 2.56, z - 1.7, x - 1.6 + q * .64 + .04, g + 2.66, z + 1.7, C.cream, 0);
      for (let q = 0; q < 9; q++) world.ybox(x - 1.3 + (q % 3) * 1.3 + .3 * hash(p.id, q), g + 2.6, z - 1.3 + Math.floor(q / 3) * 1.3, .9, .35 + .2 * hash(q), .8, hash(q, p.id) * 3, mul3(C.olive, .9 + .3 * hash(p.id, q, 1)), 0);
      for (const sx of [-1.4, 1.4]) world.ybox(x + sx, g, z - 1.4, .5, 2.4, .3, 0, mul3(C.olive, .85), 0);
      world.box(x - .9, g + .36, z - .27, x + .9, g + .47, z + .27, WOOD, 0); for (const s of [-.6, .6]) world.box(x + s - .15, g, z - .22, x + s + .15, g + .36, z + .22, C.stone, 6);
      use(x - .4, g + .47, z, "sit", "Sit under the pergola", 0); use(x + .4, g + .47, z, "sit", "Sit under the pergola", 0);
    } else if (k === "statue") {                              // a figure in green bronze on a stone pedestal, steps round it
      const sm = p.small ? .55 : 1, base = p.small ? 0 : .5;
      if (!p.small) for (let q = 0; q < 2; q++) world.box(x - 2.2 + q * .45, g + q * .25, z - 2.2 + q * .45, x + 2.2 - q * .45, g + (q + 1) * .25, z + 2.2 - q * .45, C.stone, 6, { 2: [mul3(C.stone, 1.05), 0] });
      const ph = base + 2.0 * sm; world.box(x - .75 * sm, g + base, z - .75 * sm, x + .75 * sm, g + ph, z + .75 * sm, C.stone, 6); world.box(x - .9 * sm, g + ph, z - .9 * sm, x + .9 * sm, g + ph + .15 * sm, z + .9 * sm, C.stoneDark, 0, { 2: [C.stoneDark, 0] });
      const y = g + ph + .15 * sm, S = 1.25 * sm;
      for (const s of [-1, 1]) world.box(x + s * .14 * S - .1 * S, y, z - .1 * S, x + s * .14 * S + .1 * S, y + .85 * S, z + .1 * S, BRONZE, 9);
      world.box(x - .3 * S, y + .7 * S, z - .2 * S, x + .3 * S, y + 1.5 * S, z + .2 * S, BRONZE, 9); world.box(x - .36 * S, y + .55 * S, z - .24 * S, x + .36 * S, y + .95 * S, z + .24 * S, mul3(BRONZE, .9), 9);
      world.ybox(x, y + 1.52 * S, z, .26 * S, .3 * S, .26 * S, Math.PI / 4, BRONZE, 9);
      world.box(x - .44 * S, y + .85 * S, z - .08 * S, x - .32 * S, y + 1.45 * S, z + .08 * S, BRONZE, 9);
      const m = M4.mul(M4.t(x + .38 * S, y + 1.42 * S, z), M4.rz(-2.5)); world.obox(m, 0, .32 * S, 0, .12 * S, .65 * S, .12 * S, BRONZE, 9);
      world.box(x + .55 * S, y + 1.95 * S, z - .06 * S, x + .75 * S, y + 2.0 * S, z + .06 * S, [.55, .45, .2], 9);
      solid(x - (p.small ? .5 : 2.2), g, z - (p.small ? .5 : 2.2), x + (p.small ? .5 : 2.2), g + ph, z + (p.small ? .5 : 2.2));
    } else if (k === "kiosk") {                               // a green news kiosk: papers on the counter, its name on the roof
      const f = p.face || -1;
      world.box(x - 1.1, g, z - .95, x + 1.1, g + .95, z + .95, GREEN, 0); world.box(x - 1.1, g + 2.15, z - .95, x + 1.1, g + 2.4, z + .95, GREEN, 0);
      for (const sz of [-1, 1]) world.box(x - 1.1, g + .95, z + sz * .95 - .08, x + 1.1, g + 2.15, z + sz * .95 + .08, GREEN, 0);
      world.box(x - f * 1.1 - .08, g + .95, z - .95, x - f * 1.1 + .08, g + 2.15, z + .95, GREEN, 0);
      world.box(x - 1.45, g + 2.4, z - 1.3, x + 1.45, g + 2.52, z + 1.3, C.terracotta, 0, { 2: [mul3(C.terracotta, .7), 0] });
      world.box(x + f * 1.1 - .02, g + .95, z - .9, x + f * 1.1 + f * .3, g + 1.0, z + .9, WOOD, 0);
      for (let q = 0; q < 6; q++) world.box(x + f * 1.12 + f * (q % 2) * .14 - .06, g + 1.0, z - .75 + Math.floor(q / 2) * .55, x + f * 1.12 + f * (q % 2) * .14 + .06, g + 1.03 + .02 * q, z - .5 + Math.floor(q / 2) * .55, pick([C.cream, [.86, .82, .7], [.6, .2, .16]], hash(p.id, q)), 0);
      for (let q = 0; q < 5; q++) world.box(x + f * 1.11, g + 1.3 + (q % 2) * .32, z - .8 + q * .36, x + f * 1.13, g + 1.56 + (q % 2) * .32, z - .54 + q * .36, pick([C.cream, C.ochre, [.6, .2, .16], C.blue], hash(p.id, q, 9)), 0);
      const cell = nameCell("News"); if (cell) lettering(signs, [x + f * 1.46, g + 2.46, z], [f, 0, 0], .1, cell);
      fbox(world, { ns: false, fix: x + f * 1.1, out: f }, z - .7, z + .7, 2.17, 2.37, 0, .03, C.cream, 0);
      const c2 = nameCell("Papers"); if (c2) lettering(signs, [x + f * 1.14, g + 2.27, z], [f, 0, 0], .12, c2);
      solid(x - 1.1, g, z - .95, x + 1.1, g + 2.4, z + .95); boards.push({ x, z, kind: "kiosk" });
    } else if (k === "bunting") {                             // flags on a line strung from a pole to the fountain: up on market days only
      if (!D.dressing || !D.dressing.bunting) return true;
      const [tx, tz] = p.to, h0 = g + p.h0, h1 = groundBase(tx, tz) + p.h1, L = Math.hypot(tx - x, tz - z), n = Math.floor(L / .55), ux = (tx - x) / L, uz = (tz - z) / L, cols = [C.ochre, C.terracotta, C.teal, C.cream, C.blue];
      if (p.poles) { post(x, g, z, p.h0 + .2, .05, C.cream, 0); solid(x - .05, g, z - .05, x + .05, g + p.h0, z + .05); }
      const yAt = t => mix(h0, h1, t) - .7 * 4 * t * (1 - t);
      for (let q = 0; q < n; q++) {
        const t0 = q / n, t1 = (q + 1) / n, a = [x + ux * L * t0, yAt(t0), z + uz * L * t0], c = [x + ux * L * t1, yAt(t1), z + uz * L * t1];
        world.box(Math.min(a[0], c[0]) - .006, Math.min(a[1], c[1]) - .006, Math.min(a[2], c[2]) - .006, Math.max(a[0], c[0]) + .006, Math.max(a[1], c[1]) + .006, Math.max(a[2], c[2]) + .006, IRON, 9);
        const m = [(a[0] + c[0]) / 2, (a[1] + c[1]) / 2 - .32, (a[2] + c[2]) / 2], col = cols[q % cols.length], nx = [-uz, 0, ux];
        world.quad(a, c, m, m, col, 0, nx); world.quad(c, a, m, m, col, 0, [-nx[0], 0, -nx[2]]);
      }
    } else if (k === "planter") {
      const h = p.big ? .75 : .48, s = p.big ? .7 : .45, col = p.big ? C.stone : WOOD;
      world.box(x - s, g, z - s, x + s, g + h, z + s, col, p.big ? 6 : 0, { 2: [mul3(col, 1.1), 0] }); world.flat(x - s + .08, z - s + .08, x + s - .08, z + s - .08, g + h + .005, C.soil, 0);
      for (let q = 0; q < 4; q++) world.ybox(x + (q % 2 - .5) * s * .9, g + h, z + (Math.floor(q / 2) - .5) * s * .9, s * .8, .3 + .35 * hash(p.id, q), s * .8, hash(q, p.id) * 3, mul3(C.olive, .9 + .3 * hash(p.id, q)), 0);
      if (D.flowers && D.flowers.size > .2) for (let q = 0; q < 5; q++) world.box(x + (hash(p.id, q, 5) - .5) * s * 1.4 - .06, g + h + .45, z + (hash(p.id, q, 6) - .5) * s * 1.4 - .06, x + (hash(p.id, q, 5) - .5) * s * 1.4 + .06, g + h + .58, z + (hash(p.id, q, 6) - .5) * s * 1.4 + .06, D.flowers.colors[(q + Math.floor(hash(p.id) * 9)) % D.flowers.colors.length], 0);
      solid(x - s, g, z - s, x + s, g + h, z + s);
    } else if (k === "bin") {
      world.ybox(x, g, z, .42, .82, .42, Math.PI / 8, GREEN, 0); world.ybox(x, g + .82, z, .5, .08, .5, Math.PI / 8, mul3(GREEN, .8), 0); world.box(x - .12, g + .9, z - .12, x + .12, g + .95, z + .12, IRON, 9);
      solid(x - .25, g, z - .25, x + .25, g + .9, z + .25);
    } else if (k === "bikerack") {                            // three hoops, now and then a bicycle leant on one
      const ax = p.along === "x";
      for (let q = -1; q <= 1; q++) { const cx = x + (ax ? q * .8 : 0), cz = z + (ax ? 0 : q * .8);
        for (const s of [-.32, .32]) world.box(cx + (ax ? 0 : s) - .03, g, cz + (ax ? s : 0) - .03, cx + (ax ? 0 : s) + .03, g + .8, cz + (ax ? s : 0) + .03, IRON, 9);
        world.box(cx - (ax ? .03 : .35), g + .8, cz - (ax ? .35 : .03), cx + (ax ? .03 : .35), g + .86, cz + (ax ? .35 : .03), IRON, 9);
        if (hash(p.id, q) < .45) { const col = pick([C.blue, RED, C.teal, C.ochre], hash(p.id, q, 2)), yaw = ax ? Math.PI / 2 : 0;
          for (const s of [-.55, .55]) world.ybox(cx + (ax ? .12 : s), g, cz + (ax ? s : .12), ax ? .05 : .66, .66, ax ? .66 : .05, 0, [.08, .08, .09], 9);
          world.ybox(cx + (ax ? .12 : 0), g + .5, cz + (ax ? 0 : .12), ax ? .04 : 1.0, .06, ax ? 1.0 : .04, 0, col, 0); world.ybox(cx + (ax ? .12 : -.3), g + .8, cz + (ax ? -.3 : .12), .1, .05, .22, yaw, [.1, .08, .07], 0); } }
      solid(x - (ax ? 1.2 : .1), g, z - (ax ? .1 : 1.2), x + (ax ? 1.2 : .1), g + .86, z + (ax ? .1 : 1.2));
    } else if (k === "postbox") {                             // a red pillar box
      world.ybox(x, g, z, .46, 1.15, .46, Math.PI / 8, RED, 0); world.ybox(x, g + 1.15, z, .52, .12, .52, Math.PI / 8, mul3(RED, .85), 0); world.ybox(x, g + 1.27, z, .3, .08, .3, Math.PI / 8, mul3(RED, .85), 0);
      const o = dir(p); world.box(x - o[0] * .25 - .12 * Math.abs(o[1]) - .01, g + .92, z - o[1] * .25 - .12 * Math.abs(o[0]) - .01, x - o[0] * .23 + .12 * Math.abs(o[1]) + .01, g + .96, z - o[1] * .23 + .12 * Math.abs(o[0]) + .01, [.05, .05, .05], 9);
      solid(x - .25, g, z - .25, x + .25, g + 1.3, z + .25);
    } else if (k === "phonebox") {                            // a red telephone box with glazed sides and a crown
      world.box(x - .5, g, z - .5, x + .5, g + .15, z + .5, RED, 0);
      for (const sx of [-.45, .45]) for (const sz of [-.45, .45]) world.box(x + sx - .06, g, z + sz - .06, x + sx + .06, g + 2.35, z + sz + .06, RED, 0);
      for (const [ax, s] of [[0, -1], [0, 1], [1, -1], [1, 1]]) { const fx = ax ? x + s * .44 : x, fz = ax ? z : z + s * .44;
        world.box(fx - (ax ? .015 : .38), g + .85, fz - (ax ? .38 : .015), fx + (ax ? .015 : .38), g + 2.05, fz + (ax ? .38 : .015), [.36, .44, .48], 9);
        for (const y of [1.25, 1.65]) world.box(fx - (ax ? .025 : .38), g + y, fz - (ax ? .38 : .025), fx + (ax ? .025 : .38), g + y + .04, fz + (ax ? .38 : .025), RED, 0); }
      world.box(x - .55, g + 2.35, z - .55, x + .55, g + 2.6, z + .55, RED, 0); world.ybox(x, g + 2.6, z, .9, .14, .9, 0, mul3(RED, .9), 0); world.box(x - .12, g + 2.74, z - .12, x + .12, g + 2.84, z + .12, mul3(RED, .9), 0);
      world.box(x - .2, g + 1.4, z - .2, x + .2, g + 1.6, z + .2, [1, .85, .6], 2);
      solid(x - .5, g, z - .5, x + .5, g + 2.6, z + .5);
    } else if (k === "bollard") {
      world.ybox(x, g, z, .2, .78, .2, Math.PI / 8, IRON, 9); world.ybox(x, g + .78, z, .24, .08, .24, Math.PI / 8, IRON, 9);
      solid(x - .1, g, z - .1, x + .1, g + .8, z + .1);
    } else if (k === "timetable") {                           // a times board on a post at the tram stop
      post(x, g, z, 2.1, .05, C.teal, 0); world.box(x - .38, g + 1.2, z - .04, x + .38, g + 2.0, z + .04, C.teal, 0);
      for (let q = 0; q < 6; q++) world.box(x - .3, g + 1.3 + q * .11, z + .04, x + .3 - .2 * hash(p.id, q), g + 1.36 + q * .11, z + .05, C.cream, 0);
      const cell = nameCell("Tram"); if (cell) lettering(signs, [x, g + 1.9, z + .05], [0, 0, 1], .1, cell);
      solid(x - .06, g, z - .06, x + .06, g + 2.1, z + .06);
    } else if (k === "advert" || k === "hoarding") {          // today's advert painted on a blank wall, or a timber hoarding pasted with posters
      const l = b.lots.find(q => q.id === p.lot); if (!l) return true;
      const B = board(l, p), s = B.s, ns = s === "n" || s === "s", S = { ns, fix: ns ? (s === "n" ? l.rect[1] : l.rect[3]) : (s === "w" ? l.rect[0] : l.rect[2]), out: s === "n" || s === "w" ? -1 : 1 };
      if (k === "advert") { const ad = advertOf(p); if (ad) paintBoard(S, B.a0, B.a1, B.y0, B.y1, ad, .02); }
      else {
        fbox(world, S, B.a0, B.a1, B.y0, B.y1, 0, .1, [.42, .3, .18], 0, true, true);
        for (let a = B.a0; a < B.a1; a += .2) fbox(world, S, a, a + .02, B.y0, B.y1, .1, .11, [.3, .2, .12], 0);
        const n = Math.max(1, Math.floor((B.a1 - B.a0) / 3.6)), w = (B.a1 - B.a0) / n;
        for (let q = 0; q < n; q++) { const ad = advertOf(p, q); if (ad) paintBoard(S, B.a0 + q * w + .25, B.a0 + (q + 1) * w - .25, B.y0 + .45, B.y1 - .3, ad, .11); }
        fbox(world, S, B.a0 - .1, B.a1 + .1, B.y1, B.y1 + .12, 0, .2, [.3, .2, .12], 0, true);
      }
    } else return false;
    return true;
  }

  // ------------------------------------------------------------ after the town is built: which spot belongs to which piece
  function init() {
    motion = Object.assign({ swing_period_s: 2.9, swing_angle_deg: 32, seesaw_period_s: 3.4, seesaw_angle_deg: 14, slide_cycle_s: 9, climb_bob_s: 6,
      player_swing_build_s: 3, player_slide_climb_s: 2.6, player_slide_down_s: 1.3 }, D.dressing ? D.dressing.motion : {});
    for (const l of LOTS) for (const u of PLAY) for (const [k, s] of ((l.objects || {})[u] || []).entries()) {
      let best = null, bd = 1e9;
      for (const pc of pieces) { if (pc.kind !== u) continue; const d = Math.hypot(pc.x - s[0], pc.z - s[1]); if (d < bd) { bd = d; best = pc; } }
      if (best && bd < 8) bySpot.set(key(s[0], s[1]), { pc: best, k, sx: s[0], sz: s[1], use: u });
    }
    // the player can have a go too (WLD-041, F-OBJ-POINTS level 1: residents and players share the places): a use point at
    // each swing seat and seesaw end, one at the foot of each slide's ladder and by each climbing frame, two at each sandpit
    const LABEL = { swing: "Have a go on the swing", seesaw: "Sit on the seesaw", slide: "Go down the slide", sandpit: "Play in the sandpit", climb: "Climb the frame" };
    const once = new Set();
    for (const sp of bySpot.values()) {
      const pc = sp.pc, g = pc.g, u = sp.use;
      if ((u === "slide" || u === "climb") && once.has(pc.p.id)) continue;
      if (u === "sandpit" && [...once].filter(q => q.startsWith(pc.p.id + "#")).length >= 2) continue;
      once.add(u === "sandpit" ? pc.p.id + "#" + sp.k : pc.p.id);
      let x = sp.sx, z = sp.sz, yaw = 0;
      if (u === "swing") { x = pc.x + (sp.k - (pc.n - 1) / 2) * 1.1; z = pc.z + .75; yaw = yawOf(0, -1); }
      else if (u === "seesaw") { x = pc.x + (sp.sx < pc.x ? -1 : 1) * 1.6; z = pc.z + .7; yaw = yawOf(0, -1); }
      else if (u === "slide") { x = pc.x - 2.35; z = pc.z; yaw = yawOf(1, 0); }
      else if (u === "climb") { x = pc.x + 1.45; z = pc.z; yaw = yawOf(-1, 0); }
      use(x, g, z, "play", LABEL[u], yaw, { sp });
    }
    Object.assign(DOING, { swing: "on the swings", slide: "on the slide", seesaw: "on the seesaw", sandpit: "in the sandpit", climb: "on the climbing frame", notices: "reading the notices", paper: "buying a paper" });
    Object.assign(GOING, { swing: "off to the swings", slide: "off to the slide", seesaw: "off to the seesaw", sandpit: "off to the sandpit", climb: "off to the climbing frame", notices: "going to read the notices", paper: "going for a paper" });
  }

  // ------------------------------------------------------------ residents: children with what they play on, readers at their board
  const yawTo = (fx, fz, tx, tz) => yawOf(tx - fx, tz - fz);
  function apply(i, r, t) {
    if (!r || r.walking) return r;
    const act = D.acts[r.act];
    if (act === "notices" || act === "paper") {                // face the board or the kiosk
      let best = null, bd = 6; for (const q of boards) { const d = Math.hypot(q.x - r.x, q.z - r.z); if (d < bd) { bd = d; best = q; } }
      if (best) r.yaw = yawTo(r.x, r.z, best.x, best.z); return r;
    }
    if (!PLAY.includes(act)) return r;
    const sp = bySpot.get(key(r.x, r.z)); if (!sp) return r;
    const pc = sp.pc, g = pc.g, ph = hash(i, "play") * 6.283;
    busy.add(pc.p.id + ":" + sp.k); busy.add(pc.p.id); busy.add(key(sp.sx, sp.sz));   // the seat moves, and the player cannot take this place
    if (pc.kind === "swing") {
      const th = swingAngle(pc, sp.k, t), a = (sp.k - (pc.n - 1) / 2) * 1.1, L = 1.85;
      return Object.assign(r, { x: pc.x + a, z: pc.z + Math.sin(th) * L, y: g + 2.36 - Math.cos(th) * L + .04, yaw: yawOf(0, 1), pose: 2 });
    }
    if (pc.kind === "seesaw") {
      const s = sp.sx < pc.x ? -1 : 1, phi = seesawAngle(pc, t);
      return Object.assign(r, { x: pc.x + s * 1.6, z: pc.z, y: g + .52 + s * 1.6 * Math.sin(phi) + .06, yaw: yawOf(-s, 0), pose: 2 });
    }
    if (pc.kind === "slide") {
      const T = motion.slide_cycle_s, u = ((t / T) + sp.k / 4 + hash(i, "sl") * .1) % 1, x = pc.x, z = pc.z;
      if (u < .3) { const q = u / .3; return Object.assign(r, { x: x - 1.55, z, y: g + q * 1.55, yaw: yawOf(1, 0), pose: 1, walking: true, ph: (t * 1.7) % 1 }); }
      if (u < .38) return Object.assign(r, { x: x - 1.25, z, y: g + 1.6, yaw: yawOf(1, 0), pose: 2 });
      if (u < .52) { const q = (u - .38) / .14, e = q * q; return Object.assign(r, { x: mix(x - 1.15, x + 2.1, e), z, y: mix(g + 1.6, g + .36, e), yaw: yawOf(1, 0), pose: 2 }); }
      if (u < .6) return Object.assign(r, { x: x + 2.4, z, y: g, yaw: yawOf(1, 0), pose: 0 });
      const q = (u - .6) / .4, px = mix(x + 2.4, x - 1.9, q), pz = z + .9 * Math.sin(Math.PI * q);
      return Object.assign(r, { x: px, z: pz, y: g, yaw: yawOf(-1, Math.cos(Math.PI * q) * .6), pose: 1, walking: true, ph: (t * 1.7) % 1 });
    }
    if (pc.kind === "sandpit") return Object.assign(r, { y: g + .3, yaw: yawTo(r.x, r.z, pc.x, pc.z), pose: 2 });
    if (pc.kind === "climb") {
      const b = Math.sin(t * 6.283 / motion.climb_bob_s + ph);
      return Object.assign(r, { y: g + .9 + .06 * b, yaw: r.yaw + .5 * Math.sin(t * .3 + ph), pose: 0 });
    }
    return r;
  }
  function swingAngle(pc, k, t) {
    const A = motion.swing_angle_deg * Math.PI / 180 * (.75 + .25 * hash(pc.p.id, k));
    return A * Math.sin(t * 6.283 / (motion.swing_period_s * (.92 + .16 * hash(k, pc.p.id))) + hash(pc.p.id, k, "ph") * 6.283);
  }
  function seesawAngle(pc, t) { return motion.seesaw_angle_deg * Math.PI / 180 * Math.sin(t * 6.283 / motion.seesaw_period_s + hash(pc.p.id) * 6.283); }

  // ------------------------------------------------------------ the player on the playground (WLD-041)
  const swingOf = new Map();                                 // a seat the player is on: its angle, so the seat goes with them
  let taken = new Set();                                     // places children were on last frame
  const takenBy = u => u.sp && u.sp.use !== "slide" && u.sp.use !== "climb" && taken.has(key(u.sp.sx, u.sp.sz));   // a slide takes turns, a frame has room
  function playerAt(st, t) {                                 // where the player is and how they sit or stand, t seconds into the go
    const sp = st.sp, pc = sp.pc, g = pc.g, e = t - st.t0, u = sp.use;
    busy.add(pc.p.id); busy.add(pc.p.id + ":" + sp.k);
    if (u === "swing") {                                     // still at first, swinging higher as they pump
      const th = swingAngle(pc, sp.k, t) * Math.min(1, e / motion.player_swing_build_s), a = (sp.k - (pc.n - 1) / 2) * 1.1, L = 1.85;
      swingOf.set(pc.p.id + ":" + sp.k, th);
      return { x: pc.x + a, z: pc.z + Math.sin(th) * L, y: g + 2.36 - Math.cos(th) * L + .04, yaw: yawOf(0, 1), pose: 2 };
    }
    if (u === "seesaw") { const s = sp.sx < pc.x ? -1 : 1, phi = seesawAngle(pc, t); return { x: pc.x + s * 1.6, z: pc.z, y: g + .52 + s * 1.6 * Math.sin(phi) + .06, yaw: yawOf(-s, 0), pose: 2 }; }
    if (u === "slide") {                                     // up the ladder, a moment at the top, down the chute, off at the bottom: one go
      const c = motion.player_slide_climb_s, dn = motion.player_slide_down_s, x = pc.x, z = pc.z;
      if (e < c) return { x: x - 1.55, z, y: g + e / c * 1.55, yaw: yawOf(1, 0), pose: 1, ph: (t * 1.7) % 1 };
      if (e < c + .5) return { x: x - 1.25, z, y: g + 1.6, yaw: yawOf(1, 0), pose: 2 };
      if (e < c + .5 + dn) { const q = (e - c - .5) / dn, k = q * q; return { x: mix(x - 1.15, x + 2.1, k), z, y: mix(g + 1.6, g + .36, k), yaw: yawOf(1, 0), pose: 2 }; }
      return { x: x + 2.6, z, y: g, yaw: yawOf(1, 0), pose: 0, done: true };
    }
    if (u === "sandpit") return { x: sp.sx, z: sp.sz, y: g + .3, yaw: yawOf(pc.x - sp.sx, pc.z - sp.sz), pose: 2 };
    if (u === "climb") return { x: pc.x + .35, z: pc.z, y: g + .9 + .04 * Math.sin(e * 1.3), yaw: yawOf(1, 0) + .4 * Math.sin(e * .35), pose: 0 };
    return { x: sp.sx, z: sp.sz, y: g, yaw: 0, pose: 0, done: true };
  }
  function playerOff(st) {                                   // where the player steps off to
    const sp = st.sp, pc = sp.pc;
    if (sp.use === "swing") return [pc.x + (sp.k - (pc.n - 1) / 2) * 1.1, pc.z + 1.1];
    if (sp.use === "seesaw") return [pc.x + (sp.sx < pc.x ? -1 : 1) * 1.6, pc.z + .8];
    if (sp.use === "slide") return [pc.x - 2.35, pc.z];
    if (sp.use === "climb") return [pc.x + 1.45, pc.z];
    return [sp.sx, sp.sz + .9];
  }

  // ------------------------------------------------------------ the moving parts, every frame near the camera
  function draw(dyn, eye, t) {
    for (const pc of pieces) {
      if (Math.abs(pc.x - eye[0]) > 70 || Math.abs(pc.z - eye[2]) > 70) continue;
      if (pc.kind === "swing") for (let k = 0; k < pc.n; k++) {
        const own = swingOf.get(pc.p.id + ":" + k), on = busy.has(pc.p.id + ":" + k), th = own !== undefined ? own : on ? swingAngle(pc, k, t) : .04 * Math.sin(t * .9 + k), a = pc.x + (k - (pc.n - 1) / 2) * 1.1;
        const m = M4.mul(M4.mul(M4.t(a, pc.g + 2.36, pc.z), M4.rx(th)), M4.t(0, 0, 0));
        for (const s of [-.2, .2]) dyn.obox(m, s, -.9, 0, .02, 1.8, .02, IRON, 9);
        dyn.obox(m, 0, -1.84, 0, .48, .05, .2, [.18, .14, .1], 0);
      }
      else if (pc.kind === "seesaw") {
        const on = busy.has(pc.p.id), phi = on ? seesawAngle(pc, t) : -.24;   // anyone on either end rocks it
        const m = M4.mul(M4.t(pc.x, pc.g + .52, pc.z), M4.rz(phi)), col = pick(PAINT(), hash(pc.p.id, "ss"));
        dyn.obox(m, 0, 0, 0, 3.6, .07, .3, col, 0);
        for (const s of [-1.45, 1.45]) { dyn.obox(m, s, .2, 0, .05, .35, .05, IRON, 9); dyn.obox(m, s, .38, 0, .05, .05, .4, IRON, 9); }
      }
    }
    taken = busy; busy = new Set(); swingOf.clear();
  }

  return { prop, init, apply, draw, covers, PLAY, playerAt, playerOff, takenBy };
})();
