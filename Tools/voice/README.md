# Tools/voice

Spoken dialogue for the slice (SND-018, ADR-0019, ADR-0060), made offline with the Kokoro voice model
(`docs/licenses/kokoro-tts.md`). The model and its Python packages are not in the repository.

| File | What it does |
|---|---|
| `make_dialogue_voices.py` | Voices every line of `World/rules/dialogue.json` and `quests.json` for every voice that can say it, by `World/rules/voices.json`, into `Prototype/play/audio/voices/` with `index.js`. Keeps pieces already made. `--plan` only counts; `--index` only writes the index again (who has which voice), with no model |
| `make_voice_samples.py` | The cast samples the owner chose from (`Prototype/play/cast_voices.html`), and the shaping of a line by mood that both scripts use |
| `test_voices.py` | The cast, the owner's picks, a named resident's own voice, the named characters' profiles and recipe, the index by resident id, and that every line can be spoken: `python -m unittest test_voices` |
| `cast/` | **Voices for named characters** (the people of a career's story), made by another model and one locked recipe. Read `cast/README.md` before making or changing any such voice |

## Who speaks in which voice
`World/rules/voices.json`:
- `roles`: a pool of voices for each role. A resident takes one by a fixed hash of their id.
- `people`: a named resident's own voice, used instead of the pool (ADR-0060). Only the owner fills it, in chat.
  ```
  "people": {
   "r0000": { "name": "Cleo", "voice": "bf_emma", "lang": "en-gb", "shift": 0.95, "speed": 0.96 },
   "r0001": { "name": "Ada", "voice": ["af_bella", "bm_george"], "lang": "en-gb" }
  }
  ```
  (An example of the shape, not picks.) `name` must be that resident's name, or the tool stops. `voice` as a list of two
  is the two mixed evenly, and `lang` is then the accent of one of them. `shift` and `speed` are as in a pool.
  An entry `{ "name": "Oda", "cast": "oda" }` gives the resident a named character's voice from `cast/`: this
  script makes nothing for them and keeps their clips.
- `roles_not_voiced`: roles with no pool. Their residents are not voiced (wordless syllables), unless they are in `people`.

## Setting up (once)
```
python -m venv kokoro_env
kokoro_env\Scripts\python -m pip install kokoro-onnx soundfile
```
Download `kokoro-v1.0.int8.onnx` and `voices-v1.0.bin` from the `model-files-v1.0` release of
github.com/thewh1teagle/kokoro-onnx into a folder of their own.

## After changing a line, a voice, a pool or a person's voice
```
kokoro_env\Scripts\python make_dialogue_voices.py <model folder>
python -m unittest test_voices
python ..\unreal\people_values.py
```
A full run is about one second a piece on the owner's PC (about an hour for everything); after a small change it only
voices what is new. The last line gives the Unreal build its copy of the index (`Game/Data/talk.json`).

Without the model, `python make_dialogue_voices.py --index` writes the index again from the rules and the pieces already
made: enough when the residents have changed, or to hear a person in a voice that already has their lines.

## Not yet
- `creative` (156 residents) and `binman` (9) have no pool: the owner has not picked one. They are not voiced.
- The people list has no Kokoro picks: the owner has not picked any. It holds Oda's cast voice.
- 976 of 4,055 Kokoro pieces are made (`status` in `voices.json`: the dialogue is not final). The rest is about an hour's run.
- The Kokoro generator does not read the story lines (`quests_story.json`); those are the cast's, voiced by `cast/`.
- The Kokoro model and its Python environment have no fixed home on the owner's PC (they sit in a temporary folder;
  the cast's models are in `C:\Tools\voice\qwen3\`).
