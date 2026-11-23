"use strict";
// Sound in the slice (SND-001 to SND-012: F-SND-WORLD, F-SND-ACTION and F-SND-MUSIC at level 1). Every sound is made here
// with the Web Audio API from the recipes in World/rules/sound.json (packed into city_data.js as D.sound), so there are no
// audio files and nothing to license. Nothing here decides what happens in the town: play.js tells it each frame where
// the listener is and what is near (residents, cars, trams), and calls in when the player does something.
//
// Loaded before play.js, like cars.js and conversations.js. Browsers start sound only after a click or a key, so nothing
// plays until begin() or the first key (Sound.start). N turns all sound off and on.
//
// The mix: world (ambience, engines, tram, steps, birds) through an "outside" bus that is muffled while the listener is
// indoors; voices (a resident's line, murmur between residents); actions (sit, lift, car door, interface); music (the
// score by time of day, music from cafés, shops and the market busker, the car radio with music and podcasts). The score fades under a
// conversation and under any nearer music.
var Sound = (() => {
  const D = window.WASTED, S = D.sound;
  if (!S || !(window.AudioContext || window.webkitAudioContext)) {
    const no = () => { };
    return { start: no, toggle: no, act: no, line: no, hush: no, frame: no, horn: no, radio: no, car: no, init: no, preview: () => false, state: () => ({ on: false, reason: S ? "no Web Audio" : "no sound data" }) };
  }
  const H = S.hearing, MU = S.music, V = S.vehicles, A = S.ambience, FS = S.footsteps, VO = S.voices;
  const hm = s => (+s.slice(0, 2)) * 60 + (+s.slice(3));
  const within = (m, a, b) => { const x = hm(a), y = hm(b); return x <= y ? m >= x && m < y : m >= x || m < y; };
  const cl = (v, a, b) => Math.max(a, Math.min(b, v));
  const sm = (a, b, v) => { const t = cl((v - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); };
  const rnd = (a, b) => a + Math.random() * (b - a);
  const mtof = n => 440 * Math.pow(2, (n - 69) / 12);
  const hashOf = i => { let h = (i + 1) * 2654435761 >>> 0; h ^= h >>> 15; h = Math.imul(h, 2246822519) >>> 0; h ^= h >>> 13; return (h >>> 0) / 4294967296; };

  let ctx = null, on = true, ready = false;
  try { on = localStorage.getItem("wasted.sound") !== "off"; } catch (e) { }
  const MIXK = { master: 1, music: 1, world: 1, voice: 1, action: 1 };   // the menu's shares of each level (mix)
  let master, meter, bus = {}, outside, outsideLP, noise, L = { x: 0, z: 0, yaw: 0, lot: null }, lots = [], fountains = [], gardens = [];
  const st = { engines: 0, trams: 0, murmur: 0, chat: 0, crowd: 0, acts: 0, laughs: 0, calls: 0, shouts: 0, greets: 0, place: null, radio: 0, mood: null, steps: 0, birds: 0, line: false, lastAct: null };

  // ---- the graph
  function build() {
    const AC = window.AudioContext || window.webkitAudioContext; ctx = new AC();
    master = ctx.createGain(); master.gain.value = on ? S.mix.master : 0; master.connect(ctx.destination);
    meter = ctx.createAnalyser(); meter.fftSize = 2048; master.connect(meter);          // for tests: how loud the mix is
    for (const k of ["world", "voice", "action"]) { bus[k] = ctx.createGain(); bus[k].gain.value = S.mix[k]; bus[k].connect(master); }
    bus.music = musicChain(master);
    outsideLP = ctx.createBiquadFilter(); outsideLP.type = "lowpass"; outsideLP.frequency.value = 18000;
    outside = ctx.createGain(); outside.connect(outsideLP); outsideLP.connect(bus.world);
    noise = makeNoise();
    ambience(); ready = true;
  }
  const now = () => ctx.currentTime;
  const ramp = (p, v, tc) => p.setTargetAtTime(v, now(), tc || .08);
  function noiseSrc() { const s = ctx.createBufferSource(); s.buffer = noise; s.loop = true; s.loopStart = Math.random(); s.start(); return s; }
  function filt(type, hz, q) { const f = ctx.createBiquadFilter(); f.type = type; f.frequency.value = hz; if (q) f.Q.value = q; return f; }
  function chain(...n) { for (let i = 0; i < n.length - 1; i++) n[i].connect(n[i + 1]); return n[n.length - 1]; }

  // how loud and where a sound at (x, z) is for the listener: [gain, pan]
  function place(x, z, far) {
    const dx = x - L.x, dz = z - L.z, d = Math.hypot(dx, dz), f = far || H.far;
    if (d > f) return [0, 0];
    const g = H.near / (H.near + H.rolloff * Math.max(0, d - H.near)) * (1 - sm(f * .7, f, d));
    const pan = d < .5 ? 0 : (dx * Math.cos(L.yaw) + dz * Math.sin(L.yaw)) / d * H.pan;
    return [g, cl(pan, -1, 1)];
  }
  function spot(dest) {                                 // gain -> pan -> dest, moved with .set(x, z, scale, far)
    const g = ctx.createGain(), p = ctx.createStereoPanner(); g.gain.value = 0; g.connect(p); p.connect(dest);
    return { g, p, input: g, set(x, z, k, far) { const [a, pn] = place(x, z, far); ramp(g.gain, a * k, .06); ramp(p.pan, pn, .06); return a; } };
  }
  function once(dest, x, z, far) {                       // a node for a one-shot sound: where it is, at the time it starts
    if (x === undefined) return dest;
    const [a, pn] = place(x, z, far); if (a < .004) return null;
    const g = ctx.createGain(), p = ctx.createStereoPanner(); g.gain.value = a; p.pan.value = pn; g.connect(p); p.connect(dest);
    return g;
  }
  function env(g, t, a, peak, len) { g.gain.setValueAtTime(0, t); g.gain.linearRampToValueAtTime(peak, t + a); g.gain.exponentialRampToValueAtTime(.0001, t + a + len); }

  // ---- recipes (SND-009): a tick, a thud, a rustle, a two-note chime
  function recipe(r, dest, t) {
    t = t || now(); const g = ctx.createGain(); g.connect(dest);
    if (r.kind === "tick" || r.kind === "chime") {
      const hz = [].concat(r.hz);
      hz.forEach((h, k) => {
        const o = ctx.createOscillator(), og = ctx.createGain(); o.type = r.kind === "tick" ? "square" : "sine"; o.frequency.value = h;
        const t0 = t + k * (r.len * .45); o.connect(og);
        if (r.kind === "tick") chain(og, filt("bandpass", h, 4), g); else og.connect(g);
        env(og, t0, .004, r.gain, r.kind === "tick" ? r.len : r.len * .9); o.start(t0); o.stop(t0 + r.len * 1.6);
      });
      g.gain.value = 1;
    } else if (KINDS2[r.kind]) { g.gain.value = 1; KINDS2[r.kind](r, g, t);
    } else {
      const s = ctx.createBufferSource(); s.buffer = noise; s.playbackRate.value = r.kind === "thud" ? .5 : 1;
      const f = r.kind === "thud" ? filt("lowpass", r.hz * 3, 1) : filt("bandpass", r.hz, .8);
      chain(s, f, g); env(g, t, r.kind === "thud" ? .003 : .03, r.gain, r.len); s.start(t, Math.random()); s.stop(t + r.len + .1);
      if (r.kind === "thud") {                         // a body to the knock
        const o = ctx.createOscillator(), og = ctx.createGain(); o.frequency.setValueAtTime(r.hz * 1.6, t); o.frequency.exponentialRampToValueAtTime(r.hz * .6, t + r.len);
        chain(o, og, dest); env(og, t, .003, r.gain * .8, r.len); o.start(t); o.stop(t + r.len + .1);
      }
    }
  }
  // the small sounds of what people do (SND-015): each draws itself into g, starting at t
  function burst(g, t, type, hz, q, peak, att, len, rate) {   // a shaped puff of noise through one filter
    const s = ctx.createBufferSource(), e = ctx.createGain(); s.buffer = noise; if (rate) s.playbackRate.value = rate;
    const f = filt(type, hz, q); chain(s, f, e, g); env(e, t, att, peak, len); s.start(t, Math.random()); s.stop(t + att + len + .1); return f;
  }
  function ping(g, t, hz, peak, len, type) {             // a struck tone
    const o = ctx.createOscillator(), e = ctx.createGain(); o.type = type || "sine"; o.frequency.value = hz; chain(o, e, g); env(e, t, .002, peak, len); o.start(t); o.stop(t + len + .1);
  }
  const KINDS2 = {
    clink(r, g, t) { for (let k = 0; k < (r.n || 1); k++) { const t0 = t + k * rnd(.05, .11), h = r.hz * rnd(.94, 1.08); ping(g, t0, h, r.gain, r.len); ping(g, t0, h * 1.51, r.gain * .5, r.len * .7); } },
    hiss(r, g, t) { burst(g, t, "bandpass", r.hz, .7, r.gain, .08, r.len); },
    snip(r, g, t) { burst(g, t, "highpass", r.hz, .7, r.gain, .001, r.len); burst(g, t + .07, "highpass", r.hz * 1.1, .7, r.gain * .8, .001, r.len); },
    type(r, g, t) { let t0 = t; for (let k = 0; k < (r.n || 6); k++) { burst(g, t0, "bandpass", r.hz * rnd(.85, 1.2), 3, r.gain, .001, r.len); t0 += rnd(.07, .16); } },
    snore(r, g, t) { burst(g, t, "lowpass", r.hz, .8, r.gain, r.len * .45, r.len * .5, .5); },
    creak(r, g, t) { const o = ctx.createOscillator(), e = ctx.createGain(); o.type = "triangle"; o.frequency.setValueAtTime(r.hz, t); o.frequency.linearRampToValueAtTime(r.hz * 1.25, t + r.len);
      chain(o, filt("bandpass", r.hz * 1.5, 2), e, g); e.gain.setValueAtTime(0, t); e.gain.linearRampToValueAtTime(r.gain, t + r.len * .3); e.gain.linearRampToValueAtTime(0, t + r.len); o.start(t); o.stop(t + r.len + .05); },
    whoosh(r, g, t) { const f = burst(g, t, "bandpass", r.hz, 1.2, r.gain, r.len * .4, r.len * .6); f.frequency.setValueAtTime(r.hz, t); f.frequency.exponentialRampToValueAtTime(r.hz * 3, t + r.len); },
    scrape(r, g, t) { burst(g, t, "bandpass", r.hz, .5, r.gain, .05, r.len); },
    clank(r, g, t) { ping(g, t, r.hz, r.gain, r.len, "triangle"); ping(g, t, r.hz * 2.76, r.gain * .5, r.len * .6); },
  };
  function act(name, x, z) {                             // an action sound, here or out in the world
    if (!ready || !S.actions[name]) return; st.lastAct = name;
    const d = once(bus.action, x, z, 30); if (d) recipe(S.actions[name], d);
  }

  // ---- instruments for the music (SND-010 to SND-012)
  // Plucked strings (harp, nylon guitar, upright bass) are Karplus-Strong tones worked out once per note and kept;
  // the electric piano and bells are two-operator FM; pads are soft detuned saws under a slowly opening filter.
  // Every part goes through one shared room (a generated reverb) and a gentle compressor on the music bus, so the
  // parts sit together in a space instead of buzzing side by side.
  const RM = MU.room, VOICE_OF = { pluck: "harp", keys: "epiano", guitar: "nylon" };   // the first version's names
  let hall = null, ksCache = new Map();
  function makeNoise() {
    const n = ctx.sampleRate * 2, b = ctx.createBuffer(1, n, ctx.sampleRate), d = b.getChannelData(0);
    for (let i = 0; i < n; i++) d[i] = Math.random() * 2 - 1;
    return b;
  }
  function musicChain(dest) {                            // the music bus: level, then a soft compressor
    const g = gainOf(S.mix.music), c = ctx.createDynamicsCompressor();
    c.threshold.value = -20; c.knee.value = 14; c.ratio.value = 3; c.attack.value = .02; c.release.value = .3;
    chain(g, c, dest); return g;
  }
  function makeRoom() {                                  // a stereo room: decaying noise that darkens as it fades
    const sr = ctx.sampleRate, n = Math.floor(sr * RM.seconds), ir = ctx.createBuffer(2, n, sr), pre = Math.floor(sr * RM.predelay);
    for (let c = 0; c < 2; c++) {
      const d = ir.getChannelData(c); let lp = 0;
      for (let i = pre; i < n; i++) { const x = (i - pre) / (n - pre); lp += (Math.random() * 2 - 1 - lp) * (.15 + .75 * (1 - x)); d[i] = lp * Math.pow(1 - x, 3); }
    }
    const cv = ctx.createConvolver(); cv.buffer = ir; chain(cv, gainOf(RM.wet), bus.music);
    return cv;
  }
  function ks(midi, kind) {                              // a plucked string: [buffer, playback rate], made once per note
    const key = kind + midi; let v = ksCache.get(key); if (v) return v;
    const sr = ctx.sampleRate, f = mtof(midi), N = Math.max(2, Math.round(sr / f - .5));
    const dur = kind === "upright" ? 1.4 : kind === "nylon" ? 2.2 : 3, n = Math.floor(sr * dur), buf = ctx.createBuffer(1, n, sr), d = buf.getChannelData(0);
    const bright = kind === "harp" ? .6 : kind === "nylon" ? .32 : .12, decay = kind === "upright" ? .993 : kind === "harp" ? .9988 : .9975;
    let p = 0, peak = 1e-6;
    for (let i = 0; i < N; i++) { p += (Math.random() * 2 - 1 - p) * bright; d[i] = p; }
    for (let i = 0; i < N; i++) peak = Math.max(peak, Math.abs(d[i]));
    for (let i = 0; i < N; i++) d[i] /= peak;
    for (let i = N; i < n; i++) d[i] = decay * .5 * (d[i - N] + (i - N - 1 >= 0 ? d[i - N - 1] : 0));
    v = [buf, f * (N + .5) / sr]; ksCache.set(key, v); return v;
  }
  const tilt = midi => cl(1.15 - (midi - 55) / 60, .45, 1.2);   // high notes a little softer, as an ear expects
  function note(dest, t, midi, len, voice, vel, pan) {
    voice = VOICE_OF[voice] || voice;
    const f = mtof(midi), g = ctx.createGain(); g.gain.value = 0;
    if (pan) { const p = ctx.createStereoPanner(); p.pan.value = pan; g.connect(p); p.connect(dest); } else g.connect(dest);
    const a = vel * tilt(midi);
    if (voice === "harp" || voice === "nylon" || voice === "upright") {
      const [buf, rate] = ks(midi, voice), s = ctx.createBufferSource(); s.buffer = buf; s.playbackRate.value = rate;
      chain(s, filt("lowpass", voice === "harp" ? 5200 : voice === "nylon" ? 2600 : 1100, .5), g);
      const ring = voice === "harp" ? len + 2.2 : voice === "nylon" ? len + .5 : len;
      g.gain.setValueAtTime(a * (voice === "upright" ? 1.5 : .8), t); g.gain.setTargetAtTime(0, t + ring, .09);
      s.start(t); s.stop(t + ring + .6);
    } else if (voice === "epiano" || voice === "bell") {
      const bell = voice === "bell", c = ctx.createOscillator(), m = ctx.createOscillator(), mg = ctx.createGain();
      c.frequency.value = f; m.frequency.value = f * (bell ? 3.5 : 1); chain(m, mg, c.frequency);
      mg.gain.setValueAtTime(f * (bell ? 2.4 : 1.4), t); mg.gain.setTargetAtTime(f * (bell ? .2 : .25), t, bell ? .6 : .3);
      const c2 = ctx.createOscillator(); c2.type = "sine"; c2.frequency.value = f; c2.detune.value = bell ? 0 : 6;   // a second tine, just off, for warmth
      const g2 = gainOf(bell ? 0 : .35); chain(c2, g2, g);
      chain(c, filt("lowpass", bell ? 7000 : 3400, .3), g);
      const peak = a * (bell ? .2 : .3), dec = bell ? 3.2 : 1.8, end = bell ? t + dec * 1.6 : t + len + .25;
      g.gain.setValueAtTime(0, t); g.gain.linearRampToValueAtTime(peak, t + .005); g.gain.setTargetAtTime(0, t + .005, dec / 2.5);
      if (!bell) g.gain.setTargetAtTime(0, t + len, .12);
      for (const o of [c, m, c2]) { o.start(t); o.stop(end + .6); }
    } else if (voice === "pad") {
      const lp = ctx.createBiquadFilter(); lp.type = "lowpass"; lp.Q.value = .4;
      lp.frequency.setValueAtTime(380, t); lp.frequency.linearRampToValueAtTime(1300, t + len * .5); lp.frequency.linearRampToValueAtTime(600, t + len + 1.8);
      chain(lp, g);
      for (const det of [-11, 0, 11]) { const o = ctx.createOscillator(); o.type = det ? "sawtooth" : "triangle"; o.frequency.value = f; o.detune.value = det; o.connect(lp); o.start(t); o.stop(t + len + 3); }
      g.gain.setValueAtTime(0, t); g.gain.linearRampToValueAtTime(a * .08, t + Math.min(1.4, len * .4)); g.gain.setTargetAtTime(0, t + len, .7);
    } else {                                             // "bass": a round synth bass
      const o = ctx.createOscillator(), o2 = ctx.createOscillator(); o2.type = "triangle"; o.frequency.value = f; o2.frequency.value = f;
      const lp = filt("lowpass", 420, .6); o.connect(lp); chain(o2, gainOf(.3), lp); chain(lp, g);
      g.gain.setValueAtTime(0, t); g.gain.linearRampToValueAtTime(a * .55, t + .012); g.gain.setTargetAtTime(a * .3, t + .012, .3); g.gain.setTargetAtTime(0, t + len, .05);
      o.start(t); o2.start(t); o.stop(t + len + .4); o2.stop(t + len + .4);
    }
  }
  function gainOf(v) { const g = ctx.createGain(); g.gain.value = v; return g; }
  function drum(dest, t, kind, vel) {
    const g = ctx.createGain(); g.gain.value = 0; g.connect(dest);
    const hit = (type, hz, q, peak, att, dec) => {
      const s = ctx.createBufferSource(); s.buffer = noise; chain(s, filt(type, hz, q), g);
      g.gain.setValueAtTime(0, t); g.gain.linearRampToValueAtTime(peak * vel, t + att); g.gain.setTargetAtTime(0, t + att, dec / 3);
      s.start(t, Math.random()); s.stop(t + att + dec + .1);
    };
    if (kind === "kick") {
      const o = ctx.createOscillator(); o.frequency.setValueAtTime(110, t); o.frequency.exponentialRampToValueAtTime(46, t + .12);
      o.connect(g); g.gain.setValueAtTime(.5 * vel, t); g.gain.setTargetAtTime(0, t + .01, .08); o.start(t); o.stop(t + .4);
    } else if (kind === "snare") {
      hit("bandpass", 1900, .8, .2, .002, .16);
      const o = ctx.createOscillator(), og = gainOf(0); o.type = "triangle"; o.frequency.value = 190; chain(o, og, dest);
      og.gain.setValueAtTime(.12 * vel, t); og.gain.setTargetAtTime(0, t, .03); o.start(t); o.stop(t + .2);
    } else if (kind === "brush") hit("bandpass", 3600, .5, .07, .035, .2);
    else if (kind === "ride") hit("highpass", 6500, .7, .025, .002, .35);
    else if (kind === "shaker") hit("highpass", 6000, .6, .035, .008, .06);
    else hit("highpass", 8000, .7, .045, .001, .04);       // closed hat
  }

  // A small endless piece in a style (World/rules/sound.json, music.styles): the chords of the style, voiced close to
  // the last chord so they move smoothly; a bass line; a comping pattern; a melody built from a short motif that
  // repeats with variations across eight bars and leaves room to breathe; and soft drums. Changes of style wait for
  // the start of a bar. Notes are scheduled a little ahead of the audio clock; the band rests while it is silent.
  function seeded(a) { return () => { a |= 0; a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; }; }
  const styleOf = s => typeof s === "string" ? MU.styles[s] : s;
  function band(style, dest, seed) {
    const out = gainOf(0), send = gainOf(1); out.connect(dest); if (!hall) hall = makeRoom(); chain(out, send, hall);
    const rng = seeded(seed || Math.floor(Math.random() * 1e9));
    let sty = style, next = null, t = now() + .1, step = 0, bar = 0, voicing = null, chordAt = -1, sec = null, base = 0;
    const sc = s => MU.scales[s.scale];
    const pitch = (s, i) => s.root + 12 * Math.floor(i / 7) + sc(s)[((i % 7) + 7) % 7];
    const isChordTone = (i, deg, s) => { const r = (((i - deg) % 7) + 7) % 7; return r === 0 || r === 2 || r === 4 || s.sevenths && r === 6; };
    const snap = (i, deg, s) => { for (const d of [0, 1, -1, 2, -2, 3]) if (isChordTone(i + d, deg, s)) return i + d; return i; };
    function voice(deg, s) {                             // the chord's tones in the inversion nearest the last chord
      const n = s.sevenths ? 4 : 3, tones = Array.from({ length: n }, (_, k) => deg + 2 * k);
      let best = null, bs = 1e9;
      for (let inv = 0; inv < n; inv++) for (const oct of [-7, 0]) {
        const v = tones.map((x, k) => (k < inv ? x + 7 : x) + oct).sort((p, q) => p - q).map(i => pitch(s, i));
        const mid = v.reduce((p, q) => p + q, 0) / n;
        const sc2 = (voicing ? v.reduce((p, x, k) => p + Math.abs(x - (voicing[k] !== undefined ? voicing[k] : x)), 0) : 0) + Math.abs(mid - (s.root + 3)) * .6;
        if (sc2 < bs) { bs = sc2; best = v; }
      }
      return best;
    }
    function motif(s) {                                  // a rhythm from the phrase book and a contour of small steps
      const book = MU.phrases, slow = s.bpm < 80, pool = book.filter(r => !slow || r.length <= 5);
      const rhythm = pool[Math.floor(rng() * pool.length)], contour = [0];
      for (let k = 1; k < rhythm.length; k++) contour.push(cl(contour[k - 1] + [-2, -1, -1, 1, 1, 2, 0, 3, -3][Math.floor(rng() * (rng() < .85 ? 7 : 9))], -4, 6));
      return { rhythm, contour };
    }
    function section(s) {                                // eight bars: which motif each two-bar phrase plays, or a rest
      const A = sec && rng() < .5 ? sec.A : motif(s), B = motif(s), d = s.density;
      const plan = d < .35 ? ["A", null, "A2", null] : d < .6 ? ["A", "A2", null, "A2"] : ["A", "A2", "B", "A2"];
      return { A, B, plan };
    }
    const b = {
      out, level: 0,
      set style(s) { if (s !== sty) { if (b.level < .002) sty = s; else next = s; } }, get style() { return next || sty; },
      run(until) {
        if (b.level < .002 && until === undefined) { t = now() + .1; step = 0; return; }
        const end = until !== undefined ? until : now() + .3;
        while (t < end) {
          if (step === 0 && next && bar % 2 === 0) { sty = next; next = null; sec = null; bar = 0; chordAt = -1; }
          const s = styleOf(sty), q = 60 / s.bpm / 4, bpc = s.bars_per_chord || 1;
          const ci = Math.floor(bar / bpc) % s.chords.length, deg = s.chords[ci] - 1;
          if (step === 0 && bar % 8 === 0 || !sec) sec = section(s);
          if (!voicing || step === 0 && ci !== chordAt) { voicing = voice(deg, s); chordAt = ci; }
          const swing = step % 4 === 2 ? (s.swing || 0) * 2 * q : 0;
          const at = (dt) => t + swing + (dt || 0) + (rng() - .5) * .01, vel = k => k * (.9 + .2 * rng());
          const lvl = (x, d) => x !== undefined ? x : d, cv = k => vel(k * lvl(s.comp_level, 1)), dv = k => vel(k * lvl(s.drum_level, 1));
          const comp = s.comp || "epiano", pat = s.comp_pattern || "block";
          // comping (comp_level scales it), and a held layer under it if the style asks (under)
          if (pat === "pad" && step === 0 && bar % bpc === 0) for (const p of voicing) note(out, at(), p, q * 16 * bpc, "pad", cv(.8), -.2);
          else if (pat === "block" && (step === 0 || s.bpm > 100 && step === 10)) voicing.forEach((p, k) => note(out, at(k * .012), p, q * (step ? 5 : 12), comp, cv(step ? .3 : .42), -.25));
          else if (pat === "arp" && step % 2 === 0) { const ord = [0, 1, 2, 1, 3, 1, 2, 1], k = ord[(step / 2) % 8] % (voicing.length + 1), p = k === voicing.length ? voicing[0] + 12 : voicing[k]; note(out, at(), p, q * 3, comp, cv(step % 8 ? .3 : .4), -.25); }
          else if (pat === "strum" && [0, 6, 8, 12, 14].indexOf(step) >= 0) { const up = step === 6 || step === 14, vs = up ? voicing.slice().reverse() : voicing; vs.forEach((p, k) => note(out, at(k * .016), p, q * (up ? 2 : 4), comp, cv(up ? .25 : .38), -.2)); }
          else if (pat === "offbeat" && step % 8 === 4) voicing.forEach((p, k) => note(out, at(k * .008), p, q * 2, comp, cv(.32), -.25));
          else if (pat === "gentle_strum" && (step === 0 || step === 8 || step === 12)) { const up = step === 12, vs = up ? voicing.slice().reverse() : voicing; vs.forEach((p, k) => note(out, at(k * .024), p, q * (up ? 3 : 7), comp, cv(up ? .17 : .3), -.2)); }
          else if (pat === "four" && step % 4 === 0) voicing.forEach((p, k) => note(out, at(k * .01), p, q * 1.8, comp, cv(step % 8 === 4 ? .3 : .22), -.2));   // a short chord on every beat, 2 and 4 leaned on
          if (s.under && step === 0 && bar % bpc === 0) for (const p of voicing) note(out, at(), p, q * 16 * bpc, s.under, vel(.8 * lvl(s.under_level, 1)), .1);   // a held layer under the comping
          // bass
          const root = pitch(s, deg) - 24, fifth = pitch(s, deg + 4) - 24, bs = s.bass === true ? "root" : s.bass;
          if (bs === "root" && (step === 0 || step === 8)) note(out, at(), step === 0 || rng() < .5 ? root : fifth, q * 7, "bass", vel(.8));
          else if (bs === "pulse" && step % 2 === 0) note(out, at(), step % 8 === 6 && rng() < .4 ? fifth : root, q * 1.6, "bass", vel(step % 8 ? .55 : .75));
          else if (bs === "walk" && step % 4 === 0) {
            const nextDeg = s.chords[(Math.floor((bar + 1) / bpc)) % s.chords.length] - 1, beat = step / 4;
            const p = beat === 0 ? root : beat === 3 && (bar + 1) % bpc === 0 ? pitch(s, nextDeg) - 24 + (rng() < .5 ? 1 : -1) : pitch(s, deg + [0, 2, 4, 5][beat] - (rng() < .3 ? 1 : 0)) - 24;
            note(out, at(), p, q * 3.6, s.bass_voice || "upright", vel(beat === 0 ? .85 : .7));
          }
          // melody: the phrase for these two bars, its notes on their steps
          const ph = sec.plan[Math.floor((bar % 8) / 2)], ls = (bar % 2) * 16 + step;
          if (ph) {
            const m = ph === "B" ? sec.B : sec.A;
            if (ls === 0) base = snap(7 + Math.round(rng() * 2), deg, s);
            m.rhythm.forEach(([st, ln], j) => {
              if (st !== ls) return;
              let i = base + m.contour[j] + (ph === "A2" && j >= m.rhythm.length - 2 ? (j === m.rhythm.length - 1 ? -1 : 1) : 0) - (ph === "B" ? m.contour[j] * 2 : 0);
              i = cl(i, 4, 14); if (st % 8 === 0 || j === m.rhythm.length - 1) i = snap(i, deg, s);
              note(out, at(), pitch(s, i), q * ln * .95, s.voice, vel(j === 0 ? .55 : .47), .2);
            });
          }
          // drums
          const dr = s.drums;
          if (dr === "brush") { if (step % 8 === 4) drum(out, at(), "brush", dv(1)); if (step % 4 === 0) drum(out, at(), "ride", dv(step % 8 ? .7 : 1)); if (step % 4 === 2 && swing) drum(out, at(), "ride", dv(.5)); }
          else if (dr === "kit") { if (step === 0 || step === 10 && rng() < .5 || step === 8) drum(out, at(), "kick", dv(1)); if (step % 8 === 4) drum(out, at(), "snare", dv(1)); if (step % 2 === 0) drum(out, at(), "hat", dv(step % 4 ? .6 : 1)); }
          else if (dr === "shaker") drum(out, at(), "shaker", dv(step % 4 === 0 ? 1 : step % 2 ? .45 : .7));
          t += q; step = (step + 1) % 16; if (step === 0) bar++;
        }
      },
      fade(v, tc) { const s = styleOf(next || sty); b.level = v; ramp(out.gain, v * (s.gain !== undefined ? s.gain : 1), tc || .6); ramp(send.gain, s.reverb !== undefined ? s.reverb : 1, tc || .6); },
    };
    return b;
  }
  async function render(style, secs) {                   // for tests: play a style into an offline context, measure it
    const save = { ctx, bus, hall, noise, ksCache };
    const oc = new OfflineAudioContext(2, Math.floor(44100 * secs), 44100);
    try {                                                // everything is scheduled at once, so the live sound never sees this context
      ctx = oc; hall = null; ksCache = new Map(); noise = makeNoise(); bus = { music: musicChain(oc.destination) };
      const b = band(style, bus.music, 7); b.level = 1; b.out.gain.value = styleOf(style).gain !== undefined ? styleOf(style).gain : 1; b.run(secs - .5);
    } finally { ({ ctx, bus, hall, noise, ksCache } = save); }
    {
      const buf = await oc.startRendering(), L = buf.getChannelData(0), R = buf.getChannelData(1), sr = buf.sampleRate;
      let peak = 0, sum = 0; const perSec = [];
      for (let s0 = 0; s0 + sr <= L.length; s0 += sr) { let q = 0; for (let i = s0; i < s0 + sr; i++) { const v = (L[i] + R[i]) / 2; q += v * v; peak = Math.max(peak, Math.abs(L[i]), Math.abs(R[i])); } sum += q; perSec.push(+Math.sqrt(q / sr).toFixed(4)); }
      return { style: typeof style === "string" ? style : style.name, peak: +peak.toFixed(3), rms: +Math.sqrt(sum / L.length).toFixed(4), quietSeconds: perSec.filter(v => v < .002).length, perSec };
    }
  }

  // ---- the ambience (SND-001): street air, room tone, crickets, the nearest fountain
  let street, streetLP, room, crick, fount;
  function ambience() {
    streetLP = filt("lowpass", A.street.day_hz, .5); street = gainOf(0); chain(noiseSrc(), filt("highpass", 60), streetLP, street, outside);
    room = gainOf(0); chain(noiseSrc(), filt("lowpass", A.room.hz), room, bus.world);
    const co = ctx.createOscillator(); co.frequency.value = A.crickets.hz; const am = gainOf(0), lfo = ctx.createOscillator(); lfo.type = "square"; lfo.frequency.value = 28;
    am.gain.value = .5; chain(lfo, gainOf(.5), am.gain);             // chirps: the tone gated 28 times a second...
    const mod = gainOf(.5), slow = ctx.createOscillator(); slow.frequency.value = .7; chain(slow, gainOf(.5), mod.gain);   // ...in slow swells
    crick = gainOf(0); chain(co, am, mod, crick, outside); co.start(); lfo.start(); slow.start();
    fount = spot(outside); chain(noiseSrc(), filt("bandpass", A.fountain.hz, .4), fount.input);
  }
  function chirp(x, z) {
    const d = once(outside, x, z, 45); if (!d) return; st.birds++;
    let t = now(); const n = 2 + Math.floor(Math.random() * 5), base = rnd(A.birds.hz[0], A.birds.hz[1]);
    for (let k = 0; k < n; k++) {
      const o = ctx.createOscillator(), g = ctx.createGain(), len = rnd(.04, .11);
      o.frequency.setValueAtTime(base * rnd(.85, 1.1), t); o.frequency.exponentialRampToValueAtTime(base * rnd(.7, 1.35), t + len);
      chain(o, g, d); env(g, t, .008, A.birds.gain, len); o.start(t); o.stop(t + len + .05); t += len + rnd(.02, .09);
    }
  }

  // ---- footsteps (SND-003)
  function step(x, z, surface, gain, far, pitch) {
    const d = once(bus.world, x, z, far); if (!d) return; st.steps++;
    const s = FS.surfaces[surface] || FS.surfaces.stone, src = ctx.createBufferSource(), g = ctx.createGain(); src.buffer = noise;
    chain(src, filt("bandpass", s.hz * (pitch || 1) * rnd(.85, 1.15), s.q), g, d); env(g, now(), .002, gain, s.len); src.start(now(), Math.random()); src.stop(now() + s.len + .05);
  }

  // ---- engines (SND-002): a pool of voices given to the nearest moving cars; the player's car has its own
  function engineVoice(dest) {
    const o1 = ctx.createOscillator(), o2 = ctx.createOscillator(); o1.type = "sawtooth"; o2.type = "square";
    const lp = filt("lowpass", V.engine.tone_hz, 1.5), g = gainOf(.6), sp = spot(dest), ty = gainOf(0);
    o1.connect(lp); o2.connect(chain(gainOf(.5), lp)); chain(lp, g, sp.input); o1.start(); o2.start();
    chain(noiseSrc(), filt("bandpass", 700, .6), ty, sp.input);
    return { sp, set(hz, tyres) { ramp(o1.frequency, hz, .1); ramp(o2.frequency, hz / 2, .1); ramp(ty.gain, tyres, .1); } };
  }
  let pool = null, mine = null, tramV = [], horn = null, skid = null;
  function engines(cars, drive) {
    if (!pool) pool = Array.from({ length: V.engine.voices }, () => engineVoice(outside));
    const E = V.engine, list = cars.map(c => [Math.hypot(c.x - L.x, c.z - L.z), c]).filter(a => a[0] < E.far).sort((a, b) => a[0] - b[0]);
    st.engines = Math.min(list.length, pool.length);
    pool.forEach((v, k) => {
      const c = list[k] && list[k][1]; if (!c) { ramp(v.sp.g.gain, 0, .1); return; }
      const hz = Math.min(E.max_hz, E.idle_hz + Math.abs(c.v) * E.hz_per_mps) * (c.van ? E.van_pitch : 1);
      v.set(hz, Math.min(1, Math.abs(c.v) / 10) * E.tyres_gain * 8); v.sp.set(c.x, c.z, E.gain * (.55 + .45 * Math.min(1, Math.abs(c.v) / 8)), E.far);
    });
    // the player's own car: louder, not muffled, with a horn, skids and bumps
    if (!mine) { mine = engineVoice(bus.world); horn = gainOf(0); for (const hz of V.player.horn_hz) { const o = ctx.createOscillator(); o.type = "square"; o.frequency.value = hz; o.connect(horn); o.start(); } chain(horn, filt("lowpass", 1800), bus.action);
      skid = gainOf(0); chain(noiseSrc(), filt("bandpass", 1300, 2), skid, bus.world); }
    if (drive) {
      const P = V.player, v = Math.abs(drive.v), started = now() - drive.since > P.start_len;
      mine.set(started ? Math.min(E.max_hz * 1.2, E.idle_hz * 1.1 + v * E.hz_per_mps * 1.1 + (drive.gas ? 8 : 0)) : E.idle_hz * (.4 + .6 * (now() - drive.since) / P.start_len), v / 14 * E.tyres_gain * 6);
      ramp(mine.sp.g.gain, started ? P.gain * (.7 + .3 * Math.min(1, v / 8)) : P.gain * .6, .08); ramp(mine.sp.p.pan, 0);
      ramp(skid.gain, drive.brake && v > P.skid_mps ? P.skid_gain : 0, .05);
      if (drive.prev !== undefined && Math.abs(drive.prev) > 2 && Math.sign(drive.prev) !== Math.sign(drive.v) && Math.abs(drive.v) < Math.abs(drive.prev) * .5)
        recipe({ kind: "thud", hz: 80, len: .3, gain: P.bump_gain }, bus.action);
    } else { ramp(mine.sp.g.gain, 0, .15); ramp(skid.gain, 0, .05); }
  }

  // ---- the tram (SND-004): hum and whine with speed, two bell notes when it stops and when it leaves
  const tramSeen = new Map();
  function trams(list, tz) {
    while (tramV.length < 2) {
      const o = ctx.createOscillator(), w = ctx.createOscillator(); o.type = "sawtooth"; w.type = "sine";
      const sp = spot(outside), wg = gainOf(0), lp = filt("lowpass", 260); chain(o, lp, sp.input); chain(w, wg, sp.input); o.start(); w.start();
      const rg = gainOf(0); chain(noiseSrc(), filt("lowpass", 180), rg, sp.input); o.frequency.value = V.tram.hum_hz;
      tramV.push({ sp, w, wg, rg });
    }
    const near = list.map(t => [Math.abs(t[0] - L.x) + Math.abs(tz - L.z), t]).sort((a, b) => a[0] - b[0]);
    st.trams = 0;
    tramV.forEach((v, k) => {
      const tr = near[k] && near[k][1]; if (!tr) { ramp(v.sp.g.gain, 0, .2); return; }
      const [x, stopped, T] = tr, prev = tramSeen.get(T), moving = !stopped;
      const sp = prev ? Math.min(1, Math.abs(x - prev.x) / Math.max(.001, now() - prev.t) / 8) : 0;
      if (v.sp.set(x, tz, V.tram.gain * (moving ? .6 + .4 * sp : .35), V.tram.far) > .01) st.trams++;
      ramp(v.w.frequency, V.tram.whine_hz * (.5 + sp), .2); ramp(v.wg.gain, moving ? .12 * sp : 0, .3); ramp(v.rg.gain, moving ? .9 * sp + .1 : 0, .3);
    });
    for (const [x, stopped, T] of list) {
      const prev = tramSeen.get(T);
      if (prev && prev.stopped !== stopped) { const d = once(bus.world, x, tz, V.tram.bell_far); if (d) recipe({ kind: "chime", hz: V.tram.bell_hz, len: .5, gain: V.tram.bell_gain }, d); }
      if (!prev || prev.stopped !== stopped || now() - prev.t > .25) tramSeen.set(T, { x, stopped, t: now() });
    }
    if (tramSeen.size > 30) for (const k of [...tramSeen.keys()].slice(0, 10)) tramSeen.delete(k);
  }

  // ---- voices (SND-005): a resident's line voiced syllable by syllable in time with their mouth (CHR-009). Never words
  // (Q-14: text first). The talk between residents is under people, below.
  const pitchOf = i => VO.pitch.min + hashOf(i) * (VO.pitch.max - VO.pitch.min);
  function syllable(dest, t, hz, shape, len, gain) {
    const fm = VO.shapes[shape]; if (!fm) return;
    const o = ctx.createOscillator(); o.type = "sawtooth"; o.frequency.setValueAtTime(hz, t); o.frequency.linearRampToValueAtTime(hz * rnd(.92, 1.06), t + len);
    const g = ctx.createGain(); g.connect(dest);
    for (const [k, hzF] of fm.entries()) { const f = filt("bandpass", hzF, 6), fg = gainOf(k ? 2 : 3); chain(o, f, fg, g); }
    env(g, t, .015, gain, Math.max(.05, len)); o.start(t); o.stop(t + len + .12);
  }
  let lineOut = null;
  function voice(dest, ln, base, gain, from) {           // every syllable of a line (from Talk.speak) from `from` seconds in
    const t0 = now() + .02 - (from || 0), ask = ln.asks || [], words = ln.words || [];
    for (const s of ln.seq) {
      if (s.t0 < (from || 0) || !VO.shapes[s.shape] || s.shape === "MBP" && s.d < .05) continue;
      const w = words.filter(w => w.t0 <= s.t0).pop(), up = ask.some(a => a > s.t0 && a - s.t0 < .35) ? 1.18 : 1;
      syllable(dest, t0 + s.t0, base * up * (w && w.long ? 1.08 : 1), s.shape, Math.min(.22, s.d), gain);
    }
  }
  function line(ln, i, x, z) {
    if (!ready) return; hush();
    lineOut = gainOf(1); const d = once(bus.voice, x, z, 30) || bus.voice; lineOut.connect(d); st.line = true;
    voice(lineOut, ln, pitchOf(i), VO.line_gain);
    setTimeout(() => { st.line = false; }, (ln.end + .2) * 1000);
  }
  function hush() { if (lineOut) { lineOut.gain.setTargetAtTime(0, now(), .03); const o = lineOut; setTimeout(() => o.disconnect(), 300); lineOut = null; } st.line = false; }
  // ---- people (SND-003, SND-006, SND-015 to SND-017): the steps, sitting and standing, and doings of the residents near
  // the listener; conversations that take turns, with laughter; the hum of a crowd; sellers calling; shouts at a driver.
  // All from World/rules/sound.json "people". Nothing is stored about anyone beyond what is near (per-resident timers).
  const PE = S.people, SND = PE.sounds, BY_ACT = new Map(), SHAPES = Object.keys(VO.shapes).filter(x => x !== "MBP");
  for (const a of PE.activities) for (const act of a.acts) { if (!BY_ACT.has(act)) BY_ACT.set(act, []); BY_ACT.get(act).push(a); }
  const voiceHz = r => { const pr = PE.pitch[r.role]; return pr ? pr[0] + hashOf(r.i) * (pr[1] - pr[0]) : pitchOf(r.i); };
  const pst = new Map(), groups = new Map();
  let crowdBed = null, crowdT = 0, pruneT = 0;
  function sfx(name, x, z, far) { const r = SND[name], d = r && once(bus.world, x, z, far || PE.far); if (d) { recipe(r, d); st.acts++; } }
  function phrase(dest, t, base, n, gain, ask) {         // n wordless syllables that fall to an end (or rise to a question)
    const C = PE.chat;
    for (let k = 0; k < n; k++) {
      const len = 1 / C.syllables_per_s * rnd(.6, 1.25), f = k / Math.max(1, n - 1), c = ask ? 1 + .22 * f * f : 1.06 - .12 * f;
      syllable(dest, t, base * c * rnd(.96, 1.04), SHAPES[Math.floor(Math.random() * SHAPES.length)], len * .8, gain); t += len;
    }
    return t;
  }
  function laugh(dest, t, base, gain) {                  // ha-ha-ha, falling and fading
    const n = 3 + Math.floor(Math.random() * 3);
    for (let k = 0; k < n; k++) { syllable(dest, t, base * (1.3 - k * .06), k % 2 ? "EH" : "AA", .09, gain * (1 - k * .13)); t += rnd(.13, .17); }
    st.laughs++; return t;
  }
  function people(res, F) {
    const C = PE.chat, tnow = now(); let nSteps = 0, busy = 0;
    const order = res.map(r => [Math.hypot(r.x - L.x, r.z - L.z), r]).sort((a, b) => a[0] - b[0]).map(a => a[1]);   // nearest first
    for (const r of order) {
      const d = Math.hypot(r.x - L.x, r.z - L.z), child = r.role === "child";
      let p = pst.get(r.i);
      if (!p) { p = { half: null, pose: r.pose, act: r.act, next: new Map(), shout: null, greet: -1e9 }; pst.set(r.i, p); }
      p.seen = tnow;
      if (r.walking && d < FS.far) {                     // steps: louder running, lighter and higher for children (SND-017)
        const half = Math.floor((r.ph || 0) * 2);
        if (p.half !== null && half !== p.half && nSteps++ < FS.max)
          step(r.x, r.z, r.inside ? "wood" : r.kind === "garden" ? "grass" : "stone", FS.resident_gain * (r.run ? PE.steps.run_louder : 1) * (child ? PE.steps.child_softer : 1), FS.far, child ? PE.steps.child_pitch : 1);
        p.half = half;
      } else p.half = null;
      if (p.pose !== r.pose && d < 10) {                 // sitting down, lying down, getting up
        const name = r.pose === 2 ? PE.poses.sit : r.pose === 3 ? PE.poses.lie : p.pose >= 2 ? PE.poses.stand : null;
        if (name) sfx(name, r.x, r.z, 10);
      }
      p.pose = r.pose;
      if (p.act !== r.act) { p.act = r.act; p.next.clear(); }
      const doing = !r.walking && d < PE.far && BY_ACT.get(r.act); if (doing) busy++;
      if (doing && busy <= PE.busy) for (const a of doing) {   // what they are doing, now and then (the nearest few only)
        if (a.roles && a.roles.indexOf(r.role) < 0 || a.chance !== undefined && hashOf(r.i * 7 + 3) > a.chance) continue;
        const due = p.next.get(a);
        if (due === undefined) { p.next.set(a, tnow + rnd(0, a.every[1])); continue; }
        if (tnow < due) continue;
        p.next.set(a, tnow + rnd(a.every[0], a.every[1]));
        if (a.sound) sfx(a.sound, r.x, r.z);
        if (a.call) { const dd = once(bus.voice, r.x, r.z, 30); if (dd) { phrase(dd, tnow + .05, voiceHz(r) * 1.15, 2 + Math.floor(Math.random() * 2), PE.call_gain, false); st.calls++; } }
      }
      if (r.shout && r.shout !== p.shout && window.Talk && Talk.speak) {   // told off: the shout, voiced (CITY-071)
        const dd = once(bus.voice, r.x, r.z, 40); if (dd) { voice(dd, Talk.speak(r.shout), voiceHz(r) * 1.2, PE.shout_gain); st.shouts++; }
      }
      p.shout = r.shout || null;
      const G = PE.greet;                                // a passer-by says hello as the player goes by
      if (F.walk && F.walk.moving && !F.talk && r.pose < 2 && d < G.m && tnow - p.greet > G.every_s) {
        p.greet = tnow;
        if (Math.random() < G.chance) { const dd = once(bus.voice, r.x, r.z, 10); if (dd) { phrase(dd, tnow + .1, voiceHz(r) * 1.1, 2, G.gain, false); st.greets++; } }
      }
    }
    // conversations: people standing or sitting together take turns to talk, and now and then laugh (SND-006)
    const can = res.filter(r => !r.walking && r.pose !== 3 && C.acts.indexOf(r.act) >= 0 && Math.hypot(r.x - L.x, r.z - L.z) < C.far);
    const used = new Set(), found = [];
    for (const r of can) {
      if (used.has(r.i)) continue;
      const g = [r]; used.add(r.i);
      for (let k = 0; k < g.length; k++) for (const o of can) if (!used.has(o.i) && Math.hypot(o.x - g[k].x, o.z - g[k].z) < C.pair_m) { g.push(o); used.add(o.i); }
      if (g.length > 1) found.push(g);
    }
    found.sort((a, b) => Math.hypot(a[0].x - L.x, a[0].z - L.z) - Math.hypot(b[0].x - L.x, b[0].z - L.z));
    const live = new Set(); st.chat = 0; st.murmur = 0;
    for (const g of found.slice(0, C.groups)) {
      const key = g.map(r => r.i).sort((a, b) => a - b).join(","); live.add(key); st.chat++; st.murmur += g.length;
      let gs = groups.get(key); if (!gs) { gs = { turn: Math.floor(Math.random() * g.length), next: tnow + rnd(0, 1.5) }; groups.set(key, gs); }
      if (tnow < gs.next) continue;
      const who = g[gs.turn % g.length], dd = once(bus.voice, who.x, who.z, C.far); let end = tnow + .3;
      if (dd) end = phrase(dd, tnow + .02, voiceHz(who), C.syllables[0] + Math.floor(Math.random() * (C.syllables[1] - C.syllables[0] + 1)), C.gain, Math.random() < C.question);
      const kids = g.some(r => r.role === "child");
      if (Math.random() < (kids ? C.child_laugh : C.laugh)) {          // someone else in the group laughs at it
        const o = g[(gs.turn + 1) % g.length], d2 = once(bus.voice, o.x, o.z, C.far); if (d2) end = Math.max(end, laugh(d2, end + .05, voiceHz(o), C.laugh_gain));
      }
      gs.turn++;
      gs.next = end + (gs.turn % (2 + Math.floor(Math.random() * 2)) === 0 ? rnd(C.pause_s[0], C.pause_s[1]) : rnd(C.turn_gap_s[0], C.turn_gap_s[1]));
    }
    for (const k of groups.keys()) if (!live.has(k)) groups.delete(k);
    // the hum of a crowd: where many are gathered, a low bed and distant syllables from among them
    const Cr = PE.crowd, near = res.filter(r => !r.walking && r.pose !== 3 && Math.hypot(r.x - L.x, r.z - L.z) < Cr.far);
    const k = sm(Cr.min, Cr.full, near.length); st.crowd = near.length;
    if (!crowdBed) { crowdBed = gainOf(0); chain(noiseSrc(), filt("bandpass", Cr.hz, .9), crowdBed, bus.voice); }
    ramp(crowdBed.gain, Cr.gain * k * .5, .8);
    crowdT -= F.dt;
    if (k > 0 && crowdT <= 0) {
      const r = near[Math.floor(Math.random() * near.length)], dd = once(bus.voice, r.x, r.z, Cr.far);
      if (dd) phrase(dd, tnow + .02, voiceHz(r), 2 + Math.floor(Math.random() * 4), Cr.gain * (.5 + .5 * k), Math.random() < .2);
      crowdT = rnd(.5, 1.5) / (Cr.per_s * (1 + near.length / 4));
    }
    if (tnow - pruneT > 10) { pruneT = tnow; for (const [i, q] of pst) if (tnow - q.seen > 30) pst.delete(i); }
  }

  // ---- music (SND-010 to SND-012)
  let score = null, places = [], radio = null, radioIx = 0;
  let pv = null;                                         // a style played on its own by preview()
  function moodAt(m) { let best = MU.score.moods[MU.score.moods.length - 1]; for (const md of MU.score.moods) if (hm(md.from) <= m) best = md; return best; }
  function music(F) {
    if (!score) { score = band(moodAt(F.m), bus.music); }
    const md = moodAt(F.m); if (score.style !== md) score.style = md; st.mood = md.name;
    // music from places: the nearest open café or shop, the busker on a market day
    let best = null, bd = 1e9;
    for (const l of lots) {
      const pl = MU.places[l.kind]; if (!pl || !within(F.m, pl.from, pl.to) || pl.market_days_only && !D.day.market) continue;
      const cx = (l.rect[0] + l.rect[2]) / 2, cz = (l.rect[1] + l.rect[3]) / 2;
      const d = l === L.lot ? 0 : Math.hypot(Math.max(l.rect[0] - L.x, 0, L.x - l.rect[2]), Math.max(l.rect[1] - L.z, 0, L.z - l.rect[3]));
      if (d < pl.far && d < bd) { bd = d; best = { l, pl, x: l.door ? l.door[0] : cx, z: l.door ? l.door[1] : cz, cx, cz }; }
    }
    if (best && (!places[0] || places[0].l !== best.l)) {
      if (places[1]) places[1].b.out.disconnect();
      if (places[0]) { places[0].b.fade(0, .5); places[1] = places[0]; }
      const lp = filt("lowpass", 18000), sp = spot(lp); lp.connect(bus.music);
      const b = band(best.pl.style, sp.input); places[0] = { l: best.l, b, sp, lp, pl: best.pl }; b.fade(1, .8);
    } else if (!best && places[0]) { if (places[1]) places[1].b.out.disconnect(); places[0].b.fade(0, .6); places[1] = places[0]; places[0] = null; }
    let srcNear = 0;
    for (const p of places) {
      if (!p) continue;
      if (p === places[0]) {
        const inside = L.lot === p.l, open = p.pl.style === "busker", x = inside ? L.x : best.x, z = inside ? L.z : best.z;
        const g = inside ? 1 : p.sp.set(x, z, 1, p.pl.far); if (inside) { ramp(p.sp.g.gain, 1); ramp(p.sp.p.pan, 0); }
        ramp(p.lp.frequency, inside || open && !L.lot ? 18000 : S.indoors.muffle_hz * 1.6, .2); srcNear = Math.max(srcNear, g);
      }
      p.b.run();
    }
    if (places[1] && places[1].b.level < .01) { const o = places[1]; setTimeout(() => o.b.out.disconnect(), 2000); places[1] = null; }
    st.place = places[0] ? places[0].pl.style + " at " + places[0].l.name : null;
    // the car radio
    const sta = F.drive && radioIx > 0 ? MU.radio[radioIx - 1] : null;
    if (sta) {
      if (!radio) { const lp = filt("lowpass", 4200), hp = filt("highpass", 180); chain(hp, lp, bus.music); radio = band(sta.style || "calm", hp); }
      if (sta.podcast) { const sh = PC.shows[sta.podcast]; radio.style = sh.bed; if (Math.abs(radio.level - PC.bed_gain * 2) > .01) radio.fade(PC.bed_gain * 2, .3); podcast(F, sta); }
      else { radio.style = sta.style; radio.fade(1, .3); offAir(); caption(sta.name, null); }
      srcNear = 1;
    } else { if (radio && radio.level > 0) radio.fade(0, .2); offAir(); caption(null); }
    if (radio) radio.run();
    st.radio = F.drive ? radioIx : 0; st.station = sta ? sta.name : null;
    const k = (F.talk ? MU.score.talk_duck : 1) * (1 - (1 - MU.score.source_duck) * Math.min(1, srcNear * 2.5));
    score.fade(MU.score.gain * k, 1.2); score.run();
  }

  // ---- podcasts (SND-014): where a show is comes from the world clock, as a broadcast: pace seconds a game minute
  const PC = S.podcasts, plans = {};
  let onAir = null, podOut = null, capEl = null, capUntil = 0;
  function plan(id) {                                    // every part of the show in order: [start s, length s, intro or line]
    if (plans[id]) return plans[id];
    const sh = PC.shows[id], items = []; let t = 0;
    const speak = window.Talk && Talk.speak ? Talk.speak : text => ({ seq: [], words: [], asks: [], end: text.length * .065 });
    sh.episodes.forEach((ep, e) => {
      items.push({ t0: t, d: PC.intro_s, key: e + ":intro", ep }); t += PC.intro_s;
      ep.lines.forEach(([host, text], k) => { const ln = speak(text); items.push({ t0: t, d: ln.end + PC.gap_s, key: e + ":" + k, ep, host: sh.hosts[host], text, ln }); t += ln.end + PC.gap_s; });
    });
    return plans[id] = { items, total: t };
  }
  function podcast(F, sta) {
    const sh = PC.shows[sta.podcast], P = plan(sta.podcast), pos = ((F.m * D.pace) % P.total + P.total) % P.total;
    const it = P.items.find(x => pos >= x.t0 && pos < x.t0 + x.d) || P.items[0], key = sta.podcast + "/" + it.key;
    if (onAir !== key) {
      offAir(); onAir = key;
      podOut = gainOf(1); chain(podOut, filt("highpass", 160), filt("lowpass", 4600), bus.voice);
      if (it.host) voice(podOut, it.ln, it.host.pitch, PC.voice_gain, pos - it.t0);
      else if (pos - it.t0 < .6) recipe({ kind: "chime", hz: [523, 659, 784], len: .5, gain: .16 }, podOut);
    }
    caption(sh.title + " · " + it.ep.title, it.host ? it.host.name + ": " + it.text : "…", true);
    st.caption = it.host ? it.host.name + ": " + it.text : sh.title;
  }
  function offAir() { if (podOut) { podOut.gain.setTargetAtTime(0, now(), .03); const o = podOut; setTimeout(() => o.disconnect(), 300); podOut = null; } onAir = null; st.caption = null; }
  function caption(head, text, keep) {                   // what is on the radio, shown above the prompt; a music station only for a moment
    if (!capEl) {
      capEl = document.createElement("div");
      capEl.style.cssText = "position:fixed;left:50%;bottom:150px;transform:translateX(-50%);max-width:min(640px,90vw);padding:8px 16px;background:rgba(23,23,28,.72);border-radius:6px;color:var(--cream,#d9cfb6);font-family:var(--data,sans-serif);font-size:16px;line-height:1.35;text-align:center;pointer-events:none;display:none";
      capEl.innerHTML = '<div style="font-size:12px;letter-spacing:.06em;text-transform:uppercase;color:var(--dim,#a79f8c)"></div><div></div>';
      document.body.appendChild(capEl);
    }
    if (!head) { capEl.style.display = "none"; capUntil = 0; return; }
    if (!keep && !text && capUntil && performance.now() > capUntil) { capEl.style.display = "none"; return; }
    if (!keep && !capUntil) capUntil = performance.now() + 2500;
    if (keep) capUntil = 0;
    capEl.firstChild.textContent = "Radio · " + head; capEl.lastChild.textContent = text || ""; capEl.lastChild.style.display = text ? "block" : "none";
    capEl.style.display = "block";
  }

  // ---- each frame, from play.js
  let lastStep = null, birdT = 0, prevV, drive = null, prevGround = true, prevVy = 0;
  function frame(F) {
    if (!ready || ctx.state !== "running") return;
    L.x = F.x; L.z = F.z; L.yaw = F.yaw; L.lot = F.lot;
    const indoor = !!(F.lot && F.y < 30);
    ramp(outsideLP.frequency, indoor ? S.indoors.muffle_hz : 18000, .15); ramp(outside.gain, indoor ? S.indoors.muffle_gain : 1, .15);
    const day = sm(0, 1, Math.min((F.m - (D.day.sunrise * 60 - 40)) / 80, ((D.day.sunset * 60 + 40) - F.m) / 80));
    ramp(street.gain, A.street.night_gain + (A.street.day_gain - A.street.night_gain) * day, .5);
    ramp(streetLP.frequency, A.street.night_hz + (A.street.day_hz - A.street.night_hz) * day, .5);
    ramp(room.gain, indoor ? A.room.gain : 0, .3);
    ramp(crick.gain, within(F.m, A.crickets.from, A.crickets.to) && !indoor ? A.crickets.gain : 0, 1);
    let fb = null, fd = 1e9; for (const f of fountains) { const d = Math.hypot(f[0] - L.x, f[1] - L.z); if (d < fd) { fd = d; fb = f; } }
    if (fb) fount.set(fb[0], fb[1], A.fountain.gain, A.fountain.far);
    if (within(F.m, A.birds.from, A.birds.to) && !F.drive) {   // birds: now and then, somewhere round the listener; more by the gardens
      const g = gardens.some(p => Math.hypot(p[0] - L.x, p[1] - L.z) < A.birds.garden_m), rate = (g ? A.birds.garden_per_minute : A.birds.per_minute) / 60;
      birdT -= F.dt; if (birdT <= 0) { const a = Math.random() * 6.283, r = rnd(6, 28); chirp(L.x + Math.cos(a) * r, L.z + Math.sin(a) * r); birdT = rnd(.3, 1.7) / rate; }
    }
    // the player's steps and landings
    if (F.walk) {
      const w = F.walk, half = Math.floor(w.ph * 2);
      if (w.moving && w.onGround && lastStep !== null && half !== lastStep) step(L.x, L.z, w.surface, FS.player_gain * (w.run ? 1.4 : 1), 5);
      lastStep = half;
      if (w.onGround && !prevGround && prevVy < -3) act2("land", FS.land_gain * Math.min(1.5, -prevVy / 6));
      prevGround = w.onGround; prevVy = w.vy;
    }
    // the residents near: steps, sitting and standing, what they are doing, their talk, the crowd
    people(F.residents, F);
    // vehicles
    if (F.drive && !drive) drive = { since: now() }; else if (!F.drive) drive = null;
    if (drive) Object.assign(drive, F.drive, { prev: prevV }); prevV = F.drive ? F.drive.v : undefined;
    engines(F.cars, drive);
    trams(F.trams, F.tramZ);
    music(F);
  }
  function act2(name, gain) { recipe({ kind: "thud", hz: 120, len: .15, gain }, bus.action); st.lastAct = name; }

  return {
    init(L0, F0, G0) { lots = L0; fountains = F0; gardens = G0; },
    start() { if (!ctx) { try { build(); } catch (e) { console.warn("sound off:", e); return; } this.mix({}); } if (ctx.state === "suspended") ctx.resume(); },
    toggle() { on = !on; try { localStorage.setItem("wasted.sound", on ? "on" : "off"); } catch (e) { } if (ctx) ramp(master.gain, on ? S.mix.master * MIXK.master : 0, .05); return on; },
    mix(m, onNow) {                                      // the menu's sound settings (UI-009): a share of each bus's level from World/rules/sound.json, and on or off
      Object.assign(MIXK, m); if (onNow !== undefined) on = !!onNow; if (!ctx) return;
      master.gain.value = on ? S.mix.master * MIXK.master : 0; for (const k of ["world", "voice", "action"]) bus[k].gain.value = S.mix[k] * MIXK[k]; bus.music.gain.value = S.mix.music * MIXK.music;
    },
    act, line, hush, frame, render,
    preview(style) {                                     // play one music style on its own (jukebox.html); null stops it
      if (!ready) return false;
      if (pv) { const o = pv; o.b.fade(0, .15); clearInterval(o.timer); setTimeout(() => o.b.out.disconnect(), 800); pv = null; }
      if (!style) return true;
      const b = band(style, bus.music); b.fade(1, .2); pv = { b, timer: setInterval(() => b.run(), 40) }; b.run(); return true;
    },
    car(inOut) { act("car_door"); if (inOut === "in" && ready) { const s = ctx.createBufferSource(); s.buffer = noise; const g = ctx.createGain(); chain(s, filt("bandpass", 300, 3), g, bus.action); const t = now() + .3; env(g, t, .05, .25, V.player.start_len); s.start(t); s.stop(t + V.player.start_len + .2); } },
    horn(down) { if (horn) ramp(horn.gain, down ? V.player.horn_gain : 0, .01); },
    radio() { radioIx = (radioIx + 1) % (MU.radio.length + 1); act("radio"); capUntil = 0; return radioIx ? MU.radio[radioIx - 1].name : "off"; },
    level() { if (!meter) return 0; const d = new Float32Array(meter.fftSize); meter.getFloatTimeDomainData(d); let q = 0; for (const v of d) q += v * v; return Math.sqrt(q / d.length); },
    bus(name) { if (!meter) return 0; return bus[name] ? bus[name].gain.value : 0; },
    state: () => Object.assign({ on, running: !!ctx && ctx.state === "running" }, st),
  };
})();
