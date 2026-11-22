"use strict";
// Money, shops, the market stall, quests and jobs for the slice (ADR-0012). A port of Tools/citysim/economy.py and
// Tools/citysim/quests.py, rule for rule and in the same order, so the same data gives the same ledger. The page checks
// that it does: Econ.selfTest() runs the script build_play.py ran in Python and compares (city_data.js: D.life.checks).
// Money is whole chips (ECO-001) and moves only as balanced ledger transactions (ECO-002). Nothing is ticked: stock and
// stall sales are worked out from the residents' routine, and the bank settles each day that has ended (BANK-008).
// No DOM and none of play.js's globals here: life.js does the screens.
var Econ = (() => {
  const PLAYER = ["p:cash", "p:current", "p:savings"];
  const fl = Math.floor;
  function hash01() {                                     // exactly hash() in play.js and hash01() in economy.py
    let h = 2166136261;
    for (const a of arguments) { const s = String(a); for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } h ^= 124; h = Math.imul(h, 16777619); }
    h ^= h >>> 15; h = Math.imul(h, 2246822519); h ^= h >>> 13; h = Math.imul(h, 3266489917); h ^= h >>> 16;
    return (h >>> 0) / 4294967296;
  }
  const fmt = c => (c < 0 ? "-" : "") + "Bx " + fl(Math.abs(c) / 100) + "." + String(Math.abs(c) % 100).padStart(2, "0");
  const isSys = a => a.startsWith("sys:");
  class Refused extends Error { }
  const no = msg => { throw new Refused(msg); };
  const clone = x => JSON.parse(JSON.stringify(x));
  const inWindow = (m, a, b) => a <= b ? a <= m && m < b : m >= a || m < b;
  const dist = (p, q) => Math.hypot(p[0] - q[0], p[1] - q[1]);
  const centre = l => [(l.rect[0] + l.rect[2]) / 2, (l.rect[1] + l.rect[3]) / 2];

  class Ledger {
    constructor() { this.tx = []; this.bal = {}; }
    balance(a) { return this.bal[a] || 0; }
    post(day, minute, memo, lines) {
      if (!lines.length) no("nothing to post");
      let total = 0; const after = {};
      for (const [a, v] of lines) {
        if (typeof v !== "number" || !Number.isInteger(v)) no("amounts are whole chips");
        total += v; after[a] = (a in after ? after[a] : this.balance(a)) + v;
      }
      if (total !== 0) no("lines do not balance");
      for (const a in after) if (after[a] < 0 && !isSys(a)) no(a + " would go below zero");
      const t = [this.tx.length, day, minute, memo, lines.filter(l => l[1] !== 0).map(l => [l[0], l[1]])];
      this.tx.push(t); Object.assign(this.bal, after);
      return t;
    }
    supply() { let s = 0; for (const a in this.bal) if (!isSys(a)) s += this.bal[a]; return s; }
    systemTotal() { let s = 0; for (const a in this.bal) if (isSys(a)) s += this.bal[a]; return s; }
    statement(accts, n) {
      const out = [];
      for (let k = this.tx.length - 1; k >= 0; k--) {
        const [no_, day, minute, memo, lines] = this.tx[k];
        if (lines.some(l => accts.includes(l[0]))) out.push([no_, day, minute, memo, lines.reduce((s, l) => s + (accts.includes(l[0]) ? l[1] : 0), 0)]);
        if (n && out.length >= n) break;
      }
      return out;
    }
  }

  class Economy {
    constructor(R, lots, visits, staff) {
      this.R = R; this.lots = lots; this.lotIndex = {}; lots.forEach((l, n) => this.lotIndex[l.id] = n);
      this.visits = visits; this.staff = staff || {}; this.items = R.items;
      this.L = new Ledger(); this.day = 0; this.minute = 0; this.settled = 0;
      this.inv = {}; this.accounts = []; this.welcomed = false; this.loans = []; this.barred = false; this.trades = {}; this.rep = {};
      this.stall = null; this.stall_sold_total = 0;
      this.bankLot = this.lotIndex[R.bank.lot]; this.marketLot = this.lotIndex[R.stall.lot];
      this.kinds = lots.map((l, n) => this.kindOf(n));
      const lines = [["bank:vault", R.bank.vault]];
      lots.forEach((l, n) => { const k = this.kinds[n]; if (k) lines.push(["till:" + l.id, R.shops[k].till]); });
      lines.push(["sys:founding", -lines.reduce((s, l) => s + l[1], 0)]);
      this.L.post(0, 0, "The town's bank and shops open", lines);
    }
    // ---- the player
    newCharacter() { const c = this.R.start.cash; this.L.post(this.day, this.minute, "Starting cash", [["sys:start", -c], ["p:cash", c]]); }
    totalMoney() { return PLAYER.reduce((s, a) => s + this.L.balance(a), 0); }
    giveItem(item, n = 1) { if (!(item in this.items)) no("no such item " + item); this.inv[item] = (this.inv[item] || 0) + n; }
    takeItem(item, n = 1) { if ((this.inv[item] || 0) < n) no("you have no " + item); this.inv[item] -= n; if (!this.inv[item]) delete this.inv[item]; }
    spend(amount, to, memo) {
      if (amount <= 0) no("nothing to pay");
      const a = Math.min(this.L.balance("p:cash"), amount), lines = a ? [["p:cash", -a]] : [];
      if (amount > a) { if (!this.accounts.includes("current")) no("not enough cash"); lines.push(["p:current", -(amount - a)]); }
      lines.push([to, amount]);
      return this.L.post(this.day, this.minute, memo, lines);
    }
    // ---- the clock and the bank
    advance(day, minute) {
      if (day < this.day || (day === this.day && minute < this.minute)) throw new Error("the clock only goes forward");
      if (this.stall && this.stall.day < day) this.runStall(this.stall.day, 1440);
      while (this.settled < day) { this.day = this.settled; this.minute = 1440; this.settle(this.settled); this.settled++; }
      this.day = day; this.minute = minute;
      if (this.stall && this.stall.day === day) this.runStall(day, minute);
    }
    settle(d) {
      const B = this.R.bank;
      for (const kind of this.accounts) {
        const rate = B.accounts[kind].rate_bp, acct = "p:" + kind, gain = fl(this.L.balance(acct) * rate / 10000);
        if (gain > 0 && this.L.balance("bank:vault") >= gain) this.L.post(d, 1440, "Interest, " + B.accounts[kind].name.toLowerCase(), [["bank:vault", -gain], [acct, gain]]);
      }
      for (const loan of this.loans) if (loan.status === "active") this.loanDay(loan, d);
      if (this.stall && this.stall.day === d) { for (const s of this.stall.slots) if (!s.sold) this.giveItem(s.item); this.stall = null; }
    }
    openAccount(kind) {
      if (!(kind in this.R.bank.accounts)) no("no such account");
      if (this.accounts.includes(kind)) no("already open");
      if (kind === "savings" && !this.accounts.includes("current")) no("open a current account first");
      this.accounts.push(kind);
      if (kind === "current" && !this.welcomed) { this.welcomed = true; const w = this.R.start.welcome_deposit; this.L.post(this.day, this.minute, "Linden Bank welcome deposit", [["sys:start", -w], ["p:current", w]]); }
    }
    move(src, dst, amount) {
      for (const a of [src, dst]) if (!PLAYER.includes(a) || (a !== "p:cash" && !this.accounts.includes(a.slice(2)))) no("no such account open: " + a);
      if (src === dst || amount <= 0) no("nothing to move");
      return this.L.post(this.day, this.minute, "Transfer", [[src, -amount], [dst, amount]]);
    }
    loanTerms(principal, term) {
      const C = this.R.bank.loan, total = principal + fl(principal * C.rate_bp * term / 10000), inst = fl(total / term);
      return [total, Array(term - 1).fill(inst).concat([total - inst * (term - 1)])];
    }
    takeLoan(principal, term) {
      const C = this.R.bank.loan;
      if (!this.accounts.includes("current")) no("a loan needs a current account");
      if (this.barred) no("the bank will not lend to you again");
      if (this.loans.filter(l => l.status === "active").length >= C.max_loans) no("you already have a loan");
      if (!(C.min <= principal && principal <= C.max) || principal % C.step || !C.terms.includes(term)) no("the bank does not offer that loan");
      if (this.L.balance("bank:vault") < principal) no("the bank cannot lend that today");
      const [total, plan] = this.loanTerms(principal, term);
      const loan = { id: this.loans.length + 1, day: this.day, principal, total, plan, paid: 0, fees: 0, fees_paid: 0, missed: 0, taken: 0, status: "active" };
      this.L.post(this.day, this.minute, "Loan " + loan.id + " paid out", [["bank:vault", -principal], ["p:current", principal]]);
      this.loans.push(loan);
      return loan;
    }
    loanOwed(l) { return l.total + l.fees - l.paid - l.fees_paid - l.taken; }
    payLoan(loan, amount, d, minute, memo) {
      const fee = Math.min(amount, loan.fees - loan.fees_paid), lines = [["p:current", -amount]];
      if (fee) lines.push(["sys:fees", fee]);
      if (amount - fee) lines.push(["bank:vault", amount - fee]);
      this.L.post(d, minute, memo, lines);
      loan.fees_paid += fee; loan.paid += amount - fee;
      if (this.loanOwed(loan) <= 0) loan.status = "repaid";
    }
    loanDay(loan, d) {
      const C = this.R.bank.loan, k = d - loan.day;
      if (k < 1) return;
      const sched = loan.plan.slice(0, k).reduce((s, v) => s + v, 0) - loan.paid - loan.taken, due = Math.max(0, sched) + loan.fees - loan.fees_paid;
      if (due <= 0) return;
      if (this.accounts.includes("current") && this.L.balance("p:current") >= due) { this.payLoan(loan, due, d, 1440, "Loan " + loan.id + " repayment"); loan.missed = 0; return; }
      loan.missed++; loan.fees += C.late_fee;
      if (loan.missed >= C.default_after_missed) this.defaultLoan(loan, d);
    }
    defaultLoan(loan, d) {
      const cur = this.L.balance("p:current");
      if (cur > 0) this.payLoan(loan, Math.min(cur, this.loanOwed(loan)), d, 1440, "Loan " + loan.id + ": account seized");
      const share = this.R.price.sell_share_bp;
      for (const item of Object.keys(this.inv).sort()) {
        const I = this.items[item]; if (!I.sell || I.price <= 0) continue;
        while (this.inv[item] && this.loanOwed(loan) > 0) { this.takeItem(item); loan.taken += fl(I.price * share / 10000); }
      }
      loan.status = "defaulted"; this.barred = true;
    }
    repay(id) {
      const loan = this.loans.find(l => l.id === id && l.status === "active"); if (!loan) no("no such loan");
      const owed = this.loanOwed(loan); if (this.L.balance("p:current") < owed) no("not enough in the current account");
      this.payLoan(loan, owed, this.day, this.minute, "Loan " + loan.id + " repaid early");
    }
    // ---- shops (ECO-005, ECO-010, ECO-011)
    kindOf(n) {
      const l = this.lots[n];
      for (const k in this.R.shops) { const m = this.R.shops[k].match; if (l.kind === m.kind && (l.name || "").includes(m.name_has || "")) return k; }
      return null;
    }
    shopKind(n) { return this.kinds[n]; }
    restock(day = this.day, minute = this.minute) { return minute >= this.R.stock.restock_minute ? day : day - 1; }
    residentBuys(n, item, day = this.day, minute = this.minute) {
      const S = this.R.shops[this.kinds[n]].sells, rd = this.restock(day, minute), r0 = this.R.stock.restock_minute;
      const spans = [[rd, r0, rd === day ? minute : 1440]].concat(rd < day ? [[day, 0, minute]] : []);
      let c = 0;
      for (const [d, a, b] of spans) for (const [t, i] of this.visits[n] || []) if (a <= t && t <= b && S[fl(hash01(i, d, n, "buy") * S.length)] === item) c++;
      return c;
    }
    stock(n, item) {
      const k = this.kinds[n]; if (!k || !this.R.shops[k].sells.includes(item)) return 0;
      const [b, s] = (this.trades[n + "|" + this.restock()] || {})[item] || [0, 0];
      return Math.max(0, this.R.stock.target - this.residentBuys(n, item) - b + s);
    }
    repWithLot(n) { const reps = (this.staff[n] || []).filter(r => r in this.rep).map(r => this.rep[r]); return reps.length ? Math.max(...reps) : 0; }
    factor(n, item) {
      const P = this.R.price, T = this.R.stock.target;
      return Math.max(P.min_bp, Math.min(P.max_bp, 10000 + fl(P.slope_bp * (T - this.stock(n, item)) / T)));
    }
    price(n, item) {
      const Q = this.R.reputation, r = this.repWithLot(n);
      const mod = r >= 0 ? 10000 - fl(Q.discount_bp_at_max * r / Q.max) : 10000 + fl(Q.markup_bp_at_min * r / Q.min);
      return Math.max(1, fl((this.items[item].price * this.factor(n, item) * mod + 5e7) / 1e8));
    }
    offer(n, item) {
      const k = this.kinds[n], I = this.items[item];
      if (!k || !I || !I.sell || !this.R.shops[k].buys.includes(item)) return 0;
      const f = this.R.shops[k].sells.includes(item) ? this.factor(n, item) : 10000;
      return fl(I.price * f * this.R.price.sell_share_bp / 1e8);
    }
    trade(n, item, k) { const key = n + "|" + this.restock(), t = this.trades[key] || (this.trades[key] = {}); (t[item] || (t[item] = [0, 0]))[k]++; }
    buy(n, item) {
      const k = this.kinds[n]; if (!k || !this.R.shops[k].sells.includes(item)) no("this shop does not sell that");
      if (this.stock(n, item) < 1) no("sold out");
      const p = this.price(n, item);
      this.spend(p, "till:" + this.lots[n].id, this.items[item].name + ", " + this.lots[n].name);
      this.trade(n, item, 0); this.giveItem(item);
      return p;
    }
    sell(n, item) {
      const p = this.offer(n, item); if (p <= 0) no("this shop does not buy that");
      if ((this.inv[item] || 0) < 1) no("you have none");
      const till = "till:" + this.lots[n].id; if (this.L.balance(till) < p) no("the till is empty");
      this.L.post(this.day, this.minute, "Sold " + this.items[item].name.toLowerCase() + ", " + this.lots[n].name, [[till, -p], ["p:cash", p]]);
      this.takeItem(item);
      if (this.R.shops[this.kinds[n]].sells.includes(item)) this.trade(n, item, 1);
      return p;
    }
    // ---- the market stall (PROP-008)
    rentStall() { if (this.stall) no("you already have a stall today"); this.spend(this.R.stall.rent, "sys:fees", "Market stall rent"); this.stall = { day: this.day, slots: [], done: this.minute }; }
    setOut(item, price) {
      const S = this.R.stall, I = this.items[item];
      if (!this.stall) no("rent a stall first");
      if (!I || I.quest || I.price <= 0) no("that cannot be sold here");
      if (this.stall.slots.length >= S.slots) no("the stall is full");
      if (!Number.isInteger(price) || price < 1 || price > fl(I.price * S.max_price_factor_bp / 10000)) no("that price is not allowed");
      this.takeItem(item); this.stall.slots.push({ item, price, sold: false });
    }
    takeBack(k) { const s = this.stall && this.stall.slots[k]; if (!s || s.sold) no("nothing there"); this.stall.slots.splice(k, 1); this.giveItem(s.item); }
    stallChance(item, price) { const S = this.R.stall, base = this.items[item].price, e = S.elasticity; return Math.min(9500, fl(S.buy_bp * base ** e / price ** e)); }
    runStall(day, minute) {
      const st = this.stall, S = this.R.stall, a = Math.max(st.done, S.open[0]), b = Math.min(minute, S.open[1]);
      for (const [t, i] of this.visits[this.marketLot] || []) {
        if (!(a < t && t <= b)) continue;
        for (let k = 0; k < st.slots.length; k++) {
          const s = st.slots[k]; if (s.sold) continue;
          if (hash01(i, day, "stall", k, s.item) * 10000 < this.stallChance(s.item, s.price)) {
            s.sold = true; this.stall_sold_total++;
            this.L.post(day, t, "Stall: " + this.items[s.item].name.toLowerCase() + " sold", [["sys:residents", -s.price], ["p:cash", s.price]]);
            break;
          }
        }
      }
      st.done = Math.max(st.done, minute);
    }
    // ---- saving (QST-008)
    save() { return clone({ tx: this.L.tx, day: this.day, minute: this.minute, settled: this.settled, inv: this.inv, accounts: this.accounts, welcomed: this.welcomed, loans: this.loans, barred: this.barred, trades: this.trades, rep: this.rep, stall: this.stall, stall_sold_total: this.stall_sold_total }); }
    load(s) {
      s = clone(s); this.L = new Ledger();
      for (const [, day, minute, memo, lines] of s.tx) this.L.post(day, minute, memo, lines);   // balances worked out again from the ledger
      delete s.tx; Object.assign(this, s);
    }
  }

  // ---------------------------------------------------------------- quest lines and jobs (quests.py)
  class Quests {
    constructor(Q, E, residents, lots) {
      this.Q = Q; this.E = E; this.residents = residents; this.lots = lots;
      this.active = {}; this.done = {}; this.log = []; this.job = null; this.jobs_done = [];
      this.shops = []; this.homes = []; lots.forEach((l, n) => { if (l.kind === "shop") this.shops.push(n); if (l.kind === "home") this.homes.push(n); });
      this.onNote = null;
    }
    note(text) { this.log.push([this.E.day, this.E.minute, text]); if (this.onNote) this.onNote(text); }
    rep(rid) { return this.E.rep[rid] || 0; }
    repAdd(rid, n) { const R = this.E.R.reputation; this.E.rep[rid] = Math.max(R.min, Math.min(R.max, this.rep(rid) + n)); }
    met(need) { const [q, e] = need.split(":"); return q in this.done && (!e || this.done[q] === e); }
    newGame() { this.E.newCharacter(); this.start(this.Q.first); }
    start(qid) { const q = this.Q.quests[qid]; if (qid in this.active || qid in this.done) no("already taken"); this.active[qid] = {}; this.note("Started: " + q.title); this.goto(qid, "stage:" + q.start); }
    goto(qid, target) { const [k, v] = target.split(":"); if (k === "end") return this.end(qid, v); this.active[qid] = { stage: v, since: this.E.stall_sold_total }; }
    stage(qid) { const a = this.active[qid]; return a && this.Q.quests[qid].stages[a.stage]; }
    offers(rid) { return Object.keys(this.Q.quests).filter(qid => { const q = this.Q.quests[qid]; return q.giver === rid && !(qid in this.active) && !(qid in this.done) && (q.needs || []).every(n => this.met(n)); }); }
    talkingTo(rid) { return Object.keys(this.active).filter(qid => this.stage(qid).goal.talk === rid); }
    needOk(need) {
      for (let x of need || []) {
        const neg = x.startsWith("!"); x = x.replace(/^!/, ""); const [k, v] = x.split(":");
        const ok = k === "have" ? (this.E.inv[v] || 0) > 0 : this.E.totalMoney() >= +v;
        if (ok === neg) return false;
      }
      return true;
    }
    act(qid, acts, rid) {
      for (const a of acts || []) {
        const [k, v] = a.split(":");
        if (k === "start") this.start(qid);
        else if (k === "stage" || k === "end") this.goto(qid, a);
        else if (k === "give") this.E.giveItem(v);
        else if (k === "take") this.E.takeItem(v);
        else if (k === "rep") this.repAdd(rid, +v);
      }
    }
    end(qid, eid) {
      const q = this.Q.quests[qid], e = q.endings[eid];
      if (e.charge) this.E.spend(e.charge, "sys:residents", q.title);
      if (e.pay) this.E.L.post(this.E.day, this.E.minute, "Reward: " + q.title, [["sys:rewards", -e.pay], ["p:cash", e.pay]]);   // QST-004
      for (const it of e.give || []) this.E.giveItem(it);
      for (const it of e.take || []) if (this.E.inv[it]) this.E.takeItem(it);
      for (const r in e.rep || {}) this.repAdd(r, e.rep[r]);
      delete this.active[qid]; this.done[qid] = eid;
      this.note("Finished: " + q.title + ". " + e.log);
    }
    goalMet(qid, ctx) {
      const g = this.stage(qid).goal, a = this.active[qid];
      if ("go" in g) return ctx.lot === g.go && inWindow(ctx.minute, g.from || 0, "to" in g ? g.to : 1440);
      if ("near" in g) return !!ctx.pos && dist(ctx.pos, g.near) <= g.r && inWindow(ctx.minute, g.from || 0, "to" in g ? g.to : 1440);
      if ("have" in g) return (this.E.inv[g.have] || 0) > 0;
      if ("bank" in g) return this.E.accounts.includes("current");
      if ("money" in g) return this.E.totalMoney() >= g.money;
      if ("stall" in g) return this.E.stall !== null;
      if ("stall_sold" in g) return this.E.stall_sold_total - a.since >= g.stall_sold;
      return false;
    }
    check(ctx) {
      let moved = true;
      while (moved) {
        moved = false;
        for (const qid of Object.keys(this.active)) {
          const st = this.stage(qid);
          if ("talk" in st.goal || !this.goalMet(qid, ctx)) continue;
          for (const it of (st.on_done || {}).give || []) this.E.giveItem(it);
          for (const it of (st.on_done || {}).take || []) this.E.takeItem(it);
          this.goto(qid, st.then); moved = true;
        }
      }
      this.checkJob(ctx);
    }
    // ---- daily jobs (QST-006)
    jobOffer(i, day) {
      const J = this.Q.jobs, r = this.residents[i], kid = Object.keys(J.kinds).find(k => J.kinds[k].roles.includes(r.role));
      if (!kid) return null;
      const K = J.kinds[kid];
      if (hash01(i, day, "job") * 10000 >= K.chance_bp) return null;
      const rn = (K.role_needs || {})[r.role];
      if (!(K.needs || []).every(n => this.met(n)) || (rn && !this.met(rn))) return null;
      if (this.rep(r.id) < J.min_rep || this.jobs_done.includes(i + "|" + day)) return null;
      const o = { kind: kid, giver: i, day, pay: K.pay, hours: K.hours };
      if (kid === "parcel") {
        const frm = this.shops[fl(hash01(i, day, "from") * this.shops.length)], to = this.homes[fl(hash01(i, day, "to") * this.homes.length)];
        Object.assign(o, { frm, to, pay: K.pay + fl(K.pay_per_100m * Math.trunc(dist(centre(this.lots[frm]), centre(this.lots[to]))) / 100) });
      } else if (kid === "shopping") o.item = K.items[fl(hash01(i, day, "item") * K.items.length)];
      else if (kid === "shift") { if (!r.work) return null; Object.assign(o, { to: this.E.lotIndex[r.work], minutes: K.minutes }); }
      return o;
    }
    takeJob(o) {
      if (this.job) no("one job at a time");
      const now = this.E.day * 1440 + this.E.minute;
      this.job = Object.assign({}, o, { deadline: now + o.hours * 60, step: o.kind === "parcel" ? "pickup" : "work", worked: 0, last: null });
      this.jobs_done.push(o.giver + "|" + o.day);
      this.note("Job taken: " + this.Q.jobs.kinds[o.kind].title);
    }
    finishJob(extra) {
      const j = this.job, K = this.Q.jobs.kinds[j.kind];
      if (extra) this.E.L.post(this.E.day, this.E.minute, "Paid back", [["sys:residents", -extra], ["p:cash", extra]]);
      this.E.L.post(this.E.day, this.E.minute, "Job: " + K.title, [["sys:rewards", -j.pay], ["p:cash", j.pay]]);
      this.repAdd(this.residents[j.giver].id, K.rep); this.job = null;
      this.note("Job done: " + K.title);
    }
    failJob() {
      const j = this.job, K = this.Q.jobs.kinds[j.kind];
      if (j.kind === "parcel" && j.step === "deliver" && this.E.inv.parcel) this.E.takeItem("parcel");
      this.repAdd(this.residents[j.giver].id, K.fail_rep); this.job = null;
      this.note("Job failed: " + K.title);
    }
    handOver() {
      const j = this.job; if (!j || j.kind !== "shopping" || !this.E.inv[j.item]) no("nothing to hand over");
      this.E.takeItem(j.item); this.finishJob(this.E.items[j.item].price);
    }
    checkJob(ctx) {
      const j = this.job; if (!j) return;
      const now = this.E.day * 1440 + this.E.minute;
      if (now > j.deadline) return this.failJob();
      if (j.kind === "parcel") {
        if (j.step === "pickup" && ctx.lot === this.lots[j.frm].id) { this.E.giveItem("parcel"); j.step = "deliver"; }
        else if (j.step === "deliver" && ctx.lot === this.lots[j.to].id && this.E.inv.parcel) { this.E.takeItem("parcel"); this.finishJob(); }
      } else if (j.kind === "shift") {
        const inside = ctx.lot === this.lots[j.to].id;
        if (inside && j.last !== null) j.worked += now - j.last;
        j.last = inside ? now : null;
        if (j.worked >= j.minutes) this.finishJob();
      }
    }
    save() { return clone({ active: this.active, done: this.done, log: this.log, job: this.job, jobs_done: this.jobs_done }); }
    load(s) { Object.assign(this, clone(s)); }
  }

  // ---------------------------------------------------------------- the self-check against Python (economy.run_script)
  function runScript(E, ops, shops) {
    const results = [];
    for (const op of ops) {
      const [name, ...a] = op;
      try {
        if (name === "new") E.newCharacter();
        else if (name === "buy") E.buy(shops[a[0]], a[1]);
        else if (name === "sell") E.sell(shops[a[0]], a[1]);
        else if (name === "open") E.openAccount(a[0]);
        else if (name === "move") E.move(a[0], a[1], a[2]);
        else if (name === "advance") E.advance(a[0], a[1]);
        else if (name === "loan") E.takeLoan(a[0], a[1]);
        else if (name === "rent") E.rentStall();
        else if (name === "setout") E.setOut(a[0], a[1]);
        results.push("ok");
      } catch (e) { if (!(e instanceof Refused)) throw e; results.push("refused"); }
    }
    const balances = {}; for (const a of PLAYER.concat(["bank:vault", "sys:fees", "sys:residents"])) balances[a] = E.L.balance(a);
    const inv = {}; for (const k of Object.keys(E.inv).sort()) inv[k] = E.inv[k];
    const prices = {}; for (const k of Object.keys(shops).sort()) prices[k] = E.price(shops[k], E.R.shops[k].sells[0]);
    return { results, balances, tx: E.L.tx.length, supply: E.L.supply(), inv, sold: E.stall_sold_total, loans: E.loans.map(l => [l.status, l.paid, l.fees, l.missed]), prices,
      hashes: [["a"], [1, 2, "b"], ["r0504", 16, "job"], [523, 3, 117, "buy"]].map(x => hash01(...x)) };
  }
  function selfTest(life, lots) {
    const C = life.checks, got = runScript(new Economy(life.economy, lots, life.visits, life.staff), C.script, C.shops), want = C.want, bad = [];
    for (const k of ["results", "balances", "tx", "supply", "inv", "sold", "loans", "prices"]) if (JSON.stringify(got[k]) !== JSON.stringify(want[k])) bad.push(k + ": " + JSON.stringify(got[k]) + " should be " + JSON.stringify(want[k]));
    got.hashes.forEach((h, n) => { if (Math.abs(h - want.hashes[n]) > 1e-11) bad.push("hash " + n + ": " + h + " should be " + want.hashes[n]); });
    return bad;
  }
  return { PLAYER, hash01, fmt, Refused, Ledger, Economy, Quests, runScript, selfTest, inWindow, centre };
})();
