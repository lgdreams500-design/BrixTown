"use strict";
// The real estate market for the slice (F-PROP-ESTATE level 2, ADR-0022). A port of Tools/citysim/estate.py, rule for
// rule and in the same order, so the same data and the same game give the same market. Estate.selfTest() runs the script
// build_play.py ran in Python and compares (city_data.js: D.life.estate_checks). Every building is carved into units,
// each with an owner and maybe a tenant; money moves only through the economy's ledger, settled day by day (PROP-009 to
// PROP-021). No DOM and none of play.js's globals here: life.js does the screens.
var Estate = (() => {
  const { hash01, Refused } = Econ;
  const no = msg => { throw new Refused(msg); };
  const clone = x => JSON.parse(JSON.stringify(x));
  const idiv = (a, b) => Math.floor(a / b);               // Python's // on whole numbers
  const clamp = (v, a, b) => v < a ? a : v > b ? b : v;
  const TOWN = "npc:town";
  const blockOf = id => id.slice(0, id.lastIndexOf("_l"));
  const centre = l => [(l.rect[0] + l.rect[2]) / 2, (l.rect[1] + l.rect[3]) / 2];

  function carve(R, lots, residents) {                    // PROP-009
    const byHome = new Map();
    for (const r of residents) if (r.home) { if (!byHome.has(r.home)) byHome.set(r.home, []); byHome.get(r.home).push(r); }
    const L = R.location, cl = lots.find(l => l.id === L.centre_lot), [cx, cz] = centre(cl), F = R.floor, out = [];
    lots.forEach((l, n) => {
      if (R.public_kinds.includes(l.kind) || !l.floors) return;
      const rule = R.units[l.kind] || R.units.default, floors = l.floors;
      const st = l.storeys || ["main"].concat(Array(floors - 1).fill("rooms"));
      const groups = [];
      if ("whole" in rule && !st.slice(1).some(s => s === rule.flat_use)) groups.push([rule.whole, [...Array(floors).keys()]]);
      else if ("per_floor" in rule) for (let f = 0; f < floors; f++) groups.push([rule.per_floor, [f]]);
      else {
        if ("main" in rule) groups.push([rule.main, [...Array(floors).keys()].filter(f => f === 0 || (rule.with_main || []).includes(st[f]))]);
        for (let f = 1; f < floors; f++) if (st[f] === rule.flat_use) groups.push(["flat", [f]]);
      }
      const [x, z] = centre(l), dx = x - cx, dz = z - cz, dist = Math.floor(Math.sqrt(dx * dx + dz * dz));
      const loc = L.near_bp - idiv((L.near_bp - L.far_bp) * Math.min(dist, L.radius), L.radius);
      const fw = l.rect[2] - l.rect[0], fh = l.rect[3] - l.rect[1];
      const people = byHome.get(l.id) || [], beds = (l.levels || {}).bed || [], work = (l.levels || {}).work || [];
      groups.forEach(([kind, fl], k) => {
        const f0 = fl[0];
        const fb = f0 === 0 ? 10000 : (l.lift || f0 <= F.no_lift_above) ? 10000 + F.up_bp * f0 : 10000 + F.up_bp * F.no_lift_above - F.no_lift_down_bp * (f0 - F.no_lift_above);
        const uid = l.id + ":u" + k;
        let members;
        if (kind === "house") members = people.map(r => r.id);
        else if (kind === "flat" && l.kind === "home") members = people.filter((r, i) => (i < beds.length ? beds[i] : 1 + idiv(i, R.household_size)) === f0).map(r => r.id);
        else members = [];
        let tenant;
        if (members.length) tenant = "hh";
        else if (kind === "flat" || (kind === "office" && f0 > 0 && !work.includes(f0))) tenant = hash01(uid, "lodger") * 1000 < R.letting.lodger_occupied_permille ? "lodger" : null;
        else if (kind === "house") tenant = null;
        else tenant = "firm";
        out.push({ id: uid, lot: n, lot_id: l.id, block: blockOf(l.id), kind, floors: fl, area: Math.floor(fw * fh * fl.length * R.usable_bp / 10000), loc_bp: loc, floor_bp: fb, members, tenant0: tenant });
      });
    });
    return out;
  }

  class Market {
    constructor(R, econ, lots, residents) {
      this.R = R; this.E = econ; this.lots = lots;
      this.units = carve(R, lots, residents);
      this.byId = {}; this.byLot = new Map();
      this.units.forEach((u, i) => { this.byId[u.id] = i; if (!this.byLot.has(u.lot)) this.byLot.set(u.lot, []); this.byLot.get(u.lot).push(i); });
      this.blocks = []; for (const u of this.units) if (!this.blocks.includes(u.block)) this.blocks.push(u.block);
      const roles = {}; for (const r of residents) roles[r.id] = r.role;
      const W = R.town.wages;
      this.wage = this.units.map(u => u.members.reduce((s, m) => s + (W[roles[m]] || 0), 0));
      this.cos = {}; for (const c of R.companies) this.cos[c.id] = c;
      this.office = {}; for (const c of R.companies) this.office[c.office] = c.id;
      this.base = this.units.map(u => idiv(idiv(u.area * R.kinds[u.kind].per_m2 * u.loc_bp, 10000) * u.floor_bp, 10000));
      this.baseTotal = this.base.reduce((a, b) => a + b, 0);
      this.started = false;
      this.owner = this.units.map((u, i) => this.firstOwner(i));
      this.tenant = this.units.map(u => u.tenant0);
      this.rent = this.units.map(() => 0); this.to_let = this.units.map(() => false);
      this.tenancy = {}; this.listing = {}; this.missed = {}; this.owed = {};
      this.mortgages = []; this.buys = [];
      this.idx = {}; for (const b of this.blocks) this.idx[b] = 10000;
      this.history = []; this.sales = []; this.short = {}; this.income = 0;
      this.hook();
    }
    get L() { return this.E.L; }
    hook() { if (!this._h) this._h = d => this.settle(d); if (!this.E.hooks.includes(this._h)) this.E.hooks.push(this._h); }
    firstOwner(i) {                                       // PROP-010
      const u = this.units[i], w = this.R.kinds[u.kind].owners;
      let opts = Object.entries(w).filter(([k, v]) => v > 0 && !(k === "household" && u.tenant0 !== "hh") && !(k === "firm" && u.tenant0 !== "firm"));
      if (!opts.length) opts = [["city", 1]];
      let t = hash01(u.id, "owner") * opts.reduce((s, o) => s + o[1], 0), pick = opts[opts.length - 1][0];
      for (const [k, v] of opts) { if (t < v) { pick = k; break; } t -= v; }
      if (pick === "household") return "hh";
      if (pick === "firm") return "firm";
      if (pick === "city") return "city";
      return "co:" + this.companyFor(u.kind, u.id, "first");
    }
    companyFor(kind, ...seed) {
      let cs = this.R.companies.filter(c => c.focus.includes(kind)); if (!cs.length) cs = this.R.companies;
      let best = null, bs = -1;
      for (const c of cs) { const s = hash01(c.id, ...seed) * c.weight; if (s > bs) { best = c.id; bs = s; } }
      return best;
    }
    start() {
      if (this.started) return;
      this.started = true;
      const lines = this.R.companies.map(c => ["co:" + c.id, c.cash]);
      lines.push([TOWN, this.R.town.pool_days * this.dayIncome()]);
      lines.push(["sys:founding", -lines.reduce((s, l) => s + l[1], 0)]);
      this.L.post(this.E.day, this.E.minute, "The estate agents open; the town's households and businesses begin", lines);
    }
    // ---- values (PROP-011)
    value(i) { return idiv(this.base[i] * this.idx[this.units[i].block], 10000); }
    marketRent(i) { return Math.max(1, idiv(this.value(i) * this.R.kinds[this.units[i].kind].yield_bp, 10000)); }
    rentOf(i) {                                           // PROP-014
      if (this.tenant[i] === "player") return this.tenancy[String(i)].rent;
      if (this.owner[i] === "player") { const r = this.rent[i] || this.marketRent(i); return ["hh", "firm"].includes(this.tenant[i]) ? Math.min(r, this.marketRent(i)) : r; }
      return this.marketRent(i);
    }
    townIndex() { let s = 0; for (let i = 0; i < this.units.length; i++) s += this.value(i); return idiv(s * 10000, this.baseTotal); }
    dayIncome() {
      const T = this.R.town; let n = 0;
      for (let i = 0; i < this.units.length; i++) {
        const t = this.tenant[i];
        if (t === "hh") n += this.wage[i];
        else if (t === "firm") n += this.owner[i] !== "firm" ? idiv(this.rentOf(i) * T.firm_cover_bp, 10000) : 0;
        else if (t === "lodger") n += idiv(this.rentOf(i) * T.lodger_cover_bp, 10000);
      }
      return n;
    }
    owned() { const o = []; this.owner.forEach((x, i) => { if (x === "player") o.push(i); }); return o; }
    costBp(i) {
      const C = this.R.costs; let bp = C.upkeep_bp + C.tax_bp;
      if (this.owner[i] === "player") bp += Math.min(C.portfolio_cap_bp, C.portfolio_bp_per_unit * Math.max(0, this.owned().length - C.portfolio_free));
      return bp;
    }
    costOf(i) { return idiv(this.value(i) * this.costBp(i), 10000); }
    acct(owner) {
      if (owner === "city") return "sys:city";
      if (owner === "hh" || owner === "firm") return TOWN;
      if (owner === "player") return this.E.accounts.includes("current") ? "p:current" : "p:cash";
      return owner;
    }
    // ---- the day (PROP-012, PROP-015, PROP-016)
    wave(b, d) {
      const I = this.R.index, n = I.wave_days, w = idiv(d, n), t = d % n;
      const h0 = Math.floor(hash01(b, w, "wave") * 2001) - 1000, h1 = Math.floor(hash01(b, w + 1, "wave") * 2001) - 1000;
      return idiv((h0 * (n - t) + h1 * t) * I.wave_bp, n * 1000);
    }
    updateIndex(d) {
      const I = this.R.index, occ = {}, tot = {};
      this.units.forEach((u, i) => { tot[u.block] = (tot[u.block] || 0) + 1; if (this.tenant[i] !== null) occ[u.block] = (occ[u.block] || 0) + 1; });
      const bump = {};
      for (const [day, b] of this.buys) if (d - day < I.player_buy_days) bump[b] = Math.min(I.player_buy_cap_bp, (bump[b] || 0) + I.player_buy_bp);
      for (const b of this.blocks) {
        const o = idiv((occ[b] || 0) * 1000, tot[b]);
        this.idx[b] = clamp(10000 + this.wave(b, d) + (o - I.occupancy_target_permille) * I.occupancy_bp_per_permille + (bump[b] || 0), I.min_bp, I.max_bp);
      }
      this.buys = this.buys.filter(x => d - x[0] < I.player_buy_days);
    }
    settle(d) {
      if (!this.started) return;
      this.updateIndex(d);
      let [earned, spent] = this.rentsAndCosts(d);
      this.letting(d);
      const sales = this.market(d);
      this.mortgageDays(d);
      this.companies(d);
      spent += this.townSpend(d);
      this.history.push([d, this.townIndex(), this.L.supply(), sales, earned, spent]);
      this.history = this.history.slice(-this.R.history_days);
    }
    rentsAndCosts(d) {
      const T = this.R.town, npc = new Map();
      const add = (a, v) => { if (v) npc.set(a, (npc.get(a) || 0) + v); };
      const pCosts = [], pRentOut = []; let pRentIn = 0, income = 0;
      for (let i = 0; i < this.units.length; i++) {
        const o = this.owner[i], t = this.tenant[i];
        if (o !== "city") {
          const c = this.costOf(i);
          if (o === "player") pCosts.push([i, c]); else { add(this.acct(o), -c); add("sys:tax", c); }
        }
        if (t !== null && !(t === "hh" && o === "hh") && !(t === "firm" && o === "firm")) {
          const r = this.rentOf(i);
          if (t === "player") pRentOut.push([i, r]);
          else if (o === "player") pRentIn += r;
          else { add(TOWN, -r); add(this.acct(o), r); }
        }
        let w = 0;
        if (t === "hh") w = this.wage[i];
        else if (t === "firm" && o !== "firm") w = idiv(this.rentOf(i) * T.firm_cover_bp, 10000);
        else if (t === "lodger") w = idiv(this.rentOf(i) * T.lodger_cover_bp, 10000);
        add(TOWN, w); add("sys:wages", -w); income += w;
      }
      this.short = {};
      for (const a of Array.from(npc.keys())) {
        if (a.startsWith("co:") && this.L.balance(a) + npc.get(a) < 0) {
          const gap = -(this.L.balance(a) + npc.get(a));
          npc.set(a, npc.get(a) + gap); npc.set("sys:tax", (npc.get("sys:tax") || 0) - gap); this.short[a.slice(3)] = true;
        }
      }
      if (this.L.balance(TOWN) + (npc.get(TOWN) || 0) - pRentIn < 0) {
        const gap = -(this.L.balance(TOWN) + (npc.get(TOWN) || 0) - pRentIn);
        npc.set(TOWN, (npc.get(TOWN) || 0) + gap); npc.set("sys:wages", (npc.get("sys:wages") || 0) - gap); income += gap;
      }
      const lines = []; for (const [a, v] of npc) if (v) lines.push([a, v]);
      if (lines.length) this.L.post(d, 1440, "Rents, upkeep, tax and earnings across town", lines);
      this.income = income;
      if (pRentIn) this.L.post(d, 1440, "Rent from your tenants", [[TOWN, -pRentIn], [this.acct("player"), pRentIn]]);
      for (const [i, r] of pRentOut) {                    // PROP-013
        const k = String(i), ten = this.tenancy[k];
        if (this.pay(d, r, this.acct(this.owner[i]), "Rent, " + this.name(i))) ten.missed = 0;
        else { ten.missed++; if (ten.missed >= this.R.letting.missed_before_eviction) { this.tenant[i] = null; delete this.tenancy[k]; } }
      }
      for (const [i, c] of pCosts) {                      // PROP-006, PROP-017
        const k = String(i), due = c + (this.owed[k] || 0);
        if (due && this.pay(d, due, "sys:tax", "Upkeep and tax, " + this.name(i))) { delete this.owed[k]; delete this.missed[k]; }
        else if (due) {
          this.owed[k] = due; this.missed[k] = (this.missed[k] || 0) + 1;
          if (this.missed[k] >= this.R.costs.missed_before_seizure) this.seize(i, d);
        }
      }
      return [income, 0];
    }
    pay(d, amount, to, memo) {
      if (amount <= 0) return true;
      const cash = this.L.balance("p:cash"), cur = this.E.accounts.includes("current") ? this.L.balance("p:current") : 0;
      if (cash + cur < amount) return false;
      const a = Math.min(cash, amount), lines = a ? [["p:cash", -a]] : [];
      if (amount > a) lines.push(["p:current", -(amount - a)]);
      lines.push([to, amount]);
      this.L.post(d, d === this.E.day && this.E.minute < 1440 ? this.E.minute : 1440, memo, lines);
      return true;
    }
    letting(d) {                                          // PROP-015
      const T = this.R.letting;
      for (let i = 0; i < this.units.length; i++) {
        const o = this.owner[i], t = this.tenant[i], mine = o === "player";
        const ratio = idiv((mine ? this.rentOf(i) : this.marketRent(i)) * 10000, this.marketRent(i));
        if (t === "lodger") {
          const ch = T.leave_bp + (mine ? idiv(T.leave_slope_bp * Math.max(0, ratio - 10000), 10000) : 0);
          if (hash01(this.units[i].id, d, "leave") * 10000 < ch) this.tenant[i] = null;
        } else if (t === null && (!mine || this.to_let[i])) {
          const ch = mine ? clamp(T.arrive_bp - idiv(T.arrive_slope_bp * (ratio - 10000), 10000), 0, 10000) : T.arrive_bp;
          if (hash01(this.units[i].id, d, "arrive") * 10000 < ch) this.tenant[i] = "lodger";
        }
      }
    }
    market(d) {                                           // PROP-012, PROP-016
      const M = this.R.trade; let sold = 0;
      for (let i = 0; i < this.units.length; i++) {
        const k = String(i), ls = this.listing[k];
        if (ls) {
          const ratio = idiv(this.value(i) * 10000, ls.price);
          const ch = clamp(M.sale_bp + idiv(M.sale_slope_bp * (ratio - 10000), 10000), 0, M.sale_max_bp);
          if (hash01(this.units[i].id, d, "sale") * 10000 < ch) {
            const b = this.buyer(i, ls.price, d);
            if (b) { this.transfer(i, b, ls.price, d, ls.agent, "Sold: " + this.name(i)); sold++; continue; }
          }
          if (this.owner[i] !== "player" && d - ls.since > 0 && (d - ls.since) % M.listing_days === 0) ls.price = idiv(ls.price * (10000 - M.price_cut_bp), 10000);
        } else if (this.owner[i] !== "player" && this.owner[i] !== "city") {
          const o = this.owner[i], typ = o.startsWith("co:") ? "company" : o === "hh" ? "household" : "firm";
          let ch = M.list_bp[typ] + idiv(Math.max(0, this.idx[this.units[i].block] - 10000) * M.list_high_index_bp, 100);
          if (typ === "company" && this.short[o.slice(3)]) ch += M.list_low_cash_bp;
          if (typ === "company" && this.L.balance(o) < this.cos[o.slice(3)].reserve) ch += M.list_low_cash_bp;
          if (hash01(this.units[i].id, d, "list") * 10000 < ch) {
            const [lo, hi] = M.ask_bp, ask = lo + Math.floor(hash01(this.units[i].id, d, "ask") * (hi - lo + 1));
            const agent = typ === "company" ? o.slice(3) : this.companyFor(this.units[i].kind, this.units[i].id, d, "agent");
            this.listing[k] = { price: idiv(this.value(i) * ask, 10000), since: d, agent, by: o };
          }
        }
      }
      return sold;
    }
    buyer(i, price, d) {
      const M = this.R.trade, u = this.units[i], tax = idiv(price * M.transfer_tax_bp, 10000);
      if (this.tenant[i] === "hh" && this.owner[i] !== "hh") {
        if (hash01(u.id, d, "hh") * 10000 < M.household_buy_bp && this.L.balance(TOWN) >= price + tax + M.household_min_pool_days * this.income) return "hh";
      }
      let best = null, bs = -1;
      for (const c of this.R.companies) {
        const a = "co:" + c.id;
        if (a === this.owner[i] || !c.focus.includes(u.kind)) continue;
        if (this.L.balance(a) < price + tax + c.reserve) continue;
        const s = hash01(c.id, u.id, d, "buyer") * c.weight;
        if (s > bs) { best = a; bs = s; }
      }
      return best;
    }
    transfer(i, buyer, price, d, agent, memo, extra, minute) {
      if (minute === undefined) minute = 1440;
      const M = this.R.trade, seller = this.owner[i], tax = idiv(price * M.transfer_tax_bp, 10000);
      const fee = (agent === null || agent === undefined || seller === "co:" + agent) ? 0 : idiv(price * M.agent_fee_bp, 10000);
      const lines = extra ? extra.slice() : [[this.acct(buyer), -(price + tax)]];
      lines.push(["sys:tax", tax]);
      let debt = 0;
      if (seller === "player") {
        const m = this.mortgageOn(i);
        if (m) { debt = this.mortgageOwed(m); if (price - fee < debt) no("the price would not pay off the mortgage"); lines.push(["bank:vault", debt]); }
      }
      lines.push([this.acct(seller), price - fee - debt]);
      if (fee) lines.push(["co:" + agent, fee]);
      this.L.post(d, minute, memo, lines);
      if (seller === "player" && debt) { const m = this.mortgageOn(i); m.paid += debt; m.status = "repaid"; }
      this.owner[i] = buyer;
      if (buyer === "player") { this.rent[i] = this.marketRent(i); this.to_let[i] = true; this.buys.push([d, this.units[i].block]); }
      else this.to_let[i] = false;
      const k = String(i); delete this.listing[k]; delete this.owed[k]; delete this.missed[k];
      this.sales.push([d, i, price, seller, buyer]); this.sales = this.sales.slice(-40);
    }
    seize(i, d) {                                         // PROP-017
      const C = this.R.costs, owed = (this.owed[String(i)] || 0) + C.seizure_fee;
      this.forcedSale(i, d, owed, "sys:tax", "Seized for unpaid tax: " + this.name(i));
    }
    forcedSale(i, d, owed, to, memo) {
      const M = this.R.trade, price = idiv(this.value(i) * M.forced_bid_bp, 10000), tax = idiv(price * M.transfer_tax_bp, 10000);
      let buyer = null;
      for (const c of this.R.companies) if (this.L.balance("co:" + c.id) >= price + tax) { buyer = "co:" + c.id; break; }
      if (buyer === null) buyer = "city";
      const m = this.mortgageOn(i), debt = m ? this.mortgageOwed(m) : 0, take = Math.min(price, owed);
      const lines = [[this.acct(buyer), -(price + tax)], ["sys:tax", tax], [to, take]];
      const back = Math.min(price - take, debt);
      if (back) lines.push(["bank:vault", back]);
      const rest = price - take - back;
      if (rest) lines.push([this.acct("player"), rest]);
      this.L.post(d, 1440, memo, lines);
      if (m) { m.paid += back; m.status = back >= debt ? "repaid" : "defaulted"; }
      this.owner[i] = buyer; this.to_let[i] = false;
      const k = String(i); delete this.listing[k]; delete this.owed[k]; delete this.missed[k];
      this.sales.push([d, i, price, "player", buyer]); this.sales = this.sales.slice(-40);
    }
    companies(d) {                                        // PROP-018
      for (const c of this.R.companies) {
        const a = "co:" + c.id, over = this.L.balance(a) - c.cap;
        if (over > 0) { const pay = idiv(over * c.dividend_bp, 10000); if (pay) this.L.post(d, 1440, c.name + " pays a dividend", [[a, -pay], ["sys:dividends", pay]]); }
      }
    }
    townSpend(d) {
      const T = this.R.town, over = this.L.balance(TOWN) - T.pool_days * this.income;
      if (over <= 0) return 0;
      const pay = idiv(over * T.spend_bp, 10000);
      if (pay) this.L.post(d, 1440, "What the town's households and businesses spend", [[TOWN, -pay], ["sys:spend", pay]]);
      return pay;
    }
    // ---- mortgages (BANK-002, PROP-019)
    mortgageTerms(principal, term) {
      const C = this.R.mortgage, total = principal + idiv(principal * C.rate_bp * term, 10000), inst = idiv(total, term);
      return [total, Array(term - 1).fill(inst).concat([total - inst * (term - 1)])];
    }
    mortgageOn(i) { return this.mortgages.find(m => m.status === "active" && m.units.includes(i)) || null; }
    mortgageOwed(m) { return m.total + m.fees - m.paid - m.fees_paid; }
    mortgageDays(d) {
      const C = this.R.mortgage;
      for (const m of this.mortgages) {
        if (m.status !== "active") continue;
        const k = d - m.day; if (k < 1) continue;
        let due = Math.max(0, m.plan.slice(0, k).reduce((a, b) => a + b, 0) - m.paid) + m.fees - m.fees_paid;
        due = Math.min(due, this.mortgageOwed(m));
        if (due <= 0) continue;
        if (this.E.accounts.includes("current") && this.L.balance("p:current") >= due) {
          const fee = m.fees - m.fees_paid, lines = [["p:current", -due]];
          if (fee) lines.push(["sys:fees", fee]);
          if (due - fee) lines.push(["bank:vault", due - fee]);
          this.L.post(d, 1440, "Mortgage " + m.id + " repayment", lines);
          m.fees_paid += fee; m.paid += due - fee; m.missed = 0;
          if (this.mortgageOwed(m) <= 0) m.status = "repaid";
          continue;
        }
        m.missed++; m.fees += C.late_fee;
        if (m.missed >= C.default_after_missed) {
          for (const i of m.units.slice()) if (this.owner[i] === "player" && m.status === "active") this.forcedSale(i, d, 0, "sys:fees", "Repossessed by the bank: " + this.name(i));
          if (m.status === "active") m.status = "defaulted";
          this.E.barred = true;
        }
      }
    }
    // ---- what the player sees
    name(i) {
      const u = this.units[i], l = this.lots[u.lot], K = this.R.kinds[u.kind].name;
      if (this.byLot.get(u.lot).length === 1) return l.name;
      const f = u.floors[0];
      return l.name + ", " + K.toLowerCase() + (f === 0 && u.floors.length > 1 ? "" : f ? " on floor " + f : " on the ground floor");
    }
    ownerName(i) {
      const o = this.owner[i];
      if (o === "player") return "you";
      if (o === "city") return "the town";
      if (o === "hh") return "the household living there";
      if (o === "firm") return "the business there";
      return this.cos[o.slice(3)].name;
    }
    ask(i) {                                              // PROP-011
      if (this.owner[i] === "player") return null;
      const ls = this.listing[String(i)];
      if (ls) return ls.price;
      const o = this.owner[i], M = this.R.trade;
      if (o === "city" && !M.city_sells.includes(this.units[i].kind)) return null;
      const typ = o.startsWith("co:") ? "company" : o === "hh" ? "household" : o;
      return idiv(this.value(i) * M.offer_bp[typ], 10000);
    }
    bid(i) { return idiv(this.value(i) * this.R.trade.bid_bp, 10000); }
    // ---- what the player does
    needOffice(agent) { if (agent !== null && agent !== undefined && !(agent in this.cos)) no("no such estate agent"); }
    buy(units, agent, deposit, term) {                    // PROP-011, PROP-019
      this.needOffice(agent);
      if (!units.length) no("nothing to buy");
      const M = this.R.trade;
      let prices = units.map(i => { const p = this.ask(i); if (p === null) no(this.name(i) + " is not for sale"); return p; });
      if (units.length > 1) {
        const lot = this.units[units[0]].lot, rest = this.byLot.get(lot).filter(i => this.owner[i] !== "player");
        if (units.some(i => this.units[i].lot !== lot) || JSON.stringify(units.slice().sort((a, b) => a - b)) !== JSON.stringify(rest.slice().sort((a, b) => a - b))) no("a whole building is bought together");
        prices = prices.map(p => idiv(p * (10000 - M.whole_building_discount_bp), 10000));
      }
      const total = prices.reduce((a, b) => a + b, 0), tax = prices.reduce((a, p) => a + idiv(p * M.transfer_tax_bp, 10000), 0);
      let loan = 0, m = null;
      const mortgage = (deposit !== undefined && deposit !== null) || (term !== undefined && term !== null);
      if (mortgage) {
        const C = this.R.mortgage;
        if (!this.E.accounts.includes("current")) no("a mortgage needs a current account");
        if (this.E.barred) no("the bank will not lend to you again");
        if (this.mortgages.filter(x => x.status === "active").length >= C.max) no("you already have " + C.max + " mortgages");
        if (!C.terms.includes(term) || deposit === undefined || deposit === null || deposit * 10000 < total * C.deposit_bp || deposit > total) no("the bank does not offer that mortgage");
        loan = total - deposit;
        if (this.L.balance("bank:vault") < loan) no("the bank cannot lend that today");
      }
      const need = total + tax - loan, cash = this.L.balance("p:cash"), cur = this.E.accounts.includes("current") ? this.L.balance("p:current") : 0;
      if (cash + cur < need) no("you cannot afford it");
      if (loan) {
        const [totalM, plan] = this.mortgageTerms(loan, term);
        m = { id: this.mortgages.length + 1, day: this.E.day, principal: loan, total: totalM, plan, paid: 0, fees: 0, fees_paid: 0, missed: 0, status: "active", units: units.slice() };
      }
      let leftLoan = loan;
      units.forEach((i, n) => {
        const p = prices[n], t = idiv(p * M.transfer_tax_bp, 10000), fromBank = Math.min(leftLoan, p);
        leftLoan -= fromBank;
        const own = p + t - fromBank, a = Math.min(this.L.balance("p:cash"), own), lines = a ? [["p:cash", -a]] : [];
        if (own > a) lines.push(["p:current", -(own - a)]);
        if (fromBank) lines.push(["bank:vault", -fromBank]);
        const ls = this.listing[String(i)];
        const ag = ls ? ls.agent : (this.owner[i] === "hh" || this.owner[i] === "firm") ? (agent === undefined ? null : agent) : null;
        this.transfer(i, "player", p, this.E.day, ag, "Bought: " + this.name(i), lines, this.E.minute);
      });
      if (loan) this.mortgages.push(m);
      return total + tax;
    }
    sellNow(i, agent) {                                   // PROP-012
      this.needOffice(agent);
      if (this.owner[i] !== "player") no("that is not yours");
      const price = this.bid(i), tax = idiv(price * this.R.trade.transfer_tax_bp, 10000);
      let best = null;
      for (const c of this.R.companies) {
        const a = "co:" + c.id;
        if (c.focus.includes(this.units[i].kind) && this.L.balance(a) >= price + tax + c.reserve) if (best === null || this.L.balance(a) > this.L.balance(best)) best = a;
      }
      if (best === null) no("no company will buy it today");
      this.transfer(i, best, price, this.E.day, null, "Sold: " + this.name(i), null, this.E.minute);
      return price;
    }
    listForSale(i, price, agent) {
      this.needOffice(agent);
      if (agent === null || agent === undefined) no("list it with an estate agent");
      if (this.owner[i] !== "player") no("that is not yours");
      const [lo, hi] = this.R.trade.player_ask_range_bp, v = this.value(i);
      if (!Number.isInteger(price) || !(idiv(v * lo, 10000) <= price && price <= idiv(v * hi, 10000))) no("the agent will not list it at that price");
      this.listing[String(i)] = { price, since: this.E.day, agent, by: "player" };
    }
    unlist(i) { if (this.owner[i] !== "player" || !(String(i) in this.listing)) no("it is not listed"); delete this.listing[String(i)]; }
    setRent(i, rent, toLet) {                             // PROP-014
      if (toLet === undefined) toLet = true;
      if (this.owner[i] !== "player") no("that is not yours");
      const [lo, hi] = this.R.letting.rent_range_bp, m = this.marketRent(i);
      if (!Number.isInteger(rent) || !(idiv(m * lo, 10000) <= rent && rent <= idiv(m * hi, 10000))) no("that rent is out of bounds");
      this.rent[i] = rent; this.to_let[i] = !!toLet;
    }
    rentUnit(i, agent) {                                  // PROP-013
      this.needOffice(agent);
      if (this.owner[i] === "player") no("it is yours already");
      if (this.tenant[i] !== null) no("it is let");
      if (Object.keys(this.tenancy).length >= this.R.letting.max_tenancies) no("you rent as many places as you may");
      const r = this.marketRent(i);
      if (!this.pay(this.E.day, r, this.acct(this.owner[i]), "Rent, " + this.name(i))) no("you cannot afford the first day's rent");
      this.tenant[i] = "player"; this.tenancy[String(i)] = { rent: r, since: this.E.day, missed: 0 };
    }
    leave(i) { if (this.tenant[i] !== "player") no("you do not rent it"); this.tenant[i] = null; delete this.tenancy[String(i)]; }
    repay(mid) {
      const m = this.mortgages.find(x => x.id === mid && x.status === "active");
      if (!m) no("no such mortgage");
      const owed = this.mortgageOwed(m);
      if (!this.E.accounts.includes("current") || this.L.balance("p:current") < owed) no("not enough in the current account");
      const fee = m.fees - m.fees_paid, lines = [["p:current", -owed]].concat(fee ? [["sys:fees", fee]] : []).concat([["bank:vault", owed - fee]]);
      this.L.post(this.E.day, this.E.minute, "Mortgage " + m.id + " repaid early", lines);
      m.fees_paid += fee; m.paid += owed - fee; m.status = "repaid";
    }
    // ---- saving
    save() { return clone({ started: this.started, owner: this.owner, tenant: this.tenant, rent: this.rent, to_let: this.to_let, tenancy: this.tenancy, listing: this.listing, missed: this.missed, owed: this.owed, mortgages: this.mortgages, buys: this.buys, idx: this.idx, history: this.history, sales: this.sales, short: this.short, income: this.income }); }
    load(s) { Object.assign(this, clone(s)); }
  }

  // ---------------------------------------------------------------- the self-check against Python (estate.run_script)
  function runScript(est, ops) {
    const E = est.E, results = [], mine = [];
    const minBy = (c, key) => c.reduce((b, j) => (b === null || key(j) < key(b) || (key(j) === key(b) && j < b)) ? j : b, null);
    for (const op of ops) {
      const [name, ...a] = op;
      try {
        if (name === "advance") E.advance(a[0], a[1]);
        else if (name === "new") E.newCharacter();
        else if (name === "start") est.start();
        else if (name === "open") E.openAccount(a[0]);
        else if (name === "give") E.L.post(E.day, E.minute, "Test money", [["sys:start", -a[0]], ["p:current", a[0]]]);
        else if (name === "buy_cheapest") {
          const [kind, agent, depPc, term] = a;
          const c = []; est.units.forEach((u, i) => { if (u.kind === kind && est.ask(i) !== null) c.push(i); });
          const i = minBy(c, j => est.ask(j));
          if (depPc) { const p = est.ask(i); est.buy([i], agent, idiv(p * depPc, 100), term); } else est.buy([i], agent);
          mine.push(i);
        } else if (name === "rent_cheapest") {
          const c = []; est.units.forEach((u, i) => { if (u.kind === a[0] && est.tenant[i] === null && est.owner[i] !== "player") c.push(i); });
          est.rentUnit(minBy(c, j => est.marketRent(j)));
        } else if (name === "setrent") { const i = mine[a[0]]; if (i === undefined) no("none"); est.setRent(i, idiv(est.marketRent(i) * a[1], 10000)); }
        else if (name === "list") { const i = mine[a[0]]; if (i === undefined) no("none"); est.listForSale(i, idiv(est.value(i) * a[1], 10000), a[2]); }
        else if (name === "sell_now") { const i = mine[a[0]]; if (i === undefined) no("none"); est.sellNow(i); }
        results.push("ok");
      } catch (e) { if (!(e instanceof Refused)) throw e; results.push("refused"); }
    }
    const balances = {};
    for (const a of Object.keys(E.L.bal).filter(a => /^(co:|npc:|sys:|p:|bank:)/.test(a)).sort()) balances[a] = E.L.balance(a);
    const idx = {}; for (const b of Object.keys(est.idx).sort()) idx[b] = est.idx[b];
    const listing = {}; for (const k of Object.keys(est.listing).sort((x, y) => x - y)) listing[k] = est.listing[k].price;
    return { results, balances, tx: E.L.tx.length, supply: E.L.supply(), owner: est.owner.slice(), tenant: est.tenant.slice(), idx, history: est.history,
      mortgages: est.mortgages.map(m => [m.status, m.paid, m.fees, m.missed]), listing, units: est.units.length, mine };
  }
  function residentsOf(D, lots) { return D.residents.map(r => ({ id: r.id, role: D.roles[r.r], home: lots[r.h] ? lots[r.h].id : null })); }
  function selfTest(D, lots) {
    const X = D.life, C = X.estate_checks, E = new Econ.Economy(X.economy, lots, {}, {});
    const got = runScript(new Market(X.estate, E, lots, residentsOf(D, lots)), C.script), want = C.want, bad = [];
    for (const k of ["results", "balances", "tx", "supply", "owner", "tenant", "idx", "history", "mortgages", "listing", "units", "mine"])
      if (JSON.stringify(got[k]) !== JSON.stringify(want[k])) bad.push(k + ": " + JSON.stringify(got[k]).slice(0, 300) + " should be " + JSON.stringify(want[k]).slice(0, 300));
    return bad;
  }
  return { Market, carve, runScript, selfTest, residentsOf, TOWN };
})();
