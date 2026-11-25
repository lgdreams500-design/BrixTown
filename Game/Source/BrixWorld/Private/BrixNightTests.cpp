// WLD-042 (ADR-0023): the eye and the night's light in the Unreal build.
#include "BrixNight.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	float Lum(const FLinearColor& C) { return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B; }
	bool Near(const FLinearColor& A, const FLinearColor& B, float Tol) { return FMath::Abs(A.R - B.R) < Tol && FMath::Abs(A.G - B.G) < Tol && FMath::Abs(A.B - B.B) < Tol; }   // the colour, not its alpha
	bool Same(const FLinearColor& A, const FLinearColor& B) { return Near(A, B, 1e-6f); }
}

// By day, with the sun well up, nothing of the approved daylight is touched: the haze and its glow are the game mode's own
// (in clear weather, in the morning's tint, under a covered sky) and the eye is free. At night the haze is the night's,
// the glow is gone, the eye opens only so far (further in a room), and the night's light and sky are in, less of the light
// under rain. In between the eye closes steadily as the day goes, a low sun's haze is dimmer and cooler than the day's, and
// with the sun on the horizon the eye opens only so far out of doors.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD042_DayKeptNightDark, "BrixTown.World.WLD042_the_days_light_is_kept_and_the_night_is_dark",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD042_DayKeptNightDark::RunTest(const FString&)
{
	const FBrixNightRules R;
	FBrixLightNowIn In;
	In.DayGlow = FLinearColor(0.25f, 0.25f, 0.125f);
	In.SunColourNow = FLinearColor(1.f, 0.86f, 0.68f);

	// noon, clear
	In.Daylight = 1.f; In.SunUp = 0.95f; In.SunLet = 1.f;
	FBrixLightNow N = BrixLightNow(R, In);
	TestTrue(TEXT("at noon the haze is the game mode's own"), Same(N.Haze, In.HazeColour));
	TestTrue(TEXT("at noon the glow towards the sun is the engine's own"), Same(N.Glow, In.DayGlow));
	TestTrue(TEXT("by day the eye is free"), N.MinEV <= R.DayEV - 10.f);
	TestEqual(TEXT("by day there is no night light"), N.NightLux, 0.f);
	TestTrue(TEXT("by day the sky is the air's own"), Same(N.Sky, FLinearColor::White));
	TestEqual(TEXT("by day it is not night"), N.Night, 0.f);
	TestEqual(TEXT("with the sun up the lamps are not wanted"), N.LowSun, 0.f);
	In.Indoors = 1.f;
	TestEqual(TEXT("in a room by day the eye opens DayOpenIndoors stops, where the reference rooms match Blender's"), BrixLightNow(R, In).MinEV, R.DayEV - R.DayOpenIndoors);
	In.Indoors = 0.f;

	// the game's own start, 07:20 in summer: the sun 24 degrees up, the haze in the morning's tint as the game mode has it
	In.SunUp = 0.40f;
	N = BrixLightNow(R, In);
	const float Low = FMath::SmoothStep(0.05f, 0.5f, 0.40f);
	TestTrue(TEXT("at 07:20 the haze is the game mode's own, in the morning's tint"), Same(N.Haze, In.HazeColour * FMath::Lerp(In.SunDawnColour, FLinearColor::White, Low)));
	TestTrue(TEXT("at 07:20 the glow is the engine's own"), Same(N.Glow, In.DayGlow));

	// noon under rain: the covered sky's grey, as the game mode has it
	In.SunUp = 0.95f; In.SunLet = 0.08f;
	N = BrixLightNow(R, In);
	TestTrue(TEXT("under rain at noon the haze is the game mode's grey"), Same(N.Haze, In.HazeColour * FMath::Lerp(FLinearColor::White, In.OvercastHaze, 0.92f)));

	// a low sun, half an hour before sunset: dimmer and cooler than the day's haze, not the orange wall
	In.SunLet = 1.f; In.SunUp = 0.08f;
	N = BrixLightNow(R, In);
	TestTrue(TEXT("with the sun on the horizon the lamps are wanted, before the calendar's dusk"), N.LowSun > 0.99f && N.Night == 0.f);
	TestTrue(TEXT("with the sun on the horizon the eye opens no more than DuskOpen stops"), FMath::Abs(N.MinEV - (R.DayEV - R.DuskOpen)) < 0.02f);
	In.Indoors = 1.f;
	TestEqual(TEXT("in a room it opens DayOpenIndoors stops then, as by day"), BrixLightNow(R, In).MinEV, R.DayEV - R.DayOpenIndoors);
	In.Indoors = 0.f;
	TestTrue(TEXT("a low sun's haze is under a third of the day's"), Lum(N.Haze) < Lum(In.HazeColour) / 3.f);
	TestTrue(TEXT("a low sun's haze is cool, not orange"), N.Haze.B > N.Haze.R);
	TestTrue(TEXT("a low sun's glow is the sun's own colour, dimmed"), Near(N.Glow, In.SunColourNow * R.LowSunGlow, 1e-3f));

	// night, clear, out of doors and in a room
	In.Daylight = 0.f;
	N = BrixLightNow(R, In);
	TestTrue(TEXT("at night the haze is the night's"), Same(N.Haze, R.NightHaze));
	TestTrue(TEXT("at night the air does not glow towards the sun"), Same(N.Glow, FLinearColor::Black));
	TestEqual(TEXT("at night the eye opens NightOpen stops past the day's"), N.MinEV, R.DayEV - R.NightOpen);
	TestEqual(TEXT("at night the night's light is in"), N.NightLux, R.NightLux);
	TestTrue(TEXT("at night the sky is the night's"), Same(N.Sky, R.NightSky));
	In.Indoors = 1.f;
	TestEqual(TEXT("in a room at night the eye opens NightOpenIndoors stops"), BrixLightNow(R, In).MinEV, R.DayEV - R.NightOpenIndoors);
	In.Indoors = 0.f; In.SunLet = 0.08f;
	TestTrue(TEXT("under rain there is less of the night's light, but some"), BrixLightNow(R, In).NightLux < N.NightLux && BrixLightNow(R, In).NightLux > 0.f);

	// through dusk: the eye closes steadily, and the night comes in only as the last of the day goes
	In.SunLet = 1.f;
	float Last = -100.f;
	for (int32 k = 100; k >= 0; --k)
	{
		In.Daylight = k / 100.f;
		const FBrixLightNow D = BrixLightNow(R, In);
		if (!TestTrue(FString::Printf(TEXT("the eye does not open again as the day goes (daylight %.2f)"), In.Daylight), D.MinEV >= Last - 1e-5f)) return false;
		Last = D.MinEV;
		if (In.Daylight >= R.NightBelow) TestEqual(TEXT("no night light while the day lasts"), D.NightLux, 0.f);
	}
	return true;
}

#endif
