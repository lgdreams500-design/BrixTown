"""Every building that is not a designed house gets its approved colourway (ADR-0036; BUILD-001).

For each such lot: lot.style = {"group": <its group in World/rules/building_styles.json>, "way": "a" | "b" | "c"}, the
group from its build type (Tools/worldgen/draw_builds.py classify()) and the colourway by a hash of the lot, picked once
here and kept in the lot, so loading the game never changes it. world.py design_of() then paints the building in that
colourway's colours (apply_style), and the slice builds it from its design as before. Designed houses have their own
colourways (add_houses_04.py); open places (gardens, the market, stops) have none.
Run from Tools/worldgen:   python add_styles_01.py      (running it again writes the same)
"""
import collections, hashlib, json, os

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
STYLES = json.load(open(os.path.join(HERE, "..", "..", "World", "rules", "building_styles.json"), encoding="utf-8"))
from draw_builds import classify                              # the build types of the builds page


def h(*k):
    return int(hashlib.md5("|".join(map(str, k)).encode()).hexdigest()[:8], 16) / 0xFFFFFFFF


def main():
    city = json.load(open(os.path.join(CITY, "city.json"), encoding="utf-8"))
    by, none = collections.Counter(), collections.Counter()
    for e in city["blocks"]:
        p = os.path.join(CITY, e["file"])
        b = json.load(open(p, encoding="utf-8"))
        for l in b["lots"]:
            if l.get("house"):
                continue
            key = classify(l, b)[0]
            group = STYLES["types"].get(key)
            if not group:
                none[key] += 1
                l.pop("style", None)
                continue
            ways = sorted(STYLES["colourways"][group])
            l["style"] = dict(group=group, way=ways[int(h(l["id"], "style") * len(ways)) % len(ways)])
            by[(group, l["style"]["way"])] += 1
        with open(p, encoding="utf-8") as f:
            end = "\n" if f.read().endswith("\n") else ""
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            json.dump(b, f, indent=1, ensure_ascii=False)
            f.write(end)
    print("%d buildings given a colourway in %d groups; no group (open places): %s" % (
        sum(by.values()), len({g for g, w in by}), ", ".join("%s %d" % kv for kv in sorted(none.items()))))


if __name__ == "__main__":
    main()
