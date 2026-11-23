"""Heat map of empty ground: how far each spot in the city is from the nearest thing on it (a building, road, path,
tree, plant, prop, field or pond), drawn from World/ as the data has it. Bare stretches glow; the hotter, the further
you can walk there without passing anything. Use it to pick which spaces need filling.

Run:  python empty_space_map.py [out.png]      (default Art/StyleTests/empty_space_24/heat.png)
Also prints the biggest empty patches (spots 15 m or more from anything), largest first, with the area they lie in.
Reads the data only; writes nothing back to World/.
"""
import json, math, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Art", "StyleTests", "empty_space_24", "heat.png")
CITY = os.path.join(ROOT, "World", "city_01")
jl = lambda p: json.load(open(p, encoding="utf-8"))
city, roads = jl(os.path.join(CITY, "city.json")), jl(os.path.join(CITY, "roads.json"))
veg = jl(os.path.join(ROOT, "World", "rules", "vegetation.json"))
blocks = [jl(os.path.join(CITY, e["file"])) for e in city["blocks"]]
G = city["grid"]; P, SW = G["pitch"], G["street_width"]
W_M, H_M = G["columns"] * P, G["rows"] * P                     # the city's grid, in metres
M = 2.0                                                          # metres a pixel
MARGIN = 96                                                      # shown round the grid, dimmed (the fields beyond the map)
OPEN = ("garden", "market", "stop")
NEAR, FAR = 6.0, 40.0                                            # no colour nearer than NEAR m to something; full heat at FAR m

X0, Z0 = -MARGIN, -MARGIN
NX, NZ = int((W_M + 2 * MARGIN) / M), int((H_M + 2 * MARGIN) / M)
occ = Image.new("L", (NX, NZ), 0); d = ImageDraw.Draw(occ)
T = lambda x, z: ((x - X0) / M, (z - Z0) / M)


def line(pts, w):
    pts = [T(*p) for p in pts]
    if len(pts) == 1:
        pts = pts * 2
    d.line(pts, fill=255, width=max(1, int(round(w / M))))
    for p in (pts[0], pts[-1]):
        r = w / M / 2; d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=255)


def rect(x0, z0, x1, z1):
    d.rectangle([T(x0, z0), T(x1, z1)], fill=255)


def disc(x, z, r):
    a = T(x, z); r /= M; d.ellipse([a[0] - r, a[1] - r, a[0] + r, a[1] + r], fill=255)


J = {j["id"]: j["pos"] for j in roads["junctions"]}
for s in roads["streets"]:
    line([J[s["a"]], J[s["b"]]], SW)
for g in roads["gates"]:
    a = J[g["junction"]]; line([a, [a[0] + g["dir"][0] * g["length"], a[1] + g["dir"][1] * g["length"]]], 9)
for l in roads.get("links", []):
    line(l["points"], l["width"])
tz = city["streets"]["east_west"].index("Tram") * P - 1.8
wl = city["wall"]["cells"]
line([[wl[0] * P - SW / 2 - 40, tz], [wl[2] * P + SW / 2 + 40, tz]], 2)

R_TREE = {k: v["r_max"] * 0.8 for k, v in veg["species"].items()}
names = []                                                       # (x0, z0, x1, z1, name) of every block, for the patch list
for b in blocks:
    sp = b.get("span", [1, 1]); ox, oz = b["origin"]
    ex, ez = b["size"] + (sp[0] - 1) * P, b["size"] + (sp[1] - 1) * P
    names.append((ox, oz, ox + ex, oz + ez, b["district"], b["template"]))
    for ln in b.get("lanes", []):
        line(ln["points"], ln["width"])
        if ln.get("end") == "turning":
            disc(*ln["points"][-1], ln["width"])
    for t in b.get("tracks", []):
        line(t["points"], t["width"])
    for p, q in b["paths"]:
        line([p, q], 1.6)
    for l in b["lots"]:
        if l["kind"] in OPEN:
            continue
        x0, z0, x1, z1 = l["rect"]
        if l.get("angle"):
            cx, cz, t = (x0 + x1) / 2, (z0 + z1) / 2, math.radians(l["angle"])
            c, s = math.cos(t), math.sin(t)
            corners = [(cx + (u - cx) * c - (v - cz) * s, cz + (u - cx) * s + (v - cz) * c) for u, v in ((x0, z0), (x1, z0), (x1, z1), (x0, z1))]
            d.polygon([T(*q) for q in corners], fill=255)
        else:
            rect(x0, z0, x1, z1)
    for t in b["trees"]:
        disc(*t["pos"], R_TREE.get(t["species"], 3))
    for p in b["props"]:
        x, z = p["pos"]; k = p["kind"]
        if "size" in p and isinstance(p["size"], list):
            w, h = p["size"]; rect(x - w / 2, z - h / 2, x + w / 2, z + h / 2)
        elif p.get("len"):
            L, dep = p["len"], p.get("depth", 1.0)
            line([[x - L / 2, z], [x + L / 2, z]] if p.get("along", "x") == "x" else [[x, z - L / 2], [x, z + L / 2]], dep)
        elif p.get("r"):
            disc(x, z, p["r"])
        elif k == "bunting":
            continue
        else:
            disc(x, z, {"playground": 9, "fountain": 3.6, "statue": 2, "stall": 2.5, "shelter": 3, "bandstand": 5, "watertower": 3}.get(k, 1.2))

# distance from the nearest thing, in metres (a chamfer distance: steps of 1 and 1.41 pixels), up to FAR + 20 m
o = np.array(occ) > 0
INF = 1e9
dist = np.where(o, 0.0, INF)
cap = (FAR + 20) / M
for _ in range(int(cap) + 2):
    prev = dist
    nd = dist.copy()
    for dx, dz, c in ((1, 0, 1), (-1, 0, 1), (0, 1, 1), (0, -1, 1), (1, 1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (-1, -1, 1.414)):
        sh = np.full_like(dist, INF)
        sh[max(0, dz):NZ + min(0, dz), max(0, dx):NX + min(0, dx)] = dist[max(0, -dz):NZ + min(0, -dz), max(0, -dx):NX + min(0, -dx)]
        np.minimum(nd, sh + c, out=nd)
    dist = nd
    if np.array_equal(prev, dist):
        break
dist = np.minimum(dist, cap) * M

# the picture: the town in grey, the heat over it
img = Image.new("RGB", (NX, NZ), (120, 120, 112))
base = np.array(img).astype(float)
base[o] = (70, 70, 66)
t = np.clip((dist - NEAR) / (FAR - NEAR), 0, 1)
ramp = np.array([[255, 236, 160], [250, 180, 70], [230, 90, 40], [150, 20, 30]], float)
idx = t * (len(ramp) - 1); i0 = np.floor(idx).astype(int).clip(0, len(ramp) - 2); f = (idx - i0)[..., None]
heat = ramp[i0] * (1 - f) + ramp[i0 + 1] * f
alpha = (np.where(dist > NEAR, 0.35 + 0.6 * t, 0.0))[..., None]
pic = base * (1 - alpha) + heat * alpha
gx0, gz0 = int(-X0 / M), int(-Z0 / M); gx1, gz1 = int((W_M - X0) / M), int((H_M - Z0) / M)
outside = np.ones((NZ, NX), bool); outside[gz0:gz1, gx0:gx1] = False
pic[outside] = pic[outside] * 0.45 + 30
img = Image.fromarray(pic.clip(0, 255).astype(np.uint8))
S = 2
img = img.resize((NX * S, NZ * S), Image.NEAREST)
dr = ImageDraw.Draw(img)
try:
    font = ImageFont.truetype("arial.ttf", 15); small = ImageFont.truetype("arial.ttf", 12)
except OSError:
    font = small = ImageFont.load_default()
TT = lambda x, z: ((x - X0) / M * S, (z - Z0) / M * S)
for i in range(G["columns"] + 1):
    dr.text((TT(i * P + 36, -60)), str(i), fill=(230, 230, 220), font=small)
for j in range(G["rows"] + 1):
    dr.text((TT(-70, j * P + 40)), str(j), fill=(230, 230, 220), font=small)

# patches: spots 15 m or more from anything, joined where they touch (on a 4 m grid), largest first
cell = 2
small_d = dist[::cell, ::cell]
hot = small_d >= 15
hot &= ~outside[::cell, ::cell]                                  # only on the map grid: the fields beyond it are not counted
seen = np.zeros_like(hot)
patches = []
for zz in range(hot.shape[0]):
    for xx in range(hot.shape[1]):
        if not hot[zz, xx] or seen[zz, xx]:
            continue
        stack, pts = [(zz, xx)], []
        seen[zz, xx] = True
        while stack:
            a, c = stack.pop(); pts.append((a, c))
            for da, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                na, nc = a + da, c + dc
                if 0 <= na < hot.shape[0] and 0 <= nc < hot.shape[1] and hot[na, nc] and not seen[na, nc]:
                    seen[na, nc] = True; stack.append((na, nc))
        pa, pc = max(pts, key=lambda q: small_d[q[0], q[1]])    # the emptiest spot: where the number goes
        mx, mz = pc * cell * M + X0, pa * cell * M + Z0
        peak = small_d[pa, pc]
        inside = 0 <= mx <= W_M and 0 <= mz <= H_M
        where = next((f"{n[4]} ({n[5]})" for n in names if n[0] - 8 <= mx <= n[2] + 8 and n[1] - 8 <= mz <= n[3] + 8), "open field between blocks" if inside else "beyond the map grid")
        patches.append(dict(area=len(pts) * (cell * M) ** 2, x=mx, z=mz, peak=peak, where=where, inside=inside,
                            cellxy=(int(mx // P), int(mz // P))))
patches.sort(key=lambda p: -p["area"])
inner = [p for p in patches if p["inside"] and p["area"] >= 400]
for n, p in enumerate(inner[:30], 1):
    x, y = TT(p["x"], p["z"])
    dr.ellipse([x - 13, y - 13, x + 13, y + 13], fill=(20, 20, 24), outline=(255, 240, 200), width=2)
    w = dr.textlength(str(n), font=font); dr.text((x - w / 2, y - 9), str(n), fill=(255, 240, 200), font=font)
# legend
lx, ly = 12, img.height - 110
dr.rectangle([lx - 6, ly - 8, lx + 330, ly + 100], fill=(25, 25, 28))
for k in range(200):
    tt = k / 199; ii = min(int(tt * 3), 2); ff = tt * 3 - ii; c = ramp[ii] * (1 - ff) + ramp[ii + 1] * ff
    dr.line([(lx + k, ly + 30), (lx + k, ly + 48)], fill=tuple(int(v) for v in c))
dr.text((lx, ly), "Empty ground: metres to the nearest thing", fill=(240, 235, 220), font=font)
dr.text((lx, ly + 52), f"{NEAR:.0f} m", fill=(220, 220, 210), font=small); dr.text((lx + 175, ly + 52), f"{FAR:.0f}+ m", fill=(220, 220, 210), font=small)
dr.text((lx, ly + 72), "dark grey: built, paved or planted   numbers: patches below", fill=(200, 200, 190), font=small)
os.makedirs(os.path.dirname(OUT), exist_ok=True)
img.save(OUT)
print("wrote", os.path.relpath(OUT, ROOT), img.size)
print("%-3s %-8s %-6s %-9s %s" % ("#", "area m2", "peak", "grid cell", "where"))
for n, p in enumerate(inner[:30], 1):
    print("%-3d %-8d %-6.0f %-9s %s" % (n, p["area"], p["peak"], "%d,%d" % p["cellxy"], p["where"]))
