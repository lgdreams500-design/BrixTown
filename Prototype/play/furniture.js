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
  const DARK = [.12, .1, .09], INK = [.08, .08, .09], CHALK = [.1, .13, .12], CORK = [.5, .35, .2], CARD = [.58, .48, .32], DUSTY = [.45, .5, .58], KEYS = [.72, .68, .58];
  const P = () => Outlet.pieces;
  const legs = (Q, L, w, d, h, inset, col) => P().legs(Q, L, w, d, h, inset, col);
  const leg = (Q, L, u, v, h, col) => P().leg(Q, L, u, v, h, col);
  const iw = L => L.leg === "iron" ? [IRON, 9] : [L.wood];                  // iron in Workshop, the line's wood otherwise
  const at = (Q, du, dv) => (u, y, v, ...x) => Q(u + du, y, v + dv, ...x);   // a part of a piece set off from its middle
  // a part of a piece turned by a (Blender's turn about z) about (x, v) of the piece: the slice's boxes turn the other way
  const turn = (Q, x, v, a) => (u, y, w, su, sy, sv, col, f) => Q(x + u * Math.cos(a) - w * Math.sin(a), y, v + u * Math.sin(a) + w * Math.cos(a), su, sy, sv, col, f || 19, -a);
  const oc = (Q, u, y, v, d, h, col, f) => { Q(u, y, v, d * .83, h, d * .83, col, f); Q(u, y, v, d * .83, h, d * .83, col, f || 19, Math.PI / 4); };   // a round part lying flat
  const disc = (Q, u, y, v, r, t, col, f) => { Q(u, y - r * .6, v, 2 * r, 1.2 * r, t, col, f); Q(u, y - r, v, 1.2 * r, 2 * r, t, col, f); };      // standing up, facing the front, centred at y
  // what a cabinet stands on, as the kit's base(): returns where its body starts
  function base(Q, L, w, d) {
    if (L.leg === "turned" || L.leg === "taper") { legs(Q, L, w, d, .14, .06); return .12; }
    if (L.leg === "iron") { legs(Q, L, w, d, .15, .03); Q(0, .1, 0, w, .04, d, IRON, 9); return .14; }
    Q(0, 0, .02, w - .04, .1, d - .04, L.woodD); return .1;
  }
  // a table in the line's way, as the kit's tbl(): legs and a top, or a pedestal under a round top
  function tbl(Q, L, w, d, h, round) {
    if (round == null ? L.round : round) { const m = Math.min(w, d); Q(0, 0, 0, m * .2, h - .05, m * .2, L.wood); oc(Q, 0, 0, 0, m * .64, .05, L.wood); oc(Q, 0, h - .05, 0, w, .05, L.wood); }
    else { legs(Q, L, w, d, h - L.top, .05); Q(0, h - L.top, 0, w, L.top, d, L.wood); }
  }
  function stool(Q, L, h, s, ring) {
    if (L.round) { for (let i = 0; i < 3; i++) { const a = i * 2 * Math.PI / 3; Q(Math.cos(a) * s * .36, 0, Math.sin(a) * s * .36, .045, h - .04, .045, L.wood); } Q(0, h - .045, 0, s + .04, .09, s + .04, L.cloth); }
    else { legs(Q, L, s, s, h - .04, .04); Q(0, h - .045, 0, s + .02, .05, s + .02, L.wood); if (L.buttons) Q(0, h + .005, 0, s - .04, .05, s - .04, L.cloth); }
    if (ring) { const a = s / 2 - .02, y = h * .35 - .02; for (const b of [-1, 1]) { Q(0, y, b * a, 2 * a, .04, .04, ...iw(L)); Q(b * a, y, 0, .04, .04, 2 * a, ...iw(L)); } }
  }
  const post = (Q, L, u, v, h, m) => { if (L.leg === "turned") leg(Q, L, u, v, h); else Q(u, 0, v, .05, h, .05, ...m); };
  const pot = (Q, L, u, y, v, s, seed) => { Q(u, y, v, .34 * s, .26 * s, .34 * s, POT); plant(Q, u, y + .26 * s, v, .8 * s, seed); };

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
    gardenbench(Q, L) {
      const w = 1.5, d = .55, m = L.leg === "iron" || L.leg === "turned" ? [IRON, 9] : [L.wood];
      for (const s of [-1, 1]) {
        const u = s * w / 2;
        if (L.round) Q(u, 0, .03, .08, .68, d + .04, L.wood);
        else { Q(u, 0, -d / 2 + .04, .05, .66, .05, ...m); Q(u, 0, d / 2 - .01, .05, .9, .05, ...m); Q(u, .65, 0, .07, .04, d, L.wood); }
        Q(u, .39, 0, .044, .044, d - .08, ...m);
      }
      for (let i = 0; i < 4; i++) Q(0, .43, -d / 2 + .08 + i * .12, w - .06, .04, .09, L.wood);
      for (let i = 0; i < 3; i++) Q(0, .585 + i * .13, d / 2 - .02 + i * .02, w - .06, .09, .04, L.wood);
    },
    planter(Q, L, seed) {
      const w = 1.2, d = .42, h = .5;
      if (L.round) { for (const [u, s, k] of [[-.35, 1, 0], [.3, .8, 1]]) { Q(u, 0, 0, .5 * s, .5 * s, .5 * s, POT); plant(Q, u, .45 * s, 0, .85 * s, seed + k); } return; }
      Q(0, 0, 0, w, h, d, L.leg === "square" ? POT : L.wood);
      if (L.straps) for (const u of [-.4, .4]) Q(u, .02, 0, .05, h - .04, d + .02, IRON, 9);
      if (L.panels) for (const u of [-.3, .3]) Q(u, .07, -d / 2 - .01, .44, h - .14, .02, L.woodD);
      Q(0, h - .02, 0, w - .06, .02, d - .06, DARK);
      for (let i = 0; i < 3; i++) plant(Q, -.38 + i * .38, h - .05, 0, .65, seed + i);
      for (let i = 0; i < 4; i++) Q(-.45 + i * .3, h + .265, -.1, .08, .07, .08, [C.terracotta, C.cream, C.plum, C.ochre][i]);
    },
    parasol(Q, L) {
      const m = iw(L), can = L.leg === "square" ? C.cream : L.accent;
      Q(0, 0, 0, .52, .1, .52, STONE); Q(0, .1, 0, .05, 2.25, .05, ...m);
      if (L.panels) oc(Q, 0, 1.88, 0, 2.42, .08, C.cream);
      [[2.3, 1.95], [1.45, 2.07], [.6, 2.19]].forEach(([r, y]) => oc(Q, 0, y, 0, L.round ? r * .96 : r, .12, can));
      if (L.round) Q(0, 2.23, 0, .3, .14, .3, can);
      Q(0, 2.34, 0, .08, .08, .08, ...m);
    },
    lounger(Q, L) {
      const w = .68, l = 1.9, m = iw(L);
      if (L.panels) {                                         // Old Town: a striped deck chair
        for (const s of [-1, 1]) { const u = s * w / 2; Q(u, 0, -.45, .05, .5, .05, L.wood); Q(u, .45, .3, .05, .5, .05, L.wood); Q(u, 0, .38, .05, .45, .05, L.wood); }
        Q(0, .925, .35, w, .05, .05, L.wood); Q(0, .375, -.3, w, .05, .05, L.wood);
        for (let i = 0; i < 5; i++) { const c = i % 2 ? C.cream : C.terracotta; Q(-.24 + i * .12, .33, -.15, .12, .04, .4, c); Q(-.24 + i * .12, .37, .2, .12, .5, .06, c); }
        return;
      }
      legs(Q, L, w, l, .28, .06, m[0]); Q(0, .275, 0, w, .05, l, ...m);
      Q(0, L.round ? .32 : .34, -.25, w - (L.round ? .02 : .06), L.round ? .14 : .08, l * .67, L.cloth);
      Q(0, .55, .53, w - .06, .25, .17, L.cloth); Q(0, .32, .71, w - .06, .25, .17, L.cloth);
      Q(0, .32, .72, .04, .3, .04, ...m); Q(0, .62, .55, .4, .2, .14, L.accent);
    },
    plantstand(Q, L, seed) {
      const m = L.leg === "iron" || L.leg === "turned" ? [IRON, 9] : [L.wood];
      [[.3, -.25], [.62, 0], [.94, .25]].forEach(([y, u], i) => { Q(u, y - .02, 0, .36, .04, .34, L.wood); pot(Q, L, u, y + .02, 0, .6 - i * .08, seed + i); });
      for (const u of [-.315, .435]) for (const v of [-.15, .15]) Q(u, 0, v, .044, .96, .044, ...m);
      const can = L.leg === "iron" ? [IRON, 9] : [C.teal]; Q(.5, 0, -.1, .2, .22, .2, ...can); Q(.65, .12, -.1, .2, .04, .04, ...can);
    },
    // ---- bathroom
    towelrail(Q, L) {
      const m = iw(L);
      for (const s of [-1, 1]) { post(Q, L, s * .35, 0, .9, m); Q(s * .35, 0, 0, .06, .04, .32, ...m); }
      for (const y of [.528, .858]) Q(0, y, 0, .7, .044, .044, ...m);
      Q(-.1, .48, -.03, .42, .42, .05, L.accent); Q(.12, .27, .03, .4, .3, .05, C.cream);
    },
    // ---- dining
    sideboard(Q, L) {
      const h = .85, lift = L.panels ? .06 : 0;
      cabinet(Q, L, 1.6, .48, h, 3); if (L.panels) Q(0, h, .01, 1.67, .06, .53, L.wood);
      Q(-.5, h + lift, 0, .16, .3, .16, C.teal); Q(.4, h + lift, 0, .26, .08, .26, C.ochre); Q(.41, h + lift + .08, 0, .16, .08, .1, C.terracotta);
    },
    dresser(Q, L, seed) {
      const w = 1.2, d = .48, h = .9, uw = w - .06, side = iw(L);
      cabinet(Q, L, w, d, h, 2); Q(0, h - .005, 0, w + .04, .05, d + .03, L.wood);
      for (const s of [-1, 1]) Q(s * (uw / 2 - .025), h + .04, -.01, .05, 1.05, .26, ...side);
      Q(0, h + .04, .11, uw - .06, 1.05, .02, L.woodD);
      for (const z of [h + .4, h + .75, h + 1.07]) Q(0, z - .022, -.01, uw, .045, .26, L.wood);
      if (L.panels) Q(0, h + 1.105, -.01, uw + .08, .07, .31, L.wood);
      for (let i = 0; i < 4; i++) { Q(-.39 + i * .26, h + .07, .06, .2, .2, .02, [C.cream, C.teal, C.cream, C.terracotta][i]); Q(-.39 + i * .26, h + .422, .06, .22, .22, .02, [C.cream, DUSTY, C.cream, C.teal][i]); }
      for (let i = 0; i < 4; i++) Q(-.39 + i * .26, h + .772, -.01, .11, .1 + .1 * hash(seed, i), .11, pick([C.cream, C.terracotta, C.teal, C.ochre, PORC], hash(seed, i, 1)));
    },
    bench(Q, L) {
      const w = 1.4, d = .38, h = .45;
      if (L.round) { for (const s of [-1, 1]) Q(s * (w / 2 - .05), 0, 0, .08, h, d - .04, L.wood); Q(0, h - .06, 0, w + .04, .12, d + .06, L.cloth); return; }
      legs(Q, L, w, d, h - L.top, .06); Q(0, h - L.top, 0, w, L.top, d, L.wood);
      if (L.leg === "iron") Q(0, .1, 0, w - .14, .04, .04, IRON, 9); else Q(0, .115, 0, w - .14, .05, .05, L.wood);
      if (L.buttons) Q(0, h + .005, 0, w - .08, .06, d - .06, L.cloth);
    },
    barstool(Q, L) {
      stool(Q, L, .75, .38, !L.round);
      if (L.round) oc(Q, 0, .245, 0, .4, .03, IRON, 9);
      if (L.panels || L.leg === "iron") { Q(0, .89, .16, .34, .12, .05, L.wood); for (const s of [-1, 1]) Q(s * .14, .74, .16, .045, .2, .045, ...iw(L)); }
    },
    display(Q, L) {
      const w = .9, d = .42, h = 1.9, side = iw(L), up = h - .85;
      cabinet(Q, L, w, d, .85, 2);
      for (const s of [-1, 1]) Q(s * (w / 2 - .03), .85, 0, .06, up, d, ...side);
      Q(0, .85, d / 2 - .02, w - .08, up, .03, L.woodD); Q(0, h - .03, 0, w + (L.panels ? .06 : 0), .06, d + .03, L.wood);
      for (const z of [1.2, 1.55]) Q(0, z - .018, 0, w - .08, .035, d - .04, L.wood);
      Q(0, .85, -d / 2 - .01, .045, up, .03, ...side); Q(0, 1.43, -d / 2 - .01, w - .08, .04, .03, ...side);
      for (let i = 0; i < 3; i++) Q(-.25 + i * .25, .87, .08, .24, .24, .02, [C.cream, C.teal, C.cream][i]);
      [-.25, -.05, .15, .3].forEach((u, i) => { Q(u, 1.22, -.02, .1, .09, .1, [C.cream, DUSTY, C.terracotta, C.cream][i]); Q(u, 1.57, 0, .12, .18, .12, [C.ochre, C.cream, C.teal, C.plum][i]); });
    },
    // ---- study
    filing(Q, L) {
      const w = .48, d = .6, h = 1.32, m = L.leg === "iron" ? [IRON, 9] : L.leg === "square" ? [ENAMEL] : [L.wood];
      let z0 = 0; if (L.leg === "turned" || L.leg === "taper") { legs(Q, L, w, d, .08, .06); z0 = .06; }
      Q(0, z0, 0, w, h - z0, d, ...m);
      const hh = (h - z0 - .06) / 4;
      for (let i = 0; i < 4; i++) {
        const zz = z0 + .03 + i * hh;
        Q(0, zz + .0125, -d / 2 - .012, w - .04, hh - .025, .03, ...m); Q(0, zz + hh * .7 - .025, -d / 2 - .032, .14, .05, .012, C.cream);
        Q(0, zz + hh * .45 - .018, -d / 2 - .05, .12, .035, .035, ...(L.straps ? [IRON, 9] : [L.knob]));
      }
      Q(0, h, 0, .34, .06, .26, L.knob); Q(0, h + .055, 0, .3, .02, .22, C.cream);
    },
    desklamp(Q, L) {
      const m = L.leg === "iron" ? [IRON, 9] : [L.knob];
      if (L.round) { oc(Q, 0, 0, 0, .24, .05, ...m); Q(0, .05, 0, .044, .37, .044, ...m); Q(0, .4, 0, .26, .2, .26, GLOW, 2); }
      else if (L.panels) { oc(Q, 0, 0, 0, .26, .05, L.wood); Q(0, .05, .02, .044, .31, .044, L.knob); Q(0, .34, -.02, .42, .12, .18, L.accent); Q(0, .33, -.02, .34, .02, .1, GLOW, 2); }
      else { Q(0, 0, 0, .2, .05, .16, ...m); Q(0, .05, .05, .044, .31, .044, ...m); Q(0, .36, -.03, .044, .044, .26, ...m); Q(0, .34, -.18, .24, .14, .24, ...(L.leg === "iron" ? m : [L.accent])); Q(0, .32, -.18, .1, .06, .1, GLOW, 2); }
    },
    pinboard(Q, L, seed) {
      Q(0, 1.05, 0, 1.1, .8, .05, ...iw(L)); Q(0, 1.1, -.03, 1.0, .7, .02, CORK);
      for (let i = 0; i < 6; i++) Q(-.35 + .7 * hash(seed, i), 1.22 + .4 * hash(seed, i, 1), -.045, .16, .14, .01, pick([C.cream, C.cream, C.ochre, C.teal, C.terracotta], hash(seed, i, 2)));
    },
    shelving(Q, L, seed) {
      const w = 1.0, d = .4, h = 1.9;
      for (const s of [-1, 1]) { if (L.leg === "iron") for (const v of [-d / 2 + .02, d / 2 - .02]) Q(s * w / 2, 0, v, .04, h, .04, IRON, 9); else Q(s * (w / 2 - .025), 0, 0, .05, h, d, L.wood); }
      for (const z of [.08, .5, .92, 1.34, h - .03]) Q(0, z - .022, 0, w - .02, .045, d - .02, L.wood);
      for (const z of [.1, .94]) for (let j = 0; j < 4; j++) Q(-.33 + j * .2, z + .01, 0, .16, .3, d - .08, [C.teal, C.cream, C.terracotta, DUSTY][j]);
      for (const [z, n] of [[.52, 12], [1.36, 9]]) for (let k = 0; k < n; k++) Q(-.43 + k * .068, z, 0, .055, .2 + .1 * hash(seed, z * 10, k), .2, pick([C.terracotta, C.teal, C.cream, C.ochre, DUSTY, C.olive, C.plum], hash(seed, k, z * 10)));
      Q(.3, 1.36, 0, .24, .22, .3, CARD);
    },
    // ---- children
    bunk(Q, L) {
      const w = 1.0, d = 2.05, m = iw(L), t = L.leg === "iron" ? .05 : .07;
      for (const a of [-1, 1]) for (const b of [-1, 1]) { Q(a * w / 2, 0, b * d / 2, t, 1.75, t, ...m); if (L.panels) Q(a * w / 2, 1.75, b * d / 2, .1, .1, .1, L.wood); }
      [.2, 1.15].forEach((z, lvl) => {
        for (const a of [-1, 1]) Q(a * w / 2, z - .06, 0, .05, .12, d, ...m);
        Q(0, z + .02, 0, w - .08, .16, d - .08, C.cream); Q(0, z + .16, -.2, w - .06, .08, d - .55, lvl ? L.cloth : L.accent); Q(0, z + .18, d / 2 - .28, .6, .12, .32, C.cream);
        const eh = L.round ? .36 : .32; for (const b of [-1, 1]) Q(0, z + .25 - eh / 2, b * d / 2, w, eh, .05, ...(L.leg === "iron" ? m : [L.wood]));
      });
      Q(-w / 2, 1.4, .3, .05, .1, d - .7, ...m);
      for (const v of [-d / 2 + .05, -d / 2 + .45]) Q(-w / 2 - .02, 0, v, .044, 1.5, .044, ...m);
      for (let i = 0; i < 5; i++) Q(-w / 2 - .02, .33 + i * .27, -d / 2 + .25, .04, .044, .4, ...m);
      Q(.25, 1.34, .7, .2, .22, .16, C.ochre); Q(.25, 1.53, .7, .15, .14, .13, C.ochre);
    },
    cot(Q, L) {
      const w = .7, d = 1.25, h = 1.0, m = iw(L);
      if (L.round) {
        Q(0, .3, 0, w + .1, .5, d + .1, L.wood); Q(0, .73, 0, w - .06, .1, d - .08, C.cream);
        for (const a of [-1, 1]) for (const b of [-1, 1]) Q(a * (w / 2 - .09), 0, b * (d / 2 - .14), .05, .36, .05, L.wood);
        Q(0, .81, -.15, w - .14, .06, d * .55, L.accent); return;
      }
      for (const a of [-1, 1]) for (const b of [-1, 1]) Q(a * w / 2, 0, b * d / 2, .06, h, .06, ...m);
      for (const z of [.32, h - .03]) { for (const a of [-1, 1]) Q(a * w / 2, z - .025, 0, .05, .05, d, ...m); for (const b of [-1, 1]) Q(0, z - .025, b * d / 2, w, .05, .05, ...m); }
      for (const a of [-1, 1]) for (let i = 0; i < 7; i++) Q(a * w / 2, .32, -d / 2 + d * (i + .5) / 7, .04, h - .35, .04, ...m);
      for (const b of [-1, 1]) { if (L.panels) Q(0, .33, b * d / 2, w - .04, .66, .04, L.wood); else for (let i = 0; i < 3; i++) Q(-w / 2 + w * (i + .5) / 3, .32, b * d / 2, .04, h - .35, .04, ...m); }
      Q(0, .36, 0, w - .06, .12, d - .06, C.cream); Q(0, .47, -.15, w - .08, .06, d * .55, L.accent);
    },
    toychest(Q, L) {
      const w = .85, d = .48, h = .5;
      if (L.round) { Q(0, 0, 0, w, h, d, L.accent); Q(0, h - .06, 0, w + .04, .12, d + .04, L.wood); }
      else {
        let z0 = 0;
        if (L.leg === "turned") { legs(Q, L, w, d, .08, .06); z0 = .06; } else if (L.leg === "iron") { legs(Q, L, w, d, .09, .03); Q(0, .04, 0, w, .04, d, IRON, 9); z0 = .08; }
        Q(0, z0, 0, w, h - z0, d, L.accent); Q(0, h, 0, w + .04, .06, d + .04, L.wood);
        if (L.straps) for (const u of [-.28, .28]) Q(u, 0, 0, .05, h + .04, d + .02, IRON, 9);
      }
      [-.25, 0, .25].forEach((u, i) => Q(u, h * .5 - .07, -d / 2 - .012, .14, .14, .02, [C.terracotta, C.teal, C.ochre][i]));
      Q(w / 2 + .2, 0, -.1, .22, .22, .22, C.terracotta);
    },
    kidtable(Q, L) {
      tbl(Q, L, .7, .7, .52);
      for (const u of [-.52, .52]) stool(at(Q, u, 0), L, .3, .3);
      for (let i = 0; i < 4; i++) Q(-.1 + (i % 2) * .1, .53 + (i >> 1) * .08, .05, .08, .08, .08, [C.terracotta, C.teal, C.ochre, DUSTY][i]);
      Q(.15, .52, -.12, .24, .006, .18, C.cream);
    },
    rockinghorse(Q, L) {
      const bm = L.panels ? C.cream : L.accent;
      for (const s of [-1, 1]) {
        Q(0, .03, s * .17, .6, .05, .05, L.wood); for (const u of [-.45, .45]) Q(u, .09, s * .17, .22, .1, .05, L.wood);
        for (const u of [-.24, .24]) Q(u, .08, s * .13, .05, .37, .05, L.wood);
      }
      Q(0, .37, 0, .7, .26, .26, bm); Q(.32, .48, 0, .2, .36, .16, bm); Q(.43, .74, 0, .3, .16, .14, bm);
      Q(.28, .63, 0, .18, .26, .06, L.woodD); Q(.47, .84, -.075, .04, .04, .02, INK); Q(-.02, .605, 0, .24, .05, .28, L.cloth);
      Q(.3, .718, 0, .044, .044, .24, IRON, 9); Q(-.38, .38, 0, .06, .24, .06, L.woodD);
    },
    toyshelf(Q, L) {
      const w = 1.1, d = .36, h = .8;
      for (const s of [-1, 0, 1]) Q(s * (w / 2 - .025), 0, 0, .05, h, d, ...iw(L));
      for (const z of [.03, .4, h]) Q(0, z - .022, 0, w, .045, d, L.wood);
      [-.27, .27].forEach((u, i) => Q(u, .04, -.01, .42, .32, d - .06, L.round ? [DUSTY, C.ochre][i] : [C.teal, C.terracotta][i]));
      Q(-.32, .43, 0, .2, .2, .2, C.ochre); for (let i = 0; i < 3; i++) Q(.12 + i * .1, .425, 0, .09, .09, .09, [C.terracotta, C.teal, C.cream][i]);
      Q(-.25, h + .02, 0, .2, .2, .16, C.ochre); Q(-.25, h + .2, 0, .15, .14, .13, C.ochre); Q(.25, h + .02, 0, .24, .14, .24, C.terracotta);
    },
    // ---- shop
    shopcounter(Q, L) {
      const w = 1.8, d = .62, h = 1.0, m = L.round ? [ENAMEL] : [IRON, 9];
      cabinet(Q, L, w, d, h - L.top, 3); Q(0, h - L.top, 0, w + .06, L.top, d + .06, L.wood);
      Q(.4, h, .05, .42, .26, .36, ...m); Q(.4, h + .23, .14, .38, .14, .14, ...m); Q(.4, h + .365, .14, .24, .07, .03, C.cream); Q(.4, h + .26, -.02, .26, .03, .16, KEYS);
      Q(-.45, h, 0, .3, .12, .26, IRON, 9); Q(-.45, h + .12, 0, .32, .03, .32, C.ochre); Q(-.05, h, .1, .3, .08, .2, CARD);
    },
    gondola(Q, L, seed) {
      const w = 1.6, d = .8, h = 1.55, m = L.leg === "iron" || L.leg === "square" ? [IRON, 9] : [L.wood];
      Q(0, 0, 0, w, h, .06, L.woodD); for (const s of [-1, 1]) Q(s * (w / 2 + .02), 0, 0, .05, h, d, ...m);
      [.08, .5, .92, 1.3].forEach((z, i) => {
        for (const b of [-1, 1]) {
          Q(0, z - .02, b * d / 4, w, .04, d / 2 - .04, L.wood); Q(0, z - .022, b * (d / 2 - .03), w, .045, .012, L.accent);
          if (i < 3) for (let k = 0, u = -w / 2 + .06; k < 6; k++) { const bw = .14 + .1 * hash(seed, i, b, k); Q(u + bw / 2, z + .02, b * d / 4, bw, .12 + .18 * hash(seed, k, i, b), d / 2 - .12, pick([C.teal, C.terracotta, C.cream, C.ochre, DUSTY, C.olive, CARD], hash(seed, b, k, i))); u += bw + .04; }
        }
      });
      Q(0, h + .04, 0, .6, .22, .04, C.cream); Q(0, h + .12, -.025, .6, .06, .012, L.accent);
    },
    clothesrail(Q, L) {
      const w = 1.4, h = 1.5, m = L.leg === "turned" ? [L.wood] : [IRON, 9];
      for (const s of [-1, 1]) { if (L.leg === "turned") leg(Q, L, s * w / 2, 0, h); else Q(s * w / 2, .05, 0, .044, h - .05, .044, ...m); Q(s * w / 2, .005, 0, .08, .05, .5, ...m); if (L.round) Q(s * w / 2, 0, 0, .06, .06, .06, DARK); }
      Q(0, h - .022, 0, w, .044, .044, ...m); Q(0, .18, 0, w - .06, .04, .4, L.wood);
      for (let i = 0; i < 7; i++) Q(-w / 2 + .16 + i * (w - .32) / 6, .62, 0, .12, .8, .46, [C.teal, C.terracotta, C.cream, C.olive, DUSTY, C.plum, C.ochre][i]);
      Q(.3, .22, 0, .4, .2, .3, CARD); Q(-.3, .22, 0, .22, .1, .26, C.terracotta);
    },
    shoptable(Q, L) {
      tbl(Q, L, 1.2, .8, .78);
      for (let i = 0; i < 3; i++) for (let j = 0; j < 4; j++) Q(-.35 + i * .35, .7775 + j * .05, 0, .28, .045, .24, j % 2 ? [DUSTY, C.ochre, C.cream][i] : [C.teal, C.cream, C.terracotta][i]);
      Q(0, .78, -.3, .16, .1, .02, C.cream);
    },
    aboard(Q, L) {
      const m = iw(L);
      for (const s of [-1, 1]) { Q(0, 0, s * .145, .62, .5, .05, ...m); Q(0, .5, s * .055, .62, .5, .05, ...m); Q(0, .13, s * .172, .5, .37, .012, CHALK); Q(0, .5, s * .082, .5, .37, .012, CHALK); }
      [[.75, .34], [.6, .26], [.45, .3], [.3, .2]].forEach(([z, ww], i) => Q(0, z - .0175, z > .5 ? -.092 - (.75 - z) * .2 : -.18, ww, .035, .008, i === 1 ? C.ochre : C.cream));
      if (L.round) Q(0, .94, 0, .62, .08, .08, L.wood);
    },
    dressform(Q, L) {
      const m = L.leg === "iron" || L.leg === "square" ? [IRON, 9] : [L.wood], form = L.leg === "square" ? C.cream : L.cloth;
      if (L.leg === "turned" || L.leg === "taper") for (let i = 0; i < 3; i++) { const a = i * 2 * Math.PI / 3; Q(Math.cos(a) * .14, 0, Math.sin(a) * .14, .04, .42, .04, ...m); }
      else oc(Q, 0, 0, 0, .44, .04, ...m);
      Q(0, .04, 0, .044, .96, .044, ...m); Q(0, 1.0, 0, .32, .25, .22, form); Q(0, 1.25, 0, .38, .35, .26, form); Q(0, 1.6, 0, .1, .08, .1, L.knob);
      Q(0, .7, -.13, .36, .5, .05, L.accent); Q(.15, 1.17, -.1, .03, .5, .02, C.ochre);
    },
    // ---- café
    cafecounter(Q, L) {
      const w = 1.8, d = .65, h = 1.05;
      if (L.round) { Q(0, 0, 0, w + .1, h - .05, d + .1, L.wood); Q(0, h - .07, 0, w + .14, .08, d + .12, L.woodD); }
      else { cabinet(Q, L, w, d, h - L.top, 4); Q(0, h - L.top, 0, w + .06, L.top, d + .06, L.wood); Q(0, .178, -d / 2 - .12, w, .044, .044, IRON, 9); }
      for (const u of [-.6, -.35]) oc(Q, u, h, 0, .24, .08, PORC);
      oc(Q, -.6, h + .08, 0, .22, .09, C.cream); oc(Q, -.6, h + .17, 0, .224, .02, C.terracotta);
      Q(.1, h, .05, .1, .28, .1, C.cream); Q(.55, h, 0, .16, .1, .16, IRON, 9);
    },
    coffeemachine(Q, L) {
      const w = .6, d = .48, h = .55, z = .9, m = L.leg === "square" || L.leg === "taper" ? [ENAMEL] : [IRON, 9];
      Q(0, 0, 0, .8, .9, .55, L.wood);
      if (L.round) Q(0, z - .04, 0, w + .06, h + .08, d + .04, ...m);
      else if (L.panels) { oc(Q, 0, z, .06, .42, h + .1, IRON, 9); Q(0, z + h - .01, .06, .38, .22, .38, L.knob); Q(0, z + h + .19, .06, .08, .1, .08, L.knob); }
      else Q(0, z, 0, w, h, d, ...m);
      for (const u of [-.14, .14]) { Q(u, z + .26, -d / 2 - .02, .09, .08, .12, IRON, 9); Q(u, z + .25, -d / 2 - .14, .044, .044, .16, C.brown); Q(u, z + .02, -d / 2 - .02, .09, .07, .09, C.cream); }
      Q(0, z, -d / 2 - .02, w - .08, .02, .14, IRON, 9); Q(0, z + h - .17, -d / 2 - .01, .1, .1, .04, PORC); Q(.32, z, .12, .16, .16, .16, C.brown);
    },
    banquette(Q, L) {
      const w = 1.8, d = .6, seat = .46, cw = w / 3;
      if (L.leg === "iron") { Q(0, .095, 0, w, .05, d, IRON, 9); legs(Q, L, w, d, .12, .04); Q(0, .14, 0, w - .04, .12, d - .04, L.wood); }
      else Q(0, 0, 0, w, .32, d, L.wood);
      for (let i = 0; i < 3; i++) {
        const cu = -w / 2 + cw * (i + .5);
        Q(cu, seat - .13, -.04, cw - .02, .14, d - .12, L.cloth);
        if (L.buttons) { Q(cu, seat + .08, d / 2 - .08, cw - .03, .48, .14, L.cloth); Q(cu, seat + .34, d / 2 - .16, .04, .04, .03, L.clothD); }
        else Q(cu, seat + .09, d / 2 - .08, cw - .03, .42, .14, L.cloth);
      }
      Q(0, seat - .05, d / 2 + .02, w, .7, .06, L.wood); if (L.panels) Q(0, seat + .645, d / 2 + .02, w + .04, .05, .1, L.wood);
    },
    cafetable(Q, L) {
      const h = .74;
      if (L.round || L.panels) { oc(Q, 0, 0, 0, .52, .05, IRON, 9); Q(0, .04, 0, .06, h - .08, .06, IRON, 9); oc(Q, 0, h - .045, 0, .68, .045, L.panels ? PORC : L.wood); }
      else tbl(Q, L, .7, .7, h, false);
      Q(-.12, h, -.1, .1, .085, .1, C.teal); Q(.13, h, .1, .1, .085, .1, C.cream);
      Q(.1, h, -.15, .2, .015, .2, C.cream); Q(.1, h + .015, -.15, .08, .05, .06, C.ochre); Q(-.05, h, .12, .06, .1, .06, PORC); Q(-.05, h + .11, .12, .06, .06, .06, C.terracotta);
    },
    menu(Q, L, seed) {
      Q(0, 1.15, 0, 1.2, .9, .05, ...iw(L)); Q(0, 1.2, -.03, 1.1, .8, .02, CHALK); Q(0, 1.845, -.045, .5, .07, .01, C.cream);
      for (let i = 0; i < 5; i++) { const y = 1.74 - i * .1 - .0175; Q(-.12, y, -.045, .35 + .2 * hash(seed, i), .035, .01, C.cream); Q(.38, y, -.045, .12, .035, .01, C.ochre); }
      if (L.round) Q(-.36, 1.27, -.045, .12, .1, .01, C.cream);
    },
    cakestand(Q, L) {
      tbl(Q, L, .6, .6, .9, true);
      [[.9, .2], [1.12, .15], [1.3, .1]].forEach(([z, r], i) => {
        oc(Q, 0, z, 0, 2 * r, .025, PORC);
        if (i < 2) Q(0, z, 0, .04, .22 - i * .04, .04, ...(L.round ? [L.knob] : [IRON, 9]));
        for (let j = 0; j < 4 - i; j++) { const a = j * 2 * Math.PI / (4 - i); Q(Math.cos(a) * r * .6, z + .03, Math.sin(a) * r * .6, .08, .06, .08, [C.ochre, C.terracotta, C.cream, C.plum][j]); }
      });
      Q(0, 1.345, 0, .07, .07, .07, C.terracotta);
    },
    // ---- bedroom
    blanketbox(Q, L) {
      const w = 1.1, d = .45, h = .46;
      if (L.round) { Q(0, .02, 0, w + .06, h + .02, d + .06, L.cloth); legs(Q, L, w - .1, d - .06, .06, .05); }
      else if (L.buttons) { legs(Q, L, w, d, .1, .05); Q(0, .1, 0, w, h - .1, d, L.cloth); for (let i = 0; i < 3; i++) Q(-.35 + i * .35, h - .01, 0, .045, .03, .045, L.clothD); }
      else {
        let z0 = 0; if (L.leg === "iron") { legs(Q, L, w, d, .09, .03); Q(0, .04, 0, w, .04, d, IRON, 9); z0 = .08; }
        Q(0, z0, 0, w, h - z0, d, L.wood); Q(0, h, 0, w + .03, .05, d + .03, L.wood);
        if (L.straps) for (const u of [-.35, .35]) Q(u, 0, 0, .05, h, d + .02, IRON, 9);
        Q(0, h - .12, -d / 2 - .04, .12, .04, .04, ...(L.straps ? [IRON, 9] : [L.knob]));
      }
      Q(.2, h + .035, 0, .5, .07, d - .04, L.accent);
    },
    vanity(Q, L) {
      const w = 1.1, d = .48, h = .75, mz = h + .42, m = iw(L);
      if (L.round) { legs(Q, L, w, d, h - .05, .08); Q(0, h - .06, 0, w + .06, .08, d + .04, L.wood); } else tbl(Q, L, w, d, h, false);
      for (const s of [-1, 1]) { Q(s * (w / 2 - .2), h - .19, 0, .34, .14, d - .04, L.wood); Q(s * (w / 2 - .2), h - .15, -d / 2 - .03, .055, .055, .05, ...(L.straps ? [IRON, 9] : [L.knob])); }
      if (L.round) { Q(0, mz - .36, d / 2 - .06, .72, .72, .05, L.wood); Q(0, mz - .3, d / 2 - .09, .6, .6, .02, MIRROR); }
      else {
        Q(0, mz - .37, d / 2 - .06, .66, .74, .05, ...m); Q(0, mz - .31, d / 2 - .09, .54, .62, .02, MIRROR);
        if (L.panels) for (const s of [-1, 1]) Q(s * .47, mz - .28, d / 2 - .1, .26, .56, .04, L.wood, 19, s * .35);
      }
      [[-.3, C.terracotta], [-.2, C.cream], [.28, C.plum]].forEach(([u, c], i) => Q(u, h, -.04, .08, .12 - i * .02, .08, c));
      Q(.12, h, -.1, .2, .03, .06, C.brown);
      stool(at(Q, 0, -.55), L, .45, .4);
    },
    screen(Q, L) {
      const pw = .5, h = 1.7, m = iw(L);
      for (let i = 0; i < 3; i++) {
        const a = [-.45, 0, .45][i], q = turn(Q, (i - 1) * (pw / 2 + pw / 2 * Math.cos(.45)), Math.abs(i - 1) * pw / 2 * Math.sin(.45), a);
        for (const s of [-1, 1]) q(s * (pw / 2 - .025), .04, 0, .05, h, .05, ...m);
        if (L.round) q(0, .15, 0, pw - .04, h - .1, .04, L.cloth);
        else { q(0, .225, 0, pw - .08, h - .25, .025, i === 1 ? L.accent : L.cloth); q(0, h + .005, 0, pw, .05, .05, ...m); q(0, .175, 0, pw, .05, .05, ...m); if (L.panels) q(0, h + .05, 0, .1, .1, .06, L.wood); }
      }
      Q(pw * .94, 1.05, -.28, .3, .7, .05, L.round ? C.cream : L.accent);
    },
    cheval(Q, L) {
      const w = .55, h = 1.4, m = iw(L);
      for (const s of [-1, 1]) { const u = s * (w / 2 + .08); if (L.leg === "turned") leg(Q, L, u, 0, 1.6); else Q(u, .05, 0, .05, 1.55, .05, ...m); Q(u, 0, 0, .07, .06, .5, ...m); Q(s * (w / 2 + .05), .965, 0, .07, .07, .07, L.knob); }
      Q(0, .17, 0, w + (L.round ? .08 : .06), h + .06, L.round ? .06 : .05, ...(L.round ? [L.wood] : m)); Q(0, .23, -.03, w - .06, h - .06, .02, MIRROR);
    },
    // ---- living
    records(Q, L) {
      const w = 1.1, d = .45, h = .7, z0 = base(Q, L, w, d), lift = L.panels ? .06 : 0, side = iw(L);
      for (const s of [-1, 0, 1]) Q(s * (w / 2 - .025), z0, 0, .05, h - z0, d, ...(s ? side : [L.wood]));
      Q(0, z0, 0, w, .04, d, L.wood); Q(0, h - .0425, 0, w + .02, .045, d + .02, L.wood); Q(0, z0, d / 2 - .015, w - .04, h - z0, .03, L.woodD);
      Q(w / 4, z0 + .03, 0, w / 2 - .06, h - z0 - .06, d - .04, L.wood); if (L.panels) Q(0, h, .01, w + .07, .06, d + .05, L.wood);
      for (let i = 0; i < 10; i++) Q(-w / 2 + .08 + i * .042, z0 + .04, -.02, .03, .32, d - .12, [C.terracotta, C.teal, C.cream, C.ochre, C.plum, DUSTY][i % 6]);
      Q(w / 4, z0 + .05, -d / 2 - .012, w / 2 - .1, h - z0 - .1, .03, L.panels ? L.woodD : L.wood); Q(.08, (z0 + h) / 2, -d / 2 - .05, .05, .05, .04, ...(L.straps ? [IRON, 9] : [L.knob]));
      Q(.15, h + lift, 0, .42, .12, .34, ...(L.straps ? [IRON, 9] : [L.knob])); oc(Q, .12, h + lift + .12, 0, .28, .012, INK);
      if (L.panels || L.round) { Q(-.25, h + lift, .05, .08, .15, .08, C.ochre); Q(-.25, h + lift + .13, .05, .32, .3, .2, L.wood); Q(-.25, h + lift + .2, -.055, .16, .16, .012, L.cloth); }
      else { Q(-.3, h + lift, .02, .26, .36, .24, L.wood); Q(-.3, h + lift + .06, -.105, .18, .18, .012, L.straps ? DARK : L.clothD); }
    },
    magrack(Q, L) {
      const w = .45, d = .3, m = iw(L);
      if (L.round) oc(Q, 0, 0, 0, .46, .36, WICKER);
      else { for (const s of [-1, 1]) Q(s * w / 2, 0, 0, .04, .44, d, ...m); Q(0, .04, 0, w, .04, d, L.wood); Q(0, .08, 0, w, .34, .03, L.wood); Q(0, .478, 0, w, .044, .044, ...m); }
      for (let i = 0; i < 4; i++) Q(-.08 + i * .05, .1, (i % 2 ? -1 : 1) * .07, .024, .3, .22, [C.teal, C.terracotta, C.cream, C.ochre][i], 19, -.1 * (i - 1.5));
    },
    // ---- decoration
    smallplants(Q, L, seed) { Q(0, 0, 0, .8, .02, .28, L.wood); [[-.25, .5], [0, .7], [.28, .45]].forEach(([u, s], i) => pot(Q, L, u, .02, 0, s, seed + i)); },
    clock(Q, L) {
      let fy, fv, fr;
      if (L.panels) {                                         // Old Town: a long-case clock on the floor
        Q(0, 0, 0, .42, 1.8, .28, L.wood); Q(0, 1.75, 0, .5, .34, .34, L.wood); Q(0, 2.09, 0, .1, .12, .1, L.knob);
        Q(0, .7, -.145, .2, .5, .012, L.woodD); disc(Q, 0, .8, -.16, .06, .02, L.knob); fy = 1.92; fv = -.175; fr = .15;
      } else {
        fy = 1.8; fv = -.04; fr = L.round ? .22 : .18; const rim = iw(L);
        if (L.leg === "square") Q(0, fy - fr - .03, 0, fr * 2 + .06, fr * 2 + .06, .06, ...rim); else disc(Q, 0, fy, 0, fr + .04, .06, ...rim);
      }
      disc(Q, 0, fy, fv, fr, .012, C.cream);
      Q(.03, fy, fv - .015, .022, fr * .55, .01, INK); Q(-.02, fy, fv - .02, .02, fr * .8, .01, INK);
    },
    vases(Q, L) {
      const k = L.round ? .7 : 1, tz = .7;
      if (L.round) oc(Q, 0, 0, 0, .5, .7, L.wood); else Q(0, 0, 0, .7, .7, .32, L.wood);
      for (const [u, c, r, h] of [[-.18, C.teal, .09, .36], [.02, C.terracotta, .12, .24], [.2, C.cream, .07, .3]]) { Q(u * k, tz, 0, 2 * r, h, 2 * r, c); if (L.round) Q(u * k, tz + h * .05, 0, r * 2.4, h * .6, r * 2.4, c); }
      Q(-.18 * k, tz + .3, 0, .04, .3, .04, LEAF);
      for (let i = 0; i < 3; i++) Q(-.18 * k + (i - 1) * .08, tz + .58, 0, .09, .08, .09, [C.terracotta, C.cream, C.plum][i]);
    },
    candles(Q, L) {
      const m = L.leg === "iron" ? [IRON, 9] : [L.knob], hh = [.2, .26, .16];
      let us, ys;
      if (L.panels) { oc(Q, 0, 0, 0, .16, .04, ...m); Q(0, .04, 0, .044, .26, .044, ...m); Q(0, .278, 0, .32, .044, .044, ...m); us = [-.16, 0, .16]; ys = [.3, .3, .3]; }
      else { us = [-.14, 0, .14]; ys = [.05, .11, .17]; us.forEach((u, i) => Q(u, 0, 0, L.round ? .14 : .12, ys[i], L.round ? .14 : .12, ...m)); }
      us.forEach((u, i) => { Q(u, ys[i], 0, .06, hh[i], .06, C.cream); Q(u, ys[i] + hh[i] + .005, 0, .035, .06, .035, GLOW, 2); });
    },
    curtains(Q, L) {
      const w = 2.3, z = 2.35, m = iw(L), cl = L.straps ? L.accent : L.cloth;
      Q(0, z - .025, 0, w + .2, .05, .05, ...m);
      for (const s of [-1, 1]) {
        Q(s * (w / 2 + .12), z - .045, 0, .09, .09, .09, ...(L.panels ? [L.knob] : m));
        for (let k = 0; k < 5; k++) Q(s * (w / 2 - .2) - .25 + .1 * (k + .5), z - .04 - 2.2, .04 + (k % 2 ? .012 : -.012), .12, 2.2, .05, cl);
        if (L.panels || L.round) Q(s * (w / 2 - .2), .97, -.02, .54, .06, .08, L.straps ? C.cream : L.accent);
      }
      if (L.panels) Q(0, z - .07, -.03, w + .24, .24, .06, L.cloth);
    },
    wallshelf(Q, L, seed) {
      const w = 1.0, z = 1.4, t = L.leg === "iron" ? .06 : .045;
      Q(0, z - t / 2, -.1, w, t, .22, L.wood);
      for (const s of [-1, 1]) { const u = s * (w / 2 - .12); if (L.leg === "iron") Q(u, z - .15, -.06, .04, .14, .14, IRON, 9); else if (L.panels) Q(u, z - .16, -.06, .05, .16, .16, L.wood); else Q(u, z - .15, -.02, .04, .14, .06, L.wood); }
      for (let k = 0; k < 6; k++) Q(-.42 + k * .065, z + t / 2, -.1, .055, .2 + .1 * hash(seed, k), .2, pick([C.terracotta, C.teal, C.cream, C.ochre, DUSTY, C.olive, C.plum], hash(seed, k, 1)));
      Q(.15, z + t / 2, -.1, .12, .16, .12, C.cream); Q(.35, z + .03, -.1, .12, .1, .1, C.terracotta);
    },
    cushions(Q, L) {
      const cols = L.panels ? [L.cloth, L.accent, C.plum] : [C.terracotta, C.teal, C.ochre];
      [[-.2, 0, .2], [.15, 0, -.15], [0, .16, .05]].forEach(([u, y, a], i) => Q(u, y, 0, .44, .36, .18, cols[i], 19, -a));
      oc(Q, .62, 0, 0, .44, .32, WICKER); Q(.62, .29, 0, .36, .1, .36, L.accent);
    },
    globe(Q, L) {
      const m = iw(L), y = .85;
      if (L.leg === "turned" || L.leg === "taper") for (let i = 0; i < 3; i++) { const a = i * 2 * Math.PI / 3; Q(Math.cos(a) * .1, 0, Math.sin(a) * .1, .044, .64, .044, ...m); }
      else Q(0, 0, 0, .4, .6, .4, ...(L.leg === "iron" ? [L.wood] : m));
      Q(0, y - .21, 0, .3, .42, .3, DUSTY); Q(0, y - .15, 0, .42, .3, .42, DUSTY);
      for (let i = 0; i < 4; i++) { const a = i * 1.6 + .3; Q(Math.cos(a) * .17, y + (i - 1.5) * .08 - .07, Math.sin(a) * .17, .12, .14, .12, [C.olive, C.ochre, C.olive, C.cream][i]); }
      Q(0, y - .25, 0, .03, .5, .03, ...m);
    },
    fruit(Q, L) {
      const tz = .58, tp = L.leg === "square" || L.leg === "taper" ? [C.teal] : L.panels ? [PORC] : [IRON, 9];
      tbl(Q, L, .5, .5, tz);
      Q(-.08, tz, 0, .24, .08, .24, L.round ? C.teal : C.ochre);
      for (let i = 0; i < 4; i++) Q(-.08 + (i - 1.5) * .06, tz + .08, (i % 2) * .05 - .02, .08, .08, .08, [C.terracotta, C.olive, C.ochre, C.terracotta][i]);
      Q(.12, tz + .005, .05, .18, .15, .15, ...tp); Q(.12, tz + .135, .05, .08, .05, .08, ...tp); Q(.21, tz + .05, .05, .08, .04, .04, ...tp); Q(.15, tz, -.14, .09, .07, .09, C.cream);
    },
    bust(Q, L) {
      const m = L.leg === "iron" ? [IRON, 9] : [PORC];
      if (L.leg === "iron") Q(0, 0, 0, .36, 1.0, .36, STONE_D);
      else if (L.round) oc(Q, 0, 0, 0, .34, 1.0, L.wood);
      else { Q(0, 0, 0, .36, 1.0, .36, L.panels ? STONE : L.wood); if (L.panels) Q(0, .99, 0, .42, .06, .42, STONE); }
      Q(0, 1.04, 0, .34, .22, .2, ...m); Q(0, 1.2, 0, .1, .12, .1, ...m); Q(0, 1.29, 0, .2, .26, .22, ...m); Q(0, 1.39, -.11, .04, .06, .05, ...m);
    },
    // the television cabinet of the sample houses (house_samples_19.py), which the showroom sets do not use
    tv(Q, L) { cabinet(Q, L, 1.3, .45, .55, 2); Q(0, .55, 0, .72, .5, .44, L.round ? ENAMEL : L.panels ? L.wood : IRON, L.panels ? 19 : 9); Q(-.07, .62, -.23, .46, .36, .02, [.12, .15, .16]); for (const s of [-1, 1]) Q(s * .12, 1.05, .05, .025, .3, .025, IRON, 9, s * .5); },
  };
  // Blender kit names (furniture_kit_18.py, "k17:" for kit 17) to these, so a house's furniture list reads as the Blender one
  const NAMES = { l_tv: "tv", l_fireplace: "fireplace", o_painting: "painting", l_sidetable: "sidetable", l_piano: "piano", o_floorplant: "floorplant",
    h_console: "console", h_mirror: "mirror", h_shoebench: "shoebench", h_hooks: "hooks", h_coatstand: "coatstand", h_umbrella: "umbrella",
    k_counter: "counter", k_sink: "sink", k_stove: "stove", k_wall: "wallcupboard", k_fridge: "fridge", k_potshelf: "potshelf",
    d_pendant: "pendant", b_bath: "bath", b_toilet: "toilet", b_basin: "basin", b_mirror: "mirrorcab", b_basket: "basket",
    r_chest: "chest", r_single: "single", s_desk: "desk", s_chair: "chair", g_bistro: "bistro", "k17:dining": "dining",
    o_smallplants: "smallplants", o_clock: "clock", o_vases: "vases", o_wallshelf: "wallshelf", o_globe: "globe", o_fruit: "fruit", o_bust: "bust",
    o_cushions: "cushions", o_candles: "candles", o_curtains: "curtains", b_rail: "towelrail", d_dresser: "dresser", d_sideboard: "sideboard",
    d_bench: "bench", d_display: "display", d_barstool: "barstool", s_lamp: "desklamp", s_pinboard: "pinboard", s_shelving: "shelving", s_filing: "filing",
    c_bunk: "bunk", c_cot: "cot", c_toyshelf: "toyshelf", c_table: "kidtable", c_horse: "rockinghorse", c_toychest: "toychest",
    g_bench: "gardenbench", g_planter: "planter", g_parasol: "parasol", g_lounger: "lounger", g_plantstand: "plantstand",
    p_counter: "shopcounter", p_gondola: "gondola", p_rail: "clothesrail", p_table: "shoptable", p_form: "dressform", p_aboard: "aboard",
    f_counter: "cafecounter", f_machine: "coffeemachine", f_menu: "menu", f_banquette: "banquette", f_table: "cafetable", f_cakestand: "cakestand",
    r_blanket: "blanketbox", r_vanity: "vanity", r_screen: "screen", r_cheval: "cheval", l_records: "records", l_magrack: "magrack" };
  // what each piece takes up, for walking into it: [width across, depth front to back, height]; pieces on the wall or the
  // ceiling (painting, mirror, hooks, wall cupboard, mirror cabinet, pendant) and rugs are left out: you walk under or over them
  const SIZE = { l_tv: [1.3, .45, 1.1], "k17:sofa": [2.0, .92, 1.0], "k17:armchair": [.95, .92, 1.0], "k17:coffee": [1.1, .6, .45], "k17:bed": [1.6, 2.2, .6],
    "k17:bedside": [.48, .42, .6], "k17:wardrobe": [1.1, .6, 2.1], "k17:bookcase": [.9, .34, 1.8], "k17:floor_lamp": [.34, .34, 1.7],
    "k17:dining": [1.4, 1.7, .95], l_fireplace: [1.7, .6, 1.2], l_sidetable: [.5, .5, .6], l_piano: [1.45, .58, 1.25], o_floorplant: [.4, .4, 1.0],
    h_console: [1.0, .34, .8], h_shoebench: [1.0, .36, .5], h_coatstand: [.42, .42, 1.8], h_umbrella: [.3, .3, .6],
    k_counter: [1.84, .64, .9], k_sink: [1.14, .64, .9], k_stove: [.75, .62, .9], k_fridge: [.68, .66, 1.75], k_potshelf: [1.0, .3, 1.1],
    b_bath: [1.76, .84, .65], b_toilet: [.44, .56, .8], b_basin: [.62, .46, .9], b_basket: [.5, .5, .6],
    r_chest: [.95, .5, 1.05], r_single: [.99, 2.1, .6], s_desk: [1.35, .68, .75], s_chair: [.48, .46, .95], g_bistro: [1.7, .64, .9],
    // kit 18's other pieces; the small plants, cushions, candles and desk lamp, the curtains and the wall pieces (clock, wall
    // shelf, pinboard, menu board) are left out. The vases and the fruit bowl stand on their own plinth and side table.
    o_vases: [.7, .32, 1.3], o_globe: [.42, .42, 1.1], o_fruit: [.5, .5, .75], o_bust: [.42, .42, 1.55], b_rail: [.76, .32, .92],
    d_dresser: [1.24, .5, 2.1], d_sideboard: [1.6, .5, .9], d_bench: [1.44, .44, .5], d_display: [.96, .45, 1.9], d_barstool: [.42, .42, .8],
    s_shelving: [1.04, .4, 1.9], s_filing: [.48, .62, 1.4], c_bunk: [1.06, 2.1, 1.8], c_cot: [.76, 1.31, 1.0], c_toyshelf: [1.1, .36, .85],
    c_table: [1.4, .7, .55], c_horse: [1.15, .4, .9], c_toychest: [.9, .52, .56], g_bench: [1.58, .6, .95], g_planter: [1.2, .42, .6],
    g_parasol: [.52, .52, 2.4], g_lounger: [.68, 1.9, .8], g_plantstand: [1.0, .36, 1.2], p_counter: [1.86, .68, 1.0], p_gondola: [1.7, .8, 1.6],
    p_rail: [1.48, .5, 1.55], p_table: [1.2, .8, .8], p_form: [.45, .45, 1.65], p_aboard: [.62, .45, 1.0], f_counter: [1.9, .75, 1.1],
    f_machine: [.8, .55, 1.5], f_banquette: [1.8, .66, 1.15], f_table: [.7, .7, .76], f_cakestand: [.6, .6, 1.4], r_blanket: [1.13, .48, .53],
    r_vanity: [1.1, 1.5, 1.55], r_screen: [1.5, .5, 1.8], r_cheval: [.8, .5, 1.6], l_records: [1.12, .47, .75], l_magrack: [.5, .32, .5] };
  // what a person can sit or lie on: [use, height of the seat or mattress top, label]
  const SEAT = { "k17:sofa": ["sit", .45, "Sit on the sofa"], "k17:armchair": ["sit", .45, "Sit in the armchair"], "k17:bed": ["lie", .55, "Lie on the bed"],
    "k17:dining": ["sit", .49, "Sit at the table"], s_chair: ["sit", .52, "Sit at the desk"], r_single: ["lie", .55, "Lie on the bed"],
    h_shoebench: ["sit", .48, "Sit on the bench"], l_piano: ["sit", .48, "Sit at the piano"], g_bistro: ["sit", .47, "Sit at the bistro table"],
    d_bench: ["sit", .47, "Sit on the bench"], d_barstool: ["sit", .77, "Sit on the stool"], c_bunk: ["lie", .42, "Lie on the bunk"],
    c_table: ["sit", .3, "Sit at the little table"], g_bench: ["sit", .47, "Sit on the bench"], g_lounger: ["lie", .42, "Lie on the lounger"],
    f_banquette: ["sit", .47, "Sit on the banquette"], r_vanity: ["sit", .45, "Sit at the dressing table"], r_blanket: ["sit", .5, "Sit on the blanket box"] };
  function draw(kind, Q, L, seed) {
    const k17 = { "k17:sofa": q => P().sofa(q, L, 2.0), "k17:armchair": q => P().sofa(q, L, .95), "k17:coffee": q => P().coffee(q, L), "k17:bed": q => P().bed(q, L),
      "k17:bedside": q => P().bedside(q, L), "k17:wardrobe": q => P().wardrobe(q, L), "k17:bookcase": q => P().bookcase(q, L, seed), "k17:floor_lamp": q => P().floorLamp(q, L),
      "k17:rug": q => P().rug(q, L) }[kind];
    if (k17) return k17(Q);
    const fn = D[NAMES[kind] || kind];
    if (fn) fn(Q, L, seed);
  }
  return { draw, NAMES, SIZE, SEAT };
})();
