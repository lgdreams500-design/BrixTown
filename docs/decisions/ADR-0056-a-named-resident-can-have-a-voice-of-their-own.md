---
adr: ADR-0056
title: A named resident can have a voice of their own; the voice index names residents by id
status: proposed        # the owner is asked on the pull request; no voice is chosen here
date: 2026-11-25
amends: ADR-0019
relates_to: [SND-018, CITY-007, ADR-0019, ADR-0008]
---

## Context
ADR-0019 gives every resident a voice from their role's pool, by a fixed hash of their id. Nothing else decides it: not
who the person is, and not whether they are a man or a woman. For a town of passers-by that is enough. For a person the
player comes to know it is not: in the slice today Mei (r0504) speaks in a man's voice, and Abe, Jai and Olu in women's.
The owner is choosing voices for the thirteen named people of the proposed Baker career from a listening page, and those
choices cannot be used while a person cannot be given a voice.

Two more things had gone wrong since ADR-0019, both because the town grew from 1,134 residents to 3,984:
- the roles `creative` and `binman` were added without a pool, so the generator stopped with an error and its tests failed;
- the index of voiced pieces listed each resident's voice by their place in the list, so after the town was dealt again
  the slice and the Unreal build gave 277 of the first 1,134 residents a voice that was no longer the rule's, and the
  other 2,850 none.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Pools only, add a gender to each voice and each resident | Fixes the mismatch for the whole town at once | Residents have no gender in the data; still cannot say "this person, this voice"; a larger change the owner has not asked for |
| A pool of one for each named person (a role of their own) | No new rule | A role is how someone spends their day; a role per person breaks that |
| **A list of people in `voices.json`**, used before the pool | Small; the pick sits beside the pools; everyone else is unchanged | Each entry is a pick the owner must make; a person's lines are voiced again in their voice |

For voices not in the model's set of 28: kokoro-onnx accepts a style made by averaging two of its voices, which gives a
voice between them. Allowing an entry to name two voices costs a few lines and no new files.

## Decision
- **People before pools.** `World/rules/voices.json` gets a list `people`: resident id to a voice (`voice`, `lang`, and
  if wanted `shift` and `speed`, as in a pool). A resident in the list speaks in that voice; everyone else takes one from
  their role's pool exactly as in ADR-0019. Each entry also carries the resident's `name`, and the tool stops if the id
  is not a resident or the name is not theirs, so that dealing the town again cannot hand the voice to someone else.
- **Two voices mixed.** In the people list `voice` may be two of the model's voices in a list. They are mixed evenly;
  `lang` is the accent of one of them. Pools stay single voices.
- **Only the owner fills the list**, by saying so in chat (as ADR-0008 for levels). It starts empty. Whether men and women
  should be told apart for the rest of the town is not decided here.
- **A role with no pool is not voiced.** Its residents' lines stay wordless syllables, as any line with no voiced piece
  does, and the tool runs. Such roles are listed in `roles_not_voiced` (today `creative` and `binman`) so that a role
  cannot go without a pool unseen; the owner picks their pools. A named resident of such a role is voiced all the same.
- **The index is by resident id.** `Prototype/play/audio/voices/index.js` gives each voiced resident's voice by their id,
  and `voices.js` looks it up by id. The Unreal build still reads a list by place; `Tools/unreal/people_values.py` makes
  that list from the ids and the residents as they are when it runs. `make_dialogue_voices.py --index` writes the index
  again without voicing anything, for when only who-has-which-voice has changed.

## Consequences
- A pick takes one line of data, then a run of the generator for that person's lines (their own name, role and workplace,
  and every fixed piece of the talks and quests they can have): some minutes each, not the hour of a full run. Until it
  is run, the person speaks only the pieces that already exist in that voice.
- The index grows by about 45 KB (one entry per voiced resident).
- 277 residents change voice in the slice and in Unreal with this change, back to what the rule gives them; the 2,850
  added since the index was made get the voice the rule gives them where pieces exist in it.
- Voicing the Baker quests is not covered: their drafts name a speaker on every line, several to a stage, and lines of
  narration, which the generator cannot read yet.
- The Unreal reader (`BrixTalk`) should take the index by id too, so that it cannot go stale between runs of
  `people_values.py`.
