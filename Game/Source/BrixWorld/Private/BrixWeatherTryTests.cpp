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

#endif
