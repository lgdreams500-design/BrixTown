"""Voice every line of one named character, in one run, by the cast recipe (ADR-0060, SND-018).

    python make_cast_voices.py PERSON DESIGN_DIR BASE_DIR     voice what is new for that person, write the index
    python make_cast_voices.py PERSON --plan                  only count what would be made
    python make_cast_voices.py PERSON --page OUT.html         a page to listen to their lines on, in story order

PERSON is an id in cast_voices.json (oda, noor, ...). DESIGN_DIR and BASE_DIR hold the two voice models, Qwen3-TTS
1.7B VoiceDesign and Base (Apache-2.0, docs/licenses/qwen3-tts.md; not in the repository).

The recipe, the same for everyone and with nothing chosen by ear (cast_voices.json "recipe"):
 1. Voice card. The person's description (gender, age and accent first) and one fixed neutral sentence give
    cards/<person>.flac. The owner approves this one clip; it is then kept and never made again.
 2. Mood cards. The Base model says a few fixed sentences for each mood in the voice of the voice card:
    cards/<person>_<mood>.flac. Kept too, so the voice cannot move between runs.
 3. Lines. Every line the person speaks in World/rules/quests_story.json is spoken by the Base model in the voice of
    the card of the line's mood, all in one run, with the recipe's one seed.
A clip is written as the model spoke it: nothing is cut, padded or re-timed afterwards. Clips go to
Prototype/play/audio/voices/cast_<person>/ and into the index beside the town's voices; a clip already there is kept,
so a line the owner has approved is not voiced again unless its words or mood change.
"""
import base64, hashlib, html, json, os, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..")))
import make_dialogue_voices as M

CAST = os.path.join(HERE, "cast_voices.json")
CARDS = os.path.join(HERE, "cards")


def cast():
    return M.read(CAST)


def person(C, pid):
    return next(p for p in C["people"] if p["id"] == pid)


def cfg(pid):
    return "cast_" + pid


def profile_problems(p):
    """What a profile still lacks before a voice may be made for it: gender, age and accent must be stated, and the
    description given to the model must open with the gender and the age (a model settles what is left open by chance)."""
    out = []
    if p.get("voice_is") not in ("woman", "man"):
        out.append("voice_is must be woman or man")
    if not isinstance(p.get("age"), int):
        out.append("age must be stated")
    if not p.get("accent"):
        out.append("accent must be stated")
    d = p.get("description") or ""
    if p.get("voice_is") and p.get("age") and not (p["voice_is"] in d and str(p["age"]) in d):
        out.append("the description must state the gender and the age")
    return out


def lines(pid):
    """Every line the person speaks in the story, in the order of the quest data: (where, mood, text). A line's speaker
    is the node's 'who' (a cast id), else the resident the stage sends the player to, else the quest's giver."""
    names = M.read(M.RULES, "story_cast.json")["cast"]
    rid = names[pid]
    out = []
    for qid, q in M.read(M.RULES, "quests_story.json")["quests"].items():
        for sid, st in q["stages"].items():
            talk = (st.get("goal") or {}).get("talk")
            for nid, n in st.get("nodes", {}).items():
                who = n.get("who") or talk or q.get("giver")
                if names.get(who, who) == rid and isinstance(n.get("say"), str):
                    out.append((q.get("title", qid) + " · " + nid, n.get("mood", "neutral"), n["say"]))
    return out


def jobs(pid):
    """The pieces to voice: {(mood, piece)}. A line is cut at its blanks as the game cuts it (make_dialogue_voices
    pieces()); a blank is voiced only where it is the player's default name."""
    V = M.read(M.RULES, "voices.json")
    out = {}
    for _, mood, text in lines(pid):
        for kind, piece in M.pieces(text):
            if kind == "text":
                out[(mood, piece)] = True
            elif piece == "player":
                out[(mood, M.norm(V["player"]["default_name"]))] = True
    return list(out)


def key(pid, mood, piece):
    return cfg(pid) + "|" + mood + "|" + piece


def rel(pid, mood, piece):
    return cfg(pid) + "/" + hashlib.md5(key(pid, mood, piece).encode()).hexdigest()[:12] + ".ogg"


def card_path(pid, mood):
    return os.path.join(CARDS, pid + ("" if mood == "neutral" else "_" + mood) + ".flac")


def card_text(C, p, mood):
    if mood == "neutral":
        return p.get("card_text") or C["recipe"]["card_text"]      # a card that is an approved clip says its own words
    return C["recipe"]["mood_text"][mood]


class Voices:
    """The two models, loaded when first needed, and the cards as the Base model takes them."""

    def __init__(self, C, design_dir, base_dir):
        self.C, self.design_dir, self.base_dir, self.design, self.base, self.prompts = C, design_dir, base_dir, None, None, {}

    def _load(self, which):
        import torch
        from qwen_tts import Qwen3TTSModel
        return Qwen3TTSModel.from_pretrained(self.design_dir if which == "design" else self.base_dir, device_map="cpu", dtype=torch.float32)

    def _write(self, path, wavs, rate):
        import numpy as np, soundfile as sf
        s = np.asarray(wavs[0], dtype=np.float32)
        s = s / (float(np.abs(s).max()) or 1.0) * 0.9
        os.makedirs(os.path.dirname(path), exist_ok=True)
        if path.endswith(".flac"):
            sf.write(path, s, rate, format="FLAC")
        else:
            sf.write(path, s, rate, format="OGG", subtype="VORBIS")
        return len(s) / rate

    def prompt(self, p, mood):
        import soundfile as sf, torch
        path = self.card(p, mood)
        if path not in self.prompts:
            s, r = sf.read(path, dtype="float32")
            self.base = self.base or self._load("base")
            self.prompts[path] = self.base.create_voice_clone_prompt(ref_audio=(s, r), ref_text=card_text(self.C, p, mood))
        return self.prompts[path]

    def card(self, p, mood):
        import torch
        path, R = card_path(p["id"], mood), self.C["recipe"]
        if os.path.exists(path):
            return path
        wrong = profile_problems(p)
        if wrong:
            raise SystemExit("%s: %s" % (p["id"], "; ".join(wrong)))
        torch.manual_seed(R["seed"])
        if mood == "neutral":
            self.design = self.design or self._load("design")
            wavs, rate = self.design.generate_voice_design(text=R["card_text"], language=self.C["language"], instruct=p["description"] + " " + R["card_direction"])
            print("NEW VOICE CARD %s: the owner must hear and approve it before any line is kept" % path, flush=True)
        else:
            pr = self.prompt(p, "neutral")
            torch.manual_seed(R["seed"])
            wavs, rate = self.base.generate_voice_clone(text=R["mood_text"][mood], language=self.C["language"], voice_clone_prompt=pr)
        self._write(path, wavs, rate)
        return path

    def line(self, p, mood, piece, out):
        import torch
        pr = self.prompt(p, mood)
        torch.manual_seed(self.C["recipe"]["seed"])
        wavs, rate = self.base.generate_voice_clone(text=piece, language=self.C["language"], voice_clone_prompt=pr)
        return self._write(out, wavs, rate)


def page(pid, out):
    C = cast()
    p = person(C, pid)
    clips = M.read(os.path.join(M.OUT, "index.json"))["clips"]
    e = html.escape

    def player(path):
        b = base64.b64encode(open(path, "rb").read()).decode()
        return '<audio controls preload="none" src="data:audio/ogg;base64,%s"></audio>' % b

    rows = []
    for where, mood, text in lines(pid):
        parts = [M.norm(t) for k, t in M.pieces(text) if k == "text"]
        have = [clips.get(key(pid, mood, t)) for t in parts]
        rows.append('<section><p class="w">%s · %s</p><blockquote>%s</blockquote>%s</section>' % (
            e(where), e(mood), e(text), "".join(player(os.path.join(M.OUT, c[0])) if c else '<p class="w">not voiced yet</p>' for c in have)))
    doc = """<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>%s: voiced lines</title><style>
:root { --bg:#f6f2ea; --card:#fffdf8; --ink:#2b2622; --soft:#6d635a; --line:#e2d9cb; --accent:#a8752a; }
@media (prefers-color-scheme: dark) { :root { --bg:#1d1a17; --card:#27231f; --ink:#efe8dd; --soft:#a89e92; --line:#3a342e; --accent:#d9a45a; } }
body { margin:0; background:var(--bg); color:var(--ink); font:16px/1.5 Georgia, serif; } main { max-width:820px; margin:0 auto; padding:24px 16px 64px; }
h1 { font-size:26px; margin:8px 0 4px; } .w, .lead { font:13px system-ui, 'Segoe UI', sans-serif; color:var(--soft); margin:0 0 4px; }
section { background:var(--card); border:1px solid var(--line); border-radius:10px; padding:14px 16px; margin:14px 0; }
.w { letter-spacing:.03em; color:var(--accent); } blockquote { margin:0 0 8px; font-size:18px; } audio { width:100%%; height:36px; }
</style></head><body><main><h1>%s</h1><p class="lead">%s, %d · %s · every line they speak in the story, as it is in the game. Made %s.</p>%s</main></body></html>
""" % (e(p["name"]), e(p["name"]), e(p["voice_is"]), p["age"], e(p["trade"]), time.strftime("%Y-%m-%d"), "".join(rows))
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write(doc)
    print("wrote %s, %d lines" % (out, len(rows)))


def main(pid=None, *args):
    if not pid or pid.startswith("--"):
        raise SystemExit(__doc__)
    C = cast()
    p = person(C, pid)
    if "--page" in args:
        return page(pid, args[args.index("--page") + 1])
    idx_path = os.path.join(M.OUT, "index.json")
    index = M.read(idx_path)
    todo = [(m, t) for m, t in jobs(pid) if key(pid, m, t) not in index["clips"] or not os.path.exists(os.path.join(M.OUT, index["clips"][key(pid, m, t)][0]))]
    print("%s: %d pieces in %d lines; %d to make" % (p["name"], len(jobs(pid)), len(lines(pid)), len(todo)))
    if "--plan" in args:
        return
    paths = [a for a in args if not a.startswith("--")]
    if len(paths) != 2:
        raise SystemExit(__doc__)
    V = Voices(C, *paths)
    t0 = time.time()
    for n, (mood, piece) in enumerate(todo):                 # the whole part in one run, one piece after another
        dur = V.line(p, mood, piece, os.path.join(M.OUT, rel(pid, mood, piece)))
        index["clips"][key(pid, mood, piece)] = [rel(pid, mood, piece), round(dur, 3)]
        with open(idx_path, "w", encoding="utf-8", newline="\n") as f:      # kept as it goes: a long run that stops loses nothing
            json.dump(index, f, ensure_ascii=False, separators=(",", ":"))
        print("  %d / %d  %s  (%.0f s)" % (n + 1, len(todo), mood, time.time() - t0), flush=True)
    M.main("--index")                                        # who has which voice, and the index.js the game reads


if __name__ == "__main__":
    main(*sys.argv[1:])
