"""Shrinks the three malls' car parks to about a quarter of their cars (WLD-054, owner 2026-11-23: "Malls to ~1/4, bus
stop"), lays a paved forecourt in front of each mall's door and plants the rest of the old car park with trees. The bus
stop beside each car park comes from World/city_01/transit.json (CITY-091), not from here.

Run once:  python shrink_carparks_01.py      (a car park already shrunk is left as it is)
Then:      python -m unittest test_world  and  python build_play.py
"""
import json, os, random

HERE = os.path.dirname(os.path.abspath(__file__))
BLOCKS = os.path.normpath(os.path.join(HERE, "..", "..", "World", "city_01", "blocks"))

# block: new car park (centre, size, rows, cars), the forecourt (x0, z0, x1, z1), where the trees go (x0, z0, x1, z1), species.
# Each new car park keeps the end of its road (roads.json links) on its edge: Westgate Road at (356, 905.5), Eastgate Road at
# (1564, 905.5), Northgate Road at (960, 280).
PLAN = {
    "b_1_5": dict(carpark=([326.0, 895.0], [60, 28], 4, 40), forecourt=(170.0, 863.0, 310.0, 879.0),
                  trees=[(132.0, 920.0, 350.0, 1028.0), (132.0, 884.0, 288.0, 910.0)], species="linden"),
    "b_16_5": dict(carpark=([1594.0, 895.0], [60, 28], 4, 40), forecourt=(1610.0, 863.0, 1750.0, 879.0),
                   trees=[(1570.0, 920.0, 1788.0, 1028.0), (1632.0, 884.0, 1788.0, 910.0)], species="linden"),
    "b_7_0": dict(carpark=([960.0, 258.0], [80, 28], 4, 50), forecourt=(900.0, 210.0, 1020.0, 242.0),
                  trees=[(708.0, 212.0, 880.0, 268.0), (1040.0, 212.0, 1212.0, 268.0)], species="plane"),
}
TREE_GAP = 16.0                     # metres between the new trees, in rows


def shrink(bid, plan):
    path = os.path.join(BLOCKS, bid + ".json")
    with open(path, encoding="utf-8") as f:
        b = json.load(f)
    cp = next(p for p in b["props"] if p["kind"] == "carpark")
    if cp["cars"] <= plan["carpark"][3]:
        return "%s: already shrunk" % bid
    pos, size, rows, cars = plan["carpark"]
    cp.update(pos=pos, size=size, rows=rows, cars=cars)
    x0, z0, x1, z1 = plan["forecourt"]
    b["props"].append(dict(kind="paving", pos=[(x0 + x1) / 2, (z0 + z1) / 2], size=[x1 - x0, z1 - z0]))
    rng = random.Random("trees " + bid)
    n0 = n = len(b["trees"])
    for (tx0, tz0, tx1, tz1) in plan["trees"]:
        z, row = tz0, 0
        while z <= tz1 + 1e-6:
            x = tx0 + (TREE_GAP / 2 if row % 2 else 0.0)
            while x <= tx1 + 1e-6:
                b["trees"].append(dict(id="%s_t%d" % (bid, n), species=plan["species"], pos=[round(x, 2), round(z, 2)],
                                       planted=-round(rng.uniform(4, 18) * 48), seed=rng.randrange(10 ** 6)))
                n += 1
                x += TREE_GAP
            z += TREE_GAP * 0.87
            row += 1
    with open(path, "w", encoding="utf-8") as f:
        json.dump(b, f, ensure_ascii=False, indent=1)
    return "%s: car park %s m for %d cars, a forecourt and %d new trees" % (bid, size, cars, n - n0)


if __name__ == "__main__":
    for bid in sorted(PLAN):
        print(shrink(bid, PLAN[bid]))
