"use strict";
// Buying furniture at the furniture outlet (FURN-022 to FURN-029; F-FURN-STORE): a port of Tools/citysim/furniture.py, rule
// for rule and message for message, so the same data and the same game give the same answers. Store.selfTest() runs the
// script build_play.py ran in Python and compares (city_data.js: D.life.furniture.checks).
//
//   const S = new Store.Shop(R, F, E, holds, routeM)     R: World/rules/furniture.json, F: furnishing.json, E: the Economy
//   S.tag(kind, line)            what a piece's tag says (FURN-022)
//   S.checkout(basket, atHome)   pay at the till: one ledger transaction, each piece taken now or booked for delivery (FURN-025, FURN-026)
//   S.van(order, day, minute)    where a delivery's van is, worked out from the booking and the clock (FURN-027)
//   S.advance(day, minute)       what has been carried in by now is in the unit, or with the belongings (FURN-028)
//   S.assemble(id, day, minute)  the player puts a flat-packed piece together; S.fitter(id, day, slot) pays for it (FURN-029)
//   S.settle(day)                the company pays its staff and its stock from its takings (FURN-024)
//
// Nothing is ticked. Loaded after economy.js (Econ.Refused, Econ.fmt).
var Store = (() => {
  const no = msg => { throw new Econ.Refused(msg); };
  const vol = P => P.size[0] * P.size[1] * P.size[2];
  const clone = x => JSON.parse(JSON.stringify(x));

  class Shop {
    constructor(R, F, E, holds, routeM) {
      this.R = R; this.F = F; this.E = E;
      this.holds = holds || (() => true); this.routeM = routeM || (() => 600);
      this.acct = R.company.account;
      this.n = 0; this.owned = []; this.orders = []; this.arrived = {}; this.notes = []; this.hasCatalogue = false; this.settled = -1;
    }
    // ---- the catalogue and the tags (FURN-022, FURN-023)
    sells() { return Object.keys(this.F.pieces).filter(k => !this.F.pieces[k].from); }
    price(kind) { return Math.max(1, Math.floor((this.F.pieces[kind].price * this.R.price_level_bp + 5000) / 10000)); }
    flatPacked(kind) { const P = this.F.pieces[kind]; return !P.rug && vol(P) >= this.R.flat_pack.from_m3; }
    boxes(kind) {                                          // how many boxes, and the stack they make
      if (!this.flatPacked(kind)) return [0, null];
      const K = this.R.flat_pack, [w, d, h] = K.box;
      const n = Math.min(K.most_boxes, Math.max(1, Math.ceil(vol(this.F.pieces[kind]) / K.packs_to / (w * d * h) - 1e-9)));
      return [n, [w, d, Math.round(h * n * 100) / 100]];
    }
    tag(kind, line) {
      if (!this.sells().includes(kind)) no("the outlet does not sell that");
      if (!this.F.lines.includes(line)) no("there is no such line");
      const P = this.F.pieces[kind], fp = this.flatPacked(kind);
      return { name: P.name, price: this.price(kind), size: P.size.slice(), line, other_lines: this.F.lines.filter(x => x !== line),
        flat_packed: fp, boxes: this.boxes(kind)[0], assembly_fee: fp ? this.assemblyFee(kind) : 0, assembly_min: fp ? this.assemblyMin(kind) : 0 };
    }
    takeCatalogue() { this.hasCatalogue = true; }
    // ---- fees and times
    deliveryFee(kinds) {
      const v = kinds.reduce((s, k) => s + vol(this.F.pieces[k]), 0), B = this.R.delivery.fee_by_m3;
      for (const [upto, fee] of B) if (upto === null || v <= upto + 1e-9) return fee;
      return B[B.length - 1][1];
    }
    assemblyFee(kind) { const A = this.R.assembly; return Math.max(A.least_fee, Math.floor((this.price(kind) * A.fee_bp + 5000) / 10000)); }
    assemblyMin(kind) { const A = this.R.assembly; return Math.max(A.least_min, Math.floor(vol(this.F.pieces[kind]) * A.min_per_m3 + .5)); }
    slots() {                                              // what can be booked now: [day, slot], soonest first
      const D = this.R.delivery, out = [];
      for (let d = this.E.day + D.first_day_after; d <= this.E.day + D.days_ahead; d++) for (let k = 0; k < D.slots.length; k++) out.push([d, k]);
      return out;
    }
    // ---- the till (FURN-025, FURN-026)
    quote(basket, atHome) {
      if (!basket.length) no("the basket is empty");
      if (atHome && !this.hasCatalogue) no("you have no catalogue to order from");
      const lines = [], runs = new Map(), sells = this.sells(), ok = this.slots();
      for (const it of basket) {
        const kind = it.kind, line = it.line;
        if (!sells.includes(kind)) no("the outlet does not sell that");
        if (!this.F.lines.includes(line)) no("there is no such line");
        const name = this.F.pieces[kind].name;
        lines.push([name, 1, this.price(kind)]);
        if (it.how === "instant") {
          if (atHome) no("ordering from home books delivery only");
          if (this.R.instant_fee) lines.push(["Take it now: " + name, 1, this.R.instant_fee]);
        } else if (it.how === "delivery") {
          if (!it.unit || !this.holds(it.unit)) no("that is not a home of yours to deliver to");
          if (!ok.some(q => q[0] === it.day && q[1] === it.slot)) no("that delivery slot cannot be booked");
          if (it.assembled && this.flatPacked(kind)) lines.push(["Put together: " + name, 1, this.assemblyFee(kind)]);
          const key = JSON.stringify([it.unit, it.day, it.slot]);
          if (!runs.has(key)) runs.set(key, { unit: it.unit, day: it.day, slot: it.slot, items: [] });
          runs.get(key).items.push(it);
        } else no("each piece is taken now or delivered");
      }
      const order = [...runs.values()].sort((a, b) => a.unit < b.unit ? -1 : a.unit > b.unit ? 1 : a.day - b.day || a.slot - b.slot);
      for (const r of order) lines.push(["Delivery, day " + (r.day + 1), r.items.length, this.deliveryFee(r.items.map(it => it.kind))]);
      return [lines.reduce((s, l) => s + l[2], 0), lines, order];
    }
    piece(kind, line) {
      this.n += 1; const P = this.F.pieces[kind], [w, d, h] = P.size;
      return { id: "f" + this.n, kind, line, name: P.name, w, d, h, rug: !!P.rug, boxed: false, ready: null };
    }
    checkout(basket, atHome) {
      const [total, , runs] = this.quote(basket, atHome);
      this.E.spend(total, this.acct, this.R.company.name + ": " + basket.length + " piece" + (basket.length === 1 ? "" : "s"));
      const got = [], made = new Map();
      for (const it of basket) { const p = this.piece(it.kind, it.line); made.set(it, p); got.push(p.id); if (it.how === "instant") this.owned.push(p); }
      for (const r of runs) {
        const ps = [];
        for (const it of r.items) {
          const p = made.get(it);
          p.boxed = this.flatPacked(p.kind);
          p.fitted = !!it.assembled && p.boxed;             // the fitter comes with the van and puts it together in place
          if (it.at) p.at = it.at.slice();
          ps.push(p);
        }
        this.orders.push({ id: "o" + (this.orders.length + 1), unit: r.unit, day: r.day, slot: r.slot, pieces: ps, landed: 0 });
      }
      return [total, got];
    }
    // ---- the delivery (FURN-027, FURN-028)
    legs(o) {
      const D = this.R.delivery, drive = this.routeM(o.unit) / D.van_m_per_min, down = [];
      let t = drive;
      for (const p of o.pieces) { t += D.carry_min; if (p.fitted) t += this.assemblyMin(p.kind); down.push(t); }
      return { drive, down, leave: t, back: t + drive };
    }
    van(o, day, minute) {
      const t = (day - o.day) * 1440 + minute - this.R.delivery.slots[o.slot][0], { drive, down, leave, back } = this.legs(o);
      const landed = down.filter(x => t >= x - 1e-9).length;
      if (t < 0) return { phase: "booked", f: 0, landed: 0 };
      if (t < drive) return { phase: "out", f: drive ? t / drive : 1, landed: 0 };
      if (t < leave) return { phase: "at the door", f: leave > drive ? (t - drive) / (leave - drive) : 1, landed };
      if (t < back) return { phase: "back", f: back > leave ? (t - leave) / (back - leave) : 1, landed };
      return { phase: "done", f: 1, landed };
    }
    advance(day, minute) {
      const fresh = [];
      for (const o of this.orders) {
        const n = this.van(o, day, minute).landed;
        while (o.landed < n) {
          const p = Object.assign({}, o.pieces[o.landed]); o.landed += 1;   // (the order keeps the piece as it was booked: the van's times stay what they were)
          if (p.fitted) p.boxed = false;
          delete p.fitted;
          if (this.holds(o.unit)) (this.arrived[o.unit] = this.arrived[o.unit] || []).push(p);
          else { this.owned.push(p); this.notes.push([day, minute, "Your " + p.name.toLowerCase() + " could not be delivered: " + o.unit + " is no longer yours. It is with your belongings."]); }
          fresh.push(p.id);
        }
      }
      return fresh;
    }
    onItsWay() { return this.orders.flatMap(o => o.pieces.slice(o.landed).map(p => p.id)); }
    moveOut(unit) { this.owned.push(...(this.arrived[unit] || [])); delete this.arrived[unit]; }
    // ---- putting a flat-packed piece together (FURN-029)
    find(pid) {
      for (const p of this.owned) if (p.id === pid) return ["belongings", p];
      for (const u of Object.keys(this.arrived).sort()) for (const p of this.arrived[u]) if (p.id === pid) return [u, p];
      return no("you have no such piece here");
    }
    isBoxed(pid, day, minute) { const p = this.find(pid)[1]; return !!p.boxed && !(p.ready !== null && day * 1440 + minute >= p.ready - 1e-9); }
    asPlaced(pid, day, minute) {
      const p = this.find(pid)[1], q = { id: p.id, kind: p.kind, line: p.line, name: p.name, w: p.w, d: p.d, h: p.h, rug: p.rug };
      if (this.isBoxed(pid, day, minute)) { const [w, d, h] = this.boxes(p.kind)[1]; Object.assign(q, { name: p.name + " (in its boxes)", w, d, h, boxed: true }); }
      return q;
    }
    assemble(pid, day, minute) {
      const [where, p] = this.find(pid);
      if (!this.isBoxed(pid, day, minute)) no("that piece is not in its boxes");
      if (where === "belongings") no("put it down in a home of yours first");
      if (p.ready !== null) no("that piece is being put together");
      const m = this.assemblyMin(p.kind);
      p.ready = day * 1440 + minute + m;
      return m;
    }
    fitter(pid, day, slot) {
      const [where, p] = this.find(pid);
      if (!this.isBoxed(pid, this.E.day, this.E.minute)) no("that piece is not in its boxes");
      if (where === "belongings") no("put it down in a home of yours first");
      if (p.ready !== null) no("that piece is being put together");
      if (!this.slots().some(q => q[0] === day && q[1] === slot)) no("that slot cannot be booked");
      const fee = this.assemblyFee(p.kind);
      this.E.spend(fee, this.acct, this.R.company.name + ": a fitter for the " + p.name.toLowerCase());
      p.ready = day * 1440 + this.R.delivery.slots[slot][0] + this.assemblyMin(p.kind);
      return fee;
    }
    // ---- the company's money (FURN-024)
    found(day, minute) {
      const f = this.R.company.float;
      if (f && !this.E.L.balance(this.acct)) this.E.L.post(day || 0, minute || 0, this.R.company.name + " opens", [["sys:founding", -f], [this.acct, f]]);
    }
    settle(day) {
      const c = this.R.company;
      for (let d = this.settled + 1; d <= day; d++) {
        const over = this.E.L.balance(this.acct) - c.float;
        if (over > 0) { const wages = Math.floor(over * c.wages_bp / 10000); this.E.L.post(d + 1, 0, c.name + ": wages and stock", [[this.acct, -over], ["sys:wages", wages], ["sys:spend", over - wages]]); }
        this.settled = d;
      }
    }
    // ---- the save (QST-008)
    save() { return clone({ n: this.n, owned: this.owned, orders: this.orders, arrived: this.arrived, notes: this.notes, has_catalogue: this.hasCatalogue, settled: this.settled }); }
    load(s) { const c = clone(s); this.n = c.n; this.owned = c.owned; this.orders = c.orders; this.arrived = c.arrived; this.notes = c.notes; this.hasCatalogue = c.has_catalogue; this.settled = c.settled; }
  }

  // FURN-028: where a delivered piece stands: where the player marked it if nothing is wrong with it there, else the free
  // spot on the grid nearest the way in; null when there is no room (furniture.set_down)
  function setDown(room, keep, ceiling, layout, piece, entry) {
    const was = new Set(Furnish.problems(room, keep, ceiling, layout).map(q => q[0]));
    const fits = (x, z, yaw) => { const q = Object.assign({}, piece, { x, z, yaw }); delete q.at;
      return Furnish.problems(room, keep, ceiling, layout.concat([q])).some(r => !was.has(r[0])) ? null : q; };
    if (piece.at) { const q = fits(piece.at[0], piece.at[1], piece.at[2]); if (q) return q; }
    const g = .25; let best = null;
    for (let x = room[0] + g; x < room[2]; x += g) for (let z = room[1] + g; z < room[3]; z += g) {
      const dist = Math.hypot(x - entry[0], z - entry[1]);
      if (best === null || dist < best[0] - 1e-9) { const q = fits(Math.round(x * 100) / 100, Math.round(z * 100) / 100, 0); if (q) best = [dist, q]; }
    }
    return best ? best[1] : null;
  }

  const SCRIPT_MONEY = 600000;
  function runScript(R, F, E, script) {                    // furniture.run_script(), op for op
    E.advance(0, 440); E.newCharacter(); E.openAccount("current");
    E.L.post(0, 440, "Test money", [["sys:start", -SCRIPT_MONEY], ["p:current", SCRIPT_MONEY]]);
    const held = { "u:mine": true, "u:lost": true }, S = new Shop(R, F, E, u => !!held[u], u => u === "u:mine" ? 800 : 2000), out = [];
    S.found();
    for (const op of script) {
      try {
        const k = op[0]; let r;
        if (k === "tag") r = S.tag(op[1], op[2]);
        else if (k === "checkout" || k === "home") r = S.checkout(clone(op[1]), k === "home");
        else if (k === "catalogue") { S.takeCatalogue(); r = null; }
        else if (k === "clock") { E.advance(op[1], op[2]); S.settle(op[1] - 1); r = S.advance(op[1], op[2]); }
        else if (k === "van") { const v = S.van(S.orders.find(o => o.id === op[1]), E.day, E.minute); r = [v.phase, Math.round(v.f * 1e6) / 1e6, v.landed]; }
        else if (k === "assemble") r = S.assemble(op[1], E.day, E.minute);
        else if (k === "boxed") r = S.isBoxed(op[1], E.day, E.minute);
        else if (k === "fitter") r = S.fitter(op[1], op[2], op[3]);
        else if (k === "lose") { held[op[1]] = false; r = null; }
        else no("no such op");
        out.push(["ok", r]);
      } catch (e) { if (!(e instanceof Econ.Refused)) throw e; out.push(["refused", e.message]); }
    }
    const arrived = {}; for (const u of Object.keys(S.arrived).sort()) arrived[u] = S.arrived[u].map(p => p.id);
    return { results: out, owned: S.owned.map(p => p.id), arrived, on_its_way: S.onItsWay(), notes: S.notes, company: E.L.balance(S.acct), player: E.totalMoney(),
      supply: E.L.supply(), words: Econ.fmt(E.L.balance(S.acct)) };
  }
  function selfTest(life, lots) {                          // the Python's fixed run, result for result
    const C = life.furniture.checks, got = runScript(life.furniture.rules, life.builder.furnish.rules, new Econ.Economy(life.economy, lots, {}, {}), C.script), bad = [];
    got.results.forEach((r, k) => { if (JSON.stringify(r) !== JSON.stringify(C.want.results[k])) bad.push("op " + k + " " + JSON.stringify(C.script[k]).slice(0, 60) + ": " + JSON.stringify(r) + " should be " + JSON.stringify(C.want.results[k])); });
    for (const k of ["owned", "arrived", "on_its_way", "notes", "company", "player", "supply", "words"]) if (JSON.stringify(got[k]) !== JSON.stringify(C.want[k])) bad.push(k + ": " + JSON.stringify(got[k]) + " should be " + JSON.stringify(C.want[k]));
    return bad;
  }
  return { Shop, setDown, runScript, selfTest };
})();
