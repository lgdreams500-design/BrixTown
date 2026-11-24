"use strict";
// Buy menus that show what is for sale as objects (UI-020, UI-021; ADR-0044; the owner, 2026-11-24: "improve all the buy menus
// (game wide) by showing full 3rd model of object in a grid layout instead of a long list of text with buy btn", and of look
// test 33, Art/StyleTests/buy_menu_33: "B, and D when narrow").
//
//   wide (layout B)    a grid of the things, each drawn as an object with its price under it, and beside the grid the
//                      chosen one large, turning, to turn by hand, with its name, where it goes, its size, what is left
//                      and the price in words, and the one button that buys it
//   narrow (layout D)  small cells four across, the chosen one turning, described in a strip under the grid with its
//                      button; a panel narrower than a width in the rules shows this one (the building designer's side panel)
//
// BuyMenu.grid(opts) returns the element; life.js gives it the things and what buying one does, and calls BuyMenu.clear()
// before it redraws a panel and when it closes one. How it looks is data: World/rules/buy_menu.json (D.life.buy_menu.rules).
//
// What a thing shows is worked out by BuyMenu.shows(id), line for line world.buy_menu_shows of Tools/worldgen/world.py:
// its own object where one is made (a mall good for the home from goods_models.js, FURN-065; a piece of furniture as
// furniture.js draws it, boxes as everywhere in the slice), else a stand-in of the rules (a parcel, a paper bag, a cup,
// folded cloth in the clothing's own colour...). BuyMenu.selfTest() compares every thing with the Python's answer.
//
// The objects are drawn by one small WebGL canvas of this file's own, never shown: each cell is a 2D canvas the picture
// is copied into, so cells scroll, clip and stack like any other part of the page. Light as the kit daylight (ADR-0007),
// matte and mottled (ART_DIRECTION W1). Loaded before life.js; hash, Furniture, Outlet and Goods are play.js's and its
// neighbours' and are only touched when a menu is drawn.
var BuyMenu = (() => {
  const L = () => window.WASTED.life, R = () => L().buy_menu.rules;
  function h(tag, attrs, ...kids) {
    const e = document.createElement(tag);
    for (const k in attrs || {}) { if (k === "on") for (const ev in attrs.on) e.addEventListener(ev, attrs.on[ev]); else if (k === "cls") e.className = attrs[k]; else if (k === "html") e.innerHTML = attrs[k]; else if (k in e && k !== "list") e[k] = attrs[k]; else e.setAttribute(k, attrs[k]); }
    for (const c of kids.flat(3)) if (c !== null && c !== undefined && c !== false) e.appendChild(typeof c === "string" || typeof c === "number" ? document.createTextNode(String(c)) : c);
    return e;
  }

  // ---------------------------------------------------------------- what a thing shows (world.buy_menu_shows)
  function shows(thing) {
    const B = R(), goods = L().goods.goods, items = L().economy.items, pieces = L().builder.furnish.rules.pieces, S = B.stand_in;
    if (thing in pieces && !(thing in goods) && !(thing in items)) return ["piece", thing];
    const G = goods[thing];
    if (G && G.goes === "home") return ["piece", G.piece];
    if (thing in S.things) return ["shape", S.things[thing], null];
    if (G && G.goes === "wear") {
      for (const [part, shape, colour] of S.wear) if ((G.parts || {})[part]) return ["shape", shape, colour ? (G.parts[colour] === undefined ? null : G.parts[colour]) : null];
      return null;
    }
    if (G || thing in items) return ["shape", S.goes[G ? G.goes : "carry"] || null, null];
    return null;
  }
  function selfTest() {                                     // every thing a menu can show, against the Python's answer
    const want = L().buy_menu.checks, bad = [];
    for (const t in want) { const got = JSON.stringify(shows(t)); if (got !== JSON.stringify(want[t])) bad.push(t + ": " + got + " should be " + JSON.stringify(want[t])); }
    for (const t in want) if (want[t][0] === "shape" && !R().shapes[want[t][1]]) bad.push(t + ": no shape " + want[t][1]);
    return bad;
  }

  // ---------------------------------------------------------------- meshes: points, normals, colours (x across, y up, z towards the viewer)
  const mul = (c, k) => [c[0] * k, c[1] * k, c[2] * k];
  function finish(a, n) {                                   // bounds, the ball it fits in, its footprint
    const lo = [1e9, 1e9, 1e9], hi = [-1e9, -1e9, -1e9];
    for (let i = 0; i < n; i += 9) for (let k = 0; k < 3; k++) { lo[k] = Math.min(lo[k], a[i + k]); hi[k] = Math.max(hi[k], a[i + k]); }
    const cen = [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2]; let r = 0, foot = 0;
    for (let i = 0; i < n; i += 9) { r = Math.max(r, Math.hypot(a[i] - cen[0], a[i + 1] - cen[1], a[i + 2] - cen[2])); foot = Math.max(foot, Math.hypot(a[i] - cen[0], a[i + 2] - cen[2])); }
    return n ? { a: a.subarray(0, n), count: n / 9, cen, r: Math.max(r, .01), foot, lo, hi } : null;
  }
  function modelMesh(piece) {                               // a mall good as made in Blender, as Goods.put draws it (goods.js)
    const GM = window.GOODS_MODELS, M = GM.models[piece], Lo = M.lods[0], pal = GM.palette.map(p => p[1]);
    const A0 = GM.palette.findIndex(p => p[0] === "accent0"), A1 = GM.palette.findIndex(p => p[0] === "accent1");
    const ACC = [[.5, .25, .19], [.2, .37, .38], [.6, .46, .2], [.36, .26, .38], [.28, .38, .52], [.33, .37, .2]], k0 = Math.floor(hash(piece, "a0") * 6) % 6;
    const a0 = ACC[k0], a1 = ACC[(k0 + 1 + Math.floor(hash(piece, "a1") * 5) % 5) % 6], a = new Float32Array(Lo.i.length * 9); let o = 0;
    for (const q of Lo.i) { const i = q * 3, col = Lo.c[q] === A0 ? a0 : Lo.c[q] === A1 ? a1 : pal[Lo.c[q]];
      a[o++] = Lo.p[i] * .001; a[o++] = Lo.p[i + 1] * .001; a[o++] = -Lo.p[i + 2] * .001; a[o++] = Lo.n[i] * .01; a[o++] = Lo.n[i + 1] * .01; a[o++] = -Lo.n[i + 2] * .01; a[o++] = col[0]; a[o++] = col[1]; a[o++] = col[2]; }
    return finish(a, o);
  }
  function boxMesh(fn) {                                    // from boxes Q(u, y, v, su, sy, sv, col, f, yaw), as furniture.js draws its pieces
    let a = new Float32Array(9 * 36 * 64), o = 0;
    const Q = (u, y, v, su, sy, sv, col, f, yaw) => {
      if (!col || !(su > 0) || !(sy > 0) || !(sv > 0)) return;
      if (o + 324 > a.length) { const b = new Float32Array(a.length * 2); b.set(a); a = b; }
      const c = Math.cos(-(yaw || 0)), s = Math.sin(-(yaw || 0)), hx = su / 2, hz = sv / 2;
      const P = (du, dy, dv) => [u + c * du - s * dv, y + dy, -(v + s * du + c * dv)], N = (nu, nv) => [c * nu - s * nv, 0, -(s * nu + c * nv)];
      const face = (p0, p1, p2, p3, n) => { for (const p of [p0, p1, p2, p0, p2, p3]) { a[o++] = p[0]; a[o++] = p[1]; a[o++] = p[2]; a[o++] = n[0]; a[o++] = n[1]; a[o++] = n[2]; a[o++] = col[0]; a[o++] = col[1]; a[o++] = col[2]; } };
      const k = [[-hx, -hz], [hx, -hz], [hx, hz], [-hx, hz]], lo = k.map(([x, z]) => P(x, 0, z)), hi = k.map(([x, z]) => P(x, sy, z));
      face(hi[0], hi[1], hi[2], hi[3], [0, 1, 0]); face(lo[0], lo[1], lo[2], lo[3], [0, -1, 0]);
      face(lo[0], lo[1], hi[1], hi[0], N(0, -1)); face(lo[1], lo[2], hi[2], hi[1], N(1, 0)); face(lo[2], lo[3], hi[3], hi[2], N(0, 1)); face(lo[3], lo[0], hi[0], hi[3], N(-1, 0));
    };
    fn(Q); return finish(a, o);
  }
  function shapeMesh(name, band, own) {                     // a stand-in of the rules: a few boxes
    const B = R(), cols = B.colours;
    return boxMesh(Q => { for (const [u, y, v, su, sy, sv, col, shade, turn] of B.shapes[name])
      Q(u, y, v, su, sy, sv, mul(col === "band" ? band : col === "own" ? own : cols[col], shade === undefined ? 1 : shade), 19, (turn || 0) * Math.PI / 180); });
  }
  const meshes = new Map();
  function meshOf(e) {                                      // e: an entry of a grid (thing, and for a piece its line, for a stand-in its seller's band)
    const s = shows(e.thing || e.id); if (!s || !s[1]) return null;
    const key = s[0] === "piece" ? "p|" + s[1] + "|" + (e.line || "plain") : "s|" + s[1] + "|" + (e.band || "") + "|" + (s[2] || "");
    if (meshes.has(key)) return meshes.get(key);
    let m = null;
    try {
      if (s[0] === "piece" && window.GOODS_MODELS && window.GOODS_MODELS.models[s[1]]) m = modelMesh(s[1]);
      else if (s[0] === "piece") m = boxMesh(Q => Furniture.draw(s[1], Q, Outlet.LINES[e.line || "plain"] || Outlet.LINES.plain, hash("buy menu", s[1])));
      else m = shapeMesh(s[1], R().colours[e.band] || R().colours.ochre, s[2] ? L().designer.palette[s[2]] : R().colours.cream);
    } catch (err) { console.warn("Buy menu: " + (e.thing || e.id) + " could not be drawn.", err); }
    if (m) m.stand = s[0] === "shape";
    meshes.set(key, m); return m;
  }

  // ---------------------------------------------------------------- one small picture-maker, shared by every cell
  let glc = null, gl = null, prog = null, U = null, LOC = null, dead = false;
  const VS = `attribute vec3 aP, aN, aC; uniform mat4 uVP; uniform mat3 uR; uniform vec3 uCen; varying vec3 vN, vC, vO;
    void main() { vec3 p = uR * (aP - uCen); vN = uR * aN; vC = aC; vO = aP; gl_Position = uVP * vec4(p, 1.); }`;
  // matte and mottled: two close tones blotched together; a warm low sun against a cool sky, a grey bounce from below
  const FS = `precision mediump float; varying vec3 vN, vC, vO; uniform float uGrain, uLo, uH;
    float hs(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
    float ns(vec3 p) { vec3 i = floor(p), f = fract(p); f = f * f * (3. - 2. * f);
      return mix(mix(mix(hs(i), hs(i + vec3(1, 0, 0)), f.x), mix(hs(i + vec3(0, 1, 0)), hs(i + vec3(1, 1, 0)), f.x), f.y),
                 mix(mix(hs(i + vec3(0, 0, 1)), hs(i + vec3(1, 0, 1)), f.x), mix(hs(i + vec3(0, 1, 1)), hs(i + vec3(1, 1, 1)), f.x), f.y), f.z); }
    void main() { vec3 n = normalize(vN); float m = ns(vO * uGrain) * .62 + ns(vO * uGrain * 2.9 + 7.) * .38;
      vec3 col = vC * mix(.84, 1.13, m);
      float d = max(dot(n, normalize(vec3(-.5, .62, .6))), 0.), up = n.y * .5 + .5;
      vec3 light = vec3(1., .86, .68) * d * 1.5 + vec3(.42, .55, .82) * mix(.2, .62, up) + vec3(.5, .44, .38) * (1. - up) * .4 + .1;
      light *= mix(.7, 1., smoothstep(0., .22, (vO.y - uLo) / uH));
      vec3 c = 1. - exp(-col * light * 1.25);
      gl_FragColor = vec4(pow(c, vec3(1. / 2.2)), 1.); }`;
  function setup() {                                        // the hidden canvas and its program; false when the machine gives no WebGL, or has taken it away for now
    if (dead) return false;
    try {
      if (!glc) { glc = document.createElement("canvas"); glc.width = glc.height = R().picture.most_px;
        glc.addEventListener("webglcontextlost", e => { e.preventDefault(); prog = null; for (const m of meshes.values()) if (m) m.buf = null; });   // the browser took it: wait, the pictures already made stay
        glc.addEventListener("webglcontextrestored", () => { for (const v of views) v.dirty = true; });
        gl = glc.getContext("webgl", { alpha: true, antialias: true, premultipliedAlpha: true, preserveDrawingBuffer: true });
        if (!gl) { dead = true; return false; } }
      if (gl.isContextLost()) return false;
      if (prog) return true;
      const sh = (t, s) => { const x = gl.createShader(t); gl.shaderSource(x, s); gl.compileShader(x); if (!gl.getShaderParameter(x, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(x)); return x; };
      prog = gl.createProgram(); gl.attachShader(prog, sh(gl.VERTEX_SHADER, VS)); gl.attachShader(prog, sh(gl.FRAGMENT_SHADER, FS)); gl.linkProgram(prog); gl.useProgram(prog);
      U = {}; for (const n of ["uVP", "uR", "uCen", "uGrain", "uLo", "uH"]) U[n] = gl.getUniformLocation(prog, n);
      LOC = ["aP", "aN", "aC"].map(n => gl.getAttribLocation(prog, n));
      gl.enable(gl.DEPTH_TEST); gl.enable(gl.SCISSOR_TEST); gl.clearColor(0, 0, 0, 0);
      for (const m of meshes.values()) if (m) m.buf = null;
      return true;
    } catch (err) { console.warn("Buy menu: no pictures of the goods (WebGL).", err); dead = true; prog = null; return false; }
  }
  function shot(m, W, H, yaw, zoom) {                       // draw mesh m into the corner of the hidden canvas; returns where its foot stands in the picture
    const P = R().picture, FOV = P.fov_deg * Math.PI / 180, pitch = P.pitch_deg * Math.PI / 180;
    if (!m.buf) { m.buf = gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER, m.buf); gl.bufferData(gl.ARRAY_BUFFER, m.a, gl.STATIC_DRAW); }
    gl.bindBuffer(gl.ARRAY_BUFFER, m.buf);
    LOC.forEach((l, k) => { gl.enableVertexAttribArray(l); gl.vertexAttribPointer(l, 3, gl.FLOAT, false, 36, k * 12); });
    const asp = W / H, half = Math.min(FOV / 2, Math.atan(Math.tan(FOV / 2) * asp)), dist = m.r / Math.sin(half) * 1.06 / zoom, f = 1 / Math.tan(FOV / 2), near = Math.max(.01, dist - m.r * 1.5), far = dist + m.r * 1.5;
    const cp = Math.cos(pitch), sp = Math.sin(pitch), cy = Math.cos(yaw), sy = Math.sin(yaw);
    const V = [1, 0, 0, 0, cp, -sp, 0, sp, cp], p0 = f / asp, p10 = (far + near) / (near - far), p14 = 2 * far * near / (near - far), VP = new Float32Array(16);   // the eye stands dist away, raised by pitch, looking at the middle
    for (let c = 0; c < 3; c++) { VP[c * 4] = p0 * V[c]; VP[c * 4 + 1] = f * V[3 + c]; VP[c * 4 + 2] = p10 * V[6 + c]; VP[c * 4 + 3] = -V[6 + c]; }
    VP[14] = p10 * -dist + p14; VP[15] = dist;
    gl.viewport(0, 0, W, H); gl.scissor(0, 0, W, H); gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    gl.uniformMatrix4fv(U.uVP, false, VP); gl.uniformMatrix3fv(U.uR, false, new Float32Array([cy, 0, -sy, 0, 1, 0, sy, 0, cy])); gl.uniform3fv(U.uCen, m.cen);
    gl.uniform1f(U.uGrain, 2.6 / Math.max(.06, m.r)); gl.uniform1f(U.uLo, m.lo[1]); gl.uniform1f(U.uH, Math.max(.02, m.hi[1] - m.lo[1]));
    gl.drawArrays(gl.TRIANGLES, 0, m.count);
    const gy = m.lo[1] - m.cen[1], ey = cp * gy, ez = sp * gy - dist, px = H / 2 * f / -ez;
    return { y: H / 2 - ey * px, rx: m.foot * px, ry: m.foot * px * Math.max(.16, sp) };
  }
  // every cell that shows an object: a 2D canvas the picture is copied into
  let views = [], running = false, last = 0;
  const yaws = new Map();
  function view(key, e, turns) {
    const P = R().picture, m = meshOf(e), cv = h("canvas", { cls: "bm-obj" });
    const v = { cv, ctx: cv.getContext("2d"), key, mesh: m, yaw: yaws.has(key) ? yaws.get(key) : P.start_deg * Math.PI / 180, spin: turns ? 1 / P.turn_s : 0, zoom: m && m.stand ? P.stand_in_fill : 1, dirty: true };
    views.push(v); return v;
  }
  function paint(v) {
    const most = R().picture.most_px, dpr = Math.min(2, window.devicePixelRatio || 1), box = v.cv.getBoundingClientRect();   // (the size on the screen: the text size setting zooms the panel)
    const W = Math.min(most, Math.round(box.width * dpr)), H = Math.min(most, Math.round(box.height * dpr));
    if (W < 4 || H < 4 || !v.mesh || !setup()) return;
    if (v.cv.width !== W || v.cv.height !== H) { v.cv.width = W; v.cv.height = H; }
    const g = shot(v.mesh, W, H, v.yaw, v.zoom), c = v.ctx;
    c.clearRect(0, 0, W, H);
    const sh = c.createRadialGradient(0, 0, 0, 0, 0, 1); sh.addColorStop(0, "rgba(0,0,0,.5)"); sh.addColorStop(.7, "rgba(0,0,0,.22)"); sh.addColorStop(1, "rgba(0,0,0,0)");
    c.save(); c.translate(W / 2, g.y); c.scale(g.rx * 1.25, g.ry * 1.25); c.fillStyle = sh; c.beginPath(); c.arc(0, 0, 1, 0, 7); c.fill(); c.restore();   // the shade it stands in
    c.drawImage(glc, 0, glc.height - H, W, H, 0, 0, W, H);
    v.dirty = false;
  }
  function frame(now) {
    if (!views.length) { running = false; return; }
    const dt = Math.min(.05, (now - last) / 1000); last = now;
    for (const v of views) { if (v.spin) { v.yaw += v.spin * dt * Math.PI * 2; yaws.set(v.key, v.yaw); v.dirty = true; } if (v.dirty && v.cv.isConnected) paint(v); }
    requestAnimationFrame(frame);
  }
  function settle() {                                       // once a new grid is on the page: paint it before the page is shown, keep the list where it was
    for (const g of live) { const box = g.root.querySelector(".bm-list"); if (box && scrolls.has(g.id)) box.scrollTop = scrolls.get(g.id);
      if (g.focus) { const c = g.root.querySelector(".bm-cell.sel"); if (c) c.focus({ preventScroll: true }); } }
    for (const v of views) if (v.cv.isConnected) paint(v);
    if (!running && views.length) { running = true; last = performance.now(); requestAnimationFrame(frame); }
  }
  addEventListener("resize", () => { for (const v of views) v.dirty = true; const g = live[0];
    if (g && g.root.isConnected && g.opts.redraw && g.opts.narrow === undefined && g.opts.measure && (g.opts.measure() < R().narrow_px) !== g.narrow) g.opts.redraw(); });

  // ---------------------------------------------------------------- the grid
  const ICON = {
    home: '<svg viewBox="0 0 16 16" aria-hidden="true"><path fill="currentColor" d="M8 1.5 1.5 7.5H3.5V14H7V10H9V14H12.5V7.5H14.5Z"/></svg>',
    wear: '<svg viewBox="0 0 16 16" aria-hidden="true"><path fill="currentColor" d="M5.5 2 1.5 4.5 3 7.5 4.5 6.8V14H11.5V6.8L13 7.5 14.5 4.5 10.5 2Q8 4 5.5 2Z"/></svg>',
    carry: '<svg viewBox="0 0 16 16" aria-hidden="true"><path fill="currentColor" d="M2.5 6H13.5L14.5 14.5H1.5Z"/><path fill="none" stroke="currentColor" stroke-width="1.6" d="M5.5 7V4.5Q8 1.5 10.5 4.5V7"/></svg>',
    eat: '<svg viewBox="0 0 16 16" aria-hidden="true"><path fill="currentColor" d="M2.5 4H11.5L10.5 14.5H3.5Z"/><path fill="none" stroke="currentColor" stroke-width="1.6" d="M11.3 6H14V9.5H11"/></svg>',
    place: '<svg viewBox="0 0 16 16" aria-hidden="true"><path fill="currentColor" d="M2 3H5V8H11V3H14V11H12.5V14H10.5V11H5.5V14H3.5V11H2Z"/></svg>',
  };
  const board = goes => { const G = R().goes[goes], c = G && L().settings.menu.colours[G.colour]; return c ? "background:" + c[0] + ";color:" + c[1] : ""; };
  const chip = goes => R().goes[goes] ? h("span", { cls: "bm-chip", style: board(goes), html: (ICON[goes] || "") + R().goes[goes].name }) : null;
  const dot = goes => R().goes[goes] ? h("span", { cls: "bm-dot", style: board(goes), title: R().goes[goes].name, html: ICON[goes] || "" }) : null;
  const chosen = new Map(), scrolls = new Map();            // by grid: the thing chosen, how far its list is scrolled
  const held = new Map(), last1 = new Map();                // by grid: the thing turned by hand, the thing last shown large
  const clicked = { id: "", t: 0 };                         // the cell clicked last, and when
  let live = [];                                            // the grids on the page now
  const narrowAt = o => o.narrow !== undefined ? !!o.narrow : (o.width || R().wide.panel_px) < R().narrow_px;
  const sizeWords = e => e.size ? e.size.map(v => Math.round(v * 100)).join(" × ") + " cm" : null;
  let hadFocus = false;                                     // a cell had the keyboard when the panel was last emptied: the chosen cell gets it back
  function clear() { const at = document.activeElement; hadFocus = !!(at && at.classList && at.classList.contains("bm-cell")); views = []; live = []; }
  // opts: { id, things: [{ id, name, price (chips, or null), goes, left (a number, or null when it never runs out), state: "ok" | "out" | "short",
  //   short (chips missing), size ([w, d, h] in metres, or null), line (a furniture line), band (a colour of the rules, for a stand-in) }],
  //   verb ("Buy"), fmt (chips to words), restock ("06:00"), width (px the panel has, read before it is emptied), measure (() => that width now, for when the window changes), narrow (to force a layout), keys (it answers BuyMenu.key),
  //   leftWords (n => words), stockLabel, priceLabel, buy (thing => ...), redraw () }
  function grid(opts) {
    const B = R(), narrow = narrowAt(opts), things = opts.things, fmt = opts.fmt, verb = opts.verb || "Buy";
    if (!things.length) return h("p", { cls: "dim" }, opts.empty || "Nothing here.");
    let pick = things.find(e => e.id === chosen.get(opts.id)); if (!pick) { pick = things.find(e => e.state !== "out") || things[0]; chosen.set(opts.id, pick.id); }
    const g = { id: opts.id, opts, narrow, things, root: null, focus: hadFocus };
    // A click chooses; a second click on the same thing soon after buys it. (Not the browser's double click: choosing redraws
    // the panel, so the second click lands on a new cell.)
    const click = e => { const now = performance.now(), twice = clicked.id === opts.id + ":" + e.id && now - clicked.t < 450; clicked.id = twice ? "" : opts.id + ":" + e.id; clicked.t = now;
      if (twice) { if (e.state === "ok") opts.buy(e); return; }
      if (chosen.get(opts.id) !== e.id) { chosen.set(opts.id, e.id); if (opts.redraw) opts.redraw(); } };
    const price = e => e.price === null ? h("span", { cls: "dim" }, "no price yet") : h("span", { cls: "money" + (e.state === "short" ? " neg" : "") }, fmt(e.price));
    const leftWords = e => e.left === null ? null : e.state === "out" ? h("span", { cls: "neg" }, "sold out" + (opts.restock ? ", back at " + opts.restock : ""))
      : h("span", { cls: e.left === 1 && !opts.leftWords ? "bm-last" : "dim" }, opts.leftWords ? opts.leftWords(e.left) : e.left + " left");
    const short = e => e.state === "short" ? h("span", { cls: "bm-short" }, fmt(e.short) + " short") : null;
    const button = (e, label) => h("button", { type: "button", cls: "pbtn bm-buy", disabled: e.state !== "ok", on: { click: ev => { ev.stopPropagation(); opts.buy(e); } } }, label || verb);
    const stage = (e, key, turns, extra) => { const m = meshOf(e), v = m && !dead ? view(key, e, turns) : null;
      return [h("div", { cls: "bm-stage" }, v ? v.cv : h("div", { cls: "bm-glyph", html: ICON[e.goes] || ICON.carry }), e.state === "out" ? h("span", { cls: "bm-flag" }, "Sold out") : null, extra || null), v]; };
    const cell = (e, kids, turns, extra) => h("button", { type: "button", cls: "bm-cell" + (e.id === pick.id ? " sel" : "") + (e.state === "out" ? " out" : ""), "data-id": e.id, title: e.name, "aria-pressed": e.id === pick.id ? "true" : "false",
      on: { click: () => click(e) } }, stage(e, opts.id + ":" + e.id, turns, extra)[0], kids);
    const list = h("div", { cls: "bm-list", on: { scroll: ev => scrolls.set(opts.id, ev.currentTarget.scrollTop) } });
    if (narrow) {
      list.style.setProperty("--bm-cols", B.narrow.columns); list.style.maxHeight = B.narrow.list_px + "px";
      for (const e of things) list.append(cell(e, h("div", { cls: "bm-pr" }, price(e)), e.id === pick.id && e.state !== "out",
        [e.left !== null && e.left > 0 ? h("span", { cls: "bm-n" + (e.left === 1 && !opts.leftWords ? " bm-last" : "") }, "×" + e.left) : null, dot(e.goes)]));
      g.root = h("div", { cls: "bm bm-narrow" }, list, h("div", { cls: "bm-strip" }, h("span", { cls: "bm-name" }, pick.name), chip(pick.goes), leftWords(pick), sizeWords(pick) ? h("span", { cls: "dim bm-size" }, sizeWords(pick)) : null,
        h("span", { cls: "bm-end" }, short(pick), price(pick), button(pick))));
    } else {
      const W = B.wide; list.style.setProperty("--bm-cell", W.cell_px + "px"); list.style.setProperty("--bm-stage", W.stage_px + "px"); list.style.maxHeight = W.list_px + "px";
      for (const e of things) list.append(cell(e, [h("div", { cls: "bm-row" }, price(e), dot(e.goes)), h("div", { cls: "bm-sub" }, e.name)], false));
      const [st, big] = stage(pick, opts.id + ":show", true, h("span", { cls: "bm-turn" }, "drag to turn")); st.classList.add("bm-big"); st.style.height = W.show_stage_px + "px";
      if (big) { let drag = null;                           // turned by hand: it stays where it is left until another thing is chosen
        if (held.get(opts.id) === pick.id) big.spin = 0; else held.delete(opts.id);
        st.addEventListener("pointerdown", ev => { drag = [ev.clientX, big.yaw]; big.spin = 0; held.set(opts.id, pick.id); st.setPointerCapture(ev.pointerId); });
        st.addEventListener("pointermove", ev => { if (drag) { big.yaw = drag[1] + (ev.clientX - drag[0]) * B.picture.drag_deg_px * Math.PI / 180; yaws.set(big.key, big.yaw); big.dirty = true; } });
        st.addEventListener("pointerup", () => { drag = null; }); st.addEventListener("pointercancel", () => { drag = null; });
        if (last1.get(opts.id) !== pick.id) { last1.set(opts.id, pick.id); big.yaw = B.picture.start_deg * Math.PI / 180; yaws.set(big.key, big.yaw); } }
      const rows = [pick.goes && B.goes[pick.goes] ? ["Goes to", chip(pick.goes)] : null, sizeWords(pick) ? ["Size", sizeWords(pick)] : null, pick.left === null ? null : opts.stockLabel ? [opts.stockLabel, String(pick.left)] : ["In stock", leftWords(pick)], [opts.priceLabel || "Price", h("span", {}, short(pick), " ", price(pick))]];
      g.root = h("div", { cls: "bm bm-wide", style: "--bm-show:" + W.show_px + "px" }, list,
        h("div", { cls: "bm-show" }, st, h("div", { cls: "bm-name" }, pick.name), h("table", {}, h("tbody", {}, rows.filter(Boolean).map(([a, b]) => h("tr", {}, h("td", {}, a), h("td", {}, b))))),
          button(pick, pick.price === null ? verb : verb + " · " + fmt(pick.price))));
    }
    g.pick = pick; live.push(g); queueMicrotask(settle);
    return g.root;
  }
  // Arrows choose the nearest thing that way, Enter buys it (life.js hands the keys over while a counter is open). True when the key was used.
  function key(ev) {
    const g = live.find(x => x.opts.keys && x.root.isConnected); if (!g) return false;
    const move = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] }[ev.code], at = document.activeElement;
    if (ev.code === "Enter" || ev.code === "NumpadEnter") {
      if (at && at !== document.body && !(at.classList && at.classList.contains("bm-cell"))) return false;   // a button or a field of the panel has the key
      if (g.pick.state === "ok") g.opts.buy(g.pick); return true;
    }
    if (!move || (at && /^(INPUT|SELECT|TEXTAREA)$/.test(at.tagName))) return false;
    const cells = [...g.root.querySelectorAll(".bm-cell")], cur = cells.find(c => c.dataset.id === g.pick.id); if (!cur) return true;
    const mid = c => { const r = c.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2]; }, [cx, cy] = mid(cur), [dx, dy] = move;
    let best = null, bd = 1e18;
    for (const c of cells) { if (c === cur) continue; const [x, y] = mid(c), along = (x - cx) * dx + (y - cy) * dy, across = Math.abs((x - cx) * dy) + Math.abs((y - cy) * dx); if (along < 4) continue; const d = along + across * 3; if (d < bd) { bd = d; best = c; } }
    if (best) { chosen.set(g.id, best.dataset.id); const id = g.id; if (g.opts.redraw) g.opts.redraw();
      queueMicrotask(() => { const n = live.find(x => x.id === id), c = n && n.root.querySelector(".bm-cell.sel"); if (c) { c.scrollIntoView({ block: "nearest" }); scrolls.set(id, n.root.querySelector(".bm-list").scrollTop); } }); }
    return true;
  }
  return { shows, selfTest, grid, clear, key, narrowAt, get views() { return views.length; }, get lost() { return dead; } };
})();
