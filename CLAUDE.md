# Wasted — instructions for AI agents

Third-person multiplayer life-sim in Unreal Engine 5.8. Art source in Blender (5.3 installed; pin is Q-16).

## Before any work
1. Read `docs/specs/GAME_SPEC.md`. It is the source of truth for scope.
2. Work only on requirements whose status is `accepted` or `in-progress`, in the current phase.
3. Treat `[ASSUMPTION]` values as placeholders and `[OPEN: Q-nn]` items as undecided: ask, do not decide.
4. Read `docs/process/FEATURE_PROCESS.md` and `docs/backlog/feature_choices.json`. A feature is in scope for a phase only
   at or below the level the owner picked for it; a missing pick means undecided: ask, do not build.
5. For anything visual, read `docs/art/ART_DIRECTION.md` and look at `Art/Reference/` first. For faces read
   `docs/art/CHARACTER_FACE.md`; for body animation read `docs/art/CHARACTER_MOTION.md`.

## Rules
- Reference requirement IDs in commits, tests and task notes (e.g. `ECO-002: add ledger transaction type`).
- Never renumber or reuse requirement IDs. To drop one, set `status: cut`.
- Every `P0` requirement needs an automated test named with its ID.
- Gameplay logic in C++ modules; tunable numbers in data, not code.
- Money is integer minor units and moves only through the ledger.
- The server is authoritative; never trust client input.
- Do not commit secrets, engine binaries, or `Saved/`, `Intermediate/`, `DerivedDataCache/`, `Binaries/`.
- Binary assets (`.uasset`, `.umap`, `.blend`) cannot be merged: lock before editing.
- Record any significant design choice as an ADR in `docs/decisions/`.
- The look is locked (ADR-0004). New art must match `Art/Reference/`; never overwrite files there. Changing the
  look needs a new ADR and the owner's approval.
- Every head uses the one face control set and every body the one skeleton (joint names in CHARACTER_MOTION §2).
- Look tests are scripts in `Tools/blender/` that build on each other; keep them together and runnable.
- Never edit `docs/backlog/feature_choices.json` or `docs/backlog/BACKLOG.md`: only the owner changes them, through
  `Tools/feature_picker.html`. Propose features and levels by editing `docs/backlog/features.json` and saying so.
- If something can be calculated from the world clock and data, do not tick it or store it. Prove city rules in
  `Tools/citysim/` with tests before building them in the engine (`docs/specs/domains/LIVING_CITY.md` §1).
- The map is data in `World/` (`docs/specs/domains/WORLD_MAP.md`). Change the city by editing block files, never by
  adding places in code. After any change run `python -m unittest test_world` in `Tools/worldgen/`, then
  `python build_play.py` to refresh the playable slice in `Prototype/play/`.

## Layout
| Path | Contents |
|---|---|
| `Game/` | Unreal project (`Wasted.uproject`, `Source/`, `Content/`, `Config/`, `Plugins/`) |
| `Art/Reference/` | Approved look targets (read-only) |
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
