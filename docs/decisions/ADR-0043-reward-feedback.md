# ADR-0043: Reward feedback: coins, chimes and achievements

Date: 2026-11-24. Status: accepted for building (owner's request in chat, 2026-11-24, and the owner's choice of samples:
"Q1, I1, A1"). The feature `F-UI-REWARD` was then picked by the owner at level 2 for the first slice ("Level 2, achievements").

## Context

Money arrived silently: the purse number changed and a "+" showed beside it. The owner asked for "a good feedback system
for the player with delightful chimes and sounds on rewards and achievements": interest from the bank with a sound and an
animation going in near the money indicator, and on a quest's reward coins that "spring out around the character and
should get auto collected with nice sound", with samples to choose from first.

Eight samples were made in `Prototype/play/feedback_samples.html` (three quest rewards, two for interest, two for
achievements, one for money spent). The owner chose Q1 "Lantern bells", I1 "Till and bell" and A1 "Town fanfare".

## Options considered

| Option | For | Against |
|---|---|---|
| Coins as real things in the town that the player walks over to pick up | A game of its own | Money would exist outside the ledger until picked up; a reward could be lost or taken; the server would have to hold it |
| **Coins as a showing of money already paid** | The ledger stays the only place money moves; nothing to save, nothing to lose; works the same in multiplayer, each player seeing their own | The coins are not objects: they pass through walls and other players do not see them |
| Recorded sound files | Any sound at all | Files to license and ship; the slice makes every other sound in code |
| **Sounds made in code from data** | Same as every other sound in the slice; tuning is data | Limited to struck tones and noise |

## Decision

1. **The ledger pays first; the coins only show it.** A reward or interest is posted to the ledger as before (QST-004,
   BANK-008). `rewards.js` reads the new transactions and plays the showing. While coins are on their way the purse shows
   that much less, so it counts up as they land; the ledger balance is never touched.
2. **Which transactions count is worked out from the ledger**: a reward is paid to the player from the rewards account;
   interest is paid by the bank with an interest memo. The account names are data (`rewards.ledger` in
   `World/rules/sound.json`). The rule is `world.reward_events()`, tested against the real economy, and the page checks
   its own copy against it on load. All interest in one settling (several accounts, several days) is shown once.
3. **Sounds and timings are data** (`rewards` in `World/rules/sound.json`): two struck voices (a glass bell, a music box)
   given as partials, the fanfare, the scale the coins climb, the chord, the till, and how many coins a reward of each size
   springs. Checked by `world.reward_sound_problems()`.
4. **Coins are simulated in the town's space** around the player (metres, seconds), drawn over the picture with the
   labels, and are drawn in to the player wherever they have walked to. They are not saved: closing the game mid-flight
   loses nothing, because the money is already in the ledger.
5. **Achievements are earned from facts, not counted.** After the samples the owner picked `F-UI-REWARD` level 2. An
   achievement is data (`World/rules/achievements.json`): a name, a line, and a fact about the player with the number it
   must reach. The facts are worked out from what the game already keeps (ledger, quests, loans, property market) by
   `Tools/citysim/achievements.py`; the page's port checks itself against it on load. Only which were earned, and on
   which day, is kept in the save, so an achievement stays earned when its fact later falls (a home sold).
6. **The banner waits its turn.** Several earned together are shown one after another, and none while a menu covers
   the screen. The Me tab lists them all, earned or not (UI-019).

## Consequences

- SND-019 to SND-021, QST-009 and UI-019 added; `Prototype/play/rewards.js` and `achievements.js` are new; `sound.js` gains `Sound.reward`.
- The ten achievements, their names and their numbers are placeholders (`[ASSUMPTION]`) for the owner to reword and retune.
- A save from before achievements earns at once whatever its facts already reach, shown one after another.
- The character does not hop or cheer when paid (the sample's stand-in did). Body motion follows CHARACTER_MOTION and
  would be its own requirement.
- Money going out keeps its plain "−" beside the purse (sample S1 was for contrast and was not chosen).
- In Unreal the same split holds: the server posts the ledger transaction; the client plays the coins from the
  replicated transaction. Nothing about the coins needs to replicate.
