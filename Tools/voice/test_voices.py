"""Checks on spoken dialogue (SND-018, ADR-0019): the voice cast, the owner's picks, and that every line can be spoken.

    python -m unittest test_voices        (from Tools/voice; needs no voice model)
"""
import json, os, re, unittest

import make_dialogue_voices as M

RULES = M.RULES
V = M.read(RULES, "voices.json")
KOKORO = {"af_alloy", "af_aoede", "af_bella", "af_heart", "af_jessica", "af_kore", "af_nicole", "af_nova", "af_river", "af_sarah", "af_sky",
          "am_adam", "am_echo", "am_eric", "am_fenrir", "am_liam", "am_michael", "am_onyx", "am_puck", "am_santa",
          "bf_alice", "bf_emma", "bf_isabella", "bf_lily", "bm_daniel", "bm_fable", "bm_george", "bm_lewis"}   # Kokoro v1.0 English voices
INDEX = os.path.join(M.OUT, "index.json")


class TheVoices(unittest.TestCase):

    def test_SND_018_every_role_has_voices(self):
        roles = set(M.read(RULES, "roles.json")["roles"])
        self.assertEqual(set(V["roles"]), roles)
        for role, pool in V["roles"].items():
            self.assertGreaterEqual(len(pool), 2, role + " needs more than one voice")
            for o in pool + list(V["player"]["voices"].values()):
                self.assertIn(o["voice"], KOKORO)
                self.assertEqual(o["lang"], "en-gb" if o["voice"][0] == "b" else "en-us", o["voice"])   # the voice's own accent
                self.assertTrue(0.7 <= o.get("shift", 1) <= 1.4 and 0.7 <= o.get("speed", 1) <= 1.3, o["voice"])
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
        self.assertEqual([M.cfg_id(o) for o in a], [M.cfg_id(o) for o in b])
        for r, o in zip(res, a):
            self.assertIn(o, V["roles"][r["role"]])

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
