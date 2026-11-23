"""The approved colourways' colours become colours of the building kit (ADR-0036; BUILD-001 to BUILD-003, BUILD-007,
BUILD-016).

A building's design names its colours by id from World/rules/building_kit.json "colours" (design v2). The colourways of
World/rules/building_styles.json are palette colours, most of which the kit has already (cool plaster is its limewash,
dusty blue its blue, wood red its oxblood ...). This
  - adds the ones it has not, under their own names (matched by rgb, within 0.02),
  - puts each in the kit's lists of the slots it is used for (plaster, roof_tiles, accents, frames) and in the town's own
    lists of the same slots (BUILD-016: the town picks from the same colours as the kit offers),
  - and lets every district of World/rules/planning.json offer the new plasters and roof tiles (BUILD-007), as each
    offers the town's own,
so the colourways are kit choices like any other and the building designer offers them too. Nothing already there is
changed or taken away; both files keep their own layout (small edits).
Run from Tools/worldgen:   python add_kit_colours_01.py      (running it again writes the same)
"""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
RULES = os.path.join(HERE, "..", "..", "World", "rules")
KIT_P, PLAN_P = os.path.join(RULES, "building_kit.json"), os.path.join(RULES, "planning.json")
SLOTS = ("plaster", "roof_tiles", "accents", "frames")


def needed(S):
    """(slot, name, rgb) of every colour the colourways use; slot None where the kit has no list for it (shutters)."""
    out = []
    for ways in S["colourways"].values():
        for c in ways.values():
            if c.get("wall") and c["wall"] != "stone":
                out.append(("plaster", c["wall"], S["plasters"][c["wall"]][1]))
            if c.get("roof"):
                lo, hi = S["roofs"][c["roof"]]
                out.append(("roof_tiles", "tiles_" + c["roof"], [round((a + b) / 2, 3) for a, b in zip(lo, hi)]))
            for part, slot in (("shutter", None), ("frame", "frames"), ("front", "accents")):
                if c.get(part):
                    out.append((slot, c[part], S["palette"][c[part]]))
            for k in c.get("awning") or []:
                out.append(("accents", k, S["palette"][k]))
    return out


def find(colours, rgb):
    for cid, v in colours.items():
        if all(abs(a - b) < .02 for a, b in zip(v["rgb"], rgb)):
            return cid
    return None


def set_list(txt, head, start, ids):
    """Append to the JSON list that follows `head` (searched from `start`) the ids it lacks."""
    i = txt.index(head, start) + len(head)
    j = txt.index("]", i)
    have = json.loads("[" + txt[i:j] + "]")
    more = [c for c in ids if c not in have]
    return txt if not more else txt[:j] + "".join(", " + json.dumps(c) for c in more) + txt[j:]


def main():
    K = json.load(open(KIT_P, encoding="utf-8"))
    S = json.load(open(os.path.join(RULES, "building_styles.json"), encoding="utf-8"))
    colours, added, per_slot = dict(K["colours"]), [], {s: [] for s in SLOTS}
    for slot, name, rgb in needed(S):
        cid = find(colours, rgb)
        if cid is None:
            cid = name if name not in colours else "style_" + name
            colours[cid] = dict(name=name.replace("_", " ").capitalize(), rgb=[round(v, 3) for v in rgb])
            added.append(cid)
        if slot and cid not in per_slot[slot]:
            per_slot[slot].append(cid)
    txt = open(KIT_P, encoding="utf-8").read()
    if added:                                                # the new colours at the end of "colours"
        end = txt.index("\n }", txt.index('"colours": {'))
        txt = txt[:end] + "".join(',\n  %-18s{"name": %-17s "rgb": %s}' % ('"%s":' % c, json.dumps(colours[c]["name"]) + ",", json.dumps(colours[c]["rgb"])) for c in added) + txt[end:]
    town = txt.index('\n "town"')
    for slot in SLOTS:
        txt = set_list(txt, '\n "%s": [' % slot, 0, per_slot[slot])            # the kit's list
        txt = set_list(txt, '\n  "%s": [' % slot, town, per_slot[slot])        # the town's own list
    json.loads(txt)
    with open(KIT_P, "w", encoding="utf-8", newline="\n") as f:
        f.write(txt)
    ptxt, P = open(PLAN_P, encoding="utf-8").read(), json.load(open(PLAN_P, encoding="utf-8"))
    dists = ptxt.index('"districts"')
    for name in P["districts"]:
        at = ptxt.index('"%s"' % name, dists)
        for slot in ("plaster", "roof_tiles"):
            ptxt = set_list(ptxt, '"%s": [' % slot, at, per_slot[slot])
    json.loads(ptxt)
    with open(PLAN_P, "w", encoding="utf-8", newline="\n") as f:
        f.write(ptxt)
    print("added %d colours (%s); slots: %s" % (len(added), ", ".join(added), "; ".join("%s %s" % (s, ",".join(v)) for s, v in per_slot.items())))


if __name__ == "__main__":
    main()
