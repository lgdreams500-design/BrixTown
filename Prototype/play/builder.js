"use strict";
// The building designer's rules in the slice (docs/specs/domains/BUILDER.md BUILD-004 to BUILD-007, BUILD-016 to BUILD-019):
// a port of Tools/citysim/builder.py and world.py's built(), allowed() and design_problems(), rule for rule and message for
// message. A design (lot.dsg) names its choices by id; Builder.built() turns them into what play.js builds with (lot.design).
// Builder.selfTest() runs the fixed remodels of builder.check_script() and compares every result with the Python's
// (D.life.builder.checks), so the two cannot drift apart. Loaded before life.js.
var Builder = (() => {
  const X = () => window.WASTED.life.builder;
  const FACADES = ["n", "s", "e", "w"], FACADE_NAME = { n: "north", s: "south", e: "east", w: "west" };
  const OLD_BASE = 3.55, GF = 3.2;
  const clone = x => JSON.parse(JSON.stringify(x));
  const same = (a, b) => JSON.stringify(a === undefined ? null : a) === JSON.stringify(b === undefined ? null : b);
  const isObj = v => v !== null && typeof v === "object" && !Array.isArray(v);
  const pyNum = x => Number.isInteger(x) ? x + ".0" : String(x);   // how Python prints a float (50.0, 320.72)
  const roundEven = x => { const f = Math.floor(x), d = x - f; return d > .5 ? f + 1 : d < .5 ? f : (f % 2 === 0 ? f : f + 1); };   // Python's round()
  const round2 = x => Number(x.toFixed(2));

  function paths(d, prefix, out) {                          // every leaf of a design, as a dotted path
    out = out || {};
    for (const k of Object.keys(d)) { const v = d[k], p = (prefix || "") + k; if (isObj(v)) paths(v, p + ".", out); else out[p] = v; }
    return out;
  }
  function changed(a0, b0) {
    const a = paths(a0), b = paths(b0), ks = [...new Set(Object.keys(a).concat(Object.keys(b)))];
    return ks.filter(p => !same(a[p], b[p])).sort();
  }
  function match(path, pattern) {
    const p = path.split("."), q = pattern.split(".");
    return p.length >= q.length && q.every((s, i) => s === "*" || s === p[i]);
  }
  const part = (path, allowed) => allowed.find(a => match(path, a)) || null;
  function get(d, path) { for (const k of path.split(".")) d = d[k]; return d; }
  function remodel(old, changes) {                          // a copy of a design with some parts changed by dotted path; '*' is every facade
    const nw = clone(old);
    for (const path of Object.keys(changes)) {
      const ks = path.split("."), last = ks.pop(); let nodes = [nw];
      for (const k of ks) nodes = nodes.flatMap(n => (k === "*" ? FACADES : [k]).map(x => n[x]));
      for (const n of nodes) n[last] = clone(changes[path]);
    }
    return nw;
  }
  function civic(l) {
    const C = X().planning.civic_only, b = l.block;
    return C.kinds.includes(l.kind) || (l.tags || []).some(t => C.tags.includes(t)) || C.names.includes(l.name) || C.templates.includes(b.template);
  }

  // ---- world.py: built(), allowed(), design_problems()
  const colour = id => X().kit.colours[id].rgb.slice();
  function innerColour(id) { const p = colour(id), M = X().kit.inner_mix; return [0, 1, 2].map(k => p[k] + (M.with[k] - p[k]) * M.share); }
  function built(d) {
    const K = X().kit, pats = X().patterns, out = clone(d);
    for (const f of FACADES) {
      const fa = out.walls[f]; fa.plaster = colour(fa.plaster);
      fa.stone.color = fa.stone.color === null ? null : pats[fa.stone.pattern][fa.stone.color];
    }
    out.roof.tiles = colour(d.roof.tiles);
    for (const k of ["shutter_color", "frame", "awning"]) out.openings[k] = colour(d.openings[k]);
    const win = K.windows[out.openings.class]; Object.assign(out.openings, { width: win.width, gap: win.gap, tall: win.tall });
    const FF = K.floor_finishes;
    out.inside.floors = d.inside.floors.map(x => [FF[x].mat, FF[x].rgb.slice()]);
    out.inside.walls = d.inside.walls.map(innerColour);
    return out;
  }
  function allowed(item, d) {
    if (!item || item.status !== "approved") return false;
    const wh = item.when || {}, sh = d.shell;
    if ("big" in wh && wh.big !== sh.big) return false;
    if ("old" in wh && wh.old !== sh.old) return false;
    if ("tower" in wh && wh.tower !== sh.tower) return false;
    if ("kinds" in wh && !wh.kinds.includes(sh.kind)) return false;
    if ("not_kinds" in wh && wh.not_kinds.includes(sh.kind)) return false;
    if ("specials" in wh && !wh.specials.includes(d.special)) return false;
    return true;
  }
  function problems(d) {                                    // world.design_problems() for one design, without the lot id
    const K = X().kit, cols = K.colours, pats = X().patterns, out = [];
    const bad = ["shell", "walls", "roof", "openings", "ornament", "inside", "circulation"].filter(p => !(p in d));
    for (const p of bad) out.push("design has no " + p);
    if (bad.length) return out;
    for (const f of FACADES) {
      const fa = d.walls[f];
      if (!fa) { out.push("no " + f + " facade"); continue; }
      if (!K.plaster.includes(fa.plaster)) out.push(f + " facade's plaster colour not in the kit");
      const st = fa.stone || {};
      if (st.pattern !== null && st.pattern !== undefined) {
        if (!(st.pattern in pats)) out.push(f + " facade's stone pattern not in surfaces.json");
        else if (st.color !== null && st.color !== undefined && !(Number.isInteger(st.color) && st.color >= 0 && st.color < pats[st.pattern].length))
          out.push(f + " facade's stone colour is not one " + st.pattern + " comes in");
      }
    }
    if (!K.roof_tiles.includes(d.roof.tiles)) out.push("roof tiles not in the kit");
    if (!(d.roof.type in K.roofs)) out.push("roof type " + d.roof.type + " not in the kit");
    else if (!allowed(K.roofs[d.roof.type], d)) out.push("a " + K.roofs[d.roof.type].name.toLowerCase() + " is not for this building");
    if (!(d.openings.class in K.windows)) out.push("window type not in the kit");
    else if (!allowed(K.windows[d.openings.class], d)) out.push(K.windows[d.openings.class].name.toLowerCase() + " are not for this building");
    for (const [k, lst] of [["awning", "accents"], ["frame", "frames"]]) if (!K[lst].includes(d.openings[k])) out.push(k + " colour not in the kit");
    if (!(d.openings.shutter_color in cols)) out.push("shutter colour not in the kit");
    for (const [p, cat] of [["openings", "openings"], ["ornament", "ornaments"]])
      for (const k of Object.keys(d[p])) if (d[p][k] === true && k in K[cat] && K[cat][k].status !== "approved") out.push(K[cat][k].name.toLowerCase() + " is not approved");
    const n = d.shell.floors, I = d.inside;
    if (!(I.floors.length === I.uses.length && I.uses.length === I.walls.length && I.walls.length === n)) out.push(n + " floors but " + I.floors.length + " finishes");
    I.floors.forEach((x, f) => { if (!(x in K.floor_finishes) || x === "note") out.push("floor " + f + "'s finish not in the kit"); });
    I.walls.forEach((x, f) => { if (!K.plaster.includes(x)) out.push("floor " + f + "'s inside walls not a kit colour"); });
    if (n >= 2 && !d.circulation.stair && !d.shell.plan) out.push("no stair in the design");
    return out;
  }

  // ---- builder.py: measures(), cost(), options(), check()
  function measures(l, d) {
    const [x0, z0, x1, z1] = l.rect, W = x1 - x0, Dp = z1 - z0, sh = d.shell, H = sh.height, n = sh.floors;
    const band = sh.old ? OLD_BASE : GF, gap = X().kit.windows[d.openings.class].gap, length = { n: W, s: W, e: Dp, w: Dp }, facades = {};
    for (const f of FACADES) {
      const fa = d.walls[f], L = length[f], toEaves = fa.stone_to_eaves || fa.brick_to_eaves;
      facades[f] = { stone_m2: L * (toEaves ? H : band), plaster_m2: toEaves ? 0 : L * Math.max(0, H - band), upper_m2: L * Math.max(0, H - band) };
    }
    const windows = FACADES.reduce((s, f) => s + Math.max(1, Math.floor((length[f] - 1.6) / gap)), 0) * n;
    return { facades, roof_m2: W * Dp * (d.roof.type === "pitched" ? 1.25 : 1.0), windows, floor_m2: Math.max(0, (W - .7) * (Dp - .7)), floors: n };
  }
  function cost(l, old, nw) {                               // [total chips, [[what, quantity, chips]]] (BUILD-005)
    const P = X().kit.prices, M = measures(l, nw), lines = [], ch = changed(old, nw), has = p => ch.some(c => match(c, p));
    const add = (what, qty, unit) => { if (qty) lines.push([what, round2(qty), roundEven(qty * unit)]); };
    let plaster = 0, stone = 0, brick = 0;
    for (const f of FACADES) {
      const fa = nw.walls[f], Mf = M.facades[f];
      if (has("walls." + f + ".plaster")) plaster += Mf.plaster_m2;
      if (has("walls." + f + ".stone")) stone += !fa.brick_to_eaves ? Mf.stone_m2 : Mf.stone_m2 - Mf.upper_m2;
      if (has("walls." + f + ".brick_to_eaves") && fa.brick_to_eaves) brick += Mf.upper_m2;
    }
    add("Repaint the plaster", plaster, P.plaster_m2); add("New stone or brick", stone, P.stone_m2); add("Brick to the eaves", brick, P.brick_to_eaves_m2);
    if (has("roof.tiles")) add("Re-tile the roof", M.roof_m2, P.roof_tiles_m2);
    if (has("openings.shutters") && nw.openings.shutters) add("Shutters", M.windows, P.shutters_each);
    else if (has("openings.shutter_color") && nw.openings.shutters) add("Paint the shutters", M.windows, P.shutter_paint_each);
    if (has("openings.frame")) add("Paint the window frames", M.windows, P.frame_each);
    if (has("openings.balcony") && nw.openings.balcony) add("A balcony", 1, P.balcony);
    if (has("openings.keystones") && nw.openings.keystones) add("Keystones over the windows", M.windows, P.keystones_each);
    if (has("ornament.quoins") && nw.ornament.quoins) add("Corner stones", 1, P.quoins);
    if (has("ornament.lantern") && nw.ornament.lantern) add("A wall lantern", 1, P.lantern);
    const s = k => k > 1 ? "s" : "", N = Math.min(old.inside.floors.length, nw.inside.floors.length);
    const floors = [], walls = [];
    for (let f = 0; f < N; f++) { if (old.inside.floors[f] !== nw.inside.floors[f]) floors.push(f); if (old.inside.walls[f] !== nw.inside.walls[f]) walls.push(f); }
    if (floors.length) add("New floors (" + floors.length + " storey" + s(floors.length) + ")", M.floor_m2 * floors.length, P.floor_m2);
    if (walls.length) add("Repaint inside (" + walls.length + " storey" + s(walls.length) + ")", M.floor_m2 * walls.length, P.inner_m2);
    if (lines.length) lines.push(["Permit", 1, P.permit]);
    return [lines.reduce((a, x) => a + x[2], 0), lines];
  }
  function options(l, d) {                                  // what the designer may offer on this building, by part (BUILD-017)
    const K = X().kit, PL = X().planning, pats = X().patterns, C = K.colours;
    const pal = PL.districts[l.block.district] || { stone: [], plaster: [], roof_tiles: [] };
    const col = ids => ids.map(i => ({ id: i, name: C[i].name, rgb: C[i].rgb }));
    const stones = pal.stone.concat(civic(l) ? PL.civic_only.stone : []);
    const cap = s => { const t = s.replace(/_/g, " "); return t[0].toUpperCase() + t.slice(1).toLowerCase(); };
    const menu = {
      "walls.*.plaster": col(K.plaster.filter(c => pal.plaster.includes(c))),
      "walls.*.stone": stones.map(s => ({ id: s, name: cap(s), colors: pats[s].map((c, k) => typeof c === "string" ? null : k), brick_to_eaves: PL.brick_to_eaves.includes(s) })),
      "roof.tiles": col(K.roof_tiles.filter(c => pal.roof_tiles.includes(c))),
      "openings.frame": col(K.frames),
      "inside.walls": col(K.plaster),
      "inside.floors": Object.keys(K.floor_finishes).filter(k => k !== "note").map(k => ({ id: k, name: K.floor_finishes[k].name, rgb: K.floor_finishes[k].rgb, mat: K.floor_finishes[k].mat })),
    };
    if (allowed(K.openings.shutters, d)) {
      menu["openings.shutters"] = [{ id: true, name: "With shutters" }, { id: false, name: "No shutters" }];
      menu["openings.shutter_color"] = col(K[PL.shutters[d.openings.class === "old" ? "old" : "default"]]);
    }
    for (const [p, cat, key] of [["openings.balcony", "openings", "balcony"], ["openings.keystones", "openings", "keystones"], ["ornament.quoins", "ornaments", "quoins"], ["ornament.lantern", "ornaments", "lantern"]]) {
      const it = K[cat][key];
      if (allowed(it, d)) menu[p] = [{ id: true, name: it.name }, { id: false, name: "No " + it.name[0].toLowerCase() + it.name.slice(1) }];
    }
    return menu;
  }
  function held(owner, floors) {
    if (owner.whole) return new Set([...Array(floors).keys()]);
    return new Set(owner.floors || []);
  }
  function check(l, old, nw, owner, funds) {                // [] when the remodel may go ahead, else what is wrong (BUILD-004, -006, -007)
    const K = X().kit, PL = X().planning, R = PL.remodel, out = [], ch = changed(old, nw);
    if (!ch.length) return ["nothing has changed"];
    for (const p of ch) if (!(part(p, R.outside) || part(p, R.inside))) out.push("a remodel cannot change " + p);
    if (ch.some(p => part(p, R.outside)) && !owner.whole) out.push("changing the outside needs the whole building");
    const hf = held(owner, l.floors);
    for (let f = 0; f < l.floors; f++)
      if ((old.inside.floors[f] !== nw.inside.floors[f] || old.inside.walls[f] !== nw.inside.walls[f]) && !hf.has(f)) out.push("floor " + f + " is not yours to change");
    const before = new Set(problems(old));
    for (const p of problems(nw)) if (!before.has(p)) out.push(p);
    const pal = PL.districts[l.block.district];
    if (!pal) { out.push("no planning palette for " + l.block.district); return out; }
    for (const f of FACADES) {
      const fa = nw.walls[f], where = FACADE_NAME[f], st = fa.stone.pattern;
      if (ch.includes("walls." + f + ".plaster") && !pal.plaster.includes(fa.plaster)) out.push("that plaster colour is not one of " + l.block.district + "'s");
      if (ch.some(p => match(p, "walls." + f + ".stone"))) {
        if (PL.civic_only.stone.includes(st)) { if (!civic(l)) out.push(st + " stone is kept for the town's civic buildings"); }
        else if (!pal.stone.includes(st)) out.push(st + " is not one of " + l.block.district + "'s stones or bricks");
        const cols = X().patterns[st] || [], c = fa.stone.color;
        if (c !== null && c !== undefined && (!Number.isInteger(c) || c < 0 || c >= cols.length || typeof cols[c] === "string")) out.push("that colour is not one " + st + " comes in");
      }
      if (fa.brick_to_eaves && !PL.brick_to_eaves.includes(st)) out.push("only brick can run to the eaves (the " + where + " facade)");
    }
    if (ch.includes("roof.tiles") && !pal.roof_tiles.includes(nw.roof.tiles)) out.push("those roof tiles are not " + l.block.district + "'s");
    if (ch.includes("openings.shutter_color") && !K[PL.shutters[nw.openings.class === "old" ? "old" : "default"]].includes(nw.openings.shutter_color))
      out.push("that shutter colour is not for these windows");
    for (const [p, cat, key] of [["openings.shutters", "openings", "shutters"], ["openings.balcony", "openings", "balcony"], ["openings.keystones", "openings", "keystones"], ["ornament.quoins", "ornaments", "quoins"], ["ornament.lantern", "ornaments", "lantern"]])
      if (ch.includes(p) && get(nw, p) && !allowed(K[cat][key], nw)) out.push(K[cat][key].name.toLowerCase() + " cannot go on this building");
    if (funds !== null && funds !== undefined) { const total = cost(l, old, nw)[0]; if (total > funds) out.push("it costs " + pyNum(total / 100) + " brix and you have " + pyNum(funds / 100)); }
    return [...new Set(out)];
  }

  function selfTest(lots) {                                 // the Python's fixed run, result for result
    const C = X().checks, byId = {}, bad = []; for (const l of lots) byId[l.id] = l;
    C.script.forEach(([id, changes, owner, funds], k) => {
      const l = byId[id], old = l.dsg, nw = remodel(old, changes), [total, lines] = cost(l, old, nw), menu = options(l, old), bt = built(nw);
      const ids = {}; for (const p of Object.keys(menu).sort()) ids[p] = menu[p].map(o => o.id);
      const got = { options: ids, problems: check(l, old, nw, owner, funds), total, lines, front: bt.walls[old.shell.side], inside: bt.inside }, want = C.want[k];
      for (const key of Object.keys(want)) if (JSON.stringify(got[key]) !== JSON.stringify(want[key])) bad.push("remodel " + k + " (" + id + ") " + key + ": " + JSON.stringify(got[key]).slice(0, 200) + " should be " + JSON.stringify(want[key]).slice(0, 200));
    });
    return bad;
  }
  return { FACADES, FACADE_NAME, remodel, changed, built, allowed, problems, measures, cost, options, check, selfTest, get };
})();
