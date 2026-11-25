#pragma once

#include "CoreMinimal.h"

// The eye and the night (WLD-042, ADR-0023; ART_DIRECTION W5 to W7): what the picture needs as the sun gets low and after
// it is down, worked out from the day's light, the sun's height and the weather alone. Nothing here is stored or ticked.
// The game mode makes the day's light; UBrixLight (BrixTown) asks this each frame and applies it.

// The numbers (Game/Config/DefaultGame.ini, [/Script/BrixTown.BrixLight])
struct BRIXWORLD_API FBrixNightRules
{
	// the eye: DayEV is the exposure (EV100) the engine settles on in the open street at noon; the others are how many
	// stops it may open past that by day (free), in a room by day, out of doors with the sun on the horizon, at night out
	// of doors, and at night in a room
	float DayEV = 1.3f, DayOpen = 12.f, DayOpenIndoors = 2.4f, DuskOpen = 3.f, NightOpen = 2.f, NightOpenIndoors = 2.5f;
	float NightBelow = 0.3f;                                         // the night comes in as the day's light falls from this to 0
	FLinearColor NightHaze = FLinearColor(0.030f, 0.032f, 0.042f);   // the distance at night
	// a low sun: below LowSunBelow of the game mode's measure of the sun's height the haze turns to its own colour x
	// LowSunHaze, and the glow of the air towards the sun to the sun's colour x LowSunGlow
	float LowSunBelow = 0.6f;
	FLinearColor LowSunHaze = FLinearColor(0.14f, 0.15f, 0.24f);
	float LowSunGlow = 0.1f;
	float NightLux = 0.35f;                                          // the night's faint light from high up
	FLinearColor NightSky = FLinearColor(2.f, 2.3f, 3.f);            // how much of it the air gives back: the sky's blue and its fill
};

// What the game mode's light is now
struct BRIXWORLD_API FBrixLightNowIn
{
	float Daylight = 1.f;      // FBrixSky::Daylight: 1 by day, 0 at night
	float SunUp = 1.f;         // the sine of the height the sun's light comes from (never under sky.json's sun_lift)
	float SunLet = 1.f;        // the share of the sun the weather lets through (1 clear)
	float Indoors = 0.f;       // 0 out of doors to 1 in a room
	FLinearColor HazeColour = FLinearColor(0.7f, 0.69f, 0.68f);      // the game mode's
	FLinearColor SunDawnColour = FLinearColor(1.f, 0.42f, 0.22f);
	FLinearColor OvercastHaze = FLinearColor(0.9f, 0.95f, 1.05f);
	FLinearColor SunColourNow = FLinearColor::White;                 // the sun light's colour this frame
	FLinearColor DayGlow = FLinearColor::Black;                      // the glow of the air towards the sun as the haze was made
};

struct BRIXWORLD_API FBrixLightNow
{
	float Night = 0.f;                              // 0 by day to 1 at night
	float LowSun = 0.f;                             // 0 with the sun up to 1 with it on the horizon or down: the streets are in shade, the lamps are wanted
	float MinEV = -10.f;                            // the eye may not open past this exposure
	FLinearColor Haze = FLinearColor::Black;        // the haze's colour
	FLinearColor Glow = FLinearColor::Black;        // the glow of the air towards the sun
	float NightLux = 0.f;                           // the night light's strength
	FLinearColor Sky = FLinearColor::White;         // times the air's own brightness
};

// By day with the sun well up this gives back the game mode's own haze and glow and leaves the eye free, so the approved
// daylight is as it was. A low sun's haze is dimmer and cooler (the game mode turns all of it the low sun's orange at the
// day's full strength while the light itself falls away); at night the haze is the night's, the eye opens only so far, and
// the night's light and sky come in.
BRIXWORLD_API FBrixLightNow BrixLightNow(const FBrixNightRules& R, const FBrixLightNowIn& In);
