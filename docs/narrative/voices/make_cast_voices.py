"""Voice the Baker cast's candidate voices for the owner to listen to (docs/narrative/voices/cast_voices.html).

    <kokoro python> make_cast_voices.py MODEL_DIR [--only noor,tam]

A proposal only: it reads the game's voice rules and the narrative drafts and writes nothing outside this folder.

Reads   cast_voices.json                      the candidates, the reasons and which lines each character says
        ../src/cast.json, ../src/q*.json, r_*.json   the cast and their lines (looked up by file / stage / node)
        World/rules/voices.json               the moods, the pools and the player's voices
        World/rules/quests.json               live-game lines, for the two people with too few lines in the drafts
        World/city_01/residents.json and Prototype/play/audio/voices/index.json   what each live resident speaks in today
Writes  audio/<character>_<option>_<n>.ogg    each candidate saying each line, in the line's own mood
        data.js                               everything the page shows (window.CAST_VOICES)

Every clip is made by the game's own function, voice() in Tools/voice/make_voice_samples.py, with the game's moods, as
OGG like the game's (ADR-0019). MODEL_DIR holds kokoro-v1.0.int8.onnx and voices-v1.0.bin; needs kokoro-onnx and
soundfile (Tools/voice/README.md). Clips already made are kept unless their text, mood or voice changed.
"""
import hashlib, json, os, re, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
SRC = os.path.join(HERE, "..", "src")
AUDIO = os.path.join(HERE, "audio")
TEST = "Forty years I have counted the loaves before the tram. Nobody asked me why. It was the same bread, done right."


def read(*p):
    with open(os.path.join(*p), encoding="utf-8") as f:
        return json.load(f)


def cfg_id(o):
    """A voice setting's name, as Tools/voice/make_dialogue_voices.py names it."""
    s = o["voice"]
    if o.get("shift", 1) != 1:
        s += "_p%d" % round(o["shift"] * 100)
    if o.get("speed", 1) != 1:
        s += "_v%d" % round(o["speed"] * 100)
    return s


def base(cfg):
    """The model voice behind a setting's name: af_sarah_p92_v88 -> af_sarah."""
    return re.sub(r"(_p\d+)?(_v\d+)?$", "", cfg)


def draft_line(f, stage, node):
    q = read(SRC, f + ".json")
    for st in q["stages"]:
        if st["id"] == stage:
            n = st["nodes"][node]
            return {"who": n["who"], "mood": n.get("mood", "neutral"), "text": n["say"], "from": "%s · %s · %s" % (q["title"], st["title"], node)}
    raise KeyError("%s has no stage %s" % (f, stage))


def live_line(path, node):
    quest, stage = path.split("/")
    q = read(ROOT, "World", "rules", "quests.json")["quests"][quest]
    st = q["stages"][stage]
    n = st["nodes"][node]
    return {"who": (st.get("goal") or {}).get("talk") or q.get("giver"), "mood": n.get("mood", "neutral"), "text": n["say"],
            "from": "the live game · %s · %s" % (q.get("title", quest), node), "live": True}


def today(resident):
    """What a live resident speaks in: by the rule (their role's pool, by a hash of their id), and in the shipped audio."""
    V = read(ROOT, "World", "rules", "voices.json")
    res = read(ROOT, "World", "city_01", "residents.json")["residents"]
    i = next(k for k, r in enumerate(res) if r["id"] == resident)
    pool = V["roles"].get(res[i]["role"])
    rule = cfg_id(pool[int(hashlib.md5(resident.encode()).hexdigest(), 16) % len(pool)]) if pool else None
    idx = read(ROOT, "Prototype", "play", "audio", "voices", "index.json")
    slice_ = idx["configs"][idx["residents"][i]] if i < len(idx["residents"]) else None
    return {"role": res[i]["role"], "rule": rule, "slice": slice_, "pool": [cfg_id(o) for o in pool or []]}


def f0_track(s, rate, lo=60, hi=420):
    """Pitch of the clearly voiced 40 ms frames, by autocorrelation (the shortest lag nearly as good as the best)."""
    import numpy as np
    n, hop, out, peak = int(rate * 0.04), int(rate * 0.01), [], float(abs(s).max()) or 1
    for a in range(0, len(s) - n, hop):
        x = s[a:a + n]
        if (x * x).mean() ** 0.5 < 0.05 * peak:
            continue
        x = x - x.mean()
        ac = np.correlate(x, x, "full")[n - 1:]
        if ac[0] <= 0:
            continue
        seg = (ac / ac[0])[int(rate / hi):int(rate / lo)]
        top = seg.max()
        if top > 0.5:
            cand = [i for i in range(1, len(seg) - 1) if seg[i] >= 0.9 * top and seg[i] >= seg[i - 1] and seg[i] >= seg[i + 1]]
            out.append(rate / (int(rate / hi) + (cand[0] if cand else int(seg.argmax()))))
    return np.array(out)


def measure(s, rate):
    import numpy as np
    f = f0_track(s, rate)
    spec = abs(np.fft.rfft(s))
    return {"hz": round(float(np.median(f))), "lo": round(float(np.percentile(f, 10))), "hi": round(float(np.percentile(f, 90))),
            "bright": round(float((np.fft.rfftfreq(len(s), 1 / rate) * spec).sum() / spec.sum())), "secs": round(len(s) / rate, 2)}


def main(model_dir, *flags):
    only = next((f.split("=", 1)[1].split(",") for f in flags if f.startswith("--only=")), None)
    sys.path.insert(0, os.path.join(ROOT, "Tools", "voice"))
    import soundfile as sf
    from kokoro_onnx import Kokoro
    from make_voice_samples import voice                    # the game's own shaping of a line
    C = read(HERE, "cast_voices.json")
    V = read(ROOT, "World", "rules", "voices.json")
    people = {p["id"]: p for p in read(SRC, "cast.json")["people"]}
    k = Kokoro(os.path.join(model_dir, "kokoro-v1.0.int8.onnx"), os.path.join(model_dir, "voices-v1.0.bin"))
    os.makedirs(AUDIO, exist_ok=True)
    old_path = os.path.join(HERE, "made.json")              # what each clip was made from, so unchanged ones are kept
    made = read(old_path) if os.path.exists(old_path) else {}
    pooled = {}                                             # which roles' pools hold each voice setting
    for role, pool in V["roles"].items():
        for o in pool:
            pooled.setdefault(cfg_id(o), []).append(role)

    def model_voice(o):
        """The option as the model takes it: a mix 'a+b' becomes the even blend of the two voices' styles."""
        if "+" not in o["voice"]:
            return o
        a, b = o["voice"].split("+")
        return dict(o, voice=(k.get_voice_style(a) + k.get_voice_style(b)) / 2)

    def clip(name, text, o, mood):
        key = json.dumps([text, mood, cfg_id(o), o.get("lang"), V["moods"][mood]], sort_keys=True)
        path = os.path.join(AUDIO, name + ".ogg")
        if made.get(name, {}).get("key") == key and os.path.exists(path):
            return made[name]["secs"]
        s, rate = voice(k, text, model_voice(o), V["moods"][mood])
        sf.write(path, s, rate, format="OGG", subtype="VORBIS")
        made[name] = {"key": key, "secs": round(len(s) / rate, 2)}
        return made[name]["secs"]

    def measured(o):
        name = "m|" + cfg_id(o) + "|" + o.get("lang", "en-us")
        if name not in made:
            s, rate = voice(k, TEST, model_voice(o), V["moods"]["neutral"])
            made[name] = measure(s, rate)
        return made[name]

    out, t0, n = [], time.time(), 0
    for c in C["characters"]:
        p = people[c["id"]]
        lines = [live_line(a[1], a[2]) if a[0] == "live" else draft_line(*a) for a in c["lines"]]
        for ln in lines:
            if not ln.get("live") and ln["who"] != c["id"]:
                raise ValueError("%s: line %s is said by %s" % (c["id"], ln["from"], ln["who"]))
        d = {"id": c["id"], "name": p["name"], "age": p.get("age"), "trade": p["career"], "status": p["status"], "color": p["color"],
             "quote": p["quote"], "note": p["voice"], "difficulty": c["difficulty"], "summary": c["summary"], "lines": lines, "options": []}
        if p.get("resident"):
            d["resident"] = p["resident"]
            d["today"] = today(p["resident"])
        opts = list(c["options"]) + ([dict(c["now"], key="now")] if c.get("now") else [])
        for o in opts:
            if d.get("today") and o["key"] == "now" and cfg_id(o) != d["today"]["slice"]:
                raise ValueError("%s: the slice plays %s today, not %s" % (c["id"], d["today"]["slice"], cfg_id(o)))
            e = {x: o[x] for x in ("key", "rec", "about", "why", "needs", "current", "lang") if x in o}
            e.update(cfg=cfg_id(o), shift=o.get("shift", 1), speed=o.get("speed", 1), pools=pooled.get(cfg_id(o), []),
                     base_pools=sorted({r for cf, rs in pooled.items() if base(cf) == o["voice"] for r in rs}))
            if only is None or c["id"] in only:
                e["clips"] = []
                for i, ln in enumerate(lines):
                    name = "%s_%s_%d" % (c["id"], o["key"], i + 1)
                    e["clips"].append({"file": "audio/" + name + ".ogg", "secs": clip(name, ln["text"], o, ln["mood"])})
                    n += 1
            else:
                e["clips"] = [{"file": "audio/%s_%s_%d.ogg" % (c["id"], o["key"], i + 1), "secs": made["%s_%s_%d" % (c["id"], o["key"], i + 1)]["secs"]} for i in range(len(lines))]
            e["measure"] = measured(o)
            d["options"].append(e)
        out.append(d)
        print("%-5s %d voices  (%.0f s)" % (c["id"], len(opts), time.time() - t0), flush=True)
        with open(old_path, "w", encoding="utf-8", newline="\n") as f:
            json.dump(made, f, ensure_ascii=False, indent=0, sort_keys=True)

    # every English voice in the model, measured on one sentence, and who in town already uses it
    voices = {}
    for v in sorted(k.get_voices()):
        if v[0] in "ab":
            o = {"voice": v, "lang": "en-gb" if v[0] == "b" else "en-us"}
            voices[v] = dict(measured(o), pools=sorted({r for cf, rs in pooled.items() if base(cf) == v for r in rs}),
                             player=[w for w, po in V["player"]["voices"].items() if po["voice"] == v])
    with open(old_path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(made, f, ensure_ascii=False, indent=0, sort_keys=True)
    res = read(ROOT, "World", "city_01", "residents.json")["residents"]
    idx = read(ROOT, "Prototype", "play", "audio", "voices", "index.json")
    roles = sorted({r["role"] for r in res})
    data = {"note": "Made by docs/narrative/voices/make_cast_voices.py from cast_voices.json. Do not edit.",
            "made": time.strftime("%Y-%m-%d %H:%M"), "characters": out, "together": C["together"], "scenes": C["scenes"], "voices": voices, "moods": V["moods"],
            "system": {"residents": len(res), "indexed": len(idx["residents"]), "roles_without_pool": [r for r in roles if r not in V["roles"]],
                       "pool_roles": len(V["roles"]), "english_voices": len(voices), "test_sentence": TEST}}
    with open(os.path.join(HERE, "data.js"), "w", encoding="utf-8", newline="\n") as f:
        f.write("// Made by make_cast_voices.py. Do not edit.\nwindow.CAST_VOICES = " + json.dumps(data, ensure_ascii=False, separators=(",", ":")) + ";\n")
    keep = {cl["file"][6:] for d in out for e in d["options"] for cl in e["clips"]}
    for f in os.listdir(AUDIO):                             # clips no candidate needs any more
        if f.endswith(".ogg") and f not in keep:
            os.remove(os.path.join(AUDIO, f))
    size = sum(os.path.getsize(os.path.join(AUDIO, f)) for f in os.listdir(AUDIO)) / 1e6
    print("%d clips, %.1f MB, %.0f s" % (len(keep), size, time.time() - t0))


if __name__ == "__main__":
    main(*sys.argv[1:])
