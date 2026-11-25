// WLD-037, CITY-077 to CITY-080: the weather and people in it.
#include "BrixWeather.h"
#include "BrixCars.h"
#include "BrixDay.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString Under(const TCHAR* P) { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), P)); }
}

// world.weather_now() (Tests/values/weather_city_01.json, from Tools/worldgen/unreal_test_values.py): through days of
// every kind of weather and a dry day tried as rain, the port gives the same kind, the same light numbers and the same
// wetness of the paving, within a millionth.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD037_WeatherAsPython, "BrixTown.World.WLD037_the_weather_is_worked_out_as_world_py_does",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD037_WeatherAsPython::RunTest(const FString&)
{
	FBrixWeatherRules R;
	FString Error, Text;
	if (!TestTrue(TEXT("the sky rules load: ") + Error, LoadBrixWeatherRules(Under(TEXT("../World/rules/sky.json")), R, Error))) return false;
	if (!TestTrue(TEXT("the test values are there"), FFileHelper::LoadFileToString(Text, *Under(TEXT("../Tests/values/weather_city_01.json"))))) return false;
	TSharedPtr<FJsonObject> Want;
	if (!TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Want) && Want.IsValid())) return false;
	int32 Wrong = 0, N = 0;
	for (const TSharedPtr<FJsonValue>& V : Want->GetArrayField(TEXT("cases")))
	{
		const TSharedPtr<FJsonObject> C = V->AsObject();
		TArray<FString> Day;
		C->TryGetStringArrayField(TEXT("day"), Day);
		FString Trying;
		C->TryGetStringField(TEXT("trying"), Trying);
		const double m = C->GetNumberField(TEXT("m"));
		const FBrixWeatherNow G = BrixWeatherNow(R, Day, m, Trying);
		const TSharedPtr<FJsonObject> E = C->GetObjectField(TEXT("want"));
		auto Same = [&](double A, const TCHAR* K) { return FMath::Abs(A - E->GetNumberField(K)) < 1e-6; };
		++N;
		if (G.Kind != E->GetStringField(TEXT("kind")) || !Same(G.Sun, TEXT("sun")) || !Same(G.Haze, TEXT("haze")) || !Same(G.Veil, TEXT("veil"))
			|| !Same(G.Grey, TEXT("grey")) || !Same(G.Rain, TEXT("rain")) || !Same(G.Wet, TEXT("wet")))
			if (Wrong++ < 8) AddError(FString::Printf(TEXT("%s at minute %.2f: %s sun %.3f rain %.3f wet %.3f; world.py %s sun %.3f rain %.3f wet %.3f"),
				*FString::Join(Day, TEXT("/")), m, *G.Kind, G.Sun, G.Rain, G.Wet, *E->GetStringField(TEXT("kind")), E->GetNumberField(TEXT("sun")), E->GetNumberField(TEXT("rain")), E->GetNumberField(TEXT("wet"))));
	}
	TestTrue(TEXT("moments checked"), N > 700);
	return Wrong == 0;
}

// WLD-035, WLD-036 (ADR-0014): the clouds as world.clouds_now() has them (the same values file): how much sky they cover
// through days of every kind of weather, where the wind has carried them and the phase of their shapes, from the clock.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD036_CloudsAsPython, "BrixTown.World.WLD036_the_clouds_are_where_world_py_puts_them",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD036_CloudsAsPython::RunTest(const FString&)
{
	FBrixWeatherRules R;
	FString Error, Text;
	if (!TestTrue(TEXT("the sky rules load: ") + Error, LoadBrixWeatherRules(Under(TEXT("../World/rules/sky.json")), R, Error))) return false;
	if (!TestTrue(TEXT("the test values are there"), FFileHelper::LoadFileToString(Text, *Under(TEXT("../Tests/values/weather_city_01.json"))))) return false;
	TSharedPtr<FJsonObject> Want;
	if (!TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Want) && Want.IsValid())) return false;
	int32 Wrong = 0, N = 0;
	for (const TSharedPtr<FJsonValue>& V : Want->GetArrayField(TEXT("clouds")))
	{
		const TSharedPtr<FJsonObject> C = V->AsObject();
		TArray<FString> Day;
		C->TryGetStringArrayField(TEXT("day"), Day);
		const double m = C->GetNumberField(TEXT("m"));
		const FBrixCloudsNow G = BrixCloudsNow(R, Day, (int32)C->GetNumberField(TEXT("n")), m);
		const TSharedPtr<FJsonObject> E = C->GetObjectField(TEXT("want"));
		const TArray<TSharedPtr<FJsonValue>>& O = E->GetArrayField(TEXT("offset"));
		++N;
		if (FMath::Abs(G.Cover - E->GetNumberField(TEXT("cover"))) > 1e-6 || FMath::Abs(G.Offset.X - O[0]->AsNumber()) > 1e-6
			|| FMath::Abs(G.Offset.Y - O[1]->AsNumber()) > 1e-6 || FMath::Abs(G.Phase - E->GetNumberField(TEXT("phase"))) > 1e-6)
			if (Wrong++ < 8) AddError(FString::Printf(TEXT("day %d minute %.2f: cover %.3f, offset (%.3f, %.3f); world.py cover %.3f, offset (%.3f, %.3f)"),
				(int32)C->GetNumberField(TEXT("n")), m, G.Cover, G.Offset.X, G.Offset.Y, E->GetNumberField(TEXT("cover")), O[0]->AsNumber(), O[1]->AsNumber()));
	}
	TestTrue(TEXT("moments checked"), N > 400);
	return Wrong == 0;
}

// CITY-077, CITY-080 (ADR-0020): what citysim gives each resident to wear and carry today (Game/Data/day.json "wear")
// is what they show: a coat in its colour whenever they wear one; out in the rain an umbrella up in the right hand, else
// the hood up, else a hand over the head while walking; under a roof or in light rain nothing but the coat. CITY-079: a
// walk in the rain is hurried without an umbrella, and a walk packed faster than the resident's own pace by more than
// halfway from hurry to run is a run for cover.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY080_DressedForTheRain, "BrixTown.World.CITY080_residents_are_dressed_for_the_rain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY080_DressedForTheRain::RunTest(const FString&)
{
	FBrixPeopleRules People;
	FBrixWear W;
	FString Error;
	if (!TestTrue(TEXT("the people's tables load: ") + Error, LoadBrixPeopleRules(Under(TEXT("Data/people.json")), People, Error))) return false;
	if (!TestTrue(TEXT("the day's wear loads: ") + Error, LoadBrixWear(Under(TEXT("Data/day.json")), Under(TEXT("../World/rules/signatures.json")), W, Error))) return false;
	TestEqual(TEXT("a row for every resident"), W.Today.Num(), W.Wet.Num());
	TestEqual(TEXT("four slots of weather"), W.Weather.Num(), 4);
	int32 Umbrellas = 0, Hoods = 0, Hands = 0, Wrong = 0;
	for (int32 i = 0; i < W.Wet.Num(); ++i)
	{
		const FBrixWear::FRow& Row = W.Wet[i];
		const FBrixWet Dry = BrixWetOf(People, W, i, 0, 0, 1, false, true, false, TEXT("Easy"), 1, 600, 590, 610, 20 * 1.14 * W.Pace);
		const FBrixWet Roof = BrixWetOf(People, W, i, 1, 0, 1, true, true, false, TEXT("Easy"), 1, 600, 590, 610, 20 * 1.14 * W.Pace);
		const FBrixWet Out = BrixWetOf(People, W, i, 1, 0, 1, false, true, false, TEXT("Easy"), 1, 600, 590, 610, 20 * 1.14 * W.Pace);
		const bool bCoat = (Row.Coat >= 0) == Out.Coat.IsSet() && (Row.Coat < 0 || *Out.Coat == People.Palette.FindRef(W.Colours[Row.Coat]));
		const bool bDry = !Dry.Umbrella && !Dry.Hood && !Dry.Arms[0] && !Dry.Arms[1] && !Roof.Umbrella && !Roof.Hood && !Roof.Arms[0] && Dry.Coat.IsSet() == Out.Coat.IsSet();
		bool bCover;
		if (Row.Umbrella >= 0) { bCover = Out.Umbrella && *Out.Umbrella == People.Palette.FindRef(W.Colours[Row.Umbrella]) && !Out.Hood && Out.Arms[1] && Out.Sig.IsEmpty(); ++Umbrellas; }
		else if (Row.bHood) { bCover = Out.Hood && !Out.Umbrella && !Out.Arms[0] && Out.Sig == TEXT("Hurried"); ++Hoods; }
		else { bCover = !Out.Umbrella && !Out.Hood && Out.Arms[0] && (*Out.Arms[0])[1] == 150 && Out.Sig == TEXT("Hurried"); ++Hands; }
		if (!(bCoat && bDry && bCover) && Wrong++ < 8) AddError(FString::Printf(TEXT("resident %d: %s"), i, !bCoat ? TEXT("not in their coat") : !bDry ? TEXT("covered up in the dry or under a roof") : TEXT("not covered as they should be")));
	}
	AddInfo(FString::Printf(TEXT("on a wet day: %d umbrellas, %d hoods, %d bare-headed"), Umbrellas, Hoods, Hands));
	TestTrue(TEXT("some of each"), Umbrellas > 100 && Hoods > 100 && Hands > 10);
	// running for cover: a walk packed at twice the resident's own pace
	const FBrixWet Run = BrixWetOf(People, W, 0, 1, 0, 1, false, true, false, TEXT("Easy"), 1, 600, 590, 600, 10 * 1.14 * W.Pace * W.Run);
	TestEqual(TEXT("a run for cover"), Run.Sig, FString(TEXT("Run")));
	TestTrue(TEXT("its step goes on with the distance"), Run.Phase >= 0 && Run.Phase < 1);
	const FBrixWet Running = BrixWetOf(People, W, 0, 1, 0, 1, false, true, true, TEXT("Easy"), 1, 600, 590, 610, 1);
	TestTrue(TEXT("already running from a car: its own walk"), Running.Sig.IsEmpty());
	return Wrong == 0;
}

// WLD-070 (ADR-0051): how hard it snows and how much snow lies, as world.snow_now() has them (the same values file),
// through days with snow in them and a dry day tried as snow: building while it snows, melting after, within a millionth.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD070_SnowAsPython, "BrixTown.World.WLD070_snow_falls_and_lies_as_world_py_says",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD070_SnowAsPython::RunTest(const FString&)
{
	FBrixWeatherRules R;
	FString Error, Text;
	if (!TestTrue(TEXT("the sky rules load: ") + Error, LoadBrixWeatherRules(Under(TEXT("../World/rules/sky.json")), R, Error))) return false;
	TestTrue(TEXT("snow is a kind of weather"), R.Kinds.Contains(TEXT("snow")) && R.Kinds[TEXT("snow")].Snow > 0);
	if (!TestTrue(TEXT("the test values are there"), FFileHelper::LoadFileToString(Text, *Under(TEXT("../Tests/values/weather_city_01.json"))))) return false;
	TSharedPtr<FJsonObject> Want;
	if (!TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Want) && Want.IsValid())) return false;
	int32 Wrong = 0, N = 0, Lying = 0;
	for (const TSharedPtr<FJsonValue>& V : Want->GetArrayField(TEXT("snow")))
	{
		const TSharedPtr<FJsonObject> C = V->AsObject();
		TArray<FString> Day;
		C->TryGetStringArrayField(TEXT("day"), Day);
		FString Trying;
		C->TryGetStringField(TEXT("trying"), Trying);
		const double m = C->GetNumberField(TEXT("m"));
		const FBrixSnowNow G = BrixSnowNow(R, Day, m, Trying);
		const TSharedPtr<FJsonObject> E = C->GetObjectField(TEXT("want"));
		++N;
		Lying += G.Lying > .1;
		if (FMath::Abs(G.Falling - E->GetNumberField(TEXT("falling"))) > 1e-6 || FMath::Abs(G.Lying - E->GetNumberField(TEXT("lying"))) > 1e-6)
			if (Wrong++ < 8) AddError(FString::Printf(TEXT("%s at minute %.2f: falling %.3f, lying %.3f; world.py %.3f, %.3f"), *FString::Join(Day, TEXT("/")), m,
				G.Falling, G.Lying, E->GetNumberField(TEXT("falling")), E->GetNumberField(TEXT("lying"))));
	}
	TestTrue(TEXT("moments checked, some with snow lying"), N > 600 && Lying > 50);
	return Wrong == 0;
}

// CITY-107 (ADR-0051): on a day of snow (day.json "wear" "snow", from citysim City.wear()) nearly everyone wears a coat;
// out in the snow the hood goes up if their coat has one, else they go bare-headed, never an umbrella, and they walk
// carefully; under a roof, nothing but the coat.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY107_DressedForSnow, "BrixTown.World.CITY107_residents_are_dressed_for_snow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY107_DressedForSnow::RunTest(const FString&)
{
	FBrixPeopleRules People;
	FBrixWear W;
	FString Error;
	if (!TestTrue(TEXT("the people's tables load: ") + Error, LoadBrixPeopleRules(Under(TEXT("Data/people.json")), People, Error))) return false;
	if (!TestTrue(TEXT("the day's wear loads: ") + Error, LoadBrixWear(Under(TEXT("Data/day.json")), Under(TEXT("../World/rules/signatures.json")), W, Error))) return false;
	if (!TestEqual(TEXT("a snow day's row for every resident"), W.Snowy.Num(), W.Today.Num())) return false;
	int32 Coats = 0, Hoods = 0, Wrong = 0;
	for (int32 i = 0; i < W.Snowy.Num(); ++i)
	{
		const FBrixWear::FRow& Row = W.Snowy[i];
		Coats += Row.Coat >= 0;
		Hoods += Row.bHood;
		const FBrixWet Out = BrixWetOf(People, W, i, 0, 1, 2, false, true, false, TEXT("Easy"), 1, 600, 590, 610, 20 * 1.14 * W.Pace * W.SnowPace);
		const FBrixWet Roof = BrixWetOf(People, W, i, 0, 1, 2, true, true, false, TEXT("Easy"), 1, 600, 590, 610, 20 * 1.14 * W.Pace * W.SnowPace);
		const bool bOk = !Out.Umbrella && Out.Hood.IsSet() == Row.bHood && Out.Sig == TEXT("Careful") && !Out.Arms[0] && !Out.Arms[1]
			&& !Roof.Hood && !Roof.Umbrella && Roof.Sig.IsEmpty() && (Row.Coat >= 0) == Out.Coat.IsSet();
		if (!bOk && Wrong++ < 8) AddError(FString::Printf(TEXT("resident %d: not dressed for the snow as citysim says"), i));
	}
	AddInfo(FString::Printf(TEXT("on a day of snow: %d coats, %d hoods of %d residents"), Coats, Hoods, W.Snowy.Num()));
	TestTrue(TEXT("nearly everyone in a coat"), Coats > W.Snowy.Num() * .88);
	TestTrue(TEXT("a careful pace, slower than their own"), W.SnowPace < 1);
	return Wrong == 0;
}

// WLD-071 (ADR-0051): in lying snow each resident walking leaves footprints behind them along their own route, a
// footstep apart, left and right in turn, older and fainter the further back, none ahead of them; a vehicle on the move
// leaves two tracks along where its timeline took it; falling snow fills them in, and with too little snow lying there are
// none. All of it from the clock and the day.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD071_TracksInTheSnow, "BrixTown.World.WLD071_footprints_and_tracks_follow_the_clock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD071_TracksInTheSnow::RunTest(const FString&)
{
	FBrixWeatherRules R;
	FBrixDay Day;
	FBrixTraffic Traffic;
	FString Error;
	if (!TestTrue(TEXT("the sky rules load: ") + Error, LoadBrixWeatherRules(Under(TEXT("../World/rules/sky.json")), R, Error))) return false;
	if (!TestTrue(TEXT("the day loads: ") + Error, LoadBrixDay(Under(TEXT("Data/day.json")), Day, Error))) return false;
	if (!TestTrue(TEXT("the vehicles load: ") + Error, LoadBrixTraffic(Under(TEXT("Data/cars.json")), Under(TEXT("Data/car_models.json")), Traffic, Error))) return false;
	const double Stride = .6, Side = .11, Lying = R.SnowCover;
	int32 Walkers = 0, Wrong = 0;
	for (int32 i = 0; i < Day.Num() && Walkers < 40; i += 37)
	{
		const TOptional<FBrixWhere> W = Day.Where(i, 500);
		if (!W || !W->bWalking || W->WalkDone < 8) continue;
		++Walkers;
		TArray<FBrixPrint> P;
		BrixFootprints(R, Day, i, 500, Stride, Side, 0, Lying, P);
		bool bOk = P.Num() >= 8;
		for (int32 n = 0; bOk && n < P.Num(); ++n)
		{
			const double Behind = FVector2D::DotProduct(W->Pos - P[n].Pos, W->Heading), Off = FMath::Abs(FVector2D::DistSquared(P[n].Pos, W->Pos));
			bOk &= Off < FMath::Square(R.TrailM + 1) && P[n].Strength > 0 && P[n].Strength <= 1;
			if (n) bOk &= P[n].Age > P[n - 1].Age && P[n].Strength <= P[n - 1].Strength + 1e-9;   // older and fainter going back
			if (n == 0) bOk &= Behind > -.01;                                                        // none ahead of them
			if (n) { const FVector2D Across(-P[n].Dir.Y, P[n].Dir.X); bOk &= FMath::Sign(FVector2D::DotProduct(P[n].Pos - P[n - 1].Pos, Across)) != 0; }
		}
		TArray<FBrixPrint> Filling, Thin;
		BrixFootprints(R, Day, i, 500, Stride, Side, 1, Lying, Filling);
		BrixFootprints(R, Day, i, 500, Stride, Side, 0, R.MinLying * .5, Thin);
		double A = 0, B = 0;
		for (const FBrixPrint& Q : P) A += Q.Strength;
		for (const FBrixPrint& Q : Filling) B += Q.Strength;
		bOk &= B < A && Thin.Num() == 0;
		if (!bOk && Wrong++ < 6) AddError(FString::Printf(TEXT("resident %d at 08:20: their footprints are not behind them as they should be (%d)"), i, P.Num()));
	}
	int32 Cars = 0;
	for (int32 k = 0; k < Traffic.Num() && Cars < 10; ++k)
	{
		const TOptional<FBrixCarAt> C = Traffic.At(k, 500), C0 = Traffic.At(k, 499.8);
		if (!C || !C->bMoving || !C0 || FVector2D::Distance(C->Pos, C0->Pos) < 5) continue;
		++Cars;
		TArray<FBrixPrint> T;
		BrixTyreTracks(R, Traffic, k, 500, 1.4, 0, Lying, T);
		double Far = 0;
		for (const FBrixPrint& Q : T) Far = FMath::Max(Far, FVector2D::Distance(Q.Pos, C->Pos));
		if ((T.Num() < 8 || T.Num() % 2 || Far > R.CarTrailM + 30) && Wrong++ < 6) AddError(FString::Printf(TEXT("vehicle %d at 08:20: no tracks behind it (%d)"), k, T.Num()));
	}
	TestTrue(TEXT("walkers and vehicles looked at"), Walkers >= 10 && Cars >= 5);
	TestEqual(TEXT("an old mark under steady snow is filled in"), BrixPrintStrength(R, R.FillMinutes * 1.01, 1, Lying), 0.0);
	return Wrong == 0;
}

#endif
