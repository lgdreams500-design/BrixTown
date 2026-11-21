"""Writes data/block_01.json: one old-town perimeter block with a courtyard garden.
All tuning lives in that data file; citysim.py only holds rules. Units: metres, minutes, game days."""
import json, os

def grid(x0, y0, x1, y1, n, cols=3):
    out = []
    rows = (n + cols - 1) // cols
    for i in range(n):
        c, r = i % cols, i // cols
        out.append([round(x0 + (x1 - x0) * (c + 0.5) / cols, 1), round(y0 + (y1 - y0) * (r + 0.5) / max(rows, 1), 1)])
    return out

lots = []
def lot(i, name, kind, rect, door, objects):
    lots.append(dict(id=i, name=name, kind=kind, rect=rect, door=door, objects=objects))

HOMES = dict(homeA=("Home A", [30, 30, 52, 52], [41, 30]), homeB=("Home B", [108, 30, 130, 52], [119, 30]),
             homeC=("Home C", [30, 108, 52, 130], [41, 130]), homeD=("Home D", [52, 108, 74, 130], [63, 130]),
             homeE=("Home E", [108, 108, 130, 130], [119, 130]), homeF=("Home F", [30, 52, 52, 74], [30, 63]))
residents = []
def person(name, role, home, work=None, **extra):
    residents.append(dict(id="r%02d" % len(residents), name=name, role=role, home=home, work=work, **extra))

person("Asha", "shopkeeper", "homeA", "grocer"); person("Tomas", "shopkeeper", "homeA", "grocer", offset=35, market_seller=True)
person("Mina", "cafe", "homeB", "cafe"); person("Jon", "cafe", "homeB", "cafe", offset=20)
person("Ruth", "nurse", "homeB", "clinic")
person("Kofi", "maker", "homeC", "workshop"); person("Ines", "maker", "homeD", "workshop", offset=15, market_seller=True); person("Bo", "maker", "homeD", "workshop", offset=-10)
for i, (n, h) in enumerate((("Dev", "homeD"), ("Lena", "homeE"), ("Omar", "homeE"), ("Yui", "homeF"), ("Sam", "homeF"))):
    person(n, "clerk", h, "office", offset=(i * 7) % 20 - 8)
for i, (n, h) in enumerate((("Pia", "homeA"), ("Raj", "homeE"), ("Nell", "homeF"), ("Abe", "homeA"))):
    person(n, "commuter", h, None, offset=i * 9 - 12)
person("Zed", "courier", "homeC", None); person("Fay", "courier", "homeC", None, offset=20)
person("Olu", "gardener", "homeF", "garden")
for i, (n, h) in enumerate((("Edda", "homeA"), ("Hugo", "homeB"), ("Mei", "homeE"), ("Walt", "homeF"))):
    person(n, "elder", h, None, offset=i * 25 - 30)
person("Kit", "student", "homeD", None); person("Noor", "student", "homeD", None, offset=10)
person("Gus", "utility", "homeC", "depot")

for hid, (name, rect, door) in HOMES.items():
    n = sum(1 for r in residents if r["home"] == hid)
    x0, y0, x1, y1 = rect
    lot(hid, name, "home", rect, door, dict(bed=grid(x0 + 2, y0 + 2, x1 - 2, y0 + 11, n), room=grid(x0 + 2, y0 + 11, x1 - 2, y1 - 2, n)))
lot("grocer", "Grocer", "shop", [52, 30, 74, 52], [63, 30], dict(work=[[63, 45], [70, 48]], customer=[[58, 39], [63, 38], [68, 39]]))
lot("cafe", "Café", "food", [86, 30, 108, 52], [97, 52], dict(work=[[97, 35], [91, 34]], seat=[[90, 45], [95, 45], [100, 45], [105, 45], [89, 56], [93, 56], [97, 57], [101, 56], [105, 56], [89, 59]]))
lot("workshop", "Workshop", "work", [108, 52, 130, 74], [130, 63], dict(work=[[114, 58], [122, 58], [114, 68], [122, 68]]))
lot("clinic", "Clinic", "health", [108, 86, 130, 108], [130, 97], dict(work=[[124, 97]], chair=[[113, 91], [113, 95], [113, 99], [113, 103]]))
lot("office", "Office", "work", [86, 108, 108, 130], [97, 130], dict(work=grid(88, 110, 106, 128, 6)))
lot("depot", "Depot", "utility", [30, 86, 52, 108], [30, 97], dict(work=[[38, 93], [44, 101]]))
lot("garden", "Garden", "garden", [64, 64, 96, 96], [80, 62], dict(
    bench=[[74, 65.5], [77, 65.5], [83, 65.5], [86, 65.5], [74, 94.5], [77, 94.5], [65.5, 78], [65.5, 82], [94.5, 78], [94.5, 82]],
    rim=[[77, 83.2], [83, 83.2], [80, 76.3]], flowerbed=[[70, 71.5], [90, 71.5], [70, 88.5], [90, 88.5]], work=[[70, 71.5], [90, 71.5], [70, 88.5], [90, 88.5]]))
lot("market", "Market", "market", [54, 54, 73, 61], [66, 62], dict(stall=[[57, 55.5], [63, 55.5], [69, 55.5]], customer=[[57, 59.5], [63, 59.5], [69, 59.5], [60, 60.2], [66, 60.2]]))
lot("stop", "Tram stop", "stop", [72, 11, 88, 16], [80, 20], dict(wait=[[74, 14.5], [77, 14.5], [80, 14.5], [83, 14.5], [86, 14.5]]))

T = lambda *rows: [list(r) for r in rows]
roles = dict(
    shopkeeper=dict(color=[0.72, 0.55, 0.20], sig="Easy", jitter=8,
        weekday=T(("00:00", "sleep", "home"), ("06:20", "home", "home"), ("06:55", "work", "work"), ("12:40", "eat", "lunch"), ("13:25", "work", "work"),
                  ("18:30", "leisure", "leisure"), ("20:00", "home", "home"), ("22:45", "sleep", "home")),
        market=T(("00:00", "sleep", "home"), ("06:20", "home", "home"), ("07:40", "sell", "market"), ("13:00", "eat", "lunch"), ("13:45", "work", "work"),
                 ("18:30", "leisure", "leisure"), ("20:00", "home", "home"), ("22:45", "sleep", "home")),
        sunday=T(("00:00", "sleep", "home"), ("08:00", "home", "home"), ("10:00", "leisure", "leisure"), ("12:30", "eat", "lunch"), ("14:00", "leisure", "leisure"),
                 ("17:00", "home", "home"), ("22:45", "sleep", "home"))),
    cafe=dict(color=[0.66, 0.33, 0.24], sig="Springy", jitter=6,
        weekday=T(("00:00", "sleep", "home"), ("05:40", "home", "home"), ("06:10", "work", "work"), ("15:00", "rest", "garden"), ("15:35", "work", "work"),
                  ("19:40", "home", "home"), ("22:30", "sleep", "home"))),
    nurse=dict(color=[0.86, 0.84, 0.78], sig="Easy", jitter=8,
        weekday=T(("00:00", "sleep", "home"), ("06:45", "home", "home"), ("07:50", "work", "work"), ("12:30", "eat", "lunch"), ("13:10", "work", "work"),
                  ("17:10", "leisure", "leisure"), ("18:45", "home", "home"), ("22:30", "sleep", "home")),
        sunday=T(("00:00", "sleep", "home"), ("08:30", "home", "home"), ("10:30", "leisure", "leisure"), ("12:45", "eat", "lunch"), ("14:30", "leisure", "leisure"),
                 ("17:30", "home", "home"), ("22:30", "sleep", "home"))),
    maker=dict(color=[0.45, 0.30, 0.20], sig="Easy", jitter=10,
        weekday=T(("00:00", "sleep", "home"), ("06:50", "home", "home"), ("07:55", "work", "work"), ("12:20", "eat", "lunch"), ("13:05", "work", "work"),
                  ("17:30", "leisure", "leisure"), ("19:20", "home", "home"), ("23:00", "sleep", "home")),
        market=T(("00:00", "sleep", "home"), ("06:50", "home", "home"), ("08:30", "sell", "market"), ("13:00", "eat", "lunch"), ("13:45", "work", "work"),
                 ("17:30", "leisure", "leisure"), ("19:20", "home", "home"), ("23:00", "sleep", "home")),
        sunday=T(("00:00", "sleep", "home"), ("08:40", "home", "home"), ("10:15", "leisure", "leisure"), ("12:40", "eat", "lunch"), ("14:10", "leisure", "leisure"),
                 ("18:00", "home", "home"), ("23:00", "sleep", "home"))),
    clerk=dict(color=[0.20, 0.26, 0.45], sig="Easy", jitter=12,
        weekday=T(("00:00", "sleep", "home"), ("06:50", "home", "home"), ("07:50", "work", "work"), ("12:30", "eat", "lunch"), ("13:15", "work", "work"),
                  ("17:30", "leisure", "leisure"), ("19:15", "home", "home"), ("22:45", "sleep", "home")),
        weekend=T(("00:00", "sleep", "home"), ("08:30", "home", "home"), ("10:00", "leisure", "leisure"), ("12:30", "eat", "lunch"), ("13:40", "leisure", "leisure"),
                  ("16:30", "home", "home"), ("23:15", "sleep", "home"))),
    commuter=dict(color=[0.40, 0.50, 0.66], sig="Hurried", jitter=6,
        weekday=T(("00:00", "sleep", "home"), ("06:30", "home", "home"), ("07:20", "away", "stop"), ("17:45", "leisure", "leisure"), ("19:00", "home", "home"),
                  ("22:30", "sleep", "home")),
        weekend=T(("00:00", "sleep", "home"), ("08:45", "home", "home"), ("10:30", "leisure", "leisure"), ("12:40", "eat", "lunch"), ("14:00", "leisure", "leisure"),
                  ("17:00", "home", "home"), ("23:00", "sleep", "home"))),
    courier=dict(color=[0.18, 0.50, 0.48], sig="Hurried", jitter=5, round_every=13, round_act="deliver",
        weekday=T(("00:00", "sleep", "home"), ("06:20", "home", "home"), ("07:10", "rounds", "rounds"), ("12:05", "eat", "lunch"), ("12:45", "rounds", "rounds"),
                  ("17:00", "leisure", "leisure"), ("19:00", "home", "home"), ("22:30", "sleep", "home")),
        sunday=T(("00:00", "sleep", "home"), ("09:00", "home", "home"), ("11:00", "leisure", "leisure"), ("12:45", "eat", "lunch"), ("14:15", "leisure", "leisure"),
                 ("18:00", "home", "home"), ("22:30", "sleep", "home"))),
    gardener=dict(color=[0.36, 0.44, 0.22], sig="Easy", jitter=8, round_every=40, round_act="tend",
        weekday=T(("00:00", "sleep", "home"), ("06:10", "home", "home"), ("06:50", "tend", "garden"), ("11:40", "eat", "lunch"), ("12:25", "tend", "garden"),
                  ("16:00", "leisure", "leisure"), ("18:30", "home", "home"), ("22:00", "sleep", "home")),
        sunday=T(("00:00", "sleep", "home"), ("08:00", "home", "home"), ("10:00", "leisure", "leisure"), ("12:30", "eat", "lunch"), ("14:00", "home", "home"),
                 ("22:00", "sleep", "home"))),
    elder=dict(color=[0.47, 0.36, 0.52], sig="Careful", jitter=18,
        weekday=T(("00:00", "sleep", "home"), ("07:15", "home", "home"), ("09:10", "rest", "garden"), ("10:35", "shop", "shopping"), ("11:25", "home", "home"),
                  ("14:10", "leisure", "leisure"), ("16:00", "visit", "afternoon"), ("17:10", "home", "home"), ("21:30", "sleep", "home"))),
    student=dict(color=[0.80, 0.50, 0.30], sig="Springy", jitter=6,
        weekday=T(("00:00", "sleep", "home"), ("06:50", "home", "home"), ("07:40", "away", "stop"), ("15:40", "leisure", "leisure"), ("18:20", "home", "home"),
                  ("22:15", "sleep", "home")),
        weekend=T(("00:00", "sleep", "home"), ("09:15", "home", "home"), ("10:45", "leisure", "leisure"), ("13:00", "eat", "lunch"), ("14:00", "leisure", "leisure"),
                  ("18:00", "home", "home"), ("23:00", "sleep", "home"))),
    utility=dict(color=[0.16, 0.16, 0.20], sig="Easy", jitter=8, round_every=28, round_act="inspect",
        weekday=T(("00:00", "sleep", "home"), ("06:30", "home", "home"), ("07:25", "work", "work"), ("09:00", "rounds", "rounds"), ("12:00", "eat", "lunch"),
                  ("12:45", "work", "work"), ("16:30", "leisure", "leisure"), ("18:30", "home", "home"), ("22:30", "sleep", "home")),
        weekend=T(("00:00", "sleep", "home"), ("08:30", "home", "home"), ("10:30", "leisure", "leisure"), ("12:45", "eat", "lunch"), ("14:00", "leisure", "leisure"),
                  ("17:30", "home", "home"), ("22:30", "sleep", "home"))),
)
data = dict(
    name="Old Town block 01", size=[160, 160],
    calendar=dict(day_minutes=1440, days_per_season=12, seasons=["Spring", "Summer", "Autumn", "Winter"], week=["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"],
                  market_days=[2, 5], rest_days=[5, 6], sunrise=[6.0, 5.0, 6.5, 7.75], sunset=[19.0, 20.5, 18.5, 16.75]),
    streets=[[[20, 20], [140, 20]], [[140, 20], [140, 140]], [[140, 140], [20, 140]], [[20, 140], [20, 20]],
             [[80, 20], [80, 62]], [[140, 80], [98, 80]], [[80, 140], [80, 98]], [[20, 80], [62, 80]],
             [[62, 62], [98, 62]], [[98, 62], [98, 98]], [[98, 98], [62, 98]], [[62, 98], [62, 62]]],
    stroll_loop=[[62, 62], [98, 62], [98, 98], [62, 98]],
    lots=lots, residents=residents, roles=roles,
    signatures=dict(Springy=1.23, Easy=1.14, Hurried=1.68, Careful=0.58),          # walking speeds, m/s, from CHARACTER_MOTION
    choices=dict(lunch=dict(default=[["eat", "cafe"], ["eat", "cafe"], ["shop", "grocer"], ["eat", "home"]], market=[["shop", "market"], ["eat", "cafe"]]),
                 leisure=dict(default=[["rest", "garden"], ["stroll", "garden"], ["eat", "cafe"], ["stroll", "garden"], ["home", "home"]],
                              market=[["shop", "market"], ["rest", "garden"]], rain=[["eat", "cafe"], ["home", "home"], ["home", "home"]]),
                 shopping=dict(default=[["shop", "grocer"]], market=[["shop", "market"]], rain=[["shop", "grocer"], ["home", "home"]]),
                 afternoon=dict(default=[["home", "home"], ["rest", "garden"], ["home", "home"], ["visit", "clinic"]], rain=[["home", "home"]]),
                 rounds=["grocer", "cafe", "clinic", "workshop", "office", "depot", "homeA", "homeB", "homeC", "homeD", "homeE", "homeF"]),
    use=dict(sleep="bed", home="room", work="work", eat="seat", shop="customer", rest="bench", sell="stall", away="wait", visit="chair", tend="flowerbed",
             eat_home="room", rest_fallback="rim"),
    weather=dict(rain_chance=[0.30, 0.16, 0.36, 0.26], cloud_chance=[0.35, 0.25, 0.40, 0.45], fog_chance=[0.05, 0.0, 0.20, 0.25]),
    tram=dict(y=20, stop_x=80, every=20, first="05:40", last="23:00", dwell=1.0, speed=8.0),
    van=dict(runs=["07:25", "14:05"], speed=5.0, loop=[[20, 20], [140, 20], [140, 140], [20, 140], [20, 20]], stops=[[63, 20, 4.0], [140, 63, 3.0], [97, 140, 3.0]]),
    species=dict(
        plane=dict(r_max=6.5, rate=0.16, shape=1.3, life=[38, 55], bud=4, full=9, turn=27, bare=34, autumn=[0.62, 0.44, 0.14], summer=[0.24, 0.36, 0.20]),
        linden=dict(r_max=5.0, rate=0.30, shape=1.4, life=[30, 45], bud=3, full=8, turn=25, bare=32, autumn=[0.70, 0.56, 0.16], summer=[0.30, 0.42, 0.22]),
        plum=dict(r_max=3.2, rate=0.45, shape=1.5, life=[14, 22], bud=2, full=7, turn=28, bare=35, blossom=[1, 5], autumn=[0.52, 0.22, 0.16], summer=[0.26, 0.38, 0.24])),
    trees=[dict(id="t%02d" % i, species=s, pos=p, planted=pl, seed=i) for i, (s, p, pl) in enumerate([
        ("plane", [12, 44], -20 * 48), ("plane", [12, 116], -31 * 48), ("plane", [148, 44], -36.4 * 48), ("plane", [148, 116], -12 * 48),
        ("plane", [44, 148], -25 * 48), ("plane", [116, 148], -44 * 48), ("plane", [44, 9], -16 * 48), ("plane", [120, 9], -28 * 48),
        ("linden", [71, 73], 0), ("linden", [89, 73], 0), ("linden", [71, 87], 0), ("linden", [89, 87], 0),
        ("plum", [58, 68], 0), ("plum", [102, 68], 0), ("plum", [58, 92], -12.5 * 48), ("plum", [102, 92], 0), ("plum", [80, 101], -2 * 48)])],
    flowers=dict(sprout=3, bloom=[9, 27], gone=31, beds=[[70, 70], [90, 70], [70, 90], [90, 90]], colors=[[0.74, 0.30, 0.28], [0.80, 0.62, 0.20], [0.52, 0.36, 0.60], [0.82, 0.74, 0.60]]),
    replant_gap_days=10, fallen_days=4,
    lamps=[[80, 30], [80, 52], [30, 80], [52, 80], [130, 80], [108, 80], [80, 130], [80, 108], [26, 26], [134, 26], [134, 134], [26, 134], [80, 80], [63, 26], [97, 134]])
os.makedirs(os.path.join(os.path.dirname(os.path.abspath(__file__)), "data"), exist_ok=True)
json.dump(data, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", "block_01.json"), "w"), indent=1, ensure_ascii=False)
print(len(lots), "lots,", len(residents), "residents")
