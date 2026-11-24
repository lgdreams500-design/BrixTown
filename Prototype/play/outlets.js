"use strict";
// The outlets of a mall whose inside is built (WORLD_MAP WLD-063, ADR-0039; the owner, 2026-11-24, with a picture of a mall's
// shop fronts: "instead of generic flat walls can you build these outlets in the mall ... cloths shops, toy shop, electronics
// shops, shoes shops, guitars, etc etc. food outlets entertainment rides etc."). Which kind each unit is, what it is called and
// what stands in a kind is data: World/rules/mall_outlets.json, copied into the lot's plan by Tools/worldgen/mall_inside_01.py
// (`plan.outlets`, each unit's `outlet` and `name`, `plan.rides`). This file only draws them, for court.js:
//
//   the front    a pier each side, a sign band in the outlet's colour with its name lit on it, slim posts, a display plinth
//                either side of an open way in (no glass: the slice has no see-through faces)
//   the room     as deep as the unit, to 9 m: its own floor, pale side walls, a back wall in the sign's colour, light panels
//                in the ceiling. It can be walked into, on every floor
//   what is in   three walls of fixtures, things standing in the middle and things in the window, each built of plain boxes
//                and a few faceted balls in the town's muted colours (ART_DIRECTION 2.2): rails of clothes, shelves of shoes,
//                toys, books, guitars on the wall, lit screens, glass cases, a servery with lit menu boards, tables and chairs
//   rides        a carousel and coin rides on the court floor; they stand still, and can be sat on
const Outlets = (() => {
  const SIGN = { terracotta: [.5, .25, .19], ochre: [.56, .43, .2], teal: [.2, .37, .38], blue: [.27, .36, .52], olive: [.33, .37, .2], plum: [.35, .25, .38] };
  const GOODS = [[.56, .26, .2], [.6, .46, .2], [.2, .4, .42], [.28, .38, .58], [.36, .42, .22], [.4, .27, .42], [.78, .72, .6], [.7, .36, .3], [.24, .24, .27], [.8, .62, .3]];
  const SCREEN = [.3, .4, .9995], GLOW = [.3, .4, .97];        // lit faces (material 1): a cool white screen or light panel, a warm glow
  const DARK = [.14, .14, .16], BROWN = [.5, .36, .22], LEAF = [.27, .37, .18];
  const mul = (c, k) => [c[0] * k, c[1] * k, c[2] * k], mix = (a, b, f) => [a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f, a[2] + (b[2] - a[2]) * f];
  const FALLBACK = { label: "Shop", sign: "ochre", floor: "tiles", walls: ["shelves:boxes", "shelves:boxes", "shelves:boxes"], middle: ["table:boxes"], window: ["stack:boxes", "plants"] };

  function front(T) {
    const { m, o, C, S, u, fy, top, k, Lf } = T, spec = T.spec || FALLBACK, sign = SIGN[spec.sign] || SIGN.ochre, seed = u.id;
    const th = Math.atan2(-S.t[1], S.t[0]), turn = (-S.n[0] * Math.sin(th) - S.n[1] * Math.cos(th)) > 0 ? 1 : -1;   // turn: which way a box's own depth runs, into the room or out of it
    const depth = T.depth || { least: 4.2, most: 9 }, D = Math.max(depth.least, Math.min(depth.most, u.rect[3] - u.rect[1] - .2));
    const a0 = .9, a1 = Lf - .9, mid = Lf / 2, door = Math.min(3.4, (a1 - a0) * .28), staffed = T.staffed, food = spec.trade === "food";
    const P = (a, y, v) => S.at(a, y, -v);                      // a metres along the front, v metres into the room
    const xz = (a, v) => { const p = P(a, 0, v); return [p[0], p[2]]; };
    const rnd = (...q) => o.hash(seed, ...q), good = (...q) => GOODS[Math.floor(rnd(...q) * GOODS.length) % GOODS.length];
    const bx = (a, v, y0, sa, sy, sv, col, f, yaw) => { const p = P(a, 0, v); m.ybox(p[0], y0, p[2], sa, sy, sv, th - turn * (yaw || 0), col, f === undefined ? 19 : f); };
    const big = (a, v, y, r, col, key) => { const p = P(a, 0, v); T.blob(p[0], y, p[2], r, col, seed + key); };   // a faceted ball as the trees' crowns are
    const ball = (a, v, y, r, col, key) => {                    // and a lighter one, of 18 faces, for goods
      const p = P(a, 0, v), q = (i, j) => { const g = 1.0472 * i + j * .5, t = [0, 1.05, 2.1, 3.1416][j]; return [p[0] + r * Math.sin(t) * Math.cos(g), y + r * Math.cos(t), p[2] + r * Math.sin(t) * Math.sin(g)]; };
      for (let j = 0; j < 3; j++) for (let i = 0; i < 6; i++) { const c = mul(col, .85 + .3 * rnd(key, i, j)), A = q(i, j), B = q(i + 1, j), Cc = q(i + 1, j + 1), Dd = q(i, j + 1), n = [(A[0] + Cc[0]) / 2 - p[0], (A[1] + Cc[1]) / 2 - y, (A[2] + Cc[2]) / 2 - p[2]];
        j ? m.quad(A, B, Cc, Dd, c, 19, n) : m.quad(Dd, Cc, B, A, c, 19, n); } };
    const disc = (a, v, y0, y1, r, colS, colT, n) => { const p = P(a, 0, v); T.prism(T.round(p[0], p[2], r, r, n || 10), y0, y1, colS, colT || colS, 19, 19); };
    const block = (a, v, r, h) => { const p = P(a, 0, v); o.solid(p[0] - r, fy, p[2] - r, p[0] + r, fy + h, p[2] + r); };
    const quad = (p, col, f, n) => m.quad(p[0], p[1], p[2], p[3], col, f, n);
    const inward = [-S.n[0], 0, -S.n[1]], along = [S.t[0], 0, S.t[1]], face = [S.n[0], 0, S.n[1]];
    const wb = (W, q, r, y0, len, sy, dep, col, f) => { const a = W.a0 + W.da * q + W.na * r, v = W.v0 + W.dv * q + W.nv * r; W.side ? bx(a, v, y0, dep, sy, len, col, f) : bx(a, v, y0, len, sy, dep, col, f); };
    const wp = (W, q, r) => [W.a0 + W.da * q + W.na * r, W.v0 + W.dv * q + W.nv * r];

    // ---- the room: floor, walls, ceiling and its light panels
    const wallC = [.95, .92, .85], backC = mix(sign, [.92, .88, .78], .5), ceilC = [.93, .91, .85], fl = spec.floor === "boards" ? [mul(C.wood, 1.25), 15] : [mul(T.floorC, .97), 16];
    quad([P(a0, fy + .004, 0), P(a1, fy + .004, 0), P(a1, fy + .004, D), P(a0, fy + .004, D)], fl[0], fl[1], [0, 1, 0]);
    quad([P(a0, fy, 0), P(a0, fy, D), P(a0, fy + 3.5, D), P(a0, fy + 3.5, 0)], wallC, 3, along);
    quad([P(a1, fy, 0), P(a1, fy, D), P(a1, fy + 3.5, D), P(a1, fy + 3.5, 0)], wallC, 3, mul(along, -1));
    quad([P(a0, fy, D), P(a1, fy, D), P(a1, fy + 3.5, D), P(a0, fy + 3.5, D)], backC, 3, face);
    quad([P(a0, fy + 3.5, 0), P(a1, fy + 3.5, 0), P(a1, fy + 3.5, D), P(a0, fy + 3.5, D)], ceilC, 3, [0, -1, 0]);
    quad([P(a0 + .4, fy + 3.12, D - .03), P(a1 - .4, fy + 3.12, D - .03), P(a1 - .4, fy + 3.3, D - .03), P(a0 + .4, fy + 3.3, D - .03)], GLOW, 1, face);   // a line of light along the top of the back wall
    for (const v of [D * .32, D * .72]) quad([P(mid - (a1 - a0) * .3, fy + 3.49, v - .3), P(mid + (a1 - a0) * .3, fy + 3.49, v - .3), P(mid + (a1 - a0) * .3, fy + 3.49, v + .3), P(mid - (a1 - a0) * .3, fy + 3.49, v + .3)], SCREEN, 1, [0, -1, 0]);
    if (T.lamp) { const p = P(mid, 0, D * .45); T.lamp(p[0], fy + 3.3, p[2], -26); }   // the room's own light, for the pool it lays (play.js roomLight)
    T.solidAlong(xz(a0, D), xz(a1, D), fy, top, .45);
    for (const a of [a0, a1]) T.solidAlong(xz(a, .5), xz(a, D), fy, top, .45);
    if (k) for (let a = a0 + .7; a < a1; a += 1.4) for (let v = .7; v < D; v += 1.4) { const p = P(a, 0, v); o.solid(p[0] - .95, fy - T.T, p[2] - .95, p[0] + .95, fy, p[2] + .95); }   // upstairs the room has its own floor to stand on

    // ---- the front: piers, the sign band and the name on it, posts, a display plinth either side of the way in
    T.boxQ(S, 0, .9, fy, top, 0, .18, T.plaster, 3); T.boxQ(S, Lf - .9, Lf, fy, top, 0, .18, T.plaster, 3);
    T.solidAlong(xz(0, 0), xz(.9, 0), fy, top, .45); T.solidAlong(xz(Lf - .9, 0), xz(Lf, 0), fy, top, .45);
    T.wallQ(S, a0, a1, fy + 3.5, fy + 4.5, .12, sign, 3); T.wallQ(S, a0, a1, fy + 4.5, top, .02, T.plaster, 3); T.wallQ(S, a0, a1, fy + 3.42, fy + 3.5, .12, mul(sign, .6), 3);
    quad([S.at(a0, fy + 3.42, 0), S.at(a1, fy + 3.42, 0), S.at(a1, fy + 3.42, .12), S.at(a0, fy + 3.42, .12)], T.dark, 3, [0, -1, 0]);
    quad([S.at(a0, fy + 3.5, 0), S.at(a1, fy + 3.5, 0), S.at(a1, fy + 3.5, -.02), S.at(a0, fy + 3.5, -.02)], T.dark, 3, [0, -1, 0]);
    if (o.name) o.name(u.name || spec.label, S.at(mid, fy + 4.0, .15), face, .5);
    const zones = [[a0 + .12, mid - door / 2 - .15], [mid + door / 2 + .15, a1 - .12]];
    zones.forEach((z, i) => { const len = z[1] - z[0], c = (z[0] + z[1]) / 2; if (len < 1.2) return;
      bx(c, .6, fy, len, .4, .95, [.7, .66, .58]); T.solidAlong(xz(z[0] + .3, .6), xz(z[1] - .3, .6), fy, fy + 1.3, .5);
      for (const a of z) bx(a, .08, fy, .09, 3.42, .09, C.metal); bx(c, .08, fy + 2.75, len, .07, .07, C.metal);
      inWindow(z, (spec.window || [])[i] || "plants", "w" + i); });

    // ---- what stands in it
    function items(W, q0, q1, r, y, what, key, cap) {             // a row of goods along a shelf or a table
      const len = q1 - q0; if (len < .3) return;
      if (what === "books") { const n = Math.min(cap || 40, Math.max(3, Math.round(len / .34))); for (let i = 0; i < n; i++) wb(W, q0 + (i + .5) * len / n, r, y, len / n * .9, .3 + .14 * rnd(key, i), .22, good(key, i, "c")); return; }   // books by the armful, each run its own colour
      const step = { folded: .8, boxes: .72, shoes: .7, balls: .6, pots: .66, jars: .5, gadgets: .7, plush: .7, bread: .62, bottles: .38, lamps: .8 }[what] || .7, n = Math.min(cap || 12, Math.max(1, Math.floor(len / step)));   // chunky and few (ART_DIRECTION 2.2), and light to draw
      for (let i = 0; i < n; i++) { const q = q0 + (i + .5) * len / n, c = good(key, i, "c"), h = rnd(key, i, "h");
        if (what === "folded") wb(W, q, r, y, .56, .14 + .2 * h, .34, c);
        else if (what === "boxes") wb(W, q, r, y, .42 + .14 * h, .3 + .18 * rnd(key, i, "g"), .3, mul(c, 1.25));
        else if (what === "shoes") { wb(W, q - .09, r, y, .13, .13, .32, c); wb(W, q + .09, r, y, .13, .13, .32, c); }
        else if (what === "balls") { const [a, v] = wp(W, q, r); ball(a, v, y + .16, .17, mul(c, 1.2), key + i); }
        else if (what === "plush") { const [a, v] = wp(W, q, r); ball(a, v, y + .17, .19, BROWN, key + i); ball(a, v, y + .42, .13, BROWN, key + i + "h"); }
        else if (what === "pots") wb(W, q, r, y, .3 + .12 * h, .22 + .24 * h, .28, mul(c, .9));
        else if (what === "jars") wb(W, q, r, y, .24, .28 + .12 * h, .22, mul(c, 1.1));
        else if (what === "gadgets") { wb(W, q, r, y, .42, .03, .3, DARK); wb(W, q, r - .1, y + .03, .42, .28, .03, SCREEN, 1); }
        else if (what === "bread") wb(W, q, r, y, .42, .16 + .08 * h, .24, [.58 + .1 * h, .4, .2]);
        else if (what === "bottles") wb(W, q, r, y, .14, .36 + .1 * h, .14, mul(c, 1.1));
        else if (what === "lamps") { wb(W, q, r, y, .1, .3, .1, DARK); wb(W, q, r, y + .3, .3, .22, .3, [.86, .78, .6]); } }
    }
    function guitar(W, q, r, y, key) {
      const c = [[.5, .25, .12], [.62, .42, .18], DARK, [.5, .14, .12], [.75, .7, .6], [.2, .32, .4]][Math.floor(rnd(key, "g") * 6) % 6];
      wb(W, q, r, y, .5, .58, .11, c); wb(W, q, r, y + .52, .37, .38, .11, c); wb(W, q, r, y + .84, .08, .9, .06, [.32, .22, .13]); wb(W, q, r, y + 1.72, .14, .22, .06, [.16, .14, .12]); wb(W, q, r + .065, y + .4, .14, .14, .02, [.08, .07, .07]);
    }
    function thing(a, v, y, what, key) {                          // one thing on a plinth
      if (what === "bear") { big(a, v, y + .45, .5, BROWN, key); big(a, v, y + 1.05, .34, BROWN, key + "h"); ball(a - .24, v, y + 1.36, .12, BROWN, key + "l"); ball(a + .24, v, y + 1.36, .12, BROWN, key + "r"); ball(a, v - .26, y + .98, .13, [.76, .66, .5], key + "m"); }
      else if (what === "blocks") { let n = 0; for (let row = 0; row < 3; row++) for (let i = 0; i < 3 - row; i++) bx(a + (i - (2 - row) / 2) * .34, v, y + row * .3, .3, .3, .3, mul(good(key, n++), 1.3)); }
      else if (what === "vase") { disc(a, v, y, y + .55, .2, [.2, .37, .38]); ball(a, v, y + .85, .32, mul(good(key), 1.25), key); }
      else if (what === "cone") { bx(a, v, y, .22, .45, .22, [.72, .56, .3]); ball(a, v, y + .62, .27, [.86, .7, .72], key); ball(a, v, y + .94, .21, [.8, .84, .7], key + "b"); }
      else if (what === "plant") { disc(a, v, y, y + .4, .26, SIGN.terracotta, [.2, .15, .1]); big(a, v, y + .9, .52, LEAF, key); }
      else if (what === "rocket") { disc(a, v, y, y + 1.2, .3, [.75, .72, .66]); ball(a, v, y + 1.32, .32, [.6, .2, .18], key); for (const s of [-1, 1]) bx(a + s * .36, v, y, .1, .42, .3, [.6, .2, .18]); }
      else if (what === "cake") { disc(a, v, y, y + .25, .4, [.85, .78, .66]); disc(a, v, y + .25, y + .45, .28, [.8, .6, .62]); ball(a, v, y + .54, .09, [.6, .15, .15], key); }
    }
    function inWindow(z, kind, key) {                             // on the display plinth of one side of the front
      const [name, what] = kind.split(":"), len = z[1] - z[0], c = (z[0] + z[1]) / 2, y = fy + .4, W = { a0: z[0], v0: .6, da: 1, dv: 0, na: 0, nv: 1 };
      if (name === "mannequins") { const n = Math.max(1, Math.min(4, Math.floor(len / 1.1))); for (let i = 0; i < n; i++) { const a = z[0] + (i + .5) * len / n, g = good(key, i);
        bx(a, .6, y, .26, .8, .18, [.2, .2, .24]); bx(a, .6, y + .8, .42, .62, .22, g); bx(a, .6, y + 1.45, .18, .22, .18, [.8, .72, .6]); for (const s of [-1, 1]) bx(a + s * .27, .6, y + .86, .09, .54, .12, g); } }
      else if (name === "stack") { for (let row = 0; row < 3; row++) items(W, .25 + row * .28, len - .25 - row * .28, 0, y + row * .3, what === "shoes" ? "boxes" : what, key + row, 6); if (what === "shoes") items(W, .3, len - .3, -.3, y, "shoes", key + "s", 6); }
      else if (name === "plinth") thing(c, .6, y, what, key);
      else if (name === "screen") { const w = Math.min(2.2, len - .5); bx(c, .6, y + .1, w, w * .58, .08, [.1, .1, .12]); bx(c, .6 - .05, y + .16, w - .12, w * .58 - .12, .02, SCREEN, 1); }
      else if (name === "guitarstand") { const n = Math.max(1, Math.min(3, Math.floor(len / .8))); for (let i = 0; i < n; i++) guitar(W, (i + .5) * len / n, 0, y + .05, key + i); }
      else { const n = Math.max(1, Math.min(3, Math.floor(len / 1.2))); for (let i = 0; i < n; i++) thing(z[0] + (i + .5) * len / n, .6, y, "plant", key + i); }
    }
    function onWall(W, kind, key) {                               // what stands against one wall
      const [name, what] = kind.split(":"), len = W.len; if (len < 1.2) return;
      let deep = .55;
      if (name === "shelves") { wb(W, len / 2, .03, fy, len, 2.5, .06, mul(C.wood, 1.5)); wb(W, len / 2, .27, fy, len, .28, .5, mul(C.wood, 1.1)); wb(W, len / 2, .27, fy + 2.45, len, .06, .52, mul(C.wood, 1.1));
        for (let s = 0; s < 3; s++) { const y = fy + .28 + s * .68; if (s) wb(W, len / 2, .27, y - .05, len, .05, .5, C.wood); const round = what === "balls" || what === "plush"; items(W, .15, len - .15, .27, y, round && s !== 1 ? "boxes" : what, key + s, round ? 6 : 0); } }
      else if (name === "rail") { for (const q of [.15, len - .15]) wb(W, q, .32, fy, .06, 1.62, .06, C.metal); wb(W, len / 2, .32, fy + 1.58, len - .2, .05, .05, C.metal);
        const n = Math.min(14, Math.floor((len - .5) / .36)); for (let i = 0; i < n; i++) { const tall = rnd(key, i, "t") > .7; wb(W, .4 + i * (len - .8) / Math.max(1, n - 1), .32, fy + (tall ? .55 : .8), .24, tall ? 1.0 : .75, .5, good(key, i)); }
        wb(W, len / 2, .22, fy + 1.95, len, .05, .4, C.wood); items(W, .2, len - .2, .22, fy + 2.0, "folded", key + "top", 5); }
      else if (name === "guitars") { wb(W, len / 2, .02, fy + .6, len, 2.3, .04, mul(C.wood, 1.4)); const n = Math.max(1, Math.floor(len / .95)); for (let i = 0; i < n; i++) guitar(W, (i + .5) * len / n, .1, fy + .78, key + i);
        wb(W, len / 2, .3, fy, len, .55, .55, DARK); wb(W, len / 2, .585, fy + .08, len - .3, .4, .02, [.3, .28, .25]); }
      else if (name === "screens") { const n = Math.max(1, Math.floor(len / 1.5)); for (let i = 0; i < n; i++) for (const y of [.95, 1.85]) { const q = (i + .5) * len / n; wb(W, q, .06, fy + y, 1.3, .78, .08, [.1, .1, .12]); wb(W, q, .11, fy + y + .05, 1.2, .68, .02, SCREEN, 1); }
        wb(W, len / 2, .25, fy, len, .6, .45, DARK); }
      else if (name === "mirror") { const w = Math.min(1.6, len * .5); wb(W, len / 2, .04, fy + .3, w, 1.9, .06, C.wood); wb(W, len / 2, .08, fy + .38, w - .16, 1.74, .02, [.62, .68, .72]);
        for (const q of [len * .16, len * .84]) wb(W, q, .25, fy, Math.min(1.6, len * .26), .9, .45, mul(C.wood, .9)); deep = .5; }
      else if (name === "rackets") { const n = Math.max(1, Math.floor(len / .6)); wb(W, len / 2, .02, fy + .8, len, 1.7, .04, mul(C.wood, .7));
        for (let i = 0; i < n; i++) { const q = (i + .5) * len / n, c = good(key, i); wb(W, q, .08, fy + 1.0, .05, .5, .04, DARK); wb(W, q, .08, fy + 1.5, .3, .4, .03, c); wb(W, q, .1, fy + 1.55, .22, .3, .02, [.8, .78, .7]); }
        wb(W, len / 2, .25, fy, len, .7, .45, mul(C.wood, .9)); items(W, .15, len - .15, .25, fy + .7, "balls", key + "b", 6); }
      else if (name === "cabinets") { const n = Math.max(1, Math.floor(len / 1.05)); for (let i = 0; i < n; i++) { const q = (i + .5) * len / n, c = mul(good(key, i), .8);
        wb(W, q, .4, fy, .85, 1.85, .78, c); wb(W, q, .8, fy + 1.1, .62, .46, .03, SCREEN, 1); wb(W, q, .8, fy + 1.62, .75, .2, .03, GLOW, 1); wb(W, q, .9, fy + .92, .85, .07, .28, DARK); } deep = .95; }
      else if (name === "posters") { const n = Math.max(1, Math.floor(len / 1.5)); for (let i = 0; i < n; i++) { const q = (i + .5) * len / n; wb(W, q, .03, fy + 1.1, 1.05, 1.45, .04, mul(C.wood, .8)); wb(W, q, .06, fy + 1.17, .93, 1.31, .02, good(key, i)); wb(W, q, .08, fy + 1.6, .6, .5, .02, mul(good(key, i, "b"), 1.3)); } deep = .1; }
      else if (name === "servery") {                             // food: a counter before the wall with a lit case and a machine, lit menu boards above, a back counter
        wb(W, len / 2, .3, fy, len, .9, .5, mul(C.wood, .85)); for (let i = 0; i < 3; i++) { const q = (i + .5) * len / 3; wb(W, q, .05, fy + 2.15, len / 3 - .3, .9, .05, DARK); wb(W, q, .09, fy + 2.2, len / 3 - .5, .8, .02, GLOW, 1); for (let b = 0; b < 3; b++) wb(W, q, .11, fy + 2.36 + b * .2, len / 3 - .9, .06, .02, mul(sign, .8)); }
        if (!staffed) { wb(W, len / 2, 1.6, fy, len - 1.2, 1.0, .7, C.wood); wb(W, len / 2, 1.6, fy + 1.0, len - 1.1, .05, .8, [.78, .72, .6]); wb(W, len * .32, 1.6, fy + 1.05, len * .36, .32, .5, GLOW, 1); wb(W, len * .74, 1.55, fy + 1.05, .6, .5, .45, [.22, .22, .25]);
          const a = wp(W, .8, 1.6), b = wp(W, len - .8, 1.6); T.solidAlong(xz(a[0], a[1]), xz(b[0], b[1]), fy, fy + 1.05, .45); } }
      if (deep > .2) { const a = wp(W, .2, deep / 2), b = wp(W, len - .2, deep / 2); T.solidAlong(xz(a[0], a[1]), xz(b[0], b[1]), fy, fy + 2.2, deep / 2 + .1); }
    }
    function island(a, v, kind, key) {                            // what stands in the middle of the floor
      const [name, what] = kind.split(":"), W = { a0: a, v0: v, da: 1, dv: 0, na: 0, nv: 1 };
      if (name === "table") { wb(W, 0, 0, fy + .74, 2.0, .06, 1.0, C.wood); for (const s of [-1, 1]) for (const r of [-1, 1]) wb(W, s * .9, r * .4, fy, .07, .74, .07, mul(C.wood, .8)); items(W, -.9, .9, -.22, fy + .8, what, key, 5); items(W, -.9, .9, .24, fy + .8, what, key + "b", 5); block(a - .5, v, .6, 1); block(a + .5, v, .6, 1); }
      else if (name === "roundrack") { wb(W, 0, 0, fy, .07, 1.6, .07, C.metal); for (let i = 0; i < 10; i++) { const g = 6.2832 * i / 10; bx(a + Math.cos(g) * .5, v + Math.sin(g) * .5, fy + .72, .46, .8, .12, good(key, i), 19, g); } block(a, v, .7, 1.5); }
      else if (name === "bin") { disc(a, v, fy, fy + .62, .78, mul(C.wood, .9), [.2, .15, .1], 12); for (let i = 0; i < 14; i++) { const g = 6.2832 * rnd(key, i), r = .6 * Math.sqrt(rnd(key, i, "r")); bx(a + r * Math.cos(g), v + r * Math.sin(g), fy + .56, .22, .22, .22, mul(good(key, i, "c"), 1.3), 19, g); } block(a, v, .75, .7); }
      else if (name === "bench") { wb(W, 0, 0, fy, 1.6, .4, .45, C.wood); wb(W, 0, 0, fy + .4, 1.5, .08, .4, good(key)); block(a - .4, v, .45, .48); block(a + .4, v, .45, .48); const p = P(a, 0, v); o.use(p[0], fy + .49, p[2], "sit", "Sit down", Math.atan2(S.n[0], -S.n[1])); }
      else if (name === "amps") { for (const q of [-.5, .5]) { wb(W, q, 0, fy, .8, .62, .42, DARK); wb(W, q, 0, fy + .62, .7, .5, .4, DARK); wb(W, q, -.22, fy + .08, .66, .46, .02, [.3, .28, .25]); wb(W, q, -.21, fy + .68, .58, .36, .02, [.3, .28, .25]); } block(a - .5, v, .5, 1.1); block(a + .5, v, .5, 1.1); }
      else if (name === "drums") { const red = [.5, .14, .12], skin = [.8, .76, .66], brass = [.7, .58, .25]; wb(W, 0, .1, fy, .62, .62, .4, red); disc(a - .55, v - .15, fy, fy + .5, .28, red, skin); disc(a + .5, v - .15, fy + .45, fy + .6, .2, red, skin); disc(a + .15, v - .45, fy + .5, fy + .62, .17, red, skin);
        for (const [q, r] of [[-.85, .25], [.85, .3]]) { wb(W, q, r, fy, .04, 1.25, .04, C.metal); disc(a + q, v + r, fy + 1.25, fy + 1.28, .27, brass); } block(a, v, .9, 1); }
      else if (name === "keyboard") { wb(W, 0, 0, fy + .82, 1.3, .08, .36, DARK); wb(W, 0, -.08, fy + .9, 1.2, .02, .14, [.86, .84, .78]); for (const s of [-1, 1]) wb(W, s * .45, 0, fy, .06, .82, .3, C.metal); block(a, v, .6, 1); }
      else if (name === "glasscase") { wb(W, 0, 0, fy, 1.7, .75, .65, mul(C.wood, .85)); wb(W, 0, 0, fy + .75, 1.7, .28, .65, GLOW, 1); wb(W, 0, 0, fy + 1.03, 1.74, .03, .69, [.75, .7, .6]); block(a - .45, v, .5, 1.1); block(a + .45, v, .5, 1.1); }
      else if (name === "cafetable") { disc(a, v, fy + .7, fy + .75, .46, C.wood); wb(W, 0, 0, fy, .08, .7, .08, C.metal); const c = good(key);
        for (const [q, r] of [[.8, 0], [-.8, 0], [0, .8], [0, -.8]]) { wb(W, q, r, fy, .36, .45, .36, c); q ? wb(W, q * 1.24, r, fy + .45, .06, .42, .36, c) : wb(W, q, r * 1.24, fy + .45, .36, .42, .06, c); } block(a, v, .55, .76); }
      else if (name === "hockey") { wb(W, 0, 0, fy, 2.0, .8, 1.0, [.6, .2, .18]); wb(W, 0, 0, fy + .8, 1.9, .03, .9, [.84, .86, .9]); wb(W, 0, 0, fy + .83, .04, .01, .9, [.6, .2, .18]); block(a - .5, v, .6, .9); block(a + .5, v, .6, .9); }
      else if (name === "claw") { const c = [.5, .2, .4]; wb(W, 0, 0, fy, .9, .85, .9, c); wb(W, 0, 0, fy + .85, .84, .95, .84, SCREEN, 1); wb(W, 0, 0, fy + 1.8, .9, .16, .9, c); block(a, v, .55, 2); }
      else if (name === "buckets") { for (let i = 0; i < 5; i++) { const q = (i - 2) * .5, r = (i % 2) * .34 - .17; disc(a + q, v + r, fy, fy + .4, .2, [.3, .32, .34]); ball(a + q, v + r, fy + .66, .27, mul(good(key, i), 1.3), key + i); } block(a - .6, v, .55, .6); block(a + .6, v, .55, .6); }
      else if (name === "plinth") { wb(W, 0, 0, fy, 1.0, .35, 1.0, [.7, .66, .58]); thing(a, v, fy + .35, what, key); block(a, v, .55, 1.2); }
    }
    const walls = spec.walls || [], mids = spec.middle || [], v0 = 1.35;
    onWall({ a0, v0, da: 0, dv: 1, na: 1, nv: 0, len: D - v0 - .15, side: true }, walls[0] || "posters", "L");
    onWall({ a0: a0 + .75, v0: D, da: 1, dv: 0, na: 0, nv: -1, len: a1 - a0 - 1.5 }, walls[1] || "posters", "B");
    onWall({ a0: a1, v0, da: 0, dv: 1, na: -1, nv: 0, len: D - v0 - .15, side: true }, walls[2] || "posters", "R");
    if (staffed) {                                              // where its staff serve: a counter 2.2 m in, with a till; a lit case on it where food is sold
      const half = Math.min(mid - 2.4, 1.1 * ((u.stand || []).length - 1) + 1.1);   // as long as its staff stand wide, so there is a broad way past it at each end
      T.boxQ(S, mid - half, mid + half, fy, fy + 1.0, -2.9, -2.2, T.wood, 19); T.solidAlong(xz(mid - half, 2.55), xz(mid + half, 2.55), fy, fy + 1.0, .4);
      bx(mid + half - .5, 2.55, fy + 1.0, .4, .3, .34, DARK); bx(mid + half - .5, 2.47, fy + 1.3, .34, .22, .03, SCREEN, 1);
      if (food) bx(mid - half + 1.3, 2.55, fy + 1.0, 2.2, .34, .5, GLOW, 1);
    }
    if (D >= 6) {
      const servery = walls[1] === "servery", vA = staffed ? 5.0 : Math.max(3.4, D * .44), vB = Math.min(D - 2.0, vA + 2.4), slots = [[.27, vA], [.73, vA], [.5, servery || staffed ? vA : vB]];
      mids.slice(0, 3).forEach((kind, i) => island(a0 + (a1 - a0) * slots[i][0], slots[i][1], kind, "m" + i));
    }
  }

  // ---- rides on the court floor: a carousel, and coin rides for small children. They stand still; each seat can be sat on.
  function horse(T, x, z, y, yaw, col, key) {                     // facing (sin yaw, -cos yaw)
    const { m, o } = T, fx = Math.sin(yaw), fz = -Math.cos(yaw), th = Math.atan2(-fz, fx), at = (d, s) => [x + fx * d - fz * s, z + fz * d + fx * s];
    const b = (d, s, y0, sx, sy, sz, c) => { const p = at(d, s); m.ybox(p[0], y0, p[1], sx, sy, sz, th, c, 19); };
    b(0, 0, y, .95, .36, .3, col); b(.52, 0, y + .28, .2, .46, .2, col); b(.68, 0, y + .6, .36, .2, .18, col); b(-.5, 0, y + .1, .1, .34, .1, mul(col, .6));
    for (const d of [-.3, .32]) for (const s of [-.1, .1]) b(d, s, y - .42, .09, .44, .08, mul(col, .85)); b(0, 0, y + .36, .34, .06, .34, [.5, .16, .14]);
    o.use(x, y + .42, z, "sit", T.label, yaw);
  }
  function ride(T, q) {
    const { m, o, C, F } = T, [x, z] = q.pos, key = q.kind + x + z, cream = [.8, .74, .6], red = [.56, .22, .18];
    if (q.kind === "carousel") {
      const r = q.r, n = q.horses || 10;
      T.prism(T.round(x, z, r, r, 28), F, F + .35, mul(C.wood, .9), mul(C.wood, 1.2), 19, 15); T.prism(T.round(x, z, r + .12, r + .12, 28), F, F + .16, C.stone, C.stone, 3, 3);
      T.prism(T.round(x, z, 1.15, 1.15, 14), F + .35, F + 3.7, cream, cream, 3, 3); T.prism(T.round(x, z, 1.19, 1.19, 14), F + 2.9, F + 3.12, GLOW, cream, 1, 3);
      for (let i = 0; i < 6; i++) { const f = i / 6, rr = (r + .35) * (1 - f) + .5 * f; T.prism(T.round(x, z, rr, rr, 28), F + 3.7 + i * .3, F + 4.0 + i * .3, i % 2 ? cream : red, i % 2 ? cream : red, 19, 19); }   // the canopy, in steps
      T.blob(x, F + 5.75, z, .38, [.7, .58, .25], key);
      for (let i = 0; i < 16; i++) { const a = 6.2832 * i / 16, bx = x + (r + .3) * Math.cos(a), bz = z + (r + .3) * Math.sin(a); m.box(bx - .08, F + 3.5, bz - .08, bx + .08, F + 3.68, bz + .08, C.lantern, 2); }
      for (let i = 0; i < n; i++) { const a = 6.2832 * i / n, hx = x + (r - 1.5) * Math.cos(a), hz = z + (r - 1.5) * Math.sin(a), yaw = Math.atan2(-Math.sin(a), -Math.cos(a)), y = F + 1.15 + (i % 2) * .28;
        m.box(hx - .035, F + .35, hz - .035, hx + .035, F + 3.7, hz + .035, [.7, .58, .25], 19); horse({ m, o, label: "Sit on the carousel" }, hx, hz, y, yaw, GOODS[(i * 3) % GOODS.length], key + i); }
      for (let i = 0; i < 12; i++) { const a = 6.2832 * i / 12; o.solid(x + r * .62 * Math.cos(a) - r * .4, F, z + r * .62 * Math.sin(a) - r * .4, x + r * .62 * Math.cos(a) + r * .4, F + .35, z + r * .62 * Math.sin(a) + r * .4); }
      o.solid(x - 1.1, F, z - 1.1, x + 1.1, F + 3.7, z + 1.1);
      return;
    }
    m.box(x - .8, F, z - .5, x + .8, F + .28, z + .5, [.3, .32, .36], 19); m.box(x + .95, F, z - .12, x + 1.19, F + .95, z + .12, red, 19); m.box(x + .97, F + .95, z - .1, x + 1.17, F + 1.05, z + .1, GLOW, 1);   // its base and its coin box
    o.solid(x - .8, F, z - .5, x + .8, F + .28, z + .5);
    if (q.kind === "pony") horse({ m, o, label: "Sit on the pony" }, x, z, F + .95, Math.PI, [.78, .7, .56], key);
    else if (q.kind === "rocket") { T.prism(T.round(x, z, .34, .34, 10), F + .28, F + 1.5, [.78, .75, .68], [.78, .75, .68], 19, 19); T.blob(x, F + 1.62, z, .36, red, key); for (const s of [-1, 1]) m.box(x + s * .36 - .05, F + .28, z - .18, x + s * .36 + .05, F + .75, z + .18, red, 19); o.use(x, F + .9, z + .42, "sit", "Sit on the rocket", Math.PI); }
    else { m.box(x - .62, F + .4, z - .34, x + .62, F + .78, z + .34, red, 19); m.box(x - .28, F + .78, z - .3, x + .3, F + 1.08, z + .3, cream, 19); for (const sx of [-.42, .42]) for (const sz of [-.36, .36]) m.box(x + sx - .14, F + .28, z + sz - .05, x + sx + .14, F + .56, z + sz + .05, DARK, 19); o.use(x, F + .8, z, "sit", "Sit in the car", Math.PI / 2); }
  }
  return { front, ride };
})();
