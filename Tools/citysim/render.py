"""Draws the city-in-a-box simulation as top-down clips. Only drawing lives here; every position, colour
state and count comes from citysim.py.

    python render.py day   <frames_dir>     one summer Wednesday, with a player walking round and bodies loading near them
    python render.py years <frames_dir>     five years at one frame a day: trees grow, seasons turn
    python render.py still <png_path>       one labelled picture

Needs Pillow and numpy. Turn the frames into a video with ffmpeg (see README.md).
"""
import math, os, random, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import citysim as cs

SS = 2                       # draw at twice the size, then shrink: smooth edges
PX = 5.6 * SS                # pixels per metre
MAP = int(160 * PX)
W, H = 1400 * SS, 940 * SS
OX, OY = 22 * SS, 22 * SS
PANEL_X = OX + MAP + 34 * SS
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
if not os.path.exists(FONT):                                   # Windows
    FONT, BOLD = "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/segoeuib.ttf"
F = lambda size, bold=False: ImageFont.truetype(BOLD if bold else FONT, int(size * SS))

INK, PAPER, MUTE = (30, 36, 64), (236, 232, 226), (110, 112, 130)
COL = dict(outer=(176, 168, 158), street=(196, 189, 180), court=(205, 197, 186), lawn=(134, 150, 104), kerb=(150, 143, 136),
           home=(170, 128, 104), shop=(186, 150, 92), food=(168, 104, 84), work=(132, 122, 128), health=(196, 190, 178), utility=(112, 110, 118),
           track=(120, 116, 116), water=(96, 146, 150), wood=(120, 90, 64), lantern=(255, 176, 92))
ACT_GROUPS = [("Asleep", ("sleep",)), ("At home", ("home",)), ("Working", ("work", "sell", "tend", "deliver", "inspect")),
              ("Eating, shopping", ("eat", "shop", "visit")), ("Resting, strolling", ("rest", "stroll")), ("Walking", ("walk",)),
              ("Waiting for the tram", ("wait",)), ("Away by tram", ("away",))]
GROUP_COL = [(96, 100, 128), (150, 138, 150), (60, 110, 106), (184, 138, 46), (134, 150, 104), (200, 110, 70), (120, 150, 190), (190, 190, 190)]


def P(p):
    return (OX + p[0] * PX, OY + p[1] * PX)


def rgb(c, k=1.0):
    return tuple(int(255 * min(1.0, max(0.0, v * k)) ** (1 / 1.9)) for v in c)


def box(d, r, fill, outline=None, width=1):
    d.rectangle([P((r[0], r[1])), P((r[2], r[3]))], fill=fill, outline=outline, width=int(width * SS))


def dot(d, p, r_m, fill, outline=None, width=1.0):
    x, y = P(p)
    r = r_m * PX
    d.ellipse([x - r, y - r, x + r, y + r], fill=fill, outline=outline, width=max(1, int(width * SS)))


class Painter:
    def __init__(self, city):
        self.c = city
        self.base = {}
        self.glow = self._glow()

    # ---- the ground and the buildings: drawn once per kind of day ----
    def _base(self, market):
        if market in self.base:
            return self.base[market]
        c, d0 = self.c, self.c.d
        im = Image.new("RGB", (W, H), PAPER)
        d = ImageDraw.Draw(im)
        box(d, (0, 0, 160, 160), COL["outer"])
        box(d, (14, 14, 146, 146), COL["street"])
        box(d, (30, 30, 130, 130), COL["court"])
        for a, b in ((0, 14), (146, 160)):                         # neighbouring blocks, just hinted
            for k in range(4):
                box(d, (2 + k * 40, a + 1.5, 38 + k * 40, b - 1.5) if a == 146 else (2 + k * 40, a + 1.5, 38 + k * 40, b - 4), (166, 158, 150))
                box(d, (a + 1.5, 20 + k * 32, b - 1.5, 46 + k * 32), (166, 158, 150))
        d.line([P((0, 20)), P((160, 20))], fill=COL["track"], width=int(1.2 * SS))
        d.line([P((0, 21.6)), P((160, 21.6))], fill=COL["track"], width=int(1.2 * SS))
        box(d, (64, 64, 96, 96), COL["lawn"])
        for l in d0["lots"]:
            if l["kind"] in ("garden", "market"):
                continue
            if l["kind"] == "stop":
                box(d, l["rect"], (150, 160, 168), INK)
                continue
            box(d, l["rect"], COL[l["kind"]], (70, 62, 60), 1.2)
            x0, y0, x1, y1 = l["rect"]
            d.line([P((x0 + 2, (y0 + y1) / 2)), P((x1 - 2, (y0 + y1) / 2))] if x1 - x0 >= y1 - y0 else [P(((x0 + x1) / 2, y0 + 2)), P(((x0 + x1) / 2, y1 - 2))],
                   fill=tuple(int(v * 0.86) for v in COL[l["kind"]]), width=int(1.5 * SS))
            dot(d, l["door"], 0.9, (60, 48, 44))
        for r in ((74, 30, 86, 52), (74, 108, 86, 130), (30, 74, 52, 86), (108, 74, 130, 86)):     # archways over the passages
            box(d, r, (150, 140, 132), (70, 62, 60), 1.2)
            box(d, (r[0] + 2.5, r[1] + 2.5, r[2] - 2.5, r[3] - 2.5), COL["court"])
        dot(d, (80, 80), 3.2, (150, 143, 136), (90, 86, 84))
        dot(d, (80, 80), 2.3, COL["water"])
        g = c.lots["garden"]["objects"]
        for i in range(0, len(g["bench"]), 2):
            a, b = g["bench"][i], g["bench"][i + 1]
            d.line([P(a), P(b)], fill=COL["wood"], width=int(5 * SS))
        for seat in c.lots["cafe"]["objects"]["seat"][4:]:
            dot(d, (seat[0], seat[1] - 1.1), 0.75, (224, 214, 196), (120, 100, 90))
        if market:
            for i, st in enumerate(c.lots["market"]["objects"]["stall"]):
                box(d, (st[0] - 2.4, st[1] - 2.0, st[0] + 2.4, st[1] + 1.6), [(176, 96, 80), (60, 110, 106), (184, 138, 46)][i], (70, 62, 60))
        for l in d0["lots"]:
            if l["kind"] in ("garden", "market", "stop"):
                continue
            x0, y0, x1, y1 = l["rect"]
            light = l["kind"] in ("health", "shop")
            d.text(P(((x0 + x1) / 2, (y0 + y1) / 2 - (7 if l["kind"] == "home" else 0))), l["name"], font=F(10.5, True), fill=INK if light else (245, 240, 232), anchor="mm")
        d.text(P((80, 8.2)), "Tram stop", font=F(9.5, True), fill=INK, anchor="mm")
        self.base[market] = im
        return im

    def _glow(self):
        g = np.zeros((H, W, 3), np.float32)
        yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
        for lp in self.c.d["lamps"]:
            x, y = P(lp)
            r = 15 * PX
            x0, x1, y0, y1 = int(max(0, x - r)), int(min(W, x + r)), int(max(0, y - r)), int(min(H, y + r))
            dd = np.hypot(xx[y0:y1, x0:x1] - x, yy[y0:y1, x0:x1] - y) / r
            g[y0:y1, x0:x1] += (np.clip(1 - dd, 0, 1) ** 2)[..., None] * np.array([1.0, 0.62, 0.30], np.float32) * 150
        return g

    # ---- plants ----
    def _plants(self, d, day):
        c = self.c
        rng = random.Random(day // 3)
        fl = c.veg.flowers(day)
        for bed, colr in zip(c.d["flowers"]["beds"], c.d["flowers"]["colors"]):
            box(d, (bed[0] - 3.2, bed[1] - 1.5, bed[0] + 3.2, bed[1] + 1.5), (104, 86, 66))
            if fl["size"] > 0:
                for k in range(14):
                    px, py = bed[0] - 2.8 + (k % 7) * 0.93, bed[1] - 0.7 + (k // 7) * 1.4
                    dot(d, (px, py), 0.28 + 0.34 * fl["size"], rgb(colr) if fl["bloom"] > 0.3 else (96, 132, 80))
        trees = [(t, c.veg.tree(t, day)) for t in c.d["trees"]]
        for t, s in trees:                                                # fallen leaves under the trees
            if s["state"] == "growing" and s["ground"] > 0.02:
                r2 = random.Random(t["seed"] * 977 + day // 2)
                for k in range(int(46 * s["ground"] * s["radius"] / 4.0)):
                    a, rr = r2.random() * 6.283, s["radius"] * (0.3 + 0.95 * r2.random())
                    dot(d, (t["pos"][0] + math.cos(a) * rr, t["pos"][1] + math.sin(a) * rr), 0.34, rgb(s["autumn"], 0.9 + 0.3 * r2.random()))
        return trees

    def _crowns(self, im, trees, day):
        ov = Image.new("RGBA", im.size, (0, 0, 0, 0))
        d = ImageDraw.Draw(ov)
        for t, s in trees:
            p = t["pos"]
            if s["state"] == "empty":
                dot(d, p, 0.5, (120, 100, 84, 255))
                continue
            if s["state"] == "fallen":
                x, y = P(p)
                L = s["radius"] * 1.6 * PX
                d.line([(x - L * 0.2, y - L * 0.1), (x + L, y + L * 0.45)], fill=(92, 70, 52, 255), width=int(0.9 * PX))
                dot(d, p, 0.9, (120, 96, 72, 255))
                continue
            r = s["radius"]
            dot(d, (p[0] + 0.5, p[1] + 0.7), r, (40, 40, 50, 46))        # shadow
            if s["leaf"] < 0.98:                                         # branches show through thin or missing leaves
                for k in range(7):
                    a = k * 0.9 + t["seed"]
                    d.line([P(p), P((p[0] + math.cos(a) * r * 0.9, p[1] + math.sin(a) * r * 0.9))], fill=(96, 78, 62, 230), width=max(1, int(0.22 * PX)))
            if s["leaf"] > 0.02:
                col = rgb(s["color"])
                dot(d, p, r * (0.55 + 0.45 * s["leaf"]), col + (int(70 + 165 * s["leaf"]),), tuple(int(v * 0.72) for v in col) + (int(200 * s["leaf"]),), 1.0)
                dot(d, (p[0] - r * 0.22, p[1] - r * 0.25), r * 0.45 * s["leaf"], tuple(min(255, int(v * 1.16)) for v in col) + (int(150 * s["leaf"]),))
            if s["blossom"] > 0:
                r2 = random.Random(t["seed"])
                for k in range(int(10 + r * 9)):
                    a, rr = r2.random() * 6.283, r * 0.9 * math.sqrt(r2.random())
                    dot(d, (p[0] + math.cos(a) * rr, p[1] + math.sin(a) * rr), 0.36, (244, 214, 222, 245))
            dot(d, p, 0.42, (86, 66, 50, 255))
        im.alpha_composite(ov)

    # ---- one picture ----
    def frame(self, t, player=None, radius=40.0, pace="", footer=""):
        c = self.c
        day, m = c.clock.split(t)
        info = c.clock.info(day)
        state = c.at(t)
        wx = cs.weather(c.d, c.clock, t)
        light = c.clock.daylight(t)
        im = self._base(info["market"] and 7 * 60 + 20 <= m <= 13 * 60 + 40).convert("RGBA")
        d = ImageDraw.Draw(im)
        trees = self._plants(d, day)
        for v in c.vehicles(t):
            x, y = v["pos"]
            if v["kind"] == "tram":
                box(d, (x - 7, y - 1.7, x + 7, y + 1.7), (176, 96, 80), INK, 1.2)
                box(d, (x - 5.5, y - 1.0, x + 5.5, y + 1.0), (228, 220, 204))
            else:
                box(d, (x - 2.4, y - 1.3, x + 2.4, y + 1.3), (60, 110, 106), INK, 1.2)
        self._crowns(im, trees, day)
        # night, cloud and rain change the light of the whole map
        arr = np.asarray(im.convert("RGB"), np.float32)
        dark = 1.0 - light
        dim = {"clear": 1.0, "cloud": 0.90, "rain": 0.78, "fog": 0.94}[wx]
        tint = np.array([1.0 - 0.62 * dark, 1.0 - 0.56 * dark, 1.0 - 0.40 * dark], np.float32) * dim
        if wx in ("rain", "cloud"):
            grey = arr.mean(axis=2, keepdims=True)
            arr = arr * 0.72 + grey * 0.28
        if wx == "fog":
            arr = arr * 0.62 + 225 * 0.38
        arr *= tint
        lit = np.zeros_like(arr)
        if dark > 0.05:
            lw = Image.new("RGB", (W, H), (0, 0, 0))
            ld = ImageDraw.Draw(lw)
            awake = {}
            for rid, s in state.items():
                if s["state"] == "inside" and s["act"] != "sleep":
                    awake[s["place"]] = awake.get(s["place"], 0) + 1
            for lid, n in awake.items():
                x0, y0, x1, y1 = c.lots[lid]["rect"]
                for k in range(min(4, n + 1)):
                    wx0 = x0 + 3 + k * (x1 - x0 - 6) / 4.0
                    box(ld, (wx0, y0 + 3, wx0 + 3, y0 + 6), (255, 190, 110))
                    box(ld, (wx0, y1 - 6, wx0 + 3, y1 - 3), (255, 190, 110))
            lit = (np.asarray(lw, np.float32) * 0.85 + self.glow) * dark
        arr = np.clip(arr + lit, 0, 255)
        arr[:, OX + MAP:, :] = PAPER
        arr[:OY, :, :] = PAPER
        arr[OY + MAP:, :, :] = PAPER
        arr[:, :OX, :] = PAPER
        im = Image.fromarray(arr.astype(np.uint8)).convert("RGBA")
        ov = Image.new("RGBA", im.size, (0, 0, 0, 0))
        d = ImageDraw.Draw(ov)
        loaded = c.loaded(state, [player], radius) if player else set(state)
        if player:
            x, y = P(player)
            r = radius * PX
            d.ellipse([x - r, y - r, x + r, y + r], fill=(255, 255, 255, 26), outline=(255, 255, 255, 200), width=int(1.6 * SS))
        k = 0.72 + 0.28 * light
        for rid, s in state.items():
            if s["pos"] is None:
                continue
            col = rgb(c.d["roles"][c.res[rid]["role"]]["color"], 1.0)
            if rid in loaded:
                big = 1.25 if s["state"] != "inside" else 0.95
                dot(d, s["pos"], big, tuple(int(v * k) for v in col) + (255,), (20, 22, 34, 255) if s["state"] != "inside" else (250, 246, 238, 255), 1.3)
                if s["act"] == "sleep":
                    dot(d, s["pos"], 0.4, (20, 22, 34, 255))
            else:                                                       # exists only as data: a faint ring
                dot(d, s["pos"], 0.95, None, tuple(int(v * k) for v in col) + (235,), 1.5)
        if player:
            dot(d, player, 1.9, (255, 255, 255, 255), INK + (255,), 1.6)
            dot(d, player, 0.9, COL["lantern"] + (255,))
        if wx == "rain":
            rr = random.Random(int(t * 30))
            for _ in range(520):
                x, y = OX + rr.random() * MAP, OY + rr.random() * MAP
                d.line([(x, y), (x - 5 * SS, y + 13 * SS)], fill=(214, 226, 240, 120), width=SS)
        d.rectangle([OX, OY, OX + MAP, OY + MAP], outline=INK + (255,), width=int(1.5 * SS))
        im.alpha_composite(ov)
        self._panel(ImageDraw.Draw(im), info, m, wx, state, loaded, player, radius, pace, footer, trees)
        return im.convert("RGB").resize((W // SS, H // SS), Image.LANCZOS)

    def _panel(self, d, info, m, wx, state, loaded, player, radius, pace, footer, trees):
        c, x, y = self.c, PANEL_X, OY
        d.text((x, y), "Old Town, block 01", font=F(15, True), fill=MUTE)
        y += 26 * SS
        d.text((x, y), "%s %02d:%02d" % (info["weekday_name"], int(m // 60), int(m % 60)), font=F(46, True), fill=INK)
        y += 62 * SS
        d.text((x, y), "%s, day %d of %d, year %d" % (info["season_name"], info["day_of_season"], c.clock.season_days, info["year"]), font=F(17), fill=INK)
        y += 26 * SS
        notes = [dict(clear="Clear", cloud="Cloudy", rain="Raining", fog="Morning fog")[wx]]
        if info["market"]:
            notes.append("market day")
        if info["sunday"]:
            notes.append("shops closed")
        d.text((x, y), ", ".join(notes), font=F(17), fill=INK)
        y += 26 * SS
        if pace:
            d.text((x, y), pace, font=F(14, True), fill=(154, 59, 46))
        y += 40 * SS
        d.text((x, y), "What %d residents are doing" % len(state), font=F(15, True), fill=INK)
        y += 26 * SS
        for (label, acts), col in zip(ACT_GROUPS, GROUP_COL):
            n = sum(1 for s in state.values() if s["act"] in acts)
            d.rectangle([x, y + 2 * SS, x + 8 * SS + n * 11 * SS, y + 17 * SS], fill=col)
            d.text((x + 16 * SS + n * 11 * SS, y), "%d  %s" % (n, label), font=F(14), fill=INK)
            y += 23 * SS
        y += 16 * SS
        if player:
            d.text((x, y), "%d of %d have bodies" % (len(loaded), len(state)), font=F(24, True), fill=INK)
            y += 34 * SS
            for line in ("Only residents within %d m of the player" % radius, "(white circle) are loaded. The rest exist as", "data: faint rings, placed by their routine."):
                d.text((x, y), line, font=F(13.5), fill=MUTE)
                y += 19 * SS
        else:
            g = [s for t, s in trees if t["species"] == "linden" and s["state"] == "growing"]
            if g:
                d.text((x, y), "Garden lindens: %.1f m across" % (2 * sum(s["radius"] for s in g) / len(g)), font=F(20, True), fill=INK)
                y += 30 * SS
            gone = [t for t, s in trees if s["state"] != "growing"]
            d.text((x, y), "Planted on day 1. Size comes from age," if not gone else "A tree has come down; a sapling follows.", font=F(13.5), fill=MUTE)
            y += 19 * SS
            d.text((x, y), "leaves from the date. Nothing is stepped.", font=F(13.5), fill=MUTE)
            y += 19 * SS
        y += 18 * SS
        d.text((x, y), "Who is who", font=F(15, True), fill=INK)
        y += 25 * SS
        roles = {}
        for r in c.d["residents"]:
            roles[r["role"]] = roles.get(r["role"], 0) + 1
        for i, (role, n) in enumerate(roles.items()):
            cx, cy = x + (i % 2) * 205 * SS, y + (i // 2) * 22 * SS
            col = rgb(c.d["roles"][role]["color"])
            d.ellipse([cx, cy + 2 * SS, cx + 13 * SS, cy + 15 * SS], fill=col, outline=INK, width=SS)
            d.text((cx + 20 * SS, cy), "%s (%d)" % (role, n), font=F(13.5), fill=INK)
        if footer:
            d.text((x, OY + MAP - 18 * SS), footer, font=F(12.5), fill=MUTE)


TOUR = [(63, 23), (80, 22), (80, 62), (98, 62), (98, 80), (140, 80), (140, 140), (80, 140), (80, 98), (62, 98), (62, 80), (20, 80), (20, 20), (63, 23)]


def day_frames(out):
    c = cs.City()
    paint = Painter(c)
    day = 16
    windows = [(cs.hm("07:45"), cs.hm("07:55")), (cs.hm("12:28"), cs.hm("12:38")), (cs.hm("17:28"), cs.hm("17:38"))]
    cum = cs.lengths(TOUR)
    t, end, walked, n = cs.hm("05:30"), cs.hm("23:30"), 0.0, 0
    os.makedirs(out, exist_ok=True)
    while t < end:
        real = any(a <= t < b for a, b in windows)
        step = 2.0 / 60.0 if real else 3.0
        if not real:
            nxt = min([a for a, b in windows if a > t] or [end])
            step = min(step, nxt - t)
        player = cs.along(TOUR, cum, walked % cum[-1])
        im = paint.frame(day * 1440 + t, player, 40.0, "Near real pace: 1 second = 48 game seconds" if real else "Fast forward: 1 second = 72 game minutes",
                         "Data: Tools/citysim/data/block_01.json. Rules: citysim.py")
        im.save(os.path.join(out, "f_%04d.png" % n), compress_level=1)
        if real:
            walked += 0.85 * 60.0 * step
        t += step
        n += 1
    print("day:", n, "frames")


def years_frames(out, years=5):
    c = cs.City()
    paint = Painter(c)
    os.makedirs(out, exist_ok=True)
    for day in range(years * c.clock.year_days):
        im = paint.frame(day * 1440 + cs.hm("13:00"), None, 0.0, "One frame a day, at 13:00", "Five game years. Every state is calculated from the date.")
        im.save(os.path.join(out, "f_%04d.png" % day), compress_level=1)
    print("years:", years * c.clock.year_days, "frames")


if __name__ == "__main__":
    mode, out = sys.argv[1], sys.argv[2]
    if mode == "day":
        day_frames(out)
    elif mode == "years":
        years_frames(out)
    else:
        c = cs.City()
        Painter(c).frame(16 * 1440 + cs.hm("12:34"), (80, 62), 40.0, "", "Wednesday lunchtime, market day").save(out)
