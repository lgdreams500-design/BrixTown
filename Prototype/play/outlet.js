"use strict";
// The furniture outlet outside the East Gate (FURNITURE FURN-020, FURN-021; F-FURN-STORE; Q-30). Its place, size, door,
// room sets and the places its staff and customers use are data (World/city_01/blocks/b_8_4.json, made by
// Tools/worldgen/add_outlet_01.py); this file only draws them. The look follows Art/StyleTests/furniture_kit_17/showroom.png
// and the furniture the four style lines of ADR-0026 (ART_DIRECTION 2.6, Art/Reference/furniture_lines.png):
//
//   a long hall of sandstone and plaster with tall arched windows and a painted sign band, under timber trusses and
//   skylights; inside, by the door, the tills and a cafe; a painted walking route with arrows; four room sets of three
//   plaster walls, one per line (Plain, Old Town, Workshop, Curve), each a living room with a bed corner, a price tag on a
//   stand in front; sales desks between them; behind them the warehouse, tall timber racks of flat-pack boxes; lanterns
//   hanging under the trusses. South of the hall, a car park.
//
// Furniture here is boxes, as everywhere in the slice: the rounded edges, mottling and light of the reference renders are
// the gaps listed in docs/art/INTERIOR_STYLE.md section 6 (R1 to R9), not something this file can close on its own.
//
// Loaded before play.js; everything here runs when play.js calls in (building, furnish, prop), by which time its globals
// (world, signs, solid, lights, labels, C, hash, pick, sideOf, fbox, SP, archHead, detailOf, nameCell, lettering, Cars,
// Y_FLOOR, Y_PAVE, DOOR_DIR, yawOf, BLOBS) all exist.
var Outlet = (() => {
  const IRON = [.10, .10, .11], INW = [.70, .63, .53], FLOORC = [.60, .56, .50], WOOD = [.34, .23, .15], CARD = [.58, .48, .32];
  const PALE = [.6, .5, .34], MID = [.32, .2, .12], DARK = [.17, .12, .09], HONEY = [.51, .38, .2];
  // The four lines (ADR-0026): the same parts, different numbers and colours (rule I6). Colours as furniture_kit_17.py.
  const LINES = {
    plain: { label: "Plain", wood: PALE, woodD: HONEY, knob: PALE, frame: [.2, .36, .37], cloth: [.22, .38, .39], clothD: [.2, .36, .37], accent: [.44, .24, .18],
      rug: [.62, .57, .47], leg: "square", top: .045, arm: .16, arms: "slab", head: "slab", round: false, buttons: false, panels: false, straps: false, extra: "stool" },
    oldtown: { label: "Old Town", wood: MID, woodD: DARK, knob: HONEY, frame: [.44, .24, .18], cloth: [.47, .26, .19], clothD: [.3, .15, .11], accent: [.3, .33, .2],
      rug: [.3, .24, .34], leg: "turned", top: .055, arm: .2, arms: "rolled", head: "panel", round: false, buttons: true, panels: true, straps: false, extra: "trunk" },
    workshop: { label: "Workshop", wood: DARK, woodD: MID, knob: MID, frame: IRON, cloth: [.62, .57, .46], clothD: [.66, .61, .5], accent: [.1, .14, .24],
      rug: [.44, .24, .18], leg: "iron", top: .07, arm: .14, arms: "iron", head: "rails", round: false, buttons: false, panels: false, straps: true, extra: "crate" },
    curve: { label: "Curve", wood: HONEY, woodD: PALE, knob: HONEY, frame: [.3, .38, .52], cloth: [.32, .4, .54], clothD: [.3, .38, .52], accent: [.52, .4, .2],
      rug: [.2, .36, .37], leg: "taper", top: .05, arm: .2, arms: "tub", head: "arch", round: true, buttons: false, panels: false, straps: false, extra: "pouffe" },
  };

  // ---- a piece's own frame: u across it, v from its front (-) to its back (+); rot turns the front to face
  // 0 north, 1 east, 2 south, 3 west. Boxes stay square to the hall, so a quarter turn swaps their width and depth.
  function placer(B, ox, oz, rot) {
    const c = [1, 0, -1, 0][rot], s = [0, 1, 0, -1][rot], sw = rot % 2 === 1;
    const at = (u, v) => [ox + u * c - v * s, oz + u * s + v * c];
    const Q = (u, y, v, su, sy, sv, col, f, yaw) => { const [x, z] = at(u, v); B(x, y, z, sw ? sv : su, sy, sw ? su : sv, col, yaw || 0, f); };
    Q.at = at;
    return Q;
  }
  const oct = (Q, u, y, v, d, h, col, f) => { Q(u, y, v, d * .83, h, d * .83, col, f); Q(u, y, v, d * .83, h, d * .83, col, f, Math.PI / 4); };   // a round top, as the slice draws round tables

  function leg(Q, L, u, v, h, col) {
    col = col || L.wood;
    if (L.leg === "iron") return Q(u, 0, v, .04, h, .04, IRON, 9);
    Q(u, 0, v, .05, h, .05, col);
    if (L.leg === "turned") { Q(u, h * .55, v, .08, .08, .08, col); Q(u, 0, v, .07, .06, .07, col); }
  }
  function legs(Q, L, w, d, h, inset, col) { for (const [a, b] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) leg(Q, L, a * (w / 2 - inset), b * (d / 2 - inset), h, col); }

  function sofa(Q, L, w) {
    const d = .92, b0 = .12, nc = w > 1.2 ? 2 : 1, cw = (w - 2 * L.arm) / nc;
    legs(Q, L, w - .1, d - .1, b0 + .02, .05);
    if (L.leg === "iron") { Q(0, b0, 0, w, .05, d, IRON, 9); Q(0, b0 + .05, .02, w - .08, .09, d - .08, L.wood); }
    else Q(0, b0, .02, w, .26, d, L.frame);
    for (let i = 0; i < nc; i++) {
      const cu = -w / 2 + L.arm + cw * (i + .5);
      Q(cu, .38, -.06, cw - .03, .14, d - .28, L.cloth); Q(cu, .44, d / 2 - .17, cw - .04, .44, .2, L.cloth);
      if (L.buttons) for (const bu of [-.18, .18]) for (const by of [.62, .8]) Q(cu + bu, by, d / 2 - .28, .05, .05, .03, L.clothD);
    }
    Q(0, b0 + .1, d / 2 - .06, w, .62, .12, L.frame);
    for (const s of [-1, 1]) {
      const au = s * (w / 2 - L.arm / 2);
      if (L.arms === "rolled") { Q(au, b0, 0, L.arm, .44, d - .02, L.frame); Q(au, b0 + .44, 0, L.arm * 1.15, .14, d, L.frame); }
      else if (L.arms === "tub") { Q(au, b0, .02, L.arm * 1.2, .5, d, L.frame); Q(au, b0 + .5, .04, L.arm, .08, d - .12, L.frame); }
      else if (L.arms === "iron") { Q(au, b0 + .05, -d / 2 + .05, .04, .55, .04, IRON, 9); Q(au, b0 + .58, 0, L.arm, .06, d, L.wood); Q(au, b0 + .1, .05, L.arm - .03, .36, d - .2, L.cloth); }
      else Q(au, b0, 0, L.arm, .5, d, L.frame);
    }
    Q(-w / 2 + L.arm + .22, .5, d / 2 - .32, .34, .32, .12, L.accent);
  }
  function coffee(Q, L) {
    const h = .42;
    if (L.round) { Q(0, 0, 0, .3, h - .05, .3, L.wood); oct(Q, 0, h - .05, 0, .9, .05, L.wood); }
    else { legs(Q, L, 1.1, .6, h - L.top, .06); Q(0, h - L.top, 0, 1.1, L.top, .6, L.wood); if (L.leg === "iron") Q(0, .1, 0, 1.0, .04, .5, L.wood); }
    Q(.18, h, .05, .26, .05, .2, [.44, .24, .18]); Q(-.2, h, -.05, .09, .09, .09, [.66, .61, .5]);
  }
  function bed(Q, L) {
    const w = 1.6, d = 2.1, h0 = .18;
    if (L.leg === "iron") { legs(Q, L, w, d, h0, .04); Q(0, h0, 0, w, .05, d, IRON, 9); }
    else { legs(Q, L, w, d, h0, .06); Q(0, h0, 0, w, .16, d, L.wood); }
    Q(0, h0 + .14, 0, w - .06, .2, d - .06, [.78, .74, .66]); Q(0, h0 + .33, -.22, w - .02, .1, d - .5, L.accent);
    for (const s of [-1, 1]) Q(s * .36, h0 + .34, d / 2 - .3, .6, .14, .34, [.85, .82, .76]);
    const hv = d / 2 + .03;
    if (L.head === "arch") { Q(0, 0, hv, w + .06, .95, .12, L.cloth); Q(0, .95, hv, w * .8, .2, .12, L.cloth); Q(0, 1.15, hv, w * .5, .12, .12, L.cloth); }
    else if (L.head === "panel") {
      for (const s of [-1, 1]) { Q(s * w / 2, 0, hv, .07, 1.15, .07, L.wood); Q(s * w / 2, 1.13, hv, .12, .12, .12, L.wood); }
      Q(0, .5, hv, w - .04, .6, .06, L.wood); Q(0, .6, hv - .035, w - .3, .42, .012, L.woodD);
    } else if (L.head === "rails") {
      for (const s of [-1, 1]) Q(s * w / 2, 0, hv, .045, 1.05, .045, IRON, 9);
      for (const y of [.62, 1.0]) Q(0, y, hv, w, .04, .04, IRON, 9);
      for (let i = 0; i < 6; i++) Q(-w / 2 + w * (i + .5) / 6, .62, hv, .03, .38, .03, IRON, 9);
    } else Q(0, .17, hv, w + .04, .9, .07, L.wood);
  }
  function bedside(Q, L) {
    legs(Q, L, .48, .42, .12, .04); Q(0, .12, 0, .48, .43, .42, L.wood);
    Q(0, .3, -.215, .44, .2, .02, L.panels ? L.woodD : L.wood); Q(0, .38, -.24, .06, .05, .05, L.knob);
    Q(.05, .55, .02, .14, .2, .14, L.knob); Q(.05, .75, .02, .3, .2, .3, [.95, .82, .55], 2);   // the lamp: its shade lights the room
  }
  function wardrobe(Q, L) {
    const w = 1.1, d = .6, h = 2.0, p = .12;
    if (L.leg === "turned" || L.leg === "taper") legs(Q, L, w, d, p + .02, .06); else Q(0, 0, .02, w - .04, p, d - .04, L.woodD);
    if (L.round) { Q(0, p, 0, w, h - p - .25, d, L.wood); Q(0, h - .25, 0, w * .82, .15, d, L.wood); Q(0, h - .1, 0, w * .5, .1, d, L.wood); Q(0, p + .2, -d / 2 - .005, .02, h - p - .6, .02, L.woodD); }
    else {
      Q(0, p, 0, w, h - p, d, L.wood);
      for (const s of [-1, 1]) {
        Q(s * w / 4, p + .04, -d / 2 - .012, w / 2 - .03, h - p - .12, .025, L.wood);
        if (L.panels) Q(s * w / 4, p + .13, -d / 2 - .03, w / 2 - .17, h - p - .3, .012, L.woodD);
        if (L.straps) for (const y of [p + .16, h - .2]) Q(s * w / 4, y, -d / 2 - .03, w / 2 - .08, .04, .012, IRON, 9);
      }
      if (L.panels) Q(0, h, 0, w + .1, .08, d + .08, L.wood);
    }
    for (const s of [-1, 1]) Q(s * .07, p + .9, -d / 2 - .05, .05, .07, .05, L.straps ? IRON : L.knob);
  }
  function bookcase(Q, L, seed) {
    const w = .9, d = .34, h = 1.8, side = L.leg === "iron" ? IRON : L.wood;
    for (const s of [-1, 1]) { if (L.leg === "iron") { Q(s * w / 2, 0, -d / 2 + .02, .035, h, .035, IRON, 9); Q(s * w / 2, 0, d / 2 - .02, .035, h, .035, IRON, 9); } else Q(s * (w / 2 - .025), 0, 0, .05, h, d, side); }
    if (L.leg !== "iron") Q(0, 0, d / 2 - .01, w - .06, h, .02, L.woodD);
    [.04, .48, .92, 1.36, h - .05].forEach((y, i) => {
      Q(0, y, 0, w - .02, .045, d - .02, L.wood);
      if (y > h - .1) return;
      let u = -w / 2 + .07; const end = w / 2 - .07 - (i % 2 ? .25 : 0);
      for (let k = 0; u < end - .04; k++) { const bw = .035 + .035 * hash(seed, i, k), bh = .2 + .1 * hash(seed, i, k, "h");
        Q(u + bw / 2, y + .045, 0, bw, bh, .2, pick([C.terracotta, C.teal, C.cream, C.ochre, C.blue, C.olive, C.plum], hash(seed, i, k, "c"))); u += bw + .006; }
    });
  }
  function floorLamp(Q, L) {
    const iron = L.leg === "iron";
    Q(0, 0, 0, .34, .05, .34, iron ? IRON : L.wood, iron ? 9 : 19); Q(0, .05, 0, .045, 1.35, .045, iron ? IRON : L.wood, iron ? 9 : 19);
    if (L.round) oct(Q, 0, 1.38, 0, .44, .34, [.95, .82, .55], 2); else Q(0, 1.38, 0, .4, .3, .4, [.95, .82, .55], 2);
  }
  function rug(Q, L) { if (L.round) oct(Q, 0, .005, 0, 2.0, .012, L.rug); else Q(0, .005, 0, 2.0, .012, 1.4, L.rug); }
  function extra(Q, L) {
    if (L.extra === "crate") { Q(0, 0, 0, .6, .44, .45, L.wood); for (const y of [.06, .3]) Q(0, y, -.228, .58, .07, .012, L.woodD); }
    else if (L.extra === "pouffe") { oct(Q, 0, 0, 0, .6, .3, L.accent); oct(Q, 0, .3, 0, .48, .1, L.accent); }
    else if (L.extra === "trunk") { Q(0, 0, 0, .8, .44, .45, L.wood); Q(0, .44, 0, .78, .1, .4, L.wood); for (const u of [-.25, .25]) Q(u, 0, 0, .05, .56, .47, IRON, 9); }
    else { legs(Q, L, .36, .36, .4, .05); Q(0, .4, 0, .38, .05, .38, L.wood); }
  }

  // one room set: the same places in every line (as furniture_kit_17.py), so only the style changes. [kind, u, v, rot]
  const LAYOUT = [["rug", -.9, .5, 0], ["sofa", -.9, 1.65, 0], ["chair", .55, .45, 3], ["coffee", -.9, .5, 0], ["lamp", -2.4, 1.85, 0],
    ["case", -2.95, -.2, 1], ["bed", 2.05, 1.1, 0], ["side", .95, 1.95, 0], ["ward", 2.85, -1.3, 3], ["extra", 2.05, -.4, 0]];
  const SIZE = { sofa: [2.0, .92, 1.0], chair: [.95, .92, 1.0], coffee: [1.1, .6, .45], bed: [1.6, 2.2, .6], side: [.48, .42, .6], ward: [1.1, .6, 2.1], case: [.9, .34, 1.8], extra: [.8, .45, .5] };
  function furnishSet(B, hard, use, L, ox, oz, F, seed) {
    for (const [kind, u, v, rot] of LAYOUT) {
      const Q = placer(B, ox + u, oz + v, rot);
      ({ rug, sofa: q => sofa(q, L, 2.0), chair: q => sofa(q, L, .95), coffee, lamp: floorLamp, case: q => bookcase(q, L, seed), bed, side: bedside, ward: wardrobe, extra })[kind](Q, L);
      const sz = SIZE[kind];
      if (sz) { const [a, b] = Q.at(-sz[0] / 2, -sz[1] / 2), [c, d] = Q.at(sz[0] / 2, sz[1] / 2); hard(Math.min(a, c), F, Math.min(b, d), Math.max(a, c), F + sz[2], Math.max(b, d)); }
      const face = yawOf(...[[0, -1], [1, 0], [0, 1], [-1, 0]][rot]);
      if (kind === "sofa" || kind === "chair") { const [x, z] = Q.at(0, -.1); use(x, F + .45, z, "sit", kind === "sofa" ? "Try the sofa" : "Try the armchair", face); }
      if (kind === "bed") { const [x, z] = Q.at(0, 0); use(x, F + .62, z, "lie", "Try the bed", 0); }
    }
  }

  // ---- the hall: walls, roof, windows, sign, door (in the town's mesh, at load) ----
  function build(l, side) {
    const [x0, z0, x1, z1] = l.rect, H = l.hall_height || 8.5, t = .5, F = Y_FLOOR, [dx] = l.door, dw = 6, dh = 4.4;
    l.side = side; l.dw = dw; l.H = H; l.sp = null;
    const wallC = [.68, .63, .55];
    // walls: a stone base, plaster above; inside, the room's mottled plaster (material 3, lit as a room)
    const wall = (ax0, az0, ax1, az1, inner) => {
      for (const [y0, y1, c, f] of [[0, 1.6, C.stone, 6], [1.6, H, wallC, 0]]) {
        world.box(ax0, y0, az0, ax1, y1, az1, c, f, { [inner]: [INW, 3], 3: y1 === H ? [C.stoneDark, 0] : null }); solid(ax0, y0, az0, ax1, y1, az1); occl.push([ax0, az0, ax1, az1, y1]);
      }
    };
    wall(x0, z0, dx - dw / 2, z0 + t, 5); wall(dx + dw / 2, z0, x1, z0 + t, 5);                   // north, either side of the door
    world.box(dx - dw / 2, dh, z0, dx + dw / 2, H, z0 + t, wallC, 0, { 5: [INW, 3], 2: [C.stoneDark, 9], 3: [C.stoneDark, 0] }); solid(dx - dw / 2, dh, z0, dx + dw / 2, H, z0 + t);
    wall(x0, z1 - t, x1, z1, 4); wall(x0, z0 + t, x0 + t, z1 - t, 1); wall(x1 - t, z0 + t, x1, z1 - t, 0);
    for (const [a, b, c, d] of [[x0, z0, x1, z0 + .4], [x0, z1 - .4, x1, z1], [x0, z0, x0 + .4, z1], [x1 - .4, z0, x1, z1]]) world.box(a - .05, H, b - .05, c + .05, H + .9, d + .05, C.stoneDark, 0);   // parapet
    // floor: pale slabs with darker joints every 4 m
    world.flat(x0 + t, z0 + t, x1 - t, z1 - t, F, FLOORC, 3);
    for (let x = x0 + 4; x < x1 - 1; x += 4) world.flat(x - .03, z0 + t, x + .03, z1 - t, F + .002, [.48, .45, .4], 3);
    for (let z = z0 + 4; z < z1 - 1; z += 4) world.flat(x0 + t, z - .03, x1 - t, z + .03, F + .002, [.48, .45, .4], 3);
    // roof on timber trusses every 6 m, with a skylight between each pair along the middle of the hall
    const tx = []; for (let x = x0 + 3; x < x1 - 1; x += 6) tx.push(x);
    const sk0 = z0 + 11, sk1 = z1 - 11; l.skyband = [x0, sk0, x1, sk1, H];   // where the daylight comes in (play.js roomLight: surfaces facing it, and near it, get more)
    l.truss = [x0 + 3, 6, .16, .5]; l.wallT = t;           // the trusses (first, spacing, half width, depth) and the wall's thickness, for corner shade (roomLight)
    const roof =(a, b, c, d) => { world.box(a, H, b, c, H + .3, d, [.30, .28, .3], 9, { 2: [mix3(INW, [1, 1, 1], .15), 3] }); solid(a, H, b, c, H + .3, d); };
    roof(x0, z0, x1, sk0); roof(x0, sk1, x1, z1);
    let xa = x0;
    for (let k = 0; k < tx.length - 1; k++) { const a = tx[k] + .9, b = tx[k + 1] - .9; roof(xa, sk0, a, sk1); xa = b;
      for (const [p, q, r, s] of [[a, sk0 - .15, b, sk0], [a, sk1, b, sk1 + .15], [a - .15, sk0, a, sk1], [b, sk0, b + .15, sk1]]) world.box(p, H + .3, q, r, H + .55, s, C.stoneDark, 0); }   // the skylights' curbs
    roof(xa, sk0, x1, sk1);
    for (const x of tx) {
      world.box(x - .16, H - .5, z0 + t, x + .16, H, z1 - t, WOOD, 19, { 2: [WOOD, 19] });            // a truss across the hall
      world.box(x - .05, H - 1.9, z0 + t, x + .05, H - 1.84, z1 - t, IRON, 9, { 2: [IRON, 9] });     // its iron tie
      for (const z of [z0 + 6, (z0 + z1) / 2, z1 - 6]) world.box(x - .06, H - 1.9, z - .06, x + .06, H - .5, z + .06, WOOD, 19);   // and the posts that hang it
    }
    // outside: tall arched windows, a painted sign band over the door, the door's stone surround, lanterns
    const dm = detailOf(l.block);
    for (const s of "nwe") {
      const S = sideOf(l, s);
      for (let a = S.a0 + 4; a < S.a1 - 3; a += 6) { if (s === "n" && Math.abs(a - dx) < dw / 2 + 2.5) continue; SP.win(S, a, 2.0, 2.2, 3.4, 0, .96 + .03 * hash(l.id, s, a), true); archHead(dm, S, a, 5.4, 1.1, .97, true); }
    }
    const S = sideOf(l, "n"), cell = nameCell(l.name);
    fbox(world, S, dx - 15, dx + 15, 5.4, 7.2, 0, .12, C.teal, 0, true);
    if (cell) lettering(signs, SP.at(S, dx, 6.3, .14), SP.n(S), 1.05, cell);
    for (const s of [-1, 1]) fbox(world, S, dx + s * (dw / 2 + .35) - .35, dx + s * (dw / 2 + .35) + .35, 0, dh + .4, 0, .25, C.stoneDark, 6);
    fbox(world, S, dx - dw / 2 - .7, dx + dw / 2 + .7, dh, dh + .5, 0, .25, C.stoneDark, 6, true);
    for (const s of [-1, 1]) SP.lamp(S, dx + s * (dw / 2 + 1.4), 3.0);
    const SS = sideOf(l, "s");                                                                   // the warehouse's loading doors, onto the car park
    for (const a of [x0 + 12, x0 + 24, x1 - 12]) { fbox(world, SS, a - 2.1, a + 2.1, 0, 4.4, 0, .25, C.stoneDark, 6); fbox(world, SS, a - 1.8, a + 1.8, 0, 4.1, .25, .3, C.metal, 9);
      for (let y = .3; y < 4.1; y += .35) fbox(world, SS, a - 1.8, a + 1.8, y, y + .04, .3, .32, [.12, .12, .14], 9); SP.lamp(SS, a, 4.7); }
    // inside, in the town's mesh too: the route, the room sets' walls, tills, cafe counter, desks, racks of boxes, lanterns
    inside0(l, F);
    l.furn = [side, C.teal, INW]; furnish(l, side, C.teal, INW);
  }

  function inside0(l, F) {
    const [x0, z0, x1, z1] = l.rect, H = l.hall_height || 8.5;
    const box = (a, y0, b, c, y1, d, col, f, all) => { world.box(a, y0, b, c, y1, d, col, f == null ? 3 : f); if (!all) solid(a, y0, b, c, y1, d); };
    // the walking route: a painted band with arrows, from the door round past the sets
    const rz0 = z0 + 5.2, rz1 = z0 + 6.6;
    world.flat(x0 + 3, rz0, x1 - 14, rz1, F + .004, [.44, .24, .18], 3);
    for (let x = x0 + 4; x < x1 - 15; x += 3) world.quad([x, F + .007, rz0 + .3], [x + .6, F + .007, (rz0 + rz1) / 2], [x + .6, F + .007, (rz0 + rz1) / 2], [x, F + .007, rz1 - .3], [.66, .61, .5], 3, [0, 1, 0]);
    // room sets: three plaster walls each, a board floor, the line's name over the back wall
    for (const st of l.sets || []) {
      const [a, b, c, d] = st.rect, L = LINES[st.line] || LINES.plain, h = 2.8;
      world.box(a, F, b, c, F + .03, d, [.4, .27, .17], 15, { 2: null });
      box(a - .15, F, d, c + .15, F + h, d + .15, INW); box(a - .15, F, b, a, F + h, d, INW); box(c, F, b, c + .15, F + h, d, INW);
      world.box(a, F, d - .03, c, F + .14, d, WOOD, 19);                                            // skirting
      world.box((a + c) / 2 - 1.7, F + 3.05, d + .02, (a + c) / 2 + 1.7, F + 3.65, d + .08, C.teal, 3);   // the name board
      const cell = nameCell(L.label); if (cell) lettering(signs, [(a + c) / 2, F + 3.35, d], [0, 0, -1], .4, cell);
      for (const [u, v] of [[a + 1.4, b - .9], [c - 1.0, b - .9]]) { box(u - .02, F, v - .02, u + .02, F + 1.0, v + .02, IRON, 9, true); world.box(u - .16, F + .98, v - .03, u + .16, F + 1.2, v, [.66, .61, .5], 3); world.box(u - .16, F + 1.14, v - .035, u + .16, F + 1.19, v - .03, L.cloth, 3); }   // price tags
    }
    // staff places: tills by the door, the cafe counter, desks between the sets, packing tables by the racks
    const o = l.objects;
    (o.work || []).forEach(([x, z]) => {
      if (z < z0 + 5) { const cafe = x > x1 - 9; box(x - (cafe ? 1.7 : 1.1), F, z - 1.2, x + (cafe ? 1.7 : 1.1), F + .95, z - .6, cafe ? C.brown : WOOD, 19);
        world.box(x - (cafe ? 1.75 : 1.15), F + .95, z - 1.25, x + (cafe ? 1.75 : 1.15), F + 1.0, z - .55, cafe ? C.cream : C.teal, 3);
        world.box(x - .2, F + 1.0, z - 1.05, x + .2, F + 1.25, z - .8, cafe ? IRON : C.charcoal, 9); }
      else if (z < z0 + 16) { box(x - .65, F, z - 1.15, x + .65, F + .76, z - .5, WOOD, 19); world.box(x - .15, F + .76, z - 1.0, x + .15, F + .78, z - .78, [.88, .86, .8], 3); }
      else box(x - 1.2, F, z - 1.3, x + 1.2, F + .9, z - .5, WOOD, 19);
    });
    // the warehouse: three rows of tall timber racks with flat-pack boxes in the company's band (rule I9)
    const racks = l.racks = []; l.rackY = [1.8, 3.5, 5];  // their outlines and shelf heights, for their shade (play.js roomLight)
    for (const rz of [z0 + 25.5, z0 + 31.5, z0 + 37]) {
      for (const [a, c] of [[x0 + 3, x0 + 20], [x0 + 24, x0 + 42], [x0 + 46, x1 - 3]]) {
        for (let x = a; x <= c + .01; x += (c - a) / 3) box(x - .06, F, rz - .7, x + .06, F + 5, rz + .7, WOOD, 19);
        [.1, 1.8, 3.5].forEach((y, si) => {
          world.box(a, F + y, rz - .7, c, F + y + .07, rz + .7, WOOD, 19);
          for (let x = a + .1, k = 0; x < c - .5; k++) { const bw = .5 + .6 * hash(l.id, rz, a, si, k), bh = .3 + .9 * hash(l.id, rz, si, k, "h"); if (x + bw > c - .1) break;
            world.box(x, F + y + .07, rz - .55, x + bw, F + y + .07 + bh, rz + .55, CARD, 3); world.box(x, F + y + .07 + bh * .62, rz - .56, x + bw, F + y + .07 + bh * .62 + .08, rz - .55, C.teal, 3); x += bw + .06; }
        });
        solid(a, F, rz - .7, c, F + 5, rz + .7); racks.push([a, rz - .7, c, rz + .7]);
      }
    }
    // lanterns hanging under the trusses (their light is added to the room in inside())
    for (const [x, z] of lanterns(l)) { world.box(x - .02, F + H - 2.8, z - .02, x + .02, H - .5, z + .02, IRON, 9); world.box(x - .22, F + H - 3.3, z - .22, x + .22, F + H - 2.8, z + .22, C.lantern, 2); world.box(x - .26, F + H - 2.8, z - .26, x + .26, F + H - 2.72, z + .26, IRON, 9); }
  }
  function lanterns(l) {
    const [x0, z0, x1, z1] = l.rect, out = [];
    for (let x = x0 + 6; x < x1 - 2; x += 12) for (const z of [z0 + 4, z0 + 17.5, z0 + 29]) out.push([x, z]);
    return out;
  }

  // ---- inside, for furnish(): the furniture near the camera (vis) and, at load, its places, uses and collisions ----
  function inside(l, B, hard, spot, use, RR, dm) {
    const F = Y_FLOOR, H = l.hall_height || 8.5, o = l.objects, z0 = l.rect[1], north = yawOf(0, -1), south = yawOf(0, 1);
    (l.sets || []).forEach((st, k) => { const [a, b, c, d] = st.rect; furnishSet(B, hard, use, LINES[st.line] || LINES.plain, (a + c) / 2, (b + d) / 2, F, l.id + k); });
    for (const [x, z] of o.work || []) spot(l, x, z, 0, north, F);                                // staff face the door
    for (const [x, z] of o.customer || []) spot(l, x, z, 0, z < z0 + 10 ? south : north, F);      // customers face the sets, or the racks
    const cafe = [], seatC = [.66, .61, .5], seats = o.seat || [];
    for (const [x, z] of seats) {                                                                // the cafe: two chairs facing across a small round table
      const mate = seats.filter(q => q[0] !== x || q[1] !== z).sort((p, q) => Math.hypot(p[0] - x, p[1] - z) - Math.hypot(q[0] - x, q[1] - z))[0];
      const pair = mate && Math.hypot(mate[0] - x, mate[1] - z) < 2.2, tx = pair ? (x + mate[0]) / 2 : x, tz = pair ? (z + mate[1]) / 2 : z - .65;
      const fy = yawOf(tx - x, tz - z), fx = Math.sin(fy), fz = -Math.cos(fy);
      B(x, .4, z, .44, .07, .44, seatC); B(x - fx * .2, .47, z - fz * .2, .44, .45, .06, seatC, -fy); for (const [u, v] of [[-.18, -.18], [.18, -.18], [-.18, .18], [.18, .18]]) B(x + u, 0, z + v, .04, .4, .04, IRON, 0, 9);
      if (!cafe.some(([p, q]) => Math.hypot(p - tx, q - tz) < .3)) { cafe.push([tx, tz]); B(tx, .7, tz, .6, .04, .6, WOOD, Math.PI / 4); B(tx, 0, tz, .08, .7, .08, IRON, 0, 9); hard(tx - .3, F, tz - .3, tx + .3, F + .74, tz + .3); }
      spot(l, x, z, 2, fy, F + .45); use(x, F + .45, z, "sit", "Sit down", fy);
    }
    if (RR) for (const [x, z] of lanterns(l)) RR.lamps.push([x, F + H - 3.1, z, 1.1]);           // the hanging lanterns light the hall
  }

  // ---- the car park south of the hall: dark tarmac, painted bays, cars of the Soft box kind (CITY-067) ----
  function carpark(p, b) {
    const [cx, cz] = p.pos, [w, d] = p.size, y = Y_PAVE + .006, a0 = cx - w / 2, c0 = cz - d / 2, rows = p.rows || 2;
    world.flat(a0, c0, a0 + w, c0 + d, y, [.27, .26, .27], 9);
    const bay = 2.6, n = Math.floor((w - 2) / bay), deep = 5, aisle = d - rows * deep, bays = [];
    for (let r = 0; r < rows; r++) {
      const zs = r === 0 ? c0 + .5 : c0 + d - .5 - deep, face = r === 0 ? 1 : -1;
      for (let k = 0; k <= n; k++) world.flat(a0 + 1 + k * bay - .05, zs, a0 + 1 + k * bay + .05, zs + deep, y + .002, [.66, .61, .5], 9);
      for (let k = 0; k < n; k++) bays.push([a0 + 1 + (k + .5) * bay, zs + deep / 2, face]);
    }
    world.flat(a0 + 1, c0 + d / 2 - .06, a0 + w - 1, c0 + d / 2 + .06, y + .002, [.66, .61, .5], 9);   // the aisle's centre line
    const want = Math.min(p.cars || 0, bays.length), used = bays.map((q, k) => [hash(b.id, "park", k), q]).sort((p1, p2) => p1[0] - p2[0]).slice(0, want);
    for (const [, [x, z, face]] of used) {
      const yaw = face > 0 ? Math.PI / 2 : -Math.PI / 2, dims = typeof Cars !== "undefined" && Cars.parked ? Cars.parked(world, x, z, yaw, "park" + x.toFixed(1) + z.toFixed(1), y) : null;
      const [L, W] = dims || [3.6, 1.7]; solid(x - W / 2, 0, z - L / 2, x + W / 2, 1.5, z + L / 2); BLOBS.push([x, z, W / 2 + .2, L / 2 + .2]);
    }
    void aisle;
  }

  return { build, inside, carpark, LINES };
})();
