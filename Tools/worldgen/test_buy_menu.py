"""Buy menus that show what is for sale as objects (docs/specs/domains/MENU.md UI-020, UI-021; ADR-0044).   python -m unittest test_buy_menu"""
import copy
import json
import os
import unittest

import world

W = world.load()
B = W["buy_menu"]
HERE = os.path.dirname(os.path.abspath(__file__))
PLAY = os.path.join(HERE, "..", "..", "Prototype", "play")


def read(name):
    with open(os.path.join(PLAY, name), encoding="utf-8") as f:
        return f.read()


def bad(change):
    """The problems world.buy_menu_problems finds after change(w) has spoilt a copy of the world."""
    w = copy.deepcopy(W)
    change(w)
    return world.buy_menu_problems(w)


class TheBuyMenus(unittest.TestCase):
    def test_UI_020_both_layouts_have_room_for_what_they_show(self):
        # the owner, 2026-11-24, of look test 33: "B, and D when narrow"
        self.assertEqual(world.buy_menu_problems(W), [])
        wide, narrow = B["wide"], B["narrow"]
        self.assertGreater(wide["panel_px"], B["narrow_px"])                      # a counter's own panel is the wide layout (B)
        self.assertLess(narrow["panel_px"], B["narrow_px"])                      # the designer's side panel is the narrow one (D)
        self.assertEqual(narrow["columns"], 4)                                   # layout D as the owner saw it: four across
        side = read("index.html")
        self.assertIn("#life.side { left: auto; right: 12px; top: 12px; transform: none; width: min(%dpx" % narrow["panel_px"], side)   # the width the rules count on
        self.assertIn("#life.wide { width: min(var(--bm-panel, %dpx)" % wide["panel_px"], side)
        self.assertTrue(any("do not fit a narrow panel" in p for p in bad(lambda w: w["buy_menu"]["narrow"].update(columns=6))))
        self.assertTrue(any("no room for three cells" in p for p in bad(lambda w: w["buy_menu"]["wide"].update(show_px=480, cell_px=200))))
        self.assertTrue(any("narrow_px" in p for p in bad(lambda w: w["buy_menu"].update(narrow_px=200))))

    def test_UI_020_every_place_a_thing_goes_has_its_words_and_a_mark_that_reads(self):
        boards = W["settings"]["menu"]["colours"]
        lum = lambda c: sum(k * ((v / 255 / 12.92) if v / 255 <= .03928 else ((v / 255 + .055) / 1.055) ** 2.4) for k, v in zip((.2126, .7152, .0722), (int(c[i:i + 2], 16) for i in (1, 3, 5))))
        for goes in list(W["mall_goods"]["goes"]) + ["place"]:
            G = B["goes"][goes]
            self.assertTrue(G["name"].strip() and G["to"].strip(), goes)
            a, b = sorted((lum(boards[G["colour"]][0]), lum(boards[G["colour"]][1])), reverse=True)
            self.assertGreaterEqual((a + .05) / (b + .05), 3.0, goes)            # the mark's lettering reads on its colour
        marks = [B["goes"][g]["colour"] for g in W["mall_goods"]["goes"]]
        self.assertEqual(len(set(marks)), len(marks))                            # home, wardrobe, carried and eaten are told apart by colour
        self.assertTrue(any("no words for where a thing goes: wear" in p for p in bad(lambda w: w["buy_menu"]["goes"].pop("wear"))))
        self.assertTrue(any("no board colour pink" in p for p in bad(lambda w: w["buy_menu"]["goes"]["home"].update(colour="pink"))))

    def test_UI_020_every_counter_of_the_slice_uses_the_grid(self):
        # "all the buy menus (game wide)": a shop's counter (buying and selling), a mall outlet's, the designer's list of pieces
        life, page = read("life.js"), read("index.html")
        for use in ('wares("shop:" + P.n + ":buy"', 'wares("shop:" + P.n + ":sell"', 'wares("outlet:" + P.n + ":" + P.u', 'BuyMenu.grid({ id: "furnish"'):
            self.assertIn(use, life)
        self.assertNotIn('btn("Buy", () => buyAt', life)                         # the old rows of text are gone
        self.assertNotIn('btn("Buy", () => { const p = E.buy', life)
        self.assertLess(page.index('<script src="buymenu.js">'), page.index('<script src="life.js">'))
        self.assertIn("BuyMenu.key(e)", life)                                    # arrows choose, Enter buys
        self.assertIn("BuyMenu.selfTest()", life)                                # and the page checks itself against this file's rules

    def test_UI_021_every_thing_for_sale_shows_an_object_or_a_stand_in(self):
        text = read("goods_models.js")
        models = json.loads(text[text.index("window.GOODS_MODELS = ") + len("window.GOODS_MODELS = "):text.rstrip().rindex(";")])["models"]
        pieces, goods, items = W["furnishing"]["pieces"], W["mall_goods"]["goods"], W["economy"]["items"]
        things = world.buy_menu_things(W)
        self.assertEqual(len(things), len(goods) + len([i for i in items.values() if not i.get("quest")]) + sum(len(r) for r in W["furnishing"]["rooms"].values()))
        own = 0
        for t in things:
            s = world.buy_menu_shows(W, t)
            self.assertIsNotNone(s, t)
            if s[0] == "piece":
                own += 1
                self.assertIn(s[1], pieces, t)
                if pieces[s[1]].get("from") == "mall":
                    self.assertIn(s[1], models, t)                               # a good for the home shows the object made in Blender (FURN-065)
            else:
                self.assertEqual(s[0], "shape", t)
                self.assertIn(s[1], B["shapes"], t)
        self.assertEqual(own, 49 + 42)                                           # the mall's goods for the home and the designer's pieces show themselves
        for t, G in goods.items():                                               # a stand-in is never passed off as the thing: only what has no object shows one
            self.assertEqual(world.buy_menu_shows(W, t)[0], "piece" if G["goes"] == "home" else "shape", t)
        self.assertIsNone(world.buy_menu_shows(W, "no such thing"))

    def test_UI_021_clothes_show_their_own_colour(self):
        self.assertEqual(world.buy_menu_shows(W, "tee_teal"), ["shape", "top", "teal"])
        self.assertEqual(world.buy_menu_shows(W, "trousers_brown"), ["shape", "bottom", "brown"])
        self.assertEqual(world.buy_menu_shows(W, "bag_shoulder"), ["shape", "satchel", None])
        palette = W["designer"]["palette"]
        for t, G in W["mall_goods"]["goods"].items():
            if G["goes"] == "wear":
                s = world.buy_menu_shows(W, t)
                uses_own = any(b[6] == "own" for b in B["shapes"][s[1]])
                self.assertEqual(uses_own, s[2] is not None, t)
                if s[2]:
                    self.assertIn(s[2], palette, t)

    def test_UI_021_a_stand_in_is_small_and_in_the_towns_colours(self):
        for name, boxes in B["shapes"].items():
            self.assertLessEqual(len(boxes), 12, name)                           # a few boxes, not a model
            for b in boxes:
                self.assertLessEqual(b[1] + b[4], .5, name)
        for k, c in B["colours"].items():
            self.assertTrue(all(.02 <= v <= .85 for v in c), k)                  # muted: nothing pure black, white or bright (ART_DIRECTION W4)
        for k, K in W["mall_outlets"]["kinds"].items():
            self.assertIn(K["sign"], B["colours"], k)                            # an outlet's parcels carry its sign's colour
        for k in W["economy"]["shops"]:
            self.assertIn(B["bands"][k], B["colours"], k)

    def test_UI_021_a_bad_buy_menu_is_caught(self):
        self.assertTrue(any("is no shape" in p for p in bad(lambda w: w["buy_menu"]["stand_in"]["things"].update(coffee="mug"))))
        self.assertTrue(any("neither a good nor an item" in p for p in bad(lambda w: w["buy_menu"]["stand_in"]["things"].update(unicorn="cup"))))
        self.assertTrue(any("uses colour gold" in p for p in bad(lambda w: w["buy_menu"]["shapes"]["cup"][0].__setitem__(6, "gold"))))
        self.assertTrue(any("bigger than a stand-in may be" in p for p in bad(lambda w: w["buy_menu"]["shapes"]["parcel"][0].__setitem__(4, .9))))
        self.assertTrue(any("no stand-in for things to eat" in p for p in bad(lambda w: w["buy_menu"]["stand_in"]["goes"].pop("eat"))))
        self.assertTrue(any("shows nothing" in p for p in bad(lambda w: w["buy_menu"]["stand_in"]["wear"].pop(0))))            # T-shirts and coats would show nothing
        self.assertTrue(any("needs a colour of its own" in p for p in bad(lambda w: w["buy_menu"]["stand_in"]["things"].update(coffee="top"))))
        self.assertTrue(any("no band colour for the cafe" in p for p in bad(lambda w: w["buy_menu"]["bands"].pop("cafe"))))
        self.assertTrue(any("three numbers" in p for p in bad(lambda w: w["buy_menu"]["colours"].update(card=[1, 1, 1]))))
        self.assertTrue(any("picture.turn_s" in p for p in bad(lambda w: w["buy_menu"]["picture"].update(turn_s=0))))
        with open(os.path.join(HERE, "world.py"), encoding="utf-8") as f:
            self.assertIn("buy_menu_problems(w)", f.read().split("def validate(w):")[1].split("\ndef ")[0])   # checked whenever the world is

    def test_UI_021_the_slice_gets_the_rules_and_the_answers_to_check_itself(self):
        text = read("city_data.js")
        i = text.index('"buy_menu"')
        self.assertGreater(i, 0)
        got = json.JSONDecoder().raw_decode(text[text.index("{", i):])[0]
        self.assertEqual(got["rules"], {k: v for k, v in B.items() if k != "note"})   # run python build_play.py after changing buy_menu.json
        self.assertEqual(got["checks"], world.buy_menu_checks(W))


if __name__ == "__main__":
    unittest.main()
