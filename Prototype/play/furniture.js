"use strict";
// Furniture kit 18 in the slice (FURN-001, FURN-002, FURN-003, FURN-005): box versions of the Blender pieces of
// Tools/blender/furniture_kit_18.py, the same sizes and the four lines (ADR-0026), for the pieces the houses use. Kit 17's
// pieces (sofa, armchair, coffee table, bed, bedside, wardrobe, bookcase, floor lamp, rug) are outlet.js's, reached through
// Outlet.pieces. As everywhere in the slice the furniture is boxes; the rounded edges and mottling of the renders are the
// gaps of docs/art/INTERIOR_STYLE.md section 6.
//
// A piece is drawn through Q(u, y, v, su, sy, sv, col, f, yaw): u across it, y up from the floor (a box's bottom), v from its
// front (-) to its back (+), as in the kit, where yaw 0 has its back to a wall at +y. Furniture.draw(kind, Q, L, seed).
// Loaded after outlet.js, before play.js; runs when a house is furnished, by which time C, hash and pick exist.
var Furniture = (() => {
  const IRON = [.10, .10, .11], PORC = [.84, .82, .76], ENAMEL = [.78, .74, .62], STONE = [.6, .56, .5], STONE_D = [.37, .34, .32];
  const MIRROR = [.42, .5, .55], GLOW = [.95, .82, .55], WICKER = [.55, .42, .24], BRASS = [.6, .48, .24], LEAF = [.22, .34, .16], POT = [.5, .28, .18];
  const P = () => Outlet.pieces;
  const legs = (Q, L, w, d, h, inset, col) => P().legs(Q, L, w, d, h, inset, col);

  // a cabinet's carcass with doors or drawers on its front, on the line's base (legs, plinth or iron frame)
  function cabinet(Q, L, w, d, h, fronts) {
    let z0 = .1;
    if (L.leg === "turned" || L.leg === "taper") legs(Q, L, w, d, .14, .06); else if (L.leg === "iron") { legs(Q, L, w, d, .14, .03); Q(0, .12, 0, w, .04, d, IRON, 9); } else Q(0, 0, .02, w - .04, .1, d - .04, L.woodD);
    if (L.leg !== "square") z0 = .14;
    Q(0, z0, 0, w, h - z0, d, L.wood);
    const n = fronts || 2;
    for (let i = 0; i < n; i++) {
      const cu = -w / 2 + w * (i + .5) / n;
      Q(cu, z0 + .05, -d / 2 - .012, w / n - .03, h - z0 - .1, .02, L.panels ? L.woodD : L.wood);
      Q(cu + (i % 2 ? -1 : 1) * (w / n / 2 - .08), z0 + (h - z0) * .7, -d / 2 - .04, .04, .06, .04, L.straps ? IRON : L.knob);
    }
  }
  const top = (Q, L, w, d, y) => Q(0, y, 0, w + .02, .04, d + .02, L.round ? L.woodD : L.wood);
  const plant = (Q, u, y, v, s, seed) => { for (let k = 0; k < 5; k++) Q(u + (hash(seed, k) - .5) * .25 * s, y + k * .12 * s, v + (hash(seed, k, 1) - .5) * .25 * s, .3 * s - k * .03 * s, .16 * s, .3 * s - k * .03 * s, pick([LEAF, [.3, .42, .2], [.26, .38, .18]], hash(seed, k, 2)), 19, k * .7); };

  const D = {
    // ---- living
    fireplace(Q, L) {
      if (L.leg === "iron") { Q(0, 0, -.1, 1.1, .08, .8, STONE_D); Q(0, .12, 0, .6, .65, .5, IRON, 9); Q(0, .77, .1, .12, 1.6, .12, IRON, 9); Q(0, .3, -.26, .3, .25, .02, [1, .5, .2], 2); return; }
      const w = 1.5, d = .4, h = 1.15, stone = L.round ? [.8, .76, .68] : L.panels ? STONE : [.72, .68, .6];
      Q(0, 0, -.12, w + .2, .06, d + .3, STONE_D);
      for (const s of [-1, 1]) Q(s * (w / 2 - .17), .06, 0, .34, h - .14, d, stone);
      Q(0, h - .2, 0, w, .2, d, stone); Q(0, h, -.02, w + .14, .07, d + .08, L.panels ? L.wood : stone);
      Q(0, .06, .08, w - .68, h - .26, d - .16, [.12, .1, .09]); Q(0, .1, 0, .5, .12, .2, [1, .5, .2], 2);
      for (const u of [-.4, .5]) Q(u, h + .07, 0, .08, .24, .08, BRASS);
    },
    painting(Q, L) { Q(0, 1.55 - .45, -.03, 1.02, .9, .06, L.leg === "iron" ? IRON : L.wood); Q(0, 1.55 - .37, -.07, .86, .74, .02, [.3, .38, .52]); Q(0, 1.55 - .37, -.08, .86, .3, .02, [.3, .33, .2]); },
    sidetable(Q, L) { legs(Q, L, .5, .5, .55, .04); top(Q, L, .5, .5, .55); Q(0, .18, 0, .4, .035, .4, L.wood); Q(.05, .59, .05, .12, .18, .12, L.knob); Q(.05, .77, .05, .26, .18, .26, GLOW, 2); },
    piano(Q, L) {
      const w = 1.45, d = .58, h = 1.25;
      Q(0, 0, .08, w, h, d - .16, L.wood); Q(0, .68, -.12, w - .1, .06, .3, L.wood);
      Q(0, .74, -.18, w - .2, .02, .14, [.85, .82, .74]); for (let k = 0; k < 12; k++) Q(-w / 2 + .2 + k * .095, .755, -.14, .03, .02, .07, [.06, .06, .06]);
      for (const s of [-1, 1]) Q(s * (w / 2 - .05), 0, -.15, .08, .7, .08, L.wood);
      Q(0, h, .08, w + .04, .04, d - .1, L.woodD); Q(-.2, .32, -.06, .5, .45, .02, L.panels ? L.woodD : L.wood);
    },
    floorplant(Q, L, seed) { Q(0, 0, 0, .4, .42, .4, L.leg === "iron" ? L.wood : L.round ? PORC : POT); plant(Q, 0, .42, 0, 1.5, seed); },
    // ---- hallway
    console(Q, L) { legs(Q, L, 1.0, .34, .76, .04); top(Q, L, 1.0, .34, .76); Q(0, .55, .02, .92, .14, .28, L.wood); Q(.3, .8, 0, .12, .18, .12, L.knob); Q(.3, .98, 0, .28, .2, .28, GLOW, 2); },
    mirror(Q, L) { const m = L.leg === "iron" ? IRON : L.wood; Q(0, 1.35 - .64, -.02, .72, 1.28, .05, m); Q(0, 1.35 - .57, -.05, .58, 1.14, .02, MIRROR); },
    shoebench(Q, L) { legs(Q, L, 1.0, .36, .44, .05); Q(0, .43, 0, 1.0, .05, .36, L.wood); Q(0, .12, 0, .94, .03, .32, L.wood); Q(0, .48, 0, .94, .06, .32, L.cloth); for (let k = 0; k < 3; k++) Q(-.3 + k * .3, .15, -.02, .22, .1, .28, pick([C.brown, C.charcoal, C.terracotta], k * .33)); },
    hooks(Q, L) { Q(0, 1.57, -.02, 1.1, .16, .04, L.wood); Q(0, 1.77, -.1, 1.14, .04, .24, L.wood); for (let i = 0; i < 5; i++) { const u = -.4 + i * .2; Q(u, 1.6, -.08, .03, .08, .08, IRON, 9); if (i % 2 === 0) Q(u, .9, -.1, .3, .7, .1, pick([C.teal, C.olive, C.terracotta, C.blue], i * .2)); } },
    coatstand(Q, L) { const m = L.leg === "iron" ? IRON : L.wood; Q(0, 0, 0, .42, .05, .42, m); Q(0, .05, 0, .07, 1.8, .07, m); for (let k = 0; k < 4; k++) Q(0, 1.6, 0, .5, .03, .03, m, 19, k * Math.PI / 4); Q(.15, 1.0, 0, .32, .62, .14, C.olive); },
    umbrella(Q, L) { Q(0, 0, 0, .3, .55, .3, L.leg === "iron" ? IRON : L.round ? PORC : L.panels ? C.teal : L.wood); for (const [u, c] of [[-.05, C.charcoal], [.06, C.terracotta]]) Q(u, .5, 0, .04, .4, .04, c); },
    // ---- kitchen
    counter(Q, L) { cabinet(Q, L, 1.8, .6, .9 - L.top, 3); Q(0, .9 - L.top, 0, 1.84, L.top, .64, L.leg === "iron" ? L.wood : L.woodD); Q(-.5, .9, .05, .4, .02, .3, L.wood); Q(.4, .9, .1, .1, .2, .1, PORC); },
    sink(Q, L) { cabinet(Q, L, 1.1, .6, .9 - L.top, 2); Q(0, .9 - L.top, 0, 1.14, L.top, .64, L.woodD); Q(0, .62, -.04, .64, .3, .5, PORC); Q(0, .9, -.04, .5, .02, .38, [.3, .31, .32]); Q(0, .92, .2, .04, .22, .04, BRASS); Q(0, 1.12, .14, .04, .04, .16, BRASS); },
    stove(Q, L) {
      const m = L.leg === "iron" || L.leg === "turned" ? IRON : ENAMEL, f = m === IRON ? 9 : 19;
      Q(0, 0, 0, .75, .9, .62, m, f); Q(0, .12, -.315, .55, .42, .02, [.15, .15, .16]); Q(0, .6, -.32, .6, .04, .02, BRASS);
      for (const [u, v] of [[-.18, -.13], [.18, -.13], [-.18, .13], [.18, .13]]) Q(u, .9, v, .2, .02, .2, [.12, .12, .12]);
      Q(0, .92, .28, .75, .3, .06, m, f);
    },
    wallcupboard(Q, L) {
      if (L.leg === "iron") { for (const y of [1.45, 1.81, 2.11]) Q(0, y, 0, 1.2, .05, .28, L.wood); return; }
      Q(0, 1.45, 0, 1.2, .7, .34, L.wood); for (const s of [-1, 1]) { Q(s * .3, 1.49, -.18, .56, .62, .02, L.panels ? L.woodD : L.wood); Q(s * .05, 1.56, -.2, .04, .06, .04, L.knob); }
    },
    fridge(Q, L) { const m = L.round ? ENAMEL : L.leg === "iron" ? [.5, .52, .54] : ENAMEL; Q(0, 0, 0, .68, 1.75, .66, m); Q(0, 1.2, -.335, .6, .02, .02, IRON, 9); Q(.24, .9, -.36, .05, .3, .05, IRON, 9); },
    potshelf(Q, L, seed) {
      const side = L.leg === "iron" ? IRON : L.wood;
      for (const s of [-1, 1]) Q(s * .475, 0, 0, .05, 1.1, .3, side);
      for (const y of [.06, .48, 1.06]) Q(0, y, 0, .98, .045, .28, L.wood);
      for (let k = 0; k < 4; k++) Q(-.3 + k * .2, k < 2 ? .1 : .52, 0, .18, .16 + .06 * hash(seed, k), .18, k % 2 ? IRON : pick([C.terracotta, C.teal, C.cream], hash(seed, k, 1)), k % 2 ? 9 : 19);
    },
    dining(Q, L) {                                            // kit 17's dining table with four chairs
      const h = .75;
      legs(Q, L, 1.4, .8, h - L.top, .06); Q(0, h - L.top, 0, 1.4, L.top, .8, L.wood);
      for (const [u, v, r] of [[-.35, -.6, 0], [.35, -.6, 0], [-.35, .6, 1], [.35, .6, 1]]) {
        legs((a, y, b, ...x) => Q(u + a, y, v + b, ...x), L, .42, .42, .44, .04); Q(u, .44, v, .44, .05, .42, L.wood);
        Q(u, .49, v + (r ? .19 : -.19), .42, .45, .05, L.panels ? L.woodD : L.wood);
      }
    },
    pendant(Q, L) {
      Q(0, 2.0, 0, .025, .95, .025, IRON, 9);
      if (L.round) Q(0, 1.78, 0, .46, .3, .46, GLOW, 2);
      else if (L.panels) for (let k = 0; k < 3; k++) { const a = k * 2 * Math.PI / 3; Q(Math.cos(a) * .32, 1.86, Math.sin(a) * .32, .02, .14, .02, IRON, 9); Q(Math.cos(a) * .32, 1.7, Math.sin(a) * .32, .2, .16, .2, GLOW, 2); }
      else Q(0, 1.78, 0, .44, .22, .44, L.leg === "iron" ? IRON : GLOW, L.leg === "iron" ? 9 : 2);
    },
    // ---- bathroom
    bath(Q, L) {
      const w = 1.7, d = .78;
      if (L.panels || L.round) { for (const a of [-1, 1]) for (const b of [-1, 1]) Q(a * (w / 2 - .22), 0, b * (d / 2 - .16), .12, .14, .12, BRASS); Q(0, .14, 0, w, .48, d, PORC); Q(0, .62, 0, w + .06, .05, d + .06, PORC); }
      else { Q(0, 0, 0, w, .6, d, L.leg === "iron" ? [.5, .52, .54] : L.wood); Q(0, .6, 0, w + .04, .04, d + .04, PORC); }
      Q(0, .6, 0, w - .16, .025, d - .16, [.62, .7, .74]); Q(w / 2 - .12, .62, 0, .05, .22, .05, BRASS);
    },
    toilet(Q, L) {
      Q(0, 0, -.05, .36, .4, .5, PORC); Q(0, .4, -.06, .4, .04, .48, L.panels || L.leg === "iron" ? L.wood : PORC); Q(0, .44, .17, .38, .44, .04, L.panels || L.leg === "iron" ? L.wood : PORC);
      if (L.panels) { Q(0, 1.82, .24, .46, .26, .2, L.wood); Q(0, .45, .24, .05, 1.37, .05, IRON, 9); } else Q(0, .42, .18, .44, .4, .18, PORC);
    },
    basin(Q, L) { if (L.panels || L.round) Q(0, 0, .04, .2, .78, .2, PORC); else legs(Q, L, .6, .44, .78, .04); Q(0, .78, 0, .62, .14, .46, PORC); Q(0, .9, 0, .46, .02, .3, [.62, .7, .74]); Q(0, .92, .17, .04, .14, .04, BRASS); },
    mirrorcab(Q, L) { Q(0, 1.25, 0, .6, .7, .16, L.leg === "iron" ? IRON : L.wood); Q(0, 1.3, -.085, .5, .6, .02, MIRROR); Q(0, 1.18, -.03, .64, .04, .2, L.wood); },
    basket(Q, L) { if (L.leg === "iron") Q(0, 0, 0, .5, .6, .38, L.wood); else { Q(0, 0, 0, .44, .55, .44, WICKER); Q(0, .55, 0, .5, .05, .5, WICKER); } },
    // ---- bedroom
    chest(Q, L) {
      const w = .95, d = .5, h = 1.05; cabinet(Q, L, w, d, h, 1);
      for (let k = 0; k < 4; k++) { const y = .2 + k * .21; Q(0, y, -d / 2 - .02, w - .1, .17, .02, L.panels ? L.woodD : L.wood); for (const s of [-1, 1]) Q(s * .25, y + .08, -d / 2 - .04, .05, .04, .03, L.straps ? IRON : L.knob); }
      Q(.25, h, 0, .14, .2, .14, L.knob); Q(-.2, h, -.02, .3, .03, .2, L.cloth);
    },
    single(Q, L) {
      const w = .95, d = 2.05, h0 = .18;
      legs(Q, L, w, d, h0, .05); Q(0, h0, 0, w, .14, d, L.leg === "iron" ? IRON : L.wood, L.leg === "iron" ? 9 : 19);
      Q(0, h0 + .14, 0, w - .06, .18, d - .06, [.78, .74, .66]); Q(0, h0 + .31, -.25, w - .02, .1, d - .55, L.accent); Q(0, h0 + .32, d / 2 - .28, .6, .13, .32, [.85, .82, .76]);
      Q(0, 0, d / 2 + .03, w + .04, L.head === "arch" ? 1.0 : .9, .07, L.head === "arch" ? L.cloth : L.leg === "iron" ? IRON : L.wood, L.leg === "iron" ? 9 : 19);
      Q(0, 0, -d / 2 - .03, w + .04, .55, .06, L.leg === "iron" ? IRON : L.wood, L.leg === "iron" ? 9 : 19);
    },
    // ---- study
    desk(Q, L, seed) {
      const w = 1.35, d = .68, h = .75;
      if (L.panels) { for (const s of [-1, 1]) Q(s * (w / 2 - .22), 0, 0, .42, h - L.top, d - .04, L.wood); Q(0, h - L.top, 0, w, L.top, d, L.wood); Q(0, h, 0, w - .2, .005, d - .2, L.accent); }
      else { legs(Q, L, w, d, h - L.top, .05); Q(0, h - L.top, 0, w, L.top, d, L.wood); Q(.4, h - L.top - .18, 0, .4, .16, d - .06, L.wood); }
      Q(-.35, h, .1, .3, .04, .22, [.85, .82, .74]); Q(.3, h, .15, .1, .12, .1, L.knob); Q(.3, h + .12, .15, .22, .16, .22, GLOW, 2);
      void seed;
    },
    chair(Q, L) {
      const seat = .47;
      legs(Q, L, .46, .44, seat - .05, .04); Q(0, seat - .05, 0, .48, .06, .46, L.wood); Q(0, seat, 0, .44, .05, .42, L.cloth);
      Q(0, seat + .05, .2, .46, .45, .05, L.panels ? L.wood : L.frame);
    },
    // ---- garden
    bistro(Q, L) {
      Q(0, 0, 0, .44, .04, .44, IRON, 9); Q(0, .03, 0, .05, .67, .05, IRON, 9);
      if (L.round || L.panels) { Q(0, .7, 0, .64 * .83, .045, .64 * .83, L.panels ? PORC : L.wood); Q(0, .7, 0, .64 * .83, .045, .64 * .83, L.panels ? PORC : L.wood, 19, Math.PI / 4); }
      else Q(0, .7, 0, .6, .045, .6, L.wood);
      for (const s of [-1, 1]) { const u = s * .62; for (const [a, b] of [[-.17, -.17], [.17, -.17], [-.17, .17], [.17, .17]]) Q(u + a, 0, b, .03, .45, .03, IRON, 9); Q(u, .45, 0, .42, .04, .42, L.round ? L.cloth : IRON, L.round ? 19 : 9); Q(u + s * .2, .49, 0, .04, .45, .4, IRON, 9); }
    },
    // ---- the sample houses (Tools/blender/house_samples_19.py): the rest of kit 18 they use
    tv(Q, L) { cabinet(Q, L, 1.3, .45, .55, 2); Q(0, .55, 0, .72, .5, .44, L.round ? ENAMEL : L.panels ? L.wood : IRON, L.panels ? 19 : 9); Q(-.07, .62, -.23, .46, .36, .02, [.12, .15, .16]); for (const s of [-1, 1]) Q(s * .12, 1.05, .05, .025, .3, .025, IRON, 9, s * .5); },
    magrack(Q, L) { const m = L.leg === "iron" ? IRON : L.wood; if (L.round) { Q(0, 0, 0, .42, .36, .42, WICKER); } else { for (const s of [-1, 1]) Q(s * .22, 0, 0, .04, .44, .3, m); Q(0, .04, 0, .45, .04, .3, L.wood); } for (let k = 0; k < 3; k++) Q(-.08 + k * .07, .1, 0, .02, .3, .22, pick([C.teal, C.terracotta, C.cream], k)); },
    sideboard(Q, L) { cabinet(Q, L, 1.6, .48, .85, 3); Q(-.5, .85, 0, .14, .3, .14, C.teal); Q(.4, .85, 0, .3, .08, .3, C.ochre); },
    dresser(Q, L) {
      cabinet(Q, L, 1.2, .48, .9, 2); Q(0, .9, 0, 1.24, .05, .51, L.wood);
      const side = L.leg === "iron" ? IRON : L.wood;
      for (const s of [-1, 1]) Q(s * .55, .95, .1, .05, 1.05, .26, side); Q(0, .95, .22, 1.08, 1.05, .02, L.woodD);
      for (const y of [1.35, 1.7, 2.0]) Q(0, y, .1, 1.1, .04, .26, L.wood);
      for (let k = 0; k < 4; k++) Q(-.39 + k * .26, .97, .16, .22, .22, .03, pick([C.cream, C.teal, C.cream, C.terracotta], k));
      for (let k = 0; k < 4; k++) Q(-.39 + k * .26, 1.39, .16, .24, .24, .03, pick([C.cream, [.3, .38, .52], C.cream, C.ochre], k));
    },
    display(Q, L) {
      cabinet(Q, L, .9, .42, .85, 2); const side = L.leg === "iron" ? IRON : L.wood;
      for (const s of [-1, 1]) Q(s * .42, .85, 0, .06, 1.05, .42, side); Q(0, .85, .19, .78, 1.05, .03, L.woodD); Q(0, 1.9, 0, .96, .06, .45, L.wood);
      for (const y of [1.2, 1.55]) Q(0, y, 0, .82, .035, .38, L.wood); Q(0, .85, -.2, .045, 1.05, .03, side);
      for (let k = 0; k < 4; k++) { Q(-.28 + k * .18, 1.235, 0, .1, .1, .1, pick([C.cream, [.3, .38, .52], C.terracotta], k)); Q(-.28 + k * .18, 1.585, 0, .12, .2, .12, pick([C.ochre, C.cream, C.teal, C.plum], k)); }
    },
    vases(Q, L) { Q(0, 0, 0, .7, .7, .32, L.wood); Q(-.18, .7, 0, .16, .36, .16, C.teal); Q(.02, .7, 0, .22, .24, .22, C.terracotta); Q(.2, .7, 0, .12, .3, .12, C.cream); Q(-.18, 1.06, 0, .24, .16, .2, [.3, .42, .2]); },
    candles(Q, L) { const m = L.leg === "iron" ? IRON : L.knob; for (const [u, h] of [[-.14, .2], [0, .26], [.14, .16]]) { Q(u, 0, 0, .1, .06, .1, m); Q(u, .06, 0, .05, h, .05, C.cream); Q(u, .06 + h, 0, .05, .07, .05, GLOW, 2); } },
    towelrail(Q, L) { const m = L.leg === "iron" ? IRON : L.wood; for (const s of [-1, 1]) { Q(s * .35, 0, 0, .05, .9, .05, m); Q(s * .35, 0, 0, .07, .04, .32, m); } for (const y of [.55, .88]) Q(0, y, 0, .72, .04, .04, m); Q(-.1, .45, -.03, .42, .45, .05, L.accent); Q(.12, .3, .03, .4, .3, .05, C.cream); },
    bunk(Q, L) {
      const m = L.leg === "iron" ? IRON : L.wood, w = 1.0, d = 2.05;
      for (const a of [-1, 1]) for (const b of [-1, 1]) Q(a * w / 2, 0, b * d / 2, .07, 1.75, .07, m);
      for (const y of [.2, 1.15]) { for (const a of [-1, 1]) Q(a * w / 2, y, 0, .05, .12, d, m); Q(0, y + .02, 0, w - .08, .16, d - .08, [.84, .8, .72]); Q(0, y + .18, -.2, w - .08, .08, d - .55, y < 1 ? L.accent : L.cloth); Q(0, y + .18, d / 2 - .28, .6, .12, .32, C.cream); for (const b of [-1, 1]) Q(0, y + .1, b * d / 2, w, .32, .05, m); }
      Q(-w / 2, 1.4, .3, .05, .1, d - .7, m); for (let k = 0; k < 5; k++) Q(-w / 2 - .03, .35 + k * .27, -d / 2 + .25, .04, .04, .4, m);
    },
    toyshelf(Q, L) { const m = L.leg === "iron" ? IRON : L.wood; for (const u of [-.53, 0, .53]) Q(u, 0, 0, .05, .8, .36, m); for (const y of [0, .38, .78]) Q(0, y, 0, 1.1, .045, .36, L.wood); Q(-.27, .05, -.01, .42, .3, .3, C.teal); Q(.27, .05, -.01, .42, .3, .3, C.terracotta); Q(-.3, .43, 0, .2, .2, .2, C.ochre); Q(.2, .43, 0, .09, .09, .09, C.cream); Q(-.25, .83, 0, .2, .32, .16, C.ochre); },
    kidtable(Q, L) { legs(Q, L, .7, .7, .48, .05); top(Q, L, .7, .7, .48); for (const s of [-1, 1]) Q(s * .52, 0, 0, .32, .3, .32, L.round ? L.cloth : L.wood); Q(-.08, .52, .05, .08, .08, .08, C.terracotta); Q(.04, .52, .05, .08, .08, .08, C.teal); },
    horse(Q, L) { const b = L.panels ? C.cream : L.accent; for (const s of [-1, 1]) Q(0, 0, s * .17, 1.1, .06, .05, L.wood); for (const a of [-.25, .25]) for (const s of [-1, 1]) Q(a, .05, s * .13, .05, .4, .05, L.wood); Q(0, .4, 0, .7, .24, .24, b); Q(.32, .55, 0, .16, .3, .14, b); Q(.43, .76, 0, .28, .14, .14, b); Q(-.02, .63, 0, .24, .05, .28, L.cloth); },
    toychest(Q, L) { Q(0, 0, 0, .85, .48, .48, L.accent); Q(0, .48, 0, .89, .06, .52, L.wood); for (const [u, c] of [[-.25, C.terracotta], [0, C.teal], [.25, C.ochre]]) Q(u, .17, -.25, .14, .14, .02, c); },
    blanket(Q, L) { if (L.buttons || L.round) Q(0, .06, 0, 1.1, .4, .45, L.cloth); else { Q(0, 0, 0, 1.1, .46, .45, L.wood); Q(0, .46, 0, 1.13, .05, .48, L.wood); } Q(.2, .5, 0, .5, .06, .4, L.accent); },
    cheval(Q, L) { const m = L.leg === "iron" ? IRON : L.wood; for (const s of [-1, 1]) { Q(s * .36, 0, 0, .05, 1.6, .05, m); Q(s * .36, 0, 0, .07, .06, .5, m); } Q(0, .2, 0, .61, 1.4, .05, m); Q(0, .23, -.03, .5, 1.34, .02, MIRROR); },
    vanity(Q, L) { legs(Q, L, 1.1, .48, .71, .05); top(Q, L, 1.1, .48, .71); for (const s of [-1, 1]) Q(s * .35, .57, 0, .34, .14, .44, L.wood); Q(0, .75, .18, .66, .74, .05, L.leg === "iron" ? IRON : L.wood); Q(0, .81, .15, .54, .62, .02, MIRROR); legs(Q, L, .4, .4, .41, .04); Q(0, .41, -.55, .42, .05, .42, L.round ? L.cloth : L.wood); },
    desklamp(Q, L) { const m = L.leg === "iron" ? IRON : L.knob; Q(0, 0, 0, .2, .04, .16, m); Q(0, .04, .02, .04, .35, .04, m); Q(0, .34, -.1, .26, .14, .2, L.panels ? L.accent : m); Q(0, .3, -.1, .16, .04, .12, GLOW, 2); },
    shelving(Q, L, seed) { const m = L.leg === "iron" ? IRON : L.wood; for (const s of [-1, 1]) Q(s * .48, 0, 0, .05, 1.9, .4, m); for (const y of [.06, .48, .9, 1.32, 1.86]) Q(0, y, 0, .98, .045, .38, L.wood); for (let r = 0; r < 4; r++) for (let k = 0; k < 6; k++) Q(-.38 + k * .15, .1 + r * .42, 0, .1, .26 + .06 * hash(seed, r, k), .26, pick([C.teal, C.cream, C.terracotta, [.3, .38, .52], C.ochre, C.plum], hash(seed, k, r))); },
    filing(Q, L) { const m = L.leg === "iron" ? IRON : L.round || L.panels ? L.wood : ENAMEL; Q(0, 0, 0, .48, 1.32, .6, m, m === IRON ? 9 : 19); for (let k = 0; k < 4; k++) { Q(0, .06 + k * .31, -.31, .44, .27, .02, m, m === IRON ? 9 : 19); Q(0, .26 + k * .31, -.33, .14, .05, .02, C.cream); } Q(0, 1.32, 0, .34, .06, .26, L.knob); },
    pinboard(Q, L) { const m = L.leg === "iron" ? IRON : L.wood; Q(0, 1.05, -.03, 1.1, .8, .05, m); Q(0, 1.1, -.06, 1.0, .7, .02, [.55, .4, .22]); for (let k = 0; k < 5; k++) Q(-.35 + k * .17, 1.25 + (k % 2) * .2, -.075, .14, .12, .01, pick([C.cream, C.cream, C.ochre, C.teal, C.terracotta], k)); },
    curtains(Q, L) { const m = L.leg === "iron" ? IRON : L.wood, c = L.leg === "iron" ? L.accent : L.cloth; Q(0, 2.33, -.02, 2.5, .05, .05, m); for (const s of [-1, 1]) for (let k = 0; k < 4; k++) Q(s * (.95 - .12 * k), .15, -.06 + (k % 2) * .03, .13, 2.18, .05, c); },
    bust(Q, L) { const st = L.panels ? STONE : L.leg === "iron" ? STONE_D : L.wood; Q(0, 0, 0, .36, 1.0, .36, st); Q(0, 1.0, 0, .34, .18, .2, L.leg === "iron" ? IRON : PORC); Q(0, 1.18, 0, .08, .1, .08, L.leg === "iron" ? IRON : PORC); Q(0, 1.28, 0, .2, .26, .22, L.leg === "iron" ? IRON : PORC); },
    wallshelf(Q, L) { Q(0, 1.4, -.1, 1.0, .05, .22, L.wood); for (let k = 0; k < 6; k++) Q(-.42 + k * .07, 1.45, -.1, .05, .22, .18, pick([C.teal, C.terracotta, C.cream, C.ochre, C.plum, [.3, .38, .52]], k)); Q(.15, 1.45, -.1, .12, .16, .12, C.cream); Q(.35, 1.45, -.1, .12, .1, .1, C.terracotta); },
  };
  // Blender kit names (furniture_kit_18.py, "k17:" for kit 17) to these, so a house's furniture list reads as the Blender one
  const NAMES = { l_fireplace: "fireplace", o_painting: "painting", l_sidetable: "sidetable", l_piano: "piano", o_floorplant: "floorplant",
    h_console: "console", h_mirror: "mirror", h_shoebench: "shoebench", h_hooks: "hooks", h_coatstand: "coatstand", h_umbrella: "umbrella",
    k_counter: "counter", k_sink: "sink", k_stove: "stove", k_wall: "wallcupboard", k_fridge: "fridge", k_potshelf: "potshelf",
    d_pendant: "pendant", b_bath: "bath", b_toilet: "toilet", b_basin: "basin", b_mirror: "mirrorcab", b_basket: "basket",
    r_chest: "chest", r_single: "single", s_desk: "desk", s_chair: "chair", g_bistro: "bistro", "k17:dining": "dining",
    l_tv: "tv", l_magrack: "magrack", d_sideboard: "sideboard", d_dresser: "dresser", d_display: "display", o_vases: "vases", o_candles: "candles",
    b_rail: "towelrail", c_bunk: "bunk", c_toyshelf: "toyshelf", c_table: "kidtable", c_horse: "horse", c_toychest: "toychest",
    r_blanket: "blanket", r_cheval: "cheval", r_vanity: "vanity", s_lamp: "desklamp", s_shelving: "shelving", s_filing: "filing",
    s_pinboard: "pinboard", o_curtains: "curtains", o_bust: "bust", o_wallshelf: "wallshelf" };
  // what each piece takes up, for walking into it: [width across, depth front to back, height]; pieces on the wall or the
  // ceiling (painting, mirror, hooks, wall cupboard, mirror cabinet, pendant) and rugs are left out: you walk under or over them
  const SIZE = { "k17:sofa": [2.0, .92, 1.0], "k17:armchair": [.95, .92, 1.0], "k17:coffee": [1.1, .6, .45], "k17:bed": [1.6, 2.2, .6],
    "k17:bedside": [.48, .42, .6], "k17:wardrobe": [1.1, .6, 2.1], "k17:bookcase": [.9, .34, 1.8], "k17:floor_lamp": [.34, .34, 1.7],
    "k17:dining": [1.4, 1.7, .95], l_fireplace: [1.7, .6, 1.2], l_sidetable: [.5, .5, .6], l_piano: [1.45, .58, 1.25], o_floorplant: [.4, .4, 1.0],
    h_console: [1.0, .34, .8], h_shoebench: [1.0, .36, .5], h_coatstand: [.42, .42, 1.8], h_umbrella: [.3, .3, .6],
    k_counter: [1.84, .64, .9], k_sink: [1.14, .64, .9], k_stove: [.75, .62, .9], k_fridge: [.68, .66, 1.75], k_potshelf: [1.0, .3, 1.1],
    b_bath: [1.76, .84, .65], b_toilet: [.44, .56, .8], b_basin: [.62, .46, .9], b_basket: [.5, .5, .6],
    r_chest: [.95, .5, 1.05], r_single: [.99, 2.1, .6], s_desk: [1.35, .68, .75], s_chair: [.48, .46, .95], g_bistro: [1.7, .64, .9],
    l_tv: [1.3, .45, 1.1], l_magrack: [.45, .3, .5], d_sideboard: [1.6, .48, .9], d_dresser: [1.2, .48, 2.0], d_display: [.9, .42, 1.9],
    o_vases: [.7, .32, .7], b_rail: [.72, .32, .9], c_bunk: [1.0, 2.05, 1.75], c_toyshelf: [1.1, .36, .8], c_table: [1.5, .7, .55],
    c_horse: [1.1, .4, .9], c_toychest: [.85, .48, .55], r_blanket: [1.1, .45, .5], r_cheval: [.8, .5, 1.6], r_vanity: [1.1, 1.1, .75],
    s_shelving: [1.0, .4, 1.9], s_filing: [.48, .6, 1.32], o_bust: [.36, .36, 1.3] };
  function draw(kind, Q, L, seed) {
    const k17 = { "k17:sofa": q => P().sofa(q, L, 2.0), "k17:armchair": q => P().sofa(q, L, .95), "k17:coffee": q => P().coffee(q, L), "k17:bed": q => P().bed(q, L),
      "k17:bedside": q => P().bedside(q, L), "k17:wardrobe": q => P().wardrobe(q, L), "k17:bookcase": q => P().bookcase(q, L, seed), "k17:floor_lamp": q => P().floorLamp(q, L),
      "k17:rug": q => P().rug(q, L) }[kind];
    if (k17) return k17(Q);
    const fn = D[NAMES[kind] || kind];
    if (fn) fn(Q, L, seed);
  }
  return { draw, NAMES, SIZE };
})();
