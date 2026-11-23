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
  let furnishings = {};                                  // the player's arranged storeys: lot id -> storey -> layout (FURN-051), kept in the save
  let view = null;                                       // the building designer's camera while it is open (play.js designCam)
  let E = null, Q = null, M = null, look = null, transient = false, designing = false, panel = null, spin = 0, ready = false, lastSave = 0, toastT = 0, lastMoney = null, checkT = 0;
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
    E = new Econ.Economy(EC, LOTS, visits, staff);
    Q = new Econ.Quests(QR, E, residentsForQuests(), LOTS);
    Q.onNote = text => toast(text);
    M = new Estate.Market(X.estate, E, LOTS, Estate.residentsOf(D, LOTS));   // the real estate market (PROP-009 to PROP-021)
  }
  function newGame(v) {
    fresh(); look = v; clock.day = 0; outfits = []; belongings = []; waypoint = null;
    E.advance(0, clock.m); Q.newGame(); M.start(); ready = true;
    if (new URLSearchParams(location.search).has("rich")) testRich();
    lastMoney = E.totalMoney(); save();
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
  function readSave() { try { const s = localStorage.getItem(SAVE_KEY); return s ? JSON.parse(s) : null; } catch (e) { return null; } }
  function save() {
    if (!ready || transient) return;                      // ?auto=1 (tests, screenshots) never overwrites a real save
    try { localStorage.setItem(SAVE_KEY, JSON.stringify({ v: 1, look, pl: { x: pl.x, y: pl.y, z: pl.z, yaw: pl.yaw }, clock: { day: clock.day, m: clock.m }, econ: E.save(), quests: Q.save(), estate: M.save(), remodels, furnishings, outfits, belongings, at: Date.now() })); } catch (e) { }
    lastSave = performance.now();
  }
  function loadSave(s) {
    fresh(); E.load(s.econ); Q.load(s.quests); if (s.estate) M.load(s.estate); else M.start(); look = s.look;
    Object.assign(pl, s.pl); pl.pose = 0; pl.vy = 0; cam.yaw = pl.yaw; clock.day = s.clock.day; clock.m = s.clock.m;
    ready = true; lastMoney = E.totalMoney();
    remodels = s.remodels || {};
    for (const id of Object.keys(remodels)) { const l = LOTS.find(x => x.id === id); if (l && l.drawn) { l.dsg = remodels[id]; window.wasted.repaint(l, Builder.built(l.dsg)); } }
    outfits = s.outfits || []; belongings = s.belongings || []; waypoint = null;
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
    return { pay: fmt(o.pay), hours: String(o.hours), by: hhmm(dl % 1440), minutes: String(o.minutes || K.minutes || ""), item: o.item ? EC.items[o.item].name.toLowerCase() : "",
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
  function counter(n) {
    if (n === undefined || n === null) n = lotHere();
    if (n === null || n < 0) return toast("No counter here. Shops, cafés, the bank and the market square have one.");
    if (n === E.bankLot) return open("bank", n);
    if (LOTS[n] && M.office[LOTS[n].id]) return open("agent", n);
    if (n === E.marketLot) return open("stall", n);
    if (E.shopKind(n)) return open("shop", n);
    toast("No counter here. Shops, cafés, the bank and the market square have one.");
  }
  function open(kind, n) {
    if (panel && panel.kind === kind) return close();
    panel = { kind, n, msg: "" };
    for (const k in keys) keys[k] = false; pl.moving = false;
    if (document.pointerLockElement && document.exitPointerLock) document.exitPointerLock();
    el("life").classList.toggle("side", kind === "remodel");   // the designer stands to one side, so the building shows
    if (kind === "remodel") startView(LOTS[n]);
    el("life").style.display = "block"; Sound.act("talk_open"); draw();
  }
  function close() {
    if (!panel) return;
    if (panel.kind === "remodel" && panel.lot && panel.draft !== panel.lot.dsg) window.wasted.repaint(panel.lot, Builder.built(panel.lot.dsg));   // not paid for: as it was
    if (panel.kind === "remodel" && panel.lot) for (const f of Object.keys(panel.lay)) if (panel.lay[f] !== panel.lay0[f]) window.wasted.furn.showLayout(panel.lot, +f, (furnishings[panel.lot.id] || {})[f] || null);
    if (view) { view.leave = true; view.cut = null; } grab = null; document.body.classList.remove("designing"); document.getElementById("gl").style.cursor = "";
    const back = panel.back; panel = null; el("life").style.display = "none"; el("life").classList.remove("menu", "side"); Sound.act("talk_close"); save();
    if (back) menu(back);                                 // the designer was opened from the menu: back to it
  }
  function act(fn) {                                      // a button: do it, say what happened, redraw
    try { const r = fn(); panel.msg = typeof r === "string" ? r : ""; Sound.act("talk_choose"); } catch (e) { if (!(e instanceof Econ.Refused)) throw e; panel.msg = "Refused: " + e.message + "."; }
    save(); draw();
  }
  const money = c => h("span", { cls: "money" }, fmt(c));
  const btn = (label, fn, dis) => h("button", { type: "button", cls: "pbtn", disabled: !!dis, on: { click: () => act(fn) } }, label);
  const tbl = rows => h("table", { cls: "ptab" }, h("tbody", {}, rows.map(r => h("tr", {}, r.map(c => h("td", {}, c))))));
  function purseLine() { return h("p", { cls: "purse" }, "Cash ", money(E.L.balance("p:cash")), E.accounts.includes("current") ? [" · Current ", money(E.L.balance("p:current"))] : null, E.accounts.includes("savings") ? [" · Savings ", money(E.L.balance("p:savings"))] : null); }
  function statement(n) {
    const st = E.L.statement(Econ.PLAYER, n);
    const moved = no => E.L.tx[no][4].filter(l => Econ.PLAYER.includes(l[0]) && l[1] > 0).reduce((a, l) => a + l[1], 0);   // between your own accounts: how much moved
    return st.length ? tbl(st.map(([no, day, m, memo, ch]) => ["Day " + (day + 1) + " " + hhmm(m), memo, ch ? h("span", { cls: ch < 0 ? "neg" : "pos" }, (ch > 0 ? "+" : "") + fmt(ch)) : h("span", { cls: "dim" }, fmt(moved(no)))])) : h("p", { cls: "dim" }, "Nothing yet.");
  }
  function draw() {
    if (!panel) return;
    const P = panel, body = el("life-body"); body.textContent = "";
    let title = "", sub = "";
    if (P.menu) body.append(menuRail(P));                  // the game menu's tabs (UI-005)
    if (P.kind === "shop") {
      const k = E.shopKind(P.n), S = EC.shops[k]; title = lotName(P.n); sub = S.name + " · restocks at " + hhmm(EC.stock.restock_minute);
      body.append(purseLine(), h("h3", {}, "Buy"), tbl(S.sells.map(it => [EC.items[it].name, h("span", { cls: "dim" }, E.stock(P.n, it) + " left"), money(E.price(P.n, it)),
        btn("Buy", () => { const p = E.buy(P.n, it); return "Bought " + EC.items[it].name.toLowerCase() + " for " + fmt(p) + "."; }, E.stock(P.n, it) < 1)])));
      const mine = Object.keys(E.inv).filter(it => E.offer(P.n, it) > 0);
      body.append(h("h3", {}, "Sell"), mine.length ? tbl(mine.map(it => [EC.items[it].name, h("span", { cls: "dim" }, "you have " + E.inv[it]), money(E.offer(P.n, it)),
        btn("Sell", () => { const p = E.sell(P.n, it); return "Sold for " + fmt(p) + "."; })])) : h("p", { cls: "dim" }, S.buys.length ? "Nothing you carry is wanted here." : "Cafés do not buy anything."));
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
        body.append(h("h3", {}, "On your stall"), st.slots.length ? tbl(st.slots.map((s, k) => [EC.items[s.item].name, money(s.price), s.sold ? h("span", { cls: "pos" }, "sold") : btn("Take back", () => { E.takeBack(k); })])) : h("p", { cls: "dim" }, "Nothing yet."));
        const can = Object.keys(E.inv).filter(it => !EC.items[it].quest && EC.items[it].price > 0);
        if (can.length && st.slots.length < S.slots) {
          const pick = h("select", { "aria-label": "Item" }, can.map(it => h("option", { value: it }, EC.items[it].name + " (" + E.inv[it] + ")")));
          const pr = h("input", { type: "number", min: "0.01", step: "0.01", cls: "amt", "aria-label": "Price in brix" }), hint = h("span", { cls: "dim" });
          const usual = () => { pr.value = (EC.items[pick.value].price / 100).toFixed(2); hint.textContent = "usually " + fmt(EC.items[pick.value].price); }; pick.onchange = usual; usual();
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
      title = "Quests"; sub = "Day " + (clock.day + 1);
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
    else if ({ belongings: 1, wardrobe: 1, settings: 1, save: 1, help: 1 }[P.kind]) { const t = { belongings: belongingsTab, wardrobe: wardrobeTab, settings: settingsTab, save: saveTab, help: helpTab }[P.kind](P, body); title = t[0]; sub = t[1]; }
    if (P.msg) body.prepend(h("p", { cls: "pmsg" }, P.msg));
    el("life-title").textContent = title; el("life-sub").textContent = sub;
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
  const PICTURE = { shadows: [true, false], ambient_occlusion: [true, false], bounce: [true, false], resolution: ["full", "soft"] };   // better, simple
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
  function menu(tab) {                                   // open the menu at a tab (the last one, or Money)
    tab = tab || (menu.last && TABS.some(t => t.id === menu.last) ? menu.last : "wallet");
    if (tab === "map") { if (panel) close(); window.wasted.showMap(true); return; }   // the map is the town's own big map
    if (panel && panel.menu && panel.kind === tab) return close();
    if (panel && !panel.menu) close();
    menu.last = tab;
    if (panel && panel.menu) { panel = { kind: tab, n: null, msg: "", menu: true }; el("life").classList.toggle("side", tab === "wardrobe"); draw(); return; }
    open(tab, null); panel.menu = true; el("life").classList.add("menu"); el("life").classList.toggle("side", tab === "wardrobe"); draw();
  }
  function menuRail(P) {
    return h("div", { cls: "mrail" }, TABS.map(t => h("button", { type: "button", cls: "mtab" + (t.id === P.kind ? " on" : ""), on: { click: () => menu(t.id) } }, t.name)));
  }
  function unitLot(i) { return M.units[i].lot; }
  function showOnMap(n) {                                // a waypoint at a building, and the map (UI-006)
    const l = LOTS[n]; waypoint = { x: (l.rect[0] + l.rect[2]) / 2, z: (l.rect[1] + l.rect[3]) / 2, name: l.name };
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
    body.append(h("h3", {}, "You carry"), inv.length ? tbl(inv.map(it => [EC.items[it].name, "× " + E.inv[it], money(EC.items[it].price * E.inv[it])])) : h("p", { cls: "dim" }, "Nothing."));
    body.append(h("h3", {}, "Furniture"), belongings.length ? tbl(belongings.map(b => [b.name + (b.line ? " (" + ((Outlet.LINES[b.line] || {}).label || b.line) + ")" : ""),
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
      btn("Forget", () => { outfits.splice(k, 1); return "Forgotten."; })])));
    else body.append(h("p", { cls: "dim" }, "None saved yet."));
    const nm = h("input", { type: "text", maxLength: DG.name.max, placeholder: "Name this outfit", "aria-label": "Outfit name" });
    body.append(h("div", { cls: "prow" }, nm, btn("Save what you wear", () => {
      const o = Object.assign(outfitOf(look), { name: nm.value.trim() });
      if (!outfitOk(o)) throw new Econ.Refused("an outfit needs a name of 1 to " + DG.name.max + " letters");
      const k = outfits.findIndex(x => x.name === o.name); if (k < 0 && outfits.length >= ST.wardrobe.max_outfits) throw new Econ.Refused("the wardrobe holds " + ST.wardrobe.max_outfits + " outfits");
      if (k >= 0) outfits[k] = o; else outfits.push(o); return "Saved as " + o.name + ".";
    })));
    body.append(h("h3", {}, "What you wear"),
      row("Hair", chips("hair", hairNames)), row("Hair colour", h("div", { cls: "swatches" }, DG.hair_color.map((c, n) => h("button", { type: "button", cls: "sw" + (look.hair_color === n ? " on" : ""), style: "background:" + rgb(c), on: { click: () => set("hair_color", n) } })))),
      row("Top", chips("garment", DG.garment)), row("", pal("top")), row("Bottom", chips("bottom_type", DG.bottom_type)), row("", pal("bottom")), row("Shoes", pal("shoes")),
      row("Scarf", pal("scarf", true)), row("Cap", pal("cap", true)), row("Apron", pal("apron", true)),
      row("", h("div", { cls: "chips" }, h("button", { type: "button", cls: "chip" + (look.bag ? " on" : ""), on: { click: () => set("bag", !look.bag) } }, DG.extras.bag),
        h("button", { type: "button", cls: "chip" + (look.tash ? " on" : ""), on: { click: () => set("tash", !look.tash) } }, DG.extras.tash))),
      h("p", { cls: "dim" }, "Changing is free, anywhere (for now: later, clothes are bought in shops and kept in a wardrobe at home)."));
    return ["Wardrobe", look.name + " · U"];
  }
  function settingsTab(P, body) {                         // UI-009
    for (const [g, G] of Object.entries(ST.groups)) {
      body.append(h("h3", {}, G.name));
      for (const [k, s] of Object.entries(G.settings)) {
        const v = settings[g][k]; let ctl;
        if (s.type === "switch") ctl = h("div", { cls: "chips" }, [true, false].map(x => h("button", { type: "button", cls: "chip" + (v === x ? " on" : ""), on: { click: () => setting(g, k, x) } }, x ? "On" : "Off")));
        else if (s.type === "choice") ctl = h("div", { cls: "chips" }, Object.entries(s.options).map(([x, label]) => h("button", { type: "button", cls: "chip" + (v === x ? " on" : ""), on: { click: () => setting(g, k, x) } }, label)));
        else { const out = h("span", { cls: "dzval" }, String(v)); ctl = h("div", { cls: "dzslide" }, h("input", { type: "range", min: s.min, max: s.max, step: s.step, value: v, "aria-label": s.name,
          on: { input: e => { out.textContent = e.target.value; }, change: e => setting(g, k, +e.target.value) } }), out); }
        body.append(h("div", { cls: "dzrow" }, h("span", { cls: "dzl wide" }, s.name), ctl));
      }
    }
    body.append(h("div", { cls: "prow" }, btn("Back to the defaults", () => { settings = defaults(); try { localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings)); } catch (e) { } window.wasted.applySettings(settings); return "Settings are back to their defaults."; })),
      h("p", { cls: "dim" }, "Settings are kept on this device, not in your save."));
    return ["Settings", "Picture, sound, the screen and the controls"];
  }
  function saveTab(P, body) {                             // UI-007: one slot
    const s = readSave(), when = s && s.at ? new Date(s.at) : null, ago = when ? Math.round((Date.now() - when) / 60000) : null;
    if (transient) body.append(h("p", { cls: "neg" }, "This is a trial run (?auto=1): nothing is saved."));
    body.append(h("p", {}, when ? "Last saved " + when.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" }) + (ago < 1 ? ", just now" : ", " + ago + " minute" + (ago === 1 ? "" : "s") + " ago") + ". The game also saves itself as you play." : "Not saved yet."),
      h("div", { cls: "prow" }, btn("Save now", () => { save(); return transient ? "Nothing is saved in a trial run." : "Saved."; }, transient),
        btn("Load the last save", () => { const t = readSave(); if (!t) throw new Econ.Refused("there is no save"); close(); loadSave(t); toast("Loaded the last save."); }, !s || transient)),
      h("h3", {}, "Start over"),
      h("div", { cls: "prow" }, P.confirm ? btn("Yes: delete the save and start a new character", () => { try { localStorage.removeItem(SAVE_KEY); } catch (e) { } location.reload(); })
        : btn("New game", () => { P.confirm = true; }, transient),
        btn("Back to the title", () => { save(); location.reload(); })),
      P.confirm ? h("p", { cls: "neg" }, "A new game deletes this save. There is one save slot.") : null);
    return ["Save", "One save, kept in this browser"];
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
      const lv = (NL.levels || {})[t], opts = ((NL.objects || {})[t] || []).filter((_, n) => floors.includes(lv ? lv[n] : 0));
      if (opts.length) return opts[m % opts.length].slice();
      break;
    }
    const [a0, b0, a1, b1] = OL.rect, [c0, d0, c1, d1] = NL.rect;
    return [c0 + (x - a0) / (a1 - a0) * (c1 - c0), d0 + (z - b0) / (b1 - b0) * (d1 - d0)];
  }
  function planFrom(i, nl, unit, m) {
    const old = HOME0[i], OL = LOTS[old], NL = LOTS[nl], row = ORIG[i].map(p => p.slice()), moved = new Set();
    row.forEach((p, k) => { if (p[1] === 0 && p[7] === old) { const q = spotIn(OL, NL, p[2], p[3], unit.floors, m); p[2] = q[0]; p[3] = q[1]; p[7] = nl; moved.add(k); } });
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
      const row = [btn("Sell now for " + fmt(M.bid(i)), () => { const p = M.sellNow(i, agent); return "Sold for " + fmt(p) + "."; })];
      if (agent) row.push(ls ? btn("Take it off the market", () => { M.unlist(i); }) : btn("List at " + fmt(M.value(i)), () => { M.listForSale(i, M.value(i), agent); return "Listed with " + M.cos[agent].name + "; the agent takes " + (R.trade.agent_fee_bp / 100) + "% when it sells."; }));
      const owed = M.owed[String(i)];
      body.append(h("div", { cls: "qline" }, h("b", {}, M.name(i)),
        h("p", { cls: "dim" }, m2(i) + " · worth about " + fmt(M.value(i)) + " · " + tenantText(i) + " · upkeep and tax " + fmt(M.costOf(i)) + "/day" + (ls ? " · listed at " + fmt(ls.price) : "") + (owed ? " · " + fmt(owed) + " unpaid!" : "")),
        h("div", { cls: "prow" }, "Rent Bx", rent, h("span", { cls: "dim" }, "a day (market " + fmt(M.marketRent(i)) + ")"),
          btn("Set", () => { M.setRent(i, Math.round(+rent.value * 100), true); return "Rent set; offered to tenants."; }),
          btn(M.to_let[i] ? "Stop letting" : "Offer to let", () => { M.to_let[i] = !M.to_let[i]; })), h("div", { cls: "prow" }, ...row)));
    }
    const ten = Object.keys(M.tenancy).map(Number);
    if (ten.length) body.append(h("h3", {}, "Places you rent"), tbl(ten.map(i => [M.name(i), money(M.tenancy[String(i)].rent), h("span", { cls: "dim" }, "a day" + (M.tenancy[String(i)].missed ? ", " + M.tenancy[String(i)].missed + " unpaid" : "")), btn("Move out", () => { M.leave(i); return "You have moved out."; })])));
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
    if (!P.lot) Object.assign(P, { lot: l, draft: l.dsg, lay: {}, lay0: {}, undo: [], tab: designStart === "furnish" && furnishes ? "furnish" : own.whole ? "outside" : "inside", facade: "*", storey: mine[0], sel: null, room: Object.keys(FR.rooms)[0], line: FR.lines[0] });
    designStart = null;
    const d = P.draft, menu = Builder.options(l, l.dsg), K = X.builder.kit, pats = X.builder.patterns;
    if (P.tab === "furnish" && !P.lay[P.storey]) P.lay[P.storey] = P.lay0[P.storey] = layNow(l, P.storey);
    const snap = () => ({ d: P.draft, lay: Object.assign({}, P.lay) });
    const showAll = () => { window.wasted.repaint(l, Builder.built(P.draft)); for (const f of Object.keys(P.lay)) FW.showLayout(l, +f, P.lay[f]); };
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
        tbl(FR.rooms[P.room].map(k => [FR.pieces[k].name, h("span", { cls: "dim" }, FR.pieces[k].size.slice(0, 2).map(v => v.toFixed(2)).join(" × ") + " m"), money(FR.pieces[k].price),
          h("button", { type: "button", cls: "pbtn", on: { click: () => addPiece(P, k, R) } }, "Add")])));
    }
    const [rt, rl] = Builder.cost(l, l.dsg, d); let total = rt; const lines = rl.slice(), probs = d === l.dsg ? [] : Builder.check(l, l.dsg, d, own, null);
    for (const f of Object.keys(P.lay)) { const b = P.lay0[f], a = P.lay[f]; if (a === b) continue;
      const pool = poolOf(P), [t, ls] = Furnish.cost(b, a, pool), R = FW.roomFor(l, +f); total += t;
      for (const x of ls) lines.push([x[0] + (mine.length > 1 ? " (" + storeyName(+f).toLowerCase() + ")" : ""), x[1], x[2]]);
      for (const m of Furnish.check(R.room, R.keep, R.ceiling, b, a, null, pool)) probs.push(m + (mine.length > 1 ? " (" + storeyName(+f).toLowerCase() + ")" : "")); }
    if (lines.length && total > funds()) probs.push("it costs " + fmt(total) + " and you have " + fmt(funds()));
    body.append(h("h3", {}, "The work"), lines.length
      ? tbl(lines.map(([w, q, c]) => [w, h("span", { cls: "dim" }, q === 1 ? "" : Number.isInteger(q) ? "× " + q : q + " m²"), money(c)]).concat([[h("b", {}, "Total"), "", money(total)]]))
      : h("p", { cls: "dim" }, "Pick something to change: it shows on the building at once. Nothing is paid until you build."));
    if (probs.length) body.append(h("p", { cls: "neg" }, probs.map(x => x[0].toUpperCase() + x.slice(1)).join(". ") + "."));
    body.append(h("div", { cls: "prow" },
      h("button", { type: "button", cls: "pbtn", disabled: !lines.length || probs.length > 0, on: { click: () => act(() => {
        if (probs.length) throw new Econ.Refused(probs[0]);
        if (rt > 0) E.spend(rt, "sys:builders", "Remodel: " + l.name);
        if (total - rt > 0) E.spend(total - rt, "sys:builders", "Furniture: " + l.name);
        if (rl.length) remodels[l.id] = P.draft; l.dsg = P.draft;
        belongings = heldNow(P);
        for (const f of Object.keys(P.lay)) if (P.lay[f] !== P.lay0[f]) { (furnishings[l.id] = furnishings[l.id] || {})[f] = P.lay[f]; P.lay0[f] = P.lay[f]; FW.showLayout(l, +f, P.lay[f]); FW.layoutWorld(l, +f); }
        P.undo = [];
        return "Done: " + l.name + (total ? ", for " + fmt(total) : "") + ".";
      }) } }, lines.length ? (total ? "Pay " + fmt(total) + " and build" : "Keep it like this") : "Pay and build"),
      tab(false, "Undo", () => { const u = P.undo.pop(); P.draft = u.d; P.lay = u.lay; showAll(); draw(); }, !P.undo.length),
      tab(false, "Start again", () => { P.undo.push(snap()); P.draft = l.dsg; P.lay = Object.assign({}, P.lay0); showAll(); draw(); }, d === l.dsg && Object.keys(P.lay).every(f => P.lay[f] === P.lay0[f]))));
    body.append(h("p", { cls: "dim" }, "Drag the view to look round, the wheel to come nearer; W A S D slide, Q and E turn."));
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
    if (!quiet) P.undo.push({ d: P.draft, lay: Object.assign({}, P.lay) });
    P.lay = Object.assign({}, P.lay, { [f]: ch ? lay.map(p => p.id === id ? Object.assign({}, p, ch, ch.yaw !== undefined ? { yaw: ((ch.yaw % 360) + 360) % 360 } : {}) : p) : lay.filter(p => p.id !== id) });
    if (!ch) P.sel = null;
    window.wasted.furn.showLayout(P.lot, f, P.lay[f]); if (!quiet) { Sound.act("talk_choose"); draw(); }
  }
  function addPiece(P, kind, R) {                          // a new piece where the view looks, on the grid, inside the room
    const f = P.storey, g = Furnish.rules().grid_m, sn = v => +(Math.round(v / g) * g).toFixed(2), pc = Furnish.rules().pieces[kind];
    const x = sn(clamp(view.goal.tx, R.room[0] + pc.size[0] / 2, R.room[2] - pc.size[0] / 2)), z = sn(clamp(view.goal.tz, R.room[1] + pc.size[1] / 2, R.room[3] - pc.size[1] / 2));
    const here = P.lot.id + ":" + f, ks = P.lay[f].map(p => p.id).concat(poolOf(P).map(b => b.id.split(":").slice(1, 3).join(":") === here ? b.id.split(":")[3] : ""));
    const n = 1 + Math.max(0, ...ks.filter(i => /^k\d+$/.test(i)).map(i => +i.slice(1)));   // (never an id a belonging from this storey has)
    P.undo.push({ d: P.draft, lay: Object.assign({}, P.lay) });
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
    P.undo.push({ d: P.draft, lay: Object.assign({}, P.lay) });
    P.lay = Object.assign({}, P.lay, { [f]: P.lay[f].concat([Object.assign({}, b, { x, z, yaw: 0 })]) }); P.sel = b.id;
    window.wasted.furn.showLayout(P.lot, f, P.lay[f]); Sound.act("talk_choose"); draw();
  }
  let grab = null;
  const canvas = () => document.getElementById("gl");
  function pointer(type, e, held) {                      // the mouse over the picture while the designer is open (play.js)
    const P = panel; if (!view || !P) return;
    const g = view.goal;
    if (type === "wheel") { if (e.target && e.target.tagName === "CANVAS") g.dist = clamp(g.dist * (e.deltaY > 0 ? 1.12 : 1 / 1.12), 3, 150); return; }
    const FW = window.wasted.furn, fur = P.tab === "furnish" && P.lay[P.storey];
    const floorHit = (ray, F) => { if (ray.d[1] > -1e-4) return null; const t = (F - ray.o[1]) / ray.d[1]; return t > 0 ? [ray.o[0] + ray.d[0] * t, ray.o[2] + ray.d[2] * t] : null; };
    const under = () => { const R = FW.roomFor(P.lot, P.storey), ray = window.wasted.rayAt(e.clientX, e.clientY); return ray ? { R, ray, p: FW.pickPiece(P.lay[P.storey], R.F, ray) } : null; };
    if (type === "down") {
      grab = null;
      if (e.button === 2) { grab = { pan: true }; return; }
      if (!fur || e.button !== 0) { grab = { turn: true }; return; }
      const u = under(); if (!u) return; const hit = floorHit(u.ray, u.R.F);
      const was = P.sel; P.sel = u.p ? u.p.id : null;
      grab = u.p && hit ? { id: u.p.id, dx: u.p.x - hit[0], dz: u.p.z - hit[1], F: u.R.F, moved: false } : { turn: true };
      canvas().style.cursor = u.p ? "grabbing" : "default";
      if (was !== P.sel) draw(); return;
    }
    if (type === "move") {
      if (!held || !grab) {                                // hovering: the piece under the mouse is outlined, and the mouse says it can be moved
        if (fur) { const u = under(), id = u && u.p ? u.p.id : null; P.hover = id; canvas().style.cursor = id ? "grab" : "default"; }
        return;
      }
      if (grab.id) {
        const ray = window.wasted.rayAt(e.clientX, e.clientY), hit = ray && floorHit(ray, grab.F); if (!hit) return;
        const gm = Furnish.rules().grid_m, sn = v => +(Math.round(v / gm) * gm).toFixed(2), p = P.lay[P.storey].find(q => q.id === grab.id); if (!p) return;
        const x = sn(hit[0] + grab.dx), z = sn(hit[1] + grab.dz); if (x === p.x && z === p.z) return;
        if (!grab.moved) { P.undo.push({ d: P.draft, lay: Object.assign({}, P.lay) }); grab.moved = true; }
        editPiece(P, grab.id, { x, z }, true);
      } else if (grab.pan) {                               // the right button slides the view across the ground
        const k = g.dist * .0016, sy = Math.sin(g.yaw), cy = Math.cos(g.yaw), [x0, z0, x1, z1] = P.lot.rect;
        g.tx = clamp(g.tx - (cy * e.movementX - sy * e.movementY) * k, x0 - 8, x1 + 8); g.tz = clamp(g.tz - (sy * e.movementX + cy * e.movementY) * k, z0 - 8, z1 + 8);
      } else { g.yaw += e.movementX * .006; g.pitch = clamp(g.pitch - e.movementY * .005, -1.5, -.12); }
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
    const P = panel; if (!view || !P || P.tab !== "furnish" || !P.lay[P.storey]) return [];
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
    const typing = /^(INPUT|TEXTAREA|SELECT)$/.test((document.activeElement || {}).tagName) && (document.activeElement.type || "text") === "text";
    if (typing && e.code !== "Escape") return;
    const tab = { KeyI: "wallet", KeyG: "belongings", KeyP: "property", KeyJ: "quests", KeyU: "wardrobe", KeyH: "help", KeyM: "map" }[e.code];
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
    if (performance.now() - checkT > 250) { checkT = performance.now(); Q.check(ctx()); }   // four times a second, however slow the frames
    const tm = E.totalMoney();
    if (lastMoney !== null && tm !== lastMoney) { const d = tm - lastMoney; el("purse-delta").textContent = (d > 0 ? "+" : "") + fmt(d); el("purse-delta").className = d > 0 ? "pos" : "neg"; el("purse-delta").style.opacity = 1; setTimeout(() => el("purse-delta").style.opacity = 0, 2200); if (panel) draw(); }
    lastMoney = tm;
    if ((toastT -= dt) <= 0) el("toast").style.opacity = 0;
    hud();
    if (performance.now() - lastSave > 15000) save();
  }
  function hud() {
    el("purse").style.display = "block";
    el("purse-cash").textContent = fmt(E.L.balance("p:cash"));
    el("purse-bank").textContent = E.accounts.length ? " · bank " + fmt(E.L.balance("p:current") + E.L.balance("p:savings")) : "";
    const tr = el("track"), lines = [];
    for (const qid of Object.keys(Q.active).slice(0, 3)) lines.push([QR.quests[qid].title, Q.stage(qid).log]);
    if (Q.job) { const left = Q.job.deadline - (clock.day * 1440 + clock.m); lines.push([QR.jobs.kinds[Q.job.kind].title + " · " + Math.floor(left / 60) + "h " + String(Math.floor(left % 60)).padStart(2, "0") + "m left", fillJob(QR.jobs.kinds[Q.job.kind].log, Q.job)]); }
    const key = JSON.stringify(lines);
    if (tr.dataset.k !== key) { tr.dataset.k = key; tr.textContent = ""; for (const [a, b] of lines) tr.append(h("div", { cls: "trk" }, h("b", {}, a), h("span", {}, b))); }
    tr.style.display = lines.length && !Talk.on ? "block" : "none";
    const n = lotHere(), hint = el("counter");
    const what = n === null || n < 0 ? null : n === E.bankLot ? "Bank counter" : n === E.marketLot ? "Your market stall" : LOTS[n] && M.office[LOTS[n].id] ? "Estate agent: " + M.cos[M.office[LOTS[n].id]].name : E.shopKind(n) ? "Buy and sell at " + lotName(n) : null;
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
    if (waypoint) out.push([waypoint.x, waypoint.z, 1]);   // the place marked from Properties (UI-006)
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
    const badF = Furnish.selfTest(Furniture.SIZE);
    if (badF.length) console.error("builder.js Furnish does not match Tools/citysim/furnish.py:\n  " + badF.join("\n  ")); else console.info("builder.js Furnish matches Tools/citysim/furnish.py (self-test passed).");
    const badE = Estate.selfTest(D, LOTS);
    if (badE.length) console.error("estate.js does not match Tools/citysim/estate.py:\n  " + badE.join("\n  ")); else console.info("estate.js matches Tools/citysim/estate.py (self-test passed).");
    const bad = Econ.selfTest(X, LOTS);
    if (bad.length) console.error("economy.js does not match Tools/citysim/economy.py:\n  " + bad.join("\n  ")); else console.info("economy.js matches Tools/citysim/economy.py (self-test passed).");
  });
  return {
    get on() { return !!panel; }, get designing() { return designing; }, get ready() { return ready; }, get view() { return view; }, pointer, designMarks, designTags, endView: () => { view = null; },
    go, frame, camera, mark, marks, boards, remodelTarget, remodel, conversation, allowed, chose, talkEnded, vars, key, counter, toast,
    panel: k => menu(k), menu: () => menu(), setting, spin: dx => { spin += dx * .006; },
    get paused() { return !!(panel && panel.menu && ST.menu.pauses) || !!(view && !view.leave); },   // the menu, and the building designer, stop the clock
  };
})();
