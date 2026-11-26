# The Quest Atlas: careers' stories, their people and what is left to do

This folder holds the writing of the game's story quest lines and the page the owner reads them on. The first career,
the Baker (five quest lines), and courting Mei (three lines and a wedding) are written, approved by the owner on
2026-11-25 and running in the game's rules. Design: `docs/decisions/ADR-0056-story-lines-of-a-career.md`; the looks:
`docs/decisions/ADR-0059-old-age-noor-and-the-stone-oven-inside.md`. Requirements QST-010 to QST-015.

## What is here
| Path | What |
|---|---|
| `src/q1.json` to `q5.json` | The Baker career, one file per quest line: The Four O'Clock Door, Feeding the Mother, Stone Ground, Market Day, The Name on the Door. |
| `src/r_m1.json` to `r_m3.json`, `r_w.json` | Courting Mei: Twenty Minutes, The End of the Line, A Door of Our Own, and the wedding. |
| `src/live.json` | A word-for-word copy of the four starter quest lines of `World/rules/quests.json`, so the page shows them too. Keep the two the same. |
| `src/cast.json` | The people: who they are, what they want and hide, how they are tied to each other. Noor's has `gender` and `look`. |
| `src/world.json` | The career's pitch, lore, threads that run between quests, the owner's decisions, six careers sketched for later, the end game sketched, and `exclusive` (facts of which one at most is true). |
| `src/head.html`, `app.js`, `app3.js`, `app2.js` | The page's own code: dialogue trees, quest map, play mode, cast, connections, consequences. |
| `build.py` | Checks every branch, plays the career through at random 6,000 times, and writes `quest_atlas.html`. `python build.py`; `--check` only checks; `--fragment <path>` writes the copy that is published as an artifact. |
| `quest_atlas.html` | The page. Open it in a browser. Published for the owner at https://claude.ai/artifact/7EFC41Y9Hioc9siW6PDHn5 (republish with `--fragment` and that address). |
| `AUDIT.md` | Four audits by a second agent, what they found and what was done. |
| `shots/` | Two pictures from the running Unreal game: the career starting at Noor's counter, the count scene on Market Square. |
| `voices/` | A first page of Kokoro voice picks for the cast, made by another session on 2026-11-25 (`cast_voices.html`, its sounds and the script that made them). The owner did not accept this approach ("Kokoro voices miss age, personality, tone and context"); kept so the work is not lost. The newer voice work is pull request 121 (`work/2026-11-25-voices-by-person`), open and not merged. |

## How the writing reaches the game
The game's quest data is made from these files, never typed. To change a line:
1. change it in `src/`, run `python build.py`, and show the owner the page (writing is reviewed as a page first);
2. in `Tools/citysim/` run `python story.py`, which writes `World/rules/quests_story.json` from `src/` and
   `World/rules/story_cast.json` (who plays whom, where the scenes are); a test fails when they differ;
3. in `Tools/worldgen/` run `python build_play.py` (the slice's data and `Game/Data/life.json`, with the recorded game
   Unreal must reproduce) and, when anybody's home, work or name changed, `python unreal_test_values.py`;
4. run `python -m unittest test_economy` in `Tools/citysim` and the Unreal tests (`Game/README.md`).

A new career is a new set of files in `src/`, listed in `story_cast.json` `lines`, its people cast as residents. The rules
should need no change.

## The owner's decisions (do not reopen without the owner)
- Only Mei can be courted. Noor is 58 and cannot ("leave noor old and remove her from dating option").
- No seasons: no quest is tied to a time of year; the fair is the Town Fair ("drop the seasons").
- The story is each player's own, unseen by other players in the same city ("each player's own").
- Jai works at the museum: the quests mean r1714, the Jai who is a museum guide.
- The town is Old Town, not Linden. Linden stays the name of the street and the bank.
- The Baker career and the courting are built; the end game after the wedding was not asked for.
- Baker Grocer on Baker Street is The Stone Oven Bakery ("Yes, make the bakery").
- Noor's look, the elders and the Stone Oven's inside are approved as Blender looks (ADR-0059).
- Genders the owner stated on 2026-11-26: Hari is a man, Abe a woman, Jai a man, Olu a woman, Eli a woman; Pip is a young
  woman, written as "they". Every profile must state gender, age and accent before a voice or a look is made.

## Not yet
Each item is also noted where it lives (`Game/README.md`, `Tools/README.md`, `Tools/citysim/README.md`).

**So that what was approved shows in the game**
1. The Stone Oven's inside in Unreal is still fitted as a grocer's (shelves and tills). The approved inside is
   `Art/Reference/bakery_inside_day.png` and `bakery_inside_four.png`, made by `Tools/blender/baker_look_47.py`.
2. Unreal's people (and the slice's) have no age values (lines, stoop, glasses, cane, shawl, beard, thin hair), still have
   the shell hair that reads as a cap with pale hair, and cannot give one named resident a look of her own: Noor wears
   her job's stock clothes there. The approved look is `Art/Reference/character_noor.png` and `character_elders.png`
   (`Tools/blender/elders_48.py`).
3. Pip, Tam, Vera, Oda, Suvi and Len have no designed look. Vera, Oda, Suvi, Len and Tam have no gender written down,
   and five of the cast have no age decided: ask the owner.

**Left from building the Baker career**
4. The cast sheet and the quest lines write Abe and Eli as "he"; the owner says both are women. The text is not
   corrected. Correct it in `src/` (and in `World/rules/quests.json` and `live.json` where the starter lines say it), then
   follow "How the writing reaches the game".
5. The story has been played in the running game only as far as the two pictures in `shots/`; the rest by test (1,500
   random games in Python; a recorded 456-step game in Unreal).
6. In a scene the camera comes in too close behind the player's shoulder.
7. None of the story is voiced. A line said by a second person in a talk is murmured in the voice of the person spoken
   to. Voices are separate work (pull request 121).
8. The map has no mill house or Brook Farm kitchen: Oda, Suvi and Len are residents renamed where they live. The bakery
   has four staff, two more than the story's Noor and Pip, because the staffing rule gives a shop of its size four posts.
   A talk happens wherever the person's day has put them, not where the scene is set.
9. How fast regard with Mei rises against the gates of 25, 45 and 75 is not balanced; nor is any pay in the career.
10. The browser slice carries the four starter lines only; it has no port of the story rules.
11. The quest log does not show a career's rank.

**Not started**
12. The next career. Six are sketched in `src/world.json` (`careers_ahead`); each needs the owner's go-ahead, and its
    writing is reviewed on the page before any game data.
13. The end game after the wedding (a household, children, a fortune, what is left to them): a sketch in `src/world.json`
    (`endgame`).

**Weak points of the approved looks** (in `Tools/README.md` too): the old man's cap is the courier's and reads as a
helmet; the shawl reads as a collar; the bakery has no shop front or street door, its fire reads a little like candle
flames, its far loaves are small and alike, and its floor is boards, not flags. `Tools/blender/look_check.py` measures
heads only, from the front, and cannot tell a cap from cap-like hair or that hair is one smooth lump.
