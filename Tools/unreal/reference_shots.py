"""Takes each place's light settings as pictures in the game and puts them on the reference page (ADR-0075;
docs/process/UNREAL_REFERENCES.md). No agent is needed to run it: it reads the "unreal" block of each reference in
Art/Reference/catalog.json, takes the shots in one run of the game, and writes the pictures, their numbers, the block
and the page.

    python Tools/unreal/reference_shots.py --area IndoorHomeRoom      one reference (--area may be given more than once)
    python Tools/unreal/reference_shots.py --all                      every reference that has an "unreal" block
    python Tools/unreal/reference_shots.py --all --page-only          the page again, no shots
    python Tools/unreal/reference_shots.py --all --from <folder>      no run of the game: use the shots already in <folder>
    ... --out <folder>     where the run's own shots and log go (default Game/Saved/ReferenceShots, not in the repository)

A reference's block (the catalog's note says the same):
    "unreal": {
      "where": "refroom:home",                 a tour stop's place: refroom:<room> | cam:x,z,up,yaw,pitch | stand:x,z,up,yaw | in:<lot>[,<pitch>]
      "blender": "Indoor/IndoorHomeRoom_Midday.jpg",    the one Blender picture the page keeps showing
      "settings": {"Morning": "ref", "Midday": "12:00", "Rain": "12:00|rain"},    hh:mm or ref, |<weather> after it
      "missing": {"NightFullMoon": "why the game cannot show it"},
      "issues": ["what is seen wrong in the game's pictures, written by whoever looked at them"],
      "files", "checks", "shot", "status"      written here
    }
What it does for the references asked for:
  1. checks every block (reference_catalog.unreal_problems); on a problem it stops and says, and takes nothing;
  2. writes one tour list for all of them and runs Tools/unreal/light_tour.ps1 once (one launch, about 7 s a shot);
  3. saves each shot as Art/Reference/<Category>/Unreal/<Id>_<Setting>.jpg (never beside or over a Blender file);
  4. measures each with look_check.py, and reads the grey card with card_read.py where the game's log places one, and
     writes the numbers to Art/Reference/<Category>/Unreal/<Id>.json;
  5. writes "files", "checks" (what the look check fails, a setting each), "shot" and "status": "proposed" into the
     block. A block the owner has approved stays approved only while every picture is the one approved (SAME); a
     picture that has changed puts it back to proposed, and its "decision" is kept as "was";
  6. writes Art/Reference/index.html again (Tools/reference_catalog.py).
It needs a built checkout (docs/setup/DEV_ENVIRONMENT.md), Pillow and numpy. It never approves anything.
"""
import datetime
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "Tools"))
import reference_catalog as RC  # noqa: E402

QUALITY = "High, 1066 x 666"                               # what light_tour.ps1 runs the game at
JPEG = 90
SAME = 1.5                                                 # a picture within this many levels (mean, 0 to 255) of the approved one is the same picture
LOG_STOP = re.compile(r"Brix Town: tour stop (\S+) at (\d\d:\d\d), weather (\S+), the camera ([a-z ]+), daylight ([\d.]+), ([\d.]+) frames a second")
LOG_WORST = re.compile(r"Brix Town: tour stop (\S+): the longest of its last \d+ frames (\d+) ms")


def stop_name(ref_id, setting):
    return "%s_%s" % (ref_id, setting)


def tour_lines(refs):
    """One line a setting, in the tour list's form: name|hh:mm|weather|where."""
    out = []
    for r in refs:
        u = r["unreal"]
        for setting, value in u["settings"].items():
            hour, _, weather = value.partition("|")
            out.append("%s|%s|%s|%s" % (stop_name(r["id"], setting), hour, weather, u["where"]))
    return out


def judged_as(ref_id, value):
    """The name look_check.py judges a shot by: it reads night from the hour at a name's end."""
    hour = value.partition("|")[0]
    return "%s_%s" % (ref_id, "1200" if hour == "ref" else hour.replace(":", ""))


def run_tour(lines, out):
    os.makedirs(out, exist_ok=True)
    for n in os.listdir(out):
        if n.lower().endswith(".png") or n == "tour.log":
            os.remove(os.path.join(out, n))
    listing = os.path.join(out, "reference_shots_tour.txt")
    with open(listing, "w", encoding="utf-8", newline="\n") as f:
        f.write("# written by Tools/unreal/reference_shots.py\n" + "\n".join(lines) + "\n")
    done = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", os.path.join(HERE, "light_tour.ps1"),
                           "-OutDir", out, "-List", listing])
    return done.returncode


def log_of(folder):
    """What the game's log says of each stop: its hour, weather, frames a second, the longest frame."""
    out = {}
    try:
        with open(os.path.join(folder, "tour.log"), encoding="utf-8", errors="replace") as f:
            for line in f:
                m = LOG_STOP.search(line)
                if m:
                    out.setdefault(m.group(1), {}).update({"at": m.group(2), "weather": m.group(3), "camera": m.group(4).strip(),
                                                           "daylight": float(m.group(5)), "fps": float(m.group(6))})
                m = LOG_WORST.search(line)
                if m:
                    out.setdefault(m.group(1), {})["worst_ms"] = int(m.group(2))
    except OSError:
        pass
    return out


def same_picture(a, b):
    from PIL import Image, ImageChops, ImageStat
    x, y = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    return x.size == y.size and max(ImageStat.Stat(ImageChops.difference(x, y)).mean) <= SAME


def commit():
    try:
        return subprocess.run(["git", "-C", ROOT, "rev-parse", "--short", "HEAD"], capture_output=True, text=True).stdout.strip() or "?"
    except OSError:
        return "?"


def keep(r, folder, log, cards):
    """One reference's shots from the run's folder into Art/Reference, its numbers and its block. [] or what is wrong."""
    from PIL import Image
    import look_check as LC
    u, cat_dir = r["unreal"], os.path.join(RC.REF, r["category"], "Unreal")
    shots = {s: os.path.join(folder, stop_name(r["id"], s) + ".png") for s in u["settings"]}
    gone = [s for s, p in shots.items() if not os.path.isfile(p)]
    if gone:
        return ["%s: the run took no picture of %s (see %s)" % (r["id"], ", ".join(gone), os.path.join(folder, "tour.log"))]
    os.makedirs(cat_dir, exist_ok=True)
    was_approved = u.get("status") == "approved"
    figure = u["where"].startswith(("stand:", "in:"))
    files, checks, numbers, changed = [], {}, {}, []
    for setting, value in u["settings"].items():
        name = stop_name(r["id"], setting)
        rel = "%s/Unreal/%s.jpg" % (r["category"], name)
        dest = os.path.join(RC.REF, rel)
        new = dest + ".new.jpg"
        Image.open(shots[setting]).convert("RGB").save(new, quality=JPEG)
        if was_approved and os.path.isfile(dest) and same_picture(dest, new):
            os.remove(new)                                 # the approved picture stays, byte for byte
        else:
            if was_approved:
                changed.append(setting)
            os.replace(new, dest)
        look = LC.measure(dest, with_figure=figure)
        fails = LC.judge(judged_as(r["id"], value), look)
        files.append(rel)
        if fails:
            checks[setting] = fails
        numbers[setting] = dict({"hour": value, "look": look, "fails": fails}, **log.get(name, {}))
        if name in cards:
            numbers[setting]["card"] = cards[name]
    fps = [n["fps"] for n in numbers.values() if n.get("fps")]
    shot = {"commit": commit(), "date": datetime.date.today().isoformat(), "quality": QUALITY}
    if fps:
        shot["fps"] = "%.0f to %.0f" % (min(fps), max(fps))
    u["files"], u["shot"] = files, shot
    u.pop("checks", None)
    if checks:
        u["checks"] = checks
    if was_approved and changed:
        u["was"] = "approved (%s); shot again on %s and %s changed" % (u.pop("decision", "?"), shot["date"], ", ".join(changed))
    if not was_approved or changed:
        u["status"] = "proposed"
    with open(os.path.join(cat_dir, r["id"] + ".json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump({"reference": r["id"], "where": u["where"], "shot": shot, "settings": numbers}, f, indent=1)
        f.write("\n")
    return []


def save(cat):
    with open(RC.CATALOG, "w", encoding="utf-8", newline="\n") as f:
        json.dump(cat, f, indent=1, ensure_ascii=False)
        f.write("\n")


def main(argv):
    args, areas, out, taken = argv[1:], [], os.path.join(ROOT, "Game", "Saved", "ReferenceShots"), None
    every = page_only = False
    while args:
        a = args.pop(0)
        if a == "--area":
            areas.append(args.pop(0))
        elif a == "--all":
            every = True
        elif a == "--page-only":
            page_only = True
        elif a == "--out":
            out = os.path.abspath(args.pop(0))
        elif a == "--from":
            taken = os.path.abspath(args.pop(0))
        else:
            sys.exit(__doc__)
    if not (areas or every):
        sys.exit(__doc__)
    cat = RC.load()
    if not page_only:
        by_id = {r["id"]: r for r in cat["references"]}
        for a in areas:
            if a not in by_id or not by_id[a].get("unreal"):
                sys.exit("%s: not a reference with an \"unreal\" block in Art/Reference/catalog.json" % a)
        refs = [r for r in cat["references"] if r.get("unreal") and (every or r["id"] in areas)]
        bad = [p for r in refs for p in RC.unreal_problems(r, files=False)]
        for p in bad:
            print("problem:", p)
        if bad or not refs:
            return 1
        lines = tour_lines(refs)
        print("%d references, %d shots" % (len(refs), len(lines)))
        folder = taken or out
        if not taken:
            code = run_tour(lines, out)
            if code:
                print("light_tour.ps1 ended with %d" % code)
        log = log_of(folder)
        try:
            import card_read
            cards = card_read.folder_readings(folder)
        except Exception as ex:                             # the card's reading is extra: a run without it still keeps its pictures
            print("no card read: %s" % ex)
            cards = {}
        wrong = []
        for r in refs:
            w = keep(r, folder, log, cards)
            wrong += w
            if not w:
                u = r["unreal"]
                print("%-26s %d pictures, %s%s" % (r["id"], len(u["files"]), u["status"],
                                                 "".join("\n    %s FAILS the look check: %s" % (s, "; ".join(f)) for s, f in u.get("checks", {}).items())))
        save(cat)
        for w in wrong:
            print("problem:", w)
        if wrong:
            RC.main(["reference_catalog.py"])
            return 1
    return RC.main(["reference_catalog.py"])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
