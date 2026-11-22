"""Voice the cast's sample lines with the Kokoro voice model, offline (docs/licenses/kokoro-tts.md).

    python make_voice_samples.py MODEL_DIR [CAST_JSON] [--moods-only]

MODEL_DIR holds kokoro-v1.0.int8.onnx and voices-v1.0.bin (from the kokoro-onnx model-files-v1.0 release; not in the
repository). Needs the packages kokoro-onnx and soundfile. Reads Prototype/play/audio/voice_sample/cast.json and writes,
beside it, <character>_<option>.wav for each character's line and you_<option>_<character>.wav for the player's reply.
A 'shift' above 1 raises pitch and voice size together (a younger voice), below 1 lowers them (an older one).

Each character's chosen voice is also voiced in every mood of cast.json "moods", as <character>_<option>_<mood>.wav.
A mood sets the pitch and pace, lays a melody over the line (bounce, sink, punch, leap, wonder), colours the tone, adds
grit or breath, sets the loudness, changes the punctuation the model reads (never the words), and can put a sound
before the line (a laugh, a sigh, a huff, a gasp, a "hmm") that the caption leaves out.
"""
import json, os, re, sys, time
import numpy as np
import soundfile as sf
from kokoro_onnx import Kokoro

HERE = os.path.dirname(os.path.abspath(__file__))
CAST = os.path.join(HERE, "..", "..", "Prototype", "play", "audio", "voice_sample", "cast.json")
RNG = np.random.default_rng(7)


def shifted(samples, shift):
    """Play back `shift` times faster: pitch and formants move together. Made slower first, so the pace comes out right."""
    if abs(shift - 1) < 1e-3:
        return samples
    n = int(len(samples) / shift)
    return np.interp(np.arange(n) * shift, np.arange(len(samples)), samples).astype(np.float32)


def melody_curve(kind, n, rate, depth):
    """Semitones over the line: the tune a mood lays over the voice."""
    t = np.arange(n) / rate
    x = t / max(t[-1], 1e-3)                                # 0 at the start, 1 at the end
    if kind == "bounce":                                    # happy: lively ups and downs, a lift at the end
        return depth * (0.55 * np.sin(2 * np.pi * 2.3 * t) + 0.45 * x)
    if kind == "sink":                                      # sad: the voice sinks through the line, with a small tremble
        return depth * (0.25 - x) + 0.25 * np.sin(2 * np.pi * 5.5 * t) * x
    if kind == "punch":                                     # angry: hard stresses about twice a second
        ph = (t * 2.1) % 1
        return depth * np.exp(-ph * 9) - 0.25 * depth
    if kind == "leap":                                      # surprised: up high at once, down, and up again at the end
        return depth * (np.exp(-x * 5) * 0.9 + np.clip((x - 0.7) / 0.3, 0, 1) ** 2 * 0.8 - 0.2)
    if kind == "wonder":                                    # thinking: drifts down, wavers, lifts as if unsure
        return depth * (-0.5 * x + 0.25 * np.sin(2 * np.pi * 0.9 * t) + 0.9 * np.clip((x - 0.82) / 0.18, 0, 1) ** 2)
    return np.zeros(n)


def with_melody(s, rate, kind, depth):
    """Lay a tune over the line by reading it faster where it should be higher and slower where lower."""
    if not kind or not depth:
        return s
    step = 2 ** (melody_curve(kind, len(s), rate, depth) / 12)
    pos = np.cumsum(step)
    pos = pos[pos < len(s) - 1]
    return np.interp(pos, np.arange(len(s)), s).astype(np.float32)


def band(noise, rate, lo, hi):
    f = np.fft.rfftfreq(len(noise), 1 / rate)
    return np.fft.irfft(np.fft.rfft(noise) * ((f > lo) & (f < hi)), len(noise)).astype(np.float32)


def breath_sound(kind, rate):
    """Air: a sigh (long falling exhale), a huff (short hard exhale), a gasp (quick rising inhale)."""
    if kind == "sigh":
        n = int(rate * 0.9); e = np.sin(np.linspace(0, np.pi, n)) ** 1.5 * np.linspace(1, 0.4, n); lo, hi = 300, 2600
    elif kind == "huff":
        n = int(rate * 0.22); e = np.exp(-np.linspace(0, 6, n)); lo, hi = 400, 3500
    else:                                                   # gasp
        n = int(rate * 0.32); e = np.linspace(0, 1, n) ** 2 * (np.linspace(0, 1, n) < 0.93); lo, hi = 900, 5000
    return band(RNG.standard_normal(n).astype(np.float32), rate, lo, hi) * e.astype(np.float32)


def punctuate(text, how):
    """Ask the voice model to phrase the line for a mood by its punctuation alone; the words stay the same."""
    if how == "lift":                                       # brighter endings
        return re.sub(r"\.(\s|$)", r"!\1", text)
    if how == "soften":                                     # no exclamations, trailing ends
        t = text.replace("!", ".")
        return t[:-1] + "..." if t.endswith(".") else t
    if how == "surprise":                                   # questions and endings lifted in surprise
        return re.sub(r"\?", "?!", re.sub(r"\.(\s|$)", r"!\1", text))
    if how == "pause":                                      # thinking: longer pauses at the commas
        return text.replace(", ", "... ")
    return text


def tone(s, rate, tilt, edge, breath):
    """Tone colour by a spectral tilt about 1 kHz (above 0 brighter, below 0 darker), grit, and air in the voice."""
    if tilt:
        f = np.fft.rfftfreq(len(s), 1 / rate)
        g = np.clip((np.maximum(f, 60) / 1000.0) ** tilt, 0.12, 5)
        s = np.fft.irfft(np.fft.rfft(s) * g, len(s)).astype(np.float32)
    if edge:
        s = (np.tanh(edge * s / (np.abs(s).max() or 1)) / np.tanh(edge)).astype(np.float32)
    if breath:                                              # air that follows the loudness of the voice
        env = np.convolve(np.abs(s), np.ones(int(rate * 0.02)) / int(rate * 0.02), "same")
        s = s + band(RNG.standard_normal(len(s)).astype(np.float32), rate, 1500, 6000) * env * breath * 8
    return s.astype(np.float32)


def speak(k, text, o, speed, shift):
    s, rate = k.create(text, voice=o["voice"], speed=speed / shift, lang=o.get("lang", "en-us"))
    return shifted(s, shift), rate


def voice(k, text, o, mood=None):
    m = mood or {}
    shift = o.get("shift", 1.0) * 2 ** (m.get("semitones", 0) / 12)
    speed = o.get("speed", 1.0) * m.get("speed", 1.0)
    s, rate = speak(k, punctuate(text, m.get("punctuation")), o, speed, shift)
    s = with_melody(s, rate, m.get("melody"), m.get("depth", 0))
    s = tone(s, rate, m.get("tilt", 0), m.get("edge", 0), m.get("breath", 0))
    s = s / (float(np.abs(s).max()) or 1.0)
    before = m.get("before")
    if before:                                              # a sound before the line, not in the caption
        if before in ("sigh", "huff", "gasp"):
            pre = breath_sound(before, rate) * {"sigh": 0.35, "huff": 0.5, "gasp": 0.4}[before]
        else:                                               # laugh and hmm, in the speaker's own voice
            pre, _ = speak(k, {"laugh": "Ha ha!", "hmm": "Hmm..."}[before], o, speed * (1.1 if before == "laugh" else 0.8), shift)
            pre = tone(pre, rate, m.get("tilt", 0), 0, 0); pre = pre / (float(np.abs(pre).max()) or 1.0) * (0.7 if before == "laugh" else 0.6)
        s = np.concatenate([pre, np.zeros(int(rate * (0.12 if before in ("huff", "gasp") else 0.22)), np.float32), s])
    return (s * m.get("peak", 0.9)).astype(np.float32), rate     # every line set to its mood's loudness


def main(model_dir, cast_path=CAST, *flags):
    if cast_path.startswith("--"):
        flags, cast_path = (cast_path,) + flags, CAST
    cast = json.load(open(cast_path, encoding="utf-8"))
    out = os.path.dirname(os.path.abspath(cast_path))
    k = Kokoro(os.path.join(model_dir, "kokoro-v1.0.int8.onnx"), os.path.join(model_dir, "voices-v1.0.bin"))
    made = []
    for c in cast["characters"]:
        if "--moods-only" not in flags:
            for key, o in c["options"].items():
                made.append((c["id"] + "_" + key, c["say"], o))
            for pk, po in cast["player"]["options"].items():
                made.append(("you_" + pk + "_" + c["id"], c["reply"], po))
        if c.get("chosen"):                                 # the chosen voice in every mood
            for mood, m in cast.get("moods", {}).items():
                if isinstance(m, dict):
                    made.append((c["id"] + "_" + c["chosen"] + "_" + mood, c["say"], dict(c["options"][c["chosen"]], mood=m)))
    for name, text, o in made:
        t0 = time.time()
        s, rate = voice(k, text, o, o.get("mood"))
        sf.write(os.path.join(out, name + ".wav"), s, rate, subtype="PCM_16")
        print("%-24s %5.2f s  made in %.2f s  (%s)" % (name + ".wav", len(s) / rate, time.time() - t0, o["voice"]))


if __name__ == "__main__":
    main(*sys.argv[1:])
