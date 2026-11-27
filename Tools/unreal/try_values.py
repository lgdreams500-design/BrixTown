"""One trial of the matching round (docs/process/LOOK_GATE.md): sets numbers in Game/Config/DefaultGame.ini, takes a
tour's shots in one Unreal launch, and measures every shot against its own place's reference.

  python Tools/unreal/try_values.py <out folder> <tour list> [<Section>:<Key>=<Value> ...]

  python Tools/unreal/try_values.py C:/shots/d Tools/unreal/reference_tour.txt BrixRooms:RoomFill=0.7 BrixLight:DayOpenIndoors=1.5

<Section> is the class's name ([/Script/BrixTown.<Section>]). The file is left as the trial set it: git shows it, and
git checkout puts it back. A key the section does not have yet is added to it. Needs this checkout to have a build.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
INI = os.path.normpath(os.path.join(HERE, "..", "..", "Game", "Config", "DefaultGame.ini"))


def set_values(pairs, path=INI):
    with open(path, encoding="utf-8", newline="") as f:
        text = f.read()
    nl = "\r\n" if "\r\n" in text else "\n"
    for item in pairs:
        where, value = item.split("=", 1)
        section, key = where.split(":", 1)
        m = re.search(r"\[/Script/BrixTown\.%s\][^\[]*" % re.escape(section), text)
        if not m:
            sys.exit("no section [/Script/BrixTown.%s] in %s" % (section, path))
        body, n = re.subn(r"(?m)^%s=[^\r\n]*" % re.escape(key), lambda _: "%s=%s" % (key, value), m.group(0))
        if not n:
            body = body.rstrip("\r\n") + nl + "%s=%s" % (key, value) + nl + nl
        text = text[:m.start()] + body + text[m.end():]
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def main(argv):
    if len(argv) < 3:
        sys.exit(__doc__)
    out, tour = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    set_values(argv[3:])
    os.makedirs(out, exist_ok=True)
    r = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", os.path.join(HERE, "light_tour.ps1"),
                        "-OutDir", out, "-List", tour], capture_output=True, text=True)
    print((r.stdout.strip().splitlines() or ["no output from the tour"])[0], "|", " ".join(argv[3:]))
    return subprocess.call([sys.executable, os.path.join(HERE, "look_match.py"), out, "--json", os.path.join(out, "match.json")])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
