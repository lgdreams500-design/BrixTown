"""Widens every lane of the communities outside the wall to WIDTH metres (the owner, 2026-11-23: "make the roads wider near
the independent houses they are too narrow"), the width of the roads out of the gates. Every house keeps its least garden
and a 2 m verge at 9 m (the nearest house centre is 13 m from a lane's centre line).

Then run, from Tools/worldgen, so the gardens and the street dressing follow the new edges:
    python add_houses_03.py
    python add_lanes_dressing_01.py
"""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
WIDTH = 9.0


def main():
    with open(os.path.join(CITY, "city.json"), encoding="utf-8") as f:
        city = json.load(f)
    for e in city["blocks"]:
        path = os.path.join(CITY, e["file"])
        with open(path, encoding="utf-8") as f:
            b = json.load(f)
        if not b.get("lanes"):
            continue
        for ln in b["lanes"]:
            ln["width"] = WIDTH
        with open(path, "w", encoding="utf-8") as f:
            json.dump(b, f, ensure_ascii=False, indent=1)
        print("%s: %d lanes now %.0f m wide" % (b["id"], len(b["lanes"]), WIDTH))


if __name__ == "__main__":
    main()
