"use strict";
// Money coming in, shown and heard (SND-019, SND-020), as the owner chose from feedback_samples.html: when a quest or a job
// pays, coins spring out around the player, lie a moment, are drawn back in and fly to the purse, each ringing a note higher;
// when the bank pays interest a slip slides out under the purse and pays coins up into the bank balance. Every number is
// data: D.sound.rewards (World/rules/sound.json). The sounds are made by sound.js (Sound.reward).
//
// Nothing here moves money. The ledger has already been posted (economy.js); this reads the new transactions, and while
// the coins are on their way it tells the purse how much not to show yet (held), so the number counts up as they land.
// events() is the same rule as reward_events() in Tools/worldgen/world.py, and the page checks it against Python's answer
// (D.life.reward_checks, packed by build_play.py).
//
// Loaded after sound.js and before life.js. life.js calls frame() each frame; play.js calls draw() with the labels.
var Rewards = (() => {
  const D = window.WASTED, RW = D.sound && D.sound.rewards, C = RW && RW.coins;
  const rnd = (a, b) => a + Math.random() * (b - a), el = id => document.getElementById(id);
  const sound = (what, i, n) => { if (window.Sound) Sound.reward(what, i, n); };

  // ---- which transactions are shown (the same as world.reward_events)
  function coinsFor(amount) { let n = 0; for (const [least, coins] of C.reward_tiers) if (amount >= least) n = coins; return n; }
  function events(tx) {
    const lg = RW.ledger, out = [], interest = {};
    for (const t of tx) {
      const memo = t[3], lines = t[4], got = {}; let paid = 0;
      for (const [a, v] of lines) if (a.startsWith("p:") && v > 0) { got[a] = (got[a] || 0) + v; paid += v; }
      if (!paid) continue;
      if (lines.some(([a, v]) => a === lg.reward_from && v < 0)) out.push({ kind: "reward", amount: paid, coins: coinsFor(paid), memo, to: got });
      else if (memo.startsWith(lg.interest_memo) && lines.some(([a, v]) => a === lg.interest_from && v < 0)) for (const a in got) interest[a] = (interest[a] || 0) + got[a];
    }
    const accounts = Object.keys(interest).sort();
    if (accounts.length) out.push({ kind: "interest", amount: accounts.reduce((s, a) => s + interest[a], 0), accounts, to: interest });
    return out;
  }
  function selfTest() {                                  // the page's answer against Python's, like Econ.selfTest
    const K = D.life && D.life.reward_checks; if (!K) return ["no reward checks in city_data.js"];
    const got = events(K.tx).map(e => { const o = Object.assign({}, e); delete o.to; return o; });
    return JSON.stringify(got) === JSON.stringify(K.want) ? [] : ["reward events differ from Python: " + JSON.stringify(got) + " not " + JSON.stringify(K.want)];
  }

  // ---- what is on its way
  let seen = 0, clock = 0, coins = [], flies = [], sparks = [], puffs = [], timeline = [], run = 0, last = { x: 0, y: 0, z: 0 }, onPaid = null;
  const held = { cash: 0, bank: 0 }, bump = { cash: 0, bank: 0 }, st = { rewards: 0, interest: 0, taken: 0, landed: 0, banners: 0 };
  const purseOf = a => a === "p:cash" ? "cash" : "bank";
  const later = (dt, fn) => timeline.push({ t: clock + dt, fn, run });
  function reset(n) { run++; seen = n || 0; coins = []; flies = []; sparks = []; puffs = []; timeline = []; held.cash = held.bank = 0; banners = []; bannerUntil = 0; for (const id of ["slip", "feat"]) if (el(id)) el(id).classList.remove("on"); }
  function glow(k) { const e = el("purse-" + k); if (!e) return; e.classList.add("glow"); later(.5, () => e.classList.remove("glow")); }
  function shares(e, n) {                                // the amount in n parts, each for one purse; the parts add up exactly
    const parts = []; let k = 0, left = 0;
    for (const a in e.to) { left += e.to[a]; }
    const each = Math.floor(e.amount / n), main = purseOf(Object.keys(e.to).sort((a, b) => e.to[b] - e.to[a])[0]);
    for (; k < n; k++) parts.push([main, k === n - 1 ? left - each * (n - 1) : each]);
    return parts;
  }
  function land(part, e) {                               // one coin's worth reaches the purse
    held[part[0]] -= part[1]; bump[part[0]] = 1; st.landed++;
    if (++e.landed < e.n) return;
    if (e.dev) later(2.4, () => { held[part[0]] += e.amount; });   // a showing only: the purse counted up past the real balance, and goes back
    const k = part[0];
    if (e.kind === "reward") { sound("quest_end"); puffs.push({ at: k, n: 16, v: 150 }); glow(k); if (onPaid) onPaid(e.amount); }
    else { later(.12, () => { sound("interest_end"); puffs.push({ at: k, n: 12, v: 120 }); glow(k); if (onPaid) onPaid(e.amount); }); later(C.slip_s, () => el("slip") && el("slip").classList.remove("on")); }
  }
  function reward(e) {                                   // coins spring out around the player, are drawn in, and fly to the purse
    const n = e.n = Math.max(1, e.coins), parts = shares(e, n); e.landed = 0; e.taken = 0; st.rewards++;
    if (!e.dev) for (const p of parts) held[p[0]] += p[1];
    sound("quest_start");
    // Where they land: a ring round the player (two rings when there are many), evenly spaced, each coin thrown so that it
    // comes down on its own spot. They spring out one after another round the ring, and are taken in the same order.
    const two = n > 10, phase = rnd(0, 6.2832), turn = Math.random() < .5 ? -1 : 1;
    for (let i = 0; i < n; i++) later(C.start_s + i * C.gap_s, () => {
      if (i % 2 === 0) sound("coin_out");
      const a = phase + turn * (6.2832 * i / n + rnd(-.08, .08)), r = (two ? (i % 3 === 0 ? C.ring_m[0] : C.ring_m[1]) : (C.ring_m[0] + C.ring_m[1]) / 2) * rnd(.92, 1.08);
      const vy = rnd(C.up_mps[0], C.up_mps[1]), T = (vy + Math.sqrt(vy * vy + 2 * C.fall_mps2 * (C.from_m - C.size_m))) / C.fall_mps2;
      coins.push({ x: last.x, y: last.y + C.from_m, z: last.z, vx: Math.cos(a) * r / T, vz: Math.sin(a) * r / T, vy, floor: last.y + C.size_m, spin: rnd(0, 6), vs: rnd(9, 15), ph: rnd(0, 6), age: 0, t: 0, state: 0, bounces: 0, side: turn, trail: [],
        pullAt: C.pull_after_s + i * C.pull_gap_s, hit: false,
        take: () => { sound("coin_take", e.taken++, n); st.taken++; puffs.push({ at: "player", n: 5, v: 90 }); flies.push({ from: "player", to: parts[i][0], t: 0, len: C.fly_s, bend: rnd(-30, 30), coin: true, land: () => land(parts[i], e) }); } });
    });
  }
  function interest(e) {                                 // a slip from the bank slides out under the purse and pays coins up into it
    const n = e.n = C.interest_coins, parts = shares(e, n), s = el("slip"); e.landed = 0; st.interest++;
    if (!e.dev) for (const p of parts) held[p[0]] += p[1];
    if (s) { el("slip-what").textContent = e.accounts.map(a => ({ "p:current": "current account", "p:savings": "savings account" }[a] || a.slice(2))).join(" and "); s.classList.add("on"); }
    later(.3, () => sound("interest_start"));
    for (let i = 0; i < n; i++) later(C.interest_start_s + i * C.interest_gap_s, () => flies.push({ from: "slip", dx: -40 + i * 12, to: parts[i][0], t: 0, len: C.interest_fly_s, bend: -34, coin: true, land: () => { sound("interest_coin", i); land(parts[i], e); } }));
  }

  // ---- an achievement (QST-009, SND-021): the painted banner drops in with sparkles and the fanfare; several wait their turn
  let banners = [], bannerUntil = 0;
  function achieve(a) { banners.push(a); }
  function nextBanner() {
    const b = el("feat"); if (!b || clock < bannerUntil || !banners.length || document.body.classList.contains("menuing") || document.body.classList.contains("designing")) return;
    const a = banners.shift(), secs = (D.life && D.life.achievements && D.life.achievements.banner_s) || 3.6; bannerUntil = clock + secs + .7; st.banners++;
    el("feat-name").textContent = a.name; el("feat-line").textContent = a.line; b.classList.add("on"); sound("achievement");
    later(.5, () => { for (let i = 0; i < 5; i++) puffs.push({ at: "feat", n: 8, v: 170, dx: rnd(-120, 120), dy: rnd(-10, 20) }); });
    later(secs, () => b.classList.remove("on"));
  }

  // the showing on its own, for dev mode (dev.js): no transaction, so no money moves; the purse counts up as if paid, then goes back to the real balance
  function show(kind, amount) {
    if (!RW) return;
    if (kind === "reward") reward({ kind, amount, coins: coinsFor(amount), memo: "dev", to: { "p:cash": amount }, dev: true });
    else interest({ kind: "interest", amount, accounts: ["p:savings"], to: { "p:savings": amount }, dev: true });
  }

  // each frame (life.js): new ledger transactions start their show; returns how much of the money that came in is being shown here
  function frame(dt, tx, pl, paid) {
    if (!RW) return 0;
    let shown = 0; onPaid = paid; last = { x: pl.x, y: pl.y, z: pl.z };
    if (tx.length < seen) seen = tx.length;
    if (tx.length > seen) { for (const e of events(tx.slice(seen))) { shown += e.amount; (e.kind === "reward" ? reward : interest)(e); } seen = tx.length; }
    clock += dt; nextBanner();
    for (const e of timeline.filter(e => e.t <= clock)) if (e.run === run) e.fn(); timeline = timeline.filter(e => e.t > clock);
    const chest = { x: pl.x, y: pl.y + C.from_m, z: pl.z };
    for (const c of coins) {
      c.age += dt; c.spin += c.vs * dt;
      if (c.state === 0) {                                // thrown out: falls to its spot and bounces twice
        c.vy -= C.fall_mps2 * dt; c.x += c.vx * dt; c.y += c.vy * dt; c.z += c.vz * dt;
        if (c.y < c.floor) { c.y = c.floor; if (!c.hit) { c.hit = true; sound("coin_land"); } c.vy *= -C.bounce; c.vx *= .3; c.vz *= .3; c.vs = Math.max(4, c.vs * .6); if (++c.bounces >= 2 || c.vy < 1) { c.state = 1; c.t = 0; } }
      } else if (c.state === 1) {                         // waiting: it stands up off the ground, turning slowly and bobbing
        c.t += dt; const k = Math.min(1, c.t / .4), e = 1 - (1 - k) * (1 - k) * (1 - k);
        c.y = c.floor + e * C.hover_m + Math.sin(c.age * 3.6 + c.ph) * .03 * e; c.vs += (3 - c.vs) * Math.min(1, dt * 5);
      }
      if (c.state < 2 && c.age >= c.pullAt) { c.state = 2; c.t = 0; c.from = [c.x, c.y, c.z]; }
      if (c.state === 2) {                                // taken: a small lift, then a curving swoop in to the player, wherever they have walked to
        c.t += dt; c.vs = 18; c.trail.push([c.x, c.y, c.z]); if (c.trail.length > 6) c.trail.shift();
        if (c.t < C.lift_s) c.y = c.from[1] + c.t / C.lift_s * .16;
        else {
          const u = Math.min(1, (c.t - C.lift_s) / C.pull_s), e = u * u, sx = c.from[0], sy = c.from[1] + .16, sz = c.from[2], dx = chest.x - sx, dz = chest.z - sz, d = Math.hypot(dx, dz) || 1;
          const mx = (sx + chest.x) / 2 - dz / d * .55 * c.side, my = Math.max(sy, chest.y) + .45, mz = (sz + chest.z) / 2 + dx / d * .55 * c.side, a = (1 - e) * (1 - e), b = 2 * e * (1 - e), q = e * e;
          c.x = a * sx + b * mx + q * chest.x; c.y = a * sy + b * my + q * chest.y; c.z = a * sz + b * mz + q * chest.z; c.small = 1 - .45 * e;
          if (u >= 1) { c.done = true; c.take(); }
        }
      }
    }
    coins = coins.filter(c => !c.done);
    for (const f of flies) { f.t += dt / f.len; if (f.t >= 1) { f.done = true; f.land(); } }
    flies = flies.filter(f => !f.done);
    for (const s of sparks) { s.t += dt; s.vy += 300 * dt; s.x += s.vx * dt; s.y += s.vy * dt; }
    sparks = sparks.filter(s => s.t < s.life);
    for (const k of ["cash", "bank"]) { bump[k] = Math.max(0, bump[k] - dt * 5); const e = el("purse-" + k); if (e) e.style.transform = bump[k] ? "scale(" + (1 + bump[k] * .13).toFixed(3) + ")" : ""; }
    return shown;
  }

  // ---- drawn over the picture with the labels (play.js): proj(x, y, z) gives [x, y, pixels to a metre] on the canvas, or null behind the eye
  // The coin is painted brass in the town's own way (ART_DIRECTION W1): matte and mottled, two close tones blotched together,
  // with a raised rim, a ring of beads and a stamped B. Its face is painted once and squeezed as the coin turns; the edge
  // shows its thickness; the glow round it is lantern light (W6), not shine.
  let face = null;
  function texture() {
    if (face) return face;
    const S = 128, R = S / 2, cv = document.createElement("canvas"), g = cv.getContext("2d"); cv.width = cv.height = S;
    let seed = 7; const rand = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
    g.beginPath(); g.arc(R, R, R - 1, 0, 7); g.clip();
    const base = g.createLinearGradient(S * .2, S * .08, S * .8, S * .95); base.addColorStop(0, "#edc466"); base.addColorStop(.55, "#d6a242"); base.addColorStop(1, "#b07a28");
    g.fillStyle = base; g.fillRect(0, 0, S, S);
    for (let k = 0; k < 60; k++) { g.fillStyle = k % 2 ? "rgba(248,218,138,.15)" : "rgba(142,92,26,.13)"; g.beginPath(); g.arc(rand() * S, rand() * S, 5 + rand() * 13, 0, 7); g.fill(); }
    g.lineWidth = S * .09; g.strokeStyle = "rgba(196,144,54,.9)"; g.beginPath(); g.arc(R, R, R - S * .045, 0, 7); g.stroke();                 // the raised rim
    g.lineWidth = 2.5; g.strokeStyle = "rgba(104,66,16,.7)"; g.beginPath(); g.arc(R, R, R - S * .1, 0, 7); g.stroke();                       // and the groove inside it
    g.lineWidth = 1.5; g.strokeStyle = "rgba(255,236,172,.5)"; g.beginPath(); g.arc(R, R, R - S * .1 - 2.5, Math.PI * .9, Math.PI * 1.7); g.stroke();
    for (let k = 0; k < 26; k++) { const a = k / 26 * 6.2832, bx = R + Math.cos(a) * (R - S * .17), by = R + Math.sin(a) * (R - S * .17);   // beads
      g.fillStyle = "rgba(104,66,16,.55)"; g.beginPath(); g.arc(bx + .8, by + .8, 2.2, 0, 7); g.fill(); g.fillStyle = "rgba(244,212,128,.8)"; g.beginPath(); g.arc(bx, by, 2, 0, 7); g.fill(); }
    g.font = "bold " + S * .56 + "px Constantia, 'Palatino Linotype', Georgia, serif"; g.textAlign = "center"; g.textBaseline = "middle";
    g.fillStyle = "rgba(98,62,14,.75)"; g.fillText("B", R + 2.5, R + 5.5); g.fillStyle = "rgba(255,238,178,.6)"; g.fillText("B", R - 1.5, R + 1.5); g.fillStyle = "#c08d34"; g.fillText("B", R, R + 3);   // stamped: shade below, light above
    return face = cv;
  }
  function halo(g, x, y, r, a) {                          // lantern light round a coin
    if (a <= 0) return; g.globalCompositeOperation = "lighter";
    const h = g.createRadialGradient(x, y, r * .2, x, y, r); h.addColorStop(0, "rgba(255,172,84," + (.36 * a).toFixed(3) + ")"); h.addColorStop(1, "rgba(255,172,84,0)");
    g.fillStyle = h; g.fillRect(x - r, y - r, r * 2, r * 2); g.globalCompositeOperation = "source-over";
  }
  function coin(g, x, y, r, spin, glow) {
    const c = Math.cos(spin), s = Math.sin(spin), rx = Math.max(r * .09, r * Math.abs(c)), th = r * .22 * Math.abs(s), dir = s * c > 0 ? 1 : -1;
    halo(g, x, y, r * 2.8, glow || 0);
    const edge = ["#8c6120", "#a5762c", "#7a5219"];                                                   // the edge, seen as the coin turns
    for (let k = 3; k >= 1; k--) { g.fillStyle = edge[k - 1]; g.beginPath(); g.ellipse(x + dir * th * k / 3, y, rx, r, 0, 0, 7); g.fill(); }
    g.drawImage(texture(), x - rx, y - r, rx * 2, r * 2);
    const turned = 1 - Math.abs(c); if (turned > .02) { const a = g.globalAlpha; g.globalAlpha = a * turned * .4; g.fillStyle = "#4e3210"; g.beginPath(); g.ellipse(x, y, rx, r, 0, 0, 7); g.fill(); g.globalAlpha = a; }   // darker as it turns away from the eye
  }
  function glint(g, x, y, r) {                            // a small four-pointed twinkle
    g.globalCompositeOperation = "lighter"; g.fillStyle = "rgba(255,240,200,.85)"; g.beginPath();
    g.moveTo(x, y - r); g.quadraticCurveTo(x, y, x + r, y); g.quadraticCurveTo(x, y, x, y + r); g.quadraticCurveTo(x, y, x - r, y); g.quadraticCurveTo(x, y, x, y - r); g.fill(); g.globalCompositeOperation = "source-over";
  }
  function draw(g, proj, sc) {
    if (!RW || !(coins.length || flies.length || sparks.length || puffs.length)) return;
    const W = g.canvas.width, H = g.canvas.height, box = g.canvas.getBoundingClientRect();
    const spot = id => { const e = el(id), r = e && e.getBoundingClientRect(); return r && r.width ? [(r.left - box.left + r.width / 2) * sc, (r.top - box.top + r.height / 2) * sc] : [W * .1, H * .2]; };
    const p0 = proj(last.x, last.y + C.from_m, last.z), where = { player: p0 && p0[0] > 0 && p0[0] < W && p0[1] > 0 && p0[1] < H ? [p0[0], p0[1]] : [W / 2, H * .92], cash: spot("purse-cash"), bank: spot("purse-bank"), slip: spot("slip") };
    g.globalAlpha = 1;
    // The coins, far ones first. Each has a shadow and a pool of its light on the ground under it; one that is behind the
    // player's body (third person) is not drawn, since this layer lies over the whole picture.
    const feet = proj(last.x, last.y, last.z), head = proj(last.x, last.y + 1.75, last.z), body = feet && head && p0 && feet[1] > head[1] ? { x0: feet[0] - .3 * feet[2], x1: feet[0] + .3 * feet[2], y0: head[1], y1: feet[1], near: p0[2] } : null;
    const seenCoins = [];
    for (const c of coins) {
      const p = proj(c.x, c.y, c.z); if (!p) continue;
      if (body && p[2] < body.near * .97 && p[0] > body.x0 && p[0] < body.x1 && p[1] > body.y0 && p[1] < body.y1) continue;
      seenCoins.push([c, p]);
    }
    seenCoins.sort((a, b) => a[1][2] - b[1][2]);
    for (const [c] of seenCoins) {
      const f = proj(c.x, c.floor - C.size_m + .01, c.z); if (!f) continue;
      const up = Math.min(1, (c.y - c.floor) / 1.2), r = C.size_m * f[2] * (1.1 - up * .4);
      g.fillStyle = "rgba(20,14,8," + (.38 * (1 - up)).toFixed(3) + ")"; g.beginPath(); g.ellipse(f[0], f[1], r, r * .34, 0, 0, 7); g.fill();
      if (c.state === 1) { g.globalCompositeOperation = "lighter"; const h = g.createRadialGradient(f[0], f[1], 0, f[0], f[1], r * 4); h.addColorStop(0, "rgba(255,172,84,.16)"); h.addColorStop(1, "rgba(255,172,84,0)"); g.save(); g.translate(f[0], f[1]); g.scale(1, .34); g.translate(-f[0], -f[1]); g.fillStyle = h; g.fillRect(f[0] - r * 4, f[1] - r * 4, r * 8, r * 8); g.restore(); g.globalCompositeOperation = "source-over"; }
    }
    for (const [c, p] of seenCoins) {
      const pop = c.age < .14 ? (k => 1 + 2.2 * Math.pow(k - 1, 3) + 1.2 * Math.pow(k - 1, 2))(c.age / .14) : 1;       // it pops into being a little too big, then settles
      const r = Math.max(3 * sc, C.size_m * p[2]) * pop * (c.small || 1) * (c.state === 2 && c.t < C.lift_s ? 1 + .18 * c.t / C.lift_s : 1);
      c.trail.forEach((q, k) => { const t = proj(q[0], q[1], q[2]); if (t) halo(g, t[0], t[1], r * (1 + k * .22), (k + 1) / c.trail.length * .55); });
      coin(g, p[0], p[1], r, c.spin, c.state === 1 ? .75 + .25 * Math.sin(c.age * 3.6 + c.ph) : c.state === 2 ? 1 : .45);
      const tw = Math.sin(c.age * 2.3 + c.ph * 5); if (c.state === 1 && tw > .93) glint(g, p[0] + r * .55, p[1] - r * .6, r * .7 * (tw - .93) / .07);
    }
    for (const f of flies) {
      if (!f.a) f.a = [where[f.from][0] + (f.dx || 0) * sc, where[f.from][1]];
      const b = where[f.to], k = Math.min(1, f.t), e = k * k * (3 - 2 * k), mx = (f.a[0] + b[0]) / 2 + f.bend * sc, my = Math.min(f.a[1], b[1]) - 50 * sc;
      const x = (1 - e) * (1 - e) * f.a[0] + 2 * e * (1 - e) * mx + e * e * b[0], y = (1 - e) * (1 - e) * f.a[1] + 2 * e * (1 - e) * my + e * e * b[1];
      for (let j = 3; j >= 1; j--) {                      // a fading streak of light behind it
        const kk = Math.max(0, k - j * .07), ee = kk * kk * (3 - 2 * kk);
        halo(g, (1 - ee) * (1 - ee) * f.a[0] + 2 * ee * (1 - ee) * mx + ee * ee * b[0], (1 - ee) * (1 - ee) * f.a[1] + 2 * ee * (1 - ee) * my + ee * ee * b[1], (11 - j * 2) * sc, .5 - j * .12);
      }
      coin(g, x, y, (f.from === "slip" ? 7 : 6.5 - 1.5 * e) * sc, clock * 13 + f.a[0], .8);
    }
    for (const p of puffs) for (let i = 0; i < p.n; i++) { const a = rnd(0, 6.2832), v = rnd(.3, 1) * p.v * sc, o = where[p.at] || (where[p.at] = spot(p.at)); sparks.push({ x: o[0] + (p.dx || 0) * sc, y: o[1] + (p.dy || 0) * sc, vx: Math.cos(a) * v, vy: Math.sin(a) * v - p.v * sc * .3, t: 0, life: rnd(.4, .9) }); }
    puffs = [];
    for (const s of sparks) { const a = 1 - s.t / s.life; if (a > 0) { g.globalAlpha = a; glint(g, s.x, s.y, 4.5 * sc * (.5 + a * .5)); g.globalAlpha = 1; } }
  }

  return { events, coinsFor, selfTest, reset, frame, draw, achieve, show, held, state: () => Object.assign({ coins: coins.length, flies: flies.length, sparks: sparks.length, held: Object.assign({}, held), seen }, st) };
})();
