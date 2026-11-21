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
// score by time of day, music from cafés, shops and the market busker, the car radio). The score fades under a
// conversation and under any nearer music.
var Sound = (() => {
  const D = window.WASTED, S = D.sound;
  if (!S || !(window.AudioContext || window.webkitAudioContext)) {
    const no = () => { };
    return { start: no, toggle: no, act: no, line: no, hush: no, frame: no, horn: no, radio: no, car: no, init: no, state: () => ({ on: false, reason: S ? "no Web Audio" : "no sound data" }) };
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
  let master, meter, bus = {}, outside, outsideLP, noise, L = { x: 0, z: 0, yaw: 0, lot: null }, lots = [], fountains = [], gardens = [];
  const st = { engines: 0, trams: 0, murmur: 0, place: null, radio: 0, mood: null, steps: 0, birds: 0, line: false, lastAct: null };

  // ---- the graph
  function build() {
    const AC = window.AudioContext || window.webkitAudioContext; ctx = new AC();
    master = ctx.createGain(); master.gain.value = on ? S.mix.master : 0; master.connect(ctx.destination);
    meter = ctx.createAnalyser(); meter.fftSize = 2048; master.connect(meter);          // for tests: how loud the mix is
    for (const k of ["world", "voice", "action", "music"]) { bus[k] = ctx.createGain(); bus[k].gain.value = S.mix[k]; bus[k].connect(master); }
    outsideLP = ctx.createBiquadFilter(); outsideLP.type = "lowpass"; outsideLP.frequency.value = 18000;
    outside = ctx.createGain(); outside.connect(outsideLP); outsideLP.connect(bus.world);
    const n = ctx.sampleRate * 2, b = ctx.createBuffer(1, n, ctx.sampleRate), d = b.getChannelData(0);
    for (let i = 0; i < n; i++) d[i] = Math.random() * 2 - 1;
    noise = b;
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
  function act(name, x, z) {                             // an action sound, here or out in the world
    if (!ready || !S.actions[name]) return; st.lastAct = name;
    const d = once(bus.action, x, z, 30); if (d) recipe(S.actions[name], d);
  }

  // ---- instruments for the music (SND-010 to SND-012)
  function note(dest, t, midi, len, voice, vel) {
    const f = mtof(midi), g = ctx.createGain(); g.connect(dest);
    const osc = (type, hz, det) => { const o = ctx.createOscillator(); o.type = type; o.frequency.value = hz; if (det) o.detune.value = det; o.start(t); o.stop(t + len + 1.6); return o; };
    if (voice === "pad") {
      const lp = filt("lowpass", 900, .7); chain(lp, g);
      osc("sawtooth", f, -7).connect(lp); osc("sawtooth", f, 7).connect(lp);
      g.gain.setValueAtTime(0, t); g.gain.linearRampToValueAtTime(vel * .25, t + .35); g.gain.setTargetAtTime(0, t + len, .4);
    } else if (voice === "keys") {
      const lp = filt("lowpass", 2600); chain(lp, g);
      osc("triangle", f).connect(lp); const o2 = osc("sine", f * 2); const g2 = ctx.createGain(); g2.gain.value = .3; chain(o2, g2, lp);
      env(g, t, .006, vel * .5, len * 1.4 + .3);
    } else if (voice === "guitar") {
      const lp = ctx.createBiquadFilter(); lp.type = "lowpass"; lp.frequency.setValueAtTime(3200, t); lp.frequency.exponentialRampToValueAtTime(500, t + .35);
      chain(lp, g); osc("sawtooth", f).connect(lp); osc("triangle", f * 2, 4).connect(lp);
      env(g, t, .003, vel * .35, len + .4);
    } else if (voice === "bass") {
      const lp = filt("lowpass", 500); chain(lp, g); osc("triangle", f).connect(lp);
      env(g, t, .01, vel * .55, len * .9);
    } else {                                             // pluck
      const lp = filt("lowpass", 2200); chain(lp, g); osc("triangle", f).connect(lp); osc("square", f, 3).connect(chain(gainOf(.12), lp));
      env(g, t, .004, vel * .45, len * .8 + .15);
    }
  }
  function gainOf(v) { const g = ctx.createGain(); g.gain.value = v; return g; }
  function drum(dest, t, kind, vel) {
    const g = ctx.createGain(); g.connect(dest);
    if (kind === "kick") {
      const o = ctx.createOscillator(); o.frequency.setValueAtTime(120, t); o.frequency.exponentialRampToValueAtTime(42, t + .14);
      o.connect(g); env(g, t, .002, vel * .8, .22); o.start(t); o.stop(t + .3);
    } else {
      const s = ctx.createBufferSource(); s.buffer = noise;
      const f = kind === "hat" ? filt("highpass", 7000) : kind === "snare" ? filt("bandpass", 1800, .7) : filt("bandpass", 5000, .5);
      chain(s, f, g); env(g, t, kind === "brush" ? .02 : .002, vel * (kind === "hat" ? .18 : kind === "snare" ? .4 : .22), kind === "brush" ? .14 : kind === "hat" ? .05 : .16);
      s.start(t, Math.random()); s.stop(t + .3);
    }
  }
  // A small endless tune in a style: chords from the style's scale, a walking melody, bass and soft drums, scheduled
  // a little ahead of the audio clock. Set .gain to fade it; it stops making notes while silent.
  function band(style, dest) {
    const out = ctx.createGain(); out.gain.value = 0; out.connect(dest);
    let sty = style, t = now() + .1, step = 0, bar = 0, mel = 0;
    const b = {
      out, level: 0,
      set style(s) { sty = s; }, get style() { return sty; },
      run() {
        if (b.level < .002) { t = now() + .1; return; }
        const s = MU.styles[sty] || sty, sc = MU.scales[s.scale], q = 60 / s.bpm / 4, swing = s.drums === "brush" || sty === "swing";
        while (t < now() + .3) {
          const deg = s.chords[bar % s.chords.length] - 1, tone = k => s.root + sc[(deg + k) % 7] + 12 * Math.floor((deg + k) / 7);
          const tt = t + (swing && step % 4 === 2 ? q * .33 : 0);
          if (step === 0) {
            if (s.voice === "pad" || s.voice === "keys") for (const k of [0, 2, 4]) note(out, tt, tone(k) - 12, q * 16 * (s.voice === "pad" ? 1 : .5), s.voice, .5);
            if (s.bass) note(out, tt, tone(0) - 24, q * 6, "bass", .9);
          }
          if (s.bass && step === 8) note(out, tt, tone(Math.random() < .5 ? 4 : 0) - 24, q * 6, "bass", .7);
          if ((s.voice === "pluck" || s.voice === "guitar") && step % 2 === 0) note(out, tt, tone([0, 2, 4, 2][(step / 2) % 4]) - 12 + (s.voice === "guitar" ? 0 : 12), q * 2, s.voice, .35);
          if (step % 2 === 0 && Math.random() < s.density) {           // melody: a step or two along the scale from the last note
            mel = cl(mel + [-2, -1, -1, 1, 1, 2, 0][Math.floor(Math.random() * 7)], -3, 9);
            const n = s.root + 12 + sc[((mel % 7) + 7) % 7] + 12 * Math.floor(mel / 7);
            note(out, tt, step % 8 === 0 ? tone(Math.random() < .5 ? 0 : 2) : n, q * (Math.random() < .3 ? 4 : 2), s.voice === "pad" ? "keys" : s.voice, .45);
          }
          if (s.drums === "brush") { if (step % 4 === 0) drum(out, tt, "brush", step % 8 === 4 ? 1 : .55); }
          else if (s.drums === "kit") { if (step % 8 === 0) drum(out, tt, "kick", 1); if (step % 8 === 4) drum(out, tt, "snare", 1); if (step % 2 === 0) drum(out, tt, "hat", 1); }
          t += q; step = (step + 1) % 16; if (step === 0) bar++;
        }
      },
      fade(v, tc) { b.level = v; ramp(out.gain, v * ((MU.styles[sty] || {}).gain || .5), tc || .6); },
    };
    return b;
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
  function step(x, z, surface, gain, far) {
    const d = once(bus.world, x, z, far); if (!d) return; st.steps++;
    const s = FS.surfaces[surface] || FS.surfaces.stone, src = ctx.createBufferSource(), g = ctx.createGain(); src.buffer = noise;
    chain(src, filt("bandpass", s.hz * rnd(.85, 1.15), s.q), g, d); env(g, now(), .002, gain, s.len); src.start(now(), Math.random()); src.stop(now() + s.len + .05);
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

  // ---- voices (SND-005, SND-006): a resident's line voiced syllable by syllable in time with their mouth (CHR-009), and
  // the murmur between residents who stand together. Never words (Q-14: text first).
  const pitchOf = i => VO.pitch.min + hashOf(i) * (VO.pitch.max - VO.pitch.min);
  function syllable(dest, t, hz, shape, len, gain) {
    const fm = VO.shapes[shape]; if (!fm) return;
    const o = ctx.createOscillator(); o.type = "sawtooth"; o.frequency.setValueAtTime(hz, t); o.frequency.linearRampToValueAtTime(hz * rnd(.92, 1.06), t + len);
    const g = ctx.createGain(); g.connect(dest);
    for (const [k, hzF] of fm.entries()) { const f = filt("bandpass", hzF, 6), fg = gainOf(k ? 2 : 3); chain(o, f, fg, g); }
    env(g, t, .015, gain, Math.max(.05, len)); o.start(t); o.stop(t + len + .12);
  }
  let lineOut = null;
  function line(ln, i, x, z) {
    if (!ready) return; hush();
    lineOut = gainOf(1); const d = once(bus.voice, x, z, 30) || bus.voice; lineOut.connect(d); st.line = true;
    const t0 = now() + .02, base = pitchOf(i), ask = ln.asks || [], words = ln.words || [];
    for (const s of ln.seq) {
      if (!VO.shapes[s.shape] || s.shape === "MBP" && s.d < .05) continue;
      const w = words.filter(w => w.t0 <= s.t0).pop(), up = ask.some(a => a > s.t0 && a - s.t0 < .35) ? 1.18 : 1;
      syllable(lineOut, t0 + s.t0, base * up * (w && w.long ? 1.08 : 1), s.shape, Math.min(.22, s.d), VO.line_gain);
    }
    setTimeout(() => { st.line = false; }, (ln.end + .2) * 1000);
  }
  function hush() { if (lineOut) { lineOut.gain.setTargetAtTime(0, now(), .03); const o = lineOut; setTimeout(() => o.disconnect(), 300); lineOut = null; } st.line = false; }
  const talkers = new Map();
  const SHAPES = Object.keys(VO.shapes).filter(s => s !== "MBP");
  function murmur(res) {
    const M = VO.murmur, can = res.filter(r => !r.walking && r.pose !== 3 && M.acts.indexOf(r.act) >= 0 && Math.hypot(r.x - L.x, r.z - L.z) < M.far);
    const chat = can.filter(r => can.some(o => o !== r && Math.hypot(o.x - r.x, o.z - r.z) < M.pair_m)).sort((a, b) => Math.hypot(a.x - L.x, a.z - L.z) - Math.hypot(b.x - L.x, b.z - L.z)).slice(0, M.groups * 2);
    st.murmur = chat.length;
    for (const r of chat) {
      let tk = talkers.get(r.i); if (!tk) { tk = { next: now() + rnd(0, 2) }; talkers.set(r.i, tk); }
      if (now() < tk.next) continue;
      const d = once(bus.voice, r.x, r.z, M.far); let t = now() + .02; const n = 2 + Math.floor(Math.random() * 7), base = pitchOf(r.i);
      if (d) for (let k = 0; k < n; k++) { const len = 1 / M.syllables_per_s * rnd(.6, 1.2); syllable(d, t, base * rnd(.94, 1.08), SHAPES[Math.floor(Math.random() * SHAPES.length)], len * .8, M.gain); t += len; }
      tk.next = t + rnd(M.pause_s[0], M.pause_s[1]);
    }
    if (talkers.size > 200) talkers.clear();
  }

  // ---- music (SND-010 to SND-012)
  let score = null, places = [], radio = null, radioIx = 0;
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
    if (F.drive && radioIx > 0) {
      const sty = MU.radio[radioIx - 1];
      if (!radio) { const lp = filt("lowpass", 4200), hp = filt("highpass", 180); chain(hp, lp, bus.music); radio = band(sty, hp); }
      radio.style = sty; if (radio.level < 1) radio.fade(1, .3); srcNear = 1;
    } else if (radio && radio.level > 0) radio.fade(0, .2);
    if (radio) radio.run();
    st.radio = F.drive ? radioIx : 0;
    const k = (F.talk ? MU.score.talk_duck : 1) * (1 - (1 - MU.score.source_duck) * Math.min(1, srcNear * 2.5));
    score.fade(MU.score.gain / .5 * k, 1.2); score.run();
  }

  // ---- each frame, from play.js
  let lastStep = null, lastRes = new Map(), birdT = 0, prevV, drive = null, prevGround = true, prevVy = 0;
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
    // residents walking near: their steps; residents standing together: their murmur
    let n = 0; const seen = new Map();
    for (const r of F.residents) {
      if (!r.walking) continue; const d = Math.hypot(r.x - L.x, r.z - L.z); if (d > FS.far) continue;
      const half = Math.floor((r.ph || 0) * 2), p = lastRes.get(r.i); seen.set(r.i, half);
      if (p !== undefined && p !== half && n++ < FS.max) step(r.x, r.z, r.inside ? "wood" : "stone", FS.resident_gain, FS.far);
    }
    lastRes = seen;
    murmur(F.residents);
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
    start() { if (!ctx) { try { build(); } catch (e) { console.warn("sound off:", e); return; } } if (ctx.state === "suspended") ctx.resume(); },
    toggle() { on = !on; try { localStorage.setItem("wasted.sound", on ? "on" : "off"); } catch (e) { } if (ctx) ramp(master.gain, on ? S.mix.master : 0, .05); return on; },
    act, line, hush, frame,
    car(inOut) { act("car_door"); if (inOut === "in" && ready) { const s = ctx.createBufferSource(); s.buffer = noise; const g = ctx.createGain(); chain(s, filt("bandpass", 300, 3), g, bus.action); const t = now() + .3; env(g, t, .05, .25, V.player.start_len); s.start(t); s.stop(t + V.player.start_len + .2); } },
    horn(down) { if (horn) ramp(horn.gain, down ? V.player.horn_gain : 0, .01); },
    radio() { radioIx = (radioIx + 1) % (MU.radio.length + 1); act("radio"); return radioIx ? MU.radio[radioIx - 1] : "off"; },
    level() { if (!meter) return 0; const d = new Float32Array(meter.fftSize); meter.getFloatTimeDomainData(d); let q = 0; for (const v of d) q += v * v; return Math.sqrt(q / d.length); },
    bus(name) { if (!meter) return 0; return bus[name] ? bus[name].gain.value : 0; },
    state: () => Object.assign({ on, running: !!ctx && ctx.state === "running" }, st),
  };
})();
