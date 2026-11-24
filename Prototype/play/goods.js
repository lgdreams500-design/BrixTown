"use strict";
// The mall's goods for the home (ECO-014, FURN-064; ADR-0040; the owner, 2026-11-24: "some of these object can be placed in the
// house like guitar, books etc."). A good for the home is a piece of World/rules/furnishing.json (`from: "mall"`): bought at an
// outlet it goes to the belongings, and is placed in a home from the building designer's Furnish tab like any other piece.
//
// Goods.draw(kind, Q, seed) draws one through Q(u, y, v, su, sy, sv, col, f, yaw), as furniture.js draws its pieces: u across,
// y up from the floor (a box's bottom), v from its front (-) to its back (+). Each is a few plain boxes in the town's muted
// colours, fitted inside the piece's size from the rules; they are the stand-ins until the Blender objects are exported
// (Tools/blender, the next stage), as the box cars stood in for the car models. Loaded after furniture.js.
var Goods = (() => {
  const WOOD = [.5, .36, .22], WOOD_D = [.3, .2, .12], DARK = [.12, .11, .11], IRON = [.1, .1, .11], CREAM = [.78, .72, .6], PAPER = [.82, .78, .68];
  const TERRA = [.5, .25, .19], OCHRE = [.6, .46, .2], TEAL = [.2, .37, .38], BLUE = [.28, .38, .52], OLIVE = [.33, .37, .2], PLUM = [.36, .26, .38];
  const LEAF = [.27, .37, .18], LEAF_L = [.33, .44, .2], BRASS = [.62, .5, .24], RED = [.5, .14, .12], PLUSH = [.5, .36, .22], GLASS = [.2, .24, .27], STEEL = [.42, .43, .45];
  const BOOKS = [TERRA, TEAL, OCHRE, PLUM, BLUE, OLIVE, CREAM, WOOD_D];
  const rules = () => window.WASTED.life.builder.furnish.rules.pieces;
  const rnd = (seed, ...k) => (typeof hash === "function" ? hash(seed, ...k) : .5);
  const round = (Q, u, y, v, d, h, col) => { Q(u, y, v, d * .86, h, d * .86, col); Q(u, y, v, d * .86, h, d * .86, col, 19, Math.PI / 4); };   // a round thing standing up: two boxes at 45 degrees
  const ball = (Q, u, y, v, d, col) => { round(Q, u, y + d * .15, v, d, d * .7, col); Q(u, y, v, d * .6, d, d * .6, col); };
  const stand = (Q, w, d, h) => { Q(0, 0, 0, w * .8, .04, d * .8, IRON); Q(0, .04, d * .22, .04, h, .04, IRON); Q(0, h * .45, d * .1, w * .5, .03, .12, IRON); };
  function guitar(Q, w, d, h, col, solid) {                // on its stand, leaning back a little
    stand(Q, w, d, h * .62);
    const bw = solid ? w * .72 : w * .84, t = solid ? .06 : .1;
    Q(0, .1, -.03, bw, h * .3, t, col); Q(0, .1 + h * .28, -.02, bw * .74, h * .2, t, col);
    if (!solid) Q(0, .1 + h * .24, -.03 - t / 2, .1, .1, .012, DARK); else Q(0, .1 + h * .16, -.03 - t / 2, bw * .4, .05, .012, CREAM);
    Q(0, .1 + h * .46, -.01, .055, h * .36, .04, WOOD_D); Q(0, .1 + h * .8, -.01, .09, h * .1, .04, solid ? col : DARK);
  }
  function pot(Q, u, v, d, h) { round(Q, u, 0, v, d, h, TERRA); Q(u, h - .02, v, d * .7, .03, d * .7, [.2, .15, .1]); }
  function leaves(Q, u, y, v, s, seed, n) { for (let k = 0; k < (n || 5); k++) Q(u + (rnd(seed, k) - .5) * .3 * s, y + k * .13 * s, v + (rnd(seed, k, 1) - .5) * .3 * s, (.42 - k * .05) * s, .18 * s, (.42 - k * .05) * s, k % 2 ? LEAF_L : LEAF, 19, k * .7); }

  const D = {
    // ---- toys
    g_bear: (Q) => { ball(Q, 0, 0, .02, .3, PLUSH); ball(Q, 0, .27, .02, .23, PLUSH); for (const s of [-1, 1]) { Q(s * .1, .48, .02, .08, .08, .05, PLUSH); Q(s * .16, .12, -.04, .08, .16, .08, PLUSH); Q(s * .1, 0, -.12, .1, .08, .14, PLUSH); } Q(0, .33, -.1, .09, .07, .05, CREAM); },
    g_blocks: (Q, w, d, h, seed) => { const c = [TERRA, TEAL, OCHRE, BLUE, OLIVE, PLUM]; let n = 0; for (let row = 0; row < 3; row++) for (let i = 0; i < 3 - row; i++) Q((i - (2 - row) / 2) * .16, row * .14, (row % 2) * .03, .14, .14, .14, c[(n++ + Math.floor(rnd(seed) * 6)) % 6]); Q(.16, 0, -.1, .14, .14, .14, c[4], 19, .5); },
    g_rocket: (Q) => { round(Q, 0, .12, 0, .2, .52, CREAM); round(Q, 0, .64, 0, .15, .14, RED); Q(0, .78, 0, .07, .12, .07, RED); for (const s of [-1, 1]) { Q(s * .14, 0, 0, .06, .3, .16, RED); Q(0, 0, s * .14, .16, .3, .06, RED); } Q(0, .42, -.09, .08, .08, .02, BLUE); },
    g_rockinghorse: (Q) => { for (const s of [-1, 1]) Q(0, 0, s * .13, .95, .06, .05, WOOD_D); Q(0, .34, 0, .56, .2, .2, CREAM); Q(.3, .46, 0, .14, .3, .14, CREAM); Q(.38, .68, 0, .26, .13, .13, CREAM); Q(-.3, .4, 0, .06, .2, .06, WOOD_D);
      for (const u of [-.2, .2]) for (const s of [-1, 1]) Q(u, .06, s * .09, .06, .3, .05, CREAM); Q(0, .54, 0, .2, .04, .22, TERRA); },
    g_train: (Q) => { Q(.24, .04, 0, .28, .1, .14, TERRA); Q(.3, .14, 0, .12, .07, .12, DARK); Q(.16, .14, 0, .12, .08, .14, TERRA); Q(-.06, .04, 0, .24, .1, .14, TEAL); Q(-.32, .04, 0, .22, .1, .14, OCHRE);
      for (const u of [.32, .16, -.0, -.12, -.26, -.38]) for (const s of [-1, 1]) Q(u, 0, s * .08, .07, .07, .03, DARK); },
    g_dollhouse: (Q) => { Q(0, 0, 0, .8, .04, .45, WOOD_D); Q(0, .04, .1, .74, .56, .22, CREAM); Q(0, .6, .1, .8, .05, .3, WOOD); Q(0, .65, .1, .6, .14, .26, TERRA); Q(0, .79, .1, .34, .12, .24, TERRA);
      for (const u of [-.22, .22]) for (const y of [.12, .38]) Q(u, y, -.015, .14, .14, .02, GLASS); Q(0, .04, -.015, .12, .22, .02, TEAL); },
    // ---- electronics
    g_tv: (Q) => { Q(0, 0, 0, 1.0, .08, .36, WOOD_D); Q(0, .08, 0, 1.1, .34, .4, WOOD); for (const s of [-1, 1]) Q(s * .27, .12, -.205, .48, .26, .02, WOOD_D); Q(0, .42, .04, .3, .05, .2, IRON); Q(0, .47, .04, .96, .52, .07, DARK); Q(0, .5, .003, .9, .46, .01, GLASS); },
    g_radio: (Q) => { Q(0, 0, 0, .46, .26, .22, WOOD); Q(-.08, .05, -.115, .22, .16, .01, CREAM); Q(.14, .14, -.115, .1, .07, .01, DARK); for (const u of [.1, .18]) Q(u, .05, -.12, .04, .04, .02, BRASS); Q(.18, .26, .05, .012, .04, .012, IRON); },
    g_recordplayer: (Q) => { Q(0, 0, 0, .48, .14, .4, WOOD); round(Q, -.05, .14, 0, .3, .02, DARK); round(Q, -.05, .16, 0, .09, .008, TERRA); Q(.17, .15, .02, .03, .03, .24, STEEL); Q(.17, .14, -.14, .06, .04, .06, CREAM); },
    g_desklamp: (Q) => { round(Q, 0, 0, .02, .18, .03, IRON); Q(0, .03, .04, .03, .34, .03, IRON); Q(0, .34, -.02, .03, .03, .16, IRON); round(Q, 0, .3, -.07, .16, .14, TEAL); },
    g_console: (Q) => { Q(0, 0, .02, .3, .07, .22, DARK); Q(0, .07, .02, .26, .012, .18, STEEL); for (const s of [-1, 1]) { Q(s * .12, 0, -.11, .12, .05, .07, CREAM); Q(s * .12, .05, -.11, .03, .02, .03, RED); } },
    g_computer: (Q) => { Q(0, 0, .08, .44, .1, .34, CREAM); Q(0, .1, .08, .4, .36, .34, CREAM); Q(0, .15, -.093, .32, .26, .01, GLASS); Q(0, 0, -.18, .46, .035, .15, PAPER); Q(.26, 0, -.18, .07, .03, .1, PAPER); },
    g_speakers: (Q) => { for (const s of [-1, 1]) { Q(s * .2, 0, 0, .28, .9, .3, WOOD_D); Q(s * .2, .12, -.155, .18, .18, .01, DARK); Q(s * .2, .4, -.155, .22, .22, .01, DARK); Q(s * .2, .7, -.155, .1, .1, .01, DARK); } },
    g_alarmclock: (Q) => { round(Q, 0, .02, 0, .12, .09, TEAL); Q(0, .03, -.05, .08, .08, .01, CREAM); for (const s of [-1, 1]) { Q(s * .04, 0, 0, .02, .03, .02, BRASS); Q(s * .04, .12, 0, .04, .03, .04, BRASS); } },
    // ---- music
    g_guitar: (Q, w, d, h) => guitar(Q, w, d, h, [.62, .42, .18], false),
    g_guitar_electric: (Q, w, d, h) => guitar(Q, w, d, h, TEAL, true),
    g_guitar_bass: (Q, w, d, h) => guitar(Q, w, d, h, RED, true),
    g_ukulele: (Q, w, d, h) => guitar(Q, w, d, h, [.5, .3, .14], false),
    g_violin: (Q, w, d, h) => { guitar(Q, w, d, h, [.42, .2, .1], false); Q(.11, .12, -.04, .015, h * .7, .015, WOOD_D); },
    g_keyboard: (Q) => { for (const s of [-1, 1]) { Q(s * .5, 0, 0, .05, .04, .36, IRON); Q(s * .5, .04, 0, .04, .76, .04, IRON); } Q(0, .4, 0, 1.0, .03, .03, IRON); Q(0, .8, 0, 1.3, .1, .38, DARK); Q(0, .9, -.07, 1.2, .02, .16, CREAM); for (let i = 0; i < 12; i++) Q(-.55 + i * .1, .915, -.02, .04, .012, .08, DARK); },
    g_drums: (Q) => { const skin = [.8, .76, .66]; Q(0, .02, .1, .5, .56, .4, RED); Q(0, .02, -.11, .44, .5, .02, skin); round(Q, -.52, .38, -.2, .36, .16, RED); round(Q, -.52, .54, -.2, .34, .012, skin); Q(-.52, 0, -.2, .04, .38, .04, STEEL);
      round(Q, .55, .0, .0, .42, .44, RED); round(Q, .55, .44, .0, .4, .012, skin); for (const s of [-1, 1]) { round(Q, s * .2, .64, .14, .28, .2, RED); round(Q, s * .2, .84, .14, .26, .012, skin); }
      for (const [u, v, y] of [[-.74, .3, 1.18], [.74, .36, 1.26]]) { Q(u, 0, v, .035, y, .035, STEEL); round(Q, u, y, v, .36, .02, BRASS); } Q(0, 0, .46, .34, .5, .3, DARK); Q(0, .5, .46, .36, .05, .32, WOOD_D); },
    g_amp: (Q) => { Q(0, 0, 0, .6, .6, .35, DARK); Q(0, .05, -.18, .52, .4, .012, [.3, .28, .25]); Q(0, .48, -.18, .52, .07, .012, BRASS); for (const u of [-.18, -.06, .06, .18]) Q(u, .5, -.19, .03, .03, .02, DARK); Q(0, .6, 0, .2, .03, .05, DARK); },
    // ---- books
    g_bookstack: (Q, w, d, h, seed) => { let y = 0; for (let i = 0; i < 6; i++) { const t = .04 + .02 * rnd(seed, i); Q((rnd(seed, i, 1) - .5) * .03, y, (rnd(seed, i, 2) - .5) * .02, .26 - .04 * rnd(seed, i, 3), t, .2, BOOKS[Math.floor(rnd(seed, i, 4) * 8) % 8], 19, (rnd(seed, i, 5) - .5) * .3); y += t; } },
    g_bookrow: (Q, w, d, h, seed) => { for (const s of [-1, 1]) { Q(s * .235, 0, 0, .03, .2, .14, IRON); Q(s * .21, 0, 0, .08, .015, .14, IRON); } let u = -.2; for (let i = 0; i < 9 && u < .19; i++) { const t = .035 + .02 * rnd(seed, i), hh = .2 + .07 * rnd(seed, i, 1); Q(u + t / 2, 0, 0, t, hh, .16, BOOKS[Math.floor(rnd(seed, i, 2) * 8) % 8]); u += t + .004; } },
    g_atlas: (Q) => { Q(0, 0, 0, .4, .05, .36, WOOD_D); Q(0, .05, 0, .08, .8, .08, WOOD); Q(0, .85, 0, .55, .04, .42, WOOD); Q(0, .89, .0, .5, .05, .36, CREAM); Q(-.13, .94, 0, .24, .012, .34, PAPER); Q(.13, .94, 0, .24, .012, .34, PAPER); Q(0, .945, 0, .012, .012, .34, TERRA); },
    g_globe: (Q) => { round(Q, 0, 0, 0, .22, .03, WOOD_D); Q(0, .03, 0, .03, .12, .03, BRASS); ball(Q, 0, .14, 0, .32, TEAL); Q(.03, .24, -.13, .12, .1, .03, OLIVE); Q(-.08, .3, -.1, .08, .08, .06, OCHRE); Q(0, .46, 0, .02, .04, .02, BRASS); },
    // ---- sport
    g_football: (Q) => { ball(Q, 0, 0, 0, .24, CREAM); Q(0, .08, -.1, .08, .08, .03, DARK); Q(.08, .14, .02, .05, .07, .07, DARK); },
    g_weights: (Q) => { Q(0, 0, 0, .7, .05, .35, IRON); for (const [v, c] of [[-.09, TERRA], [.09, TEAL]]) { Q(0, .1, v, .5, .04, .04, STEEL); for (const s of [-1, 1]) { Q(s * .2, .05, v, .07, .14, .14, c); Q(s * .27, .07, v, .05, .1, .1, c); } } Q(0, .19, 0, .6, .03, .03, STEEL); for (const s of [-1, 1]) Q(s * .28, .19, 0, .03, .16, .16, DARK); },
    g_mat: (Q) => { Q(0, 0, 0, .6, .03, 1.8, TEAL); Q(0, .03, .82, .6, .004, .1, CREAM); Q(0, .03, -.82, .6, .004, .1, CREAM); },
    g_exercisebike: (Q) => { for (const v of [-.42, .42]) Q(0, 0, v, .5, .06, .08, IRON); Q(0, .06, 0, .08, .08, .9, IRON); round(Q, 0, .1, -.3, .42, .09, DARK); Q(0, .14, .2, .07, .66, .07, IRON); Q(0, .8, .24, .24, .07, .28, DARK);
      Q(0, .14, -.3, .07, .96, .07, IRON); Q(0, 1.1, -.3, .46, .05, .05, IRON); Q(0, 1.14, -.3, .16, .1, .04, GLASS); Q(.13, .22, -.02, .04, .04, .2, STEEL); Q(-.13, .34, .05, .04, .04, .2, STEEL); },
    g_rackets: (Q) => { Q(0, 0, .05, .6, .05, .26, WOOD_D); for (const s of [-1, 1]) Q(s * .27, .05, .08, .04, .9, .04, WOOD); Q(0, .9, .08, .6, .05, .06, WOOD);
      for (const [u, c] of [[-.13, TERRA], [.13, TEAL]]) { Q(u, .08, -.02, .035, .36, .03, DARK); Q(u, .44, -.02, .22, .3, .025, c); Q(u, .47, -.035, .16, .24, .01, CREAM); } ball(Q, 0, .05, -.06, .07, OCHRE); },
    // ---- jewellery
    g_jewelbox: (Q) => { Q(0, 0, 0, .3, .1, .2, PLUM); Q(0, .1, .0, .3, .05, .2, [.3, .22, .32]); Q(0, .07, -.105, .05, .04, .012, BRASS); for (const s of [-1, 1]) Q(s * .12, 0, 0, .03, .15, .21, BRASS); },
    g_mantelclock: (Q) => { Q(0, 0, 0, .3, .04, .14, WOOD_D); Q(0, .04, 0, .24, .2, .12, WOOD); Q(0, .24, 0, .18, .05, .1, WOOD); Q(0, .29, 0, .1, .03, .08, WOOD_D); Q(0, .08, -.062, .14, .14, .01, CREAM); Q(0, .14, -.07, .012, .06, .008, DARK); Q(.02, .14, -.07, .04, .012, .008, DARK); },
    // ---- home
    g_tablelamp: (Q) => { round(Q, 0, 0, 0, .16, .04, WOOD_D); round(Q, 0, .04, 0, .1, .22, TEAL); Q(0, .26, 0, .025, .06, .025, BRASS); round(Q, 0, .3, 0, .3, .2, [.86, .78, .6]); },
    g_vase: (Q) => { round(Q, 0, 0, 0, .12, .04, TEAL); round(Q, 0, .04, 0, .2, .18, TEAL); round(Q, 0, .22, 0, .1, .12, TEAL); },
    g_floorvase: (Q, w, d, h, seed) => { round(Q, 0, 0, 0, .24, .06, OCHRE); round(Q, 0, .06, 0, .38, .34, OCHRE); round(Q, 0, .4, 0, .22, .14, OCHRE); round(Q, 0, .54, 0, .16, .06, OCHRE); for (let i = 0; i < 4; i++) Q((rnd(seed, i) - .5) * .1, .58, (rnd(seed, i, 1) - .5) * .1, .015, .26, .015, WOOD); },
    g_pots: (Q) => { round(Q, -.12, 0, 0, .24, .16, STEEL); round(Q, -.12, .16, 0, .25, .02, DARK); Q(-.12, .18, 0, .05, .04, .05, DARK); round(Q, .14, 0, .03, .18, .12, [.5, .3, .2]); Q(.26, .08, .03, .12, .025, .03, DARK); round(Q, .14, .12, .03, .19, .02, DARK); },
    g_jars: (Q) => { const c = [CREAM, OCHRE, TERRA, OLIVE]; for (let i = 0; i < 4; i++) { const u = -.2 + i * .135, hh = .2 - (i % 2) * .04; round(Q, u, 0, 0, .12, hh, c[i]); round(Q, u, hh, 0, .1, .03, WOOD_D); } },
    g_mirror: (Q) => { for (const s of [-1, 1]) { Q(s * .27, 0, 0, .05, .04, .45, WOOD_D); Q(s * .27, .04, 0, .045, 1.5, .045, WOOD); } Q(0, .2, 0, .5, 1.5, .04, WOOD); Q(0, .26, -.022, .42, 1.38, .01, [.55, .62, .66]); },
    g_rug: (Q) => { Q(0, 0, 0, 1.6, .03, 1.1, TERRA); Q(0, .03, 0, 1.36, .004, .86, CREAM); Q(0, .034, 0, 1.1, .004, .6, TEAL); Q(0, .038, 0, .5, .004, .26, OCHRE); },
    g_cushions: (Q) => { Q(0, 0, 0, .58, .16, .58, TEAL); Q(.02, .16, -.01, .52, .14, .52, OCHRE, 19, .3); Q(-.02, .3, .02, .44, .12, .44, PLUM, 19, -.25); },
    g_candlesticks: (Q) => { for (const s of [-1, 1]) { round(Q, s * .09, 0, 0, .1, .02, BRASS); Q(s * .09, .02, 0, .025, .14, .025, BRASS); round(Q, s * .09, .16, 0, .06, .02, BRASS); Q(s * .09, .18, 0, .028, .12, .028, CREAM); } },
    // ---- flowers and gifts
    g_fern: (Q, w, d, h, seed) => { pot(Q, 0, 0, .34, .26); leaves(Q, 0, .26, 0, .95, seed, 4); },
    g_figtree: (Q, w, d, h, seed) => { pot(Q, 0, 0, .44, .36); Q(0, .36, 0, .06, .6, .06, WOOD_D); leaves(Q, 0, .86, 0, 1.25, seed, 5); },
    g_flowervase: (Q, w, d, h, seed) => { round(Q, 0, 0, 0, .14, .22, BLUE); const c = [TERRA, OCHRE, PLUM, CREAM, [.6, .3, .32]]; for (let i = 0; i < 5; i++) { const u = (rnd(seed, i) - .5) * .16, v = (rnd(seed, i, 1) - .5) * .16; Q(u * .5, .22, v * .5, .012, .14 + .08 * rnd(seed, i, 2), .012, LEAF); Q(u, .36 + .08 * rnd(seed, i, 2), v, .09, .08, .09, c[i], 19, i); } },
    g_cactus: (Q) => { pot(Q, 0, 0, .18, .1); round(Q, 0, .1, 0, .1, .18, LEAF); Q(.07, .16, 0, .06, .03, .04, LEAF); Q(.09, .16, 0, .035, .1, .04, LEAF); },
    g_trough: (Q, w, d, h, seed) => { Q(0, 0, 0, .8, .22, .26, WOOD); Q(0, .2, 0, .74, .03, .2, [.2, .15, .1]); const c = [TERRA, OCHRE, PLUM, CREAM]; for (let i = 0; i < 6; i++) { const u = -.31 + i * .125; Q(u, .22, (i % 2 ? .04 : -.04), .11, .12, .11, LEAF, 19, i); Q(u, .34, (i % 2 ? .04 : -.04), .08, .08, .08, c[i % 4], 19, i * .6); } },
    // ---- the arcade's prizes
    g_rabbit: (Q) => { ball(Q, 0, 0, .02, .22, CREAM); ball(Q, 0, .19, .02, .17, CREAM); for (const s of [-1, 1]) { Q(s * .045, .33, .03, .04, .11, .03, CREAM); Q(s * .08, 0, -.08, .07, .06, .1, CREAM); } Q(0, .23, -.06, .03, .025, .02, [.6, .3, .32]); },
    g_tabletop: (Q) => { Q(0, 0, 0, .5, .34, .5, PLUM); Q(0, .34, .1, .5, .36, .3, PLUM); Q(0, .38, -.055, .36, .26, .01, GLASS); Q(0, .34, -.14, .46, .03, .2, DARK); Q(-.1, .37, -.14, .03, .06, .03, RED); for (const u of [.06, .14]) Q(u, .37, -.14, .04, .02, .04, OCHRE); Q(0, .66, .1, .46, .04, .26, [.86, .78, .6]); }
  };
  // what each takes up, from the rules (furnishing.json), for the pieces drawn here: Furnish.selfTest checks every piece of the list has one
  const SIZE = {};
  function sizes() { const P = rules(); for (const k of Object.keys(D)) if (P[k]) SIZE[k] = P[k].size; return SIZE; }
  function draw(kind, Q, seed) { const fn = D[kind]; if (!fn) return false; const s = rules()[kind].size; fn(Q, s[0], s[1], s[2], seed); return true; }
  return { draw, sizes, SIZE, has: k => k in D };
})();
Object.assign(Furniture.SIZE, Goods.sizes());   // so a placed good is walked round like any piece, and the list's sizes are checked (Furnish.selfTest)
