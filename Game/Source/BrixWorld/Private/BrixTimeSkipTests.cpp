// WLD-073, WLD-074: skip time (Tools/citysim/test_time_skip.py).
#include "BrixTimeSkip.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool LoadSkipRules(FAutomationTestBase& Test, FBrixTimeSkipRules& R)
	{
		FString Error;
		return Test.TestTrue(TEXT("time_skip.json loads: ") + Error, LoadBrixTimeSkipRules(
			FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/time_skip.json"))), R, Error));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD073_Steps, "BrixTown.World.WLD073_ten_minutes_an_hour_and_the_next_weather",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD073_Steps::RunTest(const FString&)
{
	FBrixTimeSkipRules R;
	if (!LoadSkipRules(*this, R)) return false;
	TArray<FString> Ids;
	for (const FBrixSkipStep& S : R.Steps) Ids.Add(S.Id);
	TestEqual(TEXT("the three steps, in data"), FString::Join(Ids, TEXT(",")), FString(TEXT("10m,1h,weather")));
	const TArray<FString> Day = { TEXT("cloud"), TEXT("clear"), TEXT("clear"), TEXT("rain") };   // 00:00, 06:00, 12:00, 18:00
	const double T = 16 * 1440.0 + 7 * 60 + 20;   // day 16, 07:20
	TestEqual(TEXT("10 minutes"), BrixSkipTarget(R, Day, T, TEXT("10m")), T + 10);
	TestEqual(TEXT("an hour"), BrixSkipTarget(R, Day, T, TEXT("1h")), T + 60);
	TestEqual(TEXT("from clear at 07:20 to the rain at 18:00"), BrixSkipTarget(R, Day, T, TEXT("weather")), 16 * 1440.0 + 18 * 60);
	TestEqual(TEXT("from the rain at 19:00 round to the cloud at midnight"), BrixNextWeather(R, Day, 16 * 1440.0 + 19 * 60), 17 * 1440.0);
	TestEqual(TEXT("from the cloud at 01:00 to the clear at 06:00"), BrixNextWeather(R, Day, 17 * 1440.0 + 60), 17 * 1440.0 + 360);
	const TArray<FString> Same = { TEXT("clear"), TEXT("clear"), TEXT("clear"), TEXT("clear") };
	TestEqual(TEXT("never changing: the limit, max_days on"), BrixNextWeather(R, Same, T), T + R.MaxDays * 1440.0);
	TestTrue(TEXT("an unknown step"), BrixSkipTarget(R, Day, T, TEXT("1y")) < 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD074_Who, "BrixTown.World.WLD074_only_the_host_skips_and_not_while_busy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD074_Who::RunTest(const FString&)
{
	FBrixTimeSkipRules R;
	if (!LoadSkipRules(*this, R)) return false;
	const TArray<FString> Day = { TEXT("clear"), TEXT("clear"), TEXT("clear"), TEXT("clear") };
	const double T = 16 * 1440.0 + 12 * 60;
	double Out = 0;
	TestEqual(TEXT("the host: allowed"), BrixAskSkip(R, Day, T, TEXT("1h"), true, {}, Out), FString());
	TestEqual(TEXT("the host: an hour on"), Out, T + 60);
	TestEqual(TEXT("anyone else: refused"), BrixAskSkip(R, Day, T, TEXT("1h"), false, {}, Out), FString(TEXT("not_host")));
	TestEqual(TEXT("refused: the clock as it was"), Out, T);
	TestEqual(TEXT("in a conversation: refused"), BrixAskSkip(R, Day, T, TEXT("10m"), true, { TEXT("conversation") }, Out), FString(TEXT("busy:conversation")));
	TestEqual(TEXT("on a ride: refused"), BrixAskSkip(R, Day, T, TEXT("10m"), true, { TEXT("ride") }, Out), FString(TEXT("busy:ride")));
	TestEqual(TEXT("an unknown step: refused"), BrixAskSkip(R, Day, T, TEXT("1y"), true, {}, Out), FString(TEXT("unknown_step")));
	return true;
}

#endif
