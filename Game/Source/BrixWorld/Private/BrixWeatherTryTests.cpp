// WLD-037: the weather key steps through the kinds as the slice's K does, snow after fog where sky.json has it.
#include "BrixWeatherTry.h"
#include "BrixWeather.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD037_WeatherKey, "BrixTown.World.WLD037_the_weather_key_steps_through_the_kinds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD037_WeatherKey::RunTest(const FString&)
{
	const TArray<FString> Slice = { TEXT("fog"), TEXT("rain"), TEXT("cloud"), TEXT("clear") };   // the slice's kinds, in any order
	TestEqual(TEXT("from the day's own: clear"), BrixNextWeatherTry(FString(), Slice), FString(TEXT("clear")));
	TestEqual(TEXT("then cloud"), BrixNextWeatherTry(TEXT("clear"), Slice), FString(TEXT("cloud")));
	TestEqual(TEXT("then rain"), BrixNextWeatherTry(TEXT("cloud"), Slice), FString(TEXT("rain")));
	TestEqual(TEXT("then fog"), BrixNextWeatherTry(TEXT("rain"), Slice), FString(TEXT("fog")));
	TestEqual(TEXT("then the day's own again"), BrixNextWeatherTry(TEXT("fog"), Slice), FString());
	TestEqual(TEXT("a kind it does not know: the day's own (indexOf -1 + 1)"), BrixNextWeatherTry(TEXT("hail"), Slice), FString());
	FBrixWeatherRules R;
	FString Error;
	if (!TestTrue(TEXT("sky.json loads: ") + Error, LoadBrixWeatherRules(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/sky.json"))), R, Error))) return false;
	TArray<FString> Kinds;
	R.Kinds.GetKeys(Kinds);
	TArray<FString> Seen;
	FString T;
	for (int32 k = 0; k < 8; ++k) { T = BrixNextWeatherTry(T, Kinds); Seen.Add(T.IsEmpty() ? TEXT("own") : T); if (T.IsEmpty()) break; }
	TestEqual(TEXT("with sky.json's kinds: clear, cloud, rain, fog, snow, the day's own"), FString::Join(Seen, TEXT(", ")), FString(TEXT("clear, cloud, rain, fog, snow, own")));
	return true;
}

// WLD-037: people out in the rain are a little darker, as the slice darkens everything out of doors (play.js: c *= 1 -
// wet * rain.wet_dark * .3), drying as the paving dries; nobody indoors.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD037_WetClothes, "BrixTown.World.WLD037_people_out_in_the_rain_are_a_little_darker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD037_WetClothes::RunTest(const FString&)
{
	FBrixWeatherRules R;
	FString Error;
	if (!TestTrue(TEXT("sky.json loads: ") + Error, LoadBrixWeatherRules(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/sky.json"))), R, Error))) return false;
	const double Share = .3;   // the slice's (DefaultGame.ini PeopleWet)
	const TArray<FString> Wet = { TEXT("rain"), TEXT("clear"), TEXT("clear"), TEXT("clear") }, Dry = { TEXT("clear"), TEXT("clear"), TEXT("clear"), TEXT("clear") };
	const FBrixWeatherNow InRain = BrixWeatherNow(R, Wet, 180), Drying = BrixWeatherNow(R, Wet, 360 + R.DryMinutes / 2), After = BrixWeatherNow(R, Wet, 360 + R.DryMinutes + 30);
	const float Full = BrixWetClothes(InRain.Wet, R.WetDark, Share, false);
	TestTrue(FString::Printf(TEXT("in the rain: %.3f darker, the slice's wet x wet_dark x .3 (%.3f)"), Full, R.WetDark * Share), FMath::IsNearlyEqual(Full, (float)(R.WetDark * Share), 1e-6f));
	const float Half = BrixWetClothes(Drying.Wet, R.WetDark, Share, false);
	TestTrue(FString::Printf(TEXT("drying with the paving: %.3f half-way through dry_minutes"), Half), Half > Full * .3f && Half < Full * .7f);
	TestEqual(TEXT("dry again after it"), BrixWetClothes(After.Wet, R.WetDark, Share, false), 0.f);
	TestEqual(TEXT("dry weather: as they are"), BrixWetClothes(BrixWeatherNow(R, Dry, 180).Wet, R.WetDark, Share, false), 0.f);
	TestEqual(TEXT("indoors in the rain: as they are"), BrixWetClothes(InRain.Wet, R.WetDark, Share, true), 0.f);
	TestEqual(TEXT("trying rain on a dry day: wet"), BrixWetClothes(BrixWeatherNow(R, Dry, 180, TEXT("rain")).Wet, R.WetDark, Share, false), Full);
	return true;
}

#endif
