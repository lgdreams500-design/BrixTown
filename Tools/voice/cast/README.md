# Tools/voice/cast

Voices for named characters (SND-018, ADR-0060): the people of a career's story, each in a voice of their own that fits
their gender, age and manner. Made offline with Qwen3-TTS (`docs/licenses/qwen3-tts.md`). The two model folders and
their Python packages are not in the repository.

| File | What it is |
|---|---|
| `cast_voices.json` | The recipe (the same for everyone) and each person's profile: resident, gender (`voice_is`), age, accent, how they talk, the description given to the model, their voice card and the owner's approval of it |
| `cards/<person>.flac` | The voice card: the one clip the owner approved as that person's voice. Kept; never made again |
| `cards/<person>_<mood>.flac` | The person's card for a mood, spoken in the voice of the voice card. Kept once made |
| `import_maya_clips.py` | Brings a person's Maya1 clips (made outside the repository) into the same folder and index, only re-encoded (ADR-0063) |
| `make_cast_voices.py` | Voices every line a person speaks in `World/rules/quests_story.json`, in one run, into `Prototype/play/audio/voices/cast_<person>/` and the index |

## The process, in order
1. **Profile first.** Before any voice is made, the person's profile in `cast_voices.json` states `voice_is` (woman or
   man), `age` and `accent`, and the `description` opens with the gender and the age. If the writing does not say, ask
   the owner. Never leave it open ("androgynous", no age): the model settles what is left open by chance, and a young
   woman once came out as a man. The tool refuses a profile that does not state them.
2. **One voice card.** The description and the recipe's one neutral sentence make `cards/<person>.flac`. The owner
   listens to that one clip and approves it or asks for a change (then change the description, delete the card, make
   it again). Record the owner's words in `card_approved`. Only the owner approves a card.
3. **The whole part in one run.** `make_cast_voices.py <person> <design model> <base model>` makes the person's mood
   cards if they are missing, then every line they speak, each in the voice of the card of the line's mood tag, one
   after another with the recipe's one seed.
4. **The owner hears the part once.** `make_cast_voices.py <person> --page <file>.html` writes a page of every line in
   story order (keep the page out of the repository). The owner approves the part as a whole, or names the lines that
   are wrong.
5. **Into the game.** Give the resident the cast voice in `World/rules/voices.json`:
   `"people": { "<resident id>": { "name": "<their name>", "cast": "<person>" } }`, then
   ```
   python ..\make_dialogue_voices.py --index
   python -m unittest test_voices            (in Tools/voice)
   python ..\..\unreal\people_values.py      (the Unreal build's copy, Game/Data/talk.json)
   ```
   Then build and run the Unreal test `BrixTown.World.SND018_the_cast_speak_their_story_lines`: every line of every
   cast-voiced person must find its clips. To see a scene without anyone at the keys, stage it (the save is not touched):
   `-BrixNoSave -BrixLife=act,<quest>,<giver id>,start|stage:<stage> -BrixTalk=<the resident's place in the list> -BrixAnswers=1,1,...`;
   the log says `<resident> says "..." aloud` for each line the game speaks.

## Rules (the owner's, 2026-11-26)
- **The instructions are locked.** No direction written for a single line, no choosing between several readings of a
  line, no seed tried until one sounds right. A whole cast has hundreds of lines; the owner approves a card and a part,
  not every line. To change how someone sounds, change their description or the recipe, for everyone it applies to.
- **Never edit a voiced clip.** No pause added, cut or moved, no trimming, no re-timing, no pitch or pace change after
  the model has spoken. A step that set pauses at commas put holes inside words. If the pacing is wrong, change what the
  model is given (the card it copies), not the audio.
- **A character's lines are voiced together**, in one run from the same cards.
- **A line that is wrong is voiced again as it is** (same words, same mood), by deleting its clip and running the tool.
  Changing the writing or the mood tag is a change to the story, shown to the owner first.
- **No agent can hear.** Pitch and length can be measured and reported; whether a voice is right is the owner's ear.
  A speaker-likeness score from the model's own embedding was tried and is useless (0.94 to 0.99 for everything).

## Setting up (once)
```
python -m venv env
env\Scripts\python -m pip install torch torchaudio --index-url https://download.pytorch.org/whl/cpu
env\Scripts\python -m pip install qwen-tts huggingface_hub soundfile
```
Download `Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign` and `Qwen/Qwen3-TTS-12Hz-1.7B-Base` from Hugging Face (4.5 GB each,
`huggingface_hub.snapshot_download`; the connection may drop, it resumes). On the owner's PC both and the environment
are in `C:\Tools\voice\qwen3\`. With no graphics card a line takes about a minute: Noor's part (111 lines) is about two
hours, the whole Baker cast about six.

## Where the cast stands
All thirteen people of the Baker career have a profile and an approved voice card. Voiced and in the game: **Oda**
(17 lines, approved by the owner on 2026-11-26). Voiced by the recipe on 2026-11-26, kept in the repository, **not heard
by the owner and not in the game** (no `cast` in `World/rules/voices.json`): Abe (1 line), Len (4), Olu (7), Eli (9) and
Suvi (10), with their mood cards. The run was then stopped at the owner's word; Jai's part was left at 7 of 19 clips,
which are not committed.

**Noor** speaks in a Maya1 voice (ADR-0063), not the recipe's: 107 of her 111 lines are in the game (`cast_noor`,
`"partial"` in `World/rules/voices.json`), from two Maya1 runs (35 lines, then the other 76 on 2026-11-27). The owner
listened to the twelve clips the length check flagged and found them right (2026-11-27: `verdict/v_lie` "is the only
bad one"), so they went in with `--keep`.

**Every clip is checked by its length** (owner, 2026-11-26: no agent can hear and nobody can listen to thousands of
lines). A clip longer than its words allow (`words / 1.6 + 2 s`) has more in it than the line: the model read its
description aloud, or ran away and never stopped (Noor's `verdict/v_lie`: 73 s of babble for 28 words, the request
was right). Shorter than `words / 5`, words are missing. The numbers are `"length_check"` in `cast_voices.json`;
`import_maya_clips.py` takes no flagged clip into the game (and takes one out if it was in), unless `--keep` names it
after the owner has heard it. The Maya1 voicing script outside the repository flags the same clips as it makes them.
It cannot hear wrong words of the right length; a speech-to-text check would (offered, not wanted for now).

## Not yet
- The Baker's hero, Ashby (`"heroes"` in `cast_voices.json`, ADR-0068), has no voice card: the player still speaks in
  the Kokoro player voice of `World/rules/voices.json` (a woman's, US English by default). One test of a line with
  "Ashby" in Noor's Maya1 voice was made outside the repository (`together2`, 19.9 s for 27 words, a second over the
  length check); it is not in the game.
- Noor: four lines murmured. `four_oclock_door/verdict/v_lie` ran to Maya1's token limit (73 s of babble) on both
  tries, 2026-11-26 and 2026-11-27; its words or another model would need the owner's say. `morning/together2`,
  `morning/w_join` and `door/y_table3` had the player's name inside; since 2026-11-27 they say "Ashby" (ADR-0068) and
  can be voiced whole, but are not voiced yet (the one test of `together2` was made before the writing changed). Nobody has listened to her clips in the Unreal game. Do not run `make_cast_voices.py noor` (the Qwen voice)
  over her part.
- Noor's clips have not been heard in a staged Unreal talk: `-BrixTalk=262` opened Pip's talk at 10:00 (she stands
  nearer) and found nobody near enough at 06:40, 07:21, 12:30 and 16:00. The Unreal test finds every one of her voiced
  lines' clips.
- The length check is not in `make_cast_voices.py` (the Qwen recipe): Oda's and the others' clips were never checked.
- Not voiced: Mei 86, Tam 38, Vera 23, Pip 22, Jai 19 (Hari speaks only in the starter quests). Abe, Len,
  Olu, Eli and Suvi are voiced but wait for the owner's ear (a page each, `--page`) before they go into the game.
- The 25 lines nobody says (`who: scene`) have no narrator's voice.
- A resident with a cast voice speaks only what has been voiced for them: Oda's everyday talk as a passer-by (the
  conversations in `dialogue.json`, which her role's Kokoro voice used to say in part) is wordless until it is voiced
  in her own voice.
- Lines with blanks: only the player's default name is voiced as a blank's value; no story line of the Baker career
  has a blank today.
- The writing (`docs/narrative/src/cast.json`) has no gender field, and writes Abe and Eli as "he"; the owner said
  both are women (2026-11-26). The profiles here are right; the writing is to be corrected.
- The accents (British throughout) were this tool's assumption; the owner approved the cards made with it but was
  never asked about accents as such.
- Nobody has listened to Oda in the Unreal game itself: the staged runs had the sound off.
