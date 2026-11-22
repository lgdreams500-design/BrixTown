"""Dress city_01's streets, squares and parks (WORLD_MAP WLD-028 to WLD-034; F-MAP-DRESS level 2, the owner's pick of
2026-11-22: "Level 2: used, changing").

Adds props to the block files, each marked "set": "dress" so that running this again first takes away what it added
before and then puts the same things back (running it twice changes nothing):
  parks       a playground in each of the four big parks (swings, slide, seesaw, sandpit, climbing frame), a notice
              board by the path in, and a bandstand in Linden Park
  courts      a centrepiece of its own in each court garden (sundial, well, pergola or small statue)
  squares     a statue before the Town Hall with a notice board; at Market Square a news kiosk, a notice board and
              bunting (shown only on market days), and the fountain made the grand one
  streets     bins, planters, bicycle racks, post boxes, telephone boxes and corner bollards on the pavements, a
              timetable and a bin at every tram stop
  buildings   painted adverts high on blank street walls, timber hoardings on works and depots, window boxes on homes
The places people use (play spots, the notice boards, the kiosk and its keeper) are added to the lots' objects and
tags, children are made from students who live near a park (World/rules/roles.json "child"), and the choice lists in
World/rules/choices.json send them there.

Run from Tools/worldgen:   py add_dressing_01.py
Then:                      py -m unittest test_world ;  py build_play.py
"""
import json, math, os, hashlib

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
RULES = os.path.join(HERE, "..", "..", "World", "rules")
SET = "dress"
PLAY = ("swing", "slide", "seesaw", "sandpit", "climb")          # play spots (lot objects) and the acts that use them
CHILDREN = 60                                                     # students living nearest a big park become children


def rnd(*k):
    return int(hashlib.md5("|".join(map(str, k)).encode()).hexdigest()[:8], 16) / 0xFFFFFFFF


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def jsave(p, d, indent):
    with open(p, "w", encoding="utf-8") as f:
        json.dump(d, f, ensure_ascii=False, indent=indent)


class Room:
    """What already stands in a block, so new things keep clear of it."""
    def __init__(self, b):
        self.pts = [(t["pos"][0], t["pos"][1], 1.4) for t in b["trees"]]
        for p in b["props"]:
            if p["kind"] in ("lantern", "arch"):
                continue
            r = {"fountain": p.get("r", 1.5) * 1.5 + .6, "flowerbed": 1.9, "bench": 1.4, "lamp": .8, "stall": 2.6, "shelter": 7.0}.get(p["kind"], 1.2)
            self.pts.append((p["pos"][0], p["pos"][1], r))
        for l in b["lots"]:
            for k, v in l.get("objects", {}).items():
                for q in v:
                    self.pts.append((q[0], q[1], .7))

    def clear(self, x, z, r):
        return all(math.hypot(x - a, z - b) > r + c for a, b, c in self.pts)

    def take(self, x, z, r):
        self.pts.append((x, z, r))


def add(b, kind, pos, **kw):
    p = dict(kind=kind, pos=[round(pos[0], 2), round(pos[1], 2)], set=SET)
    p.update(kw)
    p["id"] = "%s_d%d" % (b["id"], sum(1 for q in b["props"] if q.get("set") == SET))
    b["props"].append(p)
    return p


# ---------------------------------------------------------------- parks
def playground(b, lot, room):
    """Swings, slide, seesaw, sandpit and climbing frame on soft ground, in whichever quarter of the park is clear."""
    x0, z0, x1, z1 = lot["rect"]
    cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
    for ox, oz in ((-13, 13), (13, 13), (-13, -12), (13, -12), (0, 15), (0, -14), (-15, 0), (15, 0)):
        px, pz = cx + ox, cz + oz
        if all(room.clear(px + u, pz + v, .6) for u in (-7, -3.5, 0, 3.5, 7) for v in (-5, -2.5, 0, 2.5, 5)):
            break
    else:
        raise SystemExit("%s: no clear quarter for a playground" % lot["id"])
    spots = {k: [] for k in PLAY}
    add(b, "playground", (px, pz), w=16.0, d=12.0)                                  # the soft ground under it all
    add(b, "swing", (px - 4.5, pz - 2.8), seats=4, along="x")
    for k in range(4):
        spots["swing"].append([px - 4.5 + (k - 1.5) * 1.1, pz - 2.8])
    add(b, "slide", (px + 4.5, pz - 2.8), along="x")                                 # ladder at -x, chute down to +x; the rest queue
    spots["slide"] += [[px + 2.9, pz - 2.8], [px + 2.4, pz - 1.9], [px + 2.0, pz - 1.4], [px + 3.4, pz - 1.7]]
    for v in (1.9, 4.1):
        add(b, "seesaw", (px - 4.5, pz + v), along="x")
        spots["seesaw"] += [[px - 6.2, pz + v], [px - 2.8, pz + v]]
    add(b, "sandpit", (px + 4.5, pz + 2.8), w=4.0)
    spots["sandpit"] += [[px + 3.5, pz + 1.9], [px + 5.5, pz + 1.9], [px + 3.5, pz + 3.7], [px + 5.5, pz + 3.7], [px + 4.5, pz + 1.7], [px + 4.5, pz + 3.9]]
    add(b, "climber", (px, pz - 2.8))
    spots["climb"] += [[px - .55, pz - 3.35], [px + .55, pz - 2.25], [px - .55, pz - 2.25], [px + .55, pz - 3.35]]
    add(b, "bench", (px, pz + 5.4), along="x")                                       # for the grown-ups watching
    for u in (-7, -3.5, 0, 3.5, 7):
        for v in (-5, -2.5, 0, 2.5, 5):
            room.take(px + u, pz + v, 1.2)
    for k, v in spots.items():
        lot["objects"][k] = [[round(a, 2), round(c, 2)] for a, c in v]
    return px, pz


def notice_board(b, lot, room, near):
    """A board on two posts, and a place to stand and read it."""
    x, z = near
    for dx, dz in ((0, 0), (3, 0), (-3, 0), (0, 3), (0, -3), (5, 2), (-5, 2)):
        if room.clear(x + dx, z + dz, 1.0):
            x, z = x + dx, z + dz
            break
    cx, cz = (lot["rect"][0] + lot["rect"][2]) / 2, (lot["rect"][1] + lot["rect"][3]) / 2
    fx, fz = (cx - x), (cz - z)
    n = math.hypot(fx, fz) or 1
    fx, fz = fx / n, fz / n
    face = "x" if abs(fx) > abs(fz) else "z"
    sgn = (1 if (fx if face == "x" else fz) > 0 else -1)
    add(b, "noticeboard", (x, z), face=face, sign=sgn)
    room.take(x, z, 1.2)
    for u in (-.7, 0, .7):                                                        # three can read it at once
        rx, rz = (x + sgn * 1.0, z + u) if face == "x" else (x + u, z + sgn * 1.0)
        lot["objects"]["notice"] = lot["objects"].get("notice", []) + [[round(rx, 2), round(rz, 2)]]


def bandstand(b, lot, room):
    x0, z0, x1, z1 = lot["rect"]
    cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
    for ox, oz in ((13, -12), (-13, -12), (13, 13), (-13, 13), (15, 0), (-15, 0)):
        if all(room.clear(cx + ox + u, cz + oz + v, .6) for u in (-4, 0, 4) for v in (-4, 0, 4)):
            add(b, "bandstand", (cx + ox, cz + oz), r=3.6)
            room.take(cx + ox, cz + oz, 4.6)
            return
    raise SystemExit("%s: no room for the bandstand" % lot["id"])


# ---------------------------------------------------------------- courts and squares
def court_piece(b, lot, room):
    x0, z0, x1, z1 = lot["rect"]
    cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
    kind = ("sundial", "well", "pergola", "statue")[int(rnd(lot["id"], "piece") * 4)]
    for ox, oz in ((0, 8), (0, -8), (8, 0), (-8, 0), (7, 7), (-7, -7), (7, -7), (-7, 7)):
        if room.clear(cx + ox, cz + oz, 1.8 if kind == "pergola" else 1.2):
            extra = dict(small=True) if kind == "statue" else {}
            add(b, kind, (cx + ox, cz + oz), **extra)
            room.take(cx + ox, cz + oz, 2.0)
            return


# ---------------------------------------------------------------- streets
def street_side(b, side, room, doors, food, lamps, stops):
    """Bins, planters, racks, post boxes and the odd telephone box along one pavement of a block."""
    ox, oz = b["origin"]
    S = b["size"]
    off = 3.75                                                   # from the block edge into the street: the kerb half of the pavement
    def at(t):
        return {"n": (ox + t, oz - off), "s": (ox + t, oz + S + off), "w": (ox - off, oz + t), "e": (ox + S + off, oz + t)}[side]
    along = "x" if side in "ns" else "z"
    wants = ["bin"] + [k for k, p in (("planter", .65), ("bikerack", .35), ("postbox", .22), ("planter", .3), ("phonebox", .07)) if rnd(b["id"], side, k, p) < p]
    placed = []
    t = 9.0 + 6 * rnd(b["id"], side, "t0")
    for kind in wants:
        while t < S - 9:
            x, z = at(t)
            ok = all(math.hypot(x - a, z - c) > 3.0 for a, c in lamps) and all(math.hypot(x - a, z - c) > 3.2 for a, c in doors) \
                and all(math.hypot(x - a, z - c) > 9.0 for a, c in stops) and not any(ax == along and a0 - 2 <= (x if along == "x" else z) <= a1 + 2 and abs((z if along == "x" else x) - f) < 7 for ax, a0, a1, f in food) \
                and all(abs(t - q) > 5 for q in placed)
            if ok:
                add(b, kind, (x, z), along=along, out={"n": [0, -1], "s": [0, 1], "w": [-1, 0], "e": [1, 0]}[side])
                placed.append(t)
                t += 7 + 9 * rnd(b["id"], side, kind, t)
                break
            t += 1.5


def corners(b):
    ox, oz = b["origin"]
    S = b["size"]
    for cx, cz, sx, sz in ((ox, oz, -1, -1), (ox + S, oz, 1, -1), (ox, oz + S, -1, 1), (ox + S, oz + S, 1, 1)):
        for u, v in ((3.9, 1.3), (1.3, 3.9)):
            add(b, "bollard", (cx + sx * u, cz + sz * v))


# ---------------------------------------------------------------- buildings
def wall_sides(lot, b):
    """The lot's walls that stand on the street (the block's edge) and have no door."""
    x0, z0, x1, z1 = lot["rect"]
    ox, oz = b["origin"]
    S = b["size"]
    dx, dz = lot["door"]
    out = []
    for side, on, length, mid in (("n", abs(z0 - oz) < .5, x1 - x0, ((x0 + x1) / 2, z0)), ("s", abs(z1 - oz - S) < .5, x1 - x0, ((x0 + x1) / 2, z1)),
                                  ("w", abs(x0 - ox) < .5, z1 - z0, (x0, (z0 + z1) / 2)), ("e", abs(x1 - ox - S) < .5, z1 - z0, (x1, (z0 + z1) / 2))):
        door_here = (side in "ns" and abs(dz - mid[1]) < .5) or (side in "we" and abs(dx - mid[0]) < .5)
        if on and not door_here:
            out.append((side, length, mid))
    return out


def building_dress(b):
    n_adv = 0
    for l in b["lots"]:
        if l["kind"] in ("garden", "market", "stop") or l.get("tower"):
            continue
        floors = l.get("floors", 2)
        for side, length, mid in wall_sides(l, b):
            o = {"n": [0, -1], "s": [0, 1], "w": [-1, 0], "e": [1, 0]}[side]
            if (l["kind"] == "utility" or (l["kind"] == "work" and "Works" in l["name"])) and length >= 10:
                add(b, "hoarding", (mid[0] + o[0] * .02, mid[1] + o[1] * .02), out=o, w=round(min(12.0, length - 3), 1), lot=l["id"])
            elif floors >= 2 and length >= 9 and l["kind"] in ("home", "shop", "food", "work") and rnd(l["id"], side, "adv") < .3 and n_adv < 2:
                add(b, "advert", (mid[0] + o[0] * .02, mid[1] + o[1] * .02), out=o, w=round(min(6.0, length - 3.5), 1), lot=l["id"])
                n_adv += 1
        if l["kind"] == "home":
            if rnd(l["id"], "boxes") < .45:
                l["window_boxes"] = True
            else:
                l.pop("window_boxes", None)


# ---------------------------------------------------------------- people and choices
def children_and_choices(lots, blocks, people):
    parks = [l for l in lots.values() if "playground" in l.get("tags", [])]
    boards = [l for l in lots.values() if "noticeboard" in l.get("tags", [])]
    kiosk = [l for l in lots.values() if "kiosk" in l.get("tags", [])]
    ctr = lambda l: ((l["rect"][0] + l["rect"][2]) / 2, (l["rect"][1] + l["rect"][3]) / 2)
    near = lambda r, ls: min(ls, key=lambda l: (math.dist(ctr(lots[r["home"]]), ctr(l)), l["id"]))["id"]
    for r in people:
        r.setdefault("near", {})
        r["near"]["playground"] = near(r, parks)
        r["near"]["noticeboard"] = near(r, boards)
        r["near"]["kiosk"] = kiosk[0]["id"]
    # children: the students living nearest a big park (once chosen, kept: they are found again by their mark)
    kids = [r for r in people if r.get("was") == "student"]
    if not kids:                                                  # the same number round each park: those living nearest it
        for park in parks:
            st = sorted((r for r in people if r["role"] == "student" and r["near"]["playground"] == park["id"]), key=lambda r: (math.dist(ctr(lots[r["home"]]), ctr(park)), r["id"]))
            kids += st[:CHILDREN // len(parks)]
    for r in kids:
        r["role"], r["was"] = "child", "student"
        r.pop("work", None)
        r["work"] = None
    # the kiosk keeper: the student living nearest Market Square, now its shopkeeper
    keep = [r for r in people if r.get("was") == "student, kiosk"]
    if not keep:
        st = sorted((r for r in people if r["role"] == "student"), key=lambda r: (math.dist(ctr(lots[r["home"]]), ctr(kiosk[0])), r["id"]))
        keep = st[:1]
    for r in keep:
        r["role"], r["was"], r["work"] = "shopkeeper", "student, kiosk", kiosk[0]["id"]
    return len(kids), keep[0]["id"]


CHILD = {
    "color": [0.85, 0.42, 0.32], "sig": "Springy", "jitter": 10,
    "weekday": [["00:00", "sleep", "home"], ["07:10", "home", "home"], ["07:50", "away", "stop"], ["15:30", "play", "play"], ["16:10", "play", "play"],
                ["16:50", "play", "play"], ["17:30", "play", "play"], ["18:15", "home", "home"], ["20:30", "sleep", "home"]],
    "weekend": [["00:00", "sleep", "home"], ["08:30", "home", "home"], ["09:40", "play", "play"], ["10:20", "play", "play"], ["11:00", "play", "play"],
                ["11:40", "play", "play"], ["12:30", "home", "home"], ["14:00", "play", "play"], ["14:40", "play", "play"], ["15:20", "play", "play"],
                ["16:00", "play", "play"], ["17:00", "home", "home"], ["20:30", "sleep", "home"]],
}
PLAY_CHOICES = {"default": [[k, "playground"] for k in PLAY], "rain": [["home", "home"]]}
USE = {"swing": "swing", "slide": "slide", "seesaw": "seesaw", "sandpit": "sandpit", "climb": "climb", "notices": "notice", "paper": "kiosk"}
EXTRA = {("shopping", "default"): [["paper", "kiosk"], ["paper", "kiosk"]], ("afternoon", "default"): [["notices", "noticeboard"]]}


def rules(nkids):
    rp = os.path.join(RULES, "roles.json")
    ro = jload(rp)
    ro["roles"]["child"] = CHILD
    jsave(rp, ro, 1)
    cp = os.path.join(RULES, "choices.json")
    ch = jload(cp)
    ch["choices"]["play"] = PLAY_CHOICES
    for (name, case), opts in EXTRA.items():
        ch["choices"][name][case] = [o for o in ch["choices"][name][case] if o[0] not in ("notices", "paper")] + opts
    ch["choices"]["leisure"]["default"] = [o for o in ch["choices"]["leisure"]["default"] if o[0] != "notices"]
    ch["use"].update(USE)
    if "'playground'" not in ch["note"]:
        ch["note"] = ch["note"].rstrip(".") + ". 'playground', 'noticeboard', 'kiosk' likewise, and 'play' is the children's list (Tools/worldgen/add_dressing_01.py)."
    jsave(cp, ch, 1)
    dp = os.path.join(RULES, "dialogue.json")                   # the word for the role (kept in the file's own layout)
    with open(dp, encoding="utf-8") as f:
        s = f.read()
    if '"child":' not in s:
        s = s.replace('  "utility": "utility worker"\n', '  "utility": "utility worker",\n  "child": "schoolchild"\n', 1)
        with open(dp, "w", encoding="utf-8") as f:
            f.write(s)


def main():
    city = jload(os.path.join(CITY, "city.json"))
    res_doc = jload(os.path.join(CITY, "residents.json"))
    people = res_doc["residents"]
    blocks = {}
    for e in city["blocks"]:
        p = os.path.join(CITY, e["file"])
        blocks[e["id"]] = (p, jload(p))
    for _, b in blocks.values():                                  # take away what an earlier run added
        b["props"] = [p for p in b["props"] if p.get("set") != SET]
        for l in b["lots"]:
            for k in PLAY + ("notice", "kiosk"):
                l.get("objects", {}).pop(k, None)
            if l["kind"] == "market":
                l["objects"].pop("work", None)
            l["tags"] = [t for t in l.get("tags", []) if t not in ("playground", "noticeboard", "kiosk")]
            if not l["tags"]:
                l.pop("tags")
        for p in b["props"]:
            p.pop("grand", None)
    lots = {l["id"]: l for _, b in blocks.values() for l in b["lots"]}
    n = dict(play=0, court=0, street=0, building=0)
    for _, b in blocks.values():
        room = Room(b)
        doors = [tuple(l["door"]) for l in b["lots"]]
        food = []
        for l in b["lots"]:
            if l["kind"] == "food":
                x0, z0, x1, z1 = l["rect"]
                dx, dz = l["door"]
                food.append(("x", x0, x1, dz) if abs(dz - z0) < .5 or abs(dz - z1) < .5 else ("z", z0, z1, dx))   # café tables stand out on that pavement
        lamps = [tuple(p["pos"]) for p in b["props"] if p["kind"] == "lamp"]
        stops = [tuple(p["pos"]) for p in b["props"] if p["kind"] == "shelter"]
        if b["template"] == "park":
            lot = next(l for l in b["lots"] if l["kind"] == "garden" and l["name"].endswith("Park"))
            playground(b, lot, room)
            x0, z0, x1, z1 = lot["rect"]
            notice_board(b, lot, room, ((x0 + x1) / 2 + 11, z1 - 2.2))
            if lot["name"] == "Linden Park":
                bandstand(b, lot, room)
            lot["tags"] = lot.get("tags", []) + ["playground", "noticeboard"]
            n["play"] += 1
        for l in b["lots"]:
            if l["kind"] == "garden" and not l["name"].endswith("Park"):
                court_piece(b, l, room)
                n["court"] += 1
        if b["template"] == "market":
            lot = next(l for l in b["lots"] if l["kind"] == "market")
            x0, z0, x1, z1 = lot["rect"]
            cz = (z0 + z1) / 2
            add(b, "kiosk", (x1 - 3.2, cz), face=-1)                         # the counter faces into the square
            lot["objects"]["kiosk"] = [[round(x1 - 4.9, 2), round(cz - .6, 2)], [round(x1 - 4.9, 2), round(cz + .6, 2)]]
            lot["objects"]["work"] = [[round(x1 - 2.9, 2), round(cz, 2)]]
            notice_board(b, lot, room, (x0 + 2.0, cz))
            lot["tags"] = lot.get("tags", []) + ["kiosk", "noticeboard"]
            fountain = next(p for p in b["props"] if p["kind"] == "fountain")
            fountain["grand"] = True
            fx, fz = fountain["pos"]
            for cx_, cz_ in ((x0 + .6, z0 + .6), (x1 - .6, z0 + .6), (x0 + .6, z1 - .6), (x1 - .6, z1 - .6)):
                add(b, "bunting", (cx_, cz_), to=[fx, fz], h0=4.6, h1=4.2, poles=True)
        if b["template"] == "hall":
            hall = next(l for l in b["lots"] if l["name"] == "Town Hall")
            x0, z0, x1, z1 = hall["rect"]
            sx, sz = (x0 + x1) / 2, z1 + 18.0
            add(b, "statue", (sx, sz))
            room.take(sx, sz, 2.4)
            for px in (sx - 5.5, sx + 5.5):
                add(b, "planter", (px, z1 + 4.0), along="x", big=True)
            add(b, "noticeboard", (sx - 9.0, z1 + 3.2), face="z", sign=1)
        for side in "nswe":
            street_side(b, side, room, doors, food, lamps, stops)
        corners(b)
        for p in stops:
            add(b, "timetable", (p[0] + 7.3, p[1]), out=[0, 1])
            add(b, "bin", (p[0] - 7.3, p[1]), along="x", out=[0, 1])
        building_dress(b)
        n["street"] += 1
    kids, keeper = children_and_choices(lots, blocks, people)
    for p, b in blocks.values():
        jsave(p, b, 1)
    jsave(os.path.join(CITY, "residents.json"), res_doc, 0)
    rules(kids)
    count = sum(1 for _, b in blocks.values() for p in b["props"] if p.get("set") == SET)
    print("%d props: %d playgrounds, %d court pieces; %d children; kiosk keeper %s" % (count, n["play"], n["court"], kids, keeper))


if __name__ == "__main__":
    main()
