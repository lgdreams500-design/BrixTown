"""Checks on spoken dialogue (SND-018, ADR-0019, ADR-0056): the voice cast, the owner's picks, a named resident's own
voice, the index by resident id, and that every line can be spoken.

    python -m unittest test_voices        (from Tools/voice; needs no voice model)
"""
import copy, json, os, re, unittest

import make_dialogue_voices as M

RULES = M.RULES
V = M.read(RULES, "voices.json")
KOKORO = {"af_alloy", "af_aoede", "af_bella", "af_heart", "af_jessica", "af_kore", "af_nicole", "af_nova", "af_river", "af_sarah", "af_sky",
          "am_adam", "am_echo", "am_eric", "am_fenrir", "am_liam", "am_michael", "am_onyx", "am_puck", "am_santa",
          "bf_alice", "bf_emma", "bf_isabella", "bf_lily", "bm_daniel", "bm_fable", "bm_george", "bm_lewis"}   # Kokoro v1.0 English voices
INDEX = os.path.join(M.OUT, "index.json")


def accent(name):
    return "en-gb" if name[0] == "b" else "en-us"


class TheVoices(unittest.TestCase):

    def sound(self, o, mixed=False):
        """A voice as the generator can make it: a Kokoro voice in its own accent (or, for a named resident, two of
        them mixed, in the accent of one), shifted and paced within what still sounds like a person."""
        names = [o["voice"]] if isinstance(o["voice"], str) else o["voice"]
        self.assertTrue(len(names) == 1 or (mixed and len(names) == 2 and names[0] != names[1]), names)
        for n in names:
            self.assertIn(n, KOKORO)
        self.assertIn(o["lang"], {accent(n) for n in names}, names)
        self.assertTrue(0.7 <= o.get("shift", 1) <= 1.4 and 0.7 <= o.get("speed", 1) <= 1.3, names)

    def test_SND_018_every_role_has_voices(self):
        roles = set(M.read(RULES, "roles.json")["roles"])
        waiting = set(V["roles_not_voiced"])                 # roles with no pool yet: the owner has not picked one
        self.assertEqual(set(V["roles"]) | waiting, roles)
        self.assertFalse(set(V["roles"]) & waiting)
        for role, pool in V["roles"].items():
            self.assertGreaterEqual(len(pool), 2, role + " needs more than one voice")
            for o in pool + list(V["player"]["voices"].values()):
                self.sound(o)
        self.assertIn(V["player"]["default"], V["player"]["voices"])

    def test_SND_018_the_owners_picks_are_kept(self):
        picks = {role: o for role, pool in V["roles"].items() for o in pool if o.get("pick")}
        self.assertEqual(picks["cafe"]["voice"], "af_bella")                                  # Mina, voice C
        self.assertEqual((picks["child"]["voice"], picks["child"]["shift"]), ("af_nova", 1.24))   # Bo, voice B
        self.assertEqual(picks["nurse"]["voice"], "bm_george")                                # Tomas, voice B
        self.assertEqual((picks["elder"]["voice"], picks["elder"]["shift"]), ("af_sarah", 0.92))  # Edda, voice B
        self.assertEqual(picks["commuter"]["voice"], "bm_lewis")                              # Dev, voice B
        moods = M.read(RULES, "dialogue.json")["moods"]
        self.assertEqual(set(V["moods"]), set(moods))
        self.assertEqual(V["moods"]["sad"]["semitones"], -1.5)                               # the level the owner kept

    def test_SND_018_each_resident_always_has_the_same_voice(self):
        _, _, res, _ = M.world()
        a, b = M.speakers(V, res), M.speakers(V, res)
        self.assertEqual(a, b)
        for r, o in zip(res, a):
            if r["id"] in V["people"]:
                self.assertIs(o, V["people"][r["id"]])
            elif r["role"] in V["roles"]:
                self.assertIn(o, V["roles"][r["role"]])
            else:
                self.assertIsNone(o)                         # a role with no pool: not voiced, and the tool still runs

    def test_SND_018_a_named_resident_speaks_in_their_own_voice(self):
        _, _, res, _ = M.world()
        self.assertEqual(M.people_problems(V, res), [])      # the list as it is: residents, by id and name
        for o in V["people"].values():
            self.sound(o, mixed=True)
        # tried on two residents with voices of this test's own (the list itself is only the owner's picks)
        one = next(r for r in res if r["role"] == "cafe")
        two = next(r for r in res if r["role"] == "creative")   # a role with no pool: a named resident there is voiced all the same
        W = copy.deepcopy(V)
        W["people"] = {one["id"]: {"name": one["name"], "voice": "bf_alice", "lang": "en-gb", "shift": 0.95},
                       two["id"]: {"name": two["name"], "voice": ["bm_george", "af_bella"], "lang": "en-gb"}}
        for o in W["people"].values():
            self.sound(o, mixed=True)
        was, now = M.speakers(V, res), M.speakers(W, res)
        at = {r["id"]: i for i, r in enumerate(res)}
        self.assertEqual(M.cfg_id(now[at[one["id"]]]), "bf_alice_p95")
        self.assertEqual(M.cfg_id(now[at[two["id"]]]), "af_bella-bm_george")          # two voices mixed, either way round
        self.assertEqual(M.cfg_id({"voice": ["af_bella", "bm_george"]}), "af_bella-bm_george")
        for i, r in enumerate(res):
            if r["id"] not in W["people"]:
                self.assertEqual(now[i], was[i])             # everyone else: their pool voice, as before
        # the plan voices their lines in that voice: their own name, and the fixed pieces of the talks they can have
        _, _, _, jobs = M.plan(W)
        said = {(c, t) for c, _, t in jobs}
        self.assertIn(("bf_alice_p95", one["name"]), said)
        self.assertIn(("af_bella-bm_george", two["name"]), said)
        self.assertGreater(sum(1 for c, _ in said if c == "bf_alice_p95"), 10)
        self.assertGreater(sum(1 for c, _ in said if c == "af_bella-bm_george"), 10)
        mixed = [o for (c, _, _), o in jobs.items() if c == "af_bella-bm_george"]
        self.assertTrue(all(o["voice"] == ["bm_george", "af_bella"] for o in mixed))   # what the model is asked for: both voices
        # and the slice is told so, by their id
        index = M.index_of(W, res, now, {})
        self.assertEqual(index["configs"][index["residents"][one["id"]]], "bf_alice_p95")
        self.assertEqual(index["configs"][index["residents"][two["id"]]], "af_bella-bm_george")
        # a name that is not that resident's, or an id that is nobody's, stops the tool
        W["people"] = {one["id"]: {"name": "Nobody", "voice": "bf_alice", "lang": "en-gb"}, "r_none": {"name": "X", "voice": "bf_alice", "lang": "en-gb"}}
        self.assertEqual(len(M.people_problems(W, res)), 2)
        self.assertRaises(SystemExit, M.plan, W)

    def test_SND_018_the_index_names_residents_by_their_id(self):
        _, _, res, _ = M.world()
        who = M.speakers(V, res)
        want = M.index_of(V, res, who, {})
        self.assertEqual(set(want["residents"]), {r["id"] for r, o in zip(res, who) if o})
        for r, o in zip(res, who):
            if o:
                self.assertEqual(want["configs"][want["residents"][r["id"]]], M.cfg_id(o))
        src = open(os.path.join(M.ROOT, "Prototype", "play", "voices.js"), encoding="utf-8").read()
        self.assertIn("V.residents[idOf(who)]", src)                                  # the slice looks a voice up by id too
        if not os.path.exists(INDEX):
            self.skipTest("no voices made yet: run make_dialogue_voices.py")
        made = M.read(INDEX)                                 # the index in the slice says who has which voice as the rule does today
        self.assertTrue(isinstance(made["residents"], dict), "the index is by place in the list: run make_dialogue_voices.py --index")
        self.assertEqual({k: made["configs"][v] for k, v in made["residents"].items()},
                         {k: want["configs"][v] for k, v in want["residents"].items()}, "run make_dialogue_voices.py --index")

    def test_SND_018_lines_are_cut_at_their_blanks(self):
        self.assertEqual(M.pieces("{greeting}, young one. Stay a while, if you like."),
                         [("blank", "greeting"), ("text", "young one. Stay a while, if you like.")])
        self.assertEqual(M.pieces("{greeting}! You must be {player}, from across the courtyard."),
                         [("blank", "greeting"), ("text", "You must be"), ("blank", "player"), ("text", "from across the courtyard.")])
        src = open(os.path.join(M.ROOT, "Prototype", "play", "voices.js"), encoding="utf-8").read()
        self.assertIn(r'replace(/^[\s,;:.!?—-]+/, "")', src)                          # the slice cuts pieces the same way

    def test_SND_018_every_line_can_be_spoken(self):
        if not os.path.exists(INDEX):
            self.skipTest("no voices made yet: run make_dialogue_voices.py")
        clips = M.read(INDEX)["clips"]
        _, _, _, jobs = M.plan()
        missing = [k for k in jobs if "|".join(k) not in clips]
        if not V.get("status", {}).get("complete", True):        # lines still being revised: voiced in part, on purpose
            self.skipTest("dialogue not final: %d of %d pieces voiced" % (len(jobs) - len(missing), len(jobs)))
        self.assertEqual(missing[:10], [], "%d pieces not voiced: run make_dialogue_voices.py" % len(missing))
        for rel, dur in clips.values():
            self.assertTrue(os.path.exists(os.path.join(M.OUT, rel)), rel)
            self.assertGreater(dur, 0.1, rel)


if __name__ == "__main__":
    unittest.main()
