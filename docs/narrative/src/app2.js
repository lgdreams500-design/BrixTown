/* ---------- quest lines: play ---------- */
const game = {st: null, run: null, snap: {}};
function newState() { return {flags: new Set(), items: {}, rep: {}, money: 2000, done: {}, home: null, tried: new Set(), again: new Set(), awarded: {}}; }
function cloneState(st) { return {flags: new Set(st.flags), items: {...st.items}, rep: {...st.rep}, money: st.money, done: {...st.done}, home: st.home, tried: new Set(st.tried), again: new Set(st.again), awarded: JSON.parse(JSON.stringify(st.awarded))}; }
function needOk(need, st) {
  const neg = need.startsWith("!"), raw = neg ? need.slice(1) : need;
  const i = raw.indexOf(":"), kind = raw.slice(0, i), rest = raw.slice(i + 1);
  let ok = false;
  if (kind === "flag") ok = st.flags.has(rest);
  else if (kind === "have") ok = (st.items[rest] || 0) > 0;
  else if (kind === "money") ok = st.money >= +rest;
  else if (kind === "rep") { const [who, n] = rest.split(":"); ok = (st.rep[who] || 0) >= +n; }
  else if (kind === "done") { const [qid, ends] = rest.split(":"), got = st.done[qid]; ok = got != null && (ends === "*" || ends.split("|").includes(got)); }
  else if (kind === "home") ok = st.home === rest;
  else if (kind === "again") ok = st.again.has(rest);
  return ok !== neg;
}
const needsOk = (needs, st) => (needs || []).every(n => needOk(n, st));
const clampRep = n => Math.max(-100, Math.min(100, n));
function applyActs(acts, st, ledger) {
  let move = null;
  for (const act of acts || []) {
    const i = act.indexOf(":"), kind = act.slice(0, i), rest = act.slice(i + 1);
    if (kind === "flag") st.flags.add(rest);
    else if (kind === "unflag") st.flags.delete(rest);
    else if (kind === "give") st.items[rest] = (st.items[rest] || 0) + 1;
    else if (kind === "take") { if (st.items[rest] > 0) st.items[rest]--; }
    else if (kind === "rep") { const [who, n] = rest.split(":"); st.rep[who] = clampRep((st.rep[who] || 0) + +n); if (ledger) ledger[who] = (ledger[who] || 0) + +n; }
    else if (kind === "pay") st.money += +rest;
    else if (kind === "charge") st.money -= +rest;
    else if (kind === "home") st.home = rest;
    else if (kind === "stage" || kind === "end") move = [kind, rest];
  }
  return move;
}
/* A quest line taken again starts clean: what its own replies set, gave and awarded is taken back. Its ending's cost stays. */
function beginQuest(q, st) {
  if (st.tried.has(q.id)) {
    st.again.add(q.id);
    for (const s of q.stages) for (const n of Object.values(s.nodes || {})) for (const o of n.options) for (const a of o.act || []) {
      if (a.startsWith("flag:")) st.flags.delete(a.slice(5));
      else if (a.startsWith("give:")) delete st.items[a.slice(5)];
    }
    for (const [who, n] of Object.entries(st.awarded[q.id] || {})) st.rep[who] = clampRep((st.rep[who] || 0) - n);
  }
  st.tried.add(q.id);
  return (st.awarded[q.id] = {});
}
function applyEnding(q, id, st) {
  const e = endingOf(q, id);
  st.money += (e.pay || 0) - (e.charge || 0);
  for (const [who, n] of Object.entries(e.rep || {})) st.rep[who] = clampRep((st.rep[who] || 0) + n);
  for (const g of e.give || []) st.items[g] = (st.items[g] || 0) + 1;
  for (const g of e.take || []) if (st.items[g] > 0) st.items[g]--;
  for (const f of e.flags || []) st.flags.add(f);
  st.done[q.id] = id;
}
function startNode(s, st) {
  for (const alt of s.starts || []) if (needsOk(alt.need, st)) return alt.node;
  return s.start;
}
const moving = o => o.next || (o.act || []).some(a => /^(stage|end):/.test(a));
function autoplay(q, st) {
  const ledger = beginQuest(q, st);
  let s = stageOf(q, q.start);
  for (let guard = 0; guard < 600; guard++) {
    if (!s.nodes) {
      applyActs(s.sim, st);
      const [kind, target] = s.then.split(":");
      if (kind === "end") return applyEnding(q, target, st);
      s = stageOf(q, target);
      continue;
    }
    let nid = startNode(s, st), moved = false;
    for (let g2 = 0; g2 < 200 && !moved; g2++) {
      const open = s.nodes[nid].options.filter(o => needsOk(o.need, st));
      const pool = open.filter(moving).length ? open.filter(moving) : open;
      const o = pool[Math.floor(Math.random() * pool.length)];
      const move = applyActs(o.act, st, ledger);
      if (move && move[0] === "end") return applyEnding(q, move[1], st);
      if (move) { s = stageOf(q, move[1]); moved = true; } else if (!o.next) break; else nid = o.next;
    }
  }
}
function rollQuest(p, ends, depth) {
  for (let tries = 0; tries < 14; tries++) {
    if (game.st.done[p.id] && game.snap[p.id]) game.st = cloneState(game.snap[p.id]);
    else if (game.st.done[p.id]) delete game.st.done[p.id];
    if (!satisfy(p, depth + 1)) return false;
    game.snap[p.id] = cloneState(game.st);
    autoplay(p, game.st);
    const got = game.st.done[p.id];
    if (got && (ends === "*" || ends.split("|").includes(got))) return true;
  }
  return false;
}
function satisfy(q, depth) {
  if (depth > 14) return false;
  for (let pass = 0; pass < 12 && !needsOk(q.needs, game.st); pass++) {
    for (const need of q.needs || []) {
      if (needOk(need, game.st)) continue;
      if (need.startsWith("!")) return false;
      const [kind, a, b] = need.split(":");
      if (kind === "rep") game.st.rep[a] = +b;
      else if (kind === "done") { if (!rollQuest(quests[a], b, depth)) return false; }
      else return false;
    }
  }
  return needsOk(q.needs, game.st);
}
function skipAhead(q) {
  const missing = (q.needs || []).filter(n => !needOk(n, game.st));
  if (missing.length && missing.every(n => n.startsWith("rep:"))) return satisfy(q, 0);   // only regard is short: touch nothing else
  for (const id of ["new_in_town", "errand_runner", "night_gardener", "own_stall"]) {
    if (id === q.id) break;
    if (game.st.done[id] || !needsOk(quests[id].needs, game.st)) continue;
    game.snap[id] = cloneState(game.st);
    autoplay(quests[id], game.st);
  }
  return satisfy(q, 0);
}
function beginRun(q) {
  game.snap[q.id] = cloneState(game.st);
  game.run = {ledger: beginQuest(q, game.st), qid: q.id, stage: null, node: null, log: [], over: null, left: false, view: null,
    seen: new Set(), taken: new Set(), entries: {}, stagesSeen: []};
  enterStage(q, q.start);
}
function enterStage(q, sid) {
  const run = game.run, s = stageOf(q, sid);
  run.stage = sid; run.left = false; run.view = null;
  run.stagesSeen.push(sid);
  run.log.push({t: "stage", sid});
  if (s.nodes) {
    run.node = startNode(s, game.st);
    run.entries[sid] = run.node;
    run.seen.add(sid + "/" + run.node);
    run.log.push({t: "line", sid, nid: run.node});
  } else run.node = null;
}
function finishMove(q, move) {
  const run = game.run;
  if (move[0] === "end") { applyEnding(q, move[1], game.st); run.over = move[1]; run.view = null; } else enterStage(q, move[1]);
}
function pick(q, o) {
  const run = game.run, s = stageOf(q, run.stage);
  run.taken.add(`${run.stage}/${run.node}#${s.nodes[run.node].options.indexOf(o)}`);
  run.view = null;
  run.log.push({t: "you", text: o.text});
  const fx = (o.act || []).filter(a => !/^(stage|end):/.test(a));
  if (fx.length) run.log.push({t: "fx", acts: fx});
  const move = applyActs(o.act, game.st, run.ledger);
  if (move) finishMove(q, move);
  else if (o.next) { run.node = o.next; run.seen.add(run.stage + "/" + o.next); run.log.push({t: "line", sid: run.stage, nid: o.next}); }
  else { run.left = true; }
  renderQuests();
}
function talkAgain(q) {
  const run = game.run, s = stageOf(q, run.stage);
  run.left = false; run.node = startNode(s, game.st);
  run.seen.add(s.id + "/" + run.node);
  run.log.push({t: "line", sid: s.id, nid: run.node});
  renderQuests();
}
function doStage(q) {
  const s = stageOf(q, game.run.stage);
  if (s.sim) { applyActs(s.sim, game.st); game.run.log.push({t: "fx", acts: s.sim, sim: true}); }
  finishMove(q, s.then.split(":"));
  renderQuests();
}
function renderPlay(q, body) {
  const st = game.st, run = game.run, left = h("div", {class: "talk"});
  const nextQuest = () => { if (q.next) return quests[q.next]; const i = baker.findIndex(b => b.id === q.id); return i >= 0 ? baker[i + 1] : null; };
  const rewind = () => { if (game.snap[q.id]) game.st = cloneState(game.snap[q.id]); else delete game.st.done[q.id]; game.run = null; renderQuests(); };
  const goOn = () => (nextQuest() && st.done[q.id] && !endingOf(q, st.done[q.id]).retake ? h("button", {class: "btn go", text: "Go on to " + nextQuest().title, onclick: () => openQuest(nextQuest().id, "play")}) : null);
  let treeStage = null;
  if (!run && !needsOk(q.needs, st)) {
    const missing = (q.needs || []).filter(n => !needOk(n, st));
    const closed = missing.filter(n => n.startsWith("!")), regardOnly = missing.every(n => n.startsWith("rep:"));
    const now = n => " (now " + (st.rep[n.split(":")[1]] || 0) + ")";
    left.append(h("div", {class: "nowcard"},
      h("span", {class: "lbl", text: "Not yet"}),
      h("p", {style: "margin:0", text: "This quest line is not open in your story. Still missing:"}),
      h("ul", {class: "plain"}, missing.map(n => h("li", {text: needPill(n) + (n.startsWith("rep:") ? now(n) : "")}))),
      closed.length
        ? h("p", {class: "small", style: "margin:0", text: "This line is closed in your story. Rewind or reset to try it."})
        : h("div", {class: "chips"},
            h("button", {class: "btn go", text: regardOnly ? "Let time pass" : "Skip ahead", onclick: () => { skipAhead(q); renderQuests(); }}),
            h("span", {class: "small", text: regardOnly
              ? "Lifts her regard to the level needed. In the game that is shifts at the café, a little at a time."
              : "Plays what comes before with random choices, and lifts regard where a level is needed, so you arrive with a past. Or play it yourself, in order."}))));
  } else if (!run) {
    left.append(h("div", {class: "nowcard"}, h("span", {class: "lbl", text: "Finished in this story"}), endingCard(q, endingOf(q, st.done[q.id])),
      h("div", {class: "chips"}, h("button", {class: "btn", text: "Rewind to the start of this quest", onclick: rewind}), goOn())));
  } else {
    const s = stageOf(q, run.stage), card = h("div", {class: "nowcard"});
    if (run.over) {
      card.append(h("span", {class: "lbl", text: "Ending reached"}), endingCard(q, endingOf(q, run.over)),
        h("div", {class: "chips"}, goOn(),
          endingOf(q, run.over).retake ? h("button", {class: "btn go", text: "Take it again, as the game would", title: "No rewind: the quest starts clean, and what this ending cost stays",
            onclick: () => { game.run = null; delete game.st.done[q.id]; renderQuests(); }}) : null,
          h("button", {class: "btn", text: "Rewind and try another way", onclick: rewind})));
    } else {
      card.append(h("span", {class: "lbl", text: `Now · stage ${q.stages.indexOf(s) + 1} of ${q.stages.length} · ${s.title}`}),
        h("div", {class: "small", text: "Goal: " + s.goal}));
      const fresh = run.log.length > 1 && run.log[run.log.length - 2].t === "stage" || run.log[run.log.length - 1].t === "stage";
      if (fresh && s.scene) card.append(h("p", {class: "scene", style: "margin:0", text: s.scene}));
      if (!s.nodes) {
        card.append(h("div", {class: "choices"}, h("button", {class: "choice", onclick: () => doStage(q)}, "(Do it.)", h("span", {class: "small", text: s.log}))));
      } else if (run.left) {
        card.append(h("p", {class: "small", style: "margin:0", text: "You leave it there. The stage stays open."}),
          h("div", {class: "choices"}, h("button", {class: "choice", text: "(Talk again.)", onclick: () => talkAgain(q)})));
      } else {
        const opts = s.nodes[run.node].options, open = opts.filter(o => needsOk(o.need, st));
        const shown = new Set(open.map(o => o.text)), box = h("div", {class: "choices"});
        for (const o of open) box.append(h("button", {class: "choice", onclick: () => pick(q, o)}, h("span", {}, rich(o.text))));
        for (const o of opts) {
          if (shown.has(o.text)) continue;
          shown.add(o.text);
          box.append(h("button", {class: "choice", disabled: true}, h("span", {}, rich(o.text)), needChips(o.need.filter(n => !needOk(n, st)))));
        }
        card.append(lineEl(q, s, run.node, s.nodes[run.node], "now"), box);
      }
    }
    left.append(card);
    const replies = run.log.filter(en => en.t === "you").length;
    if (replies) {
      const path = h("div", {class: "talk"});
      run.log.forEach((en, i) => {
        if (en.t === "stage") path.append(h("div", {class: "small", style: "font-weight:700;margin-top:8px", text: stageOf(q, en.sid).title}));
        else if (en.t === "line") path.append(lineEl(q, stageOf(q, en.sid), en.nid, stageOf(q, en.sid).nodes[en.nid], "log" + i));
        else if (en.t === "you") path.append(h("div", {class: "you"}, rich(en.text)));
        else if (en.t === "fx") path.append(h("div", {class: "fx"}, en.sim ? h("span", {text: "Stand-in for doing it in the game:"}) : null, actChips(en.acts, q, false)));
      });
      left.append(h("details", {class: "path"}, h("summary", {text: `The path so far in this quest (${replies} ${replies === 1 ? "reply" : "replies"})`}), path));
    }
    treeStage = stageOf(q, run.view || run.stage);
  }
  body.append(h("div", {class: "play"}, left, statePanel(q)));
  if (run && treeStage) {
    const peek = !!run.view && run.view !== run.stage;
    body.append(h("div", {class: "maphead"},
      h("h3", {text: (peek ? "Looking at: " : "Where you are: ") + treeStage.title}),
      peek ? h("button", {class: "btn", text: "Back to where you are", onclick: () => { run.view = null; renderQuests(); }})
        : h("span", {class: "small", text: treeStage.nodes ? "The lit branch is the path you took. Replies with a brass outline are open to you now; click one to choose it." : ""})));
    if (treeStage.nodes) {
      const choose = (nid, i) => { if (run.stage === treeStage.id && run.node === nid && !run.over && !run.left) pick(q, treeStage.nodes[nid].options[i]); };
      body.append(treeView(q, treeStage, {run, pick: choose}).fig);
    } else body.append(h("p", {class: "small", text: "This stage has no conversation. " + treeStage.goal + "."}));
  }
}
function statePanel(q) {
  const st = game.st;
  const done = A.quests.filter(x => st.done[x.id]).map(x => h("div", {}, h("b", {text: x.title + ": "}), endingOf(x, st.done[x.id]).title));
  const flags = [...st.flags].filter(f => threads[f]).map(f => chip("gain", flagLabel(f), threads[f].what));
  const minor = [...st.flags].filter(f => !threads[f]).length;
  const items = Object.entries(st.items).filter(([, n]) => n > 0).map(([k, n]) => chip("", pretty(k) + (n > 1 ? " ×" + n : "")));
  const reps = Object.entries(st.rep).filter(([, n]) => n).sort((a, b) => b[1] - a[1]).map(([who, n]) =>
    h("div", {class: "bar"}, h("span", {text: people[who].name}),
      h("i", {}, h("b", {class: n < 0 ? "neg" : "", style: n < 0 ? `right:50%;width:${-n / 2}%` : `left:50%;width:${n / 2}%`})),
      h("span", {text: signed(n)})));
  const sect = (title, kids, empty) => h("div", {}, h("h4", {text: title}), kids.length ? h("div", {class: title === "Regard" ? "" : "chips"}, kids) : h("span", {class: "small", text: empty}));
  return h("aside", {class: "statebox", "aria-label": "Your story so far"},
    h("div", {}, h("h4", {text: "Your story so far"}), done.length ? done : h("span", {class: "small", text: "Nothing finished yet."})),
    sect("Threads you carry", flags, "None yet."),
    minor ? h("span", {class: "small", text: `and ${minor} smaller detail${minor > 1 ? "s" : ""} people will remember`}) : null,
    sect("Carrying", items, "Nothing."),
    sect("Regard", reps, "Nobody has an opinion of you yet."),
    h("div", {}, h("h4", {text: "Home and money"}), (st.home ? "Owns a home" : "No home of their own") + " \u00B7 " + bx(st.money),
      h("div", {class: "chips", style: "margin-top:6px"},
        st.home ? null : h("button", {class: "btn", text: "Buy a home", title: "Stand-in for the property market", onclick: () => { st.home = "owned"; renderQuests(); }}),
        h("button", {class: "btn", text: "+150 Bx", title: "Stand-in for earning and saving", onclick: () => { st.money += 15000; renderQuests(); }}))),
    h("button", {class: "btn", text: "Reset the whole story", onclick: () => { game.st = newState(); game.run = null; game.snap = {}; renderQuests(); }}));
}

/* ---------- cast ---------- */
const speaks = {};
for (const q of A.quests) for (const s of q.stages) for (const n of Object.values(s.nodes || {})) {
  if (n.who === "scene") continue;
  (speaks[n.who] = speaks[n.who] || {})[q.id] = ((speaks[n.who] || {})[q.id] || 0) + 1;
}
function renderCast() {
  const card = p => h("div", {class: "card person", id: "p-" + p.id},
    h("div", {class: "hd"}, ava(p.id),
      h("div", {}, h("h4", {}, p.name, p.age ? h("span", {class: "small", text: ", " + p.age}) : null,
        h("span", {class: "tag" + (p.status === "new" ? " new" : ""), text: p.status === "new" ? "new" : "in the slice"})),
        h("div", {class: "job", text: p.career}))),
    h("p", {class: "quote", text: "“" + p.quote + "”"}),
    p.story.map(t => h("p", {text: t})),
    h("div", {class: "kv"}, h("b", {text: "How they talk: "}), p.voice),
    p.wants ? h("div", {class: "kv"}, h("b", {text: "Wants: "}), p.wants) : null,
    p.hides ? h("div", {class: "kv"}, h("b", {text: "Hides: "}), p.hides) : null,
    h("div", {class: "small", text: p.place}),
    h("div", {class: "ties"}, p.ties.map(([to, , label]) => h("span", {}, h("a", {href: "#cast", text: people[to].name, onclick: ev => { ev.preventDefault(); openPerson(to); }}), ": " + label))),
    h("div", {class: "chips"}, h("span", {class: "small", text: "Speaks in:"}),
      A.quests.filter(q => (speaks[p.id] || {})[q.id]).map(q => h("button", {class: "chip goto", text: q.title, onclick: () => openQuest(q.id, "read")}))));
  const fresh = A.cast.people.filter(p => p.status === "new"), old = A.cast.people.filter(p => p.status !== "new");
  $("v-cast").replaceChildren(
    h("h2", {text: "Cast"}),
    h("p", {class: "lead", text: "Seven new residents, each with a trade, a want and something they are hiding. Then the six people already in the slice, with what this career adds to them."}),
    h("h3", {text: "New for the Baker"}), h("div", {class: "grid g2"}, fresh.map(card)),
    h("h3", {text: "Already in the slice"}), h("div", {class: "grid g2"}, old.map(card)));
}

/* ---------- connections ---------- */
const EDGE = {family: ["var(--m-happy)", ""], rift: ["var(--m-angry)", "7 5"], secret: ["var(--m-surprised)", "2 5"], friend: ["var(--m-sad)", ""],
  work: ["var(--soft)", ""], trade: ["var(--line)", ""], quest: ["var(--m-thinking)", "7 5"]};
const EDGE_NAME = {family: "Family", rift: "Estranged", secret: "Secret", friend: "Friends", work: "Work", trade: "Trade", quest: "Tied by a quest"};
function svg(tag, attrs, ...kids) {
  const e = document.createElementNS("http://www.w3.org/2000/svg", tag);
  for (const [k, v] of Object.entries(attrs || {})) if (k === "text") e.textContent = v; else if (k.startsWith("on")) e.addEventListener(k.slice(2), v); else e.setAttribute(k, v);
  e.append(...kids.flat());
  return e;
}
function renderWeb() {
  const pos = A.cast.map, pairs = new Map();
  for (const p of A.cast.people) for (const [to, kind, label] of p.ties) {
    const key = [p.id, to].sort().join("|");
    if (!pairs.has(key)) pairs.set(key, {key, a: p.id, b: to, kind, said: []});
    pairs.get(key).said.push(`${p.name}: ${label}`);
  }
  const edges = [], labels = [], nodes = [];
  for (const e of pairs.values()) {
    const [x1, y1] = pos[e.a], [x2, y2] = pos[e.b], [col, dash] = EDGE[e.kind], short = A.cast.edge_labels[e.key];
    edges.push(svg("line", {x1, y1, x2, y2, style: `stroke:${col}`, "stroke-width": e.kind === "trade" ? 2 : 2.5, "stroke-dasharray": dash, "stroke-linecap": "round"},
      svg("title", {text: e.said.join(" / ")})));
    if (short) labels.push(svg("text", {x: (x1 + x2) / 2, y: (y1 + y2) / 2 - 6, "text-anchor": "middle", "font-size": 11.5,
      style: "fill:var(--soft);stroke:var(--panel);stroke-width:5px;paint-order:stroke", text: short}));
  }
  const tieRows = [...pairs.values()].map(e => h("tr", {},
    h("td", {style: "white-space:nowrap"}, h("b", {text: `${people[e.a].name} and ${people[e.b].name}`})),
    h("td", {}, h("span", {class: "chip", text: EDGE_NAME[e.kind]})),
    h("td", {class: "small", text: e.said.join("  ·  ")})));
  for (const p of A.cast.people) {
    const [x, y] = pos[p.id];
    nodes.push(svg("g", {style: "cursor:pointer", onclick: () => openPerson(p.id), role: "link", "aria-label": p.name},
      p.status === "new" ? svg("circle", {cx: x, cy: y, r: 27, fill: "none", style: "stroke:var(--brass)", "stroke-width": 2}) : [],
      svg("circle", {cx: x, cy: y, r: 22, fill: p.color}),
      svg("text", {x, y: y + 5, "text-anchor": "middle", "font-size": 15, "font-weight": 750, fill: "#fff", text: p.name[0]}),
      svg("text", {x, y: y + 44, "text-anchor": "middle", "font-size": 13.5, "font-weight": 700, style: "fill:var(--ink);stroke:var(--panel);stroke-width:4px;paint-order:stroke", text: p.name})));
  }
  const cols = [["Before", live.map(q => q.id)], ...baker.map(q => [q.act, [q.id]]), ["Courting", romance.map(q => q.id)]];
  const rows = A.cast.people.map(p => h("tr", {},
    h("td", {}, h("a", {href: "#cast", text: p.name, onclick: ev => { ev.preventDefault(); openPerson(p.id); }}), p.status === "new" ? h("span", {class: "tag new", text: "new"}) : null),
    cols.map(([, ids]) => { const n = ids.reduce((sum, id) => sum + ((speaks[p.id] || {})[id] || 0), 0); return h("td", {class: "c", style: "font-variant-numeric:tabular-nums", text: n || ""}); })));
  $("v-web").replaceChildren(
    h("h2", {text: "Connections"}),
    h("p", {class: "lead", text: "Who is tied to whom. A brass ring marks the seven new residents. Select anyone to open their card."}),
    h("div", {class: "legend"}, Object.keys(EDGE).map(k => h("span", {}, h("i", {style: `border-top-color:${EDGE[k][0]};border-top-style:${EDGE[k][1] ? (k === "secret" ? "dotted" : "dashed") : "solid"}`}), EDGE_NAME[k]))),
    h("div", {class: "figure"}, svg("svg", {viewBox: "0 0 940 670", role: "img", "aria-label": "Relationship map of the cast"}, edges, labels, nodes)),
    h("h3", {text: "Every tie, in words"}),
    h("div", {class: "tablewrap"}, h("table", {}, h("tbody", {}, tieRows))),
    h("h3", {text: "Who speaks where"}),
    h("p", {class: "lead", text: "Lines spoken by each person in the starter quests and in each of the Baker's five quests."}),
    h("div", {class: "tablewrap"}, h("table", {}, h("thead", {}, h("tr", {}, h("th", {text: "Person"}), cols.map(([name]) => h("th", {class: "c", text: name})))), h("tbody", {}, rows))));
}

/* ---------- consequences ---------- */
function threadPlaces(t) {
  const out = [], isDone = t.id.startsWith("done:");
  const [, dq, dends] = isDone ? t.id.split(":") : [];
  const reads = need => {
    const raw = need.replace(/^!/, "");
    if (!isDone) return raw === "flag:" + t.id;
    if (!raw.startsWith(`done:${dq}:`)) return false;
    const ends = raw.split(":")[2];
    return dends === "*" || (ends !== "*" && ends.split("|").some(e => dends.split("|").includes(e)));
  };
  if (isDone) out.push({q: quests[dq], type: "set", text: dends === "*" ? "Finishing the quest line" : "Ending: " + dends.split("|").map(e => endingOf(quests[dq], e).title).join(" or ")});
  for (const q of A.quests) {
    if ((q.needs || []).some(reads)) out.push({q, type: "read", text: "Needed to start the quest line"});
    for (const s of q.stages) {
      for (const alt of s.starts || []) if (alt.need.some(reads)) out.push({q, s, nid: alt.node, type: "read", text: `${s.title}: the stage opens on a different line`});
      for (const [nid, n] of Object.entries(s.nodes || {})) for (const o of n.options) {
        if ((o.need || []).some(reads)) out.push({q, s, nid, type: "read", text: `${s.title}: ${(o.need || []).some(x => x.startsWith("!") && reads(x)) ? "closes" : "opens"} the reply “${o.text}”`});
        if (!isDone && (o.act || []).includes("flag:" + t.id)) out.push({q, s, nid, type: "set", text: `${s.title}: “${o.text}”`});
      }
    }
    if (!isDone) for (const e of q.endings) if ((e.flags || []).includes(t.id)) out.push({q, type: "set", text: "Ending: " + e.title});
  }
  return out;
}
function renderConsequences() {
  const cols = [["Before", live.map(q => q.id)], ...baker.map(q => [q.act, [q.id]]), ["Courting", romance.map(q => q.id)]];
  const detail = h("div", {class: "where card", hidden: true});
  const showWhere = (t, places) => {
    detail.hidden = false;
    detail.replaceChildren(h("b", {text: t.label}), h("span", {class: "small", text: "  " + t.what}),
      h("ul", {class: "plain"}, places.map(p => h("li", {},
        chip(p.type === "set" ? "end" : "goto", p.type === "set" ? "set" : "read"), " ",
        h("a", {href: "#quests", text: p.q.title, onclick: ev => { ev.preventDefault(); openQuest(p.q.id, "tree"); if (p.s && p.nid) setTimeout(() => scrollFlash($(`t-${p.q.id}-${p.s.id}-${p.nid}`)), 80); }}),
        " · " + p.text))));
    detail.scrollIntoView({block: "nearest", behavior: "smooth"});
  };
  const rows = A.world.threads.map(t => {
    const places = threadPlaces(t);
    return h("tr", {}, h("td", {}, h("b", {text: t.label}), h("div", {class: "small", text: t.what})),
      cols.map(([, ids]) => {
        const here = places.filter(p => ids.includes(p.q.id)), set = here.some(p => p.type === "set"), read = here.some(p => p.type === "read");
        if (!set && !read) return h("td", {class: "c"});
        const what = set && read ? "both" : set ? "set" : "read";
        return h("td", {class: "c"}, h("button", {class: "cellbtn", title: what === "both" ? "set and read here" : what + " here", "aria-label": `${t.label}: ${what} here`, onclick: () => showWhere(t, here)}, h("span", {class: "dot " + what})));
      }));
  });
  $("v-consequences").replaceChildren(
    h("h2", {text: "Consequences"}),
    h("p", {class: "lead", text: "Each row is something the game remembers. A filled token marks where a choice sets it; a ring marks where a later line, reply or ending depends on it. Select a mark to see the exact places."}),
    h("div", {class: "legend"}, h("span", {}, h("span", {class: "dot set"}), "set here"), h("span", {}, h("span", {class: "dot read"}), "read here"), h("span", {}, h("span", {class: "dot both"}), "both")),
    h("div", {class: "tablewrap"}, h("table", {}, h("thead", {}, h("tr", {}, h("th", {text: "Thread"}), cols.map(([name]) => h("th", {class: "c", text: name})))), h("tbody", {}, rows))),
    detail,
    h("h2", {style: "margin-top:44px", text: "The big decisions"}),
    h("p", {class: "lead", text: "The choices that shape the career and the courting, with what each answer does at once and what it does later."}),
    A.world.decisions.map(d => h("div", {class: "decision"},
      h("div", {class: "wh"}, d.where),
      h("h3", {}, d.title),
      h("p", {class: "ask", text: d.ask}),
      h("div", {class: "grid " + (d.options.length >= 3 ? "g3" : "g2")}, d.options.map(o => h("div", {class: "card opt"},
        h("h4", {text: o.pick}), h("p", {class: "now", text: o.now}), h("ul", {}, o.later.map(l => h("li", {text: l})))))),
      h("p", {style: "margin-top:10px"}, h("button", {class: "btn", text: "Read this quest", onclick: () => openQuest(d.quest, "read")}))))
  );
}

/* ---------- lore, careers ahead ---------- */
function renderLore() {
  $("v-lore").replaceChildren(
    h("h2", {text: "Lore"}),
    h("p", {class: "lead", text: "The history the career stands on. It is written to fit what is already on the map: the street names, the taken-down wall, Mill Fields, Brook Farm, the fountain."}),
    h("div", {class: "lore"}, A.world.lore.map(l => h("article", {}, h("h3", {text: l.title}), l.text.map(t => h("p", {text: t})), h("div", {class: "used", text: "Where it is used: " + l.used})))));
}
function renderCareers() {
  $("v-careers").replaceChildren(
    h("h2", {text: "Careers ahead"}),
    h("p", {class: "lead", text: "Six more careers, sketched only. Each would get the same treatment as the Baker: five quests, a rank for each, and the same town. The cast is shared on purpose, so a choice made as a baker can be remembered when you carry the post."}),
    h("div", {class: "grid g2 ahead"}, A.world.careers_ahead.map(c => h("div", {class: "card"},
      h("h4", {text: c.title}), h("div", {class: "ln", text: c.line}), h("p", {text: c.premise}),
      h("div", {class: "chips"}, h("span", {class: "small", text: "Shares:"}),
        c.shares.map(id => h("button", {class: "chip goto", text: people[id].name, onclick: () => openPerson(id)})))))));
}

/* ---------- start ---------- */
$("tabs").replaceChildren(...TABS.map(([id, name]) => h("button", {class: "tab", id: "t-" + id, role: "tab", text: name, onclick: () => show(id)})));
renderOverview(); renderQuests(); renderCast(); renderWeb(); renderConsequences(); renderLore(); renderCareers(); renderEndgame();
show(location.hash.slice(1) || "quests");
renderQuests();
if (document.fonts && document.fonts.ready) document.fonts.ready.then(relayoutTrees);
let resizeTimer = 0;
window.addEventListener("resize", () => { clearTimeout(resizeTimer); resizeTimer = setTimeout(relayoutTrees, 150); });
window.addEventListener("hashchange", () => show(location.hash.slice(1)));

/* ---------- after the wedding ---------- */
function renderEndgame() {
  $("v-careers").append(
    h("h2", {style: "margin-top:44px", text: "After the wedding: the end game"}),
    h("p", {class: "lead", text: "The wedding ends the quest line. What follows is open-ended play, not more quests. These four threads are sketched only; each is a feature to be picked and built in its own right."}),
    h("div", {class: "grid g2 ahead"}, A.world.endgame.map(e => h("div", {class: "card"}, h("h4", {text: e.title}), h("p", {text: e.text})))));
}
