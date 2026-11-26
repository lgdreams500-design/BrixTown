---
adr: ADR-0060
title: Named characters have voices of their own, made by one locked recipe; the voice index names residents by id
status: accepted        # the owner, 2026-11-26: "Let's finalize this process for voice generation and add this to the repo instructions along with Oda voice lines as I approve them"
date: 2026-11-26
amends: ADR-0019
relates_to: [SND-018, CITY-007, QST-012, ADR-0019, ADR-0008, ADR-0056]
---

## Context
ADR-0019 gives every resident a Kokoro voice from their role's pool, by a hash of their id. That is enough for a town
of passers-by. It is not enough for the people of a story. The owner listened to Kokoro voices for the Baker cast and
said (2026-11-25): "voices don't match the age, personality or in what tone it is being said. or what the context is".
Kokoro has 28 fixed voices, none old, and takes no direction; age was faked by lowering pitch and mood added afterwards
as sound processing.

Two more things had gone wrong since ADR-0019 because the town grew from 1,134 residents to 3,984: the roles `creative`
and `binman` had no pool, so the generator stopped; and the index listed voices by a resident's place in the list, so
277 residents had a voice that was no longer the rule's and 2,850 had none.

A trial with the owner over two days (Qwen3-TTS, a model that makes a voice from a description) found, by the owner's
ear, what works and what does not:

| Tried | The owner's verdict |
|---|---|
| A direction written for every line, the voice made afresh each time | Better than Kokoro, but one person's lines "all sound different persons" |
| Trying several readings of a line and keeping the best | "you need the instructions to the voice model locked. pick from multiple won't work, i can approve one or to not every other dialog" |
| Shortening and padding pauses after the model had spoken | Holes inside words; "it's stupid ... to make post processing of the voiced lines" |
| A description that left gender open ("androgynous") | A young woman came out as a man: "you have not put the gender for the cast?" |
| One card per person, every line spoken in its voice, in one run, clips left as spoken | "Yes, the sound much better. Let's finalize this process for voice generation" |

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Stay with Kokoro pools, tuned | Nothing new | The owner's verdict above |
| A direction per line (VoiceDesign alone) | Most acting | No fixed voice; needs a human to pick readings; cannot be done for hundreds of lines |
| **Voice card and mood cards (VoiceDesign once, then Base)** | One voice per person; nothing chosen per line; the owner approves one clip and one part | Mood comes only from a short mood clip; a minute a line on this PC; a second 4.5 GB model |
| A paid voice service | Probably better acting | Cost, licence terms, and the lines leave the PC |

## Decision
- **People before pools.** `World/rules/voices.json` has a list `people`, by resident id, used before the role's pool.
  An entry is either a Kokoro voice (`voice`, `lang`, and if wanted `shift` and `speed`; `voice` may be two mixed
  evenly) or a cast voice (`"cast": <id>`). Each entry carries the resident's `name`, and the tool stops if the id is
  not a resident or the name is not theirs. Only the owner fills the list.
- **Named characters are voiced by the cast recipe**, with Qwen3-TTS 1.7B (Apache-2.0), in `Tools/voice/cast/`:
  1. a profile that states gender, age and accent, with a description that opens with them;
  2. one voice card made from the description, which the owner approves by ear and which is then kept as a file;
  3. a card for each mood, spoken by the Base model in the voice of the voice card, kept too;
  4. every line of the person's part spoken in the voice of the card of its mood tag, all in one run, one seed.
- **Locked.** No direction for a single line, no choosing between readings, no editing of a clip after the model has
  spoken. The owner approves a person's card and hears their part once; a wrong line is voiced again as it is.
- **The rest of the town stays on ADR-0019**: Kokoro, pools, moods by processing. A role with no pool is not voiced
  (wordless syllables) and is listed in `roles_not_voiced` until the owner picks a pool; the tool no longer stops.
- **The index is by resident id.** `voices.js` looks a voice up by id; `Tools/unreal/people_values.py` makes the list
  by place that the Unreal build reads. `make_dialogue_voices.py --index` writes the index again with no model. The
  Kokoro tool makes nothing for a cast voice and keeps its clips.
- **Oda** (r3419) is the first: her seventeen lines are in the game, approved by the owner on 2026-11-26.

## Consequences
- Twelve more parts are to be voiced (about six hours of machine time), each to be heard once by the owner.
- A resident with a cast voice speaks only what has been voiced for them: their everyday talk is wordless until voiced.
- The voice cards are binary files in the repository (about 0.3 MB each): they are the voice, and cannot be made again
  exactly on another PC or another version of the model.
- Acting is limited to six moods. A line that needs something a mood tag cannot say (a whisper, sarcasm) will not get
  it. If that proves too little, the recipe changes for everyone, not for a line.
- Pauses are the model's own and uneven. If that proves a problem, the lever is the card the model copies.
- 277 residents changed voice back to what the rule gives them when the index went from place to id.
- The Unreal reader (`BrixTalk`) still takes the list by place; it should take ids, so it cannot go stale between runs.
