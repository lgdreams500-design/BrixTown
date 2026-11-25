"""The slice's sound for the Unreal build (SND-001 to SND-004, SND-007, SND-009, SND-017).

The slice makes every sound live with Web Audio (Prototype/play/sound.js, from the recipes in World/rules/sound.json).
The Unreal build plays recordings of those same sounds: this script runs sound.js itself in headless Edge, renders its
own sound-making functions into an OfflineAudioContext (steps, sitting and standing, the action sounds, birds, the tram's
bell, engines, the tram's hum, whine and rumble, the street, crickets, the fountain, the skid) and writes them as WAV:

  Game/Data/sounds/*.wav, index.json   the recordings (mono, 16-bit), and for each its length, whether it loops and the
                                       scale it was recorded at (the game multiplies its volume back by 1 / scale).
                                       Loops are cross-faded end into start, so they repeat without a click. The music
                                       (eight bars of each style, at the music bus's level) and the rewards are made Ogg Vorbis by the
                                       engine's own encoder, the BrixEncodeOgg commandlet of the editor build.
  Tests/values/sound_city_01.json      what sound.js's own logic does in scripted moments, with its audio graph swapped
                                       for recorders: which steps and pose sounds people() plays (SND-017), each engine
                                       voice's pitch and level by speed (SND-002), the tram's levels, whine and bells
                                       (SND-004), and place(): how loud and where a sound is heard (SND-007). The BrixWorld
                                       tests check the C++ port against them.

Where a level depends on what is happening (a step's loudness, an engine's level with speed) the sound is recorded at
level 1 and the game applies the slice's level from World/rules/sound.json; a recipe's own level (a chime, a thud) is
recorded in. Math.random is seeded, so a run gives the same files. sound.js stays the source while the slice is kept
(ADR-0045): run this again after changing it or World/rules/sound.json.
  python Tools/unreal/sound_values.py

Edge is driven over its DevTools socket (a small client below, the standard library only): an OfflineAudioContext
renders on its own thread, which --dump-dom and virtual time do not wait for.
"""
import base64
import json
import os
import re
import socket
import struct
import subprocess
import sys
import tempfile
import time
import urllib.request
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SOUND_JS = os.path.join(ROOT, "Prototype", "play", "sound.js")
RULES = os.path.join(ROOT, "World", "rules", "sound.json")
OUT_DIR = os.path.join(ROOT, "Game", "Data", "sounds")
OUT_VALUES = os.path.join(ROOT, "Tests", "values", "sound_city_01.json")
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
UNREAL_CMD = r"C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
UPROJECT = os.path.join(ROOT, "Game", "BrixTown.uproject")
OGG_QUALITY = 40   # the engine's Ogg Vorbis quality (1 to 100) for the music, the only long recordings
RATE = 22050   # Hz: the slice's sounds have little above 8 kHz (the brightest, a grass step, is filtered at 3.2 to 4.8 kHz)

# Inside sound.js's closure, before its closing return: the sound-making functions are private to it.
HARNESS = r"""
window.__SOUND = async function (CASES) {
  Math.random = seeded(CASES.seed);                     // the same recordings every run
  const out = { trace: {}, sounds: [] };
  const keep = { step, sfx, phrase, laugh, once, recipe, engineVoice };
  // ---- the logic, with the audio graph swapped for recorders
  const param = () => ({ value: 0, v: 0, setTargetAtTime(v) { this.v = v; }, setValueAtTime(v) { this.v = v; }, cancelScheduledValues() { }, linearRampToValueAtTime() { }, exponentialRampToValueAtTime() { } });
  const clock = { t: 0 };
  ctx = { get currentTime() { return clock.t; }, state: "running" };
  L.x = 0; L.z = 0; L.yaw = 0;
  let ev = [];
  const r6 = v => +(+v).toFixed(6);
  // people(): steps and pose sounds (SND-017)
  step = (x, z, surface, gain, far, pitch) => { ev.push(["step", r6(x), r6(z), surface, r6(gain), far, r6(pitch || 1)]); };
  sfx = (name, x, z, far) => { ev.push(["sfx", name, r6(x), r6(z), far || PE.far]); };
  phrase = (d, t) => t; laugh = (d, t) => t;
  crowdBed = { gain: param() }; pst.clear(); groups.clear();
  out.trace.people = CASES.people.map(F => { ev = []; clock.t = F.t; people(F.residents, { dt: F.dt }); return ev; });
  // engines(): the nearest cars' voices and the player's own car (SND-002, SND-009)
  const fakeVoice = () => ({ hz: 0, ty: 0, sp: { on: false, k: 0, far: 0, g: { gain: param() }, p: { pan: param() }, set(x, z, k, far) { this.on = true; this.k = k; this.far = far; return place(x, z, far)[0]; } }, set(hz, ty) { this.hz = hz; this.ty = ty; } });
  pool = Array.from({ length: V.engine.voices }, fakeVoice); mine = fakeVoice(); horn = { gain: param() }; skid = { gain: param() };
  recipe = (r, d) => ev.push(["recipe", r.kind, r.hz, r.len, r.gain]);
  let dr = null, pv;
  out.trace.engines = CASES.engines.map(F => {
    ev = []; clock.t = F.t; for (const v of pool) v.sp.on = false;
    if (F.drive && !dr) dr = { since: now() }; else if (!F.drive) dr = null;   // as frame() does
    if (dr) Object.assign(dr, F.drive, { prev: pv }); pv = F.drive ? F.drive.v : undefined;
    engines(F.cars, dr);
    return { pool: pool.map(v => v.sp.on ? [r6(v.hz), r6(v.ty), r6(v.sp.k), v.sp.far] : null), mine: [r6(mine.hz), r6(mine.ty), r6(mine.sp.g.gain.v), r6(skid.gain.v)], events: ev };
  });
  // trams(): two voices for the nearest trams, and the bell (SND-004)
  tramV.length = 0; tramSeen.clear();
  for (let k = 0; k < 2; k++) tramV.push({ sp: fakeVoice().sp, w: { frequency: param() }, wg: { gain: param() }, rg: { gain: param() } });
  once = (dest, x, z, far) => { const [a] = place(x, z, far); return a < .004 ? null : { a }; };
  recipe = (r, d) => ev.push(["bell", r.kind, r.hz, r.len, r.gain, r6(d.a)]);
  out.trace.trams = CASES.trams.map(F => {
    ev = []; clock.t = F.t; for (const v of tramV) v.sp.on = false;
    trams(F.list, F.tz);
    return { voices: tramV.map(v => v.sp.on ? [r6(v.sp.k), v.sp.far, r6(v.w.frequency.v), r6(v.wg.gain.v), r6(v.rg.gain.v)] : null), events: ev };
  });
  // place(): how loud and where (SND-007)
  out.trace.place = CASES.place.map(([x, z, far, yaw]) => { L.yaw = yaw; return place(x, z, far).map(r6); });
  L.yaw = 0;
  Object.assign(window.__keep = {}, keep);
  ({ step, sfx, phrase, laugh, once, recipe, engineVoice } = keep);
  pool = null; mine = null; horn = null; skid = null; tramV.length = 0; tramSeen.clear(); crowdBed = null; pst.clear();

  // ---- recordings: each sound into an offline context of its own, positioned nowhere (the game places it)
  const R = CASES.rate;
  once = d => d; ready = true;
  async function record(name, o) {                      // o: { secs, make(dest), loop?, lead? (s skipped), len? (s kept), xf? (s), fold? (stereo to mono) }
    const lead = o.lead || 0, xf = o.loop ? (o.xf || .08) : 0, secs = o.loop ? lead + o.len + xf : o.secs;
    const oc = new OfflineAudioContext(o.fold ? 2 : 1, Math.ceil(secs * R), R);
    ctx = oc; noise = makeNoise(); bus = { world: oc.destination, action: oc.destination, voice: oc.destination }; outside = oc.destination;
    rwOut = null; hall = null; ksCache = new Map();
    o.make(oc.destination);
    const buf = await oc.startRendering();
    let d = buf.getChannelData(0);
    if (o.fold) { const r = buf.getChannelData(1); d = d.map((v, i) => (v + r[i]) * Math.SQRT1_2); }   // the music's stereo as one channel, its power kept
    if (o.minus) {                                      // the part a voice adds, less what it makes without it (a voice's parts share one output)
      const oc2 = new OfflineAudioContext(1, Math.ceil(secs * R), R);
      ctx = oc2; noise = makeNoise(); bus = { world: oc2.destination, action: oc2.destination, voice: oc2.destination }; outside = oc2.destination;
      o.minus(oc2.destination);
      const d2 = (await oc2.startRendering()).getChannelData(0); d = d.map((v, i) => v - d2[i]);
    }
    let s;
    if (o.loop) {                                       // the tail after the loop's end faded into its start: it repeats seamlessly
      const a = Math.round(lead * R), n = Math.round(o.len * R), m = Math.round(xf * R);
      s = d.slice(a, a + n);
      for (let i = 0; i < m; i++) { const w = Math.sin(i / m * Math.PI / 2); s[i] = s[i] * w + d[a + n + i] * Math.cos(i / m * Math.PI / 2); }
    } else {                                            // a one-shot, its silent tail cut
      let e = d.length; while (e > 1 && Math.abs(d[e - 1]) < 2e-4) e--; s = d.slice(0, Math.min(d.length, e + Math.round(.01 * R)));
    }
    let peak = 0; for (const v of s) peak = Math.max(peak, Math.abs(v));
    const scale = peak > .98 ? .98 / peak : 1, pcm = new Int16Array(s.length);
    for (let i = 0; i < s.length; i++) pcm[i] = Math.max(-32767, Math.min(32767, Math.round(s[i] * scale * 32767)));
    const bytes = new Uint8Array(pcm.buffer); let bin = "";
    for (let i = 0; i < bytes.length; i += 8192) bin += String.fromCharCode.apply(null, bytes.subarray(i, i + 8192));
    out.sounds.push({ name, loop: !!o.loop, seconds: +(s.length / R).toFixed(4), scale: +scale.toFixed(6), peak: +peak.toFixed(4), pcm: btoa(bin) });
  }
  // steps on each surface, a few of each as step() varies them; children's lighter and higher (SND-003, SND-017)
  for (const surface of Object.keys(FS.surfaces)) for (const [who, pitch] of [["adult", 1], ["child", PE.steps.child_pitch]])
    for (let k = 0; k < CASES.step_variants; k++) await record("step_" + surface + "_" + who + "_" + k, { secs: .25, make: () => step(L.x, L.z, surface, 1, 5, pitch) });
  // a resident sitting, lying down, getting up (SND-017); the player's actions and the interface (SND-009)
  for (const name of new Set(Object.values(PE.poses))) await record("person_" + name, { secs: SND[name].len + .4, make: d => recipe(SND[name], d) });
  for (const [name, r] of Object.entries(S.actions)) await record("act_" + name, { secs: r.len * 2.2 + .3, make: d => recipe(r, d) });
  await record("act_land", { secs: .5, make: () => act2("land", 1) });
  await record("car_in", { secs: V.player.start_len + .9, make: () => carSound("in") });
  for (const [name, tr, kind] of [["car_bump", "engines", "recipe"], ["tram_bell", "trams", "bell"]]) {   // as the traced logic plays them
    const fr = out.trace[tr].find(f => f.events.some(e => e[0] === kind));
    if (!fr) throw new Error("no " + kind + " in the " + tr + " trace: change the scripted moments");
    const r = fr.events.find(e => e[0] === kind);
    await record(name, { secs: r[3] * 2.2 + .3, make: d => recipe({ kind: r[1], hz: r[2], len: r[3], gain: r[4] }, d) });
  }
  for (let k = 0; k < CASES.bird_variants; k++) await record("bird_" + k, { secs: 1.2, make: () => chirp(L.x, L.z) });
  // an engine voice at two pitches, its tyres at the speed each pitch means; the game crossfades and pitches between them (SND-002)
  const E = V.engine;
  for (const [name, hz] of [["engine_lo", CASES.engine_hz[0]], ["engine_hi", CASES.engine_hz[1]]]) {
    const v = (hz - E.idle_hz) / E.hz_per_mps;
    await record(name, { loop: true, lead: 2, len: 1, make: d => { const e = engineVoice(d); e.sp.g.gain.value = 1; e.set(hz, Math.min(1, Math.abs(v) / 10) * E.tyres_gain * 8); } });
  }
  // the tram's hum, and the whine and rumble its voice adds when moving (SND-004)
  const tramVoice = (wg, rg) => () => { trams([], 0); const t = tramV[0]; tramV[1].sp.g.gain.value = 0; t.sp.g.gain.value = 1; t.w.frequency.value = V.tram.whine_hz; t.wg.gain.value = wg; t.rg.gain.value = rg; };
  const fresh = () => { tramV.length = 0; };
  await record("tram_hum", { loop: true, lead: .5, len: 1, make: d => { fresh(); tramVoice(0, 0)(); } });
  await record("tram_whine", { loop: true, lead: .5, len: 1, make: d => { fresh(); tramVoice(1, 0)(); }, minus: d => { fresh(); tramVoice(0, 0)(); } });
  await record("tram_rumble", { loop: true, lead: .5, len: 2, make: d => { fresh(); tramVoice(0, 1)(); }, minus: d => { fresh(); tramVoice(0, 0)(); } });
  fresh();
  // the ambience (SND-001): the street by day and by night, the room, crickets, a fountain
  const amb = set => () => { ambience(); street.gain.value = 0; room.gain.value = 0; crick.gain.value = 0; set(); };
  await record("street_day", { loop: true, lead: .3, len: 2, make: amb(() => { street.gain.value = 1; streetLP.frequency.value = A.street.day_hz; }) });
  await record("street_night", { loop: true, lead: .3, len: 2, make: amb(() => { street.gain.value = 1; streetLP.frequency.value = A.street.night_hz; }) });
  await record("room", { loop: true, lead: .3, len: 2, make: amb(() => { room.gain.value = 1; }) });
  await record("crickets", { loop: true, lead: 0, len: 1 / .7, xf: .01, make: amb(() => { crick.gain.value = 1; }) });
  await record("fountain", { loop: true, lead: .3, len: 2, make: amb(() => { fount.g.gain.value = 1; }) });
  // the player's car skidding (SND-009)
  await record("car_skid", { loop: true, lead: .3, len: 2, make: () => { pool = null; mine = null; engines([], null); skid.gain.cancelScheduledValues(0); skid.gain.value = 1; } });
  // money coming in (SND-019 to SND-021): which note each coin rings, as reward() picks it, traced; then every note recorded
  const RQ = RW.quest, keepStruck = struck; let notes = [];
  struck = (voice, t, midi) => notes.push(midi);
  ctx = new OfflineAudioContext(1, 64, R); rwOut = null; bus = { world: ctx.destination, action: ctx.destination, voice: ctx.destination };
  out.trace.rewards = [];
  for (const [i, n] of CASES.reward_takes) { notes = []; reward("coin_take", i, n); out.trace.rewards.push(["coin_take", i, n, notes[0]]); }
  for (let i = 0; i < RW.coins.interest_coins + 4; i++) { notes = []; reward("interest_coin", i, 0); out.trace.rewards.push(["interest_coin", i, 0, notes[0]]); }
  struck = keepStruck;
  for (const w of ["quest_start", "coin_out", "quest_end", "interest_start", "interest_end", "achievement"]) await record("reward_" + w, { secs: 6, make: () => reward(w, 0, 1) });
  for (let k = 0; k < 4; k++) await record("reward_coin_land_" + k, { secs: 2, make: () => reward("coin_land", 0, 1) });
  for (let k = 0; k <= RQ.top_step; k++) await record("reward_coin_take_" + k, { secs: 3, make: () => reward("coin_take", k, RQ.top_step + 1) });
  for (let i = 0; i < RW.coins.interest_coins; i++) await record("reward_interest_coin_" + i, { secs: 3, make: () => reward("interest_coin", i, 0) });
  // music (SND-010 to SND-012): each mood of the score and each style, a section of eight bars after one that warms its
  // room, at the music bus's level and through its compressor; the radio's styles through the radio's filters (music())
  const radioStyles = new Set(MU.radio.filter(r => r.style).map(r => r.style));
  for (const [name, style] of MU.score.moods.map(m => [m.name, m]).concat(Object.keys(MU.styles).map(k => [k, k]))) {
    const s = styleOf(style), len = CASES.music_bars * 4 * 60 / s.bpm;
    await record("music_" + name, { loop: true, fold: true, lead: .1 + len, len, xf: .4, make: d => {
      bus.music = musicChain(d);
      let into = bus.music;
      if (radioStyles.has(name)) { const hp = filt("highpass", 180), lp = filt("lowpass", 4200); chain(hp, lp, bus.music); into = hp; }
      const b = band(style, into, CASES.seed); b.fade(1, .01); b.run(.1 + 2 * len + 1);
    } });
  }
  pool = null; mine = null; ready = false;
  return out;
};
"""


class Ws:
    """Just enough of a WebSocket client for one DevTools call (RFC 6455: masked text frames out, any frames in)."""

    def __init__(self, url):
        hostport, path = url[len("ws://"):].split("/", 1)
        host, port = hostport.split(":")
        self.s = socket.create_connection((host, int(port)), timeout=600)
        key = base64.b64encode(os.urandom(16)).decode()
        self.s.sendall(("GET /%s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                        "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n" % (path, hostport, key)).encode())
        head = b""
        while b"\r\n\r\n" not in head:
            head += self.s.recv(1)
        if b" 101 " not in head.split(b"\r\n")[0]:
            raise RuntimeError("DevTools refused the socket: %r" % head[:200])

    def _read(self, n):
        b = b""
        while len(b) < n:
            c = self.s.recv(n - len(b))
            if not c:
                raise RuntimeError("DevTools closed the socket")
            b += c
        return b

    def send(self, obj):
        data, mask = json.dumps(obj).encode(), os.urandom(4)
        n = len(data)
        size = bytes([0x80 | n]) if n < 126 else bytes([0x80 | 126]) + struct.pack(">H", n) if n < 65536 else bytes([0x80 | 127]) + struct.pack(">Q", n)
        self.s.sendall(bytes([0x81]) + size + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

    def recv(self):
        msg = b""
        while True:
            b0, b1 = self._read(2)
            n = b1 & 0x7f
            if n == 126:
                n = struct.unpack(">H", self._read(2))[0]
            elif n == 127:
                n = struct.unpack(">Q", self._read(8))[0]
            payload = self._read(n)
            if b0 & 0x0f == 9:   # a ping
                continue
            msg += payload
            if b0 & 0x80:
                return json.loads(msg.decode("utf-8"))


def run_in_edge(page, expression):
    """Opens page in headless Edge and returns the value of expression (a promise is awaited)."""
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as tmp:
        path = os.path.join(tmp, "sound_values.html")
        open(path, "w", encoding="utf-8").write(page)
        prof = os.path.join(tmp, "edge")
        p = subprocess.Popen([EDGE, "--headless=new", "--user-data-dir=" + prof, "--no-first-run", "--remote-debugging-port=0",
                              "file:///" + path.replace("\\", "/")], stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            port_file, t0 = os.path.join(prof, "DevToolsActivePort"), time.time()
            while not (os.path.exists(port_file) and open(port_file).read().strip()):
                if time.time() - t0 > 60:
                    sys.exit("Edge opened no DevTools port")
                time.sleep(.2)
            port = open(port_file).read().split()[0]
            pages = []
            for _ in range(100):
                pages = [t for t in json.load(urllib.request.urlopen("http://127.0.0.1:%s/json/list" % port)) if t["type"] == "page"]
                if pages:
                    break
                time.sleep(.2)
            ws = Ws(pages[0]["webSocketDebuggerUrl"])
            ws.send({"id": 1, "method": "Runtime.evaluate", "params": {"expression": expression, "awaitPromise": True, "returnByValue": True}})
            while True:
                m = ws.recv()
                if m.get("id") == 1:
                    break
            try:
                ws.send({"id": 2, "method": "Browser.close"})
            except OSError:
                pass
        finally:
            try:
                p.wait(15)
            except subprocess.TimeoutExpired:
                p.kill()
    res = m.get("result", {})
    if "exceptionDetails" in res:
        d = res["exceptionDetails"]
        sys.exit("sound.js failed in Edge: " + (d.get("exception", {}).get("description") or json.dumps(d))[:3000])
    return res["result"].get("value")


def cases():
    """The scripted moments the logic is traced through (inputs saved with the results for the C++ tests)."""
    dt = 1 / 60.0
    people = []
    # walkers near the listener at (0, 0): each walks in place, its phase on at its own rate. A crowd of eight in step at
    # 2..9 m shows the cap on steps a frame; one at 12 m is beyond the steps' reach; poses change for three more.
    walkers = [
        dict(i=1, x=3.0, z=0.0, rate=1.7, role="commuter", run=False, inside=False, kind=None),      # stone
        dict(i=2, x=0.0, z=5.0, rate=2.3, role="child", run=False, inside=False, kind="garden"),     # grass, a child
        dict(i=3, x=-4.0, z=0.0, rate=2.9, role="commuter", run=True, inside=False, kind="market"),  # running, on the market's stone
        dict(i=4, x=0.0, z=-6.0, rate=1.5, role="elder", run=False, inside=True, kind="home"),       # wood, indoors
        dict(i=5, x=12.0, z=0.0, rate=1.8, role="commuter", run=False, inside=False, kind=None),     # too far for steps
        dict(i=6, x=-1.0, z=-8.5, rate=1.9, role="child", run=True, inside=False, kind="garden"),    # a child running on grass
    ]
    crowd = [dict(i=20 + k, x=2.0 + k, z=2.0, rate=2.0, role="shopper", run=False, inside=False, kind=None) for k in range(8)]
    poses = [  # i, x, z, [(frame, pose)]
        (40, 4.0, 1.0, [(0, 0), (30, 2), (100, 0)]),    # sits down, gets up
        (41, -2.0, 7.0, [(0, 2), (60, 3), (140, 0)]),   # lies down from sitting, gets up
        (42, 11.0, 0.0, [(0, 0), (50, 2)]),             # too far to hear
        (43, 0.0, 9.5, [(0, 1), (70, 0), (90, 2)]),     # stops walking, then sits
    ]
    for f in range(180):
        t = 10 + f * dt
        res = []
        for w in walkers + (crowd if f < 120 else []):
            res.append(dict(i=w["i"], x=w["x"], z=w["z"], act="walk", walking=True, pose=1, ph=round((0.37 + w["rate"] * f * dt) % 1, 6),
                            inside=w["inside"], role=w["role"], run=w["run"], kind=w["kind"]))
        for i, x, z, seq in poses:
            pose = [p for (ff, p) in seq if ff <= f][-1]
            res.append(dict(i=i, x=x, z=z, act="walk" if pose == 1 else "idle", walking=pose == 1, pose=pose, ph=round((f * dt * 2) % 1, 6) if pose == 1 else 0,
                            inside=False, role="commuter", run=False, kind=None))
        people.append(dict(t=round(t, 6), dt=dt, residents=res))
    engines = []
    for f in range(140):
        t = 50 + f * 0.1
        cars = [dict(x=5.0 + f * 0.05, z=3.0, v=round(min(14.0, f * 0.15), 4), van=False),
                dict(x=-20.0, z=10.0, v=6.0, van=True),
                dict(x=40.0, z=-30.0, v=11.0 + (f % 7), van=False),
                dict(x=0.0, z=69.0 - f * 0.2, v=3.0, van=False),           # comes into reach
                dict(x=8.0, z=-8.0, v=0.0, van=False),                     # idling at a light
                dict(x=-30.0, z=-12.0, v=25.0, van=True),                  # faster than max_hz allows
                dict(x=15.0, z=15.0, v=4.5, van=False),
                dict(x=-50.0, z=40.0, v=9.0, van=False)]                   # an eighth: more cars than voices
        drive = None
        if 20 <= f < 130:   # the player gets in, waits for the start, speeds up, brakes hard, hits something and rolls back
            g = f - 20
            v = 0.0 if g < 12 else min(16.0, (g - 12) * 0.4) if g < 60 else 16.0 - (g - 60) * 0.5 if g < 80 else 6.0 if g < 86 else -1.5 if g < 90 else 0.0
            drive = dict(v=round(v, 4), gas=12 <= g < 60, brake=60 <= g < 80)
        engines.append(dict(t=round(t, 6), cars=cars, drive=drive))
    trams = []
    xs = 300.0
    for f in range(160):
        t = 200 + f * 0.1
        moving = not (50 <= f < 100)
        if moving:
            xs += 8.0 * 0.1 * (0.5 if f < 10 else 1.0)
        trams.append(dict(t=round(t, 6), tz=-14.0, list=[[round(xs - 310, 4), not moving, 715.0], [round(-xs + 200, 4), False, 735.0], [400.0, True, 755.0]]))
    place = [[dx, dz, far, yaw] for (dx, dz) in ((0, 0), (0.3, 0.2), (1, 0), (2.5, 0), (3, 4), (-6, 8), (10, 0), (0, -20), (30, 40), (49, 0), (60, 0), (69.9, 0), (75, 0))
             for far in (None, 30, 70) for yaw in (0, 1.2)]
    reward_takes = [[i, n] for n in (1, 2, 3, 5, 8, 10, 12, 18, 25) for i in range(n)]
    return dict(seed=20261125, rate=RATE, step_variants=4, bird_variants=6, engine_hz=[36, 76], music_bars=8, reward_takes=reward_takes,
                people=people, engines=engines, trams=trams, place=place)


def main():
    src = open(SOUND_JS, encoding="utf-8").read()
    ret = "\n  return {\n    init(L0, F0, G0)"
    if ret not in src:
        sys.exit("sound.js: its closing return has changed; update sound_values.py")
    # car() is a method of the object sound.js returns: its line is lifted out as a function the harness can call
    m = re.search(r"^    car\(inOut\) \{.*\},$", src, re.M)
    if not m:
        sys.exit("sound.js: car(inOut) has changed; update sound_values.py")
    harness = HARNESS.replace("window.__SOUND = ", "function carSound" + m.group(0)[len("    car"):-1] + "\nwindow.__SOUND = ", 1)
    rules = json.load(open(RULES, encoding="utf-8"))
    C = cases()
    page = ("<!doctype html><meta charset=utf-8><script>window.WASTED = " + json.dumps(dict(sound=rules, day=dict(sunrise=6, sunset=20, market=False)))
            + ";</script><script>\n" + src.replace(ret, "\n" + harness + ret, 1) + "\n</script>")
    out = run_in_edge(page, "(async () => { const C = %s; return await window.__SOUND(C); })()" % json.dumps(C))
    if not out:
        sys.exit("Edge gave nothing back")
    os.makedirs(OUT_DIR, exist_ok=True)
    index, total = {}, 0
    for s in out["sounds"]:
        pcm = base64.b64decode(s["pcm"])
        path = os.path.join(OUT_DIR, s["name"] + ".wav")
        with wave.open(path, "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(pcm)
        total += os.path.getsize(path)
        index[s["name"]] = dict(file=s["name"] + ".wav", loop=s["loop"], seconds=s["seconds"], scale=s["scale"])
    # the music as Ogg Vorbis, by the engine's own encoder (the BrixEncodeOgg commandlet: build the editor first)
    music = sorted(n for n in index if n.startswith("music_") or n.startswith("reward_"))   # the long ones: the music, the rewards ringing on
    if music:
        subprocess.run([UNREAL_CMD, UPROJECT, "-run=BrixEncodeOgg", "-Dir=" + OUT_DIR, "-Files=" + ",".join(music),
                        "-Quality=%d" % OGG_QUALITY, "-unattended", "-nullrhi", "-nosplash", "-stdout"], stdin=subprocess.DEVNULL, capture_output=True, timeout=900)
        for n in music:
            ogg, wav = os.path.join(OUT_DIR, n + ".ogg"), os.path.join(OUT_DIR, n + ".wav")
            if not os.path.exists(ogg) or os.path.getmtime(ogg) < os.path.getmtime(wav):
                sys.exit("the BrixEncodeOgg commandlet made no %s.ogg: build BrixTownEditor and run this again" % n)
            total += os.path.getsize(ogg) - os.path.getsize(wav)
            os.remove(wav)
            index[n]["file"] = n + ".ogg"
    for f in os.listdir(OUT_DIR):   # recordings of sounds that are no longer made
        if (f.endswith(".wav") or f.endswith(".ogg")) and f[:-4] not in index:
            os.remove(os.path.join(OUT_DIR, f))
    with open(os.path.join(OUT_DIR, "index.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(dict(note="Generated by Tools/unreal/sound_values.py from Prototype/play/sound.js and World/rules/sound.json. Do not edit: change those and run it again.",
                       rate=RATE, engine_hz=C["engine_hz"], sounds=index), f, indent=1)
    os.makedirs(os.path.dirname(OUT_VALUES), exist_ok=True)
    with open(OUT_VALUES, "w", encoding="utf-8", newline="\n") as f:
        json.dump(dict(note="Generated by Tools/unreal/sound_values.py: sound.js people(), engines(), trams() and place() in scripted moments, for the SND tests.",
                       cases=dict(people=C["people"], engines=C["engines"], trams=C["trams"], place=C["place"], rewards=C["reward_takes"]), trace=out["trace"]), f, separators=(",", ":"))
    print("wrote %d sounds (%.0f KB) to %s, and %s" % (len(index), total / 1024, os.path.relpath(OUT_DIR, ROOT), os.path.relpath(OUT_VALUES, ROOT)))


if __name__ == "__main__":
    main()
