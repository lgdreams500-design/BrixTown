# Kokoro voice model (dialogue voice samples)

| | |
|---|---|
| What | Kokoro-82M text-to-speech model, run offline through `kokoro-onnx` to make voiced dialogue samples |
| Used for | `Prototype/play/audio/voice_sample/*.wav` (the cast's sample lines and the player's replies, listed in `cast.json`), made by `Tools/voice/make_voice_samples.py` and played by `Prototype/play/cast_voices.html` |
| Model files | `kokoro-v1.0.int8.onnx`, `voices-v1.0.bin`, from the `model-files-v1.0` release of github.com/thewh1teagle/kokoro-onnx (2026-11-22) |
| Model licence | Apache-2.0 (Kokoro-82M by hexgrad, weights and voices) |
| Runtime used to make the audio | `kokoro-onnx` 0.6.1 (MIT), `onnxruntime` 1.32 (MIT), `espeakng-loader` 0.2.4 with espeak-ng (GPL-3.0, used only as a tool to turn text into pronunciations; not shipped) |
| Shipped in the game | Only the generated audio files. No model, library or espeak-ng code is in the repository or the game. |
| Status | Sample for the owner to judge (2026-11-22). Spoken dialogue reverses Q-14 ("text first") and needs the owner's decision before it goes into the game. |

The model and tools live outside the repository (not committed). To regenerate: install `kokoro-onnx` and `soundfile`
in a virtual environment, download the two model files, and run the generation script with the line, voice and language.
