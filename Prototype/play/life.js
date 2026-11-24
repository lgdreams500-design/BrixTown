"use strict";
// The player's life in the slice (ADR-0012): the character designer at a new game (CHR-001, CHR-002, CHR-013), money
// and the bank (ECO-009, BANK-007), shop counters and the market stall (ECO-011, PROP-008), quests, jobs and reputation in
// conversation (QST-001 to QST-007), and the save (QST-008). The rules are in economy.js; this file is the screens and the
// hooks play.js and conversations.js call. Loaded before play.js; play.js's globals (pl, clock, LOTS, lotAt, resident...)
// are only touched once it has run (from Life.go, Life.frame and the panels).
var Life = (() => {
  const D = window.WASTED, X = D.life, DG = X.designer, EC = X.economy, QR = X.quests, SAVE_KEY = "wasted.save.v1";
  const el = id => document.getElementById(id), fmt = Econ.fmt;
  let remodels = {};                                     // the player's remodelled buildings: lot id -> design (BUILD-004), kept in the save
  let plans = {};                                        // the player's room plans: lot id -> storey -> walls (BUILD-021), kept in the save
  let furnishings = {};                                  // the player's arranged storeys: lot id -> storey -> layout (FURN-051), kept in the save
  let view = null;                                       // the building designer's camera while it is open (play.js designCam)
  let E = null, Q = null, M = null, look = null, transient = false, designing = false, panel = null, spin = 0, ready = false, lastSave = 0, toastT = 0, lastMoney = null, checkT = 0, feats = {};
  const visits = {}; for (const k in X.visits) visits[k] = X.visits[k];
  const staff = {}; for (const k in X.staff) staff[k] = X.staff[k];
  const resIdx = {}; D.residents.forEach((r, i) => resIdx[r.id] = i);

  function h(tag, attrs, ...kids) {                      // a small element builder
    const e = document.createElement(tag);
    for (const k in attrs || {}) { if (k === "on") for (const ev in attrs.on) e.addEventListener(ev, attrs.on[ev]); else if (k === "cls") e.className = attrs[k]; else if (k in e && k !== "list") e[k] = attrs[k]; else e.setAttribute(k, attrs[k]); }
    for (const c of kids.flat()) if (c !== null && c !== undefined && c !== false) e.appendChild(typeof c === "string" || typeof c === "number" ? document.createTextNode(String(c)) : c);
    return e;
  }
  function toast(text) { const t = el("toast"); t.textContent = text; t.style.opacity = 1; toastT = 4.5; }
  const hhmm = m => String(Math.floor(m / 60) % 24).padStart(2, "0") + ":" + String(Math.floor(m % 60)).padStart(2, "0");
  const lotName = n => LOTS[n] ? LOTS[n].name : "";

  // ---------------------------------------------------------------- game state
  function residentsForQuests() { return D.residents.map(r => ({ id: r.id, name: r.n, role: D.roles[r.r], home: LOTS[r.h] ? LOTS[r.h].id : null, work: r.w >= 0 ? LOTS[r.w].id : null })); }
  function fresh() {
    E = new Econ.Economy(EC, LOTS, visits, staff, X.goods);   // with what the mall's outlets sell (ECO-013)
    Q = new Econ.Quests(QR, E, residentsForQuests(), LOTS);
    Q.onNote = text => toast(text);
    M = new Estate.Market(X.estate, E, LOTS, Estate.residentsOf(D, LOTS));   // the real estate market (PROP-009 to PROP-021)
  }
  function newGame(v) {
    fresh(); look = v; clock.day = 0; outfits = []; belongings = []; waypoint = null; plans = {};
    E.advance(0, clock.m); Q.newGame(); M.start(); ready = true;
    if (new URLSearchParams(location.search).has("rich")) testRich();
    feats = {}; lastMoney = E.totalMoney(); Rewards.reset(E.L.tx.length); save();
  }
  // ?rich=1, for trying the building designer (a test switch, not a game rule): a new game starts with Bx 5,000 more and owning
  // the nearest house that can be remodelled, and you stand at its door. The money comes in through the ledger; the deed is given.
  function testRich() {
    E.L.post(E.day, E.minute, "Test money (?rich=1)", [["sys:start", -500000], ["p:cash", 500000]]);
    let best = null, bd = 1e18;
    for (const [n, ix] of M.byLot) {
      const l = LOTS[n]; if (!l.dsg || !l.drawn || l.kind !== "home" || l.floors < 2) continue;
      const d = (l.door[0] - pl.x) ** 2 + (l.door[1] - pl.z) ** 2; if (d < bd) { bd = d; best = n; }
    }
    if (best === null) return;
    const l = LOTS[best], out = DOOR_DIR[l.design.shell.side];
    for (const i of M.byLot.get(best)) M.owner[i] = "player";
    pl.x = l.door[0] + out[0] * 1.6; pl.z = l.door[1] + out[1] * 1.6; pl.y = groundBase(pl.x, pl.z); pl.yaw = cam.yaw = Math.atan2(-out[0], out[1]);
    toast("Test: you own " + l.name + " and have Bx 5,000 more. Press E at the door to remodel it.");
  }
  function readSave(k) { try { const s = localStorage.getItem(slotKey(k || slot)); return s ? JSON.parse(s) : null; } catch (e) { return null; } }   // a slot's save (UI-013)
  function save() {
    if (!ready || transient) return;                      // ?auto=1 (tests, screenshots) never overwrites a real save
    try { localStorage.setItem(slotKey(slot), JSON.stringify({ v: 1, look, pl: { x: pl.x, y: pl.y, z: pl.z, yaw: pl.yaw }, clock: { day: clock.day, m: clock.m }, econ: E.save(), quests: Q.save(), estate: M.save(), remodels, plans, furnishings, outfits, belongings, feats, at: Date.now() })); } catch (e) { }
    lastSave = performance.now();
  }
  function loadSave(s) {
    fresh(); E.load(s.econ); Q.load(s.quests); if (s.estate) M.load(s.estate); else M.start(); look = s.look;
    Object.assign(pl, s.pl); pl.pose = 0; pl.vy = 0; cam.yaw = pl.yaw; clock.day = s.clock.day; clock.m = s.clock.m;
    ready = true; lastMoney = E.totalMoney(); Rewards.reset(E.L.tx.length); feats = s.feats || {};
    remodels = s.remodels || {};
    for (const id of Object.keys(remodels)) { const l = LOTS.find(x => x.id === id); if (l && l.drawn) { l.dsg = remodels[id]; window.wasted.repaint(l, Builder.built(l.dsg)); } }
    outfits = s.outfits || []; belongings = s.belongings || []; waypoint = null;
    plans = s.plans || {};
    for (const id of Object.keys(plans)) { const l = LOTS.find(x => x.id === id); if (!l) continue; for (const f of Object.keys(plans[id])) window.wasted.furn.showPlan(l, +f, plans[id][f]); Storeys.resolid(l); }
    furnishings = s.furnishings || {};
    for (const id of Object.keys(furnishings)) { const l = LOTS.find(x => x.id === id); if (!l || !canFurnish(l)) continue;
      for (const f of Object.keys(furnishings[id])) { window.wasted.furn.showLayout(l, +f, furnishings[id][f]); window.wasted.furn.layoutWorld(l, +f); } }
  }
  function lookOk(v) {                                    // the same checks as world.look_problems()
    for (const k in DG.sliders) { const s = DG.sliders[k], x = v[k]; if (typeof x !== "number" || x < s.min - 1e-9 || x > s.max + 1e-9) return false; }
    for (const k of ["skin", "hair_color", "eyes"]) if (!Number.isInteger(v[k]) || v[k] < 0 || v[k] >= DG[k].length) return false;
    if (!DG.hair.includes(v.hair) || !(v.garment in DG.garment) || !(v.bottom_type in DG.bottom_type) || !(v.walk in DG.walk)) return false;
    for (const k of ["top", "bottom", "shoes"]) if (!(v[k] in DG.palette)) return false;
    for (const k of ["scarf", "cap", "apron"]) if (v[k] !== null && !(v[k] in DG.palette)) return false;
    return typeof v.name === "string" && v.name.trim().length > 0 && v.name.length <= DG.name.max;
  }
  function applyLook() { window.wasted.setLook(People.design(look, DG)); }

  // ---------------------------------------------------------------- the start: continue, or a new character (CHR-001)
  function go(begin) {
    const s = readSave();
    if (s && s.v === 1 && lookOk(s.look) && !el("start").dataset.asked) {
      el("start").dataset.asked = 1;                     // first press: offer to carry on
      const c = el("card"), bar = h("div", { cls: "startbar" },
        h("button", { id: "cont", type: "button", on: { click: () => { try { loadSave(s); } catch (e) { console.warn("The save could not be read; starting afresh.", e); return openDesigner(begin); } applyLook(); begin(); toast("Welcome back, " + look.name + "."); } } }, "Carry on as " + s.look.name),
        h("button", { id: "anew", type: "button", on: { click: () => openDesigner(begin) } }, "New character"));
      el("go").replaceWith(bar); el("cont").focus();
      return;
    }
    openDesigner(begin);
  }
  function openDesigner(begin) {
    designing = true; el("start").style.display = "none";
    look = Object.assign({}, DG.default); applyLook();
    pl.y = groundBase(pl.x, pl.z); spin = pl.yaw;            // start face to face
    buildDesigner(begin); el("designer").style.display = "flex";
  }
  function buildDesigner(begin) {
    const box = el("dz-body"); box.textContent = "";
    const set = (k, v) => { look[k] = v; applyLook(); refresh(); };
    const sw = (k, list, css) => h("div", { cls: "swatches" }, list.map((c, n) => h("button", { type: "button", cls: "sw" + (look[k] === n ? " on" : ""), title: k + " " + (n + 1), style: "background:" + css(c), on: { click: () => set(k, n) } })));
    const rgb = c => "rgb(" + c.map(v => Math.round(Math.pow(v, 1 / 2.2) * 255)).join(",") + ")";
    const pal = (k, none) => h("div", { cls: "swatches" }, (none ? [null] : []).concat(Object.keys(DG.palette)).map(p => h("button", { type: "button", cls: "sw" + (look[k] === p ? " on" : "") + (p === null ? " none" : ""), title: p || "none", style: p ? "background:" + rgb(DG.palette[p]) : "", on: { click: () => set(k, p) } }, p === null ? "×" : "")));
    const chips = (k, opts) => h("div", { cls: "chips" }, Object.entries(opts).map(([v, label]) => h("button", { type: "button", cls: "chip" + (look[k] === v ? " on" : ""), on: { click: () => set(k, v) } }, label)));
    const row = (label, ...kids) => h("div", { cls: "dzrow" }, h("div", { cls: "dzlabel" }, label), ...kids);
    const sliders = Object.entries(DG.sliders).map(([k, s]) => {
      const out = h("span", { cls: "dzval" }, s.unit ? look[k].toFixed(2) + " " + s.unit : "");
      return row(s.label, h("div", { cls: "dzslide" }, h("input", { type: "range", min: s.min, max: s.max, step: s.step, value: look[k], "aria-label": s.label,
        on: { input: e => { look[k] = +e.target.value; if (s.unit) out.textContent = look[k].toFixed(2) + " " + s.unit; applyLook(); } } }), out));
    });
    const hairNames = {}; for (const v of DG.hair) hairNames[v] = v[0].toUpperCase() + v.slice(1);
    const name = h("input", { type: "text", id: "dz-name", value: look.name, maxLength: DG.name.max, "aria-label": "Name", on: { input: e => { look.name = e.target.value; el("dz-go").disabled = !lookOk(look); } } });
    function refresh() { const y = box.scrollTop; buildDesigner(begin); el("dz-body").scrollTop = y; }
    box.append(
      row("Name", name),
      h("h3", {}, "Body"), ...sliders.slice(0, 2),
      row("Skin", sw("skin", DG.skin, rgb)),
      h("h3", {}, "Face"), ...sliders.slice(2),
      row("Eyes", sw("eyes", DG.eyes, rgb)),
      h("h3", {}, "Hair"), row("Style", chips("hair", hairNames)), row("Colour", sw("hair_color", DG.hair_color, rgb)),
      h("h3", {}, "Clothes"), row("Top", chips("garment", DG.garment)), row("", pal("top")),
      row("Bottom", chips("bottom_type", DG.bottom_type)), row("", pal("bottom")), row("Shoes", pal("shoes")),
      h("h3", {}, "Extras"), row("Scarf", pal("scarf", true)), row("Cap", pal("cap", true)), row("Apron", pal("apron", true)),
      row("", h("div", { cls: "chips" }, h("button", { type: "button", cls: "chip" + (look.bag ? " on" : ""), on: { click: () => set("bag", !look.bag) } }, DG.extras.bag),
        h("button", { type: "button", cls: "chip" + (look.tash ? " on" : ""), on: { click: () => set("tash", !look.tash) } }, DG.extras.tash))),
      h("h3", {}, "Walk"), row("", chips("walk", DG.walk)));
    el("dz-go").disabled = !lookOk(look);
    el("dz-go").onclick = () => { if (!lookOk(look)) return; designing = false; el("designer").style.display = "none"; look.name = look.name.trim(); newGame(look); applyLook(); begin(); toast("Welcome to " + D.city.name + ", " + look.name + "."); };
    el("dz-rand").onclick = () => {
      const r = Math.random, pk = a => a[Math.floor(r() * a.length)], P = Object.keys(DG.palette);
      for (const k in DG.sliders) { const s = DG.sliders[k]; look[k] = +(s.min + (s.max - s.min) * r()).toFixed(3); }
      Object.assign(look, { skin: Math.floor(r() * DG.skin.length), hair: pk(DG.hair), hair_color: Math.floor(r() * DG.hair_color.length), eyes: Math.floor(r() * DG.eyes.length),
        garment: pk(Object.keys(DG.garment)), top: pk(P), bottom_type: pk(Object.keys(DG.bottom_type)), bottom: pk(P), shoes: pk(P), scarf: r() < .4 ? pk(P) : null, cap: r() < .2 ? pk(P) : null,
        apron: null, bag: r() < .3, tash: r() < .15, walk: pk(Object.keys(DG.walk)) });
      applyLook(); refresh();
    };
  }
  function camera() {                                     // while designing: a close turn round the character, framed to the right of the panel
    const S = look ? look.height / 1.72 : 1, d = 2.7 * S, t = [pl.x, pl.y + .95 * S, pl.z], a = spin;
    cam.eye = [t[0] + Math.sin(a) * d, t[1] + .18, t[2] - Math.cos(a) * d];
    const W = innerWidth, pw = W > 760 ? el("designer").offsetWidth : 0, ndc = (pw + (W - pw) / 2) / W * 2 - 1;   // the middle of the screen the panel leaves free
    const ang = Math.atan(ndc * Math.tan(.45) * W / innerHeight), dx = t[0] - cam.eye[0], dz = t[2] - cam.eye[2], dy = t[1] - .05 - cam.eye[1];
    const yaw = Math.atan2(dx, -dz) - ang, n = Math.hypot(dx, dz), pitch = Math.atan2(dy, n), cp = Math.cos(pitch);
    cam.dir = [Math.sin(yaw) * cp, Math.sin(pitch), -Math.cos(yaw) * cp];
  }

  // ---------------------------------------------------------------- conversations (conversations.js calls these)
  const prefix = (qid, nodes) => { const out = {}; for (const id in nodes) { const n = nodes[id]; out["q:" + qid + ":" + id] = Object.assign({}, n, { options: n.options.map(o => Object.assign({}, o, { next: o.next === null ? null : "q:" + qid + ":" + o.next, _q: qid })) }); } return out; };
  function jobVars(o) {
    if (!o) return {};
    const K = QR.jobs.kinds[o.kind], dl = o.deadline !== undefined ? o.deadline : (clock.day * 1440 + clock.m + o.hours * 60);
    return { pay: fmt(o.pay), hours: String(o.hours), by: hhmm(dl % 1440), minutes: String(o.minutes || K.minutes || ""), item: o.item ? E.items[o.item].name.toLowerCase() : "",
      from: o.kind === "parcel" ? lotName(o.frm) : D.residents[o.giver].n, to: o.to !== undefined ? lotName(o.to) : "" };
  }
  function conversation(i, base) {
    if (!ready) return base;
    const rid = D.residents[i].id, talks = Q.talkingTo(rid);
    if (talks.length) {                                   // this resident is the one to see for a quest stage: that talk, nothing else
      const qid = talks[0], st = Q.stage(qid);
      return { title: QR.quests[qid].title, start: "q:" + qid + ":" + st.start, nodes: prefix(qid, st.nodes), vars: {} };
    }
    const conv = { title: base.title, start: base.start, nodes: Object.assign({}, base.nodes), vars: {} };
    const extra = [];
    for (const qid of Q.offers(rid)) {                    // quest lines this resident offers
      const O = QR.quests[qid].offer;
      Object.assign(conv.nodes, prefix(qid, O.nodes));
      extra.push({ text: O.line, next: "q:" + qid + ":" + O.start });
    }
    const J = QR.jobs, L = J.lines, role = D.roles[D.residents[i].r], hasKind = Object.values(J.kinds).some(k => k.roles.includes(role));
    if (Q.job && Q.job.kind === "shopping" && Q.job.giver === i) {       // bringing an elder their shopping
      conv.vars = jobVars(Q.job);
      conv.nodes["job:thanks"] = { say: L.thanks, mood: "happy", options: [{ text: "Glad to help.", next: null }] };
      extra.push({ text: L.hand_over, next: "job:thanks", need: ["have:" + Q.job.item], _job: "hand" });
    } else if (hasKind) {                                 // asking for work (QST-006)
      const o = Q.jobOffer(i, clock.day);
      if (o) {
        conv.vars = jobVars(o);
        conv.nodes["job:offer"] = Q.job ? { say: L.busy, mood: "thinking", options: [{ text: "Right, I'll finish that first.", next: null }] }
          : { say: J.kinds[o.kind].offer, mood: "thinking", options: [{ text: L.accept, next: null, _job: "take", _offer: o }, { text: L.decline, next: null }] };
      } else conv.nodes["job:offer"] = { say: L.none, mood: "neutral", options: [{ text: "Fair enough.", next: null }] };
      extra.push({ text: L.ask, next: "job:offer" });
    }
    if (extra.length) { const n = conv.nodes[conv.start]; conv.nodes[conv.start] = Object.assign({}, n, { options: extra.concat(n.options).slice(0, 9) }); }
    return conv;
  }
  const allowed = o => !ready || Q.needOk(o.need);
  function chose(i, o) {                                  // after a reply: its quest acts, a job taken, a counter opened
    if (!ready) return;
    const rid = D.residents[i].id;
    try {
      if (o._q) Q.act(o._q, (o.act || []).filter(a => a !== "shop" && a !== "bank"), rid);
      if (o._job === "take") Q.takeJob(o._offer);
      if (o._job === "hand") Q.handOver();
    } catch (e) { if (!(e instanceof Econ.Refused)) throw e; toast(e.message); }
    if ((o.act || []).includes("shop")) pendingCounter = { i };
    save();
  }
  let pendingCounter = null;
  function talkEnded() { if (pendingCounter) { const p = pendingCounter; pendingCounter = null; const r = resident(p.i, clock.m); const n = r && r.lot ? LOTS.indexOf(r.lot) : -1; counter(n >= 0 ? n : null); } }
  function vars() { return { player: look ? look.name : "you" }; }

  // ---------------------------------------------------------------- panels: B counter, I wallet, J quest log
  function lotHere() { if (pl.y > 6) return null; const l = lotAt(pl.x, pl.z, true); return l ? LOTS.indexOf(l) : null; }
  // The outlet of a mall the player stands in, on whichever floor (WLD-066, ECO-013): {n: the mall's lot, r: its room (outlets.js)}
  function outletHere() {
    if (typeof Outlets === "undefined") return null;
    const l = lotAt(pl.x, pl.z, true); if (!l || !l.outletRooms) return null;
    const r = Outlets.at(l, pl.x, pl.y, pl.z), n = LOTS.indexOf(l);
    return r && E.outlets[n] && E.outlets[n][r.id] ? { n, r } : null;
  }
  const mallHere = () => { const l = lotAt(pl.x, pl.z, true), n = l ? LOTS.indexOf(l) : -1; return n >= 0 && E.outlets[n] ? n : null; };
  function counter(n) {
    const oh = outletHere(), mall = n === undefined || n === null ? mallHere() : (E.outlets[n] ? n : null);
    if (oh && (mall === null || mall === oh.n)) return open("outlet", oh.n, oh.r.id);   // each outlet has its own counter
    if (mall !== null) return toast("Walk into one of " + lotName(mall) + "'s outlets to buy there: each has its own goods.");
    if (n === undefined || n === null) n = lotHere();
    if (n === null || n < 0) return toast("No counter here. Shops, cafés, the bank and the market square have one.");
    if (n === E.bankLot) return open("bank", n);
    if (LOTS[n] && M.office[LOTS[n].id]) return open("agent", n);
    if (n === E.marketLot) return open("stall", n);
    if (E.shopKind(n)) return open("shop", n);
    toast("No counter here. Shops, cafés, the bank and the market square have one.");
  }
  function open(kind, n, u) {
    if (panel && panel.kind === kind) return close();
    panel = { kind, n, msg: "" }; if (u) panel.u = u;       // u: the outlet of a mall (ECO-013)
    for (const k in keys) keys[k] = false; pl.moving = false;
    if (document.pointerLockElement && document.exitPointerLock) document.exitPointerLock();
    el("life").classList.toggle("side", kind === "remodel");   // the designer stands to one side, so the building shows
    el("life").classList.toggle("wide", kind === "shop" || kind === "outlet");   // a counter shows its goods as objects, the chosen one large beside them (UI-020)
    el("life").style.setProperty("--bm-panel", BM.wide.panel_px + "px");
    if (kind === "remodel") startView(LOTS[n]);
    el("life").style.display = "block"; Sound.act("talk_open"); draw();
  }
  function close() {
    if (!panel) return;
    if (panel.kind === "remodel" && panel.lot && panel.draft !== panel.lot.dsg) window.wasted.repaint(panel.lot, Builder.built(panel.lot.dsg));   // not paid for: as it was
    if (panel.kind === "remodel" && panel.lot) for (const f of Object.keys(panel.plan || {})) if (panel.plan[f] !== panel.plan0[f]) window.wasted.furn.showPlan(panel.lot, +f, (plans[panel.lot.id] || {})[f] || null);
    if (panel.kind === "remodel" && panel.lot) for (const f of Object.keys(panel.lay)) if (panel.lay[f] !== panel.lay0[f]) window.wasted.furn.showLayout(panel.lot, +f, (furnishings[panel.lot.id] || {})[f] || null);
    if (view) { view.leave = true; view.cut = null; } grab = null; document.body.classList.remove("designing"); document.getElementById("gl").style.cursor = "";
    const back = panel.back; panel = null; el("life").style.display = "none"; el("life").classList.remove("menu", "side", "wide"); BuyMenu.clear(); placeLive(); Sound.act("talk_close"); save();
    if (back) menu(back);                                 // the designer was opened from the menu: back to it
  }
  function act(fn) {                                      // a button: do it, say what happened, redraw
    try { const r = fn(); panel.msg = typeof r === "string" ? r : ""; Sound.act("talk_choose"); } catch (e) { if (!(e instanceof Econ.Refused)) throw e; panel.msg = "Refused: " + e.message + "."; }
    save(); draw();
  }
  const money = c => h("span", { cls: "money" }, fmt(c));
  function buyAt(P, g) {                                  // buy one thing at an outlet; a thing for the home becomes a belonging to place (ECO-014, FURN-064)
    const p = E.buyAt(P.n, P.u, g), G = E.good(g);
    if (G.goes === "home") { const R = Furnish.rules().pieces[G.piece], [w, d, hh] = R.size; belongings.push({ id: "b:mall:" + E.L.tx.length, kind: G.piece, name: R.name, w, d, h: hh, rug: !!R.rug, line: "plain" }); }
    return "Bought " + G.name.toLowerCase() + " for " + fmt(p) + (G.goes === "home" ? ". It is with your belongings (G), to place at home." : G.goes === "wear" ? ". It is in your wardrobe (U)." : ".");
  }
  const btn = (label, fn, dis, cls) => h("button", { type: "button", cls: "pbtn" + (cls ? " " + cls : ""), disabled: !!dis, on: { click: () => act(fn) } }, label);
  // Buy menus show what is for sale as objects (UI-020, UI-021; buymenu.js, World/rules/buy_menu.json). ware: one thing of a
  // counter, with whether it can be bought now (sold out, or dearer than the cash and current account together, as spend pays);
  // wares: the grid of them. A shop's own things are carried; what cannot be sold on and comes in a bag or a cup is to eat.
  const BM = X.buy_menu.rules;
  const ware = (id, name, price, goes, left, band, more) => Object.assign({ id, name, price, goes, left, band, state: left !== null && left < 1 ? "out" : price > funds() ? "short" : "ok", short: Math.max(0, price - funds()) }, more);
  const wares = (id, things, o) => BuyMenu.grid(Object.assign({ id, things, fmt, width: () => el("life-body").clientWidth, restock: hhmm(EC.stock.restock_minute), keys: true, redraw: draw }, o));
  const shopGoes = it => !E.items[it].sell && (BuyMenu.shows(it) || [])[1] !== BM.stand_in.goes.carry ? "eat" : "carry";
  const tbl = rows => h("table", { cls: "ptab" }, h("tbody", {}, rows.map(r => h("tr", {}, r.map(c => h("td", {}, c))))));
  function purseLine() { return h("p", { cls: "purse" }, "Cash ", money(E.L.balance("p:cash")), E.accounts.includes("current") ? [" · Current ", money(E.L.balance("p:current"))] : null, E.accounts.includes("savings") ? [" · Savings ", money(E.L.balance("p:savings"))] : null); }
  function statement(n) {
    const st = E.L.statement(Econ.PLAYER, n);
    const moved = no => E.L.tx[no][4].filter(l => Econ.PLAYER.includes(l[0]) && l[1] > 0).reduce((a, l) => a + l[1], 0);   // between your own accounts: how much moved
    return st.length ? tbl(st.map(([no, day, m, memo, ch]) => ["Day " + (day + 1) + " " + hhmm(m), memo, ch ? h("span", { cls: ch < 0 ? "neg" : "pos" }, (ch > 0 ? "+" : "") + fmt(ch)) : h("span", { cls: "dim" }, fmt(moved(no)))])) : h("p", { cls: "dim" }, "Nothing yet.");
  }
  function draw() {
    if (!panel) return;
    BuyMenu.clear();                                       // the pictures of the last drawing go with it (buymenu.js)
    const P = panel, host = el("life-body"), was = host.querySelector(".pb"), at = was && was.dataset.kind === P.kind ? was.scrollTop : 0; host.textContent = "";
    const body = P.menu ? h("div", { cls: "pb" + (ONE_COLUMN[P.kind] ? " one" : "") }) : host;   // the game menu draws its page inside its shell (UI-015)
    let title = "", sub = "";
    if (P.kind === "shop") {
      const k = E.shopKind(P.n), S = EC.shops[k]; title = lotName(P.n); sub = S.name + " · restocks at " + hhmm(EC.stock.restock_minute);
      const mine = Object.keys(E.inv).filter(it => E.offer(P.n, it) > 0), selling = P.side === "sell" && S.buys.length > 0, band = BM.bands[k];
      const side = (on, label, to) => h("button", { type: "button", cls: "pbtn" + (on ? "" : " off"), on: { click: () => { P.side = to; P.msg = ""; draw(); } } }, label);
      body.append(purseLine());                             // the goods as objects, the chosen one large beside them (UI-020); selling uses the same cells
      if (S.buys.length) body.append(h("div", { cls: "prow bm-tabs" }, side(!selling, "Buy", "buy"), side(selling, "Sell" + (mine.length ? " (" + mine.length + ")" : ""), "sell")));
      else body.append(h("h3", {}, "Buy"));
      if (!selling) body.append(wares("shop:" + P.n + ":buy", S.sells.map(it => ware(it, E.items[it].name, E.price(P.n, it), shopGoes(it), E.stock(P.n, it), band)),
        { buy: e => act(() => { const p = E.buy(P.n, e.id); return "Bought " + e.name.toLowerCase() + " for " + fmt(p) + "."; }) }));
      else body.append(wares("shop:" + P.n + ":sell", mine.map(it => ({ id: it, name: E.items[it].name, price: E.offer(P.n, it), goes: null, left: E.inv[it], state: "ok", band })),
        { verb: "Sell", stockLabel: "You have", priceLabel: "They pay", leftWords: n => "you have " + n, empty: "Nothing you carry is wanted here.", buy: e => act(() => { const p = E.sell(P.n, e.id); return "Sold for " + fmt(p) + "."; }) }));
      if (!S.buys.length) body.append(h("p", { cls: "dim" }, "Cafés do not buy anything."));
      const r = E.repWithLot(P.n); if (r) body.append(h("p", { cls: "dim" }, r > 0 ? "They know you here: prices are a little lower." : "They don't trust you here: prices are higher."));
    } else if (P.kind === "outlet") {                       // one outlet of a mall: its own goods, stock and prices (ECO-013, ECO-014)
      const U = E.outlets[P.n][P.u], S = E.outletSells(P.n, P.u), K = ((LOTS[P.n].plan || {}).outlets || {})[U.kind] || {};
      title = U.name; sub = (K.label || "Outlet") + " · " + lotName(P.n) + (U.level ? ", floor " + U.level : ", ground floor") + " · restocks at " + hhmm(EC.stock.restock_minute);
      body.append(purseLine(), h("h3", {}, "Buy"), wares("outlet:" + P.n + ":" + P.u, S.map(g => { const G = E.good(g), piece = G.goes === "home" ? Furnish.rules().pieces[G.piece] : null;   // (UI-020)
        return ware(g, G.name, E.outletPrice(P.n, P.u, g), G.goes, E.outletStock(P.n, P.u, g), K.sign in BM.colours ? K.sign : BM.bands.mall, { size: piece ? piece.size : null }); }), { buy: e => act(() => buyAt(P, e.id)) }));
      body.append(h("p", { cls: "dim" }, "Things for the home go to your belongings (G): place them from the Furnish tab of the building designer, in any home of yours. Clothes go to your wardrobe (U). Outlets do not buy things back; what you carry can be sold on your market stall."));
      const r = E.repWithLot(P.n); if (r) body.append(h("p", { cls: "dim" }, r > 0 ? "They know you here: prices are a little lower." : "They don't trust you here: prices are higher."));
    } else if (P.kind === "bank") {
      const B = EC.bank, C = B.loan; title = lotName(P.n); sub = "Accounts, savings and loans";
      body.append(purseLine());
      if (!E.accounts.includes("current")) body.append(h("p", {}, "Open a current account. New customers get a welcome deposit of " + fmt(EC.start.welcome_deposit) + "."), btn("Open a current account", () => { E.openAccount("current"); Q.check(ctx()); return "Your current account is open."; }));
      else {
        if (!E.accounts.includes("savings")) body.append(h("p", {}, "A savings account earns " + (B.accounts.savings.rate_bp / 100) + "% a day, paid at midnight."), btn("Open a savings account", () => { E.openAccount("savings"); return "Your savings account is open."; }));
        const accts = ["p:cash"].concat(E.accounts.map(a => "p:" + a)), name = a => a === "p:cash" ? "Cash" : B.accounts[a.slice(2)].name;
        const from = h("select", { "aria-label": "From" }, accts.map(a => h("option", { value: a }, name(a)))), to = h("select", { "aria-label": "To" }, accts.map(a => h("option", { value: a }, name(a))));
        to.value = accts[1]; const amt = h("input", { type: "number", min: "0.01", step: "0.01", value: "5.00", cls: "amt", "aria-label": "Amount in brix" });
        body.append(h("h3", {}, "Move money"), h("div", { cls: "prow" }, from, h("span", {}, "to"), to, h("span", {}, "Bx"), amt, btn("Move", () => { E.move(from.value, to.value, Math.round(+amt.value * 100)); return "Moved " + fmt(Math.round(+amt.value * 100)) + "."; })));
        const loan = E.loans.find(l => l.status === "active");
        body.append(h("h3", {}, "Loans"));
        if (loan) body.append(h("p", {}, "You owe ", money(E.loanOwed(loan)), ". Instalments of ", money(loan.plan[0]), " are taken from your current account each midnight" + (loan.missed ? "; " + loan.missed + " missed so far (" + fmt(C.late_fee) + " fee each; " + C.default_after_missed + " in a row and the bank repossesses your things)." : ".")),
          btn("Repay it all now", () => { E.repay(loan.id); return "Loan repaid."; }));
        else if (E.barred) body.append(h("p", { cls: "neg" }, "After your default the bank will not lend to you again."));
        else {
          const sizes = []; for (let a = C.min; a <= C.max; a += C.step) sizes.push(a);
          const size = h("select", { "aria-label": "Loan size" }, sizes.map(a => h("option", { value: a }, fmt(a)))), term = h("select", { "aria-label": "Term" }, C.terms.map(t => h("option", { value: t }, t + " days")));
          const terms = h("p", { cls: "dim" }); const show = () => { const [tot, plan] = E.loanTerms(+size.value, +term.value); terms.textContent = "You pay back " + fmt(tot) + " in all: " + plan.length + " daily instalments of about " + fmt(plan[0]) + ", from tomorrow midnight. Late fee " + fmt(C.late_fee) + "."; };
          size.onchange = term.onchange = show; show();
          body.append(h("div", { cls: "prow" }, "Borrow", size, "over", term, btn("Borrow", () => { E.takeLoan(+size.value, +term.value); Q.check(ctx()); return "The loan is in your current account."; })), terms);
        }
        const old = E.loans.filter(l => l.status !== "active"); if (old.length) body.append(h("p", { cls: "dim" }, old.map(l => "Loan " + l.id + ": " + l.status).join(" · ")));
      }
      body.append(h("h3", {}, "Statement"), statement(12));
    } else if (P.kind === "stall") {
      const S = EC.stall; title = lotName(P.n); sub = "Your market stall · residents shop here " + hhmm(S.open[0]) + " to " + hhmm(S.open[1]);
      body.append(purseLine());
      if (!E.stall) body.append(h("p", {}, "Rent a stall for today: " + fmt(S.rent) + ". Set out up to " + S.slots + " things at your own prices; what is left comes home at midnight."), btn("Rent a stall for today", () => { E.rentStall(); Q.check(ctx()); return "The stall is yours until midnight."; }));
      else {
        const st = E.stall;
        body.append(h("h3", {}, "On your stall"), st.slots.length ? tbl(st.slots.map((s, k) => [E.items[s.item].name, money(s.price), s.sold ? h("span", { cls: "pos" }, "sold") : btn("Take back", () => { E.takeBack(k); })])) : h("p", { cls: "dim" }, "Nothing yet."));
        const can = Object.keys(E.inv).filter(it => !E.items[it].quest && E.items[it].price > 0);
        if (can.length && st.slots.length < S.slots) {
          const pick = h("select", { "aria-label": "Item" }, can.map(it => h("option", { value: it }, E.items[it].name + " (" + E.inv[it] + ")")));
          const pr = h("input", { type: "number", min: "0.01", step: "0.01", cls: "amt", "aria-label": "Price in brix" }), hint = h("span", { cls: "dim" });
          const usual = () => { pr.value = (E.items[pick.value].price / 100).toFixed(2); hint.textContent = "usually " + fmt(E.items[pick.value].price); }; pick.onchange = usual; usual();
          body.append(h("div", { cls: "prow" }, "Set out", pick, "at Bx", pr, btn("Set out", () => { E.setOut(pick.value, Math.round(+pr.value * 100)); }), hint),
            h("p", { cls: "dim" }, "Cheaper than usual sells fast; dearer sells slowly."));
        } else if (!can.length) body.append(h("p", { cls: "dim" }, "You have nothing to sell. Buy cheap at a shop, sell dear here."));
      }
    } else if (P.kind === "wallet") {
      title = "Money"; sub = look.name + " · I";
      body.append(purseLine());
      const loan = E.loans.find(l => l.status === "active"); if (loan) body.append(h("p", {}, "Loan: ", money(E.loanOwed(loan)), " owed to Linden Bank."));
      body.append(h("h3", {}, "Statement"), statement(15));
    } else if (P.kind === "quests") {
      title = P.menu ? "Journal" : "Quests"; sub = P.menu ? "Quests and work under way, on offer and finished" : "Day " + (clock.day + 1);
      const going = Object.keys(Q.active);
      body.append(h("h3", {}, "Under way"));
      if (!going.length && !Q.job) body.append(h("p", { cls: "dim" }, "Nothing under way. Talk to people: some have things for you to do, and some have work (ask them)."));
      for (const qid of going) { const st = Q.stage(qid); body.append(h("div", { cls: "qline" }, h("b", {}, QR.quests[qid].title), h("p", {}, st.log), where(st.goal))); }
      if (Q.job) body.append(h("div", { cls: "qline" }, h("b", {}, QR.jobs.kinds[Q.job.kind].title), h("p", {}, fillJob(QR.jobs.kinds[Q.job.kind].log, Q.job) + (Q.job.kind === "shift" ? " (" + Math.floor(Q.job.worked) + " of " + Q.job.minutes + " minutes done)" : "") + (Q.job.kind === "parcel" ? (Q.job.step === "pickup" ? " Collect it first." : " You have the parcel.") : ""))));
      const offered = Object.keys(QR.quests).filter(qid => QR.quests[qid].giver && Q.offers(QR.quests[qid].giver).includes(qid));
      if (offered.length) body.append(h("h3", {}, "On offer"), ...offered.map(qid => { const q = QR.quests[qid]; return h("div", { cls: "qline" }, h("b", {}, q.title), h("p", {}, q.summary), where({ talk: q.giver })); }));
      const done = Object.keys(Q.done);
      if (done.length) body.append(h("h3", {}, "Finished"), ...done.map(qid => h("div", { cls: "qline done" }, h("b", {}, QR.quests[qid].title), h("p", {}, QR.quests[qid].endings[Q.done[qid]].log))));
      const reps = Object.entries(E.rep).filter(([, v]) => v).sort((a, b) => b[1] - a[1]);
      if (reps.length) body.append(h("h3", {}, "People you know"), tbl(reps.map(([rid, v]) => [D.residents[resIdx[rid]].n, DLR(rid), h("span", { cls: v > 0 ? "pos" : "neg" }, (v > 0 ? "+" : "") + v)])));
    }
    else if (P.kind === "agent" || P.kind === "property") { const t = estatePanel(P, body); title = t[0]; sub = t[1]; }
    else if (P.kind === "remodel") { const t = remodelPanel(P, body); title = t[0]; sub = t[1]; }
    else if ({ me: 1, belongings: 1, wardrobe: 1, storage: 1, settings: 1, save: 1, help: 1 }[P.kind]) { const t = { me: meTab, belongings: belongingsTab, wardrobe: wardrobeTab, storage: storageTab, settings: settingsTab, save: saveTab, help: helpTab }[P.kind](P, body); title = t[0]; sub = t[1]; }
    if (P.menu) cards(body);
    if (P.msg) body.prepend(h("p", { cls: "pmsg" }, P.msg));
    if (P.menu) { body.dataset.kind = P.kind; host.append(menuShell(P, body, title, sub.replace(/ · [A-Z]$/, ""))); body.scrollTop = at; }
    el("life-title").textContent = P.menu ? "" : title; el("life-sub").textContent = P.menu ? "" : sub;
    el("life-hint").innerHTML = (P.kind === "shop" || P.kind === "outlet" ? "<kbd>← → ↑ ↓</kbd>choose <kbd>Enter</kbd>" + (P.kind === "shop" && P.side === "sell" ? "sell" : "buy") + " " : "") + "<kbd>Esc</kbd>close";
    placeLive();
  }
  // ---------------------------------------------------------------- the game menu (UI-005 to UI-010, INV-001, CHR-014, CHR-015; docs/specs/domains/MENU.md)
  // Esc (or the browser letting the mouse go) opens it; I, G, P, J, U, H open a tab of it. Its tabs are this file's panels
  // with a row of tabs over them. While it is open the clock stands still (settings.json menu.pauses).
  const ST = X.settings, TABS = ST.menu.tabs, SETTINGS_KEY = "wasted.settings.v1";
  let settings = null, outfits = [], waypoint = null, belongings = [];
  function defaults() { const o = {}; for (const [g, G] of Object.entries(ST.groups)) { o[g] = {}; for (const [k, s] of Object.entries(G.settings)) o[g][k] = s.default; } return o; }
  function loadSettings() {                              // the device's settings over the defaults (not the save: they belong to the device)
    settings = defaults();
    try { const s = JSON.parse(localStorage.getItem(SETTINGS_KEY) || "null"); if (s) for (const g of Object.keys(settings)) Object.assign(settings[g], s[g] || {}); else if (localStorage.getItem("wasted.sound") === "off") settings.audio.sound = false; } catch (e) { }
    for (const [g, G] of Object.entries(ST.groups)) for (const [k, s] of Object.entries(G.settings)) if (!okSetting(s, settings[g][k])) settings[g][k] = s.default;
    window.wasted.applySettings(settings);
  }
  function okSetting(s, v) { return s.type === "switch" ? typeof v === "boolean" : s.type === "choice" ? v in s.options : typeof v === "number" && v >= s.min - 1e-9 && v <= s.max + 1e-9; }
  const PICTURE = { shadows: [true, false], ambient_occlusion: [true, false], bounce: [true, false], resolution: ["auto", "soft"] };   // better, simple
  function setting(g, k, v) {                            // change one setting (no value: a switch flips, a choice goes to the next)
    const s = ST.groups[g].settings[k], cur = settings[g][k];
    if (v === undefined) v = s.type === "switch" ? !cur : s.type === "choice" ? (o => o[(o.indexOf(cur) + 1) % o.length])(Object.keys(s.options)) : cur;
    if (!okSetting(s, v)) return; settings[g][k] = v;
    if (g === "graphics" && k === "picture") for (const [p, [b, sm]] of Object.entries(PICTURE)) settings.graphics[p] = v === "better" ? b : sm;   // the picture: all its parts at once (O)
    else if (g === "graphics" && k in PICTURE) { const parts = Object.entries(PICTURE); if (parts.every(([p, [b]]) => settings.graphics[p] === b)) settings.graphics.picture = "better"; else if (parts.every(([p, [, sm]]) => settings.graphics[p] === sm)) settings.graphics.picture = "simple"; }
    try { localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings)); } catch (e) { }
    window.wasted.applySettings(settings);
    if (!panel && (k === "picture" || k === "sound")) toast(k === "picture" ? (v === "better" ? "Better picture" : "Simple picture") : "Sound " + (v ? "on" : "off"));
    if (panel) draw();
  }
  function platform() {                                  // the platform being played on (UI-010): what the controls are shown for
    try { if (navigator.getGamepads && [...navigator.getGamepads()].some(Boolean)) return "gamepad"; } catch (e) { }
    return matchMedia && matchMedia("(pointer: coarse)").matches ? "touch" : "keyboard";
  }
  function menu(tab) {                                   // open the menu at a page, or at its home screen of boards (UI-015)
    tab = tab || "hub";
    if (tab === "map") { if (panel) close(); window.wasted.showMap(true); return; }   // the map is the town's own big map
    if (panel && panel.menu && panel.kind === tab) return close();
    if (panel && !panel.menu) close();
    if (panel && panel.menu) { panel = { kind: tab, n: null, msg: "", menu: true }; el("life").classList.toggle("side", tab === "wardrobe"); draw(); return; }
    open(tab, null); panel.menu = true; el("life").classList.add("menu"); el("life").classList.toggle("side", tab === "wardrobe"); draw();
  }
  // ---------------------------------------------------------------- the menu's look: Painted signs (ADR-0033, UI-015, UI-016)
  // The whole screen. Home is a board for each page, painted in its colour (settings.json menu.colours) with its headline on
  // it; a page keeps its board's colour in its header and its buttons, with every board small down the left to change page.
  const MC = ST.menu.colours, ONE_COLUMN = { property: 1, agent: 1, save: 1, settings: 1, belongings: 1, wardrobe: 1, storage: 1 };
  const MICON = {
    me: '<circle cx="12" cy="8" r="4"/><path d="M4 21c0-4.4 3.6-7 8-7s8 2.6 8 7"/>',
    wallet: '<ellipse cx="12" cy="7" rx="7" ry="3"/><path d="M5 7v5c0 1.7 3.1 3 7 3s7-1.3 7-3V7"/><path d="M5 12v5c0 1.7 3.1 3 7 3s7-1.3 7-3v-5"/>',
    belongings: '<path d="M5 8h14l-1 12H6L5 8z"/><path d="M9 8V6a3 3 0 0 1 6 0v2"/>',
    wardrobe: '<path d="M8 3 3 7l2 4 3-2v12h8V9l3 2 2-4-5-4c-.7 1.6-2.2 2.5-4 2.5S8.7 4.6 8 3z"/>',
    property: '<path d="M3 11l9-7 9 7"/><path d="M5 10v10h14V10"/><path d="M10 20v-6h4v6"/>',
    storage: '<rect x="3" y="8" width="18" height="12" rx="1.5"/><path d="M3 13h18M8 8V5h8v3M10.5 13v2.5h3V13"/>',
    quests: '<path d="M4 5a2 2 0 0 1 2-2h13v16H6a2 2 0 0 0-2 2V5z"/><path d="M4 19a2 2 0 0 1 2-2h13M9 8h6"/>',
    map: '<path d="M12 21s-7-6.2-7-11a7 7 0 0 1 14 0c0 4.8-7 11-7 11z"/><circle cx="12" cy="10" r="2.5"/>',
    settings: '<path d="M4 7h10M18 7h2M4 17h2M10 17h10"/><circle cx="16" cy="7" r="2"/><circle cx="8" cy="17" r="2"/>',
    save: '<path d="M5 3h11l3 3v15H5z"/><path d="M8 3v5h7V3M8 21v-7h8v7"/>',
    help: '<circle cx="12" cy="12" r="9"/><path d="M9.5 9.5a2.5 2.5 0 1 1 3.5 2.3c-.7.4-1 1-1 1.7"/><path d="M12 17h.01"/>',
    hub: '<rect x="4" y="4" width="6.5" height="6.5" rx="1.5"/><rect x="13.5" y="4" width="6.5" height="6.5" rx="1.5"/><rect x="4" y="13.5" width="6.5" height="6.5" rx="1.5"/><rect x="13.5" y="13.5" width="6.5" height="6.5" rx="1.5"/>'
  };
  const micon = (id, size) => { const e = h("span", { cls: "mic" }); e.innerHTML = '<svg viewBox="0 0 24 24" width="' + size + '" height="' + size + '" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">' + MICON[id] + "</svg>"; return e; };
  (() => {                                               // the town's mottled paint (ART_DIRECTION W1): two close tones blotched together, drawn once
    const url = (seed, col, a) => 'url("data:image/svg+xml,' + encodeURIComponent("<svg xmlns='http://www.w3.org/2000/svg' width='700' height='700'><filter id='n' x='0' y='0' width='100%' height='100%'><feTurbulence type='fractalNoise' baseFrequency='.011 .016' numOctaves='2' seed='" + seed + "' stitchTiles='stitch'/><feColorMatrix values='0 0 0 0 " + col + " 0 0 0 0 " + col + " 0 0 0 0 " + col + " " + 4 * a + " 0 0 0 " + -1.7 * a + "'/></filter><rect width='100%' height='100%' filter='url(#n)'/></svg>") + '")';
    document.documentElement.style.setProperty("--mottle-d", url(3, 0, .16));
    document.documentElement.style.setProperty("--mottle-t", url(3, 0, .09) + ", " + url(8, 1, .05));
  })();
  const css3 = c => "rgb(" + c.map(v => Math.round(Math.pow(v, 1 / 2.2) * 255)).join(",") + ")";
  function faceOf(v, size) {                             // the player's face, drawn flat from their own colours and hair (the 3D head is the real one)
    const skin = css3(DG.skin[v.skin]), hair = css3(DG.hair_color[v.hair_color]), eye = css3(DG.eyes[v.eyes]), top = css3(DG.palette[v.top]);
    const behind = { long: '<path d="M27 62c-4-30 10-48 33-48s37 18 33 48l4 46H23z"/>', bob: '<path d="M27 60c-4-30 10-46 33-46s37 16 33 46l1 26H26z"/>', afro: '<circle cx="60" cy="46" r="41"/>', bun: '<circle cx="60" cy="15" r="12"/>' }[v.hair] || "";
    const fringe = v.hair === "bald" ? "" : v.hair === "afro" || v.hair === "short" ? '<path d="M31 54c0-22 12-34 29-34s29 12 29 34c-8-7-18-11-29-11s-21 4-29 11z"/>' : '<path d="M31 56c0-24 12-36 29-36s29 12 29 34c-9-2-17-8-22-17-6 10-20 17-36 19z"/>';
    const e = h("span", { cls: "mface" });
    e.innerHTML = '<svg viewBox="0 0 120 120" width="' + size + '" height="' + size + '" role="img" aria-label="' + v.name.replace(/[<>&"]/g, "") + '"><rect width="120" height="120" fill="#cfc3ab"/>'
      + '<g fill="' + hair + '">' + behind + '</g><path d="M18 120c2-20 16-28 42-28s40 8 42 28z" fill="' + top + '"/>'
      + (v.scarf ? '<path d="M40 96c6-6 34-6 40 0l-6 12c-8 4-20 4-28 0z" fill="' + css3(DG.palette[v.scarf]) + '"/>' : "")
      + '<rect x="51" y="78" width="18" height="18" rx="7" fill="' + skin + '" style="filter:brightness(.93)"/><ellipse cx="60" cy="58" rx="27" ry="29" fill="' + skin + '"/><g fill="' + hair + '">' + fringe + '</g>'
      + (v.cap ? '<path d="M29 46c2-20 14-29 31-29s29 9 31 29z" fill="' + css3(DG.palette[v.cap]) + '"/><path d="M27 46h66v5H27z" fill="' + css3(DG.palette[v.cap]) + '" style="filter:brightness(.85)"/>' : "")
      + '<ellipse cx="41" cy="71" rx="7" ry="4.5" fill="#e0705c" opacity=".4"/><ellipse cx="79" cy="71" rx="7" ry="4.5" fill="#e0705c" opacity=".4"/>'
      + '<ellipse cx="48" cy="60" rx="6.5" ry="7.5" fill="#fbf6ec"/><ellipse cx="72" cy="60" rx="6.5" ry="7.5" fill="#fbf6ec"/><circle cx="48.5" cy="61" r="4.6" fill="' + eye + '"/><circle cx="71.5" cy="61" r="4.6" fill="' + eye + '"/>'
      + '<circle cx="48.5" cy="61" r="2.3" fill="#1c1b21"/><circle cx="71.5" cy="61" r="2.3" fill="#1c1b21"/><circle cx="50" cy="59" r="1.2" fill="#fff"/><circle cx="73" cy="59" r="1.2" fill="#fff"/>'
      + '<path d="M41 49c4-3 9-3 13-1M66 48c4-2 9-2 13 1" stroke="' + hair + '" stroke-width="2.6" stroke-linecap="round" fill="none" style="filter:brightness(.8)"/>'
      + '<path d="M58 68c1 2 3 2 4 0" stroke="rgba(0,0,0,.22)" stroke-width="2" stroke-linecap="round" fill="none"/>'
      + (v.tash ? '<path d="M50 74c4-4 16-4 20 0" stroke="' + hair + '" stroke-width="4.5" stroke-linecap="round" fill="none"/>' : "")
      + '<path d="M52 78c5 4 11 4 16 0" stroke="rgba(120,40,30,.6)" stroke-width="2.6" stroke-linecap="round" fill="none"/></svg>';
    return e;
  }
  // The player themself in the menu (UI-017): not a picture but the game's own view. The camera stands in front of the player
  // (play.js stepCamera asks liveCam) and the game's picture is slid, and for the face made smaller, so that they show
  // through a window: a hole in the veil where the face goes, or the open side of the wardrobe. Sitting, lying or in a car
  // there is no such view, and the drawn face stands in.
  let live = null;
  function liveCam() {
    if (!live) return false;
    const S = look.height / 1.72, th = Math.tan(SET.fov / 2), f = [Math.sin(pl.yaw), 0, -Math.cos(pl.yaw)], face = live.mode === "face";
    const t = [pl.x, pl.y + (face ? LIVE.face_y : LIVE.body_y) * S, pl.z], d = face ? LIVE.face_d : LIVE.body_m * S / LIVE.body_fill / (2 * th);
    cam.eye = [t[0] + f[0] * d, t[1] + (face ? .02 : .12), t[2] + f[2] * d];
    const v = [t[0] - cam.eye[0], t[1] - cam.eye[1], t[2] - cam.eye[2]], n = Math.hypot(v[0], v[1], v[2]); cam.dir = v.map(x => x / n);
    return true;
  }
  const LIVE = ST.menu.live;
  function placeLive() {
    const gl = el("gl"), veil = el("veil"), P = panel, on = !!(P && P.menu), can = on && ready && pl.pose <= 1 && !pl.play, body = can && P.kind === "wardrobe";
    live = null; gl.style.transform = ""; document.body.classList.toggle("menuing", on);
    veil.style.display = on && P.kind !== "wardrobe" ? "block" : "none"; veil.style.maskImage = veil.style.webkitMaskImage = "none";
    if (body) {                                          // the wardrobe: all of you, in the middle of what the page leaves free
      const pg = el("life").querySelector(".mpage").getBoundingClientRect(), st = el("life").querySelector(".mstrip").getBoundingClientRect();
      live = { mode: "body" }; gl.style.transform = "translate(" + Math.round((st.right + pg.left) / 2 - innerWidth / 2) + "px, 0)"; return;
    }
    const win = can ? el("life").querySelector(".mlive") : null, r = win && win.getBoundingClientRect();
    if (!r || r.width < 8) return;
    win.classList.add("on");
    const S = look.height / 1.72, k = r.height / (LIVE.face_m * S) * 2 * LIVE.face_d * Math.tan(SET.fov / 2) / innerHeight, rad = getComputedStyle(win).borderTopLeftRadius;
    const rr = rad.endsWith("%") ? r.width * parseFloat(rad) / 100 : parseFloat(rad) || 0;
    live = { mode: "face" };
    gl.style.transform = "translate(" + (r.left + r.width / 2 - innerWidth / 2).toFixed(1) + "px, " + (r.top + r.height / 2 - innerHeight / 2).toFixed(1) + "px) scale(" + k.toFixed(4) + ")";
    const hole = 'url("data:image/svg+xml,' + encodeURIComponent("<svg xmlns='http://www.w3.org/2000/svg' width='" + r.width + "' height='" + r.height + "'><rect width='" + r.width + "' height='" + r.height + "' rx='" + rr + "'/></svg>") + '")';
    veil.style.maskImage = veil.style.webkitMaskImage = "linear-gradient(#000, #000), " + hole;
    veil.style.maskSize = veil.style.webkitMaskSize = "100% 100%, " + r.width + "px " + r.height + "px";
    veil.style.maskPosition = veil.style.webkitMaskPosition = "0 0, " + r.left + "px " + r.top + "px";
  }
  addEventListener("resize", () => { if (panel && panel.menu) draw(); });
  const liveFace = size => { const e = faceOf(look, size); e.classList.add("mlive"); return e; };
  function headline(id) {                                // what a board says: the page's headline, from the game as it stands
    const n = (k, w) => k + " " + w + (k === 1 ? "" : "s");
    if (id === "me") { const p = Object.entries(E.rep).filter(([, v]) => v > 0).sort((a, b) => b[1] - a[1]).slice(0, 2); return ["Day " + (clock.day + 1) + " in " + D.city.name, p.map(([rid, v]) => D.residents[resIdx[rid]].n + " " + standing(v)).join(" · ")]; }
    if (id === "wallet") { const loan = E.loans.find(l => l.status === "active"); return [fmt(E.totalMoney()), "in all" + (loan ? " · " + fmt(E.loanOwed(loan)) + " owed" : "")]; }
    if (id === "quests") return [(Object.keys(Q.active).length + (Q.job ? 1 : 0)) + " under way", Object.keys(QR.quests).filter(q => QR.quests[q].giver && Q.offers(QR.quests[q].giver).includes(q)).length + " on offer"];
    if (id === "property") { const mine = M.owned(), lots = new Set(mine.map(unitLot)).size, rents = Object.keys(M.tenancy).length; return [n(lots, "building"), lots ? "worth about " + fmt(mine.reduce((s, i) => s + M.value(i), 0)) : rents ? n(rents, "place") + " rented" : "none owned yet"]; }
    if (id === "belongings") return [n(Object.values(E.inv).reduce((s, k) => s + k, 0), "thing") + " carried", n(belongings.length, "piece") + " of furniture"];
    if (id === "wardrobe") return [n(outfits.length, "outfit"), "changing is free"];
    if (id === "map") return waypoint ? ["Mark: " + waypoint.name, Math.round(Math.hypot(waypoint.x - pl.x, waypoint.z - pl.z)) + " m"] : ["No mark", "click the map to set one"];
    if (id === "storage") return ["None yet", ""];
    return ["", ""];
  }
  function menuShell(P, body, title, sub) {
    const keyOf = t => t.action !== "menu" ? (ST.platforms[platform()].controls[t.action] || [])[0] : null, paint = (e, c) => { e.style.setProperty("--col", MC[c][0]); e.style.setProperty("--ink", MC[c][1]); return e; };
    const top = h("header", { cls: "mtop" }, h("div", { cls: "mwho" }, P.kind === "hub" || P.kind === "wardrobe" ? null : liveFace(42), h("div", {}, h("b", {}, look.name), h("span", {}, el("date").textContent))),
      h("div", { cls: "mstatus" }, h("span", {}, hhmm(clock.m)), h("span", {}, fmt(E.L.balance("p:cash")))), h("button", { type: "button", cls: "mback", on: { click: () => close() } }, "Back to town"));
    if (P.kind === "hub") return h("div", { cls: "mshell" }, top, h("div", { cls: "mhub" }, TABS.map(t => {
      const k = keyOf(t), [a, b] = headline(t.id), small = t.board === "small";
      return paint(h("button", { type: "button", cls: "mtile" + (t.board ? " " + t.board : ""), on: { click: () => menu(t.id) } }, k ? h("em", {}, k) : null,
        t.id === "me" ? [h("div", { cls: "mbig" }, liveFace(250)), h("div", { cls: "tn" }, look.name)] : small ? [micon(t.id, 26), h("div", {}, h("div", { cls: "tn" }, t.name), a ? h("div", { cls: "ts" }, a) : null)] : [micon(t.id, 30), h("div", { cls: "tn" }, t.name)],
        small ? null : [h("div", { cls: "tf" }, a), h("div", { cls: "ts" }, b)]), small && t.group === "game" ? "quiet" : t.colour);
    })));
    const t = TABS.find(x => x.id === P.kind), k = keyOf(t);
    const sh = h("div", { cls: "mshell" }, top, h("div", { cls: "msec" },
      h("nav", { cls: "mstrip", "aria-label": "Pages" }, h("button", { type: "button", cls: "all", title: "Everything", "aria-label": "Everything", on: { click: () => menu("hub") } }, micon("hub", 22)),
        TABS.map(x => paint(h("button", { type: "button", cls: x.id === t.id ? "on" : "", title: x.name, "aria-label": x.name, on: { click: () => { if (x.id !== t.id) menu(x.id); } } }, micon(x.id, 20)), x.colour))),
      h("section", { cls: "mpage" }, h("header", { cls: "mph" }, micon(t.id, 30), h("div", {}, h("h1", {}, title), h("p", {}, sub)), k ? h("em", {}, k) : null), body)));
    sh.style.setProperty("--sec", MC[t.colour][0]); sh.style.setProperty("--sec-ink", MC[t.colour][1]);
    return sh;
  }
  function cards(body) {                                  // a page's sections, each on its own card: a heading starts one
    const kids = [...body.childNodes]; let card = null; body.textContent = "";
    for (const k of kids) {
      if (k.nodeType === 1 && k.classList.contains("bare")) { body.append(k); card = null; continue; }
      if (!card || (k.nodeType === 1 && k.tagName === "H3")) { card = h("div", { cls: "mcard" }); body.append(card); }
      card.append(k);
    }
  }
  function unitLot(i) { return M.units[i].lot; }
  function showOnMap(n) {                                // a waypoint at a building, and the map (UI-006)
    const l = LOTS[n]; markAt((l.rect[0] + l.rect[2]) / 2, (l.rect[1] + l.rect[3]) / 2, l.name);
    close(); window.wasted.showMap(true); toast("Marked on the map: " + l.name + ". Esc or M closes the map.");
  }
  function designFrom(n, start) {                        // the building designer, from the menu, wherever you are (owner, 2026-11-23)
    const back = panel && panel.menu ? panel.kind : null; if (panel) { panel.back = null; close(); } designStart = start; open("remodel", n); panel.back = back;
  }
  function buildingsTab(body) {                          // UI-006: each building you own once, with Remodel and Furnish wherever you are
    const lots = [...new Set(M.owned().map(unitLot))];
    body.append(h("h3", {}, "Your buildings"));
    if (!lots.length) { body.append(h("p", { cls: "dim" }, "You own no building yet, so there is nothing to remodel. Buy one: stand in it and press P, or see an estate agent. (To try the designer: open the slice with ?rich=1.)")); return; }
    for (const n of lots) {
      const l = LOTS[n], own = holding(n), why = l.house ? "the community houses cannot be remodelled yet" : !l.dsg ? "this building has no design yet" : !l.drawn ? "this building is not drawn in the slice" : "";
      const fwhy = why || (!canFurnish(l) ? "this kind of building is not furnished" : "");
      body.append(h("div", { cls: "qline" }, h("b", {}, l.name), h("p", { cls: "dim" }, (own.whole ? "The whole building" : "Your rooms on " + own.floors.map(f => f ? "floor " + f : "the ground floor").join(", ")) + (why ? " · " + why : "")),
        h("div", { cls: "prow" }, btn(own.whole ? "Remodel" : "Redecorate your rooms", () => designFrom(n, null), !!why), btn("Furnish", () => designFrom(n, "furnish"), !!fwhy), btn("Show on map", () => showOnMap(n)))));
    }
  }
  function belongingsTab(P, body) {                       // INV-001: what you carry and the furniture not placed, in one list
    const inv = Object.keys(E.inv).sort(), FR = Furnish.rules();
    body.append(h("h3", {}, "You carry"), inv.length ? tbl(inv.map(it => [E.items[it].name, "× " + E.inv[it], money(E.items[it].price * E.inv[it])])) : h("p", { cls: "dim" }, "Nothing."));
    body.append(h("h3", {}, "Furniture"), belongings.length ? tbl(belongings.map(b => [b.name + (b.line && !(FR.pieces[b.kind] || {}).from ? " (" + ((Outlet.LINES[b.line] || {}).label || b.line) + ")" : ""),   // (a good from the mall has no line)
      h("span", { cls: "dim" }, b.w.toFixed(2) + " × " + b.d.toFixed(2) + " m" + (b.from ? " · from " + ((LOTS.find(l => l.id === b.from.lot) || {}).name || "a home") : "")),
      b.kind !== "town" && FR.pieces[b.kind] ? money(FR.pieces[b.kind].price) : ""]))
      : h("p", { cls: "dim" }, "None. A piece you take away in the building designer comes here, and you place it again from its Furnish tab, in any home of yours."));
    if (belongings.length) body.append(h("p", { cls: "dim" }, "Place these from the Furnish tab of the building designer (Properties, Furnish), in any home of yours."));
    return ["Belongings", "What you own and carry · G"];
  }
  const WPARTS = ST.wardrobe.parts;
  function outfitOf(v) { const o = {}; for (const k of WPARTS) o[k] = v[k]; return o; }
  function outfitOk(o) {                                 // as world.outfit_problems()
    for (const k of Object.keys(o)) if (k !== "name" && !WPARTS.includes(k)) return false;
    for (const k of WPARTS) if (!(k in o)) return false;
    const v = Object.assign({}, look, o, { name: look.name }); return lookOk(v) && typeof o.name === "string" && o.name.trim().length > 0 && o.name.length <= DG.name.max;
  }
  function wardrobeTab(P, body) {                         // CHR-014, CHR-015: what you wear, changed for free, and outfits to change between
    const set = (k, v) => { look[k] = v; applyLook(); save(); draw(); };
    const rgb = c => "rgb(" + c.map(v => Math.round(Math.pow(v, 1 / 2.2) * 255)).join(",") + ")";
    const pal = (k, none) => h("div", { cls: "swatches" }, (none ? [null] : []).concat(Object.keys(DG.palette)).map(p => h("button", { type: "button", cls: "sw" + (look[k] === p ? " on" : "") + (p === null ? " none" : ""), title: p || "none", style: p ? "background:" + rgb(DG.palette[p]) : "", on: { click: () => set(k, p) } }, p === null ? "×" : "")));
    const chips = (k, opts) => h("div", { cls: "chips" }, Object.entries(opts).map(([v, label]) => h("button", { type: "button", cls: "chip" + (look[k] === v ? " on" : ""), on: { click: () => set(k, v) } }, label)));
    const row = (label, ...kids) => h("div", { cls: "dzrow" }, h("span", { cls: "dzl" }, label), ...kids);
    const hairNames = {}; for (const v of DG.hair) hairNames[v] = v[0].toUpperCase() + v.slice(1);
    body.append(h("h3", {}, "Outfits"));
    if (outfits.length) body.append(tbl(outfits.map((o, k) => [h("b", {}, o.name), btn("Wear", () => { Object.assign(look, outfitOf(o)); applyLook(); return "You change into " + o.name + "."; }),
      btn("Forget", () => { outfits.splice(k, 1); return "Forgotten."; }, false, "quiet")])));
    else body.append(h("p", { cls: "dim" }, "None saved yet."));
    const nm = h("input", { type: "text", maxLength: DG.name.max, placeholder: "Name this outfit", "aria-label": "Outfit name" });
    body.append(h("div", { cls: "prow" }, nm, btn("Save what you wear", () => {
      const o = Object.assign(outfitOf(look), { name: nm.value.trim() });
      if (!outfitOk(o)) throw new Econ.Refused("an outfit needs a name of 1 to " + DG.name.max + " letters");
      const k = outfits.findIndex(x => x.name === o.name); if (k < 0 && outfits.length >= ST.wardrobe.max_outfits) throw new Econ.Refused("the wardrobe holds " + ST.wardrobe.max_outfits + " outfits");
      if (k >= 0) outfits[k] = o; else outfits.push(o); return "Saved as " + o.name + ".";
    })));
    const own = Object.keys(E.clothes).sort((a, b) => E.good(a).name < E.good(b).name ? -1 : 1);   // ECO-014: clothes bought at the mall's outlets
    const partWords = P => Object.entries(P).map(([k, v]) => k === "garment" ? DG.garment[v] : k === "bottom_type" ? DG.bottom_type[v] : k === "bag" ? DG.extras.bag : null).filter(x => x).join(", ");
    const wearing = G => Object.entries(G.parts).every(([k, v]) => look[k] === v);
    body.append(h("h3", {}, "Clothes you own"), own.length ? tbl(own.map(g => { const G = E.good(g); return [h("b", {}, G.name), h("span", { cls: "dim" }, (E.clothes[g] > 1 ? "× " + E.clothes[g] + " · " : "") + partWords(G.parts)),
      wearing(G) ? h("span", { cls: "pos" }, "on") : btn("Put on", () => { Object.assign(look, G.parts); applyLook(); return "You put on the " + G.name.toLowerCase() + "."; })]; }))
      : h("p", { cls: "dim" }, "None yet. Clothes and shoes bought at the mall's outlets are kept here."));
    body.append(h("h3", {}, "What you wear"),
      row("Hair", chips("hair", hairNames)), row("Hair colour", h("div", { cls: "swatches" }, DG.hair_color.map((c, n) => h("button", { type: "button", cls: "sw" + (look.hair_color === n ? " on" : ""), style: "background:" + rgb(c), on: { click: () => set("hair_color", n) } })))),
      row("Top", chips("garment", DG.garment)), row("", pal("top")), row("Bottom", chips("bottom_type", DG.bottom_type)), row("", pal("bottom")), row("Shoes", pal("shoes")),
      row("Scarf", pal("scarf", true)), row("Cap", pal("cap", true)), row("Apron", pal("apron", true)),
      row("", h("div", { cls: "chips" }, h("button", { type: "button", cls: "chip" + (look.bag ? " on" : ""), on: { click: () => set("bag", !look.bag) } }, DG.extras.bag),
        h("button", { type: "button", cls: "chip" + (look.tash ? " on" : ""), on: { click: () => set("tash", !look.tash) } }, DG.extras.tash))),
      h("p", { cls: "dim" }, "Changing is free, anywhere (for now: later, clothes are bought in shops and kept in a wardrobe at home)."));
    return ["Wardrobe", look.name + " · U"];
  }
  function settingsTab(P, body) {                         // UI-009, one group at a time (UI-015)
    const g = P.sub in ST.groups ? P.sub : Object.keys(ST.groups)[0], G = ST.groups[g];
    body.append(h("div", { cls: "chips bare" }, Object.entries(ST.groups).map(([k, x]) => h("button", { type: "button", cls: "chip" + (k === g ? " on" : ""), on: { click: () => { P.sub = k; draw(); } } }, x.name))));
    body.append(h("h3", {}, G.name));
    for (const [k, s] of Object.entries(G.settings)) {
      const v = settings[g][k]; let ctl;
      if (s.type === "switch") ctl = h("div", { cls: "chips" }, [true, false].map(x => h("button", { type: "button", cls: "chip" + (v === x ? " on" : ""), on: { click: () => setting(g, k, x) } }, x ? "On" : "Off")));
      else if (s.type === "choice") ctl = h("div", { cls: "chips" }, Object.entries(s.options).map(([x, label]) => h("button", { type: "button", cls: "chip" + (v === x ? " on" : ""), on: { click: () => setting(g, k, x) } }, label)));
      else { const out = h("span", { cls: "dzval" }, String(v)); ctl = h("div", { cls: "dzslide" }, h("input", { type: "range", min: s.min, max: s.max, step: s.step, value: v, "aria-label": s.name,
        on: { input: e => { out.textContent = e.target.value; }, change: e => setting(g, k, +e.target.value) } }), out); }
      body.append(h("div", { cls: "dzrow" }, h("span", { cls: "dzl wide" }, s.name), ctl));
    }
    body.append(h("div", { cls: "prow bare" }, btn("Back to the defaults", () => { settings = defaults(); try { localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings)); } catch (e) { } window.wasted.applySettings(settings); return "Settings are back to their defaults."; }, false, "quiet"),
      h("span", { cls: "dim" }, "Settings are kept on this device, not in your save.")));
    return ["Settings", "Picture, sound, the screen and the controls"];
  }
  function helpTab(P, body) {                             // UI-008: the controls of the platform played on
    const here = platform(), pf = P.pf || here, PL = ST.platforms[pf];
    body.append(h("div", { cls: "prow" }, "Show for", ...Object.entries(ST.platforms).map(([k, x]) => h("button", { type: "button", cls: "chip" + (k === pf ? " on" : ""), on: { click: () => { P.pf = k; draw(); } } }, x.name + (k === here ? " (this one)" : "")))));
    if (pf !== "keyboard") body.append(h("p", { cls: "dim" }, "The slice is played with a keyboard and mouse; these are the " + PL.name.toLowerCase() + " controls for when it comes (UI-004)."));
    for (const [c, cname] of Object.entries(ST.contexts)) {
      const acts = Object.entries(ST.actions).filter(([, a]) => a.context === c); if (!acts.length) continue;
      body.append(h("h3", {}, cname), tbl(acts.map(([a, x]) => [h("span", { cls: "keys" }, (PL.controls[a] || []).map(k => h("kbd", {}, k))), x.name])));
    }
    return ["Help", "The controls · H"];
  }
  // ---------------------------------------------------------------- the game menu, level 2 (UI-011 to UI-014): Me, Storage, the map's mark, save slots
  function standing(v) { return v >= 30 ? "trusts you" : v >= 10 ? "likes you" : v > 0 ? "knows you" : v <= -30 ? "will not deal with you" : v <= -10 ? "distrusts you" : "is wary of you"; }
  const an = w => (/^[aeiou]/.test(w) ? "an " : "a ") + w;
  function meTab(P, body) {                              // UI-011: who you are, what you have, how the town sees you
    const word = v => v ? v[0].toUpperCase() + v.slice(1) : "", G = DG.garment[look.garment] || look.garment, B = DG.bottom_type[look.bottom_type] || look.bottom_type;
    const mine = M.owned(), value = mine.reduce((s, i) => s + M.value(i), 0), rents = Object.keys(M.tenancy).length, loan = E.loans.find(l => l.status === "active");
    body.append(h("p", {}, h("b", {}, look.name), " · day " + (clock.day + 1) + " in " + D.city.name),
      h("p", { cls: "dim" }, word(G) + " in " + look.top + ", " + B.toLowerCase() + " in " + look.bottom + ", " + look.shoes + " shoes" + (look.scarf ? ", " + an(look.scarf) + " scarf" : "") + (look.cap ? ", " + an(look.cap) + " cap" : "") + "."),
      h("h3", {}, "What you have"), tbl([
        ["Money", money(E.totalMoney()), btn("Money", () => menu("wallet"))],
        ["Loans", loan ? money(E.loanOwed(loan)) : h("span", { cls: "dim" }, "none"), ""],
        ["Property", h("span", {}, mine.length + " owned, worth about " + fmt(value) + (rents ? " · " + rents + " rented" : "")), btn("Properties", () => menu("property"))],
        ["Belongings", h("span", {}, Object.values(E.inv).reduce((s, n) => s + n, 0) + " things carried · " + belongings.length + " pieces of furniture"), btn("Belongings", () => menu("belongings"))],
        ["Wardrobe", h("span", {}, outfits.length + " outfit" + (outfits.length === 1 ? "" : "s")), btn("Wardrobe", () => menu("wardrobe"))]]),
      h("h3", {}, "Work"), h("p", {}, Q.job ? QR.jobs.kinds[Q.job.kind].title + " (under way)" : "No job right now: ask people for work.", " ", h("span", { cls: "dim" }, (Q.jobs_done || []).length + " jobs done, " + Object.keys(Q.done).length + " quests finished")));
    const all = Achievements.list(), got = all.filter(a => a.id in feats);   // UI-019: earned ones first with their day, the rest still to do
    body.append(h("h3", {}, "Achievements · " + got.length + " of " + all.length), h("table", { cls: "feats" }, got.concat(all.filter(a => !(a.id in feats))).map(a =>
      h("tr", { cls: a.id in feats ? "" : "todo" }, h("td", {}, a.name), h("td", {}, a.line), h("td", { cls: "dim" }, a.id in feats ? "day " + (feats[a.id] + 1) : "")))));
    const people = Object.entries(E.rep).filter(([, v]) => v).sort((a, b) => b[1] - a[1]).slice(0, 8);
    body.append(h("h3", {}, "People"), people.length ? tbl(people.map(([rid, v]) => [D.residents[resIdx[rid]].n, DLR(rid), h("span", { cls: v > 0 ? "pos" : "neg" }, standing(v))]))
      : h("p", { cls: "dim" }, "Nobody knows you yet. Talk to people; help them, and they remember."));
    const shops = Object.keys(E.staff || {}).map(Number).map(n => [n, E.repWithLot(n)]).filter(([, v]) => v).sort((a, b) => b[1] - a[1]).slice(0, 8);
    if (shops.length) body.append(h("h3", {}, "Shops and places"), tbl(shops.map(([n, v]) => [lotName(n), h("span", { cls: v > 0 ? "pos" : "neg" }, v > 0 ? "prices a little lower for you" : "prices higher for you")])));
    return ["Me", look.name + " · C"];
  }
  function storageTab(P, body) {                          // UI-014: storage containers come with F-PROP-STORAGE
    body.append(h("p", {}, "You have no storage yet. Lockers and storage units (somewhere to keep furniture and things you are not using) come with storage containers, a feature not picked for this slice."),
      h("p", { cls: "dim" }, "Until then, furniture you take out of a home is kept in your belongings, and placed again from the building designer."),
      h("div", { cls: "prow" }, btn("Belongings", () => menu("belongings"))));
    return ["Storage", "Lockers and storage units"];
  }
  function markAt(x, z, name) {                          // UI-012: a mark on the map (again on it: cleared)
    if (waypoint && Math.hypot(waypoint.x - x, waypoint.z - z) < 6 && !name) { waypoint = null; toast("Mark cleared."); return; }
    const l = window.wasted.lotAt(x, z, true); waypoint = { x, z, name: name || (l ? l.name : "Your mark") }; toast("Marked: " + waypoint.name + ".");
  }
  function waypointTag(tags) {                           // the mark in the street, with how far it is; reached, it goes
    if (!waypoint || !settings || !settings.map.map_waypoint) return;
    const d = Math.hypot(waypoint.x - pl.x, waypoint.z - pl.z);
    if (d < 4) { toast("You are there: " + waypoint.name + "."); waypoint = null; return; }
    tags.push({ x: waypoint.x, y: window.wasted.groundBase(waypoint.x, waypoint.z) + 3.2, z: waypoint.z, d, a: 1, size: 16, text: "◆ " + waypoint.name + " · " + (d < 1000 ? Math.round(d) + " m" : (d / 1000).toFixed(1) + " km") });
  }
  // UI-013: save slots. Slot 1 is the save the slice always had; the slot played last is the one Continue carries on.
  const SLOTS = ST.save.slots, slotKey = k => k === 1 ? SAVE_KEY : SAVE_KEY + ".s" + k;
  let slot = (() => { try { const k = +localStorage.getItem(SAVE_KEY + ".slot"); return k >= 1 && k <= SLOTS ? k : 1; } catch (e) { return 1; } })();
  function useSlot(k) { slot = k; try { localStorage.setItem(SAVE_KEY + ".slot", String(k)); } catch (e) { } }
  function saveTab(P, body) {                             // UI-007, UI-013
    if (transient) body.append(h("p", { cls: "neg" }, "This is a trial run (?auto=1): nothing is saved."));
    const rows = [];
    for (let k = 1; k <= SLOTS; k++) {
      const s = readSave(k), when = s && s.at ? new Date(s.at) : null;
      const what = s ? h("span", {}, h("b", {}, s.look && s.look.name || "?"), h("span", { cls: "dim" }, " · day " + ((s.clock && s.clock.day) + 1) + " · saved " + when.toLocaleDateString([], { day: "numeric", month: "short" }) + " " + when.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" }))) : h("span", { cls: "dim" }, "Empty");
      const acts = [btn(k === slot ? "Save now" : "Save here", () => { useSlot(k); save(); return transient ? "Nothing is saved in a trial run." : "Saved in slot " + k + "."; }, transient)];
      if (s) acts.push(btn("Load", () => { useSlot(k); close(); loadSave(readSave(k)); toast("Loaded slot " + k + "."); }, transient));
      if (P.confirm === k) acts.push(btn(s ? "Yes: delete it and start a new character here" : "Yes: start a new character here", () => { try { localStorage.removeItem(slotKey(k)); } catch (e) { } useSlot(k); location.reload(); }, false, "risk"));
      else acts.push(btn(s ? "New game here" : "New game", () => { P.confirm = k; }, transient, s ? "risk" : "quiet"));
      rows.push([h("b", {}, "Slot " + k + (k === slot ? " (playing)" : "")), what, h("div", { cls: "prow" }, ...acts)]);
    }
    body.append(tbl(rows), h("p", { cls: "dim" }, "The game saves itself in the slot you are playing as you go. A new game in a slot that holds a save deletes it, so it asks twice."),
      h("div", { cls: "prow" }, btn("Back to the title", () => { save(); location.reload(); }, false, "quiet")));
    return ["Save", SLOTS + " save slots, kept in this browser"];
  }
  // ---------------------------------------------------------------- moving house (PROP-022) and the boards at the doors (PROP-023)
  // A resident whose household has moved keeps the day citysim planned, but lives at the new home: what they did at the
  // old home they do at the new one (the same kind of place, on the new home's floors), and the walks to and from it
  // follow the streets (D.life.walk_net, citysim's net). Their car trips still start where the car was parked.
  const ORIG = D.plan.slice(), HOME0 = D.residents.map(r => r.h), STROLL = D.acts.indexOf("stroll"), WN = X.walk_net || { nodes: [], edges: [], anchor: {} };
  let movesSeen = null, netAdj = null; const fromCache = new Map();
  function adjOf() {
    if (!netAdj) { netAdj = WN.nodes.map(() => []); for (const [a, b, w] of WN.edges) { netAdj[a].push([b, w]); netAdj[b].push([a, w]); } }
    return netAdj;
  }
  function nearestNode(x, z) { let best = -1, bd = 1e18; WN.nodes.forEach(([nx, nz], n) => { const d = (nx - x) ** 2 + (nz - z) ** 2; if (d < bd) { bd = d; best = n; } }); return best; }
  function walkPath(a, b) {                               // node coordinates from a to b along the streets (Dijkstra)
    if (a < 0 || b < 0 || a === b) return a >= 0 ? [WN.nodes[a]] : [];
    let prev = fromCache.get(a);
    if (!prev) {
      const A = adjOf(), dist = new Float64Array(A.length).fill(Infinity), done = new Uint8Array(A.length); prev = new Int32Array(A.length).fill(-1); dist[a] = 0;
      const heap = [[0, a]], push = x => { heap.push(x); let i = heap.length - 1; while (i > 0) { const j = (i - 1) >> 1; if (heap[j][0] <= heap[i][0]) break; [heap[i], heap[j]] = [heap[j], heap[i]]; i = j; } };
      const pop = () => { const top = heap[0], last = heap.pop(); if (heap.length) { heap[0] = last; let i = 0; for (;;) { const l = 2 * i + 1, r = l + 1; let m = i; if (l < heap.length && heap[l][0] < heap[m][0]) m = l; if (r < heap.length && heap[r][0] < heap[m][0]) m = r; if (m === i) break; [heap[i], heap[m]] = [heap[m], heap[i]]; i = m; } } return top; };
      while (heap.length) { const [d, u] = pop(); if (done[u]) continue; done[u] = 1; for (const [v, w] of A[u]) if (d + w < dist[v]) { dist[v] = d + w; prev[v] = u; push([d + w, v]); } }
      fromCache.set(a, prev);
    }
    const out = [b]; while (out[out.length - 1] !== a) { const q = prev[out[out.length - 1]]; if (q < 0) return [WN.nodes[a], WN.nodes[b]]; out.push(q); }
    return out.reverse().map(n => WN.nodes[n]);
  }
  function spotIn(OL, NL, x, z, floors, m) {              // the same kind of place in the new home, on its floors
    for (const t in OL.objects || {}) {
      const j = OL.objects[t].findIndex(q => Math.abs(q[0] - x) < .25 && Math.abs(q[1] - z) < .25); if (j < 0) continue;
      const lv = (NL.levels || {})[t], opts = ((NL.objects || {})[t] || []).map((q, n) => [q[0], q[1], lv ? lv[n] : 0]).filter(q => floors.includes(q[2]));
      if (opts.length) return opts[m % opts.length];                // with its floor, for resident() (WLD-053)
      break;
    }
    const [a0, b0, a1, b1] = OL.rect, [c0, d0, c1, d1] = NL.rect;
    return [c0 + (x - a0) / (a1 - a0) * (c1 - c0), d0 + (z - b0) / (b1 - b0) * (d1 - d0)];
  }
  function planFrom(i, nl, unit, m) {
    const old = HOME0[i], OL = LOTS[old], NL = LOTS[nl], row = ORIG[i].map(p => p.slice()), moved = new Set();
    row.forEach((p, k) => { if (p[1] === 0 && p[7] === old) { const q = spotIn(OL, NL, p[2], p[3], unit.floors, m); p[2] = q[0]; p[3] = q[1]; p[7] = nl; p.length = 8; if (q[2]) p[8] = q[2]; p.s = null; moved.add(k); } });
    row.forEach((p, k) => {
      if (p[1] !== 1 || p[2] === STROLL) return;
      const a = k > 0 && row[k - 1][1] === 0 ? k - 1 : -1, b = k + 1 < row.length && row[k + 1][1] === 0 ? k + 1 : -1;
      if (!moved.has(a) && !moved.has(b)) return;
      const from = a >= 0 ? [row[a][2], row[a][3]] : [p[3], p[4]], to = b >= 0 ? [row[b][2], row[b][3]] : [p[p.length - 2], p[p.length - 1]];
      const same = a >= 0 && b >= 0 && row[a][7] === row[b][7];
      const na = a >= 0 && WN.anchor[row[a][7]] !== undefined ? WN.anchor[row[a][7]] : nearestNode(from[0], from[1]);
      const nb = b >= 0 && WN.anchor[row[b][7]] !== undefined ? WN.anchor[row[b][7]] : nearestNode(to[0], to[1]);
      const pts = same ? [] : walkPath(na, nb), flat = [p[0], 1, p[2], from[0], from[1]];
      for (const q of pts) flat.push(q[0], q[1]);
      flat.push(to[0], to[1]); row[k] = flat;
    });
    return row;
  }
  function rehome() {
    const last = M.moves[M.moves.length - 1], key = M.moves.length + ":" + (last ? last.join(",") : "");
    if (key === movesSeen) return; movesSeen = key;
    D.residents.forEach((r, i) => {
      const u = M.home[r.id], nl = u === undefined ? HOME0[i] : M.units[u].lot;
      D.plan[i] = nl === HOME0[i] ? ORIG[i] : planFrom(i, nl, M.units[u], Math.max(0, M.members[u].indexOf(r.id)));
    });
  }
  function boardText(n) {
    const [sale, let_] = M.board(n), k = M.byLot.get(n).length;
    if (!sale && !let_) return null;
    const part = (c, w) => c ? (k > 1 ? c + " " + w : w) : null;
    return [part(sale, "for sale"), part(let_, "to let")].filter(Boolean).join(" · ").replace(/^./, c => c.toUpperCase());
  }
  function boards(tags) {                                 // a board over the door of every building near you with something on the market
    if (!ready || !M) return;
    for (const n of M.byLot.keys()) {
      const l = LOTS[n], d = Math.hypot(l.door[0] - pl.x, l.door[1] - pl.z); if (d > 32) continue;
      const t = boardText(n); if (t) tags.push({ x: l.door[0], y: 3.1, z: l.door[1], d, a: Math.min(1, (32 - d) / 8), size: 14, text: "▣ " + t });
    }
  }
  // ---------------------------------------------------------------- property: B at an estate agent, P anywhere (PROP-011 to PROP-019)
  const m2 = i => M.units[i].area + " m²";
  function tenantText(i) {
    const t = M.tenant[i];
    return t === "player" ? "you rent it" : t === "hh" ? "lived in" : t === "firm" ? "in business" : t === "lodger" ? "let" : "empty";
  }
  function payChoice() {                                  // outright, or a mortgage at the least deposit (BANK-002, PROP-019)
    const C = X.estate.mortgage, sel = h("select", { "aria-label": "How to pay" }, h("option", { value: "0" }, "outright"), ...C.terms.map(t => h("option", { value: String(t) }, "mortgage, " + t + " days")));
    sel.value = panel.pay || "0"; sel.onchange = () => { panel.pay = sel.value; };
    return sel;
  }
  function buyUnits(units, agent) {
    const term = +(panel.pay || 0), C = X.estate.mortgage, D_ = X.estate.trade.whole_building_discount_bp;
    if (!term) { const p = M.buy(units, agent); return "Bought for " + fmt(p) + " with the transfer tax."; }
    const total = units.map(i => units.length > 1 ? Math.floor(M.ask(i) * (10000 - D_) / 10000) : M.ask(i)).reduce((a, b) => a + b, 0);
    const dep = Math.ceil(total * C.deposit_bp / 10000);
    M.buy(units, agent, dep, term);
    const m = M.mortgages[M.mortgages.length - 1];
    return "Bought with a deposit of " + fmt(dep) + "; the bank lends " + fmt(m.principal) + ", repaid at about " + fmt(m.plan[0]) + " a day for " + term + " days.";
  }
  function unitRow(i, agent) {
    const a = M.ask(i), cells = [h("span", {}, M.name(i)), h("span", { cls: "dim" }, m2(i) + " · " + M.ownerName(i) + " · " + tenantText(i))];
    cells.push(a !== null ? money(a) : h("span", { cls: "dim" }, M.owner[i] === "player" ? "yours" : "not for sale"));
    cells.push(h("span", { cls: "dim" }, fmt(M.marketRent(i)) + "/day"));
    const bs = [];
    if (a !== null) bs.push(btn("Buy", () => buyUnits([i], agent)));
    if (M.tenant[i] === null && M.owner[i] !== "player") bs.push(btn("Rent", () => { M.rentUnit(i, agent); return "You rent " + M.name(i) + " for " + fmt(M.tenancy[String(i)].rent) + " a day, paid each midnight."; }));
    cells.push(h("span", {}, ...bs));
    return cells;
  }
  function estatePanel(P, body) {
    const agent = P.kind === "agent" ? M.office[LOTS[P.n].id] : null, R = X.estate, hist = M.history[M.history.length - 1];
    body.append(purseLine(), h("p", { cls: "dim" }, "Prices in town " + ((hist ? hist[1] : M.townIndex()) / 100).toFixed(1) + "% of usual · money in town " + fmt(E.L.supply()) + (hist ? " · " + hist[3] + " sold yesterday" : "")));
    const here = P.kind === "property" ? lotHere() : null;
    if (here !== null && here >= 0 && M.byLot.has(here)) {     // the building you stand in
      const ix = M.byLot.get(here), rest = ix.filter(i => M.owner[i] !== "player");
      body.append(h("h3", {}, "This building: " + lotName(here)), h("div", { cls: "prow" }, "Pay", payChoice()), tbl(ix.map(i => unitRow(i, null))));
      if (ix.length > 1 && rest.length && rest.every(i => M.ask(i) !== null)) {
        const tot = rest.map(i => Math.floor(M.ask(i) * (10000 - R.trade.whole_building_discount_bp) / 10000)).reduce((a, b) => a + b, 0);
        body.append(btn("Buy the whole building for " + fmt(tot), () => buyUnits(rest, null)));
      }
    }
    if (agent) {
      const kinds = ["all"].concat(Object.keys(R.kinds)), f = h("select", { "aria-label": "Kind" }, kinds.map(k => h("option", { value: k }, k === "all" ? "everything" : R.kinds[k].name.toLowerCase())));
      f.value = P.filter || "all"; f.onchange = () => { P.filter = f.value; draw(); };
      const want = i => P.filter && P.filter !== "all" ? M.units[i].kind === P.filter : true;
      const listed = Object.keys(M.listing).map(Number).filter(i => M.owner[i] !== "player" && want(i)).sort((a, b) => M.listing[a].price - M.listing[b].price || a - b).slice(0, 12);
      body.append(h("h3", {}, "For sale"), h("div", { cls: "prow" }, "Show", f, "Pay", payChoice()),
        listed.length ? tbl(listed.map(i => unitRow(i, agent))) : h("p", { cls: "dim" }, "Nothing of that kind on the market today."),
        h("p", { cls: "dim" }, "Anything else in town can be bought too: stand in the building and press P to see what its owners would take."));
      const free = M.units.map((u, i) => i).filter(i => M.tenant[i] === null && M.owner[i] !== "player" && want(i)).sort((a, b) => M.marketRent(a) - M.marketRent(b) || a - b).slice(0, 8);
      body.append(h("h3", {}, "To let"), free.length ? tbl(free.map(i => unitRow(i, agent))) : h("p", { cls: "dim" }, "Nothing empty just now."));
    }
    const mine = M.owned();
    if (!agent) buildingsTab(body);                       // Remodel, Furnish and Show on map, first (UI-006)
    body.append(h("h3", {}, "Your property"));
    if (!mine.length) body.append(h("p", { cls: "dim" }, "You own nothing yet."));
    for (const i of mine) {
      const ls = M.listing[String(i)], rent = h("input", { type: "number", min: "0.01", step: "0.01", cls: "amt", value: (M.rentOf(i) / 100).toFixed(2), "aria-label": "Rent in brix a day" });
      const row = [btn("Sell now for " + fmt(M.bid(i)), () => { const p = M.sellNow(i, agent); return "Sold for " + fmt(p) + "."; }, false, "risk")];
      if (agent) row.push(ls ? btn("Take it off the market", () => { M.unlist(i); }) : btn("List at " + fmt(M.value(i)), () => { M.listForSale(i, M.value(i), agent); return "Listed with " + M.cos[agent].name + "; the agent takes " + (R.trade.agent_fee_bp / 100) + "% when it sells."; }));
      const owed = M.owed[String(i)];
      body.append(h("div", { cls: "qline" }, h("b", {}, M.name(i)),
        h("p", { cls: "dim" }, m2(i) + " · worth about " + fmt(M.value(i)) + " · " + tenantText(i) + " · upkeep and tax " + fmt(M.costOf(i)) + "/day" + (ls ? " · listed at " + fmt(ls.price) : "") + (owed ? " · " + fmt(owed) + " unpaid!" : "")),
        h("div", { cls: "prow" }, "Rent Bx", rent, h("span", { cls: "dim" }, "a day (market " + fmt(M.marketRent(i)) + ")"),
          btn("Set", () => { M.setRent(i, Math.round(+rent.value * 100), true); return "Rent set; offered to tenants."; }),
          btn(M.to_let[i] ? "Stop letting" : "Offer to let", () => { M.to_let[i] = !M.to_let[i]; })), h("div", { cls: "prow" }, ...row)));
    }
    const ten = Object.keys(M.tenancy).map(Number);
    if (ten.length) body.append(h("h3", {}, "Places you rent"), tbl(ten.map(i => [M.name(i), money(M.tenancy[String(i)].rent), h("span", { cls: "dim" }, "a day" + (M.tenancy[String(i)].missed ? ", " + M.tenancy[String(i)].missed + " unpaid" : "")), btn("Move out", () => { M.leave(i); return "You have moved out."; }, false, "risk")])));
    const ms = M.mortgages.filter(m => m.status === "active");
    if (ms.length) body.append(h("h3", {}, "Mortgages"), tbl(ms.map(m => ["Mortgage " + m.id, money(M.mortgageOwed(m)), h("span", { cls: "dim" }, "about " + fmt(m.plan[0]) + " a day" + (m.missed ? "; " + m.missed + " missed" : "")), btn("Repay now", () => { M.repay(m.id); return "Mortgage repaid."; })])));
    const sales = M.sales.slice(-5).reverse(), who = b => b === "player" ? "you" : b === "hh" ? "the household" : b === "firm" ? "the business" : b === "city" ? "the town" : M.cos[b.slice(3)].name;
    if (sales.length) body.append(h("h3", {}, "Lately sold"), tbl(sales.map(([d, i, p, , b]) => [M.name(i), money(p), h("span", { cls: "dim" }, "day " + (d + 1) + " to " + who(b))])));
    return agent ? [M.cos[agent].name, "Estate agents · buy, sell and rent anywhere in town"] : ["Properties", "What you own and rent · P"];
  }
  // ---------------------------------------------------------------- the building designer: remodel (BUILD-004 to BUILD-007, BUILD-016 to BUILD-019)
  // E at the door of a building you own, or inside your unit. Every choice comes from Builder.options() (the kit and the district's
  // palette); each one is shown on the building at once (play.js repaint) with its price; Pay checks it as builder.py does and pays
  // through the ledger. Outside needs the whole building; inside, the storeys of the units you own.
  function holding(n) {                                   // what the player holds in lot n: {whole} or {floors}, or null
    const ix = M && M.byLot.get(n); if (!ix) return null;
    const mine = ix.filter(i => M.owner[i] === "player"); if (!mine.length) return null;
    return mine.length === ix.length ? { whole: true } : { floors: [...new Set(mine.flatMap(i => M.units[i].floors))].sort((a, b) => a - b) };
  }
  function remodelTarget() {
    if (!ready || panel || pl.pose > 1) return null;
    let l = lotAt(pl.x, pl.z, true);
    if (!l || !l.dsg) { l = null; for (const c of LOTS) if (c.dsg && c.drawn && Math.abs(c.door[0] - pl.x) < 2.2 && Math.abs(c.door[1] - pl.z) < 2.2) { l = c; break; } }
    if (!l || !l.dsg || !l.drawn) return null;
    const n = LOTS.indexOf(l), own = holding(n); if (!own) return null;
    return { type: "remodel", x: pl.x, z: pl.z, y: pl.y, lot: l, label: (own.whole ? "Remodel " : "Redecorate your rooms in ") + l.name };
  }
  function remodel(l) { open("remodel", LOTS.indexOf(l)); }
  function funds() { return E.L.balance("p:cash") + (E.accounts.includes("current") ? E.L.balance("p:current") : 0); }
  function remodelPanel(P, body) {
    const l = LOTS[P.n], own = holding(P.n) || { floors: [] }, BF = Builder.FACADES, FN = Builder.FACADE_NAME, FW = window.wasted.furn, FR = Furnish.rules();
    const mine = own.whole ? [...Array(l.floors).keys()] : own.floors, furnishes = canFurnish(l);
    if (!P.lot) Object.assign(P, { lot: l, draft: l.dsg, lay: {}, lay0: {}, plan: {}, plan0: {}, undo: [], tab: designStart === "furnish" && furnishes ? "furnish" : own.whole ? "outside" : "inside", facade: "*", storey: mine[0], sel: null, room: Object.keys(FR.rooms)[0], line: FR.lines[0] });
    designStart = null;
    const d = P.draft, menu = Builder.options(l, l.dsg), K = X.builder.kit, pats = X.builder.patterns;
    if (P.tab === "furnish" && !P.lay[P.storey]) P.lay[P.storey] = P.lay0[P.storey] = layNow(l, P.storey);
    const snap = () => snapOf(P);
    const showAll = () => { window.wasted.repaint(l, Builder.built(P.draft)); for (const f of Object.keys(P.plan)) FW.showPlan(l, +f, P.plan[f]); for (const f of Object.keys(P.lay)) FW.showLayout(l, +f, P.lay[f]); };
    const set = changes => { P.undo.push(snap()); P.draft = Builder.remodel(P.draft, changes); window.wasted.repaint(l, Builder.built(P.draft)); P.msg = ""; Sound.act("talk_choose"); draw(); };
    const css = c => "rgb(" + c.map(v => Math.round(Math.min(1, v * 1.25) * 255)).join(",") + ")";
    const pick = (o, cur, path, extra) => h("button", { type: "button", cls: "sw" + (o.rgb ? "" : " word") + (extra || "") + (JSON.stringify(cur) === JSON.stringify(o.id) ? " on" : ""), title: o.name, "aria-label": o.name,
      style: o.rgb ? "background-color:" + css(o.rgb) : "", on: { click: () => set(typeof path === "function" ? path(o) : { [path]: o.id }) } }, o.rgb ? "" : o.name);
    const swatches = (opts, cur, path, extra) => h("div", { cls: "swatches" }, opts.map(o => pick(o, cur, path, extra)));
    const choice = (label, ...kids) => h("div", { cls: "dzrow" }, h("span", { cls: "dzl" }, label), ...kids);
    const tab = (on, label, fn, dis) => h("button", { type: "button", cls: "pbtn" + (on ? "" : " off"), disabled: !!dis, on: { click: fn } }, label);
    const storeyName = f => f === 0 ? "Ground" : "Floor " + f;
    const storeys = () => h("div", { cls: "prow" }, "Storey", ...mine.map(f => tab(P.storey === f, storeyName(f), () => { P.storey = f; P.sel = null; draw(); })));
    body.append(purseLine(), h("div", { cls: "prow" }, tab(P.tab === "outside", "Outside", () => { P.tab = "outside"; draw(); }, !own.whole), tab(P.tab === "inside", "Inside", () => { P.tab = "inside"; draw(); }),
      furnishes ? tab(P.tab === "furnish", "Furnish", () => { P.tab = "furnish"; draw(); }) : null));
    if (!own.whole) body.append(h("p", { cls: "dim" }, "You own part of this building: the outside needs all of it."));
    if (P.tab === "outside") {
      const cap = f => FN[f][0].toUpperCase() + FN[f].slice(1) + (f === d.shell.side ? " (front)" : "");
      body.append(h("div", { cls: "prow" }, "Walls", ...["*"].concat(BF).map(f => tab(P.facade === f, f === "*" ? "All sides" : cap(f), () => { P.facade = f; draw(); }))));
      const fa = d.walls[P.facade === "*" ? d.shell.side : P.facade], w = "walls." + P.facade;
      body.append(choice("Plaster", swatches(menu["walls.*.plaster"], fa.plaster, w + ".plaster")));
      body.append(choice("Stone or brick", swatches(menu["walls.*.stone"], fa.stone.pattern,
        o => Object.assign({ [w + ".stone"]: { pattern: o.id, color: o.colors.find(c => c !== null) ?? null } }, o.brick_to_eaves ? {} : { [w + ".brick_to_eaves"]: false }))));
      const so = menu["walls.*.stone"].find(o => o.id === fa.stone.pattern);
      if (so && so.colors.length > 1) body.append(choice("Its colour", swatches(so.colors.map((c, k) => ({ id: c, name: c === null ? "The town's stone" : so.name + " " + (k + 1), rgb: c === null ? [.55, .52, .47] : pats[so.id][k] })), fa.stone.color, w + ".stone.color")));
      if (so && so.brick_to_eaves) body.append(choice("Brick to the eaves", swatches([{ id: true, name: "Yes" }, { id: false, name: "No" }], fa.brick_to_eaves, w + ".brick_to_eaves")));
      body.append(choice("Roof tiles", swatches(menu["roof.tiles"], d.roof.tiles, "roof.tiles")), choice("Window frames", swatches(menu["openings.frame"], d.openings.frame, "openings.frame")));
      if (menu["openings.shutters"]) {
        body.append(choice("Shutters", swatches(menu["openings.shutters"], d.openings.shutters, "openings.shutters")));
        if (d.openings.shutters) body.append(choice("Shutter colour", swatches(menu["openings.shutter_color"], d.openings.shutter_color, "openings.shutter_color")));
      }
      for (const [p, label] of [["openings.balcony", "Balcony"], ["openings.keystones", "Keystones"], ["ornament.quoins", "Corner stones"], ["ornament.lantern", "Wall lantern"]])
        if (menu[p]) body.append(choice(label, swatches(menu[p], Builder.get(d, p), p)));
    } else if (P.tab === "inside") {
      body.append(storeys());
      const f = P.storey, put = key => o => { const a = d.inside[key].slice(); a[f] = o.id; return { ["inside." + key]: a }; };
      const mix = c => c.map((v, k) => v + (K.inner_mix.with[k] - v) * K.inner_mix.share);
      body.append(choice("Walls", swatches(menu["inside.walls"].map(o => Object.assign({}, o, { rgb: mix(o.rgb) })), d.inside.walls[f], put("walls"))),
        choice("Floor", h("div", { cls: "swatches" }, menu["inside.floors"].map(o => pick(o, d.inside.floors[f], put("floors"), o.mat === 15 ? " boards" : " tiles")))));
      if (!P.plan[f]) P.plan[f] = P.plan0[f] = planNow(l, f);   // the room plan (BUILD-021, BUILD-022)
      const plan = P.plan[f], w = plan.find(x => x.id === P.wsel), Rp = FW.roomFor(l, f);
      body.append(h("h3", {}, "Rooms"), h("p", { cls: "dim" }, "Click an inner wall to pick it, drag it to move it either way: its ends catch on the walls they meet. Fit between walls stretches it from wall to wall. Every room must be reached from the way in, and the stairs kept clear."));
      if (w) {
        const opts = [{ id: null, name: "As the storey" }].concat(menu["inside.walls"].map(o => Object.assign({}, o, { rgb: mix(o.rgb) })));
        body.append(h("div", { cls: "prow" }, h("b", {}, "Wall, " + (w.a1 - w.a0).toFixed(2) + " m"),
          tab(false, w.door === null ? "Add a doorway" : "Close the doorway", () => editWall(P, w.id, { door: w.door === null ? gridded((w.a0 + w.a1) / 2) : null })),
          tab(false, "Fit between walls", () => editWall(P, w.id, fitWall(w, plan, Rp))),
          tab(false, "Longer", () => editWall(P, w.id, grow(w, .5, Rp))), tab(false, "Shorter", () => editWall(P, w.id, grow(w, -.5, Rp)), w.a1 - w.a0 < 1.1),
          tab(false, "Take down", () => editWall(P, w.id, null)), tab(false, "Put down", () => { P.wsel = null; draw(); })),
          choice("Its colour", h("div", { cls: "swatches" }, opts.map(o => h("button", { type: "button", cls: "sw" + (o.rgb ? "" : " word") + (w.colour === o.id ? " on" : ""), title: o.name, "aria-label": o.name,
            style: o.rgb ? "background-color:" + css(o.rgb) : "", on: { click: () => editWall(P, w.id, { colour: o.id }) } }, o.rgb ? "" : o.name)))));
      }
      body.append(h("div", { cls: "prow" }, tab(false, "Add a wall east to west", () => addWall(P, "x", Rp)), tab(false, "Add a wall north to south", () => addWall(P, "z", Rp))));
    } else {
      body.append(storeys());
      const f = P.storey, lay = P.lay[f], R = FW.roomFor(l, f), sel = lay.find(p => p.id === P.sel);
      body.append(h("p", { cls: "dim" }, "Click a piece to pick it, drag it across the floor to move it. R turns it (Shift+R the other way), Delete puts it with your belongings."));
      if (sel) body.append(h("div", { cls: "prow" }, h("b", {}, sel.name), tab(false, "Turn left", () => editPiece(P, sel.id, { yaw: sel.yaw + FR.turn_deg })), tab(false, "Turn right", () => editPiece(P, sel.id, { yaw: sel.yaw - FR.turn_deg })),
        tab(false, "To your belongings", () => editPiece(P, sel.id, null)), tab(false, "Put down", () => { P.sel = null; draw(); })));
      const held = heldNow(P);
      if (held.length) body.append(h("h3", {}, "From your belongings"), tbl(held.map(b => [b.name, h("span", { cls: "dim" }, b.w.toFixed(2) + " × " + b.d.toFixed(2) + " m"),
        h("button", { type: "button", cls: "pbtn", on: { click: () => placeHeld(P, b, R) } }, "Place")])));
      body.append(h("h3", {}, "Add a piece"), h("div", { cls: "prow" }, ...Object.keys(FR.rooms).map(r => tab(P.room === r, r[0].toUpperCase() + r.slice(1), () => { P.room = r; draw(); }))),
        choice("Line", h("div", { cls: "swatches" }, FR.lines.map(x => h("button", { type: "button", cls: "sw word" + (P.line === x ? " on" : ""), on: { click: () => { P.line = x; draw(); } } }, (Outlet.LINES[x] || {}).label || x)))),
        BuyMenu.grid({ id: "furnish", verb: "Add", fmt, width: () => el("life-body").clientWidth, redraw: draw, buy: e => addPiece(P, e.id, R),   // the pieces as objects in the chosen line, small cells (UI-020)
          things: FR.rooms[P.room].map(k => ({ id: k, name: FR.pieces[k].name, price: FR.pieces[k].price, goes: "place", left: null, state: "ok", size: FR.pieces[k].size, line: P.line })) }));
    }
    const [rt, rl] = Builder.cost(l, l.dsg, d); let total = rt; const lines = rl.slice(), probs = d === l.dsg ? [] : Builder.check(l, l.dsg, d, own, null);
    const colours = menu["inside.walls"].map(o => o.id), tag = f => mine.length > 1 ? " (" + storeyName(+f).toLowerCase() + ")" : "";
    for (const f of Object.keys(P.plan)) { const b = P.plan0[f], a = P.plan[f]; if (a === b) continue;   // the room plans (BUILD-021 to BUILD-023)
      const R = FW.roomFor(l, +f), ent = R.entries.length ? R.entries : [[(R.room[0] + R.room[2]) / 2, (R.room[1] + R.room[3]) / 2]], [t, ls] = RoomPlan.cost(b, a, R.ceiling); total += t;
      for (const x of ls) lines.push([x[0] + tag(f), x[1], x[2]]);
      for (const m of RoomPlan.check(R.room, R.keep, ent, R.ceiling, colours, b, a, null)) probs.push(m + tag(f)); }
    for (const f of Object.keys(P.lay)) { const b = P.lay0[f], a = P.lay[f]; if (a === b) continue;
      const pool = poolOf(P), [t, ls] = Furnish.cost(b, a, pool), R = FW.roomFor(l, +f); total += t;
      for (const x of ls) lines.push([x[0] + (mine.length > 1 ? " (" + storeyName(+f).toLowerCase() + ")" : ""), x[1], x[2]]);
      for (const m of Furnish.check(R.room, R.keep, R.ceiling, b, a, null, pool)) probs.push(m + (mine.length > 1 ? " (" + storeyName(+f).toLowerCase() + ")" : "")); }
    if (lines.length && total > funds()) probs.push("it costs " + fmt(total) + " and you have " + fmt(funds()));
    body.append(h("h3", {}, "The work"), lines.length
      ? tbl(lines.map(([w, q, c]) => [w, h("span", { cls: "dim" }, q === 1 ? "" : Number.isInteger(q) && !/wall/i.test(w) ? "× " + q : q + " m²"), money(c)]).concat([[h("b", {}, "Total"), "", money(total)]]))
      : h("p", { cls: "dim" }, "Pick something to change: it shows on the building at once. Nothing is paid until you build."));
    if (probs.length) body.append(h("p", { cls: "neg" }, probs.map(x => x[0].toUpperCase() + x.slice(1)).join(". ") + "."));
    body.append(h("div", { cls: "prow" },
      h("button", { type: "button", cls: "pbtn", disabled: !lines.length || probs.length > 0, on: { click: () => act(() => {
        if (probs.length) throw new Econ.Refused(probs[0]);
        if (rt > 0) E.spend(rt, "sys:builders", "Remodel: " + l.name);
        if (total - rt > 0) E.spend(total - rt, "sys:builders", "Furniture: " + l.name);
        if (rl.length) remodels[l.id] = P.draft; l.dsg = P.draft;
        belongings = heldNow(P);
        let walled = false; for (const f of Object.keys(P.plan)) if (P.plan[f] !== P.plan0[f]) { (plans[l.id] = plans[l.id] || {})[f] = P.plan[f]; P.plan0[f] = P.plan[f]; FW.showPlan(l, +f, P.plan[f]); walled = true; }
        if (walled) Storeys.resolid(l);                  // the new walls are solid, the old ones gone
        for (const f of Object.keys(P.lay)) if (P.lay[f] !== P.lay0[f]) { (furnishings[l.id] = furnishings[l.id] || {})[f] = P.lay[f]; P.lay0[f] = P.lay[f]; FW.showLayout(l, +f, P.lay[f]); FW.layoutWorld(l, +f); }
        P.undo = [];
        return "Done: " + l.name + (total ? ", for " + fmt(total) : "") + ".";
      }) } }, lines.length ? (total ? "Pay " + fmt(total) + " and build" : "Keep it like this") : "Pay and build"),
      tab(false, "Undo", () => { const u = P.undo.pop(); P.draft = u.d; P.lay = u.lay; P.plan = u.plan || P.plan; showAll(); draw(); }, !P.undo.length),
      tab(false, "Start again", () => { P.undo.push(snap()); P.draft = l.dsg; P.lay = Object.assign({}, P.lay0); P.plan = Object.assign({}, P.plan0); showAll(); draw(); }, d === l.dsg && Object.keys(P.lay).every(f => P.lay[f] === P.lay0[f]) && Object.keys(P.plan).every(f => P.plan[f] === P.plan0[f]))));
    body.append(h("p", { cls: "dim" }, "Drag the view to look round; right drag, middle drag or Shift+drag slides it; the wheel comes nearer where the mouse is; double click looks at a spot; W A S D slide, Q and E turn."));
    syncView(P);
    return [l.name, own.whole ? "Remodel · Esc to leave it as it was" : "Your rooms · Esc to leave them as they were"];
  }
  // ---------------------------------------------------------------- the designer's view and the furniture in it (BUILD-020; FURN-051, FURN-052, FURN-055)
  const canFurnish = l => !!l.furn && !l.house && l.kind !== "furniture";
  function layNow(l, f) { return (furnishings[l.id] && furnishings[l.id][f]) || window.wasted.furn.townLayout(l, f); }   // the storey as it is now
  // The view (BUILD-020; play.js designCam): goal is where the camera is going, cur where it is. It starts at the player's own
  // camera and glides up to frame the building from the front at three-quarters; each tab frames what it shows.
  function startView(l) {
    const side = l.design ? l.design.shell.side : l.furn ? l.furn[0] : "s", out = DOOR_DIR[side] || [0, 1];
    const yaw = Math.atan2(-out[0], out[1]) + .62, back = { yaw: cam.yaw, pitch: cam.pitch, dist: Math.max(2, Math.hypot(cam.eye[0] - pl.x, cam.eye[1] - pl.y - 1.5, cam.eye[2] - pl.z)) };
    const cur = view && view.leave ? view.cur : { yaw: back.yaw, pitch: back.pitch, dist: back.dist, tx: pl.x, ty: pl.y + 1.5, tz: pl.z, fov: .9 };
    view = { lot: l, goal: Object.assign({}, cur, { yaw, fov: window.wasted.DESIGN_FOV }), cur, back: view && view.leave ? view.back : back, storey: 0, cut: null, key: "" };
    document.body.classList.add("designing");
  }
  function frame3(P) {                                    // what the tab shows, framed: the whole building, or the storey cut open
    const l = P.lot, [x0, z0, x1, z1] = l.rect, g = view.goal;
    if (P.tab === "outside") { g.pitch = -.5; Object.assign(g, window.wasted.fitBox([x0, Y_FLOOR, z0, x1, (l.H || 8) + 2.5, z1], g.yaw, g.pitch)); return; }
    const F = Storeys.floorY(P.storey); g.pitch = -.95; Object.assign(g, window.wasted.fitBox([x0 + .3, F, z0 + .3, x1 - .3, F + .6, z1 - .3], g.yaw, g.pitch, .97)); g.ty = F + .3;
  }
  function syncView(P) {                                 // the view follows the tab: the whole building outside; inside, the storey cut open
    if (!view) return; const key = P.tab === "outside" ? "out" : "in" + P.storey; if (view.key === key) return; view.key = key;
    const l = P.lot, [x0, z0, x1, z1] = l.rect;
    if (P.tab === "outside") { view.cut = null; view.storey = 0; }
    else { const F = Storeys.floorY(P.storey); view.storey = P.storey; view.cut = { rect: [x0 - .6, z0 - .6, x1 + .6, z1 + .6], shell: F + 1.25, room: Storeys.ceilY(P.storey) - .05 }; }
    frame3(P);
  }
  function editPiece(P, id, ch, quiet) {                   // move, turn (ch) or take away (null) a piece of the storey in view
    const f = P.storey, lay = P.lay[f]; if (!lay) return;
    if (!quiet) P.undo.push(snapOf(P));
    P.lay = Object.assign({}, P.lay, { [f]: ch ? lay.map(p => p.id === id ? Object.assign({}, p, ch, ch.yaw !== undefined ? { yaw: ((ch.yaw % 360) + 360) % 360 } : {}) : p) : lay.filter(p => p.id !== id) });
    if (!ch) P.sel = null;
    window.wasted.furn.showLayout(P.lot, f, P.lay[f]); if (!quiet) { Sound.act("talk_choose"); draw(); }
  }
  function addPiece(P, kind, R) {                          // a new piece where the view looks, on the grid, inside the room
    const f = P.storey, g = Furnish.rules().grid_m, sn = v => +(Math.round(v / g) * g).toFixed(2), pc = Furnish.rules().pieces[kind];
    const x = sn(clamp(view.goal.tx, R.room[0] + pc.size[0] / 2, R.room[2] - pc.size[0] / 2)), z = sn(clamp(view.goal.tz, R.room[1] + pc.size[1] / 2, R.room[3] - pc.size[1] / 2));
    const here = P.lot.id + ":" + f, ks = P.lay[f].map(p => p.id).concat(poolOf(P).map(b => b.id.split(":").slice(1, 3).join(":") === here ? b.id.split(":")[3] : ""));
    const n = 1 + Math.max(0, ...ks.filter(i => /^k\d+$/.test(i)).map(i => +i.slice(1)));   // (never an id a belonging from this storey has)
    P.undo.push(snapOf(P));
    P.lay = Object.assign({}, P.lay, { [f]: P.lay[f].concat([Furnish.kitPiece(kind, "k" + n, x, z, 0, P.line)]) }); P.sel = "k" + n;
    window.wasted.furn.showLayout(P.lot, f, P.lay[f]); Sound.act("talk_choose"); draw();
  }
  // Belongings in the designer (INV-001): what was taken away from any storey of this draft is held at once, and can be
  // placed on another; what is placed from them is no longer held. pool: everything that may be placed (for the check).
  let designStart = null;
  const fromHere = (P, f) => ({ lot: P.lot.id, level: +f });
  function poolOf(P) { const out = belongings.slice(); for (const f of Object.keys(P.lay)) if (P.lay[f] !== P.lay0[f]) out.push(...Furnish.stow(P.lay0[f], P.lay[f], [], fromHere(P, f))); return out; }
  function heldNow(P) { const placed = new Set(); for (const f of Object.keys(P.lay)) for (const p of P.lay[f]) placed.add(p.id); return poolOf(P).filter(b => !placed.has(b.id)); }
  function placeHeld(P, b, R) {                          // a piece from the belongings where the view looks, on the grid, inside the room
    const f = P.storey, g = Furnish.rules().grid_m, sn = v => +(Math.round(v / g) * g).toFixed(2);
    const x = sn(clamp(view.goal.tx, R.room[0] + b.w / 2, R.room[2] - b.w / 2)), z = sn(clamp(view.goal.tz, R.room[1] + b.d / 2, R.room[3] - b.d / 2));
    P.undo.push(snapOf(P));
    P.lay = Object.assign({}, P.lay, { [f]: P.lay[f].concat([Object.assign({}, b, { x, z, yaw: 0 })]) }); P.sel = b.id;
    window.wasted.furn.showLayout(P.lot, f, P.lay[f]); Sound.act("talk_choose"); draw();
  }
  const snapOf = P => ({ d: P.draft, lay: Object.assign({}, P.lay), plan: Object.assign({}, P.plan) });
  function planNow(l, f) { return (plans[l.id] && plans[l.id][f]) || Storeys.planOf(l, f); }   // a storey's walls as they are now
  const gridded = v => { const g = RoomPlan.rules().grid_m; return +(Math.round(v / g) * g).toFixed(2); };
  const gridUp = v => { const g = RoomPlan.rules().grid_m; return +(Math.ceil(v / g - 1e-6) * g).toFixed(2); }, gridDown = v => { const g = RoomPlan.rules().grid_m; return +(Math.floor(v / g + 1e-6) * g).toFixed(2); };
  function grow(w, d, R) {                               // a wall half a metre longer (or shorter) at each end, inside the room
    const [lo, hi] = w.ax === "x" ? [R.room[0], R.room[2]] : [R.room[1], R.room[3]];
    return { a0: gridded(Math.max(lo, w.a0 - d / 2)), a1: gridded(Math.min(hi, w.a1 + d / 2)) };
  }
  function wallStops(plan, w, fix, lo, hi) {            // where a wall's ends can stop on its line: the room's sides and every wall across it
    const out = [lo, hi];
    for (const q of plan) if (q.id !== w.id && q.ax !== w.ax && q.a0 - .01 <= fix && fix <= q.a1 + .01) out.push(q.fix);
    return out.sort((a, b) => a - b);
  }
  function fitWall(w, plan, R) {                          // a wall stretched to the nearest wall (or side of the room) at each end
    const [lo, hi] = w.ax === "x" ? [R.room[0], R.room[2]] : [R.room[1], R.room[3]], c = (w.a0 + w.a1) / 2, st = wallStops(plan, w, w.fix, lo, hi);
    const a0 = gridUp(Math.max(...st.filter(v => v < c - .3))), a1 = gridDown(Math.min(...st.filter(v => v > c + .3)));   // (on the grid, never past the stop)
    const D = RoomPlan.rules(), door = w.door === null ? null : gridded(clamp(w.door, a0 + D.door_w / 2 + D.door_margin, a1 - D.door_w / 2 - D.door_margin));
    return { a0, a1, door };
  }
  function editWall(P, id, ch, quiet) {                  // change (ch) or take down (null) a wall of the storey in view
    const f = P.storey, plan = P.plan[f]; if (!plan) return;
    if (!quiet) P.undo.push(snapOf(P));
    P.plan = Object.assign({}, P.plan, { [f]: ch ? plan.map(w => w.id === id ? Object.assign({}, w, ch) : w) : plan.filter(w => w.id !== id) });
    if (!ch) P.wsel = null;
    window.wasted.furn.showPlan(P.lot, f, P.plan[f]); if (!quiet) { Sound.act("talk_choose"); draw(); }
  }
  function addWall(P, ax, R) {                           // a new 3 m wall where the view looks
    const f = P.storey, plan = P.plan[f], g = view.goal, [lo, hi, flo, fhi] = ax === "x" ? [R.room[0], R.room[2], R.room[1], R.room[3]] : [R.room[1], R.room[3], R.room[0], R.room[2]];
    const c = ax === "x" ? g.tx : g.tz, fix = gridded(clamp(ax === "x" ? g.tz : g.tx, flo + .5, fhi - .5)), a0 = gridded(clamp(c - 1.5, lo, hi - 3)), a1 = gridded(Math.min(hi, a0 + 3));
    const n = 1 + Math.max(0, ...plan.map(w => /^w\d+$/.test(w.id) ? +w.id.slice(1) : 0));
    P.undo.push(snapOf(P));
    P.plan = Object.assign({}, P.plan, { [f]: plan.concat([{ id: "w" + n, ax, fix, a0, a1, door: null, colour: null }]) }); P.wsel = "w" + n;
    window.wasted.furn.showPlan(P.lot, f, P.plan[f]); Sound.act("talk_choose"); draw();
  }
  let grab = null;
  const canvas = () => document.getElementById("gl");
  function pointer(type, e, held) {                      // the mouse over the picture while the designer is open (play.js)
    const P = panel; if (!view || !P) return;
    const g = view.goal;
    const [lx0, lz0, lx1, lz1] = P.lot.rect, R2 = 20;      // the view goes 20 m beyond the building at most
    const ground = () => view.cut ? Storeys.floorY(view.storey) : Y_FLOOR;
    const hitAt = ev => { const ray = window.wasted.rayAt(ev.clientX, ev.clientY); if (!ray || ray.d[1] > -1e-4) return null; const t = (ground() - ray.o[1]) / ray.d[1]; return t > 0 ? [ray.o[0] + ray.d[0] * t, ray.o[2] + ray.d[2] * t] : null; };
    const aim = (x, z) => { g.tx = clamp(x, lx0 - R2, lx1 + R2); g.tz = clamp(z, lz0 - R2, lz1 + R2); };
    if (type === "wheel") {                              // nearer or further, towards the point under the mouse
      if (!(e.target && e.target.tagName === "CANVAS")) return;
      const f = e.deltaY > 0 ? 1.15 : 1 / 1.15, d = clamp(g.dist * f, 2, 160), k = d / g.dist, p = hitAt(e);
      if (p && p[0] > lx0 - R2 && p[0] < lx1 + R2 && p[1] > lz0 - R2 && p[1] < lz1 + R2) aim(p[0] + (g.tx - p[0]) * k, p[1] + (g.tz - p[1]) * k); g.dist = d; return;   // (not towards the far distance)
    }
    if (type === "dbl") { const p = hitAt(e); if (p) aim(p[0], p[1]); return; }   // double click: look at that spot
    const FW = window.wasted.furn, fur = P.tab === "furnish" && P.lay[P.storey];
    const floorHit = (ray, F) => { if (ray.d[1] > -1e-4) return null; const t = (F - ray.o[1]) / ray.d[1]; return t > 0 ? [ray.o[0] + ray.d[0] * t, ray.o[2] + ray.d[2] * t] : null; };
    const under = () => { const R = FW.roomFor(P.lot, P.storey), ray = window.wasted.rayAt(e.clientX, e.clientY); return ray ? { R, ray, p: FW.pickPiece(P.lay[P.storey], R.F, ray) } : null; };
    if (type === "down") {
      grab = null;
      if (e.button === 2 || e.button === 1 || (e.button === 0 && e.shiftKey)) { grab = { pan: true }; return; }   // the right or middle button, or Shift, slides the view
      if (P.tab === "inside" && e.button === 0 && P.plan[P.storey]) {   // an inner wall: pick it, and drag it across the floor
        const R = FW.roomFor(P.lot, P.storey), ray = window.wasted.rayAt(e.clientX, e.clientY); if (!ray) return;
        const w = FW.pickWall(P.lot, P.storey, P.plan[P.storey], ray), hit = floorHit(ray, R.F), was = P.wsel; P.wsel = w ? w.id : null;
        grab = w && hit ? { wall: w.id, off: w.fix - (w.ax === "x" ? hit[1] : hit[0]), offA: w.a0 - (w.ax === "x" ? hit[0] : hit[1]), F: R.F, R, moved: false } : { turn: true };
        canvas().style.cursor = w ? "grabbing" : "default"; if (was !== P.wsel) draw(); return;
      }
      if (!fur || e.button !== 0) { grab = { turn: true }; return; }
      const u = under(); if (!u) return; const hit = floorHit(u.ray, u.R.F);
      const was = P.sel; P.sel = u.p ? u.p.id : null;
      grab = u.p && hit ? { id: u.p.id, dx: u.p.x - hit[0], dz: u.p.z - hit[1], F: u.R.F, moved: false } : { turn: true };
      canvas().style.cursor = u.p ? "grabbing" : "default";
      if (was !== P.sel) draw(); return;
    }
    if (type === "move") {
      if (!held || !grab) {                                // hovering: the piece under the mouse is outlined, and the mouse says it can be moved
        if (P.tab === "inside" && P.plan[P.storey]) { const ray = window.wasted.rayAt(e.clientX, e.clientY), w = ray && FW.pickWall(P.lot, P.storey, P.plan[P.storey], ray); P.whover = w ? w.id : null; canvas().style.cursor = w ? "grab" : "default"; return; }
        if (fur) { const u = under(), id = u && u.p ? u.p.id : null; P.hover = id; canvas().style.cursor = id ? "grab" : "default"; }
        return;
      }
      if (grab.wall) {                                     // a wall follows the mouse, across and along, on the grid, inside the room; its ends catch on walls
        const ray = window.wasted.rayAt(e.clientX, e.clientY), hit = ray && floorHit(ray, grab.F); if (!hit) return;
        const plan = P.plan[P.storey], w = plan.find(q => q.id === grab.wall); if (!w) return;
        const x = w.ax === "x", [flo, fhi] = x ? [grab.R.room[1], grab.R.room[3]] : [grab.R.room[0], grab.R.room[2]], [lo, hi] = x ? [grab.R.room[0], grab.R.room[2]] : [grab.R.room[1], grab.R.room[3]];
        const fix = gridded(clamp((x ? hit[1] : hit[0]) + grab.off, flo + .2, fhi - .2)), len = w.a1 - w.a0;
        let a0 = gridded(clamp((x ? hit[0] : hit[1]) + grab.offA, lo, hi - len));
        const stops = wallStops(plan, w, fix, lo, hi), SNAP = .3;
        const s0 = stops.find(c => Math.abs(a0 - c) < SNAP), s1 = stops.find(c => Math.abs(a0 + len - c) < SNAP);
        if (s0 !== undefined) a0 = gridUp(s0); else if (s1 !== undefined) a0 = gridDown(s1) - len;
        a0 = gridded(clamp(a0, gridUp(lo), gridDown(hi) - len));
        const a1 = gridded(a0 + len), d = a0 - w.a0;
        if (fix === w.fix && a0 === w.a0) return; if (!grab.moved) { P.undo.push(snapOf(P)); grab.moved = true; }
        editWall(P, w.id, { fix, a0, a1, door: w.door === null ? null : gridded(w.door + d) }, true); return;
      }
      if (grab.id) {
        const ray = window.wasted.rayAt(e.clientX, e.clientY), hit = ray && floorHit(ray, grab.F); if (!hit) return;
        const gm = Furnish.rules().grid_m, sn = v => +(Math.round(v / gm) * gm).toFixed(2), p = P.lay[P.storey].find(q => q.id === grab.id); if (!p) return;
        const x = sn(hit[0] + grab.dx), z = sn(hit[1] + grab.dz); if (x === p.x && z === p.z) return;
        if (!grab.moved) { P.undo.push(snapOf(P)); grab.moved = true; }
        editPiece(P, grab.id, { x, z }, true);
      } else if (grab.pan) {                               // the right button slides the view across the ground
        const k = g.dist * .0016, sy = Math.sin(g.yaw), cy = Math.cos(g.yaw), [x0, z0, x1, z1] = P.lot.rect;
        aim(g.tx - (cy * e.movementX - sy * e.movementY) * k, g.tz - (sy * e.movementX + cy * e.movementY) * k);
      } else { g.yaw += e.movementX * .006; g.pitch = clamp(g.pitch - e.movementY * .005, -1.55, -.04); }
      return;
    }
    if (type === "up") { if (grab && grab.moved) draw(); grab = null; if (fur) canvas().style.cursor = P.hover ? "grab" : "default"; }
  }
  function storeyBad(P) {                                // the pieces of the storey in view that its check finds wrong (for the red outline)
    const f = P.storey, a = P.lay[f], b = P.lay0[f]; if (!a || a === b) return new Set();
    if (P.badFor === a) return P.bad; const R = window.wasted.furn.roomFor(P.lot, f), old = new Set(Furnish.problems(R.room, R.keep, R.ceiling, b).map(q => q[0]));
    P.bad = new Set(Furnish.problems(R.room, R.keep, R.ceiling, a).filter(q => !old.has(q[0])).flatMap(q => q[0].split("|").slice(0, -1))); P.badFor = a; return P.bad;
  }
  function designMarks() {                               // for play.js: outlines round the piece picked and the piece under the mouse
    const P = panel; if (!view || !P) return [];
    if (P.tab === "inside" && P.plan && P.plan[P.storey]) {   // and round the wall picked and the wall under the mouse
      const F = Storeys.floorY(P.storey), out = [];
      for (const w of P.plan[P.storey]) if (w.id === P.wsel || (w.id === P.whover && !grab)) { const b = RoomPlan.box(w); out.push({ p: { x: (b[0] + b[2]) / 2, z: (b[1] + b[3]) / 2, w: b[2] - b[0], d: b[3] - b[1], h: 1.2, yaw: 0 }, F, sel: w.id === P.wsel, bad: false }); }
      return out;
    }
    if (P.tab !== "furnish" || !P.lay[P.storey]) return [];
    const lay = P.lay[P.storey], F = Storeys.floorY(P.storey), bad = storeyBad(P), out = [];
    for (const p of lay) if (p.id === P.sel || (p.id === P.hover && !grab)) out.push({ p, F, sel: p.id === P.sel, bad: bad.has(p.id) });
    for (const p of lay) if (p.id !== P.sel && bad.has(p.id)) out.push({ p, F, sel: false, bad: true });
    return out;
  }
  function designTags() {                                // for play.js: the name of the piece picked, over it
    const P = panel; if (!view || !P || P.tab !== "furnish" || !P.lay[P.storey]) return [];
    const p = P.lay[P.storey].find(q => q.id === (P.sel || P.hover)); if (!p) return [];
    return [{ x: p.x, y: Storeys.floorY(P.storey) + Math.min(p.h, 2) + .35, z: p.z, text: p.name + (storeyBad(P).has(p.id) ? " · can't stand here" : "") }];
  }
  function designKey(e) {                                // R turns the piece picked (Shift+R the other way), Delete takes it away
    const P = panel; if (!P || P.kind !== "remodel" || P.tab !== "furnish" || !P.sel) return false;
    const p = (P.lay[P.storey] || []).find(q => q.id === P.sel); if (!p) return false;
    if (e.code === "KeyR") { editPiece(P, p.id, { yaw: p.yaw + (e.shiftKey ? -1 : 1) * Furnish.rules().turn_deg }); return true; }
    if (e.code === "Delete" || e.code === "Backspace") { editPiece(P, p.id, null); return true; }
    return false;
  }
  const DLR = rid => { const r = D.residents[resIdx[rid]]; return D.dialogue.role_words[D.roles[r.r]] || D.roles[r.r]; };
  function fillJob(text, o) { const v = jobVars(o); return text.replace(/\{(\w+)\}/g, (_, k) => k in v ? v[k] : "{" + k + "}"); }
  function where(goal) {                                  // QST-005: where the person to see is right now
    if (goal.talk) {
      const i = resIdx[goal.talk], r = Talk.where(i, clock.m), n = D.residents[i].n;
      return h("p", { cls: "dim" }, !r ? n + " is away from the streets just now (by tram or car)." : r.lot ? n + " is at " + r.lot.name + " now." : n + " is out and about" + (r.walking ? ", walking." : "."));
    }
    if (goal.go) return h("p", { cls: "dim" }, "Go to " + lotName(E.lotIndex[goal.go]) + ".");
    if (goal.near && goal.from !== undefined) return h("p", { cls: "dim" }, "Between " + hhmm(goal.from) + " and " + hhmm(goal.to) + ". It is " + hhmm(clock.m) + ".");
    return null;
  }
  function key(e) {
    if (designKey(e)) { e.preventDefault(); return; }
    if (panel && (panel.kind === "shop" || panel.kind === "outlet") && BuyMenu.key(e)) { e.preventDefault(); return; }   // arrows choose a thing, Enter buys it (UI-020)
    const typing = /^(INPUT|TEXTAREA|SELECT)$/.test((document.activeElement || {}).tagName) && (document.activeElement.type || "text") === "text";
    if (typing && e.code !== "Escape") return;
    const tab = { KeyC: "me", KeyI: "wallet", KeyG: "belongings", KeyP: "property", KeyJ: "quests", KeyU: "wardrobe", KeyH: "help", KeyM: "map" }[e.code];
    if (tab && panel && (panel.menu || ["property", "wallet", "quests"].includes(panel.kind))) { e.preventDefault(); menu(tab); return; }
    if (e.code === "Escape" || (panel && ((e.code === "KeyB" && ["shop", "bank", "stall", "agent"].includes(panel.kind)) || (e.code === "KeyP" && panel.kind === "property") || (e.code === "KeyI" && panel.kind === "wallet") || (e.code === "KeyJ" && panel.kind === "quests")))) { e.preventDefault(); close(); }
  }

  // ---------------------------------------------------------------- each frame (play.js)
  const ctx = () => ({ lot: (() => { const n = lotHere(); return n !== null && n >= 0 ? LOTS[n].id : null; })(), pos: [pl.x, pl.z], minute: clock.m });
  function frame(dt) {
    if (!ready) { if (started && !designing) { transient = true; newGame(Object.assign({}, DG.default)); applyLook(); } else return; }   // ?auto=1 starts without the designer and saves nothing
    if (clock.day < E.day || (clock.day === E.day && clock.m < E.minute)) { clock.day = E.day; clock.m = E.minute; }   // the clock only goes forward
    E.advance(clock.day, clock.m);
    rehome();                                             // residents who moved house live at the new home (PROP-022)
    if (performance.now() - checkT > 250) { checkT = performance.now(); Q.check(ctx()); for (const id of Achievements.award(Achievements.facts(E, Q, M), feats, clock.day)) Rewards.achieve(Achievements.get(id)); }   // four times a second, however slow the frames
    const tm = E.totalMoney(), delta = d => { el("purse-delta").textContent = (d > 0 ? "+" : "") + fmt(d); el("purse-delta").className = d > 0 ? "pos" : "neg"; el("purse-delta").style.opacity = 1; setTimeout(() => el("purse-delta").style.opacity = 0, 2200); };
    const shown = Rewards.frame(dt, E.L.tx, pl, delta);   // rewards and interest: coins first, their "+" when the last one lands (SND-019, SND-020)
    if (lastMoney !== null && tm !== lastMoney) { if (tm - lastMoney !== shown) delta(tm - lastMoney - shown); if (panel) draw(); }
    lastMoney = tm;
    if ((toastT -= dt) <= 0) el("toast").style.opacity = 0;
    hud();
    if (performance.now() - lastSave > 15000) save();
  }
  function hud() {
    el("purse").style.display = "block";
    el("purse-cash").textContent = fmt(E.L.balance("p:cash") - Rewards.held.cash);   // coins still on their way are not counted yet (rewards.js)
    el("purse-bank").textContent = E.accounts.length ? " · bank " + fmt(E.L.balance("p:current") + E.L.balance("p:savings") - Rewards.held.bank) : "";
    const tr = el("track"), lines = [];
    for (const qid of Object.keys(Q.active).slice(0, 3)) lines.push([QR.quests[qid].title, Q.stage(qid).log]);
    if (Q.job) { const left = Q.job.deadline - (clock.day * 1440 + clock.m); lines.push([QR.jobs.kinds[Q.job.kind].title + " · " + Math.floor(left / 60) + "h " + String(Math.floor(left % 60)).padStart(2, "0") + "m left", fillJob(QR.jobs.kinds[Q.job.kind].log, Q.job)]); }
    const key = JSON.stringify(lines);
    if (tr.dataset.k !== key) { tr.dataset.k = key; tr.textContent = ""; for (const [a, b] of lines) tr.append(h("div", { cls: "trk" }, h("b", {}, a), h("span", {}, b))); }
    tr.style.display = lines.length && !Talk.on ? "block" : "none";
    const n = lotHere(), hint = el("counter");
    const oh = outletHere();                              // in a mall each outlet has its own counter (WLD-066); out on its walks there is none
    const what = oh ? "Buy at " + oh.r.name : n === null || n < 0 ? null : n === E.bankLot ? "Bank counter" : n === E.marketLot ? "Your market stall" : LOTS[n] && M.office[LOTS[n].id] ? "Estate agent: " + M.cos[M.office[LOTS[n].id]].name : E.shopKind(n) && !E.outlets[n] ? "Buy and sell at " + lotName(n) : null;
    const bt = !what && n !== null && n >= 0 && M.byLot.has(n) ? boardText(n) : null;   // PROP-023: what is on the market here
    if (what && !panel && !Talk.on && pl.pose !== 4) { hint.style.display = "block"; hint.innerHTML = "<kbd>B</kbd>" + what; }
    else if (bt && !panel && !Talk.on && pl.pose !== 4) { hint.style.display = "block"; hint.innerHTML = "<kbd>P</kbd>" + bt + ": see the prices"; }
    else hint.style.display = "none";
  }
  function targets() {                                    // residents to see and places to go, for markers (QST-005)
    const out = [];
    if (!ready) return out;
    for (const qid in Q.active) {
      const g = Q.stage(qid).goal;
      if (g.talk) out.push({ i: resIdx[g.talk] });
      else if (g.go) out.push({ lot: E.lotIndex[g.go] });
      else if (g.near) out.push({ x: g.near[0], z: g.near[1] });
      else if (g.bank) out.push({ lot: E.bankLot });
      else if (g.stall || g.stall_sold) out.push({ lot: E.marketLot });
    }
    if (Q.job) { const j = Q.job; if (j.kind === "parcel") out.push({ lot: j.step === "pickup" ? j.frm : j.to }); else if (j.kind === "shift") out.push({ lot: j.to }); else out.push({ i: j.giver }); }
    for (const qid in QR.quests) { const q = QR.quests[qid]; if (q.giver && Q.offers(q.giver).includes(qid)) out.push({ i: resIdx[q.giver], offer: true }); }
    return out;
  }
  let markT = 0, marked = new Map();
  function mark(i, r, d, L, tags) {                       // a marker over the head of someone with something for you
    if (!ready) return;
    if (performance.now() - markT > 500) { markT = performance.now(); marked = new Map(); for (const t of targets()) if (t.i !== undefined) marked.set(t.i, t.offer ? "◆ has something for you" : "◆ quest"); }
    const m = marked.get(i);
    if (m && d < 45) tags.push({ x: r.x, y: r.y + (r.pose === 2 ? 1.25 : r.pose === 3 ? .8 : 2.12) * L.S, z: r.z, d, a: Math.min(1, (45 - d) / 10), size: 15, text: m });
  }
  function marks() {                                      // map positions of the markers
    const out = [];
    const MS = settings ? settings.map : { map_waypoint: true, map_quests: true };   // (before the settings are read: as they start)
    if (waypoint && MS.map_waypoint) out.push([waypoint.x, waypoint.z, 2]);   // the place marked (UI-006, UI-012)
    if (!MS.map_quests) return out;
    for (const t of targets()) {
      if (t.i !== undefined) { const r = Talk.where(t.i, clock.m); if (r) out.push([r.x, r.z, t.offer ? 0 : 1]); }
      else if (t.lot !== undefined) { const l = LOTS[t.lot]; out.push([(l.rect[0] + l.rect[2]) / 2, (l.rect[1] + l.rect[3]) / 2, 1]); }
      else out.push([t.x, t.z, 1]);
    }
    return out;
  }
  addEventListener("beforeunload", save);
  addEventListener("load", () => {
    if (window.wasted) window.wasted.life = { get E() { return E; }, get Q() { return Q; }, get M() { return M; }, get look() { return look; }, selfTest: () => Econ.selfTest(X, LOTS), estateTest: () => Estate.selfTest(D, LOTS), save, newGame, readSave, loadSave, open, close, counter, get remodels() { return remodels; } };
    loadSettings();
    el("keys").innerHTML = '<div><kbd>Esc</kbd> menu <kbd>H</kbd> controls <kbd>E</kbd> use</div>';
    const badB = Builder.selfTest(LOTS);
    if (badB.length) console.error("builder.js does not match Tools/citysim/builder.py:\n  " + badB.join("\n  ")); else console.info("builder.js matches Tools/citysim/builder.py (self-test passed).");
    const badR = RoomPlan.selfTest();
    if (badR.length) console.error("builder.js RoomPlan does not match Tools/citysim/roomplan.py:\n  " + badR.join("\n  ")); else console.info("builder.js RoomPlan matches Tools/citysim/roomplan.py (self-test passed).");
    const badF = Furnish.selfTest(Furniture.SIZE);
    if (badF.length) console.error("builder.js Furnish does not match Tools/citysim/furnish.py:\n  " + badF.join("\n  ")); else console.info("builder.js Furnish matches Tools/citysim/furnish.py (self-test passed).");
    const badE = Estate.selfTest(D, LOTS);
    if (badE.length) console.error("estate.js does not match Tools/citysim/estate.py:\n  " + badE.join("\n  ")); else console.info("estate.js matches Tools/citysim/estate.py (self-test passed).");
    const badA = Achievements.selfTest();
    if (badA.length) console.error("achievements.js does not match Tools/citysim/achievements.py:\n  " + badA.join("\n  ")); else console.info("achievements.js matches Tools/citysim/achievements.py (self-test passed).");
    const badW = Rewards.selfTest();
    if (badW.length) console.error("rewards.js does not match Tools/worldgen/world.py:\n  " + badW.join("\n  ")); else console.info("rewards.js matches Tools/worldgen/world.py (self-test passed).");
    const badM = BuyMenu.selfTest();
    if (badM.length) console.error("buymenu.js does not match Tools/worldgen/world.py:\n  " + badM.join("\n  ")); else console.info("buymenu.js matches Tools/worldgen/world.py (self-test passed).");
    const bad = Econ.selfTest(X, LOTS);
    if (bad.length) console.error("economy.js does not match Tools/citysim/economy.py:\n  " + bad.join("\n  ")); else console.info("economy.js matches Tools/citysim/economy.py (self-test passed).");
  });
  return {
    get on() { return !!panel; }, get live() { return !!live; }, liveCam, get designing() { return designing; }, get ready() { return ready; }, get view() { return view; }, pointer, designMarks, designTags, markAt, waypointTag, get mapSettings() { return settings ? settings.map : null; },
    mine: () => ready && M ? [...new Set(M.owned().map(unitLot)).values()].concat(Object.keys(M.tenancy).map(i => unitLot(+i))) : [], endView: () => { view = null; },
    go, frame, camera, mark, marks, boards, remodelTarget, remodel, conversation, allowed, chose, talkEnded, vars, key, counter, toast,
    panel: k => menu(k), menu: () => menu(), setting, spin: dx => { spin += dx * .006; },
    get paused() { return !!(panel && panel.menu && ST.menu.pauses) || !!(view && !view.leave); },   // the menu, and the building designer, stop the clock
  };
})();
