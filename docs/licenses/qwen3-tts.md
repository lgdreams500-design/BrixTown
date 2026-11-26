# Qwen3-TTS voice models (named characters' voices)

| | |
|---|---|
| What | Qwen3-TTS 12Hz 1.7B, two models run offline on the owner's PC: **VoiceDesign** (makes a voice from a written description) and **Base** (speaks new words in the voice of a clip) |
| Used for | The voices of named characters (SND-018, ADR-0060): `Tools/voice/cast/cards/*.flac` and `Prototype/play/audio/voices/cast_*/*.ogg`, made by `Tools/voice/cast/make_cast_voices.py` |
| Model files | `Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign` and `Qwen/Qwen3-TTS-12Hz-1.7B-Base` from Hugging Face (downloaded 2026-11-25 and 2026-11-26; `model.safetensors` and `speech_tokenizer/model.safetensors`) |
| Model licence | Apache-2.0 (both model cards; Alibaba Qwen team) |
| Runtime used to make the audio | `qwen-tts` 0.1.1 (Apache-2.0), `torch` 2.15 CPU (BSD-3-Clause), `transformers` 4.57.3 (Apache-2.0), `soundfile` 0.14 (BSD-3-Clause) |
| Shipped in the game | Only the generated audio files. No model or library code is in the repository or the game. |
| Voices | Every voice is made from a written description; no recording of a real person is used as a reference, and no real person's voice is imitated. |

The models and tools live outside the repository (on the owner's PC in `C:\Tools\voice\qwen3\`). To set them up again
see `Tools/voice/cast/README.md`.
