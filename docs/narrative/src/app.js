"use strict";
const A = window.ATLAS;
const $ = id => document.getElementById(id);
const people = Object.fromEntries(A.cast.people.map(p => [p.id, p]));
const quests = Object.fromEntries(A.quests.map(q => [q.id, q]));
const threads = Object.fromEntries(A.world.threads.map(t => [t.id, t]));
const baker = A.quests.filter(q => q.group === "baker");
const live = A.quests.filter(q => q.group === "live");
const romance = A.quests.filter(q => q.group === "romance");
const TABS = [["quests", "Dialogue trees"], ["cast", "Cast"], ["web", "Connections"],
  ["consequences", "Consequences"], ["lore", "Lore"], ["careers", "Careers ahead"], ["overview", "About this draft"]];
const KIND = {talk: "Talk", near: "Be there", go: "Go there", moment: "A moment", have: "Have an item", bank: "At the bank",
  money: "Have the money", home: "Own a home", stall: "Rent a stall", stall_sold: "Sell from your stall"};
const ui = {quest: baker[0].id, mode: "tree", zoom: 1};

function h(tag, props, ...kids) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(props || {})) {
    if (v == null || v === false) continue;
    if (k === "class") e.className = v;
    else if (k === "text") e.textContent = v;
    else if (k === "style") e.style.cssText = v;
    else if (k.startsWith("on")) e.addEventListener(k.slice(2), v);
    else e.setAttribute(k, v === true ? "" : v);
  }
  for (const kid of kids.flat(3)) if (kid != null && kid !== false) e.append(kid.nodeType ? kid : document.createTextNode(kid));
  return e;
}
const pretty = s => s.replace(/_/g, " ");
const bx = chips => (chips % 100 ? (chips / 100).toFixed(2) : String(chips / 100)) + " Bx";
const signed = n => (n > 0 ? "+" : "−") + Math.abs(n);
const flagLabel = f => (threads[f] ? threads[f].label : pretty(f));
const endingOf = (q, id) => q.endings.find(e => e.id === id);
const stageOf = (q, id) => q.stages.find(s => s.id === id);

function rich(str) {
  return str.split(/(\{\w+\})/).map(part => /^\{\w+\}$/.test(part) ? h("span", {class: "var", text: part}) : part);
}
function ava(id) {
  const p = people[id];
  return h("span", {class: "ava", style: `background:${p ? p.color : "var(--m-neutral)"}`, text: p ? p.name[0] : "?", "aria-hidden": "true"});
}
function chip(cls, text, title) { return h("span", {class: "chip " + cls, text, title}); }

function needText(need) {
  const neg = need.startsWith("!"), raw = neg ? need.slice(1) : need;
  const i = raw.indexOf(":"), kind = raw.slice(0, i), rest = raw.slice(i + 1);
  if (kind === "flag") return (neg ? "unless: " : "if: ") + flagLabel(rest);
  if (kind === "have") return (neg ? "not carrying: " : "carrying: ") + pretty(rest);
  if (kind === "money") return "has " + bx(+rest);
  if (kind === "home") return neg ? "has no home of their own" : "owns a home";
  if (kind === "again") return neg ? "first time of asking" : "taken before";
  if (kind === "rep") { const [who, n] = rest.split(":"); return `${people[who].name}'s regard ${n}+`; }
  if (kind === "done") {
    const [qid, ends] = rest.split(":"), q = quests[qid];
    const which = ends === "*" ? "finished" : ends.split("|").map(e => endingOf(q, e).title).join(" or ");
    return (neg ? "unless " : "") + `${q.title}: ${which}`;
  }
  return need;
}
function needChips(needs) { return (needs || []).map(n => chip("need", needText(n), n)); }

function scrollFlash(el) {
  if (!el) return;
  for (let p = el.parentElement; p; p = p.parentElement) if (p.classList.contains("kids")) { p.hidden = false; }
  el.scrollIntoView({block: "center", behavior: "smooth"});
  el.classList.add("flash");
  setTimeout(() => el.classList.remove("flash"), 1600);
}
function actChips(acts, q, clickable) {
  return (acts || []).map(act => {
    const i = act.indexOf(":"), kind = act.slice(0, i), rest = act.slice(i + 1);
    if (kind === "flag") return chip("gain", "⚑ " + flagLabel(rest), "sets " + rest);
    if (kind === "unflag") return chip("lose", "clears: " + flagLabel(rest));
    if (kind === "give") return chip("gain", "+ " + pretty(rest));
    if (kind === "take") return chip("lose", "− " + pretty(rest));
    if (kind === "pay") return chip("gain", "+" + bx(+rest));
    if (kind === "charge") return chip("lose", "−" + bx(+rest));
    if (kind === "home") return chip("gain", "+ a home of your own");
    if (kind === "rep") { const [who, n] = rest.split(":"); return chip(+n >= 0 ? "gain" : "lose", `${people[who].name} ${signed(+n)}`); }
    if (kind === "stage") {
      const s = stageOf(q, rest), c = chip("goto", "→ " + s.title, "goes on to this stage");
      if (clickable) c.addEventListener("click", ev => { ev.stopPropagation(); scrollFlash($(`s-${q.id}-${rest}`)); });
      return c;
    }
    if (kind === "end") {
      const c = chip("end", "◆ " + endingOf(q, rest).title, "ends the quest line");
      if (clickable) { c.style.cursor = "pointer"; c.addEventListener("click", ev => { ev.stopPropagation(); scrollFlash($(`e-${q.id}-${rest}`)); }); }
      return c;
    }
    return chip("", act);
  });
}

/* ---------- tabs ---------- */
function show(tab) {
  if (!TABS.some(t => t[0] === tab)) tab = "quests";
  for (const [id] of TABS) {
    $("v-" + id).classList.toggle("on", id === tab);
    $("t-" + id).classList.toggle("on", id === tab);
    $("t-" + id).setAttribute("aria-selected", id === tab);
  }
  if (location.hash.slice(1) !== tab) { try { history.replaceState(null, "", "#" + tab); } catch (e) { location.hash = tab; } }
  window.scrollTo(0, 0);
  if (tab === "quests") relayoutTrees();
}
function openQuest(id, mode) {
  ui.quest = id;
  if (mode) ui.mode = mode === "read" ? "tree" : mode;
  show("quests");
  renderQuests();
}
function openPerson(id) {
  show("cast");
  scrollFlashCard($("p-" + id));
}
function scrollFlashCard(el) {
  if (!el) return;
  el.scrollIntoView({block: "start", behavior: "smooth"});
  el.style.borderColor = "var(--brass)";
  setTimeout(() => { el.style.borderColor = ""; }, 1800);
}

/* ---------- overview ---------- */
function renderOverview() {
  const c = A.world.career, s = A.stats, v = $("v-overview");
  v.replaceChildren(
    h("h2", {text: "Career one: " + c.title}),
    h("p", {class: "lead", text: c.pitch}),
    h("div", {class: "arc"}, baker.map(q => h("button", {onclick: () => openQuest(q.id, "read")},
      h("span", {class: "token", text: q.act}),
      h("span", {class: "rk", text: q.rank}),
      h("span", {class: "tt", text: q.title}),
      h("span", {class: "th", text: q.theme})))),
    h("h3", {text: "Courting and marriage"}),
    h("p", {class: "lead", text: A.world.courting.pitch}),
    h("div", {class: "card"}, h("dl", {class: "adds"}, A.world.courting.rules.map(a => [h("dt", {text: a.what}), h("dd", {text: a.why})])),
      h("div", {class: "chips", style: "margin-top:14px"},
        h("button", {class: "btn", text: "Mei: Twenty Minutes", onclick: () => openQuest("lunch_mei", "tree")}),
        h("button", {class: "btn", text: "The Wedding", onclick: () => openQuest("the_wedding", "tree")}))),
    h("div", {class: "grid g2", style: "margin-top:26px"},
      h("div", {class: "card"},
        h("h3", {style: "margin-top:0", text: "How to read this"}),
        h("ul", {class: "plain"},
          h("li", {}, h("b", {text: "Dialogue trees"}), " is the writing itself. Pick a quest on the left. The quest map shows its stages and where they branch; under it every conversation is drawn as a tree, read left to right. Script shows the same lines as plain indented text. Play lets you make the choices and watch the path light up."),
          h("li", {}, h("b", {text: "Cast"}), " has the seven new residents and the six from the slice that this career draws in."),
          h("li", {}, h("b", {text: "Connections"}), " shows who is tied to whom, and who speaks in which quest."),
          h("li", {}, h("b", {text: "Consequences"}), " follows each decision forward to where it lands, sometimes three quests later."),
          h("li", {}, "The four starter quests that are in the slice today are included unchanged, so you can see where the new career hooks into them."))),
      h("div", {class: "card"},
        h("h3", {style: "margin-top:0", text: "Where this stands"}),
        h("ul", {class: "plain"},
          h("li", {text: "Approved by the owner on 25 November 2026: the Baker career and courting Mei, as shown here."}),
          h("li", {text: "Decided along the way: no seasons, each player's own story, Jai at the museum, the town is Old Town, Noor is 58 and is not courted."}),
          h("li", {text: "Not built. Nothing here is in the game's data yet."}),
          h("li", {text: "Audited by other readers several times, with fixes after each. AUDIT.md beside this page lists every finding and what was done about it."}),
          h("li", {text: "Next: build this career into the game, or write the next one. Six are sketched under Careers ahead, sharing this cast."})))),
    h("h3", {text: "What it would need from the game"}),
    h("p", {class: "lead", text: "The dialogue is written in the slice's own quest format, so most of it drops straight into the quest data. These are the places it asks for more than the format gives today."}),
    h("div", {class: "card"}, h("dl", {class: "adds"}, A.world.additions.map(a => [h("dt", {text: a.what}), h("dd", {text: a.why})]))),
    h("p", {class: "small", style: "margin-top:16px", text:
      `Checked before building: every reply leads to a real line, every stage and ending can be reached, and ${s.play_throughs.toLocaleString()} random play-throughs of the whole career found no place where the player is left without a reply. Rewards are placeholders until balanced.`})
  );
  $("pills").replaceChildren(
    h("span", {class: "pill flagged", text: "Approved. Not in the game yet."}),
    h("span", {class: "pill"}, h("b", {text: s.quests}), " career quests"),
    h("span", {class: "pill"}, h("b", {text: s.courting_quests}), " courting quests"),
    h("span", {class: "pill"}, h("b", {text: s.lines}), " lines"),
    h("span", {class: "pill"}, h("b", {text: s.replies}), " replies"),
    h("span", {class: "pill"}, h("b", {text: s.endings}), " endings"),
    h("span", {class: "pill"}, h("b", {text: s.new_people}), " new residents"));
}

/* ---------- quest lines: read ---------- */
function needPill(n) {
  if (n.startsWith("done:")) return "After " + needText(n);
  if (n.startsWith("!flag:")) return "Closed once: " + flagLabel(n.slice(6));
  if (n.startsWith("flag:")) return "Needs: " + flagLabel(n.slice(5));
  return "Needs: " + needText(n);
}
function questButton(q) {
  return h("button", {class: "qb" + (q.id === ui.quest ? " on" : ""), onclick: () => openQuest(q.id)},
    h("span", {class: "token sm" + (q.group === "live" ? " plain" : ""), text: q.act}),
    h("span", {}, h("span", {class: "n", text: q.title}), h("br"), h("span", {class: "r", text: q.rank || "In the slice today"})));
}
function renderQuests() {
  const q = quests[ui.quest];
  const rail = h("div", {class: "qrail"},
    h("span", {class: "grp", text: "The Baker, proposed"}), baker.map(questButton), h("span", {class: "brk"}),
    h("span", {class: "grp", text: "Courting, proposed"}), romance.map(questButton), h("span", {class: "brk"}),
    h("span", {class: "grp", text: "Starter quests, in the slice"}), live.map(questButton));
  const head = h("div", {},
    h("div", {class: "qhead"},
      h("span", {class: "token" + (q.group === "live" ? " plain" : ""), text: q.act}),
      h("div", {style: "min-width:0"},
        h("h2", {text: q.title}),
        h("div", {class: "chips", style: "margin-top:6px"},
          q.rank ? h("span", {class: "pill flagged", text: "Rank: " + q.rank}) : h("span", {class: "pill", text: "In the slice today"}),
          h("span", {class: "pill"}, "From ", h("b", {text: people[q.giver].name})),
          (q.needs || []).map(n => h("span", {class: "pill", text: needPill(n)}))))),
    h("p", {class: "lead", style: "margin:14px 0 0", text: q.summary}),
    q.hook ? h("p", {class: "small", style: "margin:6px 0 0", text: "How it starts: " + q.hook}) : null,
    q.theme ? h("p", {class: "theme", text: q.theme}) : null);
  treeViews = [];
  if (ui.mode === "play") {
    if (!game.st) game.st = newState();
    if (game.run && game.run.qid !== q.id) game.run = null;
    if (!game.run && needsOk(q.needs, game.st) && !game.st.done[q.id]) beginRun(q);
  }
  const run = ui.mode === "play" && game.run && game.run.qid === q.id ? game.run : null;
  const seg = (id, label, tip) => h("button", {class: ui.mode === id ? "on" : "", text: label, title: tip, onclick: () => { ui.mode = id; renderQuests(); }});
  const zoomNow = () => (ui.zoom === "fit" ? 0.7 : ui.zoom);
  const zoom = [
    h("button", {class: "btn", text: "−", title: "Zoom out", "aria-label": "Zoom out", onclick: () => setZoom(Math.max(0.4, zoomNow() - 0.15))}),
    h("button", {class: "btn", text: "+", title: "Zoom in", "aria-label": "Zoom in", onclick: () => setZoom(Math.min(1.3, zoomNow() + 0.15))}),
    h("button", {class: "btn", text: "Fit to width", onclick: () => setZoom("fit")}),
    h("button", {class: "btn", text: "Full size", onclick: () => setZoom(1)})];
  const tools = {
    tree: [zoom, h("button", {class: "btn", text: "Fold branches", onclick: () => treeViews.forEach(v => v.foldAll(true))}),
      h("button", {class: "btn", text: "Unfold all", onclick: () => treeViews.forEach(v => v.foldAll(false))})],
    script: [h("button", {class: "btn", text: "Collapse all", onclick: () => setAll(true)}), h("button", {class: "btn", text: "Expand all", onclick: () => setAll(false)})],
    play: zoom};
  const hint = {
    tree: "Read left to right. A box is a line somebody says; each rounded box coming out of it is a reply you can choose, and its branch shows what that reply leads to.",
    script: "The same conversations as an indented script, for reading straight through.",
    play: "Choose a reply below, or click a highlighted reply in the tree. Brass marks the path you have taken; your choices carry on into the next quest."};
  const modebar = h("div", {class: "modebar"},
    h("div", {class: "seg", role: "group", "aria-label": "View"},
      seg("tree", "Tree", "Every conversation drawn as a branching tree"),
      seg("script", "Script", "The same lines as an indented script"),
      seg("play", "Play", "Walk through it and watch the path light up")),
    tools[ui.mode]);
  const toStage = sid => {
    if (run) { run.view = sid; renderQuests(); return; }
    const el = $(`s-${q.id}-${sid}`);
    if (el) el.scrollIntoView({block: "start", behavior: "smooth"});
  };
  const map = [
    h("div", {class: "maphead"}, h("h3", {text: "Quest map"}),
      h("span", {class: "small", text: run ? "Brass shows how far you have come and which branch you took." : "Each box is a stage. Where the arrows split, your choices decide the way. Select a stage to jump to its conversation."})),
    renderFlow(q, run, toStage, eid => { if (!run) scrollFlash($(`e-${q.id}-${eid}`)); })];
  const key = ui.mode === "script" ? null : h("div", {class: "keyrow"},
    h("span", {}, h("i", {class: "key-line"}), "a line somebody says"),
    h("span", {}, h("i", {class: "key-reply"}), "a reply you can choose"),
    h("span", {}, chip("need", "if: …"), "only offered when this is true"),
    h("span", {}, chip("gain", "⚑ …"), "what the reply changes"),
    h("span", {}, chip("goto", "→ …"), "moves to another stage"),
    h("span", {}, chip("end", "◆ …"), "ends the quest"));
  const body = h("div", {id: "qbody"});
  $("v-quests").replaceChildren(h("div", {class: "qlayout"}, rail, h("div", {style: "min-width:0"}, head, modebar,
    h("p", {class: "small", style: "margin:8px 0 0;max-width:80ch", text: hint[ui.mode]}), map, key, body)));
  if (ui.mode === "tree") renderTreeMode(q, body); else if (ui.mode === "script") renderRead(q, body); else renderPlay(q, body);
  relayoutTrees();
  const at = document.querySelector("#v-quests .fbox.now, #v-quests .fbox.end.done");
  if (at) at.closest(".flowfig").scrollLeft = Math.max(0, at.offsetLeft - 260);
}
function setAll(hidden) {
  document.querySelectorAll("#qbody .kids").forEach(k => { k.hidden = hidden; });
  document.querySelectorAll("#qbody .reply .car").forEach(c => { if (c.textContent.trim()) c.textContent = hidden ? "▸" : "▾"; });
}
function lineEl(q, s, id, node, prefix) {
  const domId = `${prefix || "n"}-${q.id}-${s.id}-${id}`;
  if (node.who === "scene") return h("div", {class: "narr", id: domId}, rich(node.say));
  const p = people[node.who];
  return h("div", {class: "line", id: domId, style: `border-left-color:var(--m-${node.mood})`},
    h("div", {class: "who"}, ava(node.who), h("span", {class: "nm", text: p.name}),
      h("span", {class: "mood", style: `color:var(--m-${node.mood})`, text: node.mood}), h("span", {class: "nid", text: id})),
    node.dir ? h("div", {class: "dir", text: node.dir}) : null,
    h("div", {class: "say"}, rich(node.say)));
}
function firstWords(node) {
  const t = node.say.replace(/\{\w+\}/g, "").trim();
  return t.length > 46 ? t.slice(0, 44).trim() + "…" : t;
}
function renderNode(q, s, id, shown) {
  const node = s.nodes[id];
  if (shown.has(id)) {
    const who = node.who === "scene" ? "" : people[node.who].name + ": ";
    return h("button", {class: "jump", onclick: () => scrollFlash($(`n-${q.id}-${s.id}-${id}`))}, `↳ continues at “${who}${firstWords(node)}”`);
  }
  shown.add(id);
  const list = h("ul", {class: "replies"});
  for (const o of node.options) {
    const chips = [needChips(o.need), actChips(o.act, q, true)];
    const moves = (o.act || []).some(a => /^(stage|end):/.test(a));
    if (!o.next && !moves) chips.push(chip("", "leaves the talk", "the conversation ends; the stage stays open"));
    if (o.next) {
      const kids = h("div", {class: "kids"}, renderNode(q, s, o.next, shown));
      const car = h("span", {class: "car", text: "▾"});
      list.append(h("li", {}, h("button", {class: "reply", onclick: () => { kids.hidden = !kids.hidden; car.textContent = kids.hidden ? "▸" : "▾"; }},
        car, h("span", {class: "tx"}, rich(o.text)), chips), kids));
    } else {
      list.append(h("li", {}, h("div", {class: "reply leaf"}, h("span", {class: "car", text: ""}), h("span", {class: "tx"}, rich(o.text)), chips)));
    }
  }
  return h("div", {class: "node"}, lineEl(q, s, id, node), list);
}
function stageHead(q, s, i) {
  const kind = KIND[s.kind] + (s.with ? " with " + people[s.with].name : "");
  return [
    h("div", {class: "stagehead"}, h("span", {class: "no", text: `Stage ${i + 1} · ${kind}`}), h("h3", {text: s.title}),
      s.optional ? h("span", {class: "chip", text: "optional"}) : null),
    h("div", {class: "goal", text: "Goal: " + s.goal}),
    h("div", {class: "logline"}, h("b", {text: "Quest log"}), s.log),
    s.scene ? h("p", {class: "scene", text: s.scene}) : null];
}
function renderRead(q, body) {
  q.stages.forEach((s, i) => {
    const box = h("div", {class: "stage", id: `s-${q.id}-${s.id}`}, stageHead(q, s, i));
    if (s.nodes) {
      const shown = new Set(), tree = h("div", {class: "tree"});
      const entries = [...(s.starts || []), {need: null, node: s.start}];
      for (const en of entries) {
        if (entries.length > 1) tree.append(h("div", {class: "variant"}, en.need ? ["Opens here", needChips(en.need)] : "Otherwise opens here"));
        tree.append(renderNode(q, s, en.node, shown));
      }
      box.append(tree);
    } else {
      box.append(h("div", {class: "chips", style: "margin-top:6px"}, h("span", {class: "small", text: "When done:"}), actChips([s.then], q, true)));
    }
    body.append(box);
  });
  body.append(h("h3", {style: "margin-top:36px", text: "Endings"}), h("div", {class: "endgrid"}, q.endings.map(e => endingCard(q, e))));
}
function endingCard(q, e) {
  const rep = Object.entries(e.rep || {}).filter(([, n]) => n).map(([who, n]) => chip(n > 0 ? "gain" : "lose", `${people[who].name} ${signed(n)}`));
  return h("div", {class: "ending", id: `e-${q.id}-${e.id}`},
    h("h4", {text: e.title}),
    h("p", {class: "lg", text: e.log}),
    h("div", {class: "chips"},
      e.retake ? chip("", "can be taken again", "taken again, the quest starts clean; what this ending cost stays") : null,
      e.pay ? chip("gain", "+" + bx(e.pay)) : null, e.charge ? chip("lose", "−" + bx(e.charge)) : null, rep,
      (e.give || []).map(g => chip("gain", "+ " + pretty(g))), (e.take || []).map(g => chip("lose", "− " + pretty(g))),
      (e.flags || []).filter(f => threads[f]).map(f => chip("gain", "⚑ " + flagLabel(f)))),
    e.ripples && e.ripples.length ? h("ul", {}, e.ripples.map(r => h("li", {text: r}))) : null,
    e.unlocks ? h("div", {class: "un"}, h("b", {text: "Opens: "}), e.unlocks) : null);
}
