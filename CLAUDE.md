# Brix Town — instructions for AI agents

Brix Town: a third-person multiplayer life-sim in Unreal Engine 5.8. Art source in Blender (5.3 installed; pin is Q-16).

## Before any work
1. Read `docs/specs/GAME_SPEC.md`. It is the source of truth for scope.
2. Work only on requirements whose status is `accepted` or `in-progress`, in the current phase.
3. Treat `[ASSUMPTION]` values as placeholders and `[OPEN: Q-nn]` items as undecided: ask, do not decide.
4. Read `docs/process/FEATURE_PROCESS.md` and `docs/backlog/feature_choices.json`. A feature is in scope for a phase only
   at or below the level the owner picked for it; a missing pick means undecided: ask, do not build.
5. For anything visual, read `docs/art/ART_DIRECTION.md` and `docs/process/LOOK_GATE.md`, and look at
   the reference for it in `Art/Reference/catalog.json` first (one entry per thing: its picture, rules and the models made
   from it). For a place in the game, ask which reference it is judged against before anything else:
   `python Tools/reference_catalog.py --for <tour stop, lot id or place>` names the place, its own reference and the file
   for the hour, or says it has none. Never judge a place against another place's reference (a hallway by a shop, a shop
   by the bakery), and when the owner names a kind of place (a room, a hallway, a shop) use that place's.
   **No approved reference, no building it:** make the reference first, at every setting it is seen in and in
   the town's one light, and get the owner's approval on the reference page (FEATURE_PROCESS stage 6, ADR-0066). A room's
   Blender reference says what the room is and its mood; its light is set and approved in Unreal, in the golden room,
   and never tuned by number against a Blender picture or on a room of the town (ADR-0073; LOOK_GATE, A room's light). For faces read
   `docs/art/CHARACTER_FACE.md`; for body animation read `docs/art/CHARACTER_MOTION.md`.

## Rules
- Reference requirement IDs in commits, tests and task notes (e.g. `ECO-002: add ledger transaction type`).
- Never renumber or reuse requirement IDs. To drop one, set `status: cut`.
- Every `P0` requirement needs an automated test named with its ID.
- Gameplay logic in C++ modules; tunable numbers in data, not code.
- Note every known gap in the README of the part it is in, in the same pull request: what is not done yet, a bug
  seen but not fixed, a shortcut taken. For the Unreal build that is the "Not yet" line of its section in
  `Game/README.md`. A chat message or a hand-over note is not enough. Remove the note when the gap is closed.
- Money is integer minor units and moves only through the ledger.
- The server is authoritative; never trust client input.
- Never commit directly to `main`. Commit on a work branch (e.g. `work/YYYY-MM-DD`), push it, and merge into `main` only
  through a pull request. Once its pull request is merged into `main`, delete the work branch on the remote and locally.
- Several sessions may share this checkout and branch: commit only your own changes, and check that each committed file
  matches the working file line for line, so a partial commit cannot reorder lines.
- Do not commit secrets, engine binaries, or `Saved/`, `Intermediate/`, `DerivedDataCache/`, `Binaries/`.
- Binary assets (`.uasset`, `.umap`, `.blend`) cannot be merged: lock before editing.
- Record any significant design choice as an ADR in `docs/decisions/`.
- Anything seen in the Unreal game passes the look gate before its pull request (`docs/process/LOOK_GATE.md`,
  ADR-0058): shots at noon, dusk and night beside the references, the whole frame gone through (G1 to G10),
  `Tools/unreal/look_check.py` run, the sheet kept in `Art/StyleTests/`, every "no" mended or written in the README's
  "Not yet". The bar is `Art/Reference/` and ART_DIRECTION's rules, not the browser slice: the slice was a prototype
  and "as the slice draws it" is never a reason a picture is good enough. Do not write "checked" or "looks right"
  about something seen without having done this.
- The look is locked (ADR-0004). New art must match `Art/Reference/`; never overwrite files there. Changing the
  look needs a new ADR and the owner's approval.
- Every head uses the one face control set and every body the one skeleton (joint names in CHARACTER_MOTION §2).
- Look tests are scripts in `Tools/blender/` that build on each other; keep them together and runnable.
- Only the owner picks levels, by saying them in chat. Record a pick in `docs/backlog/feature_choices.json` only from the
  owner's own words, then run `Tools/backlog.ps1` to check it and rewrite `docs/backlog/BACKLOG.md` (never edit that by
  hand; ADR-0008). Propose features and levels by editing `docs/backlog/features.json` and saying so.
- Voices for named characters are made only by the cast recipe (`Tools/voice/cast/README.md`, ADR-0060): the profile
  states gender, age and accent before any voice is made; the owner approves one voice card for a person, by ear; a
  person's lines are all voiced in one run from their cards. Never write a direction for a single line, never choose
  between readings, and never edit a voiced clip afterwards (no pauses added or cut). No agent can hear: report
  measurements and let the owner judge.
- If something can be calculated from the world clock and data, do not tick it or store it. Prove city rules in
  `Tools/citysim/` with tests before building them in the engine (`docs/specs/domains/LIVING_CITY.md` §1).
- The map is data in `World/` (`docs/specs/domains/WORLD_MAP.md`). Change the city by editing block files, never by
  adding places in code. After any change run `python -m unittest test_world` in `Tools/worldgen/`, then
  `python build_play.py` to refresh the playable slice in `Prototype/play/`.

## Layout
| Path | Contents |
|---|---|
| `Game/` | Unreal project (`BrixTown.uproject`, `Source/`, `Content/`, `Config/`, `Plugins/`) |
| `Art/Reference/` | Approved look targets (read-only), one folder per category; `catalog.json` lists them all, `index.html` shows them |
| `Art/StyleTests/` | Look and animation experiments, including rejected ones |
| `Art/Source/` | Blender files and textures being authored |
| `Art/Export/` | Files exported from Blender for import into Unreal |
| `Backend/` | Account, ledger and UGC services (from PH2) |
| `World/` | The map: shared rules and one folder per city, a file per block |
| `Prototype/play/` | Browser-playable slice built from `World/` (stand-in until the Unreal build) |
| `Tools/` | Build, export and validation scripts |
| `Tests/` | Test plans, bot scenarios, load-test configs (engine tests live beside their modules) |
| `docs/specs/` | Specifications |
| `docs/specs/domains/` | Domain specs (living city) |
| `docs/process/` | How features go from idea to game |
| `docs/backlog/` | Feature backlog, the owner's level picks, scenarios |
| `docs/art/` | Art direction, face controls, body motion |
| `docs/decisions/` | ADRs |
| `docs/standards/` | Naming and coding standards |
| `docs/setup/` | Environment setup |
| `docs/licenses/` | Third-party asset and library licences |
