# Quest Atlas: independent audit, 2026-11-25

A second agent read every file in `docs/narrative/` cold, compared it against `World/` and the spec, and ran its own
100,000 play-throughs on top of the checker's rules. It changed nothing. This is its report, condensed. Numbers in
brackets are how many of its runs hit the problem. Nothing below has been fixed yet.

Spot-checked by the author afterwards and confirmed: items 7, 20, 28, 29, and the tram, "Crumb", "Linden" and
bet-line parts of 30. The rest is taken on the auditor's quoted evidence.

## Status after the fixes (same day)

The findings below describe the draft as audited. Since then:

- **Fixed:** 1, 3 to 9, 11, 13 to 16, 20 to 22, and the parts of 10, 12, 17, 18, 19, 23, 24, 26, 27, 28, 30, 34,
  35 and 38 that needed no decision. The checker now replays retakes the way the game would and fails if two
  facts that cannot both be true are ever set together; it passes.
- **Decided by the owner:** 2 (seasons). Quests are not tied to a time of year. The fair is now the Town Fair and
  no quest text names a season or a month. Two lines of history still mention a past winter (when the Town Oven
  closed), which is backstory, not timing.
- **Decided by the owner:** 31 (shared world). The story is each player's own: every player has their own copy of
  the career and the courting, and other players in the same city do not see its results.
- **Decided by the owner:** 29 (Jai). He works at the Old Town Museum. The world data, which has him as a barista,
  changes to match when this is built; nothing in the world data has been changed yet.
- **Decided by the owner:** in 30, the town's name. It is Old Town. The live starter quest's "Welcome to Linden!"
  changes to match when this is built; the game's quest data has not been touched.
- **Waiting on the owner:** nothing from the audit. The writing itself still awaits approval.
- **Fixed in a second pass, after the owner approved the writing:** in 10, the plain loaf now has its own moment at
  the judging table and the wedding has a Town Oven opening; in 12, Vera introduces herself if the wedding comes
  before the fair; in 17, Tam has a line about the broken promise; 25 (the dead courting code and fields are
  gone); in 32 and 33, the most repeated words and jokes are thinned, Pip is messier and Noor's longest lines are
  cut; 36 (the proposal now needs regard 75; a very good dinner can still reach it at once); 39; the months
  and the mall-detour clock in 27 and 30.
- **Left as they are, on purpose:** catalogue codes and "MUM" are still spoken aloud (34); a few characters still
  speak inside narration (34); "six years" is the plot's own number and stays.
- **Second audit:** started after this pass; its result is recorded at the end of this file when it reports.

## Blockers

1. **Retaking a quest replays it with the first attempt's flags.** Flags are never cleared. Get turned away in
   Quest I and knock again: Olu still finds a mall loaf that is not there, and Noor produces it again. Fail the lunch
   with Mei after asking the cook, then bring something fancy next time: she still says "Cheese and pickle. Who told
   you?" Regard can also be farmed: each failed lunch pays its small bonuses again, and one Quest I reply gives +5
   with Noor without limit. The checker missed this because it rewinds on a retry; so does the page's Play mode.
2. **The career is tied to seasons the game calendar puts very far apart.** Quest II is the first snow of winter,
   Quest IV the Autumn Fair, Quest V midwinter. With 12 days to a season and a game day lasting four real hours,
   Quest II to Quest V is about a game year, roughly 190 real hours. There is no fair, month or power cut in the
   calendar data. Needs the owner's decision: are quests season-gated or not?

## Logic that is valid but wrong

3. Noor can be told about Tam's cutting, accept it, then be offered it again as news, and Quest V replays it as a
   revelation. Happens when the cutting is folded in and the Mother is then scalded.
4. Killing the Mother with the kettle is never acknowledged and costs nothing, while confessing a mistake you then
   recovered costs regard.
5. In the vigil, waking Noor can lead to text saying "nobody saw" and an offer to wake her.
6. Quest V says "Noor sent Pip home early" even when Pip has left for Tam's counter. Whether Pip stays changes one
   wedding line and nothing else. [36,229 of 60,000]
7. One flag, two meanings: asking "Why me?" in Quest IV sets the same flag as asking about the order book in
   Quest I, so Noor can thank you for a question you never asked. [6,853]
8. Three dropped threads in Quest V: the token Tam gives you can never be handed to Noor; you can tell Tam to wait
   in the alley and then never mention him; and Noor's "I'll speak to you both after the bread" at the fair has no
   follow-up, while Quest V is written as if the siblings had not met.
9. Tam says he heard of the Mother's loss "from a customer", though Vera announced it at a fair he entered.
10. Choices nothing reads: which loaf you enter at the fair (only the plait matters; Hari's rosemary has no payoff);
    where the wedding is held and who stands up with you; whether Mei takes the café.
11. Mei's dinner assumes the lunch branch where you asked her a question. "You asked. I've been cross with you all
    week" plays even if you only let her talk; "cheese and pickle on the ends" plays even if you never found that
    out. One ending promises a line that does not exist.
12. Courting lines assume career state: "four o'clock batch" and "my oven" play after you have left the bakery;
    Noor shapes the wedding loaf with no mention of her hands; Vera opens the wedding before the player has met
    her; "Abe gave me my first job" is offered to players who never worked for Abe.
13. "Noor said it before I could: use ours" at the fair, when Noor is not there. Two conditions in that scene can
    never fire.
14. Sell Tam the oven and he wins: the ending says the token "went elsewhere" and Quest V has no token beat.
15. Flags that should be exclusive are not: the Mother can be both lost and joined; the secret can be both kept and
    out. The format has no way to clear a flag.
16. You can take Tam's jar, promise him the truth, never use it and never return it.

## Claims in prose the data does not back

17. Seven "ripples" promise reactions that are not written, including "Noor starts every later quest cooler",
    "Vera tastes an unbroken line and says so" and "You charged my brother for my fire".
18. Eleven statements on the Consequences tab are false in reachable states. Examples: "closes the reconciliation
    endings" (Homecoming is still reachable); "lie about it and the door shuts" (it shuts on brazenness, not the
    lie); "about thirty flags" (95 are set, 52 read).
19. Regard numbers: only 7.9% of players reach Mei's lunch level from quests alone; everyone else needs café
    shifts. "Gifts" and "time" are not ways to earn regard in the game today. The checker always lifts regard to
    the gate, so it never tests this.

## Continuity

20. Quest I says "don't touch the crock", but in Quest II the crock has been in the museum for six years.
21. Vera recognises the wall wheat from her childhood, but Len first sowed it when she was 26.
22. The jar labelled MUM is on Noor's table in Quest II and still with Tam in Quest V.
23. Cast notes against quest text: Noor "never walked past his counter" (she says she did, once); Abe "lets it
    slip" (Tam says he wouldn't have); Abe "never late" (he is, at the wedding); Hari's herbs; Vera's "one sentence"
    verdicts run to four.
24. Age rollback is clean. Three lines still say "forty years" where Noor's own tenure is 34; "six years" is still
    said a year later.
25. Leftovers of the removed Noor courting: dead code only, plus one stale string that cannot currently be shown.
26. People named before they are introduced (Tam, on one path); Jai greets by name a player he may never have met.
27. Objects and clock: the signed form never goes back to Jai; quest items are never taken back and could be sold
    from a stall; the Quest I mall detour takes no time; the bakery's hours disagree between files.

## Fit with the game

28. Mei's home in the world data is a two-bed flat shared with one person, not "three cousins".
29. Jai is a barista at the Tram Café in the world data. The live starter quest already calls him "Jai at the Old
    Town Museum", so this is an existing mismatch the proposal builds on.
30. The tram runs every 20 minutes from 05:40, so "the ten past and the half past" does not match; there is no stop
    at Weaver Street; Eastgate Mall already has outlets named "Crumb" on upper floors and its food hall is at
    Northgate; the slice's "bet with the baker" line is about the night gardener, said by every shopkeeper; the
    live quest calls the town "Linden"; 91 residents are already named Noor; Bram, Mrs Ellery, the cook and the
    depot are new and unlisted.
31. Spec: nothing says whether this story state is per player or shared (one player marries Mei, kills the Mother,
    decides the fair, in a multiplayer town). Career ranks rest on a requirement not yet accepted. The list of
    format additions omits several things the files rely on. Amounts spoken in lines ("a hundred and fifty brix")
    go stale on rebalancing, and 150 Bx is only two and a half days' income at today's numbers.

## Minor

32-36. Writing: "forty", "eleven" and "six" are overused; Mei's tells-everyone joke lands about seventeen times;
voices converge on the same dry closing line; Noor talks more than her own voice note allows. One line ends
mid-word; catalogue codes are spoken aloud; characters speak inside narration. Gender is clean apart from one
"his". Nothing uncomfortable in the courting; a proposal after one lunch and one evening is quick.

37-39. Page and checker: the checker's claim to use "the same rules as Play mode" is not quite true; the
Consequences chart marks "you gave Mei the bread" as needed to start Quest I when any ending will do, and shows no
regard or item gates; stand-in grants in Play look like rewards.

## Clean

The copy of the four live quests matches the game's data. No structural hole in a further 100,000 runs. The built
page matches its sources. Noor is not courtable anywhere.

## Verdict

The writing is strong, but the draft is not ready to become game data. Retakes misfire, the seasons do not fit the
calendar, about a dozen branches say things their state contradicts, and roughly twenty stated consequences are
not backed by the data.

# Second audit, 2026-11-25

A different agent audited the draft after the fixes above: first independently, then against this file. Verdict:
nothing structurally broken, but not yet ready, because about twenty lines still contradicted the state leading to
them (several were fix lines added in the first pass), the courting numbers did not do what the page said, and
Quest II still leaned on cold weather. It confirmed as clean: structure (every choice walked), the checker's rules
matching the page's, the copy of the live quests, moods, places against the world data, no gender assumption, and
no romantic framing of Noor.

## Its findings and what was done

New findings (A):

| # | Finding | Status |
|---|---|---|
| A1 | Mei's "that's twice now" played for players who never gave her bread | Fixed: gated on the welcome loaf |
| A2 | Proposal: a player with a home but no savings still got the rented-room speech | Fixed: separate lines for home only and savings only |
| A3 | "Third jar in the world" when Noor does not know Tam's exists | Fixed: "second, so far as I know" |
| A4 | Stole the book and the crock: the book was never mentioned | Fixed: Jai's own opening for both |
| A5 | Tam says his sister refused the jar when she was never told of it | Fixed: three openings by what she knows |
| A6 | Pip offered as best person even after leaving on bad terms | Fixed: gated; Olu is always available |
| A7 | Tam as best man before the siblings are reconciled | Fixed: only after a reconciled ending |
| A8 | "Let time pass" also played the starter quests at random | Fixed |
| A9 | Eli runs the wedding with no introduction | Fixed |
| A10 | Three keepers cannot span 120 years | Fixed: Ilse is the great-grandmother, four keepers |
| A11 | Whose name is on the door disagreed between endings | Fixed: Noor's, for thirty-four years |
| A12 | Quest I verdict: an apology answered with the wrong line; blaming Noor still hired clean | Fixed |
| A13, A14, A15, A17, A18, A19 | Mismatched replies and small contradictions | Fixed |
| A16 | The Saturday loaf was never shown on screen | Fixed: it sits on the rack in Quest I |
| A20 | Weekdays the quests cannot control (Friday, Thursday, Saturday people) | Fixed, except "every Saturday" for the loaf, which is the point of it |
| A21, A22 | "Walking out" used at two stages; narrator tics; Mei's own "six years" | Fixed |
| A23, A24, A27 | Things missing from the build list and from the Consequences threads | Fixed |
| A25 | The cold win paid more than the shared win | Fixed |
| A26 | The player's name inside four spoken lines | Left: the game already speaks names by joining pieces |

Earlier fixes that were incomplete (B):

| # | Finding | Status |
|---|---|---|
| B1 | Quest II still depended on cold weather; "two winters" | Fixed: the power cut stops the proofing room's heater |
| B2, B3 | Regard claims that were false | Fixed: wording corrected; one unearned +10 for Mei removed |
| B4 | A quest taken again replayed first-time dialogue | Fixed: each of the four opens differently the second time |
| B5 | The jar could be promised home and then never mentioned | Fixed: it is the first thing that happens at the door |
| B6, B7, B9, B10 | Fix lines that fired in the wrong state | Fixed |
| B8 | Promise Tam the truth, then fold the cutting in silently: no line for it | Left: his existing line fits either promise |
| B11, B12, B13, B14, B15, B17, B18 | Clock, tram, title and small wording slips | Fixed |
| B16 | Six consequence statements still false in some states | Fixed |
| B19 | Mei's tells-everyone joke is still frequent | Left: it is the character |
| B20 | Status section claimed too much | Corrected by this section |

The checker was widened: it now also proposes with a home but no savings and with savings but no home. A third
audit follows this pass.

# Third audit, 2026-11-25

A third agent checked the second audit's fixes one by one and swept the nine quest files again, with 60,000
play-throughs of its own. Verdict: **not clean, no blocker, seven should-fix**, each a local edit. It verified
most second-audit fixes, found the checker and the page still agree, the built page identical to its sources, the
live-quest copy exact, the owner's decisions honoured and the courting numbers true.

| # | Finding | Status |
|---|---|---|
| 1 | Quest V: Noor says "I can feed the Mother" when the Mother is lost (42% of runs) | Fixed |
| 2 | Dinner: "Worth the ride?" led to a line answering a different question | Fixed |
| 3 | Dinner by the food hall after a middling lunch: the end of the line was never set up | Fixed: she says her wish on the tram |
| 4 | Proposal: home-only and savings-only lines missing after the first ask, and when asking again | Fixed |
| 5 | Pip names Tam without saying he is Noor's brother, then replies assume it | Fixed |
| 6 | "Crock, rye and the night" offered to players who had refused the crock | Fixed |
| 7 | Four consequence statements still false in common states | Fixed |
| 8 | Two fix lines about the lunch repeated each other | Fixed |
| 9, 10, 11, 13, 14, 15, 16, 17, 19 | Minor wording, the token left in the pocket, the promise flag, sunshine and moonlight | Fixed |
| 12 | A quest taken again replays the rest of its first morning after the new opening | Left as designed: Noor says it is the same test |
| 18 | "The first loaf you bake today" for a player who has left the bakery | Left: rare, and still true of a baker |
| 20, 21 | The game's own quest checker would reject two things this draft uses; the rare book can be sold | Added to the build list |
| 22 | The checker never changed house or savings between two askings | Fixed in the checker |
| 23, 24 | Journeys folded into one conversation; ripples that are colour only | Noted for the build |

A fourth, narrow check of these changes follows.

# Fourth check, 2026-11-25

A fourth agent checked the third audit's fixes node by node against every state that can reach them, with 60,000
play-throughs in the checker's order and 30,000 in free order. Verdict: **nearly clean. No blocker, three
should-fix, each a one-line text edit.** All three are fixed, with the minor wording it listed:

| # | Finding | Status |
|---|---|---|
| 1 | The page promised Tam's slip at the fair, which now only happens if you promised silence and shared the oven | Fixed in both places |
| 2 | The page said the first morning decides whether Pip stays; the fountain decides it | Fixed in three places |
| 3 | "Abe gave me my first job" is false for a player hired at the bakery before running Abe's parcel | Fixed: "gave me work when I was new in town" |
| 4 to 8, 10, 11, 13 to 15, 17 | Minor wording, a reply that sent a home-owner to buy a home, ripples in the wrong ending | Fixed |
| 9 | The token can be carried in and never handed over | Left: it stays an open reply |
| 12 | Returning the book and confessing the crock in one breath gets an answer about the crock only | Left |
| 16 | Noor's "the second question" also plays after "Why me?" | Left |
| 18, 19 | The checker always plays starter quests first; savings must count bank accounts | Noted for the build |

It found clean: every node listed for it, the order of openings in the proposal, the oven conditions at the fair,
the decision statements and ripples changed in the last pass, and the owner's decisions. With these edits no
blocker and no should-fix item is open. The draft goes forward to be built.
