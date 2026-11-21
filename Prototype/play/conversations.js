"use strict";
// Talking to residents (CITY-007, F-NPC-TALK level 1). Conversations are data: World/rules/dialogue.json, packed into
// city_data.js as D.dialogue by Tools/worldgen/build_play.py. Loaded BEFORE play.js: play.js starts drawing as soon as it
// has run, and a frame can come before a later script has downloaded. Nothing here touches play.js's globals (pl, cam,
// clock, resident, target...) until play.js calls in through the few hooks named in Talk below.
//
// While a resident talks, their day is held: they stop where they are and turn to the player. When the talk ends their
// routine carries on from that point, running late by the time spent talking, until they next stand still somewhere
// (then they are back on the clock). Nothing is stored per frame: only who is late and by how much.
var Talk = (() => {                    // var: a property of window, so index.html can stand in for it if this file is blocked
  const DL = window.WASTED.dialogue, late = new Map(), flags = new Map();
  const DAYS = { Mon: "Monday", Tue: "Tuesday", Wed: "Wednesday", Thu: "Thursday", Fri: "Friday", Sat: "Saturday", Sun: "Sunday" };
  let cand = null, s = null;                             // the nearest resident to talk to this frame; the talk under way
  const el = id => document.getElementById(id);

  function where(i, m) {                                 // resident(i, m), but held while talking and late after it
    if (s && s.i === i) return s.r;
    const dl = late.get(i); if (!dl) return resident(i, m);
    const m2 = (m - dl + 1440) % 1440, row = D.plan[i], k = pieceAt(row, m2);
    if (row[k][1] === 0 && pieceAt(row, m) === k) { late.delete(i); return resident(i, m); }   // standing in the same place either way: back on the clock
    return resident(i, m2);
  }
  function consider(i, r, d) {                           // called for each resident with a body; keeps the nearest one in reach
    if (s || r.pose === 3 || d > 2.2 || Math.abs(r.y - pl.y) > 1.2) return;
    if (!cand || d < cand.d) cand = { i, d, r };
  }
  function pick() {                                      // after everyone is placed: offer the talk if no seat or lift is nearer
    const c = cand; cand = null;
    if (s || !c || pl.pose > 1) return;
    if (target && target.type !== "talk" && Math.hypot(target.x - pl.x, target.z - pl.z) < c.d) return;
    target = { type: "talk", i: c.i, r: c.r, x: c.r.x, z: c.r.z, label: "Talk to " + D.residents[c.i].n };
  }
  function conversationFor(i, r) {
    const own = DL.residents[i]; if (own) return own;
    const role = D.roles[D.residents[i].r], act = D.acts[r.act], walking = !!r.walking;
    for (const x of DL.rules) {
      if (x.roles && x.roles.indexOf(role) < 0) continue;
      if (x.acts && x.acts.indexOf(act) < 0) continue;
      if ("walking" in x && x.walking !== walking) continue;
      return x.conversation;
    }
  }
  function fill(text) {
    const m = clock.m, res = D.residents[s.i], r = s.r0;
    const v = {
      name: res.n, role: DL.role_words[D.roles[res.r]], place: r.lot ? r.lot.name : D.city.name,
      doing: r.walking ? GOING[D.acts[r.act]] || "on my way somewhere" : DOING[D.acts[r.act]] || D.acts[r.act],
      greeting: m < 300 ? "Good evening" : m < 720 ? "Good morning" : m < 1080 ? "Good afternoon" : "Good evening",
      weekday: DAYS[D.day.weekday] || D.day.weekday, season: D.day.season.toLowerCase(),
    };
    return text.replace(/\{(\w+)\}/g, (_, k) => k in v ? v[k] : "{" + k + "}");
  }
  const has = (f, c) => c.every(x => x[0] === "!" ? !f.has(x.slice(1)) : f.has(x));
  function show(nid) {
    if (nid === null) return close();
    const node = s.conv.nodes[nid], f = flags.get(s.i);
    s.node = nid; s.opts = node.options.filter(o => has(f, o.if || []));
    el("talk-say").textContent = fill(node.say); el("talk").dataset.mood = node.mood;
    const ol = el("talk-opts"); ol.textContent = "";
    s.opts.forEach((o, n) => {
      const b = document.createElement("button"); b.type = "button";
      b.innerHTML = "<kbd>" + (n + 1) + "</kbd>"; b.appendChild(document.createTextNode(fill(o.text)));
      b.addEventListener("click", () => choose(n));
      const li = document.createElement("li"); li.appendChild(b); ol.appendChild(li);
    });
  }
  function choose(n) {
    const o = s && s.opts[n]; if (!o) return;
    const f = flags.get(s.i); for (const x of o.do || []) f.add(x);
    show(o.next);
  }
  function open(t) {
    const r0 = where(t.i, clock.m); if (!r0) return;
    const cid = conversationFor(t.i, r0), conv = DL.conversations[cid];
    const r = Object.assign({}, r0); if (r.pose === 1) { r.pose = 0; r.walking = false; r.ph = 0; }   // a walker stops and stands
    s = { i: t.i, r, r0, conv, cid, m0: clock.m, node: null, opts: [] };
    if (!flags.has(t.i)) flags.set(t.i, new Set());
    for (const k in keys) keys[k] = false; pl.moving = false;
    pl.yaw = yawOf(r.x - pl.x, r.z - pl.z); cam.yaw = cam.fp ? pl.yaw : pl.yaw - .45; cam.pitch = cam.fp ? -.08 : -.12;   // face them; in third person look past the shoulder
    if (document.pointerLockElement && document.exitPointerLock) document.exitPointerLock();
    const res = D.residents[t.i];
    el("talk-name").textContent = res.n; el("talk-role").textContent = DL.role_words[D.roles[res.r]];
    el("talk").style.display = "block"; el("keys").dataset.was = el("keys").style.display; el("keys").style.display = "none";
    target = null; show(conv.start);
  }
  function close() {
    if (!s) return;
    const spent = (clock.m - s.m0 + 1440) % 1440, dl = (late.get(s.i) || 0) + spent;
    if (spent > 0 && dl < 180) late.set(s.i, dl); else late.delete(s.i);   // held more than three hours: they just pick up the day where it is
    el("talk").style.display = "none"; el("keys").style.display = el("keys").dataset.was || "block";
    s = null;
  }
  function step(dt) {                                    // each frame while talking: the resident turns to face the player
    if (!s || s.r.pose !== 0) return;
    let da = yawOf(pl.x - s.r.x, pl.z - s.r.z) - s.r.yaw; da = Math.atan2(Math.sin(da), Math.cos(da));
    s.r.yaw += da * Math.min(1, dt * 5);
  }
  function key(e) {                                      // every key goes here while talking
    if (e.repeat) return;
    const d = /^(Digit|Numpad)([1-9])$/.exec(e.code);
    if (d) choose(+d[2] - 1);
    else if (e.code === "Escape") close();
  }
  return {
    get on() { return !!s; }, where, consider, pick, open, close, step, key, choose,
    state: () => s && { resident: s.i, name: D.residents[s.i].n, conversation: s.cid, node: s.node, say: el("talk-say").textContent, options: s.opts.map(o => fill(o.text)) },
    late: () => [...late],
  };
})();
addEventListener("load", () => { if (window.wasted) window.wasted.talk = Talk; });   // play.js makes window.wasted after this file runs
