"use strict";
// The player's life in the slice (ADR-0012): the character designer at a new game (CHR-001, CHR-002, CHR-013), money
// and the bank (ECO-009, BANK-007), shop counters and the market stall (ECO-011, PROP-008), quests, jobs and reputation in
// conversation (QST-001 to QST-007), and the save (QST-008). The rules are in economy.js; this file is the screens and the
// hooks play.js and conversations.js call. Loaded before play.js; play.js's globals (pl, clock, LOTS, lotAt, resident...)
// are only touched once it has run (from Life.go, Life.frame and the panels).
var Life = (() => {
  const D = window.WASTED, X = D.life, DG = X.designer, EC = X.economy, QR = X.quests, SAVE_KEY = "wasted.save.v1";
  const el = id => document.getElementById(id), fmt = Econ.fmt;
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
    fresh(); look = v; clock.day = 0;
    E.advance(0, clock.m); Q.newGame(); M.start(); ready = true; lastMoney = E.totalMoney(); save();
  }
  function readSave() { try { const s = localStorage.getItem(SAVE_KEY); return s ? JSON.parse(s) : null; } catch (e) { return null; } }
  function save() {
    if (!ready || transient) return;                      // ?auto=1 (tests, screenshots) never overwrites a real save
    try { localStorage.setItem(SAVE_KEY, JSON.stringify({ v: 1, look, pl: { x: pl.x, y: pl.y, z: pl.z, yaw: pl.yaw }, clock: { day: clock.day, m: clock.m }, econ: E.save(), quests: Q.save(), estate: M.save(), at: Date.now() })); } catch (e) { }
    lastSave = performance.now();
  }
  function loadSave(s) {
    fresh(); E.load(s.econ); Q.load(s.quests); if (s.estate) M.load(s.estate); else M.start(); look = s.look;
    Object.assign(pl, s.pl); pl.pose = 0; pl.vy = 0; cam.yaw = pl.yaw; clock.day = s.clock.day; clock.m = s.clock.m;
    ready = true; lastMoney = E.totalMoney();
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
    el("life").style.display = "block"; Sound.act("talk_open"); draw();
  }
  function close() { if (!panel) return; panel = null; el("life").style.display = "none"; Sound.act("talk_close"); save(); }
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
      title = look.name; sub = "Money and things you own";
      body.append(purseLine());
      const inv = Object.keys(E.inv).sort();
      body.append(h("h3", {}, "You carry"), inv.length ? tbl(inv.map(it => [EC.items[it].name, "× " + E.inv[it]])) : h("p", { cls: "dim" }, "Nothing."));
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
    if (P.msg) body.prepend(h("p", { cls: "pmsg" }, P.msg));
    el("life-title").textContent = title; el("life-sub").textContent = sub;
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
    return agent ? [M.cos[agent].name, "Estate agents · buy, sell and rent anywhere in town"] : ["Property", "What you own and rent · P to close"];
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
    for (const t of targets()) {
      if (t.i !== undefined) { const r = Talk.where(t.i, clock.m); if (r) out.push([r.x, r.z, t.offer ? 0 : 1]); }
      else if (t.lot !== undefined) { const l = LOTS[t.lot]; out.push([(l.rect[0] + l.rect[2]) / 2, (l.rect[1] + l.rect[3]) / 2, 1]); }
      else out.push([t.x, t.z, 1]);
    }
    return out;
  }
  addEventListener("beforeunload", save);
  addEventListener("load", () => {
    if (window.wasted) window.wasted.life = { get E() { return E; }, get Q() { return Q; }, get M() { return M; }, get look() { return look; }, selfTest: () => Econ.selfTest(X, LOTS), estateTest: () => Estate.selfTest(D, LOTS), save, newGame, readSave, open, close, counter };
    const badE = Estate.selfTest(D, LOTS);
    if (badE.length) console.error("estate.js does not match Tools/citysim/estate.py:\n  " + badE.join("\n  ")); else console.info("estate.js matches Tools/citysim/estate.py (self-test passed).");
    const bad = Econ.selfTest(X, LOTS);
    if (bad.length) console.error("economy.js does not match Tools/citysim/economy.py:\n  " + bad.join("\n  ")); else console.info("economy.js matches Tools/citysim/economy.py (self-test passed).");
  });
  return {
    get on() { return !!panel; }, get designing() { return designing; }, get ready() { return ready; },
    go, frame, camera, mark, marks, boards, conversation, allowed, chose, talkEnded, vars, key, counter, toast,
    panel: k => open(k, null), spin: dx => { spin += dx * .006; },
  };
})();
