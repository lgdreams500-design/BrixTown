"use strict";
// Talking to residents (CITY-007, F-NPC-TALK level 1; quests and jobs join in through life.js, QST-001 to QST-007). Conversations are data: World/rules/dialogue.json, packed into
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
  let cand = null, s = null;
  const SP = window.WASTED.speech, now = () => performance.now() / 1000;

  // ---- lip sync (CHR-009): a line's text becomes mouth shapes in time, as in CHARACTER_FACE §4 (timing from the text, Q-14)
  function speak(text) {
    const T = SP.timing, X = SP.text, fps = T.fps, seq = [], words = [], asks = [];
    let t = 0;
    const put = (shape, d) => { const p = seq[seq.length - 1]; if (p && p.shape === shape) p.d += d; else seq.push({ shape, t0: t, d }); t += d; };
    for (const m of text.toLowerCase().matchAll(/([a-z']+)|([.,!?;:—])/g)) {
      if (m[2]) { put("rest", T.pauses[m[2]] || 0); if (m[2] === "?") asks.push(t); continue; }
      const w = m[1].replace(/'/g, ""), stressed = w.length >= T.stressed_word_letters;
      words.push({ t0: t, long: w.length >= SP.accents.stressed_word_letters });
      for (let k = 0; k < w.length;) {
        const two = w.substr(k, 2), di = X.digraphs[two], ch = di ? two[0] : w[k], shape = di || X.letters[ch];
        k += di ? 2 : 1; if (!shape) continue;
        const vowel = X.vowels.indexOf(ch) >= 0;
        put(shape, (vowel ? (stressed && T.stressed_shapes.indexOf(shape) >= 0 ? T.frames.stressed : T.frames.normal) : T.frames.quick) / fps);
      }
    }
    return { seq, words, asks, end: t };
  }
  const SHAPE = n => { const m = typeof n === "string" ? SP.shapes[n] : n; return { width: m.width * SP.draw.width, opening: m.opening * SP.draw.opening, round: m.round || 0, teeth: m.teeth, tongue: m.tongue, curve: m.curve || 0 }; };
  function faceTarget(t) {                               // what the face should be t seconds into the current line
    const E = SP.expressions[s.mood] || SP.expressions.neutral, A = SP.accents, L = s.line;
    let mouth;
    if (t < L.end) { const k = L.seq.findIndex(q => t < q.t0 + q.d); mouth = SHAPE(L.seq[k < 0 ? L.seq.length - 1 : k].shape); }
    else mouth = SHAPE(s.mood === "neutral" || s.mood === "happy" ? (SP.timing.end[s.mood] || SP.timing.end.default) : E.mouth);
    let lift = t < A.line_start_seconds ? A.line_start_brow * (1 - t / A.line_start_seconds) : 0, nod = 0;
    for (const q of L.asks) if (t > q - A.question_seconds && t < q) lift = Math.max(lift, A.question_brow);
    for (const w of L.words) if (w.long && t >= w.t0 && t < w.t0 + A.nod_seconds) { const u = (t - w.t0) / A.nod_seconds; nod = A.nod * Math.sin(Math.PI * u); lift = Math.max(lift, A.line_start_brow * Math.sin(Math.PI * u)); }
    return { mouth, curve: E.curve + mouth.curve * .5, brow_l: [E.brow_l[0] + lift, E.brow_l[1] + lift], brow_r: [E.brow_r[0] + lift, E.brow_r[1] + lift], lid: E.lid, squint: E.squint, open: E.open, gaze: E.gaze.slice(), nod };
  }
  function blendFace(dt) {
    const f = s.f, g = faceTarget((now() - s.t0) * (s.k || 1)), km = Math.min(1, dt * SP.timing.blend.mouth), ke = Math.min(1, dt * SP.timing.blend.expression), mix1 = (a, b, k) => a + (b - a) * k;
    for (const k of ["width", "opening", "round"]) f.mouth[k] = mix1(f.mouth[k], g.mouth[k], km);
    f.mouth.teeth = g.mouth.teeth; f.mouth.tongue = g.mouth.tongue;
    f.curve = mix1(f.curve, g.curve, ke); f.lid = mix1(f.lid, g.lid, ke); f.squint = mix1(f.squint, g.squint, ke); f.open = mix1(f.open, g.open, ke); f.nod = mix1(f.nod, g.nod, km);
    for (const k of ["brow_l", "brow_r", "gaze"]) for (const j of [0, 1]) f[k][j] = mix1(f[k][j], g[k][j], ke);
  }
  const restFace = () => ({ mouth: SHAPE("rest"), curve: .075, brow_l: [0, 0], brow_r: [0, 0], lid: 0, squint: 0, open: 1, gaze: [0, 0], nod: 0 });
                             // the nearest resident to talk to this frame; the talk under way
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
  function vars() {                                      // what each blank in a line stands for, for this resident now
    const m = clock.m, res = D.residents[s.i], r = s.r0;
    const v = {
      name: res.n, role: DL.role_words[D.roles[res.r]], place: r.lot ? r.lot.name : D.city.name,
      doing: r.walking ? GOING[D.acts[r.act]] || "on my way somewhere" : DOING[D.acts[r.act]] || D.acts[r.act],
      greeting: m < 300 ? "Good evening" : m < 720 ? "Good morning" : m < 1080 ? "Good afternoon" : "Good evening",
      weekday: DAYS[D.day.weekday] || D.day.weekday, season: D.day.season.toLowerCase(),
    };
    Object.assign(v, s.conv.vars || {}, window.Life ? Life.vars() : {});   // {player}, and a job's {pay} {item} {from} {to} {hours} (life.js)
    return v;
  }
  function fill(text) { const v = vars(); return text.replace(/\{(\w+)\}/g, (_, k) => k in v ? v[k] : "{" + k + "}"); }
  const has = (f, c) => c.every(x => x[0] === "!" ? !f.has(x.slice(1)) : f.has(x));
  function show(nid) {
    if (nid === null) return close();
    const node = s.conv.nodes[nid], f = flags.get(s.i);
    s.node = nid; s.opts = node.options.filter(o => has(f, o.if || []) && (!window.Life || Life.allowed(o)));   // and a quest reply's needs (life.js)
    const line = fill(node.say); el("talk-say").textContent = line; el("talk").dataset.mood = node.mood;
    s.mood = node.mood; s.line = speak(line); s.t0 = now(); s.k = 1;   // the resident says the line: mouth, face and nods follow it
    const said = window.Voices && Voices.speak(s.i, node.say, vars(), node.mood || "neutral");   // spoken aloud (voices.js, SND-018)
    if (said) s.k = s.line.end / Math.max(.3, said.dur);             // the mouth finishes with the voice (CHR-009)
    else if (window.Sound) Sound.line(s.line, s.i, s.r.x, s.r.z);    // a line with no voice yet: wordless syllables, as before
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
    if (window.Life) Life.chose(s.i, o);                 // quest acts, a job taken, a counter to open (life.js)
    if (window.Sound) Sound.act("talk_choose");
    const said = window.Voices && Voices.speak("player", o.text, vars(), "neutral");   // you say your answer (SND-018)
    if (!said) return show(o.next);
    const me = s; s.opts = []; el("talk-opts").textContent = "";
    el("talk-say").textContent = "You: " + fill(o.text); el("talk").dataset.mood = "neutral";
    s.line = speak(""); s.t0 = now();                    // the resident listens, mouth at rest
    setTimeout(() => { if (s === me) show(o.next); }, (said.dur + .35) * 1000);
  }
  function open(t) {
    const r0 = where(t.i, clock.m); if (!r0) return;
    const cid = conversationFor(t.i, r0), conv = window.Life ? Life.conversation(t.i, DL.conversations[cid]) : DL.conversations[cid];   // with quests and jobs (life.js)
    const r = Object.assign({}, r0); if (r.pose === 1) { r.pose = 0; r.walking = false; r.ph = 0; }   // a walker stops and stands
    s = { i: t.i, r, r0, conv, cid, m0: clock.m, node: null, opts: [], f: restFace(), dist: cam.dist };
    if (!cam.fp) cam.dist = Math.min(cam.dist, 2.6);       // come in closer so the face can be read
    if (!flags.has(t.i)) flags.set(t.i, new Set());
    for (const k in keys) keys[k] = false; pl.moving = false;
    pl.yaw = yawOf(r.x - pl.x, r.z - pl.z); cam.yaw = cam.fp ? pl.yaw : pl.yaw - .45; cam.pitch = cam.fp ? -.08 : -.12;   // face them; in third person look past the shoulder
    if (document.pointerLockElement && document.exitPointerLock) document.exitPointerLock();
    const res = D.residents[t.i];
    el("talk-name").textContent = res.n; el("talk-role").textContent = DL.role_words[D.roles[res.r]];
    el("talk").style.display = "block"; el("keys").dataset.was = el("keys").style.display; el("keys").style.display = "none";
    target = null; if (window.Sound) Sound.act("talk_open"); show(conv.start);
  }
  function close() {
    if (!s) return;
    const spent = (clock.m - s.m0 + 1440) % 1440, dl = (late.get(s.i) || 0) + spent;
    if (spent > 0 && dl < 180) late.set(s.i, dl); else late.delete(s.i);   // held more than three hours: they just pick up the day where it is
    el("talk").style.display = "none"; el("keys").style.display = el("keys").dataset.was || "block";
    if (!cam.fp) cam.dist = s.dist;
    if (window.Voices) Voices.stop();
    if (window.Sound) { Sound.hush(); Sound.act("talk_close"); }
    s = null;
    if (window.Life) Life.talkEnded();
  }
  function step(dt) {                                    // each frame while talking: the face follows the line; a standing resident turns to the player
    if (!s) return;
    blendFace(dt);
    if (s.r.pose !== 0) return;
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
    get on() { return !!s; }, face: i => s && s.i === i ? s.f : null, speak, where, consider, pick, open, close, step, key, choose,
    state: () => s && { mood: s.mood, line: s.line, face: s.f, resident: s.i, name: D.residents[s.i].n, conversation: s.cid, node: s.node, say: el("talk-say").textContent, options: s.opts.map(o => fill(o.text)) },
    late: () => [...late],
    debug: () => s,                                      // for tests: the talk under way, as it is
  };
})();
addEventListener("load", () => { if (window.wasted) window.wasted.talk = Talk; });   // play.js makes window.wasted after this file runs
