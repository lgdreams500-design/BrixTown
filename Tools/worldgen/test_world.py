"""Checks on the city data in World/ and on the living-city rules running over it.   python -m unittest test_world"""
import os
import unittest

import world

W = world.load()
CITY = world.simulation(W)
DAY = 16
LOTS = world.lots(W)


def old_town(lots):
    """The lots inside the line where Old Town's wall stood (city.json "wall", WLD-055)."""
    P, (i0, j0, i1, j1) = W["city"]["grid"]["pitch"], W["city"]["wall"]["cells"]
    return [l for l in lots if i0 * P <= l["rect"][0] and l["rect"][2] <= i1 * P and j0 * P <= l["rect"][1] and l["rect"][3] <= j1 * P]
BUILDINGS = [l for l in LOTS if l["kind"] not in world.OPEN_KINDS]


class TheMap(unittest.TestCase):
    def test_data_is_sound(self):
        self.assertEqual(world.validate(W), [])

    def test_eight_by_eight_blocks_each_cell_once(self):
        """WLD-024: Old Town is 8x8 one-cell blocks inside the line where its wall stood (city.json "wall"); since the
        remodel (WLD-020, WLD-021) the map round it is 20x16, and every cell of it is in exactly one block (WLD-056)."""
        g, (i0, j0, i1, j1) = W["city"]["grid"], W["city"]["wall"]["cells"]
        self.assertEqual((i1 - i0, j1 - j0), (8, 8))
        cells = lambda b: [(b["grid"][0] + a, b["grid"][1] + c) for a in range(b.get("span", [1, 1])[0]) for c in range(b.get("span", [1, 1])[1])]
        inside = [b for b in W["blocks"] if any(i0 <= i < i1 and j0 <= j < j1 for i, j in cells(b))]
        self.assertTrue(all(b.get("span", [1, 1]) == [1, 1] for b in inside), "Old Town's blocks are one cell each")
        self.assertEqual(sorted(tuple(b["grid"]) for b in inside), [(i, j) for i in range(i0, i1) for j in range(j0, j1)])
        every = [c for b in W["blocks"] for c in cells(b)]
        self.assertEqual(sorted(every), [(i, j) for i in range(g["columns"]) for j in range(g["rows"])])   # each cell once

    def test_key_amenities_exist(self):
        names = {l["name"] for l in LOTS}
        for must in ("Market Square", "Town Hall", "Lantern Arcade", "Mill Street Hospital"):
            self.assertIn(must, names)
        count = lambda kind: sum(l["kind"] == kind for l in LOTS)
        self.assertGreaterEqual(count("home"), 300)
        self.assertGreaterEqual(count("shop"), 20)
        self.assertGreaterEqual(count("food"), 20)
        self.assertGreaterEqual(count("health"), 5)
        self.assertGreaterEqual(count("garden"), 20)
        self.assertGreaterEqual(count("stop"), 4)
        self.assertGreaterEqual(count("utility"), 2)
        towers = [l for l in LOTS if l.get("tower")]
        self.assertGreaterEqual(len(towers), 5)
        self.assertTrue(all(t["floors"] >= 12 for t in towers))

    def test_nearly_every_building_can_be_entered(self):
        self.assertGreaterEqual(sum(l["enterable"] for l in BUILDINGS) / len(BUILDINGS), 0.95)

    def test_every_building_you_can_enter_has_something_to_use(self):
        for l in BUILDINGS:
            if l["enterable"]:
                self.assertTrue(any(l["objects"].values()), l["id"])

    def test_a_block_added_by_hand_is_checked(self):
        import copy
        w = copy.deepcopy(W)
        w["blocks"][0]["lots"][0]["door"] = [w["blocks"][0]["lots"][0]["door"][0] + 3.3, w["blocks"][0]["lots"][0]["door"][1] + 3.3]
        self.assertTrue(any("door is not on a wall" in p for p in world.validate(w)))


class TheResidents(unittest.TestCase):
    def test_everyone_has_a_home_and_a_role(self):
        homes = {l["id"] for l in LOTS if l["kind"] == "home"}
        for r in W["residents"]:
            self.assertIn(r["home"], homes)
            self.assertIn(r["role"], W["roles"]["roles"])

    def test_WLD_027_bank_showroom_library_museum_cinema_and_second_hospital_and_mall(self):
        kinds = {k: [l for l in LOTS if l["kind"] == k] for k in ("bank", "dealer", "library", "museum", "cinema")}
        for k, ls in kinds.items():
            self.assertEqual(len(ls), 1, k)
            self.assertTrue(ls[0]["enterable"] and ls[0]["objects"].get("work"), k)
        self.assertGreaterEqual(sum("hospital" in l.get("tags", []) for l in LOTS) + 1, 2)          # Mill Street Hospital + one more
        self.assertGreaterEqual(sum("mall" in l.get("tags", []) for l in LOTS) + 1, 2)              # Lantern Arcade + one more
        staffed = {r["work"] for r in W["residents"] if r["work"]}
        visited = {s["place"] for pieces in CITY.plan(DAY).values() for kind, t0, t1, payload, s in pieces if kind == "stay"}
        for k, ls in kinds.items():
            self.assertIn(ls[0]["id"], staffed, k)
            self.assertTrue(any(r["near"].get(k) == ls[0]["id"] for r in W["residents"]), k)
        for k in ("library", "museum", "cinema", "bank"):
            self.assertIn(kinds[k][0]["id"], visited, k + " gets visitors in a day")

    def test_every_workplace_is_staffed(self):
        staffed = {r["work"] for r in W["residents"] if r["work"]}
        for l in BUILDINGS:
            if l["enterable"] and l["kind"] in ("shop", "food", "health"):
                self.assertIn(l["id"], staffed, l["name"])

    def test_the_day_has_no_gaps_and_starts_and_ends_in_bed(self):
        out = 0
        for rid, pieces in CITY.plan(DAY).items():
            self.assertAlmostEqual(pieces[0][1], 0.0)
            self.assertAlmostEqual(pieces[-1][2], 1440.0)
            for a, b in zip(pieces, pieces[1:]):
                self.assertAlmostEqual(a[2], b[1], places=6)
            self.assertEqual(pieces[0][4]["place"], CITY.res[rid]["home"])
            if pieces[-1][0] == "move":                                  # still on the way at midnight, at their own pace
                out += 1
            else:
                self.assertEqual(pieces[-1][4]["place"], CITY.res[rid]["home"])
        self.assertLessEqual(out, len(CITY.res) * .02)                   # a few are out late, as in life (the owner, 2026-11-24)

    def test_nobody_walks_faster_than_their_own_pace(self):
        for rid, pieces in CITY.plan(DAY).items():
            v = CITY.speed(rid) * CITY.pace
            for kind, t0, t1, payload, s in pieces:
                if kind == "move":
                    self.assertLessEqual(payload[2], v * 1.01, rid)

    def test_most_walks_arrive_on_time(self):
        late = [t1 - s["start"] for pieces in CITY.plan(DAY).values() for kind, t0, t1, payload, s in pieces if kind == "move" and payload[1][-1] > 5]
        self.assertGreaterEqual(sum(l < 2 for l in late) / len(late), 0.8)

    def test_the_city_sleeps_at_night_and_is_out_by_day(self):
        night = CITY.at(DAY * 1440 + 3 * 60)                         # a few may be out, as in life (the owner, 2026-11-24)
        self.assertLessEqual(sum(s["state"] != "inside" for s in night.values()), len(night) * .01)
        noon = CITY.at(DAY * 1440 + 12.5 * 60)
        self.assertGreater(sum(s["state"] in ("walking", "outside") for s in noon.values()), 80)

    def test_only_nearby_residents_get_bodies(self):
        state = CITY.at(DAY * 1440 + 18 * 60)
        spawn = tuple(W["city"]["spawn"]["pos"])
        near = CITY.loaded(state, [spawn], radius=85.0)
        self.assertTrue(0 < len(near) < len(state) / 4)


class TheDressing(unittest.TestCase):
    """WLD-028 to WLD-034: street dressing (F-MAP-DRESS level 2; Tools/worldgen/add_dressing_01.py, World/rules/dressing.json)."""
    PROPS = [p for b in W["blocks"] for p in b["props"] if p.get("set") == "dress"]

    def count(self, kind):
        return sum(p["kind"] == kind for p in self.PROPS)

    def test_WLD_028_dressing_is_sound(self):
        self.assertEqual(world.dressing_problems(W), [])
        import copy
        w = copy.deepcopy(W)
        bad = next(p for b in w["blocks"] for p in b["props"] if p.get("set") == "dress" and p["kind"] == "bin")
        bad["kind"], bad["pos"] = "trampoline", [-500, -500]
        p = world.dressing_problems(w)
        self.assertTrue(any("unknown dressing trampoline" in x for x in p))
        self.assertTrue(any("outside its block" in x for x in p))

    def test_WLD_029_parks_courts_and_squares(self):
        parks = [l for l in old_town(LOTS) if l["name"].endswith(" Park")]     # the four big parks are Old Town's (Town Park is newer)
        self.assertEqual(len(parks), 4)
        for l in parks:
            for k in world.PLAY_USES + ("notice",):
                self.assertTrue(l["objects"].get(k), (l["name"], k))
        for k in ("playground", "swing", "slide", "sandpit", "climber"):
            self.assertEqual(self.count(k), 4, k)
        self.assertEqual(self.count("seesaw"), 8)
        self.assertEqual(self.count("bandstand"), 1)
        courts = [l for l in old_town(LOTS) if l["kind"] == "garden" and not l["name"].endswith(" Park")]
        pieces = [p for p in self.PROPS if p["kind"] in ("sundial", "well", "pergola", "statue")]
        self.assertGreaterEqual(len(pieces), len(courts) + 1)                   # one in each court, and the Town Hall's statue
        market = next(l for l in LOTS if l["kind"] == "market")
        self.assertTrue(market["objects"].get("kiosk") and market["objects"].get("notice"))
        self.assertTrue(any(p["kind"] == "fountain" and p.get("grand") for b in W["blocks"] for p in b["props"]))

    def test_WLD_040_every_garden_is_planted(self):
        """WLD-040, CITY-017: every park and court garden has hedges, flower borders, shrubs and bulbs, from data; each
        border has a place for the gardener, and nothing is planted on a seat, outside a garden or of an unknown kind."""
        self.assertEqual(world.garden_problems(W), [])
        for l in [l for l in LOTS if l["kind"] == "garden"]:
            mine = [p for b in W["blocks"] for p in b["props"] if p.get("set") == "garden"
                    and l["rect"][0] <= p["pos"][0] <= l["rect"][2] and l["rect"][1] <= p["pos"][1] <= l["rect"][3]]
            shapes = {p["shape"] for p in mine}
            self.assertTrue({"hedge", "border", "bush"} <= shapes, (l["name"], l["id"], shapes))
            if l["name"].endswith(" Park"):
                self.assertIn("drift", shapes, l["id"])
            borders = [p for p in mine if p["shape"] == "border"]
            self.assertEqual(len(l.get("tend_borders", [])), len(borders), l["id"])
            for q in l["tend_borders"]:
                self.assertIn(q, l["objects"]["flowerbed"])
        import copy
        w = copy.deepcopy(W)
        bad = next(p for b in w["blocks"] for p in b["props"] if p.get("set") == "garden" and p["shape"] == "bush")
        bad["species"], bad["pos"] = "triffid", [-500, -500]
        p = world.garden_problems(w)
        self.assertTrue(any("unknown plant triffid" in x for x in p))
        self.assertTrue(any("is not in a garden" in x for x in p))

    def test_CITY_017_gardeners_tend_the_borders(self):
        """CITY-017: on the day the slice shows, gardeners are at work at a garden's places, the borders' among them."""
        spots = {(round(q[0], 2), round(q[1], 2)) for l in LOTS if l["kind"] == "garden" for q in l.get("tend_borders", [])}
        seen = set()
        for m in range(7 * 60, 17 * 60, 5):
            for rid, s in CITY.at(DAY * 1440 + m).items():
                if s.get("act") == "tend" and s.get("pos"):
                    seen.add((round(s["pos"][0], 2), round(s["pos"][1], 2)))
        self.assertTrue(seen & spots, "no gardener tended a border")

    def test_WLD_030_buildings(self):
        self.assertGreaterEqual(self.count("advert"), 15)
        self.assertGreaterEqual(self.count("hoarding"), 3)
        homes = [l for l in LOTS if l["kind"] == "home"]
        boxes = lambda l: l.get("window_boxes") or (l.get("house") or {}).get("trim", {}).get("window_boxes")   # a designed house: in its trim
        self.assertTrue(.3 < sum(bool(boxes(l)) for l in homes) / len(homes) < .6)

    def test_WLD_031_streets(self):
        for k, least in (("bin", 64), ("planter", 40), ("bikerack", 20), ("postbox", 10), ("phonebox", 3), ("bollard", 256)):
            self.assertGreaterEqual(self.count(k), least, k)
        self.assertEqual(self.count("timetable"), sum(p["kind"] == "shelter" for b in W["blocks"] for p in b["props"]))

    def test_WLD_032_children_play_after_school(self):
        kids = [r for r in W["residents"] if r["role"] == "child"]
        B = world._read(world.WORLD, "rules", "balance.json")["targets"]          # since BAL-004: the share the balance asks for
        self.assertLessEqual(abs(len(kids) / len(W["residents"]) - B["residents"]["child"]), B["residents"]["child"] * B["band"])
        self.assertEqual(len({r["near"]["playground"] for r in kids}), 4)
        self.assertIn("child", W["traffic"]["no_drive_roles"])
        state = CITY.at(DAY * 1440 + 16.5 * 60)
        playing = [rid for rid, s in state.items() if s["act"] in world.PLAY_USES and s["state"] != "walking" and s.get("slot") is not None]
        self.assertGreaterEqual(len(playing), 35)
        self.assertTrue(all(CITY.res[rid]["role"] == "child" for rid in playing))
        morning = CITY.at(DAY * 1440 + 11 * 60)
        self.assertFalse(any(s["act"] in world.PLAY_USES for s in morning.values()))   # at school on a weekday

    def test_WLD_033_notices_and_the_kiosk(self):
        stays = [s for pieces in CITY.plan(DAY).values() for kind, t0, t1, payload, s in pieces if kind == "stay"]
        self.assertTrue(any(s["act"] == "notices" and s.get("slot") is not None for s in stays))
        self.assertTrue(any(s["act"] == "paper" and s.get("slot") is not None for s in stays))
        market = next(l for l in LOTS if l["kind"] == "market")
        self.assertTrue(any(s["act"] == "work" and s["place"] == market["id"] for s in stays))

    def test_WLD_034_what_changes_with_the_day(self):
        d = W["dressing"]
        a, b = world.dressing_today(W, 0), world.dressing_today(W, d["advert_every_days"])
        self.assertNotEqual(a["adverts"], b["adverts"])
        self.assertEqual(a["adverts"], world.dressing_today(W, d["advert_every_days"] - 1)["adverts"])
        self.assertGreater(len(set(a["adverts"].values())), 3)                  # the boards are out of step
        self.assertNotEqual(a["film"], world.dressing_today(W, d["film_every_days"])["film"])
        clock = CITY.clock
        days = range(0, 14)
        for day in days:
            self.assertEqual(world.dressing_today(W, day)["bunting"], bool(clock.info(day)["market"]))
        self.assertTrue(any(clock.info(day)["market"] for day in days) and not all(clock.info(day)["market"] for day in days))
        for p in self.PROPS:                                                     # never stored in the map
            self.assertFalse({"text", "film", "advert"} & set(p), p["id"])


class TheSurfaces(unittest.TestCase):
    """WLD-038, WLD-039: stone, brick and paving by rules (World/rules/surfaces.json, ADR-0017)."""
    S = world.surfaces(W)

    def name(self, k):
        return self.S["patterns"][k]

    def test_WLD_038_surfaces_are_sound(self):
        self.assertEqual(world.surface_problems(W), [])
        import copy
        w = copy.deepcopy(W)
        w["surfaces"]["patterns"]["glass"] = {"for": "wall", "type": "mosaic", "unit": [9, 9], "colors": []}
        w["surfaces"]["rules"]["pavement"].insert(0, {"if": {"colour": ["red"]}, "use": "rubble"})
        w["blocks"][0]["lots"][0]["surface"] = {"wall": "marble"}
        p = world.surface_problems(w)
        for bit in ("unknown type mosaic", "unit size out of range", "glass has no colours", "asks about colour", "rubble is made for wall", "marble does not exist"):
            self.assertTrue(any(bit in x for x in p), bit)

    def test_WLD_038_a_lot_can_name_its_own(self):
        import copy
        w = copy.deepcopy(W)
        lot = next(l for b in w["blocks"] for l in b["lots"] if b["template"] == "rows" and l["kind"] == "home")
        lot["surface"] = {"wall": "rusticated"}
        self.assertEqual(world.surfaces(w)["patterns"][world.surfaces(w)["lots"][lot["id"]]["wall"]], "rusticated")

    def test_WLD_039_surfaces_mix_by_district(self):
        walls = {}
        for b in W["blocks"]:
            for l in b["lots"]:
                s = self.S["lots"].get(l["id"])
                if s and "wall" in s:
                    walls.setdefault(b["template"], []).append(self.name(s["wall"]))
        self.assertGreater(walls["peri"].count("rubble"), len(walls["peri"]) / 2)
        self.assertGreater(sum(n.startswith("brick") for n in walls["rows"]), len(walls["rows"]) / 2)
        civic = [l for l in LOTS if l["kind"] in ("bank", "museum", "library") or l["name"] == "Town Hall" or "hospital" in l.get("tags", [])]
        self.assertTrue(all(self.name(self.S["lots"][l["id"]]["wall"]) == "rusticated" for l in civic))
        uppers = [s for s in self.S["lots"].values() if s.get("upper")]
        self.assertTrue(uppers and all(self.name(s["wall"]).startswith("brick") for s in uppers))
        grounds = {self.name(v["pavement"]) for v in self.S["blocks"].values()}
        self.assertTrue({"slabs", "flags", "herringbone", "basket"} <= grounds)
        market = next(l for l in LOTS if l["kind"] == "market")
        self.assertEqual(self.name(self.S["lots"][market["id"]]["ground"]), "setts")
        self.assertEqual(world.surfaces(W), self.S)                              # the same every time


class TheStoreys(unittest.TestCase):
    """WLD-050 to WLD-053: every floor of every building (F-BLD-INSIDE level 3; World/rules/storeys.json, ADR-0021)."""

    def test_WLD_050_storeys_are_sound(self):
        self.assertEqual(world.storey_problems(W), [])
        import copy
        w = copy.deepcopy(W)
        lots_ = [l for l in world.lots(w) if l.get("stair")]
        del lots_[0]["stair"]
        lots_[1]["stair"]["rect"][2] += 40
        lot = next(l for l in lots_[2:] if l.get("levels", {}).get("bed"))
        lot["levels"]["bed"][0] = 99
        p = world.storey_problems(w)
        for bit in ("floors and no stair", "stair is not inside the walls", "a bed on floor 99"):
            self.assertTrue(any(bit in x for x in p), bit)

    def test_WLD_050_every_building_can_be_entered_and_climbed(self):
        for l in BUILDINGS:
            self.assertTrue(l["enterable"], l["id"])
            self.assertEqual(len(l["storeys"]), l["floors"], l["id"])
            if l["floors"] >= 2:
                self.assertTrue("stair" in l or world.house_stair(l), l["id"])   # a designed house's stair is in its plan

    def test_WLD_051_towers_have_a_lift(self):
        towers = [l for l in BUILDINGS if l.get("tower")]
        self.assertEqual(len(towers), 5)
        for l in towers:
            self.assertIn("lift", l, l["name"])

    def test_WLD_052_inside_is_the_size_of_outside(self):
        for n in range(2, 23):
            top = n * 3 + 0.4                                         # the height a building has from outside (play.js building())
            self.assertAlmostEqual(world.storey_floor(n - 1) + 2.8 + 0.2, top)   # top floor, its ceiling, the roof slab
            for f in range(1, n):
                self.assertAlmostEqual(world.storey_floor(f) - world.storey_floor(f - 1), 3.2 if f == 1 else 3.0)
        uses = set(W["storeys"]["uses"]) - {"default"}
        self.assertTrue({"home", "flats", "work", "shop"} <= uses)

    def test_WLD_053_residents_sleep_upstairs(self):
        lots_ = {l["id"]: l for l in LOTS}
        state = CITY.at(DAY * 1440 + 3 * 60)
        up = total = 0
        for rid, s in state.items():
            l = lots_[s["place"]]
            if l["kind"] == "home" and l["floors"] >= 2 and s.get("use") == "bed" and s.get("slot") is not None:
                total += 1
                f = l["levels"]["bed"][s["slot"]]
                flat = next((q for q in l.get("flats", []) if s["slot"] in q["beds"]), None)   # flats on every floor (BAL-006):
                up += f == flat["floor"] if flat else f >= 1                                  # the bed is on its own flat's floor
        self.assertGreater(total, 500)
        self.assertEqual(up, total)
        offices = [l for l in LOTS if l["kind"] == "work" and l["floors"] >= 2 and l["objects"].get("work")]
        self.assertTrue(any(f >= 1 for l in offices for f in l.get("levels", {}).get("work", [])))

    def test_WLD_053_a_stay_upstairs_carries_its_floor(self):
        """Stacked flats and offices have places at the same spot on each floor, so the slice's day plan gives a stay
        upstairs its floor (build_play.routines) and the slice finds the place by spot and floor (play.js resident)."""
        import collections, build_play
        plan, acts = build_play.routines(CITY, DAY, collections.defaultdict(int))
        lots_, seen = CITY.d["lots"], 0
        for row in plan:
            for p in row:
                if p[1] != 0 or p[5] < 0:
                    continue
                l, use = lots_[p[7]], build_play.USES[p[5]]
                floors = {f for q, f in zip(l["objects"].get(use, []), (l.get("levels") or {}).get(use, [])) if abs(q[0] - p[2]) < .2 and abs(q[1] - p[3]) < .2}
                f = p[8] if len(p) > 8 else 0
                self.assertTrue(f in floors or (not floors and f == 0), (l["id"], use, p[2], p[3], f, floors))
                seen += len(floors) > 1
        self.assertGreater(seen, 100)                                    # stays at a spot used on more than one floor


class TheDesigns(unittest.TestCase):
    """BUILD-001 to BUILD-003: every building is a design (docs/specs/domains/BUILDER.md; World/rules/building_kit.json)."""
    D = world.designs(W)

    def test_BUILD_001_every_building_has_a_whole_design(self):
        self.assertEqual(set(self.D), {l["id"] for l in BUILDINGS})
        self.assertEqual(world.design_problems(W, self.D), [])
        cafe = self.D[next(l["id"] for l in LOTS if l["name"] == "Market Café")]
        self.assertEqual((cafe["shell"]["floors"], cafe["roof"]["type"], cafe["inside"]["uses"]), (2, "cornice", ["main", "store"]))
        self.assertGreater(cafe["openings"]["shop_front"], 0)

    def test_BUILD_002_designs_pick_as_the_slice_does(self):
        # the slice's hash() gives these (taken from Prototype/play/play.js in the browser); the design picks with the same
        for keys, want in ((("b_0_0_l0", 1), 0.5495072316844016), (("b_3_1_l2", "st"), 0.1645335874054581), (("b_6_1_l0", "ch"), 0.1171064767986536),
                           (("x", 2.5, "y"), 0.3554460408631712), (("b_1_1_l2", "shc"), 0.47174760745838284), (("Café Mina", "fr"), 0.34352535218931735)):
            self.assertEqual(world.js_hash(*keys), want, keys)
        K = W["building_kit"]
        d = self.D["b_0_0_l0"]
        lot = next(l for l in world.lots(W) if l["id"] == "b_0_0_l0")
        raw = world.design_of(W, next(b for b in W["blocks"] if lot in b["lots"]), lot)
        self.assertEqual(raw["walls"]["n"]["plaster"], world.js_pick(K["town"]["plaster"], 0.5495072316844016))   # the kit's own pick ...
        self.assertEqual(d["walls"]["n"]["plaster"], (world.style_ids(W, lot.get("style")) or {}).get("plaster", raw["walls"]["n"]["plaster"]))   # ... unless the lot's approved colourway sets it (ADR-0036)
        self.assertEqual(world.built(W, d)["walls"]["n"]["plaster"], K["colours"][d["walls"]["n"]["plaster"]]["rgb"])
        self.assertEqual(world.designs(W), self.D)                                   # the same every time

    def test_BUILD_003_every_choice_comes_from_the_kit(self):
        import copy
        w = copy.deepcopy(W)
        ds = copy.deepcopy(self.D)
        d = ds["b_0_0_l0"]
        d["walls"]["s"]["plaster"] = [1, 0, 1]                                      # a colour by value, not by id (BUILD-016)
        d["roof"]["type"] = "dome"
        d["walls"]["e"]["stone"]["pattern"] = "marble"
        d["inside"]["floors"][0] = "carpet"
        del ds["b_0_0_l1"]["ornament"]
        del ds["b_0_0_l2"]["walls"]["w"]
        p = world.design_problems(w, ds)
        for bit in ("s facade's plaster colour not in the kit", "roof type dome", "e facade's stone pattern not in surfaces.json", "design has no ornament",
                    "floor 0's finish not in the kit", "b_0_0_l2: no w facade"):
            self.assertTrue(any(bit in x for x in p), bit)


class TheDialogue(unittest.TestCase):
    """CITY-007: conversations with residents (World/rules/dialogue.json)."""
    D = W["dialogue"]

    def test_CITY_007_dialogue_is_sound(self):
        import build_play
        self.assertEqual(world.dialogue_problems(W, build_play.ACTS), [])

    def test_CITY_007_at_least_three_conversations_branch(self):
        branching = [c for c in self.D["conversations"].values() if sum(len(n["options"]) > 1 for n in c["nodes"].values()) >= 2]
        self.assertGreaterEqual(len(branching), 3)

    def pick(self, role, act, walking):           # the same matching play/conversations.js does: the first rule that fits
        for x in self.D["rules"]:
            if x.get("roles") and role not in x["roles"] or x.get("acts") and act not in x["acts"] or "walking" in x and x["walking"] != walking:
                continue
            return x["conversation"]

    def test_CITY_007_who_says_what(self):
        self.assertEqual(self.pick("nurse", "work", False), "clinic")
        self.assertEqual(self.pick("cafe", "work", False), "counter")
        self.assertEqual(self.pick("shopkeeper", "sell", False), "counter")
        self.assertEqual(self.pick("shopkeeper", "work", True), "passerby")
        self.assertEqual(self.pick("elder", "rest", False), "elder")
        self.assertEqual(self.pick("student", "rest", False), "chat")
        self.assertEqual(self.pick("child", "swing", False), "child")
        self.assertEqual(self.pick("child", "home", True), "child")      # walking home too: a child does not get the passer-by talk
        for role in W["roles"]["roles"]:
            self.assertIn(self.pick(role, "home", False), self.D["conversations"])

    def test_CITY_007_broken_dialogue_is_caught(self):
        import copy
        w = copy.deepcopy(W)
        c = w["dialogue"]["conversations"]["chat"]["nodes"]
        c["hello"]["options"][0]["next"] = "nowhere"          # a reply to a node that does not exist
        c["lonely"] = {"say": "Nobody reaches me.", "mood": "sad", "options": [{"text": "...", "next": "lonely"}]}   # unreachable, and never ends
        p = world.dialogue_problems(w)
        self.assertTrue(any("nowhere" in x for x in p))
        self.assertTrue(any("lonely: cannot be reached" in x for x in p))
        self.assertTrue(any("lonely: the talk can never end" in x for x in p))


class TheFaces(unittest.TestCase):
    """CHR-007, CHR-008, CHR-009: mouth shapes, expressions and lip sync rules (World/rules/speech.json)."""

    def test_CHR_009_speech_rules_are_sound(self):
        self.assertEqual(world.speech_problems(W), [])

    def test_CHR_007_mouth_shapes_match_the_face_document(self):
        sh = W["speech"]["shapes"]
        self.assertEqual((sh["AA"]["width"], sh["AA"]["opening"]), (0.0165, 0.0115))       # CHARACTER_FACE §2
        self.assertEqual((sh["OO"]["width"], sh["OO"]["round"]), (0.0070, 1))
        self.assertLess(sh["MBP"]["opening"], sh["rest"]["opening"])                       # lips pressed

    def test_CHR_008_every_dialogue_mood_has_a_face(self):
        moods = {n["mood"] for c in W["dialogue"]["conversations"].values() for n in c["nodes"].values()}
        self.assertTrue(moods <= set(W["speech"]["expressions"]))

    def test_CHR_009_broken_speech_rules_are_caught(self):
        import copy
        w = copy.deepcopy(W)
        del w["speech"]["shapes"]["FV"]
        del w["speech"]["expressions"]["sad"]
        p = world.speech_problems(w)
        self.assertTrue(any("FV" in x for x in p))
        self.assertTrue(any("mood sad" in x for x in p))


class TheSky(unittest.TestCase):
    """WLD-035, WLD-036: the sky and its clouds (World/rules/sky.json, drawn by Prototype/play/play.js)."""

    def test_WLD_035_sky_rules_are_sound(self):
        self.assertEqual(world.sky_problems(W), [])

    def test_WLD_035_a_broken_sky_is_caught(self):
        import copy
        bad = copy.deepcopy(W)
        bad["sky"]["zenith"] = [0.2, 0.4]
        bad["sky"]["clouds"]["cover"].pop("rain")
        bad["sky"]["clouds"]["wind"] = [0, 0]
        self.assertEqual(len(world.sky_problems(bad)), 3)

    def test_WLD_036_every_kind_of_weather_has_a_sky(self):
        import citysim
        city = world.simulation(W)
        seen = {citysim.weather(city.d, city.clock, t) for t in range(0, 48 * 1440, 360)}
        self.assertTrue(seen <= set(world.WEATHER_KINDS), seen)
        self.assertTrue(seen <= set(W["sky"]["clouds"]["cover"]), seen)

    def test_WLD_037_every_kind_of_weather_changes_the_light(self):
        wx = W["sky"]["weather"]
        self.assertEqual(set(wx), set(world.WEATHER_KINDS))
        self.assertEqual(wx["clear"], dict(sun=1.0, haze=1.0, veil=0.0, grey=0.0, rain=0.0))   # clear weather is the light as approved
        self.assertGreater(wx["clear"]["sun"], wx["cloud"]["sun"])
        self.assertGreater(wx["cloud"]["sun"], wx["rain"]["sun"])
        self.assertGreater(wx["fog"]["haze"], wx["rain"]["haze"])
        self.assertEqual([k for k in wx if wx[k]["rain"] > 0], ["rain"])

    def test_WLD_037_broken_weather_is_caught(self):
        import copy
        bad = copy.deepcopy(W)
        bad["sky"]["weather"]["fog"]["haze"] = 0.5        # thinner than clear air
        bad["sky"]["weather"]["rain"]["sun"] = 1.5        # out of range, and not less than clear: two problems
        bad["sky"]["rain"]["dry_minutes"] = 500
        del bad["sky"]["weather"]["cloud"]["grey"]
        self.assertEqual(len(world.sky_problems(bad)), 5)

    def test_WLD_042_night_sky_rules_are_sound(self):
        self.assertEqual(world.night_sky_problems(W), [])
        n = W["sky"]["night"]
        self.assertEqual(max(n["weather"], key=lambda k: n["weather"][k]["stars"]), "clear")   # a clear night shows the most sky
        self.assertLess(n["weather"]["clear"]["cover"], 1)                                     # and fewer clouds than the day

    def test_WLD_042_a_broken_night_sky_is_caught(self):
        import copy
        bad = copy.deepcopy(W)
        n = bad["sky"]["night"]
        n["zenith"] = [0.5, 0.5, 0.5]              # brighter than the horizon
        n["year_days"] = 40                        # not the calendar's year
        n["stars"]["density"] = 0.9                # out of range
        del n["weather"]["fog"]["cover"]
        self.assertEqual(len(world.night_sky_problems(bad)), 4)

    def test_WLD_043_the_sun_climbs_higher_in_summer(self):
        h = world.sun_heights(W)
        self.assertGreater(h["Summer"], h["Spring"])                 # long days: north of the equator, rising north of east
        self.assertLess(h["Winter"], h["Autumn"])                    # short days: south of it, rising south of east
        self.assertGreater(h["Summer"], 0)
        self.assertLess(h["Winter"], 0)
        import math
        la = math.radians(W["sky"]["night"]["latitude"])
        cal = W["calendar"]["calendar"]
        for s, name in enumerate(cal["seasons"]):                    # seen from the latitude, that height sets the sun at the calendar's sunset
            half = (cal["sunset"][s] - cal["sunrise"][s]) / 24 * math.pi, math.radians(h[name])
            alt = math.sin(la) * math.sin(half[1]) + math.cos(la) * math.cos(half[1]) * math.cos(half[0])
            self.assertAlmostEqual(alt, 0, places=9)

    def test_WLD_043_broken_paths_are_caught(self):
        import copy
        bad = copy.deepcopy(W)
        bad["sky"]["night"]["paths"]["sun_lift"] = 0.5
        del bad["sky"]["night"]["paths"]["moon_tilt"]
        self.assertEqual(len(world.night_sky_problems(bad)), 2)

    def test_WLD_065_the_planet_hangs_in_one_place_clear_of_the_sun(self):
        import math
        self.assertEqual(world.planet_problems(W), [])
        p = W["sky"]["planet"]
        seasons = W["calendar"]["calendar"]["seasons"]
        gaps = world.planet_sun_gap(W)
        self.assertEqual(set(gaps), set(seasons))
        self.assertGreater(min(gaps.values()), p["size"])                 # the sun never goes behind it, in any season
        self.assertGreater(math.radians(p["elevation"]), p["size"])       # all of it is above the horizon, always
        self.assertLessEqual(p["by_day"], .5)                             # by day it is only just there
        self.assertGreater(p["night"], 0)                                 # and after dark it comes out
        for name in seasons:                                              # its phase is the sun's doing: it waxes and wanes through each day
            lit = [world.planet_lit(W, name, 2 * math.pi * k / 96) for k in range(96)]
            self.assertTrue(all(0 <= v <= 1 for v in lit))
            self.assertGreater(max(lit) - min(lit), .4)
            self.assertAlmostEqual(min(lit), (1 - math.cos(min(world.planet_sun_angle(W, name, 2 * math.pi * k / 96) for k in range(96)))) / 2)

    def test_WLD_065_a_broken_planet_is_caught(self):
        import copy
        bad = copy.deepcopy(W)
        bad["sky"]["planet"].update(azimuth=180, elevation=50)            # due south on the sky's equator: the autumn sun goes straight behind it, the spring sun clips it
        self.assertEqual(len(world.planet_problems(bad)), 2)
        bad = copy.deepcopy(W)
        del bad["sky"]["planet"]["spin_hours"]
        bad["sky"]["planet"]["by_day"] = 2                                # out of range
        bad["sky"]["planet"]["rings"]["outer"] = 1.2                      # inside the inner edge
        self.assertEqual(len(world.planet_problems(bad)), 3)
        bad = copy.deepcopy(W)
        del bad["sky"]["planet"]                                          # a sky with no planet is still a sky
        self.assertEqual(world.sky_problems(bad), [])

    def test_WLD_010_the_slice_can_show_every_season(self):
        """The season key (Y) of the slice: one look for each season, on the same day of the season as the built day."""
        import build_play
        cal = W["calendar"]["calendar"]
        self.assertEqual(build_play.season_days(16, cal), [4, 16, 28, 40])
        looks = build_play.season_looks(W, world.simulation(W), 16)
        self.assertEqual(list(looks), cal["seasons"])
        for n, name in enumerate(cal["seasons"]):
            self.assertEqual(looks[name]["day"]["sunrise"], cal["sunrise"][n])
            self.assertTrue(set(looks[name]["day"]["weather"]) <= set(world.WEATHER_KINDS))
        leaf = lambda name: sum(t[3] for t in looks[name]["trees"])
        self.assertGreater(leaf("Summer"), leaf("Winter"))               # trees are in leaf in summer and bare in winter

    def test_WLD_036_the_slice_gets_the_sky(self):
        import build_play
        with open(build_play.__file__, encoding="utf-8") as f:
            self.assertIn('sky={k: v for k, v in w["sky"].items()', f.read())


class TheSounds(unittest.TestCase):
    """SND-001 to SND-013: how the city sounds (World/rules/sound.json, played by Prototype/play/sound.js)."""
    S = W["sound"]

    def test_SND_008_sound_rules_are_sound(self):
        import build_play
        self.assertEqual(world.sound_problems(W, build_play.ACTS), [])

    def test_SND_005_every_vowel_the_mouth_shows_has_a_voice(self):
        self.assertTrue({"AA", "EH", "EE", "OH", "OO"} <= set(self.S["voices"]["shapes"]))

    def test_SND_009_every_action_has_a_sound(self):
        for k in world.SOUND_ACTIONS:
            self.assertIn(k, self.S["actions"])

    def test_SND_019_a_reward_springs_coins_by_its_size(self):
        self.assertEqual(world.reward_sound_problems(self.S), [])
        counts = [world.reward_coins(self.S, a) for a in (1, 4999, 5000, 14999, 15000, 40000, 10 ** 7)]
        self.assertEqual(counts, sorted(counts))                            # a bigger reward never has fewer coins
        self.assertGreater(counts[-1], counts[0])
        tx = [(0, 1, 600, "Reward: A parcel", (("sys:rewards", -5000), ("p:cash", 5000))),
              (1, 1, 610, "Sold bread", (("shop:1", -200), ("p:cash", 200))),               # a sale is not a reward
              (2, 1, 620, "Loan L1 paid out", (("bank:vault", -9000), ("p:current", 9000))),   # nor is a loan
              (3, 1, 630, "Transfer", (("p:cash", -100), ("p:current", 100)))]
        self.assertEqual(world.reward_events(self.S, tx), [{"kind": "reward", "amount": 5000, "coins": world.reward_coins(self.S, 5000), "memo": "Reward: A parcel"}])

    def test_SND_020_interest_is_shown_once_for_all_accounts(self):
        tx = [(0, 1, 1440, "Interest, current account", (("bank:vault", -12), ("p:current", 12))),
              (1, 1, 1440, "Interest, savings account", (("bank:vault", -30), ("p:savings", 30))),
              (2, 2, 1440, "Interest, savings account", (("bank:vault", -31), ("p:savings", 31)))]
        self.assertEqual(world.reward_events(self.S, tx), [{"kind": "interest", "amount": 73, "accounts": ["p:current", "p:savings"]}])
        self.assertEqual(world.reward_events(self.S, []), [])

    def test_SND_019_broken_reward_rules_are_caught(self):
        import copy
        s = copy.deepcopy(self.S)
        r = s["rewards"]
        r["quest"]["voice"] = "gong"
        r["achievement"]["chord"]["notes"] = []
        r["voices"]["box"]["partials"] = [[1, 0]]
        r["coins"]["reward_tiers"] = [[100, 5], [50, 3]]
        r["coins"]["bounce"] = 1.5
        r["interest"]["end"] = []
        del r["ledger"]["interest_memo"]
        p = world.reward_sound_problems(s)
        for bit in ("not a reward voice: gong", "achievement.chord", "reward voice box", "reward_tiers", "bounce", "interest end note", "interest_memo"):
            self.assertTrue(any(bit in x for x in p), bit)
        del s["rewards"]
        self.assertTrue(any("no rewards" in x for x in world.reward_sound_problems(s)))

    def test_SND_011_cafes_shops_and_the_market_play_music(self):
        kinds = {l["kind"] for l in LOTS}
        for k in ("food", "shop", "market"):
            self.assertIn(k, self.S["music"]["places"])
            self.assertIn(k, kinds)
        self.assertTrue(self.S["music"]["places"]["market"]["market_days_only"])

    def test_SND_010_the_score_changes_through_the_day(self):
        moods = self.S["music"]["score"]["moods"]
        self.assertGreaterEqual(len(moods), 3)
        self.assertEqual(len({(m["bpm"], m["root"]) for m in moods}), len(moods))   # each part of the day sounds different

    def test_SND_012_the_radio_has_three_stations(self):
        self.assertGreaterEqual(len([s for s in self.S["music"]["radio"] if "style" in s]), 3)

    def test_SND_015_what_people_do_makes_a_sound(self):
        pe = self.S["people"]
        acts = {a for x in pe["activities"] for a in x["acts"]}
        for a in ("eat", "shop", "sell", "work", "read", "sleep", "swing", "slide", "seesaw", "sandpit", "climb"):
            self.assertIn(a, acts)
        jobs = {r for x in pe["activities"] if "work" in x["acts"] for r in x.get("roles", [])}
        for role in ("cafe", "shopkeeper", "clerk", "maker", "nurse", "gardener", "utility"):
            self.assertIn(role, jobs, role + " at work is silent")

    def test_SND_016_talk_is_turns_and_quiet_places_stay_quiet(self):
        c = self.S["people"]["chat"]
        self.assertLess(c["turn_gap_s"][1], c["pause_s"][0] + 1)           # a reply comes sooner than a new topic
        self.assertGreater(c["child_laugh"], c["laugh"])                   # children laugh more
        for quiet in ("read", "view", "watch", "sleep"):
            self.assertNotIn(quiet, c["acts"])

    def test_SND_017_children_sound_lighter(self):
        pe = self.S["people"]
        self.assertGreater(pe["pitch"]["child"][0], self.S["voices"]["pitch"]["max"] - 40)
        self.assertGreater(pe["steps"]["child_pitch"], 1)
        self.assertLess(pe["steps"]["child_softer"], 1)
        self.assertGreater(pe["steps"]["run_louder"], 1)

    def test_SND_015_broken_people_sounds_are_caught(self):
        import copy
        w = copy.deepcopy(W)
        pe = w["sound"]["people"]
        pe["activities"].append({"acts": ["juggle"], "every": [1, 2], "sound": "drum"})
        pe["activities"].append({"acts": ["eat"], "every": [5, 1], "roles": ["pirate"], "sound": "cup"})
        pe["sounds"]["bang"] = {"kind": "explosion", "hz": 100, "len": 1, "gain": .5}
        import build_play
        p = world.sound_problems(w, build_play.ACTS)
        for bit in ("unknown sound drum", "unknown activity juggle", "unknown role pirate", "bad interval", "unknown kind explosion"):
            self.assertTrue(any(bit in x for x in p), bit)

    def test_SND_010_the_piano_only_plays_chords(self):
        import copy
        self.assertIn("epiano", self.S["music"]["chords_only"])
        w = copy.deepcopy(W)
        w["sound"]["music"]["styles"]["cafe"]["voice"] = "epiano"                 # a tune on the piano
        w["sound"]["music"]["score"]["moods"][0].update(comp="epiano", comp_pattern="arp")   # piano arpeggios
        p = world.sound_problems(w)
        self.assertTrue(any("style cafe plays single notes on epiano" in x for x in p))
        self.assertTrue(any("morning plays single notes on epiano" in x for x in p))

    def test_SND_014_the_radio_has_podcasts(self):
        talk = [s for s in self.S["music"]["radio"] if "podcast" in s]
        self.assertGreaterEqual(len(talk), 2)
        for s in talk:
            sh = self.S["podcasts"]["shows"][s["podcast"]]
            self.assertGreaterEqual(len(sh["episodes"]), 2, s["name"])
            for ep in sh["episodes"]:
                self.assertGreaterEqual(len(ep["lines"]), 5, ep["title"])
                self.assertTrue(all(len(t) <= 110 for _, t in ep["lines"]), ep["title"] + ": a caption too long to read")
        self.assertTrue(any(len(self.S["podcasts"]["shows"][s["podcast"]]["hosts"]) >= 2 for s in talk))   # someone to talk to

    def test_SND_014_broken_podcasts_are_caught(self):
        import copy
        w = copy.deepcopy(W)
        s = w["sound"]
        s["music"]["radio"].append({"name": "Nowhere FM", "podcast": "nowhere"})
        s["podcasts"]["shows"]["tram_street"]["episodes"][0]["lines"].append(["ghost", "Boo."])
        s["podcasts"]["shows"]["quiet_hour"]["episodes"][0]["lines"].append(["oskar", "..."])
        p = world.sound_problems(w)
        for bit in ("unknown podcast nowhere", "unknown host ghost", "nothing to say"):
            self.assertTrue(any(bit in x for x in p), bit)

    def test_SND_007_outside_is_muffled_indoors(self):
        self.assertLess(self.S["indoors"]["muffle_hz"], 2000)
        self.assertLess(self.S["indoors"]["muffle_gain"], 1)

    def test_SND_008_broken_sound_rules_are_caught(self):
        import copy
        w = copy.deepcopy(W)
        s = w["sound"]
        del s["actions"]["lift"]
        s["voices"]["shapes"]["XX"] = [500, 1500]
        s["music"]["places"]["casino"] = {"style": "cafe", "from": "20:00", "to": "04:00", "far": 20}
        s["music"]["styles"]["cafe"]["scale"] = "blues"
        s["music"]["radio"].append({"name": "Jazz", "style": "jazz"})
        s["ambience"]["birds"]["gain"] = 3
        s["ambience"]["birds"]["from"] = "25:00"
        p = world.sound_problems(w, ["eat"])
        for bit in ("action lift", "XX", "casino", "blues", "jazz", "birds.gain", "25:00", "chat for unknown activity sell"):
            self.assertTrue(any(bit in x for x in p), bit)



class GettingOutOfTheWay(unittest.TestCase):
    """CITY-070, CITY-071: residents dodge a car a player drives at them, then tell the driver off (World/rules/dodge.json)."""
    R = W["dodge"]

    def test_CITY_070_dodge_rules(self):
        r = self.R
        for k in ("min_speed", "see_s", "see_m", "lane_pad_m", "car_half_w_m", "car_front_m", "clear_m", "startle_s", "scramble_s", "hop_m", "return_s", "cooldown_s"):
            self.assertGreater(r[k], 0, k)
        self.assertLess(r["startle_s"] + r["scramble_s"], r["see_s"])       # they are clear before a car seen in time arrives
        self.assertLess(r["hop_m"], 0.5)                                    # a hop, not a leap

    def test_CITY_071_tell_off_rules(self):
        r = self.R
        self.assertGreater(r["scold_s"], 1)
        self.assertTrue(0 <= r["fist_share"] <= 1)
        self.assertGreaterEqual(len(r["shouts"]), 3)
        for s in r["shouts"]:
            self.assertTrue(0 < len(s) <= 40, s)                            # short enough for a tag over a head



class TheFurnitureOutlet(unittest.TestCase):
    """FURNITURE FURN-020, FURN-021: the huge outlet outside the East Gate (Q-30, Tools/worldgen/add_outlet_01.py)."""

    def outlet(self):
        return next(l for l in LOTS if l["kind"] == "furniture")

    def test_FURN_020_outlet_outside_the_walls(self):
        l, P = self.outlet(), W["city"]["grid"]["pitch"]
        x0, z0, x1, z1 = l["rect"]
        east = W["city"]["wall"]["cells"][2] * P                                  # the line where the east wall stood (WLD-055)
        self.assertGreater(x0, east + 9.5)                                          # beyond East Wall Street and that line
        self.assertGreaterEqual((x1 - x0) * (z1 - z0), 2000)                      # huge: a big hall
        self.assertTrue(l["enterable"])
        self.assertEqual(l["door"][1], z0)                                         # its door faces the gate road
        gate = next(g for g in W["roads"]["gates"] if g["id"] == "gate_east")
        self.assertGreaterEqual(east + gate["length"], x1)                         # the road runs past it
        for k in ("work", "customer", "seat"):
            self.assertTrue(l["objects"][k], k)
        staff = [r for r in W["residents"] if r.get("work") == l["id"]]
        self.assertEqual(len(staff), l["staff"])
        b = next(b for b in W["blocks"] if any(q is l for q in b["lots"]))
        self.assertTrue(any(p["kind"] == "carpark" for p in b["props"]))

    def test_FURN_021_residents_browse_and_staff_work(self):
        l = self.outlet()
        stays = [s for d in (DAY, DAY + 1, DAY + 2) for pieces in CITY.plan(d).values() for kind, t0, t1, payload, s in pieces
                 if kind == "stay" and s["place"] == l["id"]]
        self.assertTrue(any(s["act"] == "browse" for s in stays))
        self.assertTrue(any(s["act"] == "work" for s in stays))
        self.assertTrue(all(r["near"].get("furniture") == l["id"] for r in W["residents"]))

    def test_FURN_020_showroom_shows_every_kit(self):
        """The showroom floor has kit 17's room set in each line and a room for each of kit 18's kits (expand_outlet_01.py),
        every set inside the hall and clear of the others, the walks, the fittings and the cafe."""
        import expand_outlet_01 as X
        l = self.outlet()
        x0, z0, x1, z1 = l["rect"]
        sets = l["sets"]
        self.assertEqual(sorted(s["line"] for s in sets if "room" not in s), ["curve", "oldtown", "plain", "workshop"])
        rooms = X.kit18_rooms()
        self.assertEqual(sorted(s["room"] for s in sets if "room" in s), sorted(rooms))
        for s in sets:
            if "room" in s:
                self.assertEqual(s["line"], rooms[s["room"]][0], s["room"])                # in the line it was rendered in
                self.assertEqual([p[0] for p in s["pieces"]], [p[0] for p in rooms[s["room"]][1]], s["room"])
        over = lambda a, b, m=0.0: a[0] < b[2] + m and b[0] < a[2] + m and a[1] < b[3] + m and b[1] < a[3] + m
        walled = [[s["rect"][0] - .15, s["rect"][1] - .15, s["rect"][2] + .15, s["rect"][3] + .15] for s in sets]
        for i, r in enumerate(walled):
            self.assertTrue(x0 + .5 < r[0] and r[2] < x1 - .5 and z0 + .5 < r[1] and r[3] < z1 - .5, sets[i]["rect"])
            for j in range(i + 1, len(sets)):
                self.assertFalse(over(sets[i]["rect"], sets[j]["rect"]), (sets[i]["rect"], sets[j]["rect"]))
            for q in l["route"]:
                self.assertFalse(over(r, q[:4]), (sets[i].get("room"), q))
        for k, places in l["fittings"].items():
            for x, z in places:
                self.assertTrue(x0 < x < x1 and z0 < z < z1, (k, x, z))
                self.assertFalse(any(over(r, [x - 1.8, z - 1.4, x + 1.8, z]) for r in walled), (k, x, z))
        work = l["fittings"]["tills"] + l["fittings"]["desks"] + l["fittings"]["cafe"] + l["fittings"]["packing"]
        self.assertEqual(sorted(map(tuple, work)), sorted(map(tuple, l["objects"]["work"])))
        for x, z in l["objects"]["seat"]:
            self.assertFalse(any(over(r, [x, z, x, z], .3) for r in walled), (x, z))
        b = next(b for b in W["blocks"] if any(q is l for q in b["lots"]))
        cp = next(p for p in b["props"] if p["kind"] == "carpark")
        self.assertGreaterEqual(cp["pos"][1] - cp["size"][1] / 2, z1)                     # the car park is south of the hall
        self.assertLessEqual(cp["pos"][1] + cp["size"][1] / 2, b["origin"][1] + b["size"])  # and in its block


class TheMallCarParks(unittest.TestCase):
    def test_WLD_054_small_car_parks_with_a_bus_stop(self):    # owner 2026-11-23: "Malls to ~1/4, bus stop"
        import math
        stops = {s["id"]: s["at"] for s in W["transit"]["stops"]}
        served = {sid for ln in W["transit"]["lines"] for sid in ln["stops"]}
        malls = [b for b in W["blocks"] if b.get("template") == "mall" and any(p["kind"] == "carpark" for p in b["props"])]
        self.assertEqual(len(malls), 3)
        for b in malls:
            cp = next(p for p in b["props"] if p["kind"] == "carpark")
            self.assertLessEqual(cp["cars"], 50, b["id"])                  # a quarter of what they had (160, 160, 200)
            self.assertTrue(any(p["kind"] == "paving" for p in b["props"]), b["id"])
            near = [sid for sid in served if math.dist(stops[sid], cp["pos"]) < 90]
            self.assertTrue(near, b["id"])                                  # a bus stop at the car park
        self.assertLessEqual(sum(p.get("cars", 0) for b in W["blocks"] for p in b["props"] if p["kind"] == "carpark"), 170)


class TheTimings(unittest.TestCase):
    def test_CITY_096_habits_spread_the_day_and_lunch_is_near_work(self):   # owner 2026-11-23: spread timings over windows
        import math
        roles = W["roles"]["roles"]
        by = {}
        for r in W["residents"]:
            b = CITY.blocks(r["id"], DAY)
            if len(b) > 1:
                by.setdefault(r["role"], []).append(b[1][0])            # when their day gets going
        for role, starts in by.items():
            lo, hi = roles[role]["habit_min"]
            if len(starts) < 40:
                continue
            spread = max(starts) - min(starts)
            if hi - lo >= 120:
                self.assertGreater(spread, .8 * (hi - lo), role)          # an office's staff over about three hours
            self.assertLess(spread, hi - lo + 2 * (roles[role]["jitter"] + 25) + 1, role)   # school and shops keep their hours
        again = {r["id"]: CITY.blocks(r["id"], DAY + 7)[1][0] for r in W["residents"][:200] if len(CITY.blocks(r["id"], DAY)) > 1}
        for r in W["residents"][:200]:
            if r["id"] in again:                                         # a habit: about the same time a week later
                self.assertLess(abs(again[r["id"]] - CITY.blocks(r["id"], DAY)[1][0]), 2 * roles[r["role"]]["jitter"] + 1)
        lots = {l["id"]: l for l in world.lots(W)}
        near, far = 0, 0
        for r in W["residents"]:
            if not r.get("work") or r["role"] != "clerk":
                continue
            b = CITY.blocks(r["id"], DAY)
            for i in range(1, len(b) - 1):
                if b[i][1] in ("eat", "shop") and b[i - 1][1] == "work" and b[i + 1][1] == "work" and b[i][2] in lots:
                    d = math.dist(lots[b[i][2]]["door"], lots[r["work"]]["door"])
                    near += d <= 400
                    far += d > 400
        self.assertGreater(near, 20 * max(far, 1))                       # lunch from work is near work


class TheHomes(unittest.TestCase):
    """The houses' colourways (ADR-0032) and the plan homes are furnished from (ADR-0035)."""
    RULES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "World", "rules")

    @classmethod
    def setUpClass(cls):
        import json
        cls.styles = json.load(open(os.path.join(cls.RULES, "house_styles.json"), encoding="utf-8"))
        cls.plans = json.load(open(os.path.join(cls.RULES, "home_plans.json"), encoding="utf-8"))["plans"]
        cls.houses = [l for l in world.lots(W) if l.get("house", {}).get("plan") == "samples_19"]

    def test_BUILD_003_house_colourways_come_from_the_palette(self):
        S = self.styles
        art = open(os.path.join(self.RULES, "..", "..", "docs", "art", "ART_DIRECTION.md"), encoding="utf-8").read()
        for name, rgb in S["palette"].items():
            self.assertIn("(%.2f, %.2f, %.2f)" % tuple(rgb), art, name)        # every accent is a palette entry of section 2.3
        for line, ways in S["colourways"].items():
            self.assertIn(line, S["lines"])
            self.assertEqual(sorted(ways), ["a", "b", "c"], line)
            for w, c in ways.items():
                self.assertTrue(c["wall"] == "stone" if S["lines"][line]["stone"] else c["wall"] in S["plasters"], (line, w))
                for part in ("shutter", "door", "frame"):
                    self.assertIn(c[part], S["palette"], (line, w, part))
                self.assertIn(c["roof"], S["roofs"], (line, w))

    def test_FURN_060_every_home_has_a_plan(self):
        self.assertEqual(len(self.houses), 469)
        spots = self.plans["house_12x9"]["house"]["spots"]
        beds = sum(len(spots[it["piece"]]) for it in self.plans["house_12x9"]["items"] if it["piece"] in self.plans["house_12x9"]["house"]["beds"])
        for l in self.houses:
            h = l["house"]
            self.assertIn(h.get("interior"), self.plans, l["id"])
            self.assertIn(h.get("style"), self.styles["colourways"][h["furniture"]], l["id"])
            self.assertEqual(len(h["rooms"]), 10, l["id"])
            self.assertLessEqual(len(l["objects"]["bed"]), beds, l["id"])     # a bed for every resident
        for lid in ("b_0_0_l7", "b_0_0_l9", "b_0_0_l12", "b_0_0_l13"):     # the houses the owner reviewed keep the line as drawn
            self.assertEqual(next(l for l in self.houses if l["id"] == lid)["house"]["style"], "a")

    def test_FURN_060_row_houses_and_flats_have_a_plan(self):
        """ADR-0036: every row house and court flat of 80 or 160 m2 is furnished from its plan (add_homes_01.py)."""
        spots = self.plans["house_12x9"]["house"]["spots"]
        rows = [l for l in world.lots(W) if l.get("interior")]
        flats = [(l, q) for l in world.lots(W) for q in l.get("flats", []) if q.get("interior")]
        self.assertEqual(len(rows), 250)
        self.assertEqual(len(flats), 653)
        for l in rows:
            p = self.plans[l["interior"]]
            self.assertEqual(len(p["house"]["floors"]), l["floors"], l["id"])
            self.assertEqual(l["stair"]["kind"], "straight", l["id"])
            beds = sum(len(spots[i["piece"]]) for i in p["items"] if i["piece"] in ("k17:bed", "c_bunk", "r_single"))
            self.assertLessEqual(len(l["objects"]["bed"]), beds, l["id"])          # a bed for every resident
            self.assertTrue(all(f >= 1 for f in l["levels"]["bed"]), l["id"])     # the beds are upstairs
        for l, q in flats:
            self.assertIn(q["interior"], ("flat_80", "flat_160"), q["id"])
            self.assertFalse(l.get("tower"), q["id"])

    def test_FURN_061_plans_keep_clear(self):
        import importlib.util
        p = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "blender", "house_interiors_21_plan.py")
        spec = importlib.util.spec_from_file_location("house_interiors_21_plan", p)
        plan = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(plan)
        self.assertEqual(plan.check(), [])
        stored = self.plans["house_12x9"]["items"]
        self.assertEqual([(i["piece"], i["x"], i["y"], i["level"]) for i in stored],
                         [(i["piece"], i["x"], i["y"], i["level"]) for i in plan.ITEMS])   # the game's plan is the checked one

    def test_BUILD_003_building_colourways_come_from_the_palette(self):
        import json
        S = json.load(open(os.path.join(self.RULES, "building_styles.json"), encoding="utf-8"))
        names = {**S["palette"], **S["plasters"], **S.get("stones", {}), **S["roofs"], "stone": None}
        for group, ways in S["colourways"].items():
            self.assertIn(group, S["groups"])
            self.assertEqual(sorted(ways), ["a", "b", "c"], group)
            for w, c in ways.items():
                for part, v in c.items():
                    if part != "name":
                        for one in (v if isinstance(v, list) else [v]):       # an awning is a pair of stripes
                            self.assertIn(one, names, (group, w, part))
        self.assertTrue(set(S["types"].values()) <= set(S["groups"]))

    def test_FURN_061_every_home_plan_keeps_clear(self):
        import importlib.util, sys
        here = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "blender")
        sys.path.insert(0, here)
        spec = importlib.util.spec_from_file_location("home_interiors_22_plan", os.path.join(here, "home_interiors_22_plan.py"))
        plan = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(plan)
        for p in plan.build():
            self.assertEqual(p.check(), [], p.key)
            self.assertEqual([(i["piece"], i["x"], i["y"], i["level"]) for i in self.plans[p.key]["items"]],
                             [(i["piece"], i["x"], i["y"], i["level"]) for i in p.items], p.key)   # the game's plan is the checked one

    def test_FURN_062_levels_add_up(self):
        for name, p in self.plans.items():
            n = [sum(1 for it in p["items"] if it["level"] <= lv) for lv in (1, 2, 3, 4)]
            self.assertTrue(0 < n[0] < n[1] < n[2] < n[3] == len(p["items"]), (name, n))
            self.assertEqual(sorted(int(k) for k in p["levels"]), [1, 2, 3, 4])
class TheOpenGround(unittest.TestCase):
    """WLD-055: the town wall is gone; WLD-056: the ground outside Old Town is filled with woods, commons, orchards,
    fields, plots for sale and meadows, with no buildings (owner, 2026-11-24)."""
    GREEN = ("woods", "common", "orchard", "fields", "plots", "meadow")
    WILD = {"fence", "board", "peg", "log", "rock", "picnic", "haybale", "crop", "pond", "plant", "bench", "bin", "lamp"}

    def blocks(self):
        return [b for b in W["blocks"] if b["template"] in self.GREEN]

    def test_WLD_055_the_town_wall_is_gone(self):
        import os
        with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Prototype", "play", "play.js"), encoding="utf-8") as f:
            js = f.read()
        self.assertNotIn("wallBox", js)                                  # no wall, no gate towers, no gate names over an arch
        self.assertIn("kerb", js)                                        # Old Town's paving ends at a kerb on the old line
        self.assertEqual(len(W["roads"]["gates"]), 4)                    # the roads out of town are all still there

    def test_WLD_056_every_cell_has_a_block(self):
        g, seen = W["city"]["grid"], set()
        for b in W["blocks"]:
            for c in world.cells_of(b):
                self.assertNotIn(tuple(c), seen)
                seen.add(tuple(c))
        bare = [(i, j) for i in range(g["columns"]) for j in range(g["rows"]) if (i, j) not in seen]
        self.assertEqual(bare, [])

    def test_WLD_056_the_open_ground_has_no_buildings(self):
        bs = self.blocks()
        self.assertEqual({b["template"] for b in bs}, set(self.GREEN))
        for b in bs:
            self.assertEqual(b["lots"], [], b["id"])
            for p in b["props"]:
                self.assertIn(p["kind"], self.WILD, p.get("id"))
                self.assertEqual(p.get("set"), "wild", p.get("id"))
        trees = sum(len(b["trees"]) for b in bs if b["template"] == "woods")
        self.assertGreater(trees, 3000)                                   # the woods are dense

    def test_WLD_056_every_tree_and_plant_is_known(self):
        sp, pl = W["vegetation"]["species"], W["vegetation"]["plants"]
        for b in self.blocks():
            for t in b["trees"]:
                self.assertIn(t["species"], sp, t["id"])
            for p in b["props"]:
                if p["kind"] == "plant":
                    self.assertIn(p["species"], pl, p["id"])

    def test_WLD_056_nothing_stands_on_a_road(self):
        import add_green_01 as G
        R = G.Roads(W["city"], W["roads"], [b for b in W["blocks"] if b["template"] not in self.GREEN])
        for b in self.blocks():
            for t in b["trees"]:
                self.assertGreater(R.gap(*t["pos"]), 2.5, t["id"])
            for p in b["props"]:
                if p["kind"] in ("crop", "fence", "plant") and (p.get("size") or p.get("len")):
                    continue                                              # fields and hedges are checked by their corners below
                self.assertGreater(R.gap(*p["pos"]), 0.5, p["id"])
            for p in b["props"]:
                if p["kind"] == "crop":
                    (x, z), (w, d) = p["pos"], p["size"]
                    for cx, cz in ((x - w / 2, z - d / 2), (x + w / 2, z - d / 2), (x - w / 2, z + d / 2), (x + w / 2, z + d / 2), (x, z)):
                        self.assertGreater(R.gap(cx, cz), 0.5, p["id"])

    def test_WLD_056_plots_carry_a_for_sale_board_each(self):
        for b in (b for b in self.blocks() if b["template"] == "plots"):
            plots = [p for p in b["props"] if p["kind"] == "crop" and p["crop"] == "rough"]
            boards = [p for p in b["props"] if p["kind"] == "board"]
            self.assertGreater(len(plots), 4, b["id"])
            self.assertEqual(len(boards), len(plots), b["id"])
            self.assertTrue(all(p["text"] == "FOR SALE" for p in boards))

    def test_WLD_056_the_generator_writes_what_is_there(self):
        """The blocks are the files; add_green_01.py, run again, writes the same ones."""
        import add_green_01 as G, json, os
        city = W["city"]
        roads = W["roads"]
        others = [b for b in W["blocks"] if b["id"] not in {"b_%d_%d" % a[0] for a in G.AREAS}]
        R = G.Roads(city, roads, others)
        for cell, span, template, district in G.AREAS:
            b = G.Block(city, cell, span, template, district, R)
            getattr(b, template)()
            with open(os.path.join(G.CITY, "blocks", b.id + ".json"), encoding="utf-8") as f:
                self.assertEqual(json.load(f), json.loads(json.dumps(b.d)), b.id)


class TheMallShell(unittest.TestCase):
    def test_WLD_057_eastgate_has_a_ribbon_skin_and_an_egg_over_each_corner_store(self):   # owner 2026-11-24, from a photograph
        import math
        b = next(b for b in W["blocks"] if b["district"] == "Eastgate Mall")
        l = next(l for l in b["lots"] if "mall" in l.get("tags", []))
        sh, (x0, z0, x1, z1) = l["shell"], l["rect"]
        self.assertEqual(sh["style"], "ribbon")
        H, pitch = l["floors"] * l["plan"]["storey_height"], W["city"]["grid"]["pitch"]
        bx0, bz0 = b["origin"]
        bx1 = bx0 + b["size"] + (b["span"][0] - 1) * pitch
        paving = next(p for p in b["props"] if p["kind"] == "paving")
        south = paving["pos"][1] - paving["size"][1] / 2                      # the forecourt starts here; the shell stops short of it
        tones = ("ochre", "cream", "terracotta", "teal", "plaster")           # the palette names the slice knows (shell.js)
        on_hall = lambda x, z: (min(max(x, x0), x1), min(max(z, z0), z1))     # the nearest point of the hall's wall
        in_egg = lambda d, x, y, z: ((x - d["pos"][0]) / d["radii"][0]) ** 2 + ((y - d["centre"]) / d["radii"][1]) ** 2 + ((z - d["pos"][1]) / d["radii"][2]) ** 2 < 1
        pts, eggs = sh["skin"]["points"], sh["domes"]
        self.assertGreater(len(pts), 100)
        for i, (x, z, bottom, top) in enumerate(pts):
            self.assertLessEqual(math.dist((x, z), pts[(i + 1) % len(pts)][:2]), 3.5, i)       # one closed line, no gaps
            self.assertGreaterEqual(math.dist((x, z), on_hall(x, z)), 2.0, i)                  # outside the hall all the way round
            self.assertTrue(bx0 < x < bx1 and bz0 < z < south, (x, z))                         # inside its block, short of the forecourt
            self.assertGreaterEqual(top - bottom, 4.0, i)
            self.assertTrue(top >= H + 1 or in_egg_any(eggs, in_egg, *on_hall(x, z), H + .5), (x, z, top))   # the hall's box never shows over the skin
            for d in eggs:
                self.assertGreater(math.hypot((x - d["pos"][0]) / d["radii"][0], (z - d["pos"][1]) / d["radii"][2]), 1.0, i)   # the skin stands outside every egg
        self.assertTrue(all(c in tones for c in sh["skin"]["colors"]))
        self.assertGreaterEqual(sh["skin"]["bands"], 8)
        dx, dz = l["door"]                                                    # the door is under an arch high enough to walk in under
        at_door = [p for p in pts if abs(p[0] - dx) < 8 and p[1] > z1]
        self.assertTrue(at_door and all(p[2] >= 6 for p in at_door))
        self.assertTrue(any(a["at"] == [dx, dz] for a in sh["arches"]))
        for a in sh["arches"]:                                                # each arch listed is in the line: lifted where it says
            lifted = [p[2] for p in pts if math.dist(on_hall(p[0], p[1]), a["at"]) < a["width"] / 4]
            self.assertTrue(lifted and min(lifted) > a["height"] * .6, a)
        units = {u["id"]: u for u in l["plan"]["levels"][0]["units"]}
        self.assertEqual(len(eggs), 4)
        self.assertEqual(len({d["over"] for d in eggs}), 4)
        for d in eggs:                                                        # an egg over each corner store, in the block, well above the hall
            u, (rx, ry, rz) = units[d["over"]], d["radii"]
            self.assertEqual(u["kind"], "anchor")
            self.assertTrue(u["rect"][0] < d["pos"][0] < u["rect"][2] and u["rect"][1] < d["pos"][1] < u["rect"][3], d)
            self.assertTrue(bx0 <= d["pos"][0] - rx and d["pos"][0] + rx <= bx1 and bz0 <= d["pos"][1] - rz and d["pos"][1] + rz <= south, d)
            self.assertGreater(d["centre"] + ry, H + 8)
            self.assertIn(d["color"], tones)
            self.assertGreaterEqual(d["cells"], 40)
        rl, court = sh["roof_light"], l["plan"]["levels"][1]["voids"][0]      # the roof light stands on the roof, over the open court
        self.assertEqual(rl["base"], H)
        self.assertIn(rl["color"], tones)
        cxs, czs = [q[0] for q in court], [q[1] for q in court]
        self.assertTrue(min(cxs) <= rl["pos"][0] <= max(cxs) and min(czs) <= rl["pos"][1] <= max(czs))
        self.assertTrue(x0 < rl["pos"][0] - rl["radii"][0] and rl["pos"][0] + rl["radii"][0] < x1 and z0 < rl["pos"][1] - rl["radii"][2] and rl["pos"][1] + rl["radii"][2] < z1)

    def test_WLD_057_the_script_gives_the_shell_in_the_block_file(self):       # the data is what mall_shell_01.py works out, not edited by hand
        import mall_shell_01 as M
        b = next(b for b in W["blocks"] if b["district"] == "Eastgate Mall")
        l = next(l for l in b["lots"] if "mall" in l.get("tags", []))
        self.assertEqual(M.shell(b, {k: v for k, v in l.items() if k != "shell"}, M.PLAN[b["id"]], W["city"]["grid"]["pitch"]), l["shell"])


class TheMallInside(unittest.TestCase):                                        # owner 2026-11-24: "start interiors fresh as this is new design"
    def setUp(self):
        import mall_inside_01 as M
        self.M = M
        self.b = next(b for b in W["blocks"] if b["district"] == "Eastgate Mall")
        self.l = next(l for l in self.b["lots"] if "mall" in l.get("tags", []))
        self.p, self.sh = self.l["plan"], self.l["shell"]
        self.ground = self.p["levels"][0]

    def on_floor(self, k, x, z):
        L = self.p["levels"][k]
        on_bridge = any(self.M.inside(q, x, z) for q in L["bridges"])
        return on_bridge or (any(self.M.inside(q, x, z) for q in L["walks"]) and not any(self.M.inside(q, x, z) for q in L["voids"]))

    def test_WLD_058_an_entrance_under_every_arch_and_a_walk_from_each_to_the_court(self):
        x0, z0, x1, z1 = self.l["rect"]
        self.assertEqual(self.p["concept"], "court")
        ents = self.p["entrances"]
        self.assertEqual(sorted(e["side"] for e in ents), ["e", "n", "s", "w"])
        arches = {a["side"]: a for a in self.sh["arches"]}
        for e in ents:
            self.assertEqual(e["at"], arches[e["side"]]["at"])                     # the opening is where the shell is lifted
            self.assertLess(e["width"], arches[e["side"]]["width"])
            self.assertGreaterEqual(e["width"], 8)
            x, z = e["at"]
            self.assertTrue(abs(x - x0) < .01 or abs(x - x1) < .01 or abs(z - z0) < .01 or abs(z - z1) < .01)   # in the hall's wall
            ix, iz = min(max(x, x0 + 2), x1 - 2), min(max(z, z0 + 2), z1 - 2)
            self.assertTrue(self.on_floor(0, ix, iz), e)                           # and a walk begins just inside it
        self.assertIn(self.l["door"], [e["at"] for e in ents])                     # the lot's door is one of them
        court = self.ground["walks"][0]
        cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
        self.assertTrue(self.M.inside(court, cx, cz))
        halls = self.p["halls"]
        for w in self.ground["walks"][1:]:                                         # every other walk meets the court, or is an egg hall met by a walk that does
            meets = any(self.M.inside(court, x, z) for x, z in w)
            is_hall = any(all(abs(((x - h["pos"][0]) ** 2 + (z - h["pos"][1]) ** 2) ** .5 - h["r"]) < .05 for x, z in w) for h in halls)
            self.assertTrue(meets or is_hall, w[:2])
        for h in halls:
            self.assertTrue(any(self.M.inside(court, *w[0]) and any(((x - h["pos"][0]) ** 2 + (z - h["pos"][1]) ** 2) ** .5 < h["r"] for x, z in w) for w in self.ground["walks"][1:] if len(w) == 4), h)

    def test_WLD_059_a_round_hall_under_each_egg_with_its_staff_inside(self):
        x0, z0, x1, z1 = self.l["rect"]
        units = {u["id"]: u for u in self.ground["units"]}
        halls = self.p["halls"]
        self.assertEqual(sorted(h["dome"] for h in halls), [0, 1, 2, 3])
        for h in halls:
            d, (x, z), r = self.sh["domes"][h["dome"]], h["pos"], h["r"]
            self.assertEqual(h["pos"], d["pos"])                                   # under its egg's middle
            self.assertEqual(d["over"], h["unit"])
            self.assertGreaterEqual(r, 12)
            self.assertTrue(x0 < x - r and x + r < x1 and z0 < z - r and z + r < z1)   # inside the hall's walls
            self.assertLess(r, min(d["radii"][0], d["radii"][2]))                  # and narrower than the egg above it
            u = units[h["unit"]]
            self.assertEqual(u["kind"], "anchor")
            self.assertEqual(len(u["stand"]), 6)
            for sx, sz in u["stand"]:
                self.assertLess(((sx - x) ** 2 + (sz - z) ** 2) ** .5, r - 2)

    def test_WLD_060_a_court_open_to_the_roof_light_with_bridges_and_moving_stairs(self):
        import math
        x0, z0, x1, z1 = self.l["rect"]
        cx, cz, st = (x0 + x1) / 2, (z0 + z1) / 2, self.p["storey_height"]
        self.assertEqual(len(self.p["levels"]), self.l["floors"])
        v1, v2, eyes, rl = self.p["levels"][1]["voids"][0], self.p["levels"][2]["voids"][0], self.p["roof"]["openings"], self.sh["roof_light"]
        court, lobe = self.p["levels"][1]["walks"][0], self.M.LOBE
        span = lambda v, dz: (min(x for x, z in v if abs(z - cz - dz) < 2), max(x for x, z in v if abs(z - cz - dz) < 2))
        for v in (v1, v2):
            self.assertTrue(self.M.inside(v, cx, cz))
            self.assertTrue(all(self.M.inside(court, x, z) for x, z in v))         # a gallery all round it
            wide = {dz: span(v, dz)[1] - span(v, dz)[0] for dz in (-2 * lobe, -1.5 * lobe, -lobe, -.5 * lobe, 0, .5 * lobe, lobe, 1.5 * lobe, 2 * lobe)}
            for dz in (-1.5 * lobe, -.5 * lobe, .5 * lobe, 1.5 * lobe):            # a chain of round atria: at each pinch the opening is far narrower than at the atria either side
                self.assertLess(wide[dz], .75 * min(wide[dz - .5 * lobe], wide[dz + .5 * lobe]), dz)
        for dz in (-1.5 * lobe, -.5 * lobe, .5 * lobe, 1.5 * lobe):                # the two floors sway opposite ways: at a pinch one floor's gallery juts out over the other's
            m1, m2 = sum(span(v1, dz)) / 2 - cx, sum(span(v2, dz)) / 2 - cx
            self.assertLess(m1 * m2, 0, dz)
            self.assertGreater(abs(m1 - m2), 6, dz)
        self.assertEqual(len(eyes), 5)                                             # a round skylight over each atrium, under the roof light
        for e in eyes:
            ex, ez = sum(x for x, _ in e) / len(e), sum(z for _, z in e) / len(e)
            self.assertTrue(self.M.inside(v2, ex, ez) and abs(ex - cx) < .1 and min(abs(ez - cz - k * lobe) for k in (-2, -1, 0, 1, 2)) < .1)
            self.assertTrue(all(((x - rl["pos"][0]) / rl["radii"][0]) ** 2 + ((z - rl["pos"][1]) / rl["radii"][2]) ** 2 < 1 for x, z in e))
        for q in self.p["planters"]:                                               # the planters stand on the galleries, clear of the edge
            self.assertTrue(self.on_floor(q["level"], *q["pos"]) and all(math.dist(q["pos"], p) > q["r"] + 2.5 for p in self.p["levels"][q["level"]]["voids"][0]), q)
        for k in (1, 2):
            L = self.p["levels"][k]
            self.assertEqual(len(L["bridges"]), 4)                                # one at every pinch
            for b in L["bridges"]:
                xs, zs = [q[0] for q in b], [q[1] for q in b]
                mz = (min(zs) + max(zs)) / 2
                self.assertTrue(self.M.inside(L["voids"][0], (min(xs) + max(xs)) / 2, mz))            # it crosses the opening
                for ex in (min(xs) + .3, max(xs) - .3):                                              # and both its ends rest on the gallery
                    self.assertTrue(any(self.M.inside(q, ex, mz) for q in L["walks"]) and not self.M.inside(L["voids"][0], ex, mz), (k, ex, mz))
            self.assertTrue(any(len(r) > 20 for r in L["rails"]))                  # a parapet all round the opening
        ramps = self.p["ramps"]
        self.assertEqual(sorted((r["from"], r["to"]) for r in ramps), [(0, 1), (0, 1), (1, 2), (1, 2)])
        for r in ramps:
            run = math.dist(r["a"], r["b"])
            self.assertLessEqual(st / run, math.tan(math.radians(30)))             # no steeper than a moving stair
            self.assertTrue(self.on_floor(r["from"], *r["a"]) or any(abs(r["a"][1] - z) < .01 for b in self.p["levels"][r["from"]]["bridges"] for _, z in b), r)
            self.assertTrue(any(abs(r["b"][1] - z) < .01 and min(q[0] for q in b) < r["b"][0] < max(q[0] for q in b) for b in self.p["levels"][r["to"]]["bridges"] for _, z in b), r)   # it lands on a bridge's edge
        for f in self.p["fountains"]:
            self.assertTrue(self.on_floor(f["level"], *f["pos"]))
        for g in self.p["gardens"]:
            self.assertTrue(self.on_floor(0, *g["pos"]) and all(self.on_floor(0, t[0], t[1]) for t in g["trees"]))

    def test_WLD_062_a_lift_in_each_side_concourse_and_moving_stairs_wide_enough_to_run_both_ways(self):
        import math
        x0, z0, x1, z1 = self.l["rect"]
        cx, lifts = (x0 + x1) / 2, self.p["lifts"]
        self.assertEqual(sorted(q["pos"][0] < cx for q in lifts), [False, True])   # one each side of the court
        for q in lifts:
            (x, z), r, t = q["pos"], q["r"], math.radians(q["door"])
            fx, fz = math.sin(t), -math.cos(t)
            self.assertLess(abs(x + fx * 20 - cx), abs(x - cx))                    # its door faces the court
            for L in self.p["levels"]:                                             # on every floor: on a walk, clear of the opening, room before its door and a way past on both sides
                for px, pz in ((x, z), (x + fx * (r + 1.3), z + fz * (r + 1.3)), (x - fz * (r + 2.5), z + fx * (r + 2.5)), (x + fz * (r + 2.5), z - fx * (r + 2.5))):
                    self.assertTrue(any(self.M.inside(w, px, pz) for w in L["walks"]), (L["level"], px, pz))
                    self.assertFalse(any(self.M.inside(v, px, pz) for v in L["voids"]), (L["level"], px, pz))
            for u in self.p["levels"][0]["units"]:                                 # nothing stands in it: no kiosk, bench, visitor or member of staff
                if u["kind"] == "kiosk":
                    self.assertGreater(math.dist(((u["rect"][0] + u["rect"][2]) / 2, (u["rect"][1] + u["rect"][3]) / 2), (x, z)), r + 4)
            for p in [b["pos"] for b in self.p["benches"]] + self.l["objects"]["customer"] + self.l["objects"]["seat"] + self.l["objects"]["work"]:
                self.assertGreater(math.dist(p[:2], (x, z)), r + .6, p)
        for r in self.p["ramps"]:                                                  # two flights side by side, one up and one down
            self.assertGreaterEqual(r["width"], 4.0)
        a, b = [r for r in self.p["ramps"] if r["from"] == 0]                      # the two from the court floor cross: side by side, running opposite ways over the same stretch
        self.assertLess((a["b"][1] - a["a"][1]) * (b["b"][1] - b["a"][1]), 0)
        self.assertAlmostEqual(a["a"][1], b["b"][1], delta=.1)
        self.assertGreaterEqual(abs(a["a"][0] - b["a"][0]), a["width"])

    def test_WLD_064_the_plan_gives_the_light_what_it_needs_of_the_openings(self):
        import math
        x0, z0, x1, z1 = self.l["rect"]
        cx, cz, c = (x0 + x1) / 2, (z0 + z1) / 2, self.p["court"]
        self.assertEqual(c["lobe"], self.M.LOBE)
        for k in (1, 2):                                                          # the numbers say the same opening as the polygon drawn from them: the slice works the sun's way through them from the numbers
            v, poly = c["voids"][str(k)], self.p["levels"][k]["voids"][0]
            def half(z):
                u = min(1.0, abs(z) / v["long"])
                return math.sqrt(max(0.0, 1 - u ** 4)) * (v["narrow"] + (v["wide"] - v["narrow"]) * (0.5 + 0.5 * math.cos(2 * math.pi * z / c["lobe"])))
            for dz in range(-80, 81, 4):
                for dx in range(-40, 41, 4):
                    off = abs(dx - v["sway"] * math.sin(math.pi * dz / c["lobe"])) - half(dz)
                    if abs(off) > 0.6:                                             # (clear of the edge, where the polygon's straight lengths differ a little)
                        self.assertEqual(self.M.inside(poly, cx + dx, cz + dz), off < 0, (k, dx, dz))
        self.assertEqual(len(self.p["roof"]["openings"]), 5)                       # the slice's light counts five skylights

    def test_WLD_063_every_front_is_an_outlet_from_the_catalogue_and_the_rides_stand_clear(self):
        import json, math, os
        with open(self.M.OUTLETS, encoding="utf-8") as f:
            cat = json.load(f)
        walls = {"rail", "shelves", "guitars", "screens", "menu", "mirror", "rackets", "cabinets", "posters", "servery"}
        middle = {"table", "roundrack", "bin", "bench", "amps", "drums", "keyboard", "glasscase", "cafetable", "hockey", "claw", "buckets", "plinth"}
        window = {"mannequins", "stack", "plinth", "screen", "guitarstand", "plants"}
        what = {"folded", "boxes", "books", "shoes", "balls", "pots", "jars", "gadgets", "plush", "bread", "bottles", "lamps"}
        things = {"bear", "blocks", "vase", "cone", "plant", "rocket", "cake"}
        for kind, k in cat["kinds"].items():                                      # the catalogue uses only words the slice can draw
            self.assertIn(k["trade"], ("shop", "food"), kind)
            self.assertEqual(len(k["walls"]), 3, kind)
            for words, known in ((k["walls"], walls), (k["middle"], middle), (k["window"], window)):
                for w in words:
                    name, _, sub = w.partition(":")
                    self.assertIn(name, known, (kind, w))
                    if sub:
                        self.assertIn(sub, things if name == "plinth" else what, (kind, w))
        names, kinds = set(), set()
        for L in self.p["levels"]:
            byid = {u["id"]: u for u in L["units"]}
            row = [byid[f["unit"]] for f in L["fronts"]]
            for u in row:                                                          # every unit with a front is an outlet of a kind that trades as it does, with one of that kind's names
                k = cat["kinds"][u["outlet"]]
                self.assertEqual(k["trade"], u["kind"], u["id"])
                self.assertIn(u["name"], k["names"], u["id"])
                self.assertIn(u["outlet"], self.p["outlets"])
                names.add(u["name"])
                kinds.add(u["outlet"])
            for a, b in zip(row, row[1:]):
                self.assertNotEqual(a["outlet"], b["outlet"], (a["id"], b["id"]))   # no two of a kind next door
        self.assertLessEqual(len(names), 100)                                      # the slice has room for so many painted names
        for wanted in ("clothes", "toys", "electronics", "shoes", "music", "arcade", "cafe", "burger"):   # what the owner asked for is there
            self.assertIn(wanted, kinds)
        rides = self.p["rides"]
        self.assertEqual([r["kind"] for r in rides].count("carousel"), 1)
        for r in rides:                                                            # the rides stand on the court floor, clear of gardens, fountains, benches, kiosks and each other
            x, z, rad = r["pos"][0], r["pos"][1], r.get("r", 1.2)
            self.assertTrue(self.on_floor(0, x, z), r)
            for g in self.p["gardens"]:
                self.assertGreater(math.dist(g["pos"], (x, z)), rad + max(g["r"]), r)
            for f in self.p["fountains"]:
                self.assertGreater(math.dist(f["pos"], (x, z)), rad + f["r"] + 1, r)
            for b in self.p["benches"]:
                self.assertGreater(math.dist(b["pos"], (x, z)), rad + 1.0, r)
            for u in self.p["levels"][0]["units"]:
                if u["kind"] == "kiosk":
                    self.assertGreater(math.dist(((u["rect"][0] + u["rect"][2]) / 2, (u["rect"][1] + u["rect"][3]) / 2), (x, z)), rad + 3.5, r)
            for q in rides:
                if q is not r:
                    self.assertGreater(math.dist(q["pos"], (x, z)), rad + q.get("r", 1.2) + .5, r)

    def test_WLD_066_every_outlet_has_a_room_of_its_own_to_buy_in(self):
        # the owner, 2026-11-24: "add B or Buy separately at each outlet, instead of showing it for entire mall"
        import collections, math
        seen, depth = {}, self.p["outlet_depth"]
        for L in self.p["levels"]:
            fronts = collections.Counter(f["unit"] for f in L["fronts"])
            for u in L["units"]:
                if not u.get("outlet"):
                    self.assertNotIn(u["id"], fronts, u["id"])               # only outlets have fronts
                    continue
                self.assertNotIn(u["id"], seen, "an outlet's id is its own on every floor")
                seen[u["id"]] = L["level"]
                self.assertEqual(fronts[u["id"]], 1, u["id"])                # one front, so one room and one counter
                self.assertTrue(u["name"].strip(), u["id"])                  # the prompt names it
                f = next(f for f in L["fronts"] if f["unit"] == u["id"])
                self.assertGreaterEqual(math.dist(f["a"], f["b"]) - 1.8, 2.0, u["id"])   # between its piers, room to stand in
                self.assertGreaterEqual(depth["least"], 4.0)
        self.assertEqual(len(seen), 189)
        self.assertEqual(sorted(set(seen.values())), [0, 1, 2])              # on every floor
        self.assertEqual(world.mall_goods_problems(W), [])                   # and every kind has goods to sell (ECO-014)
        for kind in {u["outlet"] for L in self.p["levels"] for u in L["units"] if u.get("outlet")}:
            self.assertGreaterEqual(len(W["mall_goods"]["sells"][kind]), 6, kind)

    def test_WLD_061_shops_on_every_walk_and_the_ground_floor_keeps_its_jobs(self):
        import json, os, sys
        sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "citysim"))
        import careers
        with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "World", "rules", "careers.json"), encoding="utf-8") as f:
            R = json.load(f)
        posts = careers.posts(self.l, R, "mall")
        self.assertEqual(len(posts), 206)                                          # as the old ground floor had, so nobody's work changed
        by_unit = {}
        for _, u in posts:
            by_unit[u] = by_unit.get(u, 0) + 1
        units = {u["id"]: u for u in self.ground["units"]}
        self.assertEqual({u: len(units[u]["stand"]) for u in by_unit}, by_unit)    # a place to stand for every post
        self.assertEqual(sorted(by_unit.values()), [3] * 58 + [4] * 2 + [6] * 4)
        stands = {tuple(s) for u in self.ground["units"] for s in u.get("stand", [])}
        self.assertTrue(all(tuple(w) in stands for w in self.l["objects"]["work"]))
        self.assertEqual(self.l["staff"], len(self.l["objects"]["work"]))
        for L in self.p["levels"]:
            ids = {u["id"] for u in L["units"]}
            self.assertGreaterEqual(len(L["fronts"]), 50)
            self.assertTrue(all(fr["unit"] in ids for fr in L["fronts"]))
            self.assertTrue(all(u["kind"] in R["mall_units"] for u in L["units"]))
            shops = [self.M.corners(u) for u in L["units"] if u["kind"] != "anchor"]
            self.assertFalse(any(self.M.overlap(a, b) for i, a in enumerate(shops) for b in shops[i + 1:]))   # no two shops on the same ground
        for kind in ("customer", "seat"):
            self.assertTrue(all(self.on_floor(0, x, z) for x, z in self.l["objects"][kind]), kind)

    def test_WLD_058_the_script_gives_the_plan_in_the_block_file(self):         # the data is what mall_inside_01.py works out, not edited by hand
        plan, places = self.M.design(self.l, self.sh)
        self.assertEqual(plan, self.l["plan"])
        self.assertEqual(places["customer"], self.l["objects"]["customer"])
        self.assertEqual(places["seat"], self.l["objects"]["seat"])


def in_egg_any(eggs, inside, x, z, y):
    return any(inside(d, x, y, z) for d in eggs)


if __name__ == "__main__":
    unittest.main()
