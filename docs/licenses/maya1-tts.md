# Maya1 voice model (Noor's voice, ADR-0063)

| | |
|---|---|
| What | Maya1 (Maya Research), a 3B text-to-speech model that makes a voice from a short written description, run offline on the owner's PC in LM Studio; its audio tokens decoded with SNAC 24 kHz |
| Used for | Noor's lines: `Prototype/play/audio/voices/cast_noor/*.ogg`, brought in by `Tools/voice/cast/import_maya_clips.py` |
| Model files | `mradermacher/maya1-GGUF`, `maya1.Q4_K_M.gguf` (a quantised copy of `maya-research/maya1`), downloaded 2026-11-26; `hubertsiuzdak/snac_24khz` |
| Model licence | Apache-2.0 (Maya1 model card); SNAC: MIT |
| Runtime used to make the audio | LM Studio (local server), `snac`, `torch` CPU (BSD-3-Clause), `soundfile` (BSD-3-Clause) |
| Shipped in the game | Only the generated audio files. No model or library code is in the repository or the game. |
| Voices | Made from a written description; no recording of a real person is used as a reference. |

The model, the scripts that call it and the clips as made live outside the repository, in `C:\Projects\voice_envs\`.
