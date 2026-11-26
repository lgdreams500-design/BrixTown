---
adr: ADR-0063
title: Noor's lines voiced by Maya1, in part, alongside the cast recipe
status: accepted        # the owner, 2026-11-26: "actually, stop generating more voice and start integrating what we have so far in the game."
date: 2026-11-26
amends: ADR-0060
relates_to: [SND-018, QST-012, ADR-0019, ADR-0060]
---

## Context
ADR-0060 voices named characters with Qwen3-TTS: one approved card per person, every line spoken in its voice. On
2026-11-26 the owner tried two other local models through LM Studio (outside the repository, `C:\Projects\voice_envs`):

| Tried | The owner's verdict |
|---|---|
| Orpheus 3B (eight fixed American voices, emotion tags) | Loved the first test; no British voice |
| Orpheus pretrained, cloning a person's approved card | "this model is bad, get rid of it" |
| Maya1 (a voice made from a short written description, emotion tags) | "Maya sounds better and all voice sound close" |
| Maya1 given Noor's long profile description | The description itself was spoken aloud on a short line |
| Maya1 with "unhurried pacing" in the description | Too slow (15.1 s for 26 words); "pauses are messing the pacing. remove those additional instructions" |
| Maya1 with a short description, no pace words | "sound correct pacing" |

Noor's run with that description was stopped by the owner after 35 of her 111 lines, to put what was made into the game.

## Decision
1. **Noor speaks in Maya1's voice** where a Maya1 clip of her line exists. Her description, approved by the owner
   ("Yes, use it"), is: *Female voice in her late 50s with a British accent. Low pitch, dry and slightly worn timbre,
   firm but quiet tone.* Maya1 descriptions carry no pacing or pause words.
2. **Clips go into the game as the model spoke them.** `Tools/voice/cast/import_maya_clips.py` only re-encodes each
   WAV to Ogg Vorbis, under the same key as the cast recipe (`cast_noor|<mood>|<words>`); no gain, trim or timing is
   changed. Her approved Qwen card stays her card on record.
3. **A part may be in the game in part.** Her entry in `World/rules/voices.json` says `"model": "maya1"` and
   `"partial"`: lines with no clip are murmured, as any unvoiced line is (`SND018_unvoiced_lines_are_murmured`), and
   `SND018_the_cast_speak_their_story_lines` checks that every line she has a voice for finds its clips.
4. Of the 35 clips made, **32 are in**. Left out, kept outside the repository: `four_oclock_door/morning/report`
   (18.3 s for 5 words: likely the description spoken) and `four_oclock_door/verdict/v_lie` (73 s, ran to the token
   limit) as faults; `four_oclock_door/morning/together2` because it has the player's name inside it, and the game
   splits a line at its blank.

## Bent rules, said plainly
- The emotion tags in Noor's Maya1 text (`<sigh>`, `<chuckle>`, ...) were placed by hand per line, at the owner's
  request ("hand-placed, sparingly based on the mood and context in the quest line"). ADR-0060 forbids a direction for a
  single line. Two of the 32 clips in the game were spoken with one: `<sigh>` in `four_oclock_door/verdict/v_count` and
  `feeding_the_mother/still/s1`.
- Maya1 does not repeat a take for a seed through LM Studio: a kept clip is the only exact copy.

## Not decided
- Maya1 for the rest of Noor's part (76 lines), for the other twelve of the cast, or in place of Qwen3-TTS.
- Whether the slower clips are right (1.2 to 1.5 words a second against her card's 2.8: `card/n_again`, `card/lastone`,
  `morning/teach`, `verdict/v_wary`, `still/s1`): no agent can hear; the owner's ear decides.
