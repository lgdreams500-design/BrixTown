"""Eastgate Mall's shell (WLD-057, ADR-0035). The owner, 2026-11-24, with a photograph of a mall of flowing white bands
and big golden pierced domes: "there are 3 malls in this world and they ... are big rectangle boxes. would you be able
design one of these like attached pic?"

The hall itself does not change: its rect, door, floors and plan (malls_01.py) stay, so staff, customers, paths and the
car park are where they were. Round it goes a skin, written into the lot as `shell`:

  style        "ribbon"
  skin         points [[x, z, bottom, top], ...]: a closed line round the hall, about every 3 m (closer at the feet of an
               arch). The skin stands on that line from `bottom` to `top` metres; where bottom is above 0 it is lifted
               off the ground as an arch, and the hall's own front shows under it. roll: the top curls in towards the
               hall over this many metres; belly: how far the wall swells out half way up; bands: ribbons counted up the
               wall; colors: their two close tones, by palette name (ART_DIRECTION 2.3)
  domes        [{pos, centre, radii, cells, color, over}]: an egg over each corner store: its middle on the map, the height
               of its middle, its half-sizes [east-west, up, north-south], how many openings run round it, its colour,
               and the id of the unit (plan) under it. The skin dips round each egg and the egg stands out of it.
  roof_light   {pos, base, radii, cells, color}: the half egg over the open court, standing on the roof at `base`
  arches       [{side, at, width, height}]: where the skin is lifted (already in the points; listed so the hall's front
               under each can be glazed and lit)

Every size here is a first guess [ASSUMPTION]. Only the line and the heights are worked out here; how they are drawn
(matte mottled plaster and ochre, joints that glow at dusk) is the slice's, Prototype/play/shell.js.

Run:   python mall_shell_01.py        (works the shell out again and writes it over the old one)
Then:  python -m unittest test_world  and  python build_play.py
"""
import json, math, os

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.normpath(os.path.join(HERE, "..", "..", "World", "city_01"))
BLOCKS = os.path.join(CITY, "blocks")

STEP = 3.0                # metres between points of the skin's line
MARGIN = 2.0              # the skin stands this far outside an egg's widest line
CELL = 2.6                # metres from one opening of an egg to the next, at its widest
PLAN = {
    "b_16_5": dict(
        grow=(11.5, 13.5), power=8,                         # the line without the eggs: a rounded box this much bigger than the hall
        wave=(2.5, 7, 0.9),                                 # the wall wanders out by up to this many metres, this many times round
        keep=(1.5, 1.5, 1.5, 14.0),                         # metres kept clear: inside the block's west, north and east edges; the skin's reach south of the hall (the forecourt lies beyond)
        base=(22.3, 2.2, 5, 0.6, 1.1, 13, 2.1),             # the top: mean height, then two waves (height, times round, where it starts)
        crest=4.5, crest_at=15.0, lift=2.5,                 # the top rises beside each egg (by this much, this far from it), and over each arch
        dip=1.5, dip_reach=9.5,                             # round an egg the top dips to this much above the egg's middle; back to full height this far from it
        roll=6.0, belly=1.2, bands=18, colors=("cream", "plaster"),
        # the eggs: which corner, how far the middle is in from the hall's two walls there (x, z), half-sizes (x, up, z), height of the middle
        domes=[dict(corner="sw", inset=(24.5, 31.5), radii=(38.0, 31.0, 45.0), centre=11.0, over="Eastgate Food Market"),
               dict(corner="se", inset=(23.0, 23.0), radii=(36.0, 27.0, 36.0), centre=9.0, over="Eastgate Cinema"),
               dict(corner="nw", inset=(21.0, 26.0), radii=(34.0, 26.0, 40.0), centre=9.0, over="Eastgate Department Store"),
               dict(corner="ne", inset=(18.0, 18.0), radii=(30.0, 22.0, 30.0), centre=8.0, over="Eastgate Fashion House")],
        dome_color="ochre",
        arches=[dict(side="s", width=44.0, height=12.5),    # the grand arch, at the door
                dict(side="w", width=36.0, height=10.5), dict(side="e", width=30.0, height=9.5), dict(side="n", width=26.0, height=8.5)],
        roof_light=dict(radii=(44.0, 15.0, 94.0), color="cream", cell=3.0)),
}


def r2(v):
    return round(v, 2)


def smoothstep(a, b, v):
    t = max(0.0, min(1.0, (v - a) / (b - a)))
    return t * t * (3 - 2 * t)


def nearest_on(rect, x, z):
    """The point of the hall's wall nearest a point outside it."""
    return max(rect[0], min(rect[2], x)), max(rect[1], min(rect[3], z))


def inside_dome(d, x, y, z):
    """Above 0 inside the egg, 1 at its middle, below 0 outside."""
    return 1 - ((x - d["pos"][0]) / d["radii"][0]) ** 2 - ((y - d["centre"]) / d["radii"][1]) ** 2 - ((z - d["pos"][1]) / d["radii"][2]) ** 2


def off_dome(d, x, z):
    """About how many metres a point on the ground is outside the egg's widest line."""
    return (math.hypot((x - d["pos"][0]) / d["radii"][0], (z - d["pos"][1]) / d["radii"][2]) - 1) * min(d["radii"][0], d["radii"][2])


def shell(block, lot, p, pitch):
    x0, z0, x1, z1 = lot["rect"]
    cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
    H = lot["floors"] * lot["plan"]["storey_height"]
    bx, bz = block["origin"]
    ex = block["size"] + (block["span"][0] - 1) * pitch
    lim = (bx + p["keep"][0], bz + p["keep"][1], bx + ex - p["keep"][2], z1 + p["keep"][3])
    units = {u["name"]: u["id"] for u in lot["plan"]["levels"][0]["units"]}

    domes = []
    for d in p["domes"]:
        ix, iz = d["inset"]
        pos = [x0 + ix if "w" in d["corner"] else x1 - ix, z0 + iz if "n" in d["corner"] else z1 - iz]
        rx, ry, rz = d["radii"]
        domes.append(dict(pos=pos, centre=d["centre"], radii=[rx, ry, rz], cells=round(math.pi * (rx + rz) / CELL),
                          color=p["dome_color"], over=units[d["over"]]))

    # the line, as a distance from the hall's middle in each direction: the rounded box or an egg's widest line, whichever
    # is further out; widened and averaged so it is smooth and never inside either; held inside the block
    N = 2880
    a, b, n = (x1 - x0) / 2 + p["grow"][0], (z1 - z0) / 2 + p["grow"][1], p["power"]
    raw = []
    for k in range(N):
        th = 2 * math.pi * k / N
        c, s = math.cos(th), math.sin(th)
        r = (abs(c / a) ** n + abs(s / b) ** n) ** (-1.0 / n)
        r += p["wave"][0] * 0.5 * (1 + math.sin(p["wave"][1] * th + p["wave"][2]))
        for d in domes:                                     # where the ray leaves the egg's widest line, MARGIN outside it
            RX, RZ = d["radii"][0] + MARGIN, d["radii"][2] + MARGIN
            ox, oz = (cx - d["pos"][0]) / RX, (cz - d["pos"][1]) / RZ
            ux, uz = c / RX, s / RZ
            A, B, C = ux * ux + uz * uz, 2 * (ox * ux + oz * uz), ox * ox + oz * oz - 1
            disc = B * B - 4 * A * C
            if disc > 0:
                r = max(r, (-B + math.sqrt(disc)) / (2 * A))
        raw.append(r)
    w = N // 144                                            # 2.5 degrees either side
    wide = [max(raw[(k + j) % N] for j in range(-w, w + 1)) for k in range(N)]
    line = []
    for k in range(N):
        th = 2 * math.pi * k / N
        c, s = math.cos(th), math.sin(th)
        r = sum(wide[(k + j) % N] for j in range(-w, w + 1)) / (2 * w + 1)
        for v, lo, hi, o in ((c, lim[0], lim[2], cx), (s, lim[1], lim[3], cz)):
            if v > 1e-9:
                r = min(r, (hi - o) / v)
            elif v < -1e-9:
                r = min(r, (lo - o) / v)
        line.append((cx + r * c, cz + r * s))
    acc = [0.0]
    for k in range(N):
        acc.append(acc[-1] + math.dist(line[k], line[(k + 1) % N]))
    L = acc[-1]

    def at(s):
        """The point s metres along the line (it starts due east of the middle and runs east, south, west, north)."""
        s %= L
        lo, hi = 0, N
        while hi - lo > 1:
            mid = (lo + hi) // 2
            lo, hi = (mid, hi) if acc[mid] <= s else (lo, mid)
        f = (s - acc[lo]) / max(acc[lo + 1] - acc[lo], 1e-9)
        p0, p1 = line[lo], line[(lo + 1) % N]
        return p0[0] + (p1[0] - p0[0]) * f, p0[1] + (p1[1] - p0[1]) * f

    # the arches: each where the line passes the middle of its side of the hall (the south one at the door)
    arches = []
    for a_ in p["arches"]:
        target = {"s": (lot["door"][0], z1), "n": (cx, z0), "w": (x0, cz), "e": (x1, cz)}[a_["side"]]
        if a_["side"] == "s":
            assert abs(lot["door"][1] - z1) < 0.01, "the door is not on the south wall"
        best = min(range(0, int(L * 4)), key=lambda k: math.dist(nearest_on(lot["rect"], *at(k / 4)), target) + 0.001 * math.dist(at(k / 4), target))
        arches.append(dict(a_, at=[r2(target[0]), r2(target[1])], s=best / 4))

    count = round(L / STEP)
    ss = [L * k / count for k in range(count)]
    for a_ in arches:                                       # closer points at the feet of each arch, where it rises steeply
        for end in (-1, 1):
            foot = a_["s"] + end * a_["width"] / 2
            ss += [(foot - end * o) % L for o in (0.0, 0.12, 0.35, 0.7, 1.2, 1.9, 2.8, 4.0, 5.5)]
    ss.sort()
    kept = []
    for s in ss:
        if not kept or s - kept[-1] > 0.1:
            kept.append(s)

    points = []
    for s in kept:
        x, z = at(s)
        qx, qz = nearest_on(lot["rect"], x, z)
        off, dip = min((off_dome(d, x, z), d["centre"] + p["dip"]) for d in domes)   # metres from the nearest egg, and how low the skin dips for it
        m, h1, n1, p1, h2, n2, p2 = p["base"]
        top = m + h1 * math.sin(2 * math.pi * n1 * s / L + p1) + h2 * math.sin(2 * math.pi * n2 * s / L + p2)
        top += p["crest"] * math.exp(-((off - p["crest_at"]) / 6.0) ** 2)
        bottom = 0.0
        for a_ in arches:
            ds = (s - a_["s"] + L / 2) % L - L / 2
            u = 2 * ds / a_["width"]
            if abs(u) < 1:
                up = math.sqrt(1 - u * u)                   # a round arch
                bottom = max(bottom, a_["height"] * up)
                top += p["lift"] * up
        top = dip + (top - dip) * smoothstep(1.0, p["dip_reach"], off)   # round an egg the skin is low, and the egg stands out of it
        points.append([r2(x), r2(z), r2(bottom), r2(top)])

    rl = p["roof_light"]
    return dict(style="ribbon",
                skin=dict(points=points, roll=p["roll"], belly=p["belly"], bands=p["bands"], colors=list(p["colors"])),
                domes=domes,
                roof_light=dict(pos=[cx, cz], base=H, radii=list(rl["radii"]), cells=round(math.pi * (rl["radii"][0] + rl["radii"][2]) / rl["cell"]),
                                color=rl["color"]),
                arches=[dict(side=a_["side"], at=a_["at"], width=a_["width"], height=a_["height"]) for a_ in arches])


def run(bid):
    path = os.path.join(BLOCKS, bid + ".json")
    with open(path, encoding="utf-8") as f:
        b = json.load(f)
    lot = next(l for l in b["lots"] if "mall" in l.get("tags", []) and l.get("plan"))
    with open(os.path.join(CITY, "city.json"), encoding="utf-8") as f:
        pitch = json.load(f)["grid"]["pitch"]
    lot["shell"] = sh = shell(b, lot, PLAN[bid], pitch)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(b, f, ensure_ascii=False, indent=1)
    pts = sh["skin"]["points"]
    return "%s: a skin of %d points, top %.1f to %.1f m, %d eggs up to %.0f m, %d arches" % (
        lot["name"], len(pts), min(q[3] for q in pts), max(q[3] for q in pts), len(sh["domes"]),
        max(d["centre"] + d["radii"][1] for d in sh["domes"]), len(sh["arches"]))


if __name__ == "__main__":
    for bid in sorted(PLAN):
        print(run(bid))
