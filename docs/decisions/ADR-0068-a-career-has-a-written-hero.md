---
adr: ADR-0068
title: A career's hero is a written character, called by a surname the career gives
status: accepted          # the owner in chat, 2026-11-27 (words quoted below)
date: 2026-11-27
relates_to: [QST-014, QST-015, QST-016, SND-018, CHR-001, ADR-0056, ADR-0060, ADR-0063]
---

## Context
Story lines that say the player's typed name (`{player}`) cannot be voiced whole: the game cuts the line at the blank
and joins pieces, and a name the model never heard cannot be spoken at all. Four lines of the Baker career have one,
and three of Noor's are murmured for it (ADR-0063).

The owner, 2026-11-27: "i think we should have set last name for each career. first name is player choice, but NPC call
them with their last name. in case of relationships we can use honey, sweetheart etc. which are generic." Then:
"career story is private copy. player plays as a specific character in the quest mode, so they won't be able to
switching careers, but they will have multiple characters that they build and play separately. they can play other
careers by starting new character with the quest line." And: "I was thinking that for a given career the gender is
fixed." Asked for the Baker's hero: "Baker male, fixed age 24. you come up with a surname options", then "Ashby, British
accent".

## Decision
1. **A career's story is played by its own hero.** A player plays one career per character; another career is a new
   character. Each player's career story is a private copy, so no two players share its people.
2. **The career fixes the hero's gender, age, accent and surname.** The player chooses the first name and the looks
   (CHR-001) within that. The hero has a cast profile like the cast (`"heroes"` in `Tools/voice/cast/cast_voices.json`).
3. **The Baker's hero: Ashby, a man of 24 with a British accent.** The surname was chosen from options a session
   proposed (Ashby, Marlow, Calder, Penrose, Hale, Fenn).
4. **The cast call the hero by the surname, never by the typed first name**, so every line is voiced whole. The typed
   name may still be shown in the menus and the save.
5. **Endearments (honey, sweetheart) only from the person being courted**, once the courting has come that far. They
   are written into her lines like any other words, not filled in as blanks.

## Consequences
- The four Baker lines with `{player}` (Noor's `morning/together2`, `morning/w_join`, `door/y_table3`; `farm/f_saved2`)
  become "Ashby" in the writing (`docs/narrative/src`), after the owner has read them; then they can be voiced whole.
- No Baker line calls the player a man or a woman today, so a male hero changes no other words.
- The player's voice (`World/rules/voices.json` `"player"`, a Kokoro voice, default `woman`, US English) is wrong for
  Ashby: he needs a voice card made by the cast recipe and approved by ear (ADR-0060).
- The character designer (CHR-001) lets any body be chosen; for a career's hero it must hold to the career's gender and
  age. Not built.
- The starter quests (`World/rules/quests.json`) keep `{player}`: they are not a career's story.

## Not decided
- Mei's endearments: which, and from which stage of the courting. Writing, for the owner to read first.
- The heroes of other careers. Proposed by the session, not decided: spread heroes between men and women, vary the
  courtships, and show a career's hero and courtship before a character is made for it.
