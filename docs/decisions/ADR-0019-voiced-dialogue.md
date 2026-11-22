---
adr: ADR-0019
title: Residents speak their lines, in pre-made natural voices
status: accepted          # the owner decided in chat on 2026-11-22: "yes residents should actually speak in the game"
date: 2026-11-22
relates_to: [Q-14, CITY-007, CHR-009, SND-005, SND-018, ADR-0008]
---

## Context
Q-14 was decided "text first": lines were shown as text, mouths were timed from the text, and SND-005 voiced each line
as wordless syllables. The owner listened to five code-made voice styles and rejected them all as robotic. Natural
voices made on the owner's PC with the Kokoro voice model (Apache-2.0, `docs/licenses/kokoro-tts.md`) were accepted on a
sample page, a voice was picked for five kinds of resident, and the voices were given moods. The owner then decided that
residents should actually speak in the game.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Stay text first | Nothing to make | Owner wants speech |
| **Pre-made audio**: every line generated offline, shipped as small files | Plays at once; same voice everywhere; no model in the game | Blanks in lines must be joined from pieces; a typed player name cannot be pre-made |
| Voices made live in the browser | Speaks anything exactly | 90 MB model in the project, 1-3 s before each line, needs the server |

The owner chose pre-made audio, and a few voices per role so the town is not one voice per job.

## Decision
- **Q-14 is now: voiced.** Residents and the player speak their dialogue lines in real words. The text stays on screen.
- **Voice cast as data.** `World/rules/voices.json` gives each role a small pool of voices, starting from the owner's
  picks (café worker `af_bella`, child `af_nova` raised, nurse `bm_george`, elder `af_sarah` lowered, commuter
  `bm_lewis`). Each resident takes a voice from their role's pool by a fixed hash, so they always sound the same. The
  player speaks in one of two voices; the default is in the same file until the character designer offers a choice.
- **Moods** shape each line by its mood tag (pitch, pace, tone, loudness, punctuation; never the words), at the level
  the owner chose.
- **Pre-made, in pieces.** `Tools/voice/make_dialogue_voices.py` voices every line of `dialogue.json` and `quests.json`
  for every voice that can say it. A line with blanks is cut at the blanks: the fixed pieces are voiced once per voice,
  and each blank is voiced for every value it can take for that speaker (greeting, season, weekday, the speaker's own
  name, role and workplace, what they are doing). The player's typed name is voiced only if it is the default; any other
  name is a short pause while the caption shows it. The audio is written as OGG with an index the slice reads.
- **Lip sync follows the audio.** The mouth shapes still come from the line's text (CHR-009), stretched to the length of
  the spoken audio so they finish together.
- **SND-005 is cut**: wordless line voices are replaced by speech (SND-018). Residents chatting among themselves
  (SND-016) stay wordless murmur.

## Consequences
- Every change to a line, a voice or a pool needs the generator run again (seconds per line on the owner's PC); the model
  stays outside the repository.
- Joins at blanks can sound slightly stitched. Lines can be rewritten to keep blanks at the start or end.
- Audio adds a few tens of MB to the slice. OGG files are binary and cannot be merged.
- The Unreal build will need the same cast and lines; the index format is kept simple so it can be imported.
