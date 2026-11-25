---
adr: ADR-0056
title: A career's story lines run on the quest rules, from the writing the owner approved
status: accepted        # the owner approved the writing and asked for the build on 2026-11-25; numbers stay [ASSUMPTION]
date: 2026-11-25
relates_to: [QST-001, QST-003, QST-004, QST-007, QST-008, QST-010, QST-011, QST-012, QST-013, QST-014, QST-015, CITY-007, CAR-002, PROP-009, ADR-0012, ADR-0008]
---

## Context
The Quest Atlas (`docs/narrative/`) holds the Baker career (five quest lines) and courting Mei (three, and a wedding): 372
lines said, 609 replies, 32 endings. The owner read it as a page, had it audited four times, and on 2026-11-25 said
"Approved", then "once the audit is clean, start implementing the quest in the unreal" and, asked how much of it,
"Include courting". Earlier that day: "drop the seasons", the story is "each player's own", "Yes, museum" (Jai works at the
museum) and "Old Town".

The quest rules of the first slice (ADR-0012, `Tools/citysim/quests.py`, ported to the slice and to Unreal) run four short
starter lines. The writing needs more than they have: it remembers what the player did and asks about it three quests
later, several people speak in one talk, some lines are told rather than said, a talk opens differently for a player it
has met before, and a line that ended badly can be taken again.

## Options considered
| Option | For | Against |
|---|---|---|
| A second engine for story lines beside the quest rules | Nothing built is touched | Two logs, two saves, two sets of rewards, two ports to keep the same |
| **Extend the one set of quest rules, and make the game's data from the writing** | One engine, one save, rewards through the same ledger; the words exist once, where the owner reads them | The Python, the slice and Unreal must all learn the new rules |
| Retype the writing into `World/rules/quests.json` by hand | No new tool | 372 lines copied by hand drift from what was approved |

## Decision
- **One engine.** `quests.py` runs the story lines with the starter lines. What is new (each proved by a test named after
  its requirement in `test_economy.py`):
  - **Facts** (QST-010): a reply's act `flag:x` or an ending's `flags` leaves a named fact with the player; `unflag:x` takes
    it away. A need may ask `flag:x`, `have:item`, `money:n`, `rep:<resident>:n`, `done:<quest>:<ending|ending|*>`,
    `home:owned` or `again:<quest>`, each with `!` in front for "not". Facts are the player's own and part of the save.
    `exclusive` lists groups of facts of which one at most is ever true.
  - **How a line opens** (QST-011): a story line has no offer talk. It is on offer when its `need` holds, and starts when
    the player speaks to its giver, with the first stage's talk. A stage's `starts` names other first lines, the first
    whose needs hold.
  - **Scenes and speakers** (QST-012): a node's `who` names another resident who says it, or `scene` for a line nobody says;
    `dir` is what is seen, never spoken. A stage with lines and a place instead of a person is a scene: it plays where the
    player is, and moves on only by what the player chooses.
  - **Taking a line again** (QST-013): an ending with `retake` leaves the line on offer. Taken again it starts clean: the
    facts and things its own replies gave are cleared and the regard they gave is taken back; what the ending cost stays.
  - **Money in a reply**: `pay:n` from `sys:rewards` and `charge:n` to `sys:residents`, through the ledger (QST-004); a
    reply that costs money must need that money, and is refused whole if it cannot be paid.
  - **Never stuck**: a node where every reply has a condition passes the check only if one is open in every case the
    data allows, or it is named in `story_cast.json` `covered` with the reason, and the play-through test proves it.
- **The data is made from the writing.** `Tools/citysim/story.py` reads `docs/narrative/src` (the files the Atlas page is
  built from) and `World/rules/story_cast.json` (who plays whom, where the scenes are) and writes
  `World/rules/quests_story.json`, which `world.load()` joins to `quests.json`. A test fails when that file is not what
  the writing gives. To change a line: change the writing, show the owner the page, run `python story.py`.
- **The cast are residents who already live in town**, given the story's names; nobody's home, work or post changes.
  Noor (r0262) and Pip (r0263) work at the shop on Baker Street in Old Town, Vera (r1546) lives next door, Tam (r1046)
  keeps the Crumb outlet in Eastgate Mall. Jai is the Jai who works at the museum (r1714): the starter line Errand runner
  now sends the parcel to him.
- **Courting** (QST-015) is three lines and a wedding with Mei, gated on her regard (25, 45, 75, `[ASSUMPTION]`); she says
  yes only to a home owned and 150 Bx put by. The wedding ends the line. What comes after (a household, children, a
  fortune) is a sketch in the Atlas and is not built.
- **Order of the build**: the rules and the data in Python first (this decision), then Unreal's `BrixQuests` and
  `ABrixTalk` to the same answers, checked against a fixed run as the starter lines are.

## Consequences
- The Baker career and the courting are played to every one of their 32 endings by test, 1,500 random games, through the
  real ledger; no game gets stuck and no two facts of an exclusive group are ever both true.
- Until Unreal's port is built, `Game/Data/life.json` and the browser slice carry the starter lines only.
- The map is not changed here. The Stone Oven is still called Baker Grocer and sells groceries; Oda, Suvi and Len live in
  ordinary houses because the mill house and Brook Farm's kitchen are not built; a talk happens wherever the person's
  day has put them, not where the scene is set. Making the shop a bakery renumbers the posts of 446 residents
  (`careers.assign()`), so it is a change of its own, with the owner's word.
- Regard with Mei has to reach 75 by shifts, rounds and endings; whether that is too slow or too fast is not balanced yet.
- Whether the player owns a home is asked of whoever holds the property market (`Quests.owns_home`); in Python it is set
  by the test, in Unreal it will be the estate (PROP-009).
