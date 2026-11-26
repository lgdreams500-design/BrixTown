"""Voice every dialogue line for every voice that can say it (ADR-0019, ADR-0060, SND-018).

    python make_dialogue_voices.py MODEL_DIR      voice what is new, remove what no line needs, write the index
    python make_dialogue_voices.py --plan         only count what would be made
    python make_dialogue_voices.py --index        only write the index again: who has which voice, the pieces already made

Reads World/rules/voices.json (who speaks in which voice), dialogue.json and quests.json (the lines), the residents and
the lots, and writes Prototype/play/audio/voices/<voice>/<id>.ogg with an index, audio/voices/index.js, that the slice
reads. A resident in the 'people' list speaks in the voice given there; anyone else in one from their role's pool; a
resident whose role has no pool is not voiced. A person whose entry is { "cast": <id> } is a named character voiced by
Tools/voice/cast/make_cast_voices.py (another model, another recipe): this script makes nothing for them and keeps
their clips. A line is cut at its blanks ({greeting}, {name}, ...): the fixed pieces
are voiced once per voice and mood, and each blank is voiced for every value it can take for that speaker. Pieces
already made are kept, so a run after a small change only voices what is new. --plan and --index need no voice model;
voicing needs kokoro-onnx and soundfile, and MODEL_DIR holds kokoro-v1.0.int8.onnx and voices-v1.0.bin (not in the
repository; docs/licenses/kokoro-tts.md).
"""
import glob, hashlib, json, os, re, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
RULES = os.path.join(ROOT, "World", "rules")
OUT = os.path.join(ROOT, "Prototype", "play", "audio", "voices")
DAYS = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"]
GREETINGS = ["Good morning", "Good afternoon", "Good evening"]


def read(*p):
    with open(os.path.join(*p), encoding="utf-8") as f:
        return json.load(f)


def norm(t):
    """A piece as it is looked up: no leading punctuation or spaces, single spaces. The slice does the same (voices.js)."""
    return re.sub(r"\s+", " ", re.sub(r"^[\s,;:.!?—-]+", "", t)).strip()


def speakable(t):
    return bool(re.search(r"[A-Za-z0-9]", t))


def pieces(template):
    """[('text', piece) | ('blank', name)] in order."""
    out = []
    for tok in re.split(r"(\{\w+\})", template):
        m = re.fullmatch(r"\{(\w+)\}", tok)
        if m:
            out.append(("blank", m.group(1)))
        elif speakable(tok):
            out.append(("text", norm(tok)))
    return out


def cfg_id(o):
    """The name of a voice as it is set up: its folder of pieces and its key in the index. Two voices mixed evenly
    (ADR-0060) are named in a fixed order, so either way round is the same voice. A cast voice (a named character
    whose lines are made by Tools/voice/cast/, not by this script) is "cast_<their id in the cast>"."""
    if o.get("cast"):
        return "cast_" + o["cast"]
    s = o["voice"] if isinstance(o["voice"], str) else "-".join(sorted(o["voice"]))
    if o.get("shift", 1) != 1:
        s += "_p%d" % round(o["shift"] * 100)
    if o.get("speed", 1) != 1:
        s += "_v%d" % round(o["speed"] * 100)
    return s


def doing_phrases():
    """What a passer-by says they are doing: the DOING and GOING words of the slice (play.js, dressing.js)."""
    out = {"on my way somewhere"}
    for f in ("play.js", "dressing.js"):
        src = open(os.path.join(ROOT, "Prototype", "play", f), encoding="utf-8").read()
        for m in re.finditer(r"(?:const (?:DOING|GOING) = |Object\.assign\((?:DOING|GOING), )\{([^}]*)\}", src):
            out |= set(re.findall(r'\w+: "([^"]+)"', m.group(1)))
    return sorted(out)


def world():
    city = read(ROOT, "World", "city_01", "city.json")
    lots = {}
    for b in city["blocks"]:
        for l in read(ROOT, "World", "city_01", b["file"])["lots"]:
            lots[l["id"]] = l.get("name")
    res = read(ROOT, "World", "city_01", "residents.json")["residents"]
    cal = read(RULES, "calendar.json")["calendar"]
    return city, lots, res, cal


def speakers(V, res):
    """The voice each resident speaks in: their own if they are in the people list (ADR-0060), else one from their
    role's pool, picked by a fixed hash of their id. None if their role has no pool: they are not voiced."""
    people, out = V.get("people", {}), []
    for r in res:
        pool = V["roles"].get(r["role"])
        if r["id"] in people:
            out.append(people[r["id"]])
        elif pool:
            out.append(pool[int(hashlib.md5(r["id"].encode()).hexdigest(), 16) % len(pool)])
        else:
            out.append(None)
    return out


def people_problems(V, res):
    """What is wrong with the people list: an id that is no resident, or a name that is not that resident's. The list
    is by id; the name is there so that a town dealt again cannot hand a voice to someone else unseen."""
    by_id = {r["id"]: r for r in res}
    out = []
    for pid, o in V.get("people", {}).items():
        if pid not in by_id:
            out.append("%s is not a resident" % pid)
        elif o.get("name") != by_id[pid]["name"]:
            out.append("%s is %s, not %s" % (pid, by_id[pid]["name"], o.get("name")))
    return out


def plan(V=None):
    """V: the voices to plan for, if not World/rules/voices.json as it is (the tests try a people list of their own)."""
    V, D, Q = V or read(RULES, "voices.json"), read(RULES, "dialogue.json"), read(RULES, "quests.json")
    city, lots, res, cal = world()
    wrong = people_problems(V, res)
    if wrong:
        raise SystemExit("World/rules/voices.json, people: " + "; ".join(wrong))
    who = speakers(V, res)
    rid = {r["id"]: i for i, r in enumerate(res)}
    seasons = sorted({s.lower() for s in (cal["seasons"].values() if isinstance(cal["seasons"], dict) else cal["seasons"])})
    doing = doing_phrases()
    jobs = {}                                           # (cfg, mood, text) -> voice options

    def need(o, mood, text):
        if speakable(text):
            jobs[(cfg_id(o), mood, norm(text))] = o

    def say(template, mood, speakers_i, extra=None):
        """Every piece of a line for these residents (indices), with the values each blank can take for them."""
        ps = pieces(template)
        for i in speakers_i:
            o, r = who[i], res[i]
            if o is None or o.get("cast"):              # their role has no pool: not voiced; a cast voice: made by Tools/voice/cast/
                continue
            vals = {"greeting": GREETINGS, "season": seasons, "weekday": DAYS, "doing": doing, "name": [r["name"]],
                    "role": [D["role_words"].get(r["role"], r["role"])], "place": [lots.get(r.get("work")) or city["name"], city["name"]],
                    "player": [V["player"]["default_name"]]}
            vals.update(extra or {})
            for kind, p in ps:
                if kind == "text":
                    need(o, mood, p)
                else:
                    for v in vals.get(p, []):
                        if v:
                            need(o, mood, v)

    by_role = {}
    for i, r in enumerate(res):
        by_role.setdefault(r["role"], []).append(i)
    # one resident per distinct voice is enough for fixed pieces; blanks that depend on the person use everyone
    def per_voice(idx):
        seen, out = set(), []
        for i in idx:
            c = who[i] and not who[i].get("cast") and cfg_id(who[i])
            if c and c not in seen:
                seen.add(c); out.append(i)
        return out
    for cid, conv in D["conversations"].items():
        roles = set()
        for rule in D["rules"]:
            if rule["conversation"] == cid:
                roles |= set(rule.get("roles") or by_role.keys())
        idx = [i for role in roles for i in by_role.get(role, [])]
        for n in conv["nodes"].values():
            personal = re.search(r"\{(name|role|place)\}", n["say"])
            say(n["say"], n.get("mood", "neutral"), idx if personal else per_voice(idx))
    # quests: a stage's lines are said by the resident the player is sent to talk to; anything else by its giver
    everyone = per_voice(range(len(res)))
    for q in Q["quests"].values():
        for st in q["stages"].values():
            speaker = (st.get("goal") or {}).get("talk") or q.get("giver")
            for n in st.get("nodes", {}).values():
                say(n["say"], n.get("mood", "neutral"), [rid[speaker]] if speaker in rid else everyone)
    def other_says(x, inside_stage=False):              # lines in quests.json not under a stage (jobs and the like)
        if isinstance(x, dict):
            if "say" in x and isinstance(x["say"], str) and not inside_stage:
                say(x["say"], x.get("mood", "neutral"), everyone)
            for k, v in x.items():
                other_says(v, inside_stage or k == "stages")
        elif isinstance(x, list):
            for v in x:
                other_says(v, inside_stage)
    other_says({k: v for k, v in Q.items() if k != "quests"})
    for q in Q["quests"].values():
        other_says({k: v for k, v in q.items() if k != "stages"})
    # the player's answers, in each player voice
    opts = set()
    for conv in D["conversations"].values():
        for n in conv["nodes"].values():
            opts |= {o["text"] for o in n["options"]}
    for q in Q["quests"].values():
        for st in q["stages"].values():
            for n in st.get("nodes", {}).values():
                opts |= {o["text"] for o in n.get("options", [])}
    for o in V["player"]["voices"].values():
        for t in opts:
            for kind, p in pieces(t):
                if kind == "text":
                    need(o, "neutral", p)
    return V, res, who, jobs


def index_of(V, res, who, clips, jobs=()):
    """The index the slice reads: each voiced resident's voice by their id (ADR-0060; not by their place in the list,
    which moves when the town is dealt again), the player's voices, and every piece made. jobs: the pieces planned,
    so the note can say how many are still to make."""
    cfgs = sorted({cfg_id(o) for o in who if o} | {cfg_id(o) for o in V["player"]["voices"].values()})
    made = sum(1 for k in jobs if "|".join(k) in clips)
    return {"note": "Made by Tools/voice/make_dialogue_voices.py from World/rules/voices.json, dialogue.json and quests.json. Do not edit."
                    + (" %d of %d pieces are made so far; a line with no piece is wordless syllables." % (made, len(jobs)) if made < len(jobs) else ""),
            "play": V["play"], "configs": cfgs, "residents": {r["id"]: cfgs.index(cfg_id(o)) for r, o in zip(res, who) if o},
            "player": {k: cfgs.index(cfg_id(o)) for k, o in V["player"]["voices"].items()}, "player_default": V["player"]["default"],
            "clips": dict(sorted(clips.items()))}


def write_index(index):
    with open(os.path.join(OUT, "index.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(index, f, ensure_ascii=False, separators=(",", ":"))
    with open(os.path.join(OUT, "index.js"), "w", encoding="utf-8", newline="\n") as f:
        f.write("// Generated by Tools/voice/make_dialogue_voices.py. Do not edit.\nwindow.VOICES = " + json.dumps(index, ensure_ascii=False, separators=(",", ":")) + ";\n")


def main(*args):
    flags = {a for a in args if a.startswith("--")}
    model_dir = next((a for a in args if not a.startswith("--")), None)
    if flags - {"--plan", "--index"} or not (model_dir or flags):
        raise SystemExit(__doc__)
    V, res, who, jobs = plan()
    idx_path = os.path.join(OUT, "index.json")
    old = read(idx_path)["clips"] if os.path.exists(idx_path) else {}
    key = lambda c, m, t: c + "|" + m + "|" + t
    todo = [(k, o) for k, o in jobs.items() if key(*k) not in old or not os.path.exists(os.path.join(OUT, old[key(*k)][0]))]
    print("%d pieces for %d voices; %d to make" % (len(jobs), len({k[0] for k in jobs}), len(todo)))
    mute = sorted({r["role"] for r, o in zip(res, who) if o is None})
    print("%d residents: %d with a voice of their own, %d not voiced%s" % (
        len(res), sum(1 for r in res if r["id"] in V.get("people", {})), sum(1 for o in who if o is None),
        " (no pool for " + ", ".join(mute) + ")" if mute else ""))
    if "--plan" in flags:
        return
    if "--index" in flags:                               # who has which voice now; nothing is voiced or removed
        clips = {kk: v for kk, v in old.items() if os.path.exists(os.path.join(OUT, v[0]))}
        write_index(index_of(V, res, who, clips, jobs))
        print("index: %d residents voiced, %d pieces" % (sum(1 for o in who if o), len(clips)))
        return
    sys.path.insert(0, HERE)
    import soundfile as sf
    from kokoro_onnx import Kokoro
    from make_voice_samples import voice
    k = Kokoro(os.path.join(model_dir, "kokoro-v1.0.int8.onnx"), os.path.join(model_dir, "voices-v1.0.bin"))
    cast = {cfg_id(o) for o in who if o and o.get("cast")}   # the cast's clips are not this script's to make or to remove
    clips = {kk: v for kk, v in old.items() if tuple(kk.split("|", 2)) in jobs or kk.split("|", 1)[0] in cast}
    t0 = time.time()
    for n, ((c, m, t), o) in enumerate(todo):
        s, rate = voice(k, t, o, V["moods"][m])
        rel = c + "/" + hashlib.md5(key(c, m, t).encode()).hexdigest()[:12] + ".ogg"
        os.makedirs(os.path.join(OUT, c), exist_ok=True)
        sf.write(os.path.join(OUT, rel), s, rate, format="OGG", subtype="VORBIS")
        clips[key(c, m, t)] = [rel, round(len(s) / rate, 3)]
        if n % 100 == 0:
            print("  %d / %d  (%.0f s)" % (n, len(todo), time.time() - t0), flush=True)
    keep = {v[0] for v in clips.values()}                # remove pieces no line needs any more
    for f in glob.glob(os.path.join(OUT, "*", "*.ogg")):
        if os.path.relpath(f, OUT).replace(os.sep, "/") not in keep:
            os.remove(f)
    write_index(index_of(V, res, who, clips, jobs))
    size = sum(os.path.getsize(p) for p in glob.glob(os.path.join(OUT, "*", "*.ogg"))) / 1e6
    print("index: %d pieces, %.1f MB of audio, %.0f s" % (len(clips), size, time.time() - t0))


if __name__ == "__main__":
    main(*sys.argv[1:])
