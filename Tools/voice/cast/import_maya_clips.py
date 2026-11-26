"""Bring a person's Maya1 clips into the game's voice index (SND-018, ADR-0063).

    python import_maya_clips.py PERSON CLIP_DIR [--leave LINE_ID ...]

CLIP_DIR holds the clips one Maya1 run made, named <quest>__<stage>__<node>_maya.wav (C:\\Projects\\voice_envs, outside
the repository; not in the recipe of make_cast_voices.py). Each clip is the whole line as the model spoke it, and is
only re-encoded to Ogg Vorbis: no gain, trim or pacing change (the owner's rule, 2026-11-26: no post-processing of
voices). Older takes (<name>.takeN.wav) and clips of other models are ignored. It is written where make_cast_voices.py
writes, Prototype/play/audio/voices/cast_<person>/, under the same key (cast_<person>|<mood>|<words>), so the game
finds it the same way.

Left out: a line with a blank ({player}): the game cuts it at the blank, and a whole-line clip with a name spoken
inside it cannot be looked up; and the lines named with --leave (a clip with a fault, kept outside the repository).
A person's entry in World/rules/voices.json gets "partial": what is not voiced yet; their other lines are murmured.
"""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import make_dialogue_voices as M          # noqa: E402
import make_cast_voices as C              # noqa: E402


def story_lines(pid):
    """{line id: (mood, say)} for every line the person speaks, as make_cast_voices.lines() finds them."""
    names = M.read(M.RULES, "story_cast.json")["cast"]
    out = {}
    for qid, q in M.read(M.RULES, "quests_story.json")["quests"].items():
        for sid, st in q["stages"].items():
            talk = (st.get("goal") or {}).get("talk")
            for nid, n in st.get("nodes", {}).items():
                who = n.get("who") or talk or q.get("giver")
                if names.get(who, who) == names[pid] and isinstance(n.get("say"), str):
                    out["%s/%s/%s" % (qid, sid, nid)] = (n.get("mood", "neutral"), n["say"])
    return out


def main(pid, clip_dir, *args):
    import soundfile as sf
    leave = set(args[args.index("--leave") + 1:]) if "--leave" in args else set()
    lines = story_lines(pid)
    idx_path = os.path.join(M.OUT, "index.json")
    index = M.read(idx_path)
    made, skipped = 0, []
    for lid, (mood, say) in lines.items():
        src = os.path.join(clip_dir, lid.replace("/", "__") + "_maya.wav")
        if not os.path.exists(src):
            continue
        parts = M.pieces(say)
        if lid in leave or len(parts) != 1 or parts[0][0] != "text":
            skipped.append(lid)
            continue
        piece = parts[0][1]
        rel = C.rel(pid, mood, piece)
        s, rate = sf.read(src, dtype="float32")
        out = os.path.join(M.OUT, rel)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with sf.SoundFile(out, "w", rate, 1, format="OGG", subtype="VORBIS") as o:   # the samples as spoken, only re-encoded
            for i in range(0, len(s), 8192):                            # in blocks: one large write crashes libsndfile's Vorbis encoder
                o.write(s[i:i + 8192])
        index["clips"][C.key(pid, mood, piece)] = [rel, round(len(s) / rate, 3)]
        made += 1
    with open(idx_path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(index, f, ensure_ascii=False, separators=(",", ":"))
    print("%s: %d clips in, %d left out (%s); %d lines not voiced" % (pid, made, len(skipped), ", ".join(skipped),
                                                                     len(lines) - made - len(skipped)))
    M.main("--index")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    main(*sys.argv[1:])
