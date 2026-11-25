#include "BrixNight.h"

FBrixLightNow BrixLightNow(const FBrixNightRules& R, const FBrixLightNowIn& In)
{
	FBrixLightNow N;
	const float Day = FMath::Clamp(In.Daylight, 0.f, 1.f), Let = FMath::Clamp(In.SunLet, 0.f, 1.f);
	N.Night = 1.f - FMath::SmoothStep(0.f, FMath::Max(R.NightBelow, .01f), Day);

	// Low is the game mode's own measure of a low sun (UpdateSun); High is 1 where the day is kept as the game mode has it
	const float Low = FMath::SmoothStep(0.05f, 0.5f, In.SunUp);
	const float High = FMath::SmoothStep(0.f, FMath::Max(R.LowSunBelow, .01f), Low);
	N.LowSun = 1.f - High;

	// the eye: free by day with the sun up; with the sun on the horizon the streets are lit by the sky alone and it may
	// open DuskOpen stops, so dusk is dusk and not a grey noon; once the day is gone, NightOpen (more in a room, by its lamps)
	const float In01 = FMath::Clamp(In.Indoors, 0.f, 1.f);
	const float Open = FMath::Lerp(FMath::Lerp(R.NightOpen, R.NightOpenIndoors, In01), FMath::Lerp(FMath::Lerp(R.DuskOpen, R.DayOpen, In01), R.DayOpen, High), Day);
	N.MinEV = R.DayEV - Open;

	// the haze
	const FLinearColor SunTint = FMath::Lerp(In.SunDawnColour, FLinearColor::White, Low);
	const FLinearColor Covered = FMath::Lerp(FLinearColor::White, In.OvercastHaze, 1.f - Let);   // a covered sky's grey
	const FLinearColor DayHaze = In.HazeColour * FMath::Lerp(R.LowSunHaze * Covered, FMath::Lerp(SunTint, In.OvercastHaze, 1.f - Let), High);
	N.Haze = FMath::Lerp(R.NightHaze, DayHaze, Day);
	// the glow of the air towards the sun: the engine's own by day, the low sun's colour as it sinks, none at night
	N.Glow = FMath::Lerp(In.SunColourNow * R.LowSunGlow, In.DayGlow, High) * Day;

	// the night's light, less of it under cloud, and how much of it the air gives back
	N.NightLux = R.NightLux * N.Night * (.3f + .7f * Let);
	N.Sky = FMath::Lerp(FLinearColor::White, R.NightSky, N.Night);
	return N;
}
