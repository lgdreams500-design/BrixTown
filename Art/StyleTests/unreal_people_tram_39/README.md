# Riding the tram and people looking at you, in the Unreal build (CITY-041, ANIM-005)

Test shots of the Unreal build (Game/README.md "The tram" and "People"), at High on the owner's laptop, day 16. Not a new
look: the tram is the slice's (cars.js tram()) and the people are people.js's; what is new is that the player can ride
the tram (the slice cannot) and that people near the player turn their heads and eyes to them, as play.js does.

| Shot | How it was taken |
|---|---|
| `riding_the_tram.png` | `-BrixTime=07:55 -BrixTram`: on the tram of run 08:00 standing at the west stop (x 624); "Get off the tram" offered |
| `seated_among_the_riders.png` | the same with `"-BrixCam=627,869,2.3,-90,-8"`: the player on the front bench, south side, facing east |
| `along_tram_street.png` | the same with `-ini:Game:[/Script/BrixTown.BrixGameMode]:ShotDelaySeconds=24`: on the move east, the camera behind |
| `off_at_the_next_stop.png` | `-BrixTime=07:55 -BrixTram -BrixTramOff` and a 62 s delay: off at the middle stop (x 912) onto its platform, the tram going on |
| `looked_at_while_waiting.png` | `-BrixTime=07:53 "-BrixStand=623,860.4,-90"`: at the west stop; the child on the right and the man in front turn to look |

The wordless murmur of a line with no voice (SND-018) is heard, not seen: `-BrixTalk=504 -BrixAnswers=1` logs
`(talk) lines murmured 1` (Mei's second line, 29 syllables) after her first line is said aloud.
