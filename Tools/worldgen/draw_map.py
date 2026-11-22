"""Draws a top-down map of the city from World/ as one self-contained HTML file (SVG, hover for names).

Reads the block files and roads as they are, so it shows the city exactly as the data has it: use it to see a
remodel before and after editing World/. Writes nothing back.

Run:  python draw_map.py [out.html]        (default Prototype/play/map.html)
"""
import json, os, sys, html

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Prototype", "play", "map.html")
CITY = os.path.join(ROOT, "World", "city_01")
city = json.load(open(os.path.join(CITY, "city.json"), encoding="utf-8"))
roads = json.load(open(os.path.join(CITY, "roads.json"), encoding="utf-8"))
blocks = [json.load(open(os.path.join(CITY, b["file"]), encoding="utf-8")) for b in city["blocks"]]
TERRAIN = os.path.join(CITY, "terrain.json")
terrain = json.load(open(TERRAIN, encoding="utf-8")) if os.path.exists(TERRAIN) else None

COL = {"home": "#e8c9a0", "shop": "#7fb3d5", "food": "#f0a868", "work": "#a9a3c9", "health": "#e57373",
       "utility": "#9e9e9e", "garden": "#9cc98a", "market": "#f2d16b", "stop": "#4db6ac", "bank": "#c9a227",
       "library": "#8d6e63", "dealer": "#5c8fa8", "cinema": "#ba68c8", "museum": "#a1887f", "furniture": "#d48a5a",
       "school": "#e6c229", "fire": "#d32f2f", "police": "#3949ab"}
P, SW = city["grid"]["pitch"], city["grid"]["street_width"]
J = {j["id"]: j["pos"] for j in roads["junctions"]}
ext = lambda b: tuple(b["size"] + (n - 1) * P for n in b.get("span", [1, 1]))   # a block over several cells covers their streets too
xs = [p[0] for p in J.values()]; zs = [p[1] for p in J.values()]
for g in roads["gates"]:
    a = J[g["junction"]]; xs.append(a[0] + g["dir"][0] * g["length"]); zs.append(a[1] + g["dir"][1] * g["length"])
for b in blocks:
    sx, sz = ext(b); xs += [b["origin"][0] + sx]; zs += [b["origin"][1] + sz]
G = city["grid"]
xs += [0, G["columns"] * P]; zs += [0, G["rows"] * P]          # the whole grid, empty cells too
pad = 30
x0, z0, x1, z1 = min(xs) - pad, min(zs) - pad, max(xs) + pad, max(zs) + pad

e = lambda s: html.escape(str(s), quote=True)
svg, labels = [], []
svg.append(f'<rect x="{x0}" y="{z0}" width="{x1-x0}" height="{z1-z0}" class="ground"/>')
def contours(t, every):
    """Contour lines (marching squares) at every `every` m of height, as SVG lines."""
    H, st, (ox, oz) = t["heights"], t["step"], t["origin"]
    out = []
    top = max(max(r) for r in H)
    lv = every
    while lv < top:
        for z in range(len(H) - 1):
            for x in range(len(H[0]) - 1):
                c = [(x, z, H[z][x]), (x + 1, z, H[z][x + 1]), (x + 1, z + 1, H[z + 1][x + 1]), (x, z + 1, H[z + 1][x])]
                pts = []
                for (ax, az, a), (bx, bz, b) in zip(c, c[1:] + c[:1]):
                    if (a < lv) != (b < lv):
                        f = (lv - a) / (b - a)
                        pts.append((ox + (ax + (bx - ax) * f) * st, oz + (az + (bz - az) * f) * st))
                for k in range(0, len(pts) - 1, 2):
                    (x1, z1), (x2, z2) = pts[k], pts[k + 1]
                    out.append(f'<line x1="{x1:.1f}" y1="{z1:.1f}" x2="{x2:.1f}" y2="{z2:.1f}" class="contour{" major" if lv % (every * 5) == 0 else ""}"/>')
        lv += every
    return out
if terrain:
    svg += contours(terrain, 4)
have = {(b["grid"][0] + di, b["grid"][1] + dj) for b in blocks for di in range(b.get("span", [1, 1])[0]) for dj in range(b.get("span", [1, 1])[1])}
for i in range(G["columns"]):                                       # empty cells: an outline where a block could go
    for j in range(G["rows"]):
        if (i, j) not in have:
            svg.append(f'<rect x="{i*P+SW/2}" y="{j*P+SW/2}" width="{G["block_size"]}" height="{G["block_size"]}" class="empty">'
                       f'<title>empty cell {i}, {j}</title></rect>')
# streets
for s in roads["streets"]:
    a, b = J[s["a"]], J[s["b"]]
    svg.append(f'<line x1="{a[0]}" y1="{a[1]}" x2="{b[0]}" y2="{b[1]}" class="road" stroke-width="{SW}">'
               f'<title>{e(s["name"])} · {s["limit_kmh"]:.0f} km/h</title></line>')
for g in roads["gates"]:
    a = J[g["junction"]]; b = (a[0] + g["dir"][0] * g["length"], a[1] + g["dir"][1] * g["length"])
    svg.append(f'<line x1="{a[0]}" y1="{a[1]}" x2="{b[0]}" y2="{b[1]}" class="road" stroke-width="{SW}"><title>{e(g["name"])} road</title></line>')
    svg.append(f'<text x="{(a[0]+b[0])/2 + g["dir"][1]*18}" y="{(a[1]+b[1])/2 + g["dir"][0]*-14}" class="gate">{e(g["name"])}</text>')
for ln in roads.get("links", []):                                    # roads from the gates out to the new areas
    pts = " ".join(f"{x},{z}" for x, z in ln["points"])
    svg.append(f'<polyline points="{pts}" class="link" stroke-width="{ln["width"]}"><title>{e(ln["name"])} · {ln["limit_kmh"]:.0f} km/h</title></polyline>')
    m = ln["points"][len(ln["points"]) // 2]
    labels.append(f'<text x="{m[0]+8}" y="{m[1]-8}" class="lname">{e(ln["name"])}</text>')
for c in roads["crossings"]:
    pos = c.get("pos")
    if pos: svg.append(f'<circle cx="{pos[0]}" cy="{pos[1]}" r="1.6" class="xing"/>')
MAXF = max([l.get("floors", 1) for b in blocks for l in b["lots"] if l.get("flats") or l.get("plan")] + [1])   # floors with plans
UNIT = {"shop": "#7fb3d5", "food": "#f0a868", "anchor": "#8e7cc3", "kiosk": "#d48fb6", "foodhall": "#e07a4a"}
def poly_svg(pts, cls, tip=None):
    t = f"<title>{e(tip)}</title>" if tip else ""
    return f'<polygon points="{" ".join(f"{x},{z}" for x, z in pts)}" class="{cls}">{t}</polygon>'
FLAT = {80: "#cfe8c8", 160: "#9fd0a0", 320: "#5fae6e", 640: "#2e7d4f"}
COMMON = {"core": "#a9a296", "corridor": "#f3efe6", "lobby": "#ead9b0", "store": "#cfc8b8"}
def rect_svg(r, cls, fill=None, tip=None):
    f = f' fill="{fill}"' if fill else ""
    t = f"<title>{e(tip)}</title>" if tip else ""
    return f'<rect x="{r[0]}" y="{r[1]}" width="{r[2]-r[0]:.2f}" height="{r[3]-r[1]:.2f}" class="{cls}"{f}>{t}</rect>'
def flat_plan(l, only=None):
    """A court or tower floor by floor: core, corridor, lobby and stores, each flat by size, its door, who lives there."""
    out = []
    for f in range(l["floors"]):
        if only is not None and f != only:
            continue
        g = [f'<g class="lv lv{f}">']
        for c in l["common"]:
            if c["floor"] == f:
                g.append(rect_svg(c["rect"], "cm", COMMON[c["kind"]], c["kind"]))
        st = l["stair"]["rect"]
        g.append(rect_svg(st, "stair", None, "stair"))
        if l.get("lift"):
            x, z = l["lift"]; g.append(rect_svg([x - 1.1, z - 1.1, x + 1.1, z + 1.1], "liftb", None, "lift"))
        for q in l["flats"]:
            if q["floor"] != f:
                continue
            who = f', {len(q["household"])} living there' if q["household"] else ", empty"
            g.append(rect_svg(q["rect"], "flat" + (" lived" if q["household"] else ""), FLAT[q["size"]],
                              f'{l["name"]} {q["name"]} — {q["size"]} m² ({q["area"]:g} m²){who}'))
            g.append(f'<circle cx="{q["door"][0]}" cy="{q["door"][1]}" r=".45" class="fdoor"/>')
        out += g + ["</g>"]
    return out
HOME_TYPES = [   # (title, what it is, how a sample is picked)
    ("Row house, 2 floors", "Old Town 'rows' blocks: a house a family, rooms below, bedrooms above",
     lambda b, l: b["template"] == "rows" and l["kind"] == "home" and l["floors"] == 2 and l["rect"][2] - l["rect"][0] > 13),
    ("Row house, 3 floors", "the same, a storey higher", lambda b, l: b["template"] == "rows" and l["kind"] == "home" and l["floors"] == 3),
    ("Row house, corner (end of the row)", "the two houses on a block's side streets", lambda b, l: b["template"] == "rows" and l["kind"] == "home" and l["rect"][3] - l["rect"][1] > 15),
    ("Court building, long side", "Old Town 'peri' blocks: flats round a core and corridor", lambda b, l: b["template"] == "peri" and l.get("flats") and l["rect"][2] - l["rect"][0] > 30 and l["floors"] == 4),
    ("Court building, short side", "the same on the courtyard's east and west", lambda b, l: b["template"] == "peri" and l.get("flats") and l["rect"][3] - l["rect"][1] > 20 and l["floors"] == 4),
    ("Residential tower", "flats on 12 or 14 floors, a lift, a penthouse on top", lambda b, l: b["template"] == "towerR" and l.get("flats")),
    ("Independent house", "the communities: a family house turned to face its lane", lambda b, l: b["template"] == "community" and l["kind"] == "home"),
    ("Shop or cafe with rooms over", "a store floor above the shop, rooms above that (let by the property market)", lambda b, l: b["template"] == "peri" and l["kind"] in ("shop", "food") and l["floors"] >= 3),
]
OBJ = {"bed": ("#5d7fae", 1.0, 2.0), "room": ("#c08a4a", .7, .7), "work": ("#7a7a7a", 1.2, .6), "seat": ("#b06a5a", .6, .6), "customer": ("#d8c27a", .5, .5)}
def home_panels(blocks):
    """Under the map: a sample of every kind of home, floor by floor, with what is on each floor."""
    out = ['<h2 style="margin-top:28px">Homes: one of each kind, every floor</h2><p style="color:var(--muted)">'
           'Blue: a bed (1 x 2 m). Brown: a place to sit. Grey: a place to work. Dark: the stair; dark blue: the lift; red dot: a door. '
           'Flats as above (green by size); corridor, lobby, core and stores in greys.</p>']
    for title, what, test in HOME_TYPES:
        hit = next(((b, l) for b in blocks for l in b["lots"] if test(b, l)), None)
        if not hit:
            continue
        b, l = hit
        r = l["rect"]
        cx, cz = (r[0] + r[2]) / 2, (r[1] + r[3]) / 2
        R = max(r[2] - r[0], r[3] - r[1]) / 2 + (6 if l.get("house") else 2)   # room for a garage or conservatory
        vb = f"{cx - R} {cz - R} {2 * R} {2 * R}" if l.get("angle") else f"{r[0] - 1.5} {r[1] - 1.5} {r[2] - r[0] + 3} {r[3] - r[1] + 3}"
        rot = f' transform="rotate({l["angle"]} {cx} {cz})"' if l.get("angle") else ""
        lv = l.get("levels") or {}
        cells = []
        for f in range(l.get("floors", 1)):
            g = [f'<rect x="{r[0]}" y="{r[1]}" width="{r[2]-r[0]}" height="{r[3]-r[1]}" class="shell"{rot}/>']
            if l.get("house"):                                         # a designed house: its rooms in its own frame, turned with it
                Hs, sgn = l["house"], (-1 if l["house"]["mirror"] else 1)
                col = {"Living room": "#e8b98a", "Kitchen": "#f0d38a", "Study": "#cdbfa8", "Utility": "#bcd6d8", "Pantry": "#d8d2c4", "Hall": "#efe9dc",
                       "Main bedroom": "#b9acd8", "Bedroom 2": "#c9bfe2", "Bedroom 3": "#c9bfe2", "Bathroom": "#a9d2d6"}
                hg = [f'<g transform="translate({cx} {cz}) rotate({l.get("angle", 0)})">']
                for rm in Hs["rooms"]:
                    if rm["floor"] != f:
                        continue
                    u0, v0, u1, v1 = rm["rect"]
                    hg.append(f'<rect x="{u0}" y="{v0}" width="{u1-u0}" height="{v1-v0}" fill="{col.get(rm["name"], "#ddd")}" stroke="#555" stroke-width=".08"><title>{e(rm["name"])}</title></rect>')
                    hg.append(f'<text x="{(u0+u1)/2}" y="{(v0+v1)/2}" font-size=".55" text-anchor="middle" dominant-baseline="central" fill="#333">{e(rm["name"])}</text>')
                steps = [(-.75, -2.3 + 3.5 * k / 9, .75, -2.3 + 3.5 * (k + 1) / 9) for k in range(9)] + [(-1.5, 1.2, 1.5, 2.6)] +                         [(a if sd > 0 else -b2, 1.25, b2 if sd > 0 else -a, 2.25) for sd in (-1, 1) for k in range(9) for a, b2 in [(1.5 + 3.5 * k / 9, 1.5 + 3.5 * (k + 1) / 9)]] +                         [(-6.5, 1.2, -5, 2.3), (5, 1.2, 6.5, 2.3)]
                for u0, v0, u1, v1 in steps:
                    hg.append(f'<rect x="{u0}" y="{v0}" width="{u1-u0}" height="{v1-v0}" fill="#8f8a82" stroke="#444" stroke-width=".03"/>')
                for d in Hs["doors"]:
                    if d["floor"] == f:
                        hg.append(f'<circle cx="{d["at"][0]}" cy="{d["at"][1]}" r=".32" class="fdoor"/>')
                for ex in Hs.get("extras", []):
                    if f == 0 or ex["kind"] == "balcony":
                        u0, v0, u1, v1 = ex["rect"]
                        hg.append(f'<rect x="{u0}" y="{v0}" width="{u1-u0}" height="{v1-v0}" fill="none" stroke="#2e7d4f" stroke-width=".12" stroke-dasharray=".3 .2"><title>{ex["kind"]}</title></rect>')
                g += hg + ["</g>"]
            elif l.get("flats"):
                g += [x.replace(f'class="lv lv{f}"', 'class="one"') for x in flat_plan(l, only=f)]
            else:
                if l.get("stair"):
                    g.append(rect_svg(l["stair"]["rect"], "stair", None, "stair"))
                if l.get("lift"):
                    x, z = l["lift"]; g.append(rect_svg([x - 1.1, z - 1.1, x + 1.1, z + 1.1], "liftb", None, "lift"))
            if f == 0:
                g.append(f'<circle cx="{l["door"][0]}" cy="{l["door"][1]}" r=".6" class="fdoor"><title>front door</title></circle>')
            counts = {}
            for k, pts in l["objects"].items():
                if k not in OBJ:
                    continue
                col, w, d = OBJ[k]
                for n, (x, z) in enumerate(pts):
                    if (lv.get(k, [0] * len(pts))[n] if n < len(lv.get(k, [])) else 0) != f:
                        continue
                    counts[k] = counts.get(k, 0) + 1
                    t = f' transform="rotate({l["angle"]} {x} {z})"' if l.get("angle") else ""
                    g.append(f'<rect x="{x - w/2}" y="{z - d/2}" width="{w}" height="{d}" fill="{col}" class="obj"{t}><title>{k}</title></rect>')
            use = (l.get("storeys") or [])[f] if f < len(l.get("storeys") or []) else ""
            n_flats = sum(1 for q in l.get("flats", []) if q["floor"] == f)
            cap = f'{"Ground" if f == 0 else "Floor %d" % f}{(" · " + use) if use and use != "main" else ""}' +                   (f' · {n_flats} flats' if n_flats else "") + "".join(f" · {v} {k}{'s' if v != 1 else ''}" for k, v in counts.items())
            cells.append(f'<figure><figcaption>{e(cap)}</figcaption><svg viewBox="{vb}" class="floor" xmlns="http://www.w3.org/2000/svg">{"".join(g)}</svg></figure>')
        w = r[2] - r[0]; d = r[3] - r[1]
        out.append(f'<section class="mallrow"><h2>{e(title)} <span>· {e(l["name"])} ({e(l["id"])}), {w:g} x {d:g} m, {l.get("floors", 1)} floors · {e(what)}</span></h2>'
                   f'<div class="floors fl">{"".join(cells)}</div></section>')
    return "".join(out)
def flat_panels(blocks, picks):
    """Close-ups of a few buildings, every floor."""
    out = []
    lots = {l["id"]: l for b in blocks for l in b["lots"]}
    for lid in picks:
        l = lots.get(lid)
        if not l or not l.get("flats"):
            continue
        r, m = l["rect"], 1.5
        vb = f'{r[0]-m} {r[1]-m} {r[2]-r[0]+2*m} {r[3]-r[1]+2*m}'
        cells = []
        for f in range(l["floors"]):
            n = [q for q in l["flats"] if q["floor"] == f]
            mix = ", ".join(f'{sum(1 for q in n if q["size"] == z)}×{z}' for z in (80, 160, 320, 640) if any(q["size"] == z for q in n))
            body = "".join(x.replace(f'class="lv lv{f}"', 'class="one"') for x in flat_plan(l, only=f))
            out_r = f'<rect x="{r[0]}" y="{r[1]}" width="{r[2]-r[0]}" height="{r[3]-r[1]}" class="shell"/>'
            cells.append(f'<figure><figcaption>{"Ground" if f == 0 else "Floor %d" % f} · {mix}</figcaption>'
                         f'<svg viewBox="{vb}" class="floor" xmlns="http://www.w3.org/2000/svg">{out_r}{body}</svg></figure>')
        flats = l["flats"]
        out.append(f'<section class="mallrow"><h2>{e(l["name"])} <span>· {l["floors"]} floors, {len(flats)} flats, '
                   f'{sum(1 for q in flats if q["household"])} lived in</span></h2><div class="floors fl">{"".join(cells)}</div></section>')
    return "".join(out)
def mall_panels(blocks):
    """Under the map: each mall's floors side by side, drawn large."""
    out = []
    for b in blocks:
        for l in b["lots"]:
            if "plan" not in l:
                continue
            r, pl = l["rect"], l["plan"]
            m = 6
            vb = f'{r[0]-m} {r[1]-m} {r[2]-r[0]+2*m} {r[3]-r[1]+2*m}'
            tall = (r[3] - r[1]) > (r[2] - r[0])
            names = ["Ground", "Level 1", "Level 2"]
            cells = []
            for k in range(len(pl["levels"])):
                body = "".join(x.replace(f'class="lv lv{k}"', 'class="one"') for x in mall_plan(l, only=k))
                cells.append(f'<figure><figcaption>{names[k] if k < 3 else k}</figcaption>'
                             f'<svg viewBox="{vb}" class="floor{" tall" if tall else ""}" xmlns="http://www.w3.org/2000/svg">{body}</svg></figure>')
            out.append(f'<section class="mallrow"><h2>{e(l["name"])} <span>· {e(pl["concept"])}</span></h2><div class="floors{" tall" if tall else ""}">{"".join(cells)}</div></section>')
    return "".join(out)
def mall_plan(l, only=None):
    """A mall's floors, one group per level (shown by the level buttons), with water and fountains under them all."""
    pl, r = l["plan"], l["rect"]
    out = [f'<rect x="{r[0]}" y="{r[1]}" width="{r[2]-r[0]}" height="{r[3]-r[1]}" class="hall"><title>{e(l["name"])} — {e(pl["concept"])}</title></rect>']
    for t in pl.get("terraces", []):
        x0, z0, x1, z1 = t["rect"]
        out.append(f'<text x="{(x0+x1)/2}" y="{z1-6}" class="terr" text-anchor="middle">terrace +{t["ground"]:g} m</text>')
    for k, L in enumerate(pl["levels"]):
        if only is not None and k != only:
            continue
        g = [f'<g class="lv lv{k}">']
        g += [poly_svg(w, "walk") for w in L["walks"]]
        g += [poly_svg(v, "void", "open to the floor below") for v in L["voids"]]
        if k == 0:
            g += [poly_svg(w, "water", "canal") for w in pl.get("water", [])]
            g += [f'<line x1="{a[0]}" y1="{a[1]}" x2="{b2[0]}" y2="{b2[1]}" class="fall"><title>water steps down a terrace</title></line>' for a, b2 in pl.get("falls", [])]
        g += [poly_svg(v, "bridge", "bridge") for v in L["bridges"]]
        for u in L["units"]:
            x0, z0, x1, z1 = u["rect"]
            rot = f' transform="rotate({u["angle"]} {(x0+x1)/2} {(z0+z1)/2})"' if u.get("angle") is not None else ""
            g.append(f'<rect x="{x0}" y="{z0}" width="{x1-x0}" height="{z1-z0}" fill="{UNIT.get(u["kind"], "#ccc")}" class="unit"{rot}>'
                     f'<title>{e(u["name"])} — {e(u["kind"])}, level {k}</title></rect>')
        g += [f'<circle cx="{f["pos"][0]}" cy="{f["pos"][1]}" r="{f["r"]}" class="fount"><title>fountain</title></circle>'
              for f in pl["fountains"] if f["level"] == k]
        out += g + ["</g>"]
    return out
# blocks
for b in blocks:
    ox, oz = b["origin"]; sx, sz = ext(b)
    svg.append(f'<g class="block" data-id="{e(b["id"])}"><rect x="{ox}" y="{oz}" width="{sx}" height="{sz}" class="pave{" open" if b.get("lanes") else ""}">'
               f'<title>{e(b["id"])} · {e(b.get("template",""))} · {e(b.get("district",""))}</title></rect>')
    for ln in b.get("lanes", []):
        pts = " ".join(f"{x},{z}" for x, z in ln["points"])
        svg.append(f'<polyline points="{pts}" class="lane" stroke-width="{ln["width"]}"><title>{e(ln["name"])}</title></polyline>')
        if ln.get("end") == "turning":
            x, z = ln["points"][-1]; svg.append(f'<circle cx="{x}" cy="{z}" r="{ln["width"]}" class="turn"/>')
    for p in b.get("paths", []):
        (ax, az), (bx, bz) = p
        svg.append(f'<line x1="{ax}" y1="{az}" x2="{bx}" y2="{bz}" class="path"/>')
    for l in b["lots"]:
        if "plan" in l:                                               # a mall: its outline, then what is on each floor
            svg += mall_plan(l)
            continue
        r = l["rect"]; k = l["kind"]; col = COL.get(k, "#ccc")
        tall = l.get("floors", 0) >= 10
        tip = f'{l["name"]} — {k}' + (f', {l["floors"]} floors' if l.get("floors") else "") + f' ({l["id"]})'
        svg.append(f'<rect x="{r[0]}" y="{r[1]}" width="{r[2]-r[0]}" height="{r[3]-r[1]}" fill="{col}" '
                   f'class="lot{" tower" if tall else ""}" data-kind="{k}"'
                   + (f' transform="rotate({l["angle"]} {(r[0]+r[2])/2} {(r[1]+r[3])/2})"' if l.get("angle") else "")
                   + f'><title>{e(tip)}</title></rect>')
        d = l.get("door")
        if d: svg.append(f'<circle cx="{d[0]}" cy="{d[1]}" r="0.9" class="door"/>')
        if l.get("flats"):
            svg += flat_plan(l)
    for t in b.get("trees", []):
        svg.append(f'<circle cx="{t["pos"][0]}" cy="{t["pos"][1]}" r="2.2" class="tree"/>')
    svg.append(f'<text x="{ox+3}" y="{oz+7}" class="bid">{e(b["id"][2:])}</text></g>')
# street names
ns, ew = city["streets"]["north_south"], city["streets"]["east_west"]
for i, n in enumerate(ns):
    if not n: continue
    x = i * P; svg.append(f'<text x="{x}" y="{z0+14}" class="sname" text-anchor="middle">{e(n)}</text>')
for j, n in enumerate(ew):
    if not n: continue
    z = j * P; svg.append(f'<text x="{x0+4}" y="{z-SW/2-2}" class="sname">{e(n)}</text>')
for b in blocks:                                                      # area names over communities and malls
    if b.get("span") and b["template"] in ("community", "mall"):
        sx, sz = ext(b)
        labels.append(f'<text x="{b["origin"][0]+sx/2}" y="{b["origin"][1]+(sz/2 if b["template"]=="mall" else 26)}" class="area" text-anchor="middle">{e(b["district"])}</text>')
svg += labels
sp = city["spawn"]["pos"]
svg.append(f'<circle cx="{sp[0]}" cy="{sp[1]}" r="5" class="spawn"><title>Player spawn: {e(city["spawn"].get("note",""))}</title></circle>')

counts = {}
for b in blocks:
    for l in b["lots"]: counts[l["kind"]] = counts.get(l["kind"], 0) + 1
legend = "".join(f'<button class="lg" data-kind="{k}"><i style="background:{COL.get(k,"#ccc")}"></i>{k} <b>{counts[k]}</b></button>'
                 for k in sorted(counts, key=lambda k: -counts[k]))
dist = sorted({b.get("district", "") for b in blocks})

page = f"""<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Old Town Map</title><style>
:root{{--bg:#f6f3ec;--ink:#2b2b2b;--ground:#e9e4d6;--road:#6b6b6b;--pave:#d9d3c4;--muted:#777;--contour:#9a8456;--lane:#707070}}
@media (prefers-color-scheme:dark){{:root{{--bg:#1d1d1b;--ink:#eee;--ground:#2a2925;--road:#4a4a4a;--pave:#3a3832;--muted:#aaa;--contour:#b8a77a;--lane:#8a8a8a}}}}
body{{margin:0;background:var(--bg);color:var(--ink);font:14px system-ui,sans-serif}}
header{{padding:12px 16px}} h1{{font-size:18px;margin:0 0 4px}} p{{margin:0;color:var(--muted)}}
.legend{{display:flex;flex-wrap:wrap;gap:6px;padding:0 16px 8px}}
.lg{{border:1px solid #8884;background:transparent;color:var(--ink);border-radius:14px;padding:3px 9px;cursor:pointer;font:inherit;font-size:12px}}
.lg i{{display:inline-block;width:10px;height:10px;border-radius:2px;margin-right:5px;vertical-align:-1px}}
.lg.on{{outline:2px solid var(--ink)}}
#wrap{{padding:0 16px 16px}} svg{{width:100%;height:auto;max-height:88vh;display:block}}
.ground{{fill:var(--ground)}} .contour{{stroke:var(--contour);stroke-width:.8;fill:none}} .contour.major{{stroke-width:1.6}}
.lane{{fill:none;stroke:var(--lane);stroke-linecap:round;stroke-linejoin:round}} .turn{{fill:var(--lane)}}
.pave.open{{fill:#9cc98a;fill-opacity:.18;stroke:#3f7d3a55;stroke-width:1}}
.link{{fill:none;stroke:var(--road);stroke-linecap:round;stroke-linejoin:round}}
.lname{{font-size:11px;font-weight:600;fill:var(--ink);paint-order:stroke;stroke:var(--bg);stroke-width:3px}}
.area{{font-size:26px;font-weight:700;fill:var(--ink);opacity:.85;paint-order:stroke;stroke:var(--bg);stroke-width:5px}}
.hall{{fill:#efe9dc;stroke:#6b6b6b;stroke-width:1.5}} .lv{{display:none}} {",".join(f'svg[data-lv="{k}"] .lv{k}' for k in range(MAXF))}{{display:inline}}
.cm{{stroke:#0004;stroke-width:.08}} .flat{{stroke:#2b2b2b;stroke-width:.12}} .flat.lived{{stroke-width:.4}} .stair{{fill:#6f6a66}} .liftb{{fill:#4a4e5e}}
.fdoor{{fill:#c62828}} .obj{{stroke:#0006;stroke-width:.05}} .shell{{fill:#e9e4d6;stroke:#555;stroke-width:.35}} #malls .floors.fl{{grid-template-columns:repeat(auto-fill,minmax(240px,1fr))}}
.walk{{fill:#f3efe6;stroke:#a89f8c;stroke-width:.5}} .void{{fill:#3d4a57;opacity:.8}} .bridge{{fill:#bfb69f;stroke:#6b6b6b;stroke-width:.4}}
.water{{fill:#6fa8dc}} .fall{{stroke:#fff;stroke-width:2.5;stroke-dasharray:2 1.5}} .unit{{stroke:#0006;stroke-width:.35}} .unit:hover{{stroke:#d00;stroke-width:1.2}}
.fount{{fill:#bfe3ef;stroke:#3b8fb0;stroke-width:1}} .terr{{font-size:9px;fill:#555;font-style:italic}}
#malls{{padding:0 16px 24px}} .mallrow h2{{font-size:16px;margin:18px 0 6px}} .mallrow h2 span{{color:var(--muted);font-weight:400}}
.floors{{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}} .floors:not(.tall){{grid-template-columns:1fr}}
figure{{margin:0;background:var(--ground);border-radius:6px;padding:6px}} figcaption{{font-size:12px;color:var(--muted);margin-bottom:4px}}
svg.floor{{width:100%;height:auto;max-height:none}} svg.floor.tall{{max-height:640px}} svg.floor .one{{display:inline}}
.empty{{fill:var(--pave);fill-opacity:.35;stroke:var(--muted);stroke-width:1.2;stroke-dasharray:4 3}} .road{{stroke:var(--road);stroke-linecap:square}} .pave{{fill:var(--pave)}}
.path{{stroke:#bfb69f;stroke-width:1.2}} .lot{{stroke:#0005;stroke-width:.4}} .tower{{stroke:#000;stroke-width:1.6}}
.lot:hover{{stroke:#d00;stroke-width:1.5}} .door{{fill:#222}} .tree{{fill:#3f7d3a;opacity:.75}}
.xing{{fill:#fff8}} .bid{{font-size:6px;fill:var(--muted)}} .sname{{font-size:8px;fill:var(--ink);opacity:.8}}
.gate{{font-size:9px;font-weight:600;fill:var(--ink)}} .spawn{{fill:none;stroke:#e91e63;stroke-width:2}}
svg.filter .lot{{opacity:.15}} svg.filter .lot.hit{{opacity:1}}
</style></head><body><header><h1>{e(city["name"])} — map from World/city_01</h1>
<p>Grid {G["columns"]} × {G["rows"]} cells: {len(have)} with blocks, {G["columns"]*G["rows"]-len(have)} empty (dashed) · {sum(counts.values())} lots · districts: {e(", ".join(dist))}. Hover a building for its name and id; thick outline = tower (10+ floors); pink ring = player spawn. Click a kind to pick it out.</p></header>
<div class="legend">{legend}</div>
<div class="legend"><b style="align-self:center">Floor (malls, courts, towers):</b> {"".join(f'<button class="lg lvb{" on" if k == 0 else ""}" data-lv="{k}">{"Ground" if k == 0 else k}</button>' for k in range(MAXF))}</div>
<div class="legend"><span style="align-self:center;color:var(--muted);font-size:12px">Flats:
<i style="display:inline-block;width:10px;height:10px;background:#cfe8c8"></i> 80 m²
<i style="display:inline-block;width:10px;height:10px;background:#9fd0a0"></i> 160 m²
<i style="display:inline-block;width:10px;height:10px;background:#5fae6e"></i> 320 m²
<i style="display:inline-block;width:10px;height:10px;background:#2e7d4f"></i> 640 m²
(thick outline: lived in) ·
<i style="display:inline-block;width:10px;height:10px;background:#a9a296"></i> stair and lift core
<i style="display:inline-block;width:10px;height:10px;background:#f3efe6;outline:1px solid #8884"></i> corridor
<i style="display:inline-block;width:10px;height:10px;background:#ead9b0"></i> lobby
<i style="display:inline-block;width:10px;height:10px;background:#cfc8b8"></i> store · dot: flat door</span></div>
<div class="legend">
<span style="align-self:center;color:var(--muted);font-size:12px">
<i style="display:inline-block;width:10px;height:10px;background:#7fb3d5"></i> shop
<i style="display:inline-block;width:10px;height:10px;background:#f0a868"></i> food
<i style="display:inline-block;width:10px;height:10px;background:#8e7cc3"></i> big store
<i style="display:inline-block;width:10px;height:10px;background:#d48fb6"></i> kiosk
<i style="display:inline-block;width:10px;height:10px;background:#e07a4a"></i> food hall
<i style="display:inline-block;width:10px;height:10px;background:#f3efe6;outline:1px solid #8884"></i> walk
<i style="display:inline-block;width:10px;height:10px;background:#3d4a57"></i> open void
<i style="display:inline-block;width:10px;height:10px;background:#bfb69f"></i> bridge
<i style="display:inline-block;width:10px;height:10px;background:#6fa8dc"></i> water
<i style="display:inline-block;width:10px;height:10px;border-radius:50%;background:#bfe3ef"></i> fountain</span></div>
<div id="wrap"><svg id="m" viewBox="{x0} {z0} {x1-x0} {z1-z0}" xmlns="http://www.w3.org/2000/svg">{''.join(svg)}</svg></div>
<div id="malls">{mall_panels(blocks)}{home_panels(blocks)}</div>
<script>
const m=document.getElementById('m');let cur=null;m.dataset.lv='0';
document.querySelectorAll('.lvb').forEach(b=>b.onclick=()=>{{m.dataset.lv=b.dataset.lv;document.querySelectorAll('.lvb').forEach(x=>x.classList.toggle('on',x===b));}});
document.querySelectorAll('.lg[data-kind]').forEach(b=>b.onclick=()=>{{const k=b.dataset.kind;cur=cur===k?null:k;
document.querySelectorAll('.lg[data-kind]').forEach(x=>x.classList.toggle('on',x.dataset.kind===cur));m.classList.toggle('filter',!!cur);
m.querySelectorAll('.lot').forEach(l=>l.classList.toggle('hit',l.dataset.kind===cur));}});
</script></body></html>"""
open(OUT, "w", encoding="utf-8").write(page)
print("wrote", OUT, sum(counts.values()), "lots")
