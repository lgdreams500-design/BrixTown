# Unreal gaps: weather, gardens, street use (39)

Test shots of the gaps closed between the browser slice and the Unreal build (`Game/README.md`): the weather key, rain seen
from indoors, a lawn's own pattern and a broken bench nobody sits on. All at High
(`-ExecCmds="scalability 2"`), 1600 x 900, `-BrixNoDesigner`, at 11:00 on the built day 16. Frame rates are noisy: three
other copies of Unreal were running on the same laptop.

| Shot | Switches | What it shows |
|---|---|---|
| `lawn.png` | `"-BrixStand=830,830,0" "-BrixCam=806,839,1.8,-20,-26" -BrixWhole` | CITY-017, ART-001: b_8_8's garden lawn in M_Lawn: patches, clover, daisies. 10.0 fps |
| `lawn_before.png` | the same, with M_Lawn moved aside | the lawn as it was: one even green. 7.7 fps |
| `lawn_in_the_slice.png` | the slice, `?auto=1&x=784&z=806.5&yaw=-1.05&pitch=-0.45&t=660` (headless Edge) | the slice's material 24 nearby, for its tones and daisies |
| `rain_through_the_window.png` | `-BrixWeather=rain -BrixInside=b_9_9_l3 "-BrixCam=931.6,949.3,1.5,82,-4"` | WLD-037: from inside Quarry Grocer, rain falling outside its open door (none was drawn indoors before) |
| `weather_key_rain.png` | `-BrixWeatherKey=3 "-BrixCam=1182.5,1007.5,1.9,-42,-14" -BrixWhole` | WLD-037: K pressed three times (clear, cloud, rain): rain, puddles, umbrellas up. 7.9 fps |
| `bench_whole.png` | `"-BrixStand=1180,1012,0" "-BrixCam=1182.5,1007.5,1.9,-42,-14" -BrixWhole` | residents on the garden bench at (1187.2, 1002.0) |
| `bench_broken.png` | the same without `-BrixWhole`, with `Game/Saved/BrixDamage.json` holding `bench@1187.2,1002.0` | CITY-072: the bench in rubble, those who sat on it standing in front of it. 16.9 fps |
