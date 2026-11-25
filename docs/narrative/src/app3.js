/* ---------- quest map: stages as boxes, every branch an arrow ---------- */
function stageGraph(q) {
  const edges = [], seen = new Set(), layer = {};
  for (const s of q.stages) {
    layer[s.id] = 0;
    const add = to => { if (!seen.has(s.id + ">" + to)) { seen.add(s.id + ">" + to); edges.push({from: s.id, to}); } };
    if (s.then) add(s.then);
    for (const n of Object.values(s.nodes || {})) for (const o of n.options) for (const a of o.act || []) if (/^(stage|end):/.test(a)) add(a);
  }
  for (let pass = 0; pass < q.stages.length; pass++) for (const e of edges) {
    if (!e.to.startsWith("stage:")) continue;
    const to = e.to.slice(6);
    if (to !== e.from && layer[to] < layer[e.from] + 1) layer[to] = layer[e.from] + 1;
  }
  return {edges, layer};
}
function renderFlow(q, run, onStage, onEnd) {
  const {edges, layer} = stageGraph(q);
  const BW = 150, BH = 50, EW = 180, EH = 34, CG = 44, RG = 12;
  const cols = [];
  q.stages.forEach((s, i) => { (cols[layer[s.id]] = cols[layer[s.id]] || []).push({id: "stage:" + s.id, s, i}); });
  const endCol = q.endings.map(e => ({id: "end:" + e.id, e}));
  const colH = c => c.length * BH + (c.length - 1) * RG, endH = endCol.length * EH + (endCol.length - 1) * RG;
  const H = Math.max(endH, ...cols.map(colH)), pos = {};
  cols.forEach((c, ci) => c.forEach((b, ri) => { pos[b.id] = {x: ci * (BW + CG), y: (H - colH(c)) / 2 + ri * (BH + RG), w: BW, h: BH}; }));
  endCol.forEach((b, ri) => { pos[b.id] = {x: cols.length * (BW + CG), y: (H - endH) / 2 + ri * (EH + RG), w: EW, h: EH}; });
  const W = cols.length * (BW + CG) + EW;
  const been = new Set(run ? run.stagesSeen : []), took = new Set();
  if (run) {
    run.stagesSeen.forEach((sid, i) => { if (i) took.add(run.stagesSeen[i - 1] + ">stage:" + sid); });
    if (run.over) took.add(run.stagesSeen[run.stagesSeen.length - 1] + ">end:" + run.over);
  }
  const doneEnd = run ? run.over : (game.st && ui.mode === "play" ? game.st.done[q.id] : null);
  const lines = edges.map(e => {
    const a = pos["stage:" + e.from], b = pos[e.to], x1 = a.x + a.w, y1 = a.y + a.h / 2, x2 = b.x, y2 = b.y + b.h / 2, mx = (x1 + x2) / 2;
    return svg("path", {class: "fedge" + (took.has(e.from + ">" + e.to) ? " taken" : ""), d: `M${x1},${y1} C${mx},${y1} ${mx},${y2} ${x2 - 7},${y2}`, "marker-end": "url(#arrow)"});
  });
  const boxes = [];
  for (const c of cols) for (const b of c) {
    const p = pos[b.id], s = b.s, state = run && !run.over && run.stage === s.id ? " now" : been.has(s.id) ? " done" : run ? " far" : "";
    boxes.push(h("button", {class: "fbox" + state, style: `left:${p.x}px;top:${p.y}px;width:${p.w}px;height:${p.h}px`, title: s.goal, onclick: () => onStage(s.id)},
      h("span", {class: "k", text: `${b.i + 1} · ${KIND[s.kind]}${s.with ? " · " + people[s.with].name : ""}${s.optional ? " · optional" : ""}`}),
      h("span", {class: "t", text: s.title})));
  }
  for (const b of endCol) {
    const p = pos[b.id], state = doneEnd === b.e.id ? " done" : run ? " far" : "";
    boxes.push(h("button", {class: "fbox end" + state, style: `left:${p.x}px;top:${p.y}px;width:${p.w}px;height:${p.h}px`, title: b.e.log, onclick: () => onEnd(b.e.id)},
      h("span", {class: "t", text: "◆ " + b.e.title})));
  }
  const defs = svg("defs", {}, svg("marker", {id: "arrow", viewBox: "0 0 8 8", refX: 1, refY: 4, markerWidth: 7, markerHeight: 7, orient: "auto"},
    svg("path", {d: "M0,0 L8,4 L0,8 z", style: "fill:var(--soft)"})));
  return h("div", {class: "flowfig"}, h("div", {class: "flow", style: `width:${W}px;height:${H}px`},
    svg("svg", {width: W, height: H, "aria-hidden": "true"}, defs, lines), boxes));
}

/* ---------- the dialogue tree ---------- */
const TW = {line: 270, reply: 205, exit: 180, ref: 205, entry: 185, root: 0};
const HGAP = 38, VGAP = 10;
let treeViews = [];

function buildTreeModel(s) {
  const entries = [...(s.starts || []), {need: null, node: s.start}];
  const dist = {};
  let frontier = [...new Set(entries.map(e => e.node))];
  frontier.forEach(id => { dist[id] = 0; });
  while (frontier.length) {
    const next = [];
    for (const id of frontier) for (const o of s.nodes[id].options) if (o.next && !(o.next in dist)) { dist[o.next] = dist[id] + 1; next.push(o.next); }
    frontier = next;
  }
  const placed = new Set();
  const line = (id, d) => {
    if (placed.has(id) || dist[id] !== d) return {type: "ref", id};
    placed.add(id);
    const node = s.nodes[id];
    return {type: "line", id, node, kids: node.options.map((o, i) => ({type: "reply", id, i, opt: o, kids: [o.next ? line(o.next, d + 1) : {type: "exit", opt: o}]}))};
  };
  if (entries.length === 1) return line(s.start, 0);
  return {type: "root", kids: entries.map(en => ({type: "entry", need: en.need, node: en.node, kids: [line(en.node, 0)]}))};
}

function treeView(q, s, play) {
  const root = buildTreeModel(s), all = [];
  const edgeSvg = svg("svg", {"aria-hidden": "true"}), canvas = h("div", {class: "canvas"}, edgeSvg);
  const sizer = h("div", {}, canvas), fig = h("div", {class: "treefig grab" + (play ? " playing" : "")}, sizer);
  const domId = id => `t-${q.id}-${s.id}-${id}`;
  const flowChips = o => {
    const flow = (o.act || []).filter(a => /^(stage|end):/.test(a));
    return flow.length ? actChips(flow, q, !play) : [chip("", "leaves the talk", "the conversation ends here; the stage stays open")];
  };
  const inner = t => {
    if (t.type === "line") return lineEl(q, s, t.id, t.node, "t");
    if (t.type === "reply") {
      const fx = (t.opt.act || []).filter(a => !/^(stage|end):/.test(a)), chips = [needChips(t.opt.need), actChips(fx, q, false)].flat();
      return h("div", {class: "treply", onclick: () => { if (play && t.el.classList.contains("pickable")) play.pick(t.id, t.i); }},
        h("span", {class: "lbl", text: "You"}), h("span", {}, rich(t.opt.text)), chips.length ? h("span", {class: "chips"}, chips) : null);
    }
    if (t.type === "exit") return h("div", {class: "texit"}, flowChips(t.opt));
    if (t.type === "ref") {
      const n = s.nodes[t.id], who = n.who === "scene" ? "" : people[n.who].name + ": ";
      return h("button", {class: "tref", onclick: () => scrollFlash($(domId(t.id)))}, `↩ goes back to “${who}${firstWords(n)}”`);
    }
    return h("div", {class: "tentry"}, t.need ? ["Opens here", h("span", {class: "chips"}, needChips(t.need))] : "Otherwise opens here");
  };
  const make = (t, parent) => {
    t.parent = parent; t.w = TW[t.type]; t.h = 0;
    if (t.type !== "root") {
      t.el = h("div", {class: "tbox", style: `width:${t.w}px`}, inner(t));
      if (!play && (t.type === "line" || (t.type === "reply" && t.kids[0].type === "line"))) {
        t.foldBtn = h("button", {class: "fold", title: "Fold or unfold what follows", "aria-label": "Fold or unfold what follows", onclick: ev => { ev.stopPropagation(); t.folded = !t.folded; layout(); }});
        t.el.append(t.foldBtn);
      }
      canvas.append(t.el); all.push(t);
    }
    for (const k of t.kids || []) make(k, t);
  };
  make(root, null);
  const kidsOf = t => (t.folded ? [] : t.kids || []);

  function mark() {
    const run = play.run, seen = id => run.seen.has(s.id + "/" + id), cur = id => !run.over && run.stage === s.id && run.node === id && !run.left;
    const took = t => run.taken.has(`${s.id}/${t.id}#${t.i}`);
    for (const t of all) {
      let cls = "";
      t.lit = false;
      if (t.type === "line") {
        cls = cur(t.id) ? "current" : seen(t.id) ? "visited" : "";
        const p = t.parent;
        t.lit = cls !== "" && (!p || p.type === "root" || (p.type === "reply" ? took(p) : run.entries[s.id] === p.node));
      } else if (t.type === "reply") {
        t.lit = took(t);
        cls = cur(t.id) ? (needsOk(t.opt.need, game.st) ? "pickable" : "locked") + (t.lit ? " taken" : "") : t.lit ? "taken" : "";
      } else if (t.type === "entry") {
        t.lit = run.entries[s.id] === t.node && run.seen.size > 0 && seen(t.node);
        cls = t.lit ? "visited" : "";
      } else {
        t.lit = took(t.parent);
        cls = t.lit ? "visited" : "";
      }
      t.el.className = "tbox " + (cls || "dim");
    }
  }
  function layout() {
    if (!fig.getClientRects().length) return;
    if (play) mark();
    const vis = (t, on) => { if (t.el) t.el.hidden = !on; for (const k of t.kids || []) vis(k, on && !t.folded); };
    vis(root, true);
    for (const t of all) if (!t.el.hidden) t.h = t.el.offsetHeight;
    const size = t => { let kh = 0; kidsOf(t).forEach((k, i) => { kh += size(k) + (i ? VGAP : 0); }); t.kh = kh; return (t.sub = Math.max(t.h, kh)); };
    let maxX = 0;
    const place = (t, x, y) => {
      t.x = x; t.y = y;
      if (t.el) { t.el.style.left = x + "px"; t.el.style.top = t.y + "px"; maxX = Math.max(maxX, x + t.w); }
      let cy = y;
      for (const k of kidsOf(t)) { place(k, x + t.w + (t.w ? HGAP : 0), cy); cy += k.sub + VGAP; }
    };
    size(root); place(root, 0, 0);
    const W = maxX + 14, H = root.sub + 2, paths = [];
    const link = t => {
      for (const k of kidsOf(t)) {
        if (t.el) {
          const x1 = t.x + t.w, y1 = t.y + t.h / 2, x2 = k.x, y2 = k.y + k.h / 2, mx = (x1 + x2) / 2;
          paths.push(svg("path", {class: "tedge" + (k.lit ? " lit" : ""), d: `M${x1},${y1} C${mx},${y1} ${mx},${y2} ${x2},${y2}`}));
        }
        link(k);
      }
    };
    link(root);
    for (const t of all) {
      if (t.type !== "ref" || !t.lit || t.el.hidden) continue;
      const g = all.find(x => x.type === "line" && x.id === t.id);
      if (!g || g.el.hidden) continue;
      const x1 = t.x + t.w, y1 = t.y + t.h / 2, x2 = g.x, y2 = g.y + g.h / 2;
      paths.push(svg("path", {class: "tedge lit jump", d: `M${x1},${y1} C${x1 + 80},${y1} ${x2 - 80},${y2} ${x2},${y2}`}));
    }
    edgeSvg.replaceChildren(...paths.filter(p => !p.classList.contains("lit")), ...paths.filter(p => p.classList.contains("lit")));
    edgeSvg.setAttribute("width", W); edgeSvg.setAttribute("height", H);
    for (const t of all) if (t.foldBtn) { t.foldBtn.textContent = t.folded ? "+" : "−"; }
    const z = ui.zoom === "fit" ? Math.max(0.3, Math.min(1, (fig.clientWidth - 34) / W)) : ui.zoom;
    canvas.style.width = W + "px"; canvas.style.height = H + "px"; canvas.style.transform = `scale(${z})`;
    sizer.style.width = W * z + "px"; sizer.style.height = H * z + "px";
    if (play) {
      const here = all.find(t => t.type === "line" && t.el.classList.contains("current")) || [...all].reverse().find(t => t.lit);
      if (here) {
        fig.scrollLeft = Math.max(0, here.x * z - 70);
        fig.scrollTop = Math.max(0, (here.y + here.h / 2) * z - fig.clientHeight / 2);
      }
    }
  }
  fig.addEventListener("pointerdown", ev => {
    if (ev.pointerType !== "mouse" || ev.target.closest("button,.pickable,a")) return;
    const x0 = ev.clientX, y0 = ev.clientY, s0 = fig.scrollLeft, t0 = fig.scrollTop;
    const mv = e => { fig.scrollLeft = s0 - (e.clientX - x0); fig.scrollTop = t0 - (e.clientY - y0); if (Math.abs(e.clientX - x0) + Math.abs(e.clientY - y0) > 4) fig.classList.add("drag"); };
    const up = () => { fig.classList.remove("drag"); removeEventListener("pointermove", mv); removeEventListener("pointerup", up); };
    addEventListener("pointermove", mv); addEventListener("pointerup", up);
  });
  const foldAll = on => {
    for (const t of all) t.folded = on && t.type === "line" && !!t.parent && t.parent.type === "reply";
    layout();
  };
  const view = {fig, layout, foldAll};
  treeViews.push(view);
  return view;
}
function relayoutTrees() { for (const v of treeViews) v.layout(); }
function setZoom(z) { ui.zoom = z; relayoutTrees(); }

/* tree mode: every stage with its conversation drawn as a tree */
function renderTreeMode(q, body) {
  q.stages.forEach((s, i) => {
    const inner = h("div", {class: "stagebody"});
    if (s.nodes) inner.append(treeView(q, s, null).fig);
    else inner.append(h("div", {class: "chips", style: "margin-top:6px"}, h("span", {class: "small", text: "No conversation here. When it is done:"}), actChips([s.then], q, true)));
    body.append(h("div", {class: "stage", id: `s-${q.id}-${s.id}`}, stageHead(q, s, i), inner));
  });
  body.append(h("h3", {style: "margin-top:36px", text: "Endings"}), h("div", {class: "endgrid"}, q.endings.map(e => endingCard(q, e))));
}
