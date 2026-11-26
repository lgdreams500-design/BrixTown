"""The slice's people for the Unreal build (ART-004, ANIM-001, ANIM-002).

Runs Prototype/play/people.js itself in headless Edge (this machine has no Node) and writes two files:
  Game/Data/people.json             the tables people.js draws from: movement signatures, the palette, skins, hair and
                                    eye colours, what each role wears, the player's look. BrixWorld's BrixPeople reads them, so the Unreal
                                    people are dressed and walk from the same numbers as the slice's.
  Tests/values/people_city_01.json  looks of a dozen residents and bodies drawn by people.js draw() (vertex counts and
                                    sampled vertex positions, the far figures' boxes; talking faces too): the test
                                    ART004_people_are_drawn_as_the_slice_draws_them checks the C++ port against them.
  Game/Data/talk.json               what conversations need from the slice's code: the words for what a resident is
                                    doing or going to do (play.js DOING, GOING) and the voiced pieces of every line
                                    (audio/voices/index.js, Tools/voice/make_dialogue_voices.py; SND-018).
  Tests/values/boards_city_01.json  riders and carried boards drawn by people.js draw() with board() and ride(), and the
                                    speeds boards.js step() gives through a ride of each kind (CITY-098 to CITY-101), for
                                    CITY099_boards_are_ridden_and_drawn_as_the_slice_does.
  Tests/values/talk_city_01.json    every line of World/rules/dialogue.json mouthed by conversations.js speak() (CHR-009),
                                    for CHR009_lines_are_mouthed_as_the_slice_mouths_them.

people.js stays the source while the slice is kept (ADR-0045): run this again after changing it.
  python Tools/unreal/people_values.py
"""
import html
import json
import math
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
PEOPLE_JS = os.path.join(ROOT, "Prototype", "play", "people.js")
DAY = os.path.join(ROOT, "Game", "Data", "day.json")
OUT_DATA = os.path.join(ROOT, "Game", "Data", "people.json")
OUT_VALUES = os.path.join(ROOT, "Tests", "values", "people_city_01.json")
OUT_TALK = os.path.join(ROOT, "Game", "Data", "talk.json")
OUT_TALK_VALUES = os.path.join(ROOT, "Tests", "values", "talk_city_01.json")
CONV_JS = os.path.join(ROOT, "Prototype", "play", "conversations.js")
DODGE_JS = os.path.join(ROOT, "Prototype", "play", "dodge.js")
OUT_DODGE_VALUES = os.path.join(ROOT, "Tests", "values", "dodge_city_01.json")
BOARDS_JS = os.path.join(ROOT, "Prototype", "play", "boards.js")
OUT_BOARD_VALUES = os.path.join(ROOT, "Tests", "values", "boards_city_01.json")
OUT_DESIGN_VALUES = os.path.join(ROOT, "Tests", "values", "designer_city_01.json")
VOICES_JS = os.path.join(ROOT, "Prototype", "play", "audio", "voices", "index.js")
RULES = os.path.join(ROOT, "World", "rules")
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"

HARNESS = r"""
try {
const CASES = %CASES%;
setSun([-.45 / 1.0005, .74 / 1.0005, .5 / 1.0005], [0, 1.6, 6]);   // the studio sun of play.js (its self-shadow only changes shading, not shape)
const out = { tables: { base: BASE, signatures: {}, palette: K, skins: SKINS, hairs: HAIRS, eyes: EYES, roles: ROLE },
  looks: [], bodies: [] };
for (const [k, v] of Object.entries(SIG)) { const s = Object.assign({}, v); delete s.name; out.tables.signatures[k] = s; }
const sigName = s => Object.keys(SIG).find(k => SIG[k] === s);
const CLIMB = Object.assign({}, SIG.Careful, %CLIMB%, { name: "Climb" });   // dressing.js climbSig(), cut from dressing.js
const plain = L => { const o = Object.assign({}, L); o.sig = sigName(L.sig); delete o.cheek; return o; };

const looks = CASES.looks.map(([i, role, sig]) => look(i, role, sig));
out.tables.player = plain(player());   // the player until the designed character replaces it (CHR-001)
looks.forEach((L, k) => out.looks.push(Object.assign({ i: CASES.looks[k][0], role: CASES.looks[k][1], sigName: CASES.looks[k][2] }, plain(L))));
for (const c of CASES.bodies) {
  const ms = { a: new Float32Array(1 << 20), n: 0, boxes: [], box(x0, y0, z0, x1, y1, z1, col) { this.boxes.push([x0, y0, z0, x1, y1, z1, col[0], col[1], col[2]]); } };
  const L = looks[c.look];
  const mod = c.floor != null ? { floor: c.floor, arms: c.driver ? [[1, 58, 10, 48], [1, 58, 10, 48]] : null } : c.arms || c.chest || c.straddle ? { arms: c.arms, chest: c.chest, straddle: c.straddle } : null;   // in a car (cars.js seat()); a gesture (dodge.js, smash.js, wet.js, dressing.js); astride a seesaw
  draw(ms, c.x, c.y, c.z, c.yaw, L, c.pose, { ph: c.ph, t: c.t, lod: c.lod, ff: c.ff, fl: 14, face: c.face || null, mod,
    sig: c.sig === "Climb" ? CLIMB : c.sig ? SIG[c.sig] : undefined, hood: c.hood, umb: c.umb, look: c.look_at, sleep: c.sleep });
  const nv = ms.n / 10, step = Math.max(1, Math.floor(nv / 240)), pts = [];
  for (let v = 0; v < nv; v += step) pts.push([v, ms.a[v * 10], ms.a[v * 10 + 1], ms.a[v * 10 + 2]]);
  out.bodies.push(Object.assign({}, c, { vertices: nv, samples: pts, boxes: ms.boxes }));
}
out.boardBodies = [];                                    // CITY-099, CITY-101: a board in the hand, and riders on each kind (play.js draws them so)
for (const c of CASES.boardBodies) {
  const ms = { a: new Float32Array(1 << 20), n: 0, boxes: [], box(x0, y0, z0, x1, y1, z1, col) { this.boxes.push([x0, y0, z0, x1, y1, z1, col[0], col[1], col[2]]); } };
  const K = Object.assign({ id: c.board }, CASES.boards.kinds[c.board]);
  draw(ms, c.x, c.y, c.z, c.yaw, looks[c.look], c.pose, { ph: c.ph, t: c.t, lod: c.lod, ff: c.ff, fl: 14, board: { kind: K, carry: c.carry }, ride: c.carry ? null : { h: K.top_m, push: c.push, ph: c.ph } });
  const nv = ms.n / 10, step = Math.max(1, Math.floor(nv / 240)), pts = [];
  for (let v = 0; v < nv; v += step) pts.push([v, ms.a[v * 10], ms.a[v * 10 + 1], ms.a[v * 10 + 2]]);
  out.boardBodies.push(Object.assign({}, c, { vertices: nv, samples: pts }));
}
if (CASES.designs) {                                     // CHR-001, CHR-002: design() of designer values, and the bodies it draws
  baseCache.clear();                                      // drawn from a fresh start, as the C++ test draws them
  const DL = CASES.designs.map(v => design(v, CASES.designer));
  out.designs = DL.map(plain); out.designBodies = [];
  for (const c of CASES.designBodies) {
    const ms = { a: new Float32Array(1 << 20), n: 0, boxes: [], box(x0, y0, z0, x1, y1, z1, col) { this.boxes.push([x0, y0, z0, x1, y1, z1, col[0], col[1], col[2]]); } };
    draw(ms, c.x, c.y, c.z, c.yaw, DL[c.design], c.pose, { ph: c.ph, t: c.t, lod: c.lod, ff: c.ff, fl: 14, face: c.face || null });
    const nv = ms.n / 10, step = Math.max(1, Math.floor(nv / 240)), pts = [];
    for (let v = 0; v < nv; v += step) pts.push([v, ms.a[v * 10], ms.a[v * 10 + 1], ms.a[v * 10 + 2]]);
    out.designBodies.push(Object.assign({}, c, { vertices: nv, samples: pts, boxes: ms.boxes }));
  }
}
window.__OUT = out;
} catch (e) { document.getElementById("out").textContent = "@@" + JSON.stringify({ error: String(e.stack || e) }) + "@@"; }
"""

# after people.js: conversations.js with the dialogue and speech rules, and every line mouthed (CHR-009)
# dodge.js (CITY-070, CITY-071) with the play.js globals it reads stubbed: a car driven east past residents standing, walking
# and sitting at either side of its line, a wall to the north, then it stops; what dodge.js apply() lays on each, frame by frame
DODGE_HARNESS = r"""
try {
const out = window.__OUT; if (!out) throw new Error(window.__ERR || "people.js gave nothing");
window.blocked = p => p[2] < -2.6;                       // a wall north of z -2.6
window.groundBase = (x, z) => .12;
let V = 0; window.Cars = { speed: () => V };
const RES = [[11, -20, .3, 0, 0], [27, -15, 1.0, .4, 0], [43, -10, -1.0, 2.0, 0], [58, -5, 2.5, 1.0, 0], [71, -18, .5, 1.57, 1], [96, -12, .2, 0, 2], [130, 20, .3, 0, 0], [205, -2, -.4, 3.0, 1]];
const frames = [];
for (let f = 0; f < 330; f++) {
  const e = f / 30, t = 100 + e, cx = -40 + 9 * Math.min(e, 5);
  V = e < 5 ? 9 : 0;
  Dodge.frame(t, { pose: 4, x: cx, z: .3, y: .12, yaw: Math.PI / 2 });
  frames.push({ t, car: [cx, .3, V], rows: RES.map(([i, x, z, yaw, pose]) => {
    const r = { x, y: .12, z, yaw, pose, walking: pose === 1, ph: .3, inside: false }, q = Dodge.apply(i, r);
    return q === r ? null : [q.x, q.z, q.y - .12, q.yaw, q.pose, q.walking ? 1 : 0, q.ph, q.sig ? "Run" : "", q.mod ? q.mod.arms : null, q.mod ? q.mod.chest : null, q.shout || ""];
  }) });
}
out.dodge = { residents: RES, yaw: Math.PI / 2, frames };
} catch (e) { window.__ERR = String(e.stack || e); window.__OUT = null; }
"""

# boards.js (CITY-098, CITY-099): the speeds step() gives through a ride of each kind: W held, let go to glide, S to brake,
# indoors (capped), a knock (slow), another kind made the one in use; and what give(), toggle() and use() leave
BOARDS_HARNESS = r"""
try {
const out = window.__OUT; if (!out) throw new Error(window.__ERR || "people.js gave nothing");
const R = Boards.R, rides = [];
Boards.give([], null);
const none = { kind: Boards.kind(), toggle: Boards.toggle() };
Boards.give(R.auto_owned, R.auto_equipped);
const auto = { owned: Boards.owned(), kind: Boards.kind().id, riding: Boards.riding };
for (const k of R.order) {
  Boards.give(R.auto_owned, k); Boards.toggle();
  const vs = [];
  const run = (n, dt, go, back, cap) => { for (let i = 0; i < n; i++) vs.push([dt, go ? 1 : 0, back ? 1 : 0, cap == null ? -1 : cap, Boards.step(dt, go, back, cap)]); };
  run(40, 1 / 30, true, false, null); run(30, 1 / 60, false, false, null); run(10, 1 / 30, true, true, null); run(20, 1 / 30, false, true, null);
  run(90, 1 / 20, true, false, null); run(30, 1 / 30, true, false, R.indoor_top_kmh); run(20, 1 / 30, true, false, R.mall_top_kmh);
  Boards.slow(.4); vs.push([0, 0, 0, -2, Boards.v]); run(10, 1 / 30, false, false, null);
  rides.push({ kind: k, steps: vs });
}
Boards.give(R.auto_owned, "neon_boost"); Boards.toggle(); for (let i = 0; i < 200; i++) Boards.step(1 / 20, true, false, null);
const fast = Boards.v; Boards.use("manual");
out.boards = { none, auto, rides, use: [fast, Boards.v, Boards.kind().id], toggled: [Boards.toggle(), Boards.v] };
} catch (e) { window.__ERR = String(e.stack || e); window.__OUT = null; }
"""

TALK_HARNESS = r"""
try {
const out = window.__OUT; if (!out) throw new Error(window.__ERR || "people.js gave nothing");
out.speak = %LINES%.map(t => [t, Talk.speak(t)]);
out.tram = %TRAM%;   // the slice's tramAt() at minutes through a day (CITY-041)
document.getElementById("out").textContent = "@@" + JSON.stringify(out) + "@@";
} catch (e) { document.getElementById("out").textContent = "@@" + JSON.stringify({ error: String(e.stack || e) }) + "@@"; }
"""


def js_object(src, name):
    """A one-line object literal `const NAME = { key: "text", ... };` in play.js, as a dict."""
    m = re.search(r"^const " + name + r" = (\{.*?\});", src, re.M)
    if not m:
        sys.exit("play.js: " + name + " not found; update people_values.py")
    return json.loads(re.sub(r'([{,]\s*)(\w+):', r'\1"\2":', m.group(1)))


def tram_js():
    """play.js tramAt() as it is, with what it reads (D.tram, D.pace, P, OX, OX1) from the city's data, run at minutes of a day."""
    play = open(os.path.join(ROOT, "Prototype", "play", "play.js"), encoding="utf-8").read()
    m = re.search(r"^function tramAt\(m\) \{.*?^\}\n", play, re.S | re.M)
    if not m:
        sys.exit("play.js: tramAt() not found; update people_values.py")
    city = json.load(open(os.path.join(ROOT, "World", "city_01", "city.json"), encoding="utf-8"))
    cal = json.load(open(os.path.join(RULES, "calendar.json"), encoding="utf-8"))
    P = city["grid"]["pitch"]
    wall = city.get("wall", {}).get("cells", [0, 0, city["grid"]["columns"], city["grid"]["rows"]])
    pre = "const D = %s, P = %s, OX = %s, OX1 = %s;" % (json.dumps(dict(tram=cal["tram"], pace=cal["calendar"]["real_seconds_per_minute"])), P, wall[0] * P, wall[2] * P)
    minutes = [330 + 0.37 * k for k in range(3000)]
    return "(() => { " + pre + m.group(0) + " return " + json.dumps(minutes) + ".map(t => [t, tramAt(t)]); })()"


def design_cases(d):
    """Designer values to try (CHR-001, CHR-002): the default, then each hair style with sliders at their ends and between,
    every garment, bottom, walk and extra, colours from across the palette and the swatches."""
    keys, pal = list(d["sliders"]), list(d["palette"])
    out = [dict(d["default"])]
    garments, bottoms, walks = list(d["garment"]), list(d["bottom_type"]), list(d["walk"])
    for n, hair in enumerate(d["hair"]):
        v = dict(d["default"], name="Case %d" % n, hair=hair)
        for j, k in enumerate(keys):
            s = d["sliders"][k]
            f = ((n + j) % 3) / 2.0                                # min, middle, max in turn
            v[k] = round(s["min"] + (s["max"] - s["min"]) * f, 4)
        v.update(skin=(n * 3) % len(d["skin"]), hair_color=n % len(d["hair_color"]), eyes=(n * 2) % len(d["eyes"]),
                 garment=garments[n % len(garments)], top=pal[n % len(pal)], bottom_type=bottoms[n % len(bottoms)],
                 bottom=pal[(n * 5 + 3) % len(pal)], shoes=pal[(n * 7 + 1) % len(pal)],
                 scarf=pal[(n + 2) % len(pal)] if n % 2 else None, cap=pal[(n + 4) % len(pal)] if n % 3 == 1 else None,
                 apron=pal[(n + 6) % len(pal)] if n % 4 == 2 else None, bag=n % 3 == 0, tash=n % 2 == 1, walk=walks[n % len(walks)])
        out.append(v)
    return out


def bad_designs(d):
    """Designer values each wrong in one way, for the checks (CHR-013)."""
    D, out = d["default"], []
    for k, s in d["sliders"].items():
        out.append(dict(D, **{k: s["max"] + s["step"]}))
        out.append(dict(D, **{k: s["min"] - s["step"]}))
    out += [dict(D, skin=len(d["skin"])), dict(D, hair_color=-1), dict(D, eyes=len(d["eyes"])), dict(D, hair="mohican"), dict(D, garment="cape"),
            dict(D, bottom_type="kilt"), dict(D, walk="Strut"), dict(D, top="pink"), dict(D, shoes="gold"), dict(D, cap="neon"),
            dict(D, name=""), dict(D, name="   "), dict(D, name="x" * (d["name"]["max"] + 1)), dict(D, name="x" * d["name"]["max"])]
    return out


def outfit_cases(d, parts):
    """Outfits to check (CHR-014): one of every part, and ones that change what an outfit may not or are missing a part."""
    D = d["default"]
    good = dict({k: D[k] for k in parts}, name="Work")
    return [good, dict(good, name=""), dict(good, height=1.7), dict(good, hair="mohican"), dict(good, top="pink"),
            {k: v for k, v in good.items() if k != "shoes"}, dict(good, cap="ochre", bag=True, name="Out")]


def face_cases():
    """Talking faces as conversations.js blends them (CHARACTER_FACE §1): an expression of speech.json and a mouth shape."""
    sp = json.load(open(os.path.join(RULES, "speech.json"), encoding="utf-8"))
    def shape(n):
        m = sp["shapes"][n] if isinstance(n, str) else n
        return dict(width=m["width"] * sp["draw"]["width"], opening=m["opening"] * sp["draw"]["opening"], round=m.get("round", 0),
                    teeth=m["teeth"], tongue=m["tongue"], curve=m.get("curve", 0))
    out = []
    for mood, mouth, nod in (("happy", "AA", 0.03), ("surprised", "OH", 0), ("sad", "rest", 0), ("angry", "EE", 0.05), ("thinking", "MBP", 0), ("neutral", "FV", 0.07)):
        e = sp["expressions"][mood]
        mo = shape(mouth)
        out.append(dict(mouth=mo, curve=e["curve"] + mo["curve"] * .5, brow_l=e["brow_l"], brow_r=e["brow_r"], lid=e["lid"],
                        squint=e["squint"], open=e["open"], gaze=e["gaze"], nod=nod))
    return out


def main():
    src = open(PEOPLE_JS, encoding="utf-8").read()
    ret = "return { SIG, look,"
    if ret not in src:
        sys.exit("people.js: its closing return line has changed; update people_values.py")
    # the tables are private to people.js: hand them out too, and run the harness inside its closure
    day = json.load(open(DAY, encoding="utf-8"))
    roles, sig_of = day["roles"], day.get("role_sig", {})
    looks = []
    for i in range(12):
        r = day["residents"][i]
        looks.append([i, r.get("u") or roles[r["r"]], sig_of.get(roles[r["r"]], "Easy")])
    for role in ("shopkeeper", "courier", "elder", "child", "cafe", "binman"):   # every garment, apron, cap, bag, scarf
        looks.append([1000 + len(looks), role, "Springy"])
    looks.append([7, "commuter", "Hurried"])
    looks.append([3, "creative", "Careful"])
    bodies = []
    for k in range(len(looks)):
        for lod in (0, 1, 2, 3):
            bodies.append(dict(look=k, x=1.5, y=0.12, z=-2.0, yaw=0.7 + 0.3 * k, pose=1, ph=(0.13 + 0.17 * k) % 1, t=5.0 + k, lod=lod, ff=0.6))
        bodies.append(dict(look=k, x=0.0, y=0.0, z=0.0, yaw=0.0, pose=0, ph=0.0, t=2.0 + 0.7 * k, lod=1 if k % 2 else 0, ff=1.0))
        bodies.append(dict(look=k, x=-3.0, y=0.62, z=1.0, yaw=2.0 - 0.2 * k, pose=2, ph=0.0, t=3.0 + 0.4 * k, lod=k % 4, ff=0.8))   # on a bench (CITY-017)
        bodies.append(dict(look=k, x=4.0, y=0.6, z=-1.0, yaw=0.3 * k, pose=2, ph=0.0, t=1.0 + 0.3 * k, lod=k % 3, ff=1.0, floor=-0.45, driver=k % 2 == 0))   # in a car (CITY-040)
    # gestures, other walks, hoods and umbrellas (dodge.js, smash.js, wet.js: CITY-070, CITY-071, CITY-076, CITY-079, CITY-080)
    coat, umb = [0.25, 0.3, 0.4], [0.44, 0.24, 0.18]
    for k in range(len(looks)):
        lod = k % 3
        bodies.append(dict(look=k, x=0.5, y=0.12, z=1.0, yaw=0.2 * k, pose=0, ph=0.0, t=1.7 + k, lod=lod, ff=1.0,
                           arms=[[0.7, 55, 25, 95], [0.7, 55, 25, 95]], chest=[-6.3, 0]))   # startled by a car
        bodies.append(dict(look=k, x=0.5, y=0.12, z=1.0, yaw=0.5 + 0.2 * k, pose=1, ph=(0.31 + 0.11 * k) % 1, t=2.1 + k, lod=lod, ff=1.0, sig="Run",
                           arms=[[0.8, 40, 30, 100], [0.8, 30, 34, 105]], chest=[10, 0]))   # the scramble
        bodies.append(dict(look=k, x=0.5, y=0.12, z=1.0, yaw=1.0 + 0.2 * k, pose=0, ph=0.0, t=2.5 + k, lod=lod, ff=1.0,
                           arms=[[0.45, 10, 14, 70], [0.9, 171, 8, 71]], chest=[6.3, 0]))   # a fist shaken at the driver
        bodies.append(dict(look=k, x=-1.0, y=0.12, z=0.0, yaw=0.3 * k, pose=1, ph=(0.07 + 0.13 * k) % 1, t=3.3 + k, lod=lod, ff=1.0, hood=coat,
                           sig="Hurried", arms=[None, None]))   # a hood up, hurrying in the rain
        bodies.append(dict(look=k, x=-1.0, y=0.12, z=0.0, yaw=0.3 * k, pose=1 if k % 2 else 0, ph=(0.21 + 0.13 * k) % 1, t=3.9 + k, lod=lod, ff=1.0, umb=umb,
                           arms=[None, [1, 52, 12, 84]]))   # an umbrella up
        bodies.append(dict(look=k, x=-3.0, y=0.62, z=1.0, yaw=0.3 * k, pose=2, ph=0.0, t=4.4 + k, lod=lod, ff=1.0, umb=coat,
                           arms=[None, [1, 52, 12, 84]]))   # under an umbrella on a bench
        bodies.append(dict(look=k, x=-1.0, y=0.12, z=0.0, yaw=0.3 * k, pose=1, ph=(0.41 + 0.13 * k) % 1, t=5.1 + k, lod=lod, ff=1.0,
                           sig="Hurried", arms=[[1, 150, 30, 118], None], chest=[6, 0]))   # a hand over the head
    # the other layers people.js takes (CHARACTER_MOTION §4, ANIM-004, ANIM-005; WLD-032, CITY-075): the head and eyes turned to
    # look at the player, the eyes shut asleep, astride a seesaw, climbing a ladder hand over hand, hands on a frame's top bar,
    # a repair crew bent over their work
    reach = lambda ph, up: [[1, up + math.sin(ph * 6.283) * 18, 10, 30], [1, up - math.sin(ph * 6.283) * 18, 10, 30]]   # dressing.js reach()
    for k in range(len(looks)):
        lod = k % 3
        look = [(-0.85 + 0.15 * k) * (1 if k % 2 else 0.6), 0.25 - 0.04 * k]
        bodies.append(dict(look=k, x=0.2, y=0.12, z=0.4, yaw=0.4 * k, pose=0, ph=0.0, t=1.2 + k, lod=lod, ff=1.0, look_at=look))
        bodies.append(dict(look=k, x=0.2, y=0.12, z=0.4, yaw=0.4 * k, pose=1, ph=(0.27 + 0.19 * k) % 1, t=2.6 + k, lod=lod, ff=1.0, look_at=look))
        bodies.append(dict(look=k, x=0.0, y=0.6, z=0.0, yaw=0.2 * k, pose=2, ph=0.0, t=3.4 + k, lod=lod, ff=1.0, look_at=[look[0], -look[1]]))
        # asleep in bed: people.js lies a body head to +z, which the port does at yaw pi (it turns it to lie along its bed)
        bodies.append(dict(look=k, x=1.0, y=0.5, z=2.0, yaw=math.pi, pose=3, ph=0.0, t=1.6 + 0.5 * k, lod=k % 2, ff=1.0, sleep=True))
        bodies.append(dict(look=k, x=-2.0, y=0.52, z=0.5, yaw=1.0 + 0.3 * k, pose=2, ph=0.0, t=2.8 + k, lod=lod, ff=1.0, straddle=True))
        ph = (0.13 + 0.21 * k) % 1
        bodies.append(dict(look=k, x=0.0, y=0.9, z=-1.0, yaw=1.5708, pose=1, ph=ph, t=3.1 + k, lod=lod, ff=1.0, sig="Climb", arms=reach(ph, 150)))
        bodies.append(dict(look=k, x=0.5, y=1.22, z=0.0, yaw=0.3 * k, pose=0, ph=0.0, t=4.2 + k, lod=lod, ff=1.0, arms=[[1, 45, 12, 15], [1, 45, 12, 15]]))
        w = math.sin((5.0 + k) * 5 + k) * 20
        bodies.append(dict(look=k, x=2.0, y=0.12, z=-0.5, yaw=2.0 - 0.3 * k, pose=0, ph=0.0, t=5.0 + k, lod=lod, ff=1.0, arms=[[1, 60 + w, 20, 70], [1, 60 - w, 20, 70]], chest=[24, 0]))
    for n, face in enumerate(face_cases()):   # someone in a conversation: the full face control set (CHR-007)
        bodies.append(dict(look=n * 3, x=0.0, y=0.0, z=0.0, yaw=0.4 * n, pose=0, ph=0.0, t=1.3 + n, lod=0, ff=1.0, face=face))
        bodies.append(dict(look=n * 3 + 1, x=0.0, y=0.0, z=0.0, yaw=0.4 * n, pose=0, ph=0.0, t=1.8 + n, lod=0, ff=1.0, face=face, look_at=[0.5 - 0.2 * n, 0.1]))   # talking while looking aside
    boards = json.load(open(os.path.join(RULES, "boards.json"), encoding="utf-8"))
    board_bodies = []
    for n, kind in enumerate(boards["order"]):            # each kind carried standing and walking, and ridden standing and pushing
        for k in (n, n + 6, n + 12):
            board_bodies.append(dict(look=k, board=kind, carry=True, x=0.4, y=0.12, z=-0.3, yaw=0.5 + 0.3 * n, pose=0, ph=0.0, t=1.4 + k, lod=k % 3, ff=1.0, push=0))
            board_bodies.append(dict(look=k, board=kind, carry=True, x=0.4, y=0.12, z=-0.3, yaw=1.1 + 0.2 * n, pose=1, ph=(0.19 + 0.11 * k) % 1, t=2.2 + k, lod=k % 3, ff=1.0, push=0))
            board_bodies.append(dict(look=k, board=kind, carry=False, x=-0.6, y=0.12, z=0.8, yaw=0.2 + 0.4 * n, pose=5, ph=0.0, t=3.1 + k, lod=k % 3, ff=1.0, push=0))
        for push, ph in ((1, 0.2), (1, 0.7), (0.45, 0.4)):
            board_bodies.append(dict(look=n + 2, board=kind, carry=False, x=-0.6, y=0.12, z=0.8, yaw=2.0 - 0.3 * n, pose=5, ph=ph, t=4.3 + n, lod=0, ff=1.0, push=push))
    designer = json.load(open(os.path.join(RULES, "designer.json"), encoding="utf-8"))
    designs, design_bodies = design_cases(designer), []
    for n in range(len(designs)):
        design_bodies.append(dict(design=n, x=0.0, y=0.0, z=0.0, yaw=0.5 * n, pose=0, ph=0.0, t=1.1 + n, lod=0, ff=1.0))
        design_bodies.append(dict(design=n, x=1.0, y=0.12, z=-1.0, yaw=0.3 + 0.4 * n, pose=1, ph=(0.17 + 0.23 * n) % 1, t=2.3 + n, lod=0, ff=1.0))
        design_bodies.append(dict(design=n, x=1.0, y=0.12, z=-1.0, yaw=0.3 * n, pose=1, ph=(0.41 + 0.13 * n) % 1, t=3.7 + n, lod=1 + n % 3, ff=0.7))
        design_bodies.append(dict(design=n, x=0.0, y=0.0, z=0.0, yaw=0.2 * n, pose=0, ph=0.0, t=1.9 + n, lod=0, ff=1.0, face=face_cases()[n % 6]))
    cases = json.dumps(dict(looks=looks, bodies=bodies, designer=designer, designs=designs, designBodies=design_bodies, boards=boards, boardBodies=board_bodies))
    dressing = open(os.path.join(ROOT, "Prototype", "play", "dressing.js"), encoding="utf-8").read()
    cs = re.search(r"Object\.assign\(\{\}, People\.SIG\.Careful, (\{.*?\})\)\);", dressing, re.S)
    if not cs:
        sys.exit("dressing.js: climbSig() not found; update people_values.py")
    patched = src.replace(ret, HARNESS.replace("%CASES%", cases).replace("%CLIMB%", cs.group(1)) + ret, 1)
    # people.js takes hash() from play.js: the same function, cut from play.js
    play = open(os.path.join(ROOT, "Prototype", "play", "play.js"), encoding="utf-8").read()
    h = re.search(r"^function hash\(\) \{.*?^\}\n", play, re.S | re.M)
    if not h:
        sys.exit("play.js: hash() not found; update people_values.py")
    dialogue = json.load(open(os.path.join(RULES, "dialogue.json"), encoding="utf-8"))
    speech = json.load(open(os.path.join(RULES, "speech.json"), encoding="utf-8"))
    dodge = json.load(open(os.path.join(RULES, "dodge.json"), encoding="utf-8"))
    lines = sorted({n["say"] for c in dialogue["conversations"].values() for n in c["nodes"].values()} |
                   {o["text"] for c in dialogue["conversations"].values() for n in c["nodes"].values() for o in n["options"]})
    lines += ["Well... that's it, isn't it? Yes; quite: really \u2014 fine!", "", "Hmm."]
    conv = open(CONV_JS, encoding="utf-8").read()
    page = ("<!doctype html><meta charset=utf-8><pre id=out></pre><script>\n" + h.group(0) + patched + "\n</script>"
            + "<script>window.WASTED = " + json.dumps(dict(dialogue=dialogue, speech=speech, dodge=dodge, boards=boards)) + ";</script>"
            + "<script>\n" + open(DODGE_JS, encoding="utf-8").read() + "\n</script><script>\n" + DODGE_HARNESS + "\n</script>"
            + "<script>\n" + open(BOARDS_JS, encoding="utf-8").read() + "\n</script><script>\n" + BOARDS_HARNESS + "\n</script>"
            + "<script>\n" + conv + "\n</script><script>\n" + TALK_HARNESS.replace("%LINES%", json.dumps(lines)).replace("%TRAM%", tram_js()) + "\n</script>")
    # Edge is started through PowerShell's Start-Process with its output sent to a file: since Edge 158 the page runs in
    # a process Edge hands it to, which writes nothing to a pipe from here, and Start-Process -Wait waits for it. Edge's
    # helpers can hold the folder a while after, so it is left for the system to clear if it cannot be removed at once.
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as tmp:
        path = os.path.join(tmp, "people_values.html")
        open(path, "w", encoding="utf-8").write(page)
        dump = os.path.join(tmp, "dom.txt")
        args = ",".join("'%s'" % a for a in ("--headless=new", "--user-data-dir=" + os.path.join(tmp, "edge"), "--no-first-run",
                                              "--virtual-time-budget=20000", "--dump-dom", "file:///" + path.replace("\\", "/")))
        subprocess.run(["powershell", "-NoProfile", "-Command",
                        "Start-Process -FilePath '%s' -ArgumentList %s -RedirectStandardOutput '%s' -NoNewWindow -Wait" % (EDGE, args, dump)],
                       stdin=subprocess.DEVNULL, capture_output=True, timeout=300)
        with open(dump, encoding="utf-8", errors="replace") as f:
            text = f.read()
    m = re.search(r"<pre id=\"out\">@@(.*?)@@</pre>", text, re.S)
    if not m:
        sys.exit("Edge gave no values:\n" + text[-2000:])
    out = json.loads(html.unescape(m.group(1)))
    if "error" in out:
        sys.exit("people.js failed in Edge: " + out["error"])
    tables = dict(note="Generated by Tools/unreal/people_values.py from Prototype/play/people.js. Do not edit: change people.js and run it again.",
                  **out["tables"])
    with open(OUT_DATA, "w", encoding="utf-8", newline="\n") as f:
        json.dump(tables, f, indent=1, ensure_ascii=False)
    os.makedirs(os.path.dirname(OUT_VALUES), exist_ok=True)
    with open(OUT_VALUES, "w", encoding="utf-8", newline="\n") as f:
        json.dump(dict(note="Generated by Tools/unreal/people_values.py: people.js look() and draw() for the test ART004_people_are_drawn_as_the_slice_draws_them.",
                       looks=out["looks"], bodies=out["bodies"]), f, separators=(",", ":"))
    print("wrote %s and %s: %d looks, %d bodies" % (os.path.relpath(OUT_DATA, ROOT), os.path.relpath(OUT_VALUES, ROOT), len(out["looks"]), len(out["bodies"])))
    with open(OUT_TALK_VALUES, "w", encoding="utf-8", newline="\n") as f:
        json.dump(dict(note="Generated by Tools/unreal/people_values.py: conversations.js speak() for every line, for CHR009_lines_are_mouthed_as_the_slice_mouths_them; play.js tramAt() through a day, for CITY041_trams_are_where_the_timetable_puts_them.",
                       speak=out["speak"], tram=out["tram"]), f, separators=(",", ":"), ensure_ascii=False)
    with open(OUT_DODGE_VALUES, "w", encoding="utf-8", newline="\n") as f:
        json.dump(dict(note="Generated by Tools/unreal/people_values.py: dodge.js frame() and apply() for a car driven past residents, for CITY070_residents_dodge_and_scold_as_the_slice_does.",
                       **out["dodge"]), f, separators=(",", ":"), ensure_ascii=False)
    print("wrote %s: %d frames" % (os.path.relpath(OUT_DODGE_VALUES, ROOT), len(out["dodge"]["frames"])))
    with open(OUT_BOARD_VALUES, "w", encoding="utf-8", newline="\n") as f:
        json.dump(dict(note="Generated by Tools/unreal/people_values.py: people.js draw() with board() and ride(), and boards.js step(), give(), toggle(), use(), slow(), for CITY099_boards_are_ridden_and_drawn_as_the_slice_does.",
                       bodies=out["boardBodies"], **out["boards"]), f, separators=(",", ":"), ensure_ascii=False)
    print("wrote %s: %d bodies, %d rides" % (os.path.relpath(OUT_BOARD_VALUES, ROOT), len(out["boardBodies"]), len(out["boards"]["rides"])))
    sys.path.insert(0, os.path.join(ROOT, "Tools", "worldgen"))
    import world
    w = world.load()
    checks = [[v, world.look_problems(w["designer"], v)] for v in designs + bad_designs(designer)]
    outfits = [[o, world.outfit_problems(w, o)] for o in outfit_cases(designer, w["settings"]["wardrobe"]["parts"])]
    with open(OUT_DESIGN_VALUES, "w", encoding="utf-8", newline="\n") as f:
        json.dump(dict(note="Generated by Tools/unreal/people_values.py: people.js design() and draw() of designer values (World/rules/designer.json), with world.look_problems() and world.outfit_problems() for them, for CHR001_the_designer_makes_the_slices_look, CHR013_designer_values_stay_in_range and CHR014_an_outfit_is_only_clothes_and_hair_the_designer_offers.",
                       designs=designs, looks=out["designs"], bodies=out["designBodies"], checks=checks, outfits=outfits), f, separators=(",", ":"), ensure_ascii=False)
    print("wrote %s: %d designs, %d bodies, %d checks" % (os.path.relpath(OUT_DESIGN_VALUES, ROOT), len(designs), len(out["designBodies"]), len(checks) + len(outfits)))
    play_src = open(os.path.join(ROOT, "Prototype", "play", "play.js"), encoding="utf-8").read()
    vj = open(VOICES_JS, encoding="utf-8").read()
    voices = json.loads(vj[vj.index("{"):vj.rindex("}") + 1])
    # the index names residents by id (ADR-0060); BrixTalk looks a voice up by the resident's place in the list, -1 for none
    voices["residents"] = [voices["residents"].get(r["id"], -1) for r in json.load(open(os.path.join(ROOT, "World", "city_01", "residents.json"), encoding="utf-8"))["residents"]]
    with open(OUT_TALK, "w", encoding="utf-8", newline="\n") as f:
        json.dump(dict(note="Generated by Tools/unreal/people_values.py from Prototype/play (play.js DOING, GOING; audio/voices/index.js). Do not edit.",
                       doing=js_object(play_src, "DOING"), going=js_object(play_src, "GOING"), player_name="you",
                       voice_dir="Prototype/play/audio/voices", voices=voices), f, separators=(",", ":"), ensure_ascii=False)
    print("wrote %s and %s: %d lines mouthed" % (os.path.relpath(OUT_TALK, ROOT), os.path.relpath(OUT_TALK_VALUES, ROOT), len(out["speak"])))


if __name__ == "__main__":
    main()
