# Tools/voice

Spoken dialogue for the slice (SND-018, ADR-0019), made offline with the Kokoro voice model
(`docs/licenses/kokoro-tts.md`). The model and its Python packages are not in the repository.

| File | What it does |
|---|---|
| `make_dialogue_voices.py` | Voices every line of `World/rules/dialogue.json` and `quests.json` for every voice that can say it, by `World/rules/voices.json`, into `Prototype/play/audio/voices/` with `index.js`. Keeps pieces already made. `--plan` only counts. |
| `make_voice_samples.py` | The cast samples the owner chose from (`Prototype/play/cast_voices.html`), and the shaping of a line by mood that both scripts use |
| `test_voices.py` | The cast, the owner's picks, and that every line can be spoken: `python -m unittest test_voices` |

## Setting up (once)
```
python -m venv kokoro_env
kokoro_env\Scripts\python -m pip install kokoro-onnx soundfile
```
Download `kokoro-v1.0.int8.onnx` and `voices-v1.0.bin` from the `model-files-v1.0` release of
github.com/thewh1teagle/kokoro-onnx into a folder of their own.

## After changing a line, a voice or a pool
```
kokoro_env\Scripts\python make_dialogue_voices.py <model folder>
python -m unittest test_voices
```
A full run is about one second a piece on the owner's PC (about an hour for everything); after a small change it only
voices what is new.
