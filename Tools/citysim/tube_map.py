"""Draws the glass travel tubes (World/city_01/tubes.json) on the town's map, for the owner to review the routes.
Needs Pillow.      python tube_map.py <out.png>

Grey: the lots, darker the taller. Thin lines: the streets and the bus lines. Teal: the tubes, with each stop's booth, its
name and the ride from Market Square in seconds (tubes.Ride). Red: where a tube passes over a building taller than the tube.
"""
import json, math, os, sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "worldgen"))
import world                                               # noqa: E402
from tubes import Ride                                     # noqa: E402

S = 0.6           # pixels a metre
PAD = 40


def seg_hits_rect(p, q, r):
    """Whether the segment p-q passes over the rect (x0, z0, x1, z1)."""
    x0, z0, x1, z1 = r
    for k in range(41):
        f = k / 40
        x, z = p[0] + (q[0] - p[0]) * f, p[1] + (q[1] - p[1]) * f
        if x0 <= x <= x1 and z0 <= z <= z1:
            return True
    return False


def main(out):
    w = world.load()
    net = json.load(open(os.path.join(world.WORLD, "city_01", "tubes.json"), encoding="utf-8"))
    rules = w["tubes"]
    storey = w["storeys"]["storey"] if isinstance(w["storeys"]["storey"], (int, float)) else 3.0
    lots = world.lots(w)
    xs = [v for l in lots for v in (l["rect"][0], l["rect"][2])] + [s["pos"][0] for s in net["stops"].values()]
    zs = [v for l in lots for v in (l["rect"][1], l["rect"][3])] + [s["pos"][1] for s in net["stops"].values()]
    X0, Z0 = min(xs) - 30, min(zs) - 30
    Wd, Ht = int((max(xs) + 30 - X0) * S) + 2 * PAD + 170, int((max(zs) + 30 - Z0) * S) + 2 * PAD + 140
    P = lambda x, z: (PAD + (x - X0) * S, PAD + (z - Z0) * S)
    im = Image.new("RGB", (Wd, Ht), (246, 242, 234))
    d = ImageDraw.Draw(im)
    try:
        font, big, small = ImageFont.truetype("segoeui.ttf", 15), ImageFont.truetype("segoeuib.ttf", 22), ImageFont.truetype("segoeui.ttf", 12)
    except OSError:
        font = big = small = ImageFont.load_default()
    roads = w["roads"]
    J = {j["id"]: j["pos"] for j in roads["junctions"]}
    for s in roads["streets"]:
        d.line([P(*J[s["a"]]), P(*J[s["b"]])], fill=(214, 206, 192), width=int(16 * S))
    for ln in roads["links"]:
        d.line([P(*p) for p in ln["points"]], fill=(222, 214, 200), width=int(9 * S))
    for l in lots:
        x0, z0, x1, z1 = l["rect"]
        f = l.get("floors") or 0
        g = max(120, 205 - f * 9)
        d.rectangle([P(x0, z0), P(x1, z1)], fill=(g, g - 4, g - 10))
    tr = w["transit"]
    BS = {s["id"]: s["at"] for s in tr["stops"]}
    for L in tr["lines"]:
        d.line([P(*BS[s]) for s in L["stops"]], fill=(170, 140, 110), width=2)
    # the tubes, and where one runs over a building taller than itself
    over = []
    for t in net["tubes"]:
        pts = [(p[0], p[1]) for p in t["line"]]
        d.line([P(*p) for p in pts], fill=(24, 130, 132), width=7, joint="curve")
        for p, q in zip(pts, pts[1:]):
            if math.dist(p, q) < 1:
                continue
            for l in lots:
                top = (l.get("floors") or 0) * storey + 2.0          # its roof
                if top + 1.0 > min(t["line"][0][2], t["line"][-1][2]) - rules["tube_inner_m"] / 2 and seg_hits_rect(p, q, l["rect"]):   # less than 1 m over its roof
                    over.append((l, p, q))
    for l, p, q in over:
        x0, z0, x1, z1 = l["rect"]
        d.rectangle([P(x0, z0), P(x1, z1)], outline=(200, 40, 30), width=3)
    for j in net["junctions"].values():
        x, z = P(j["pos"][0], j["pos"][1])
        d.ellipse([x - 4, z - 4, x + 4, z + 4], fill=(24, 130, 132))
    for sid, s in net["stops"].items():
        x, z = P(*s["pos"])
        d.ellipse([x - 11, z - 11, x + 11, z + 11], fill=(250, 248, 240), outline=(24, 130, 132), width=4)
        secs = "" if sid == "market_square" else "  %d s" % round(Ride(net, rules, "market_square", sid).duration)
        d.text((x + 15, z - 10), s["name"] + secs, fill=(20, 40, 40), font=font, stroke_width=3, stroke_fill=(246, 242, 234))
    # a key
    y = Ht - 125
    d.text((PAD, y), "Glass travel tubes: proposed routes (World/city_01/tubes.json)", fill=(20, 40, 40), font=big)
    d.line([(PAD, y + 45), (PAD + 40, y + 45)], fill=(24, 130, 132), width=7)
    d.text((PAD + 50, y + 36), "tube, 9 m over the street", fill=(20, 40, 40), font=font)
    d.ellipse([PAD + 260, y + 34, PAD + 282, y + 56], fill=(250, 248, 240), outline=(24, 130, 132), width=4)
    d.text((PAD + 292, y + 36), "stop: name and the ride from Market Square", fill=(20, 40, 40), font=font)
    d.line([(PAD + 650, y + 45), (PAD + 690, y + 45)], fill=(170, 140, 110), width=2)
    d.text((PAD + 700, y + 36), "bus lines", fill=(20, 40, 40), font=font)
    d.rectangle([PAD + 800, y + 36, PAD + 820, y + 54], outline=(200, 40, 30), width=3)
    d.text((PAD + 830, y + 36), "building the tube would pass over (%d)" % len({l["id"] for l, _, _ in over}), fill=(20, 40, 40), font=font)
    total = sum(sum(math.dist(p[:2], q[:2]) for p, q in zip(t["line"], t["line"][1:])) for t in net["tubes"])
    d.text((PAD, y + 70), "%d stops, %.1f km of tube. Darker lots are taller buildings. Rides at up to %d m/s (%d km/h)." %
           (len(net["stops"]), total / 1000, rules["ride"]["speed_mps"], rules["ride"]["speed_mps"] * 3.6), fill=(60, 70, 70), font=small)
    im.save(out)
    print("wrote %s (%d x %d); %d buildings under a tube: %s" % (out, Wd, Ht, len({l['id'] for l, _, _ in over}),
          ", ".join(sorted({"%s %s (%s floors)" % (l["id"], l.get("name", ""), l.get("floors")) for l, _, _ in over}))))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "tube_map.png")
