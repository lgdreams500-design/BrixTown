"""Floor plans for the builds page (draw_builds.py): one SVG per storey of a build type's sample lot, front at the bottom.

A designed house is drawn from its plan in World/rules/home_plans.json (rooms and every piece of furniture at the Full
level, sized as furniture_sizes_21.py measured them); any other lot from its own data: its outline, flats, mall units,
stair, lift, front door and the places people work, sit, shop and sleep.
"""
import html, json, math, os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
PLANS = json.load(open(os.path.join(ROOT, "World", "rules", "home_plans.json"), encoding="utf-8"))["plans"]
SIZES_P = os.path.join(ROOT, "Art", "StyleTests", "house_interiors_21", "furniture_sizes.json")
SIZES = json.load(open(SIZES_P, encoding="utf-8")) if os.path.exists(SIZES_P) else {}
e = lambda s: html.escape(str(s), quote=True)
DOT = {"work": "#7e57c2", "seat": "#ef8f3a", "customer": "#3d8bd4", "bed": "#8d5a3b", "chair": "#ef8f3a", "stall": "#c9a227",
       "bench": "#6d4c41", "wait": "#3d8bd4"}
ROOM_FILL = "#f4ecdf"
WALL = {"k_wall", "b_mirror", "h_mirror", "h_hooks", "s_pinboard", "o_painting", "o_clock", "o_curtains", "o_wallshelf", "d_pendant"}


def svg(w_m, d_m, body, scale, label):
    pad = 6
    W, H = w_m * scale + 2 * pad, d_m * scale + 2 * pad + 16
    bar = 2 if w_m < 15 else 10 if w_m < 80 else 50
    bar_line = (f'<line x1="{pad}" y1="{H - 5:.0f}" x2="{pad + bar * scale:.1f}" y2="{H - 5:.0f}" stroke="#3b332c" stroke-width="2"/>'
                f'<text x="{pad + bar * scale + 4:.1f}" y="{H - 2:.0f}" font-size="9" fill="#3b332c">{bar} m</text>')
    return (f'<figure><svg viewBox="0 0 {W:.0f} {H:.0f}" width="{W:.0f}" height="{H:.0f}" role="img" aria-label="{e(label)}">'
            f'<g transform="translate({pad},{pad})">{body}</g>{bar_line}</svg><figcaption>{e(label)}</figcaption></figure>')


def house_plan(plan):
    """The two storeys of a home plan, from home_plans.json (x across the front, y from the front at -4.5 to the back)."""
    hp = plan["house"]
    W0, W1, D0, D1 = hp["W0"], hp["W1"], hp["D0"], hp["D1"]
    sc = 210 / (W1 - W0)
    P = lambda x, y: ((x - W0) * sc, (D1 - y) * sc)
    out = []
    for storey, label in (("ground", "Ground floor, Full"), ("upstairs", "Upstairs, Full")):
        b = [f'<rect x="0" y="0" width="{(W1 - W0) * sc:.1f}" height="{(D1 - D0) * sc:.1f}" fill="#5b5048"/>']
        for name, (x0, x1, y0, y1) in hp["rooms"][storey].items():
            (a, c), (a2, c2) = P(x0, y1), P(x1, y0)
            b.append(f'<rect x="{a:.1f}" y="{c:.1f}" width="{a2 - a:.1f}" height="{c2 - c:.1f}" fill="{ROOM_FILL}"><title>{e(name)}</title></rect>')
            b.append(f'<text x="{a + 2:.1f}" y="{c + 7:.1f}" font-size="6" fill="#9a8c7c">{e(name)}</text>')
        sx0, sx1, sy0, sy1 = hp["stair"]
        (a, c), (a2, c2) = P(sx0, sy1), P(sx1, sy0)
        b.append(f'<rect x="{a:.1f}" y="{c:.1f}" width="{a2 - a:.1f}" height="{c2 - c:.1f}" fill="#c9a77e"/>')
        for it in plan["items"]:
            if it["storey"] != storey or it["piece"] in WALL or it["piece"] not in SIZES:
                continue
            bs = [v for v in SIZES[it["piece"]].values() if v]
            x0, x1, y0, y1 = min(v[0] for v in bs), max(v[1] for v in bs), min(v[2] for v in bs), max(v[3] for v in bs)
            c_, s_ = math.cos(it["yaw"]), math.sin(it["yaw"])
            pts = [P(it["x"] + u * c_ - v * s_, it["y"] + u * s_ + v * c_) for u, v in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))]
            rug = it["piece"] == "k17:rug"
            b.append('<polygon points="%s" fill="%s" stroke="%s" stroke-width=".6"><title>%s (level %d)</title></polygon>' % (
                " ".join("%.1f,%.1f" % p for p in pts), "#e6d6bd" if rug else "#b88c62", "none" if rug else "#6b4a30", e(it["piece"].replace("k17:", "")), it["level"]))
        if storey == "ground":
            (a, c) = P((hp["front_door"][0] + hp["front_door"][1]) / 2, D0)
            b.append(f'<circle cx="{a:.1f}" cy="{c:.1f}" r="3" fill="#c0392b" stroke="#fff"/>')
        out.append(svg(W1 - W0, D1 - D0, "".join(b), sc, label))
    return out


def side_of(l):
    x0, z0, x1, z1 = l["rect"]; dx, dz = l["door"]
    return min((abs(dz - z0), "n"), (abs(dz - z1), "s"), (abs(dx - x0), "w"), (abs(dx - x1), "e"))[1]


def lot_plan(l):
    """A lot's storeys from its own data, front (the door's side) at the bottom."""
    x0, z0, x1, z1 = l["rect"]
    sd = side_of(l)
    if sd in "ns":
        W, D = x1 - x0, z1 - z0
        loc = (lambda x, z: (x1 - x, z - z0)) if sd == "n" else (lambda x, z: (x - x0, z1 - z))
    else:
        W, D = z1 - z0, x1 - x0
        loc = (lambda x, z: (z - z0, x - x0)) if sd == "w" else (lambda x, z: (z1 - z, x1 - x))
    big = max(W, D)
    sc = min(210 / W, 150 / D) if big < 60 else min(300 / W, 200 / D)
    P = lambda x, z: (loc(x, z)[0] * sc, (D - loc(x, z)[1]) * sc)

    def rect(r, fill, stroke="none", sw=0.6, title=""):
        pts = [P(r[0], r[1]), P(r[2], r[1]), P(r[2], r[3]), P(r[0], r[3])]
        t = f"<title>{e(title)}</title>" if title else ""
        return '<polygon points="%s" fill="%s" stroke="%s" stroke-width="%s">%s</polygon>' % (" ".join("%.1f,%.1f" % p for p in pts), fill, stroke, sw, t)
    floors = [0] if l["floors"] <= 1 else [0, 1]
    out = []
    for f in floors:
        b = [rect(l["rect"], "#efe8dc" if l["floors"] == 0 else ROOM_FILL, "#3b332c", 2)]
        for fl in l.get("flats", []):
            if fl["floor"] == f:
                b.append(rect(fl["rect"], "#f3e3c3" if fl["size"] <= 80 else "#e9dcef", "#6b5d50", .8, "%s, %d m²" % (fl["name"], fl["size"])))
        lv = next((v for v in (l.get("plan") or {}).get("levels", []) if v["level"] == f), None)
        if lv:
            for u in lv["units"]:
                b.append(rect(u["rect"], {"anchor": "#e9dcef", "food": "#f6dccb"}.get(u["kind"], "#e3e9f6"), "#6b5d50", .4, u["name"]))
        if l.get("stair"):
            b.append(rect(l["stair"]["rect"], "#c9a77e"))
        if l.get("lift"):
            a, c = P(*l["lift"]); b.append(f'<rect x="{a - 2:.1f}" y="{c - 2:.1f}" width="4" height="4" fill="#78909c"/>')
        levels = l.get("levels", {})
        for kind, pts in l.get("objects", {}).items():
            if kind not in DOT:
                continue
            fl_ = levels.get(kind, [0] * len(pts))
            for i, p in enumerate(pts):
                if (fl_[i] if i < len(fl_) else 0) == f:
                    a, c = P(*p); b.append(f'<circle cx="{a:.1f}" cy="{c:.1f}" r="1.4" fill="{DOT[kind]}"/>')
        if f == 0:
            a, c = P(*l["door"]); b.append(f'<circle cx="{a:.1f}" cy="{c:.1f}" r="3" fill="#c0392b" stroke="#fff"/>')
        out.append(svg(W, D, "".join(b), sc, "Ground floor" if f == 0 else "Floor 1" + (" (and up)" if l["floors"] > 2 else "")))
    return out


def plans_of(l):
    interior = (l.get("house") or {}).get("interior")
    return house_plan(PLANS[interior]) if interior in PLANS else lot_plan(l)
