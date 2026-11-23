"""Takes stock of every build in the city: one entry per type of building, with what the map data says about it,
as one HTML page (Prototype/play/builds.html). For planning remodels of the builds.

A "build type" is a design that several lots share: the designed houses by their furnishing, the row houses and court
flats by footprint, the towers, each shop, café, workshop and clinic shape, and each one-off building. Open places
(gardens, parks, Market Square, tram stops) are listed at the end. Details cover every lot of a type.
Reads World/city_01 only; writes nothing else.

Run:  python draw_builds.py [out.html]      (default Prototype/play/builds.html)
"""
import collections, html, json, os, re, sys
from draw_builds_plans import plans_of

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Prototype", "play", "builds.html")
CITY = os.path.join(ROOT, "World", "city_01")
FRONT = os.path.join(os.path.dirname(OUT), "builds", "front")   # one front picture per type (Tools/blender/building_styles_22.py)
city = json.load(open(os.path.join(CITY, "city.json"), encoding="utf-8"))
blocks = [json.load(open(os.path.join(CITY, b["file"]), encoding="utf-8")) for b in city["blocks"]]
e = lambda s: html.escape(str(s), quote=True)
STYLES = json.load(open(os.path.join(ROOT, "World", "rules", "house_styles.json"), encoding="utf-8"))

# ------------------------------------------------------------------ what each build is for
PURPOSE = {
    "house": "A family house in the communities: a designed front and roof, and inside, rooms on two floors round a stair that rises up the middle of the hall. Residents sleep, eat and spend evenings here.",
    "terrace": "A row house on the outskirts, facing its street with a garden behind. Residents sleep and live here.",
    "flats": "A court of flats round an Old Town courtyard garden: a stair in one corner and two to six flats on every floor.",
    "tower_home": "A residential tower on a plaza: a lift and a stair, and flats on every floor above the entrance hall.",
    "tower_office": "An office tower on a plaza: a lift and a stair, desks on every floor. Residents with office careers work here.",
    "shop": "A grocer or general shop: staff behind the counter, customers buying their daily needs.",
    "food": "A café: staff at the counter and kitchen, seats for residents' breakfast, lunch and evenings out.",
    "work": "A workshop or works: staff at benches and desks over its floors. A workplace for residents' careers.",
    "health": "A clinic: a receptionist and doctors, chairs for those waiting. Where residents go when unwell.",
    "kiosk": "A small pavilion on a tower's plaza, beside the tower's door.",
    "plaza_cafe": "A small café pavilion on a tower's plaza, for the tower's people at lunch.",
    "community_shop": "One of the small shops along a community's high street: grocer, kiosk, bakery.",
    "community_cafe": "The community's café on its high street.",
    "community_clinic": "The community's own clinic on its high street.",
    "market_shop": "A shop on Market Square.",
    "market_cafe": "A café on Market Square, busiest on market days.",
    "guild": "The Guild House on Market Square: the trades' hall and offices, with the gilded key over its door.",
    "hall": "Town Hall: the council's offices over three floors, facing the square.",
    "school_primary": "A community's primary school: classrooms for its children and their teachers.",
    "school_secondary": "The town's secondary school on Civic Row, beside its playing field.",
    "police": "The police station on Civic Row.",
    "fire": "The fire station on Civic Row: engine bays and the crew's rooms.",
    "works": "One of the town's works on Civic Row (water, power, waste): staff over two floors.",
    "depot": "A depot on the outskirts you can walk into: plant and workers over two floors.",
    "substation": "A small electricity substation; workers only.",
    "hospital": "A hospital: wards and doctors over its floors.",
    "mall": "A mall outside the walls: shops, cafés and anchor stores on three floors round open walks, bridges and fountains.",
    "arcade": "A covered arcade of shops and cafés.",
    "bank": "The town's bank: tellers at the counter, offices above (WLD-027).",
    "dealer": "The car showroom on a through street: cars on the floor, salespeople, offices above (WLD-027).",
    "library": "The town library: reading tables, the stacks upstairs (WLD-027).",
    "museum": "The town museum: galleries over three floors (WLD-027).",
    "cinema": "The cinema: rows of seats and the box office (WLD-027).",
    "furniture": "The furniture outlet beyond the East Gate: a showroom of furniture sets and rack rows (FURN-020).",
    "garden": "A garden or park: lawn, paths, hedges, benches and flower beds.",
    "market": "Market Square: stalls staffed on market days, a kiosk and a noticeboard.",
    "stop": "A tram or bus stop: places to wait along the kerb.",
}
LINE = {"plain": "Plain", "oldtown": "Old Town", "workshop": "Workshop", "curve": "Curve"}   # the four sample houses (add_houses_03.py)
SECTIONS = [("homes", "Homes"), ("shops", "Shops and cafés"), ("work", "Work"), ("civic", "Civic and services"),
            ("special", "Special places"), ("open", "Open places (not buildings)")]


def classify(l, b):
    """(key, section, title, purpose) of the build a lot is."""
    k, t, tags = l["kind"], b["template"], l.get("tags", [])
    w, d = round(l["rect"][2] - l["rect"][0]), round(l["rect"][3] - l["rect"][1])
    fp = f"{min(w, d)}×{max(w, d)} m"
    if "house" in l:
        h = l["house"]
        return (f"house_{h['plan']}_{h['furniture']}", "homes", f"Sample house, {LINE[h['furniture']]} line", PURPOSE["house"])
    if k == "home":
        if l.get("tower"): return ("tower_home", "homes", "Residential tower", PURPOSE["tower_home"])
        if l.get("flats"): return (f"flats_{min(w, d)}x{max(w, d)}", "homes", f"Court flats, {fp}", PURPOSE["flats"])
        return (f"terrace_{w}x{d}_{len(l.get('storeys', []))}", "homes", f"Row house, {fp}, {len(l.get('storeys', []))} storeys", PURPOSE["terrace"])
    if k in ("shop", "food", "health") and t == "community":
        n = {"shop": "community_shop", "food": "community_cafe", "health": "community_clinic"}[k]
        return (n, "shops" if k != "health" else "civic", {"shop": "Community shop", "food": "Community café", "health": "Community clinic"}[k], PURPOSE[n])
    if t in ("towerO", "towerR") and k in ("shop", "food"):
        n = "kiosk" if k == "shop" else "plaza_cafe"
        return (n, "shops", "Plaza kiosk" if k == "shop" else "Plaza café", PURPOSE[n])
    if k == "work" and l.get("tower"): return ("tower_office", "work", "Office tower", PURPOSE["tower_office"])
    if t == "market" and k in ("shop", "food"):
        n = "market_shop" if k == "shop" else "market_cafe"
        return (n, "shops", "Market Square shop" if k == "shop" else "Market Square café", PURPOSE[n])
    if t == "market" and k == "work": return ("guild", "civic", "Guild House", PURPOSE["guild"])
    if t == "hall": return ("hall", "civic", "Town Hall", PURPOSE["hall"])
    if k == "shop" and "mall" in tags and "plan" in l: return ("mall_" + l["id"], "special", l["name"], PURPOSE["mall"])
    if k == "shop" and (t == "mall" or "mall" in tags): return ("arcade_" + l["id"], "special", l["name"], PURPOSE["arcade"])
    if k == "health" and (t == "hospital" or "hospital" in tags): return ("hospital_" + l["id"], "civic", l["name"], PURPOSE["hospital"] + (" Old Town's second hospital (WLD-027), in a court building." if t != "hospital" else ""))
    if k == "school": return ("school_" + tags[-1], "civic", f"{tags[-1].title()} school", PURPOSE["school_" + tags[-1]])
    if k in ("police", "fire"): return (k, "civic", l["name"], PURPOSE[k])
    if k == "utility":
        if "works" in tags: return ("works", "civic", "Civic works (water, power, waste)", PURPOSE["works"])
        if l["floors"] > 1: return ("depot", "civic", "Depot", PURPOSE["depot"])
        return ("substation", "civic", "Substation", PURPOSE["substation"])
    if k in ("bank", "dealer", "library", "museum", "cinema", "furniture"): return (k, "special", l["name"], PURPOSE[k])
    if k in ("shop", "food", "work", "health"):
        word = {"shop": "Court shop", "food": "Court café", "work": "Court works", "health": "Court clinic"}[k]
        return (f"{k}_{min(w, d)}x{max(w, d)}", {"shop": "shops", "food": "shops", "work": "work", "health": "civic"}[k], f"{word}, {fp}", PURPOSE[k])
    if k == "garden":
        if "park" in tags or "playground" in tags or t == "park": n, title = "park_" + str(w), ("Town Park" if w > 100 else "Quarter park")
        elif "field" in tags: n, title = "field", "Playing field"
        elif t == "community": n, title = "green", "Community green"
        else: n, title = "court_garden", "Court garden"
        return (n, "open", title, PURPOSE["garden"])
    if k == "market": return ("market", "open", "Market Square", PURPOSE["market"])
    if k == "stop": return ("stop", "open", "Tram stop", PURPOSE["stop"])
    return (f"{k}_{t}", "special", l["name"], k)




OBJ_NAME = {"work": "work place", "seat": "seat", "customer": "customer spot", "bed": "bed", "chair": "waiting chair",
            "stall": "stall", "bench": "bench", "flowerbed": "flower bed", "wait": "waiting spot", "kiosk": "kiosk",
            "notice": "noticeboard"}


# ------------------------------------------------------------------ group the lots
ALL = [(l, b) for b in blocks for l in b["lots"]]
groups = collections.OrderedDict()
for l, b in ALL:
    key, sec, title, purpose = classify(l, b)
    g = groups.setdefault(key, dict(key=key, section=sec, title=title, purpose=purpose, lots=[]))
    g["lots"].append((l, b))
for g in groups.values():                                     # the lot the per-lot details are read from
    g["sample"], g["sample_block"] = min(g["lots"], key=lambda lb: lb[0]["id"])


def rng(vals, unit=""):
    vals = sorted(set(vals))
    if not vals: return "—"
    return f"{vals[0]}{unit}" if len(vals) == 1 else f"{vals[0]} to {vals[-1]}{unit}"


def details(g):
    L = [l for l, b in g["lots"]]; B = [b for l, b in g["lots"]]
    s = g["sample"]
    rows = []
    add = lambda k, v: rows.append((k, v))
    add("In the town", f"{len(L)} lot{'s' if len(L) > 1 else ''}")
    dist = collections.Counter(b["district"] for b in B)
    add("Districts", ", ".join(f"{d} ({n})" for d, n in dist.most_common()))
    add("Block template", ", ".join(sorted(set(b["template"] for b in B))))
    sizes = collections.Counter(f"{round(l['rect'][2] - l['rect'][0], 1)} × {round(l['rect'][3] - l['rect'][1], 1)}" for l in L)
    add("Footprint (m)", ", ".join(f"{k}" + (f" ({n})" if len(sizes) > 1 else "") for k, n in sizes.most_common(4)))
    if any(l["floors"] for l in L):
        add("Floors", rng([l["floors"] for l in L]))
        stc = collections.Counter(" / ".join(l["storeys"]) for l in L if l.get("storeys"))
        if stc: add("Storeys", "; ".join(k if len(k) < 70 else k[:66] + "…" for k, n in stc.most_common(3)))
    res = [l.get("residents", 0) for l in L]
    if any(res): add("Residents", f"{sum(res)} in all, {rng(res)} each")
    flats = [len(l.get("flats", [])) for l in L]
    if any(flats):
        sz = collections.Counter(f["size"] for l in L for f in l.get("flats", []))
        add("Flats", f"{sum(flats)} in all, {rng(flats)} each; sizes " + ", ".join(f"{k} m² ({n})" for k, n in sorted(sz.items())))
    staff = [l.get("staff") for l in L if l.get("staff")]
    if staff: add("Staff", rng(staff))
    objs = collections.Counter()
    for kind, pts in s.get("objects", {}).items(): objs[kind] += len(pts)
    if objs: add(f"Places inside ({s['name']})", ", ".join(f"{n} {OBJ_NAME.get(k, k)}{'s' if n > 1 else ''}" for k, n in objs.items() if k != "room"))
    for k in ("seats", "customers", "desks"):
        v = [l.get(k) for l in L if l.get(k)]
        if v: add(k.title(), rng(v))
    if "house" in s:
        h = s["house"]
        add("House line", LINE[h["furniture"]] + " (lot.house.furniture; picked by add_houses_03.py)")
        ways = collections.Counter(l["house"].get("style", "a") for l in L)
        add("Colourways", ", ".join(f"{w}: {STYLES['colourways'][h['furniture']][w]['name']} ({n})" for w, n in sorted(ways.items())) + " (house.style, add_houses_04.py; ADR-0032)")
        add("Plan", f"{h.get('interior', h['plan'])} (World/rules/home_plans.json; ADR-0033), outside {h['plan']} ({h['source']})")
        add("Rooms", ", ".join(f"{r['name']} (floor {r['floor']})" for r in h["rooms"]))
        add("Window boxes", f"{sum(1 for l in L if l['house']['trim'].get('window_boxes'))} of {len(L)} (where the line has them)")
        add("Turned to its lane", rng([l.get("angle", 0) for l in L], "°"))
    if s.get("plan"):
        p = s["plan"]
        add("Mall plan", f"{p.get('concept')}, storeys {p.get('storey_height')} m; " + ", ".join(f"level {v['level']}: {len(v['units'])} units" for v in p["levels"]) + f"; {len(p.get('fountains', []))} fountains")
    tags = sorted(set(t for l in L for t in l.get("tags", [])))
    if tags: add("Tags", ", ".join(tags))
    if s.get("lift"): add("Lift", "yes")
    if s.get("stair"): add("Stair", f"against the {({'n': 'north', 's': 'south', 'w': 'west', 'e': 'east'})[s['stair']['wall']]} wall")
    add("Can be entered", "yes" if all(l["enterable"] for l in L) else "no" if not any(l["enterable"] for l in L) else f"{sum(1 for l in L if l['enterable'])} of {len(L)}")
    wb = sum(1 for l in L if l.get("window_boxes"))
    if wb and "house" not in s: add("Window boxes", f"{wb} of {len(L)}")
    add("Example lot", f"{s['name']} ({s['id']}, block {g['sample_block']['id']})")
    return rows


def names_of(g):
    """Every name of a type, with how many lots carry it."""
    c = collections.Counter(l["name"] for l, b in g["lots"])
    return ", ".join(f"{n} ×{k}" if k > 1 else n for n, k in sorted(c.items(), key=lambda nk: [int(t) if t.isdigit() else t for t in re.split(r"(\d+)", nk[0])]))


def summary(g, key):
    L = [l for l, b in g["lots"]]
    if key == "floors": return rng([l["floors"] for l in L]) if any(l["floors"] for l in L) else "open"
    if key == "size":
        c = collections.Counter("{} × {}".format(*sorted((round(l["rect"][2] - l["rect"][0]), round(l["rect"][3] - l["rect"][1])))) for l in L)
        return c.most_common(1)[0][0] + (" …" if len(c) > 1 else "")
    if key == "people":
        r, st = sum(l.get("residents", 0) for l in L), sum(l.get("staff", 0) or 0 for l in L)
        return ", ".join(x for x in ((f"{r} residents" if r else ""), (f"{st} staff" if st else "")) if x) or "—"
    if key == "where": return ", ".join(d for d, n in collections.Counter(b["district"] for l, b in g["lots"]).most_common())


def write_page():
    total = len(ALL)
    order = []
    for sec, _ in SECTIONS:
        order += sorted((g for g in groups.values() if g["section"] == sec), key=lambda g: (-len(g["lots"]), g["title"]))
    nb = [g for g in order if g["section"] != "open"]
    rows, cards, sec_seen = [], [], None
    for i, g in enumerate(order, 1):
        if g["section"] != sec_seen:
            sec_seen = g["section"]; title = dict(SECTIONS)[sec_seen]
            gs = [x for x in order if x["section"] == sec_seen]
            rows.append(f'<tr class="sec"><th colspan="7">{e(title)} <span>{len(gs)} types · {sum(len(x["lots"]) for x in gs)} lots</span></th></tr>')
            cards.append(f'<h2 id="{sec_seen}">{e(title)}</h2>')
        q = e((g["title"] + " " + names_of(g) + " " + summary(g, "where")).lower())
        rows.append(f'<tr data-q="{q}"><td class="n">{i}</td><td><a href="#{e(g["key"])}">{e(g["title"])}</a></td><td class="num">{len(g["lots"])}</td>'
                    f'<td>{e(summary(g, "floors"))}</td><td>{e(summary(g, "size"))}</td><td>{e(summary(g, "people"))}</td><td>{e(summary(g, "where"))}</td></tr>')
        det = "".join(f"<dt>{e(k)}</dt><dd>{e(v)}</dd>" for k, v in details(g))
        nm = names_of(g)
        pic = os.path.join(FRONT, g["key"] + ".jpg")
        front = (f'<figure class="front"><img src="builds/front/{e(g["key"])}.jpg" alt="Front of {e(g["sample"]["name"])}" loading="lazy">'
                 f'<figcaption>Front: {e(g["sample"]["name"])}</figcaption></figure>' if os.path.exists(pic) else "")
        figs = f'<div class="figs">{front}{"".join(plans_of(g["sample"]))}</div>'
        cards.append(f'<article id="{e(g["key"])}" data-q="{q}"><header><h3><span class="n">{i}</span>{e(g["title"])}</h3><span class="pill">{len(g["lots"])} in town</span></header>'
                     f'<p class="purpose">{e(g["purpose"])}</p>{figs}<dl>{det}<dt>Names</dt><dd class="names">{e(nm)}</dd></dl></article>')
    page = f"""<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Brix Town builds</title>
<style>
:root {{ --bg:#f6f3ee; --card:#fffdf9; --ink:#2b2622; --mute:#776b60; --line:#e3dcd2; --acc:#8a4b2f; --row:#faf7f2; }}
@media (prefers-color-scheme:dark) {{ :root {{ --bg:#1c1a18; --card:#25221f; --ink:#ede5da; --mute:#a69a8d; --line:#3a3530; --acc:#e0a27f; --row:#2a2623; }} }}
* {{ box-sizing:border-box; }}
body {{ margin:0; background:var(--bg); color:var(--ink); font:14px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif; }}
.top {{ position:sticky; top:0; z-index:2; background:var(--bg); border-bottom:1px solid var(--line); padding:12px 24px; display:flex; flex-wrap:wrap; gap:8px 20px; align-items:baseline; }}
.top h1 {{ margin:0; font-size:20px; }} .top .sum {{ color:var(--mute); }}
.top input {{ margin-left:auto; min-width:240px; padding:7px 10px; border:1px solid var(--line); border-radius:6px; background:var(--card); color:var(--ink); font:inherit; }}
main {{ max-width:1200px; margin:0 auto; padding:16px 24px 80px; }}
p.note {{ color:var(--mute); margin:4px 0 18px; }}
table {{ width:100%; border-collapse:collapse; background:var(--card); border:1px solid var(--line); border-radius:8px; overflow:hidden; }}
th, td {{ text-align:left; padding:7px 10px; border-bottom:1px solid var(--line); vertical-align:top; }}
thead th {{ background:var(--row); color:var(--mute); font-weight:600; font-size:12px; text-transform:uppercase; letter-spacing:.04em; }}
tr.sec th {{ background:var(--row); color:var(--acc); font-size:14px; }} tr.sec span {{ color:var(--mute); font-weight:400; margin-left:8px; }}
td.num {{ text-align:right; font-variant-numeric:tabular-nums; font-weight:600; }} td.n, span.n {{ color:var(--mute); font-variant-numeric:tabular-nums; }}
td a {{ color:var(--ink); text-decoration:none; font-weight:600; }} td a:hover {{ color:var(--acc); text-decoration:underline; }}
h2 {{ margin:40px 0 12px; color:var(--acc); font-size:18px; }}
article {{ background:var(--card); border:1px solid var(--line); border-radius:8px; padding:16px 20px; margin-bottom:12px; scroll-margin-top:80px; }}
article header {{ display:flex; justify-content:space-between; gap:12px; align-items:baseline; }}
article h3 {{ margin:0; font-size:16px; }} span.n {{ margin-right:10px; font-weight:400; }}
.pill {{ background:var(--row); border:1px solid var(--line); color:var(--acc); border-radius:20px; padding:1px 10px; font-size:12px; font-weight:600; white-space:nowrap; }}
.purpose {{ margin:6px 0 12px; color:var(--mute); }}
dl {{ display:grid; grid-template-columns:180px minmax(0,1fr); gap:4px 16px; margin:0; font-size:13px; }}
dt {{ color:var(--mute); }} dd {{ margin:0; }} dd.names {{ max-height:9em; overflow:auto; }}
.figs {{ display:flex; flex-wrap:wrap; gap:14px; align-items:flex-start; margin:0 0 14px; }} figure {{ margin:0; }} figcaption {{ color:var(--mute); font-size:12px; margin-top:3px; }}
.figs svg {{ display:block; max-width:100%; height:auto; background:#faf7f2; border-radius:4px; }} .front img {{ display:block; width:360px; max-width:100%; aspect-ratio:3/2; object-fit:cover; border-radius:6px; }}
@media (max-width:720px) {{ .top, main {{ padding-left:16px; padding-right:16px; }} .top input {{ margin-left:0; width:100%; }}
  .hide-s {{ display:none; }} dl {{ grid-template-columns:1fr; }} dt {{ margin-top:6px; }} table {{ font-size:13px; }} }}
</style></head><body>
<div class="top"><h1>Brix Town builds</h1><span class="sum">{len(nb)} building types over {sum(len(g["lots"]) for g in nb)} buildings, {len(order) - len(nb)} kinds of open place; {total} lots in World/city_01</span>
<input id="q" type="search" placeholder="Filter: name, street, district…"></div>
<main>
<p class="note">One entry per type of build, read from the block files by Tools/worldgen/draw_builds.py. Front pictures are Blender renders (Tools/blender/building_styles_22.py); plans are drawn from the data, front at the bottom.</p>
<div style="overflow-x:auto"><table><thead><tr><th>#</th><th>Type</th><th>Count</th><th>Floors</th><th>Footprint (m)</th><th>People</th><th>Districts</th></tr></thead><tbody>
{"".join(rows)}
</tbody></table></div>
{"".join(cards)}
</main>
<script>
const q = document.getElementById("q");
q.addEventListener("input", () => {{ const v = q.value.trim().toLowerCase();
  document.querySelectorAll("[data-q]").forEach(r => r.style.display = !v || r.dataset.q.includes(v) ? "" : "none"); }});
</script></body></html>"""
    with open(OUT, "w", encoding="utf-8") as f: f.write(page)
    print(f"wrote {OUT}: {len(order)} types over {total} lots")


if __name__ == "__main__":
    write_page()
