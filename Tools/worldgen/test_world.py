"""Checks on the city data in World/ and on the living-city rules running over it.   python -m unittest test_world"""
import unittest

import world

W = world.load()
CITY = world.simulation(W)
DAY = 16
LOTS = world.lots(W)
BUILDINGS = [l for l in LOTS if l["kind"] not in world.OPEN_KINDS]


class TheMap(unittest.TestCase):
    def test_data_is_sound(self):
        self.assertEqual(world.validate(W), [])

    def test_eight_by_eight_blocks_each_cell_once(self):
        g = W["city"]["grid"]
        self.assertEqual((g["columns"], g["rows"]), (8, 8))
        self.assertEqual(sorted(tuple(b["grid"]) for b in W["blocks"]), [(i, j) for i in range(8) for j in range(8)])

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
        for rid, pieces in CITY.plan(DAY).items():
            self.assertAlmostEqual(pieces[0][1], 0.0)
            self.assertAlmostEqual(pieces[-1][2], 1440.0)
            for a, b in zip(pieces, pieces[1:]):
                self.assertAlmostEqual(a[2], b[1], places=6)
            self.assertEqual(pieces[0][4]["place"], CITY.res[rid]["home"])
            self.assertEqual(pieces[-1][4]["place"], CITY.res[rid]["home"])

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
        night = CITY.at(DAY * 1440 + 3 * 60)
        self.assertTrue(all(s["state"] == "inside" for s in night.values()))
        noon = CITY.at(DAY * 1440 + 12.5 * 60)
        self.assertGreater(sum(s["state"] in ("walking", "outside") for s in noon.values()), 80)

    def test_only_nearby_residents_get_bodies(self):
        state = CITY.at(DAY * 1440 + 18 * 60)
        spawn = tuple(W["city"]["spawn"]["pos"])
        near = CITY.loaded(state, [spawn], radius=85.0)
        self.assertTrue(0 < len(near) < len(state) / 4)


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
        self.assertGreaterEqual(len(self.S["music"]["radio"]), 3)

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
        s["music"]["radio"].append("jazz")
        s["ambience"]["birds"]["gain"] = 3
        s["ambience"]["birds"]["from"] = "25:00"
        p = world.sound_problems(w, ["eat"])
        for bit in ("action lift", "XX", "casino", "blues", "jazz", "birds.gain", "25:00", "murmur for unknown activity shop"):
            self.assertTrue(any(bit in x for x in p), bit)


if __name__ == "__main__":
    unittest.main()
