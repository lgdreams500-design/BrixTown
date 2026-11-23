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

// Furnishing a home in the designer (FURN-051, FURN-052, FURN-055): Tools/citysim/furnish.py, ported. A layout is a list of
// pieces {id, kind ("town" or a piece of World/rules/furnishing.json), name, w, d, h, x, z, yaw (degrees), rug, line}.
// Furnish.selfTest() runs furnish.check_script() and compares every result with the Python's (D.life.builder.furnish.checks).
var Furnish = (() => {
  const X = () => window.WASTED.life.builder.furnish, F = () => X().rules, TOL = 1e-3;
  function corners(p) {
    const a = p.yaw * Math.PI / 180, c = Math.cos(a), s = Math.sin(a);
    return [[-1, -1], [1, -1], [1, 1], [-1, 1]].map(([u, v]) => { u = u * p.w / 2; v = v * p.d / 2; return [p.x + c * u + s * v, p.z - s * u + c * v]; });
  }
  function apart(A, B) {                                    // kept apart by a line along one of their edges
    for (const P of [A, B]) for (let k = 0; k < P.length; k++) {
      const ex = P[(k + 1) % P.length][0] - P[k][0], ez = P[(k + 1) % P.length][1] - P[k][1], n = Math.hypot(ex, ez); if (n < 1e-9) continue;
      const nx = -ez / n, nz = ex / n, pa = A.map(([x, z]) => x * nx + z * nz), pb = B.map(([x, z]) => x * nx + z * nz);
      if (Math.min(Math.max(...pa), Math.max(...pb)) - Math.max(Math.min(...pa), Math.min(...pb)) <= TOL) return true;
    }
    return false;
  }
  const rect = ([x0, z0, x1, z1]) => [[x0, z0], [x1, z0], [x1, z1], [x0, z1]], the = p => "the " + p.name.toLowerCase();
  const sold = p => p.kind === "town" || p.kind in F().pieces;
  function problems(room, keep, ceiling, pieces) {
    const R = F(), g = R.grid_m, turn = R.turn_deg, out = [], say = (k, m) => out.push([k, m]);
    for (const p of pieces) {
      if (!sold(p)) { say(p.id + "|sold", "the " + p.kind + " is not sold here"); continue; }
      if (p.kind !== "town" && !R.lines.includes(p.line)) say(p.id + "|line", the(p) + " has no line " + p.line);
      if (Math.abs(p.yaw / turn - Math.round(p.yaw / turn)) > 1e-6) say(p.id + "|turn", the(p) + " is not turned in " + turn + " degree steps");
      if (p.kind !== "town" && [p.x, p.z].some(v => Math.abs(v / g - Math.round(v / g)) > 1e-6)) say(p.id + "|grid", the(p) + " is off the " + Math.round(g * 100) + " cm grid");
      const C = corners(p), xs = C.map(q => q[0]), zs = C.map(q => q[1]);
      if (Math.min(...xs) < room[0] - TOL || Math.max(...xs) > room[2] + TOL || Math.min(...zs) < room[1] - TOL || Math.max(...zs) > room[3] + TOL) say(p.id + "|room", the(p) + " sticks out of the room");
      if (p.h > ceiling + TOL) say(p.id + "|ceiling", the(p) + " does not fit under the ceiling");
      if (!p.rug && keep.some(r => !apart(C, rect(r)))) say(p.id + "|keep", the(p) + " is in the way of the door or the stairs");
    }
    const stand = pieces.filter(p => !p.rug && sold(p));
    for (let i = 0; i < stand.length; i++) for (let j = i + 1; j < stand.length; j++) { const a = stand[i], b = stand[j];
      if (!apart(corners(a), corners(b))) say([a.id, b.id].sort().join("|") + "|clash", the(a) + " and " + the(b) + " stand in each other"); }
    return out;
  }
  function cost(before, after, belongings) {
    const was = new Map(before.map(p => [p.id, p])), now = new Set(after.map(p => p.id)), lines = [], P = F().pieces, mine = new Set((belongings || []).map(b => b.id));
    for (const p of after) { const o = was.get(p.id);
      if (!o && mine.has(p.id)) lines.push(["Place: " + p.name, 1, 0]);
      else if (!o) lines.push(["Buy: " + p.name, 1, P[p.kind] ? P[p.kind].price : 0]);
      else if (o.x !== p.x || o.z !== p.z || o.yaw !== p.yaw) lines.push(["Move: " + p.name, 1, 0]); }
    for (const p of before) if (!now.has(p.id)) lines.push(["To your belongings: " + p.name, 1, 0]);
    return [lines.reduce((s, l) => s + l[2], 0), lines];
  }
  function check(room, keep, ceiling, before, after, funds, belongings) {
    const old = new Set(problems(room, keep, ceiling, before).map(q => q[0]));
    const out = [...new Set(problems(room, keep, ceiling, after).filter(q => !old.has(q[0])).map(q => q[1]))], total = cost(before, after, belongings)[0];
    const was = new Set(before.map(p => p.id)), mine = new Set((belongings || []).map(b => b.id));
    for (const p of after) if (!was.has(p.id) && p.id.startsWith("b") && !mine.has(p.id)) out.push(the(p) + " is not among your belongings");
    if (funds != null && total > funds) out.push("you cannot pay " + Math.floor(total / 100) + "." + String(total % 100).padStart(2, "0") + " brix");
    return out;
  }
  function kitPiece(kind, id, x, z, yaw, line) {
    const P = F().pieces[kind] || { name: kind, size: [1, 1, 1] }, [w, d, h] = P.size;
    return { id, kind, name: P.name, w, d, h, x, z, yaw, rug: false, line };
  }
  function stow(before, after, belongings, where) {          // the belongings after a change (INV-001): less what was placed, with what was taken away
    const now = new Set(after.map(p => p.id)), out = (belongings || []).filter(b => !now.has(b.id)), w = where || {};
    for (const p of before) {
      if (now.has(p.id)) continue;
      const q = {}; for (const k of Object.keys(p)) if (!["x", "z", "yaw"].includes(k)) q[k] = p[k];
      if (!p.id.startsWith("b")) { q.id = "b:" + (w.lot ?? "") + ":" + (w.level ?? "") + ":" + p.id; if (p.kind === "town" && !("from" in p)) q.from = Object.assign({}, w, { id: p.id }); }
      out.push(q);
    }
    return out;
  }
  function apply(before, edit, belongings) {
    const out = [];
    for (const p of before) { if ((edit.remove || []).includes(p.id)) continue; const q = Object.assign({}, p);
      if (edit.move && p.id in edit.move) [q.x, q.z, q.yaw] = edit.move[p.id]; out.push(q); }
    for (const [id, kind, x, z, yaw, line] of edit.add || []) out.push(kitPiece(kind, id, x, z, yaw, line));
    const mine = new Map((belongings || []).map(b => [b.id, b]));
    for (const [bid, x, z, yaw] of edit.place || []) out.push(Object.assign({}, mine.get(bid) || { id: bid, kind: "town", name: "Piece", w: 1.0, d: 1.0, h: 1.0, rug: false }, { x, z, yaw }));
    return out;
  }
  function selfTest(SIZE) {                                // the Python's fixed run, result for result; and the list's sizes as furniture.js draws them
    const C = X().checks, S = C.script, bad = [];
    S.cases.forEach(([ceiling, edit, funds], k) => {
      const B = S.belongings, after = apply(S.before, edit, B), [total, lines] = cost(S.before, after, B);
      const got = { problems: check(S.room, S.keep, ceiling, S.before, after, funds, B), total, lines, belongings: stow(S.before, after, B, S.where) }, want = C.want[k];
      for (const key of Object.keys(want)) if (JSON.stringify(got[key]) !== JSON.stringify(want[key])) bad.push("furnish case " + k + " " + key + ": " + JSON.stringify(got[key]) + " should be " + JSON.stringify(want[key]));
    });
    if (SIZE) for (const [k, p] of Object.entries(F().pieces)) if (JSON.stringify(SIZE[k]) !== JSON.stringify(p.size)) bad.push("furnishing.json " + k + " size " + JSON.stringify(p.size) + " but furniture.js draws " + JSON.stringify(SIZE[k]));
    return bad;
  }
  return { corners, problems, cost, check, kitPiece, apply, stow, selfTest, rules: () => F() };
})();

// The room plan of a storey (BUILD-021 to BUILD-023): Tools/citysim/roomplan.py, ported. A wall is {id, ax ("x" or "z"),
// fix, a0, a1, door (its doorway's middle, or null), colour (a kit colour id, or null)}. RoomPlan.selfTest() runs
// roomplan.check_script() and compares every result with the Python's (D.life.builder.room_plan.checks).
var RoomPlan = (() => {
  const X = () => window.WASTED.life.builder.room_plan, P = () => X().rules, TOL = 1e-6;
  const q2 = x => Math.floor(x * 100 + .5) / 100, chips = x => Math.floor(x + .5);
  function box(w) { const h = P().thick / 2; return w.ax === "x" ? [w.a0, w.fix - h, w.a1, w.fix + h] : [w.fix - h, w.a0, w.fix + h, w.a1]; }
  const meet = (a, b) => a[0] < b[2] - TOL && b[0] < a[2] - TOL && a[1] < b[3] - TOL && b[1] < a[3] - TOL;
  function blocked(room, walls) {
    const R = P(), c = R.cell, h = R.thick / 2 + c / 2, nx = Math.floor((room[2] - room[0]) / c + TOL), nz = Math.floor((room[3] - room[1]) / c + TOL), out = new Set();
    for (const w of walls) for (let i = 0; i < nx; i++) for (let j = 0; j < nz; j++) {
      const x = room[0] + (i + .5) * c, z = room[1] + (j + .5) * c, along = w.ax === "x" ? x : z, across = w.ax === "x" ? z : x;
      if (Math.abs(across - w.fix) < h - TOL && along > w.a0 - c / 2 && along < w.a1 + c / 2)
        if (w.door === null || Math.abs(along - w.door) > R.door_w / 2 - c / 2 + TOL) out.add(i * 100000 + j);
    }
    return [nx, nz, out];
  }
  function reach(room, walls, entries) {
    const c = P().cell, [nx, nz, bl] = blocked(room, walls), K = (i, j) => i * 100000 + j;
    const cell = p => [Math.min(nx - 1, Math.max(0, Math.floor((p[0] - room[0]) / c))), Math.min(nz - 1, Math.max(0, Math.floor((p[1] - room[1]) / c)))];
    const st = cell(entries[0]), seen = new Set(), nb = (i, j) => [[i + 1, j], [i - 1, j], [i, j + 1], [i, j - 1]];
    if (!bl.has(K(...st))) { const todo = [st]; seen.add(K(...st));
      while (todo.length) { const [i, j] = todo.pop(); for (const [a, b] of nb(i, j)) if (a >= 0 && a < nx && b >= 0 && b < nz && !bl.has(K(a, b)) && !seen.has(K(a, b))) { seen.add(K(a, b)); todo.push([a, b]); } } }
    const got = entries.map(p => seen.has(K(...cell(p)))), left = new Set(), pockets = [];
    for (let i = 0; i < nx; i++) for (let j = 0; j < nz; j++) if (!bl.has(K(i, j)) && !seen.has(K(i, j))) left.add(K(i, j));
    while (left.size) { const a = left.values().next().value; left.delete(a); const todo = [[Math.floor(a / 100000), a % 100000]]; let n = 1;
      while (todo.length) { const [i, j] = todo.pop(); for (const [x, y] of nb(i, j)) if (left.has(K(x, y))) { left.delete(K(x, y)); todo.push([x, y]); n++; } }
      pockets.push(Math.round(n * c * c * 10000) / 10000); }
    return [got, pockets.sort((p, q) => q - p)];
  }
  function problems(room, keep, entries, colours, walls) {
    const R = P(), g = R.grid_m, out = [], say = (k, m) => out.push([k, m]);
    for (const w of walls) {
      const k = w.id + "|";
      if (w.ax !== "x" && w.ax !== "z") { say(k + "ax", "a wall must run along x or z"); continue; }
      if (w.a1 - w.a0 < R.min_len - TOL) say(k + "short", "a wall is shorter than " + R.min_len.toFixed(1) + " m");
      if ([w.fix, w.a0, w.a1].concat(w.door !== null ? [w.door] : []).some(v => Math.abs(v / g - Math.round(v / g)) > 1e-6)) say(k + "grid", "a wall is off the " + Math.round(g * 100) + " cm grid");
      const [lo, hi, flo, fhi] = w.ax === "x" ? [room[0], room[2], room[1], room[3]] : [room[1], room[3], room[0], room[2]];
      if (w.a0 < lo - TOL || w.a1 > hi + TOL || !(flo + TOL < w.fix && w.fix < fhi - TOL)) say(k + "room", "a wall runs out of the room");
      if (w.door !== null && !(w.a0 + R.door_w / 2 + R.door_margin - TOL <= w.door && w.door <= w.a1 - R.door_w / 2 - R.door_margin + TOL)) say(k + "door", "a doorway does not fit in its wall");
      if (w.colour !== null && !colours.includes(w.colour)) say(k + "colour", "that wall colour is not one of the district's");
      if (keep.some(r => meet(box(w), r))) say(k + "keep", "a wall stands in the way of the door or the stairs");
    }
    const ok = walls.filter(w => w.ax === "x" || w.ax === "z");
    for (let i = 0; i < ok.length; i++) for (let j = i + 1; j < ok.length; j++) if (meet(box(ok[i]), box(ok[j])) && ok[i].ax === ok[j].ax) say([ok[i].id, ok[j].id].sort().join("|") + "|clash", "two walls stand in each other");
    const [got, pockets] = reach(room, ok, entries);
    if (!got.slice(1).every(Boolean)) say("reach|stairs", "the stairs cannot be reached from the way in");
    if (!got[0]) say("reach|door", "a wall stands where you come in");
    if (pockets.some(a => a >= R.pocket_m2 - TOL)) say("reach|room", "a room cannot be reached from the way in");
    return out;
  }
  function cost(before, after, height) {
    const R = P().prices, was = new Map(before.map(w => [w.id, w])), now = new Set(after.map(w => w.id)), lines = [], area = w => q2((w.a1 - w.a0) * height);
    for (const w of after) {
      const o = was.get(w.id);
      if (!o) { lines.push(["New wall", area(w), chips(area(w) * R.wall_m2)]); if (w.door !== null) lines.push(["New doorway", 1, R.doorway]); if (w.colour !== null) lines.push(["Paint a wall", q2(area(w) * 2), chips(area(w) * 2 * R.paint_m2)]); continue; }
      if (o.ax !== w.ax || o.fix !== w.fix || o.a0 !== w.a0 || o.a1 !== w.a1) lines.push(["Move a wall", area(w), chips(area(w) * R.move_m2)]);
      if (o.door !== w.door) lines.push(w.door !== null ? ["New doorway", 1, R.doorway] : ["Close a doorway", 1, R.close_doorway]);
      if (o.colour !== w.colour) lines.push(["Paint a wall", q2(area(w) * 2), chips(area(w) * 2 * R.paint_m2)]);
    }
    for (const w of before) if (!now.has(w.id)) lines.push(["Take down a wall", area(w), chips(area(w) * R.take_down_m2)]);
    return [lines.reduce((s, l) => s + l[2], 0), lines];
  }
  function check(room, keep, entries, height, colours, before, after, funds) {
    const old = new Set(problems(room, keep, entries, colours, before).map(q => q[0]));
    const out = [...new Set(problems(room, keep, entries, colours, after).filter(q => !old.has(q[0])).map(q => q[1]))], total = cost(before, after, height)[0];
    if (funds != null && total > funds) out.push("you cannot pay " + Math.floor(total / 100) + "." + String(total % 100).padStart(2, "0") + " brix");
    return out;
  }
  function apply(before, edit) {
    const out = [];
    for (const w of before) { if ((edit.remove || []).includes(w.id)) continue; out.push(Object.assign({}, w, (edit.set || {})[w.id] || {})); }
    return out.concat((edit.add || []).map(w => Object.assign({}, w)));
  }
  function selfTest() {
    const C = X().checks, S = C.script, bad = [];
    S.cases.forEach(([edit, funds], k) => {
      const after = apply(S.before, edit), [total, lines] = cost(S.before, after, S.height), got = { problems: check(S.room, S.keep, S.entries, S.height, S.colours, S.before, after, funds), total, lines }, want = C.want[k];
      for (const key of Object.keys(want)) if (JSON.stringify(got[key]) !== JSON.stringify(want[key])) bad.push("room plan case " + k + " " + key + ": " + JSON.stringify(got[key]) + " should be " + JSON.stringify(want[key]));
    });
    return bad;
  }
  return { box, reach, problems, cost, check, apply, selfTest, rules: () => P() };
})();
