// Automation tests for the map as data. Run headless:
//   UnrealEditor-Cmd.exe Game/BrixTown.uproject -ExecCmds="Automation RunTests BrixTown.World; Quit" -unattended -nullrhi
#include "BrixCity.h"
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
	FString StartingCityDir() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/city_01"))); }
}

// WLD-020: the map is data. The starting city loads from World/ with every block city.json lists, and the loader holds no
// knowledge of what is in it (the counts here come from the files, not from the code).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD020_CityLoadsFromData, "BrixTown.World.WLD020_city_loads_from_data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD020_CityLoadsFromData::RunTest(const FString&)
{
	FBrixCity City;
	FString Error;
	if (!TestTrue(TEXT("the starting city loads: ") + Error, LoadBrixCity(StartingCityDir(), City, Error))) return false;

	FString Text;
	FFileHelper::LoadFileToString(Text, *FPaths::Combine(StartingCityDir(), TEXT("city.json")));
	int32 Listed = 0;
	for (int32 At = Text.Find(TEXT("\"file\"")); At != INDEX_NONE; At = Text.Find(TEXT("\"file\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 1)) ++Listed;
	TestEqual(TEXT("one block for each file city.json lists"), City.Blocks.Num(), Listed);
	TestTrue(TEXT("the city has lots"), City.NumLots() > 0);
	TestTrue(TEXT("the spawn lies inside the map"),
		City.SpawnPos.X > 0 && City.SpawnPos.Y > 0 && City.SpawnPos.X < City.Columns * City.Pitch && City.SpawnPos.Y < City.Rows * City.Pitch);
	return true;
}

// WLD-022: every lot has a stable id, a kind, a name, a footprint and a door; the starting city passes the check.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD022_LotsAreSound, "BrixTown.World.WLD022_every_lot_is_sound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD022_LotsAreSound::RunTest(const FString&)
{
	FBrixCity City;
	FString Error;
	if (!TestTrue(TEXT("the starting city loads: ") + Error, LoadBrixCity(StartingCityDir(), City, Error))) return false;
	const TArray<FString> Problems = CheckBrixCity(City);
	for (const FString& P : Problems) AddError(P);
	return Problems.IsEmpty();
}

// WLD-022: the check catches broken lots (a doubled id, no door, a building without floors), so a pass above means something.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD022_BrokenLotsAreCaught, "BrixTown.World.WLD022_broken_lots_are_caught",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD022_BrokenLotsAreCaught::RunTest(const FString&)
{
	FBrixCity City;
	FBrixBlock& B = City.Blocks.AddDefaulted_GetRef();
	B.Id = TEXT("b_test");
	FBrixLot Good;
	Good.Id = TEXT("l1"); Good.Kind = TEXT("home"); Good.Name = TEXT("1 Test Row");
	Good.Min = FVector2D(0, 0); Good.Max = FVector2D(10, 8); Good.Door = FVector2D(5, 0); Good.Floors = 2;
	B.Lots.Add(Good);
	TestEqual(TEXT("a sound lot passes"), CheckBrixCity(City).Num(), 0);

	FBrixLot Twin = Good;                       // the same id again
	FBrixLot NoDoor = Good; NoDoor.Id = TEXT("l2"); NoDoor.Door = FVector2D::ZeroVector;
	FBrixLot NoFloors = Good; NoFloors.Id = TEXT("l3"); NoFloors.Floors = 0;
	FBrixLot Garden = Good; Garden.Id = TEXT("l4"); Garden.Kind = TEXT("garden"); Garden.Floors = 0;   // open ground needs none
	B.Lots.Append({ Twin, NoDoor, NoFloors, Garden });
	TestEqual(TEXT("three broken lots, three problems"), CheckBrixCity(City).Num(), 3);
	return true;
}

// WLD-038, WLD-039: stone, brick and paving by the rules of World/rules/surfaces.json (ADR-0017). The C++ port of
// world.surfaces() picks, for every lot and block, the same pattern, colour and "to the eaves" as the Python does
// (Tests/values/surfaces_city_01.json, written by Tools/worldgen/unreal_test_values.py).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD038_SurfacesMatchTheRules, "BrixTown.World.WLD038_surfaces_match_the_rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD038_SurfacesMatchTheRules::RunTest(const FString&)
{
	FBrixCity City;
	FString Error;
	if (!TestTrue(TEXT("the starting city loads: ") + Error, LoadBrixCity(StartingCityDir(), City, Error))) return false;
	FString Text;
	const FString Values = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../Tests/values/surfaces_city_01.json")));
	if (!TestTrue(TEXT("the test values are there: ") + Values, FFileHelper::LoadFileToString(Text, *Values))) return false;
	TSharedPtr<FJsonObject> Want;
	if (!TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Want) && Want.IsValid())) return false;

	auto Same = [&](const FString& What, const FBrixSurface& Got, const TSharedPtr<FJsonObject>& W, const TCHAR* Slot)
	{
		const FString Name = City.Patterns.IsValidIndex(Got.Pattern) ? City.Patterns[Got.Pattern].Name : TEXT("(none)");
		const FString WantName = W->GetStringField(Slot);
		if (Name != WantName) { AddError(FString::Printf(TEXT("%s: %s, the rules say %s"), *What, *Name, *WantName)); return false; }
		const TArray<TSharedPtr<FJsonValue>>* C = nullptr;
		const bool bWantColour = W->TryGetArrayField(TEXT("color"), C);
		if (bWantColour != Got.Color.IsSet() || (bWantColour && !FVector3f((float)(*C)[0]->AsNumber(), (float)(*C)[1]->AsNumber(), (float)(*C)[2]->AsNumber()).Equals(Got.Color.GetValue(), 1e-4f)))
		{
			AddError(What + TEXT(": a different colour from the rules"));
			return false;
		}
		bool bUpper = false;
		if (W->TryGetBoolField(TEXT("upper"), bUpper) && bUpper != Got.bUpper) { AddError(What + TEXT(": to the eaves or not differs")); return false; }
		return true;
	};
	int32 Checked = 0, Wrong = 0;
	const TSharedPtr<FJsonObject> Lots = Want->GetObjectField(TEXT("lots")), Blocks = Want->GetObjectField(TEXT("blocks"));
	for (const FBrixBlock& B : City.Blocks)
	{
		if (Blocks->HasField(B.Id)) { ++Checked; if (!Same(B.Id, B.Pavement, Blocks->GetObjectField(B.Id), TEXT("pavement"))) ++Wrong; }
		else { AddError(B.Id + TEXT(": no test value")); ++Wrong; }
		for (const FBrixLot& L : B.Lots)
		{
			if (L.Kind == TEXT("garden")) continue;
			if (!Lots->HasField(L.Id)) { AddError(L.Id + TEXT(": no test value")); ++Wrong; continue; }
			const TSharedPtr<FJsonObject> W = Lots->GetObjectField(L.Id);
			++Checked;
			if (!Same(L.Id, L.Surface, W, L.IsOpen() ? TEXT("ground") : TEXT("wall"))) ++Wrong;
		}
	}
	TestEqual(TEXT("every lot and block checked"), Checked, Lots->Values.Num() + Blocks->Values.Num());
	TestEqual(TEXT("surfaces that differ from the rules"), Wrong, 0);
	return Wrong == 0;
}

// ART-001, ADR-0032, ADR-0035: every designed house is built to a plan the game knows, in its line and its colourway
// (house_styles.json): the colours it is given are the colourway's, never the generic building's.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FART001_HousesHaveTheirColourways, "BrixTown.World.ART001_every_house_has_its_line_and_colourway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FART001_HousesHaveTheirColourways::RunTest(const FString&)
{
	FBrixCity City;
	FString Error;
	if (!TestTrue(TEXT("the starting city loads: ") + Error, LoadBrixCity(StartingCityDir(), City, Error))) return false;
	const FBrixHousePlan* Plan = City.HousePlans.Find(TEXT("house_12x9"));
	if (!TestNotNull(TEXT("the 12 x 9 m house plan is read"), Plan)) return false;
	TestEqual(TEXT("its nine windows"), Plan->Windows.Num(), 9);
	// every plan comes with its windows, those listed by floor too (the row houses and flats: read as a list, they gave none)
	for (const auto& KV : City.HousePlans) TestTrue(KV.Key + TEXT(" has its windows"), KV.Value.Windows.Num() > 0);
	if (const FBrixHousePlan* Row = City.HousePlans.Find(TEXT("row_14x12_2"))) TestEqual(TEXT("the row house's ground floor windows"), Row->Windows.Num(), 9);
	TestTrue(TEXT("its door on the front"), Plan->DoorWidth > 0.5f && FMath::Abs(Plan->DoorAt) < Plan->HalfU);
	int32 Houses = 0, Bad = 0;
	TSet<FString> Ways;
	for (const FBrixBlock& B : City.Blocks)
		for (const FBrixLot& L : B.Lots)
		{
			if (!L.House.IsSet()) continue;
			++Houses;
			const FBrixHouseLook& H = L.House.GetValue();
			Ways.Add(H.Line + TEXT("/") + H.Style);
			if (H.Shutter.IsNearlyZero() || H.Door.IsNearlyZero() || H.Frame.IsNearlyZero() || H.Roof.IsNearlyZero() || H.Wall.IsNearlyZero())
			{
				AddError(L.Id + TEXT(": a colour of its colourway is missing (") + H.Line + TEXT(" ") + H.Style + TEXT(")"));
				++Bad;
			}
		}
	TestTrue(TEXT("the city has designed houses"), Houses > 0);
	TestEqual(TEXT("all twelve colourways are in use"), Ways.Num(), 12);
	return Bad == 0;
}

// WLD-043 (ADR-0024): the sun follows the calendar. Its season for a day, its height for each season and its place at the
// hours of a day are those the Python works out (world.sun_heights(), world.sun_place(); Tests/values/sun_city_01.json).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD043_TheSunFollowsTheCalendar, "BrixTown.World.WLD043_the_sun_follows_the_calendar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD043_TheSunFollowsTheCalendar::RunTest(const FString&)
{
	FBrixCity City;
	FString Error;
	if (!TestTrue(TEXT("the starting city loads: ") + Error, LoadBrixCity(StartingCityDir(), City, Error))) return false;
	FString Text;
	const FString Values = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../Tests/values/sun_city_01.json")));
	if (!TestTrue(TEXT("the test values are there: ") + Values, FFileHelper::LoadFileToString(Text, *Values))) return false;
	TSharedPtr<FJsonObject> Want;
	if (!TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Want) && Want.IsValid())) return false;
	const FBrixSky& S = City.Sky;
	int32 Wrong = 0;
	for (const auto& KV : Want->GetObjectField(TEXT("season_of_day"))->Values)
	{
		const int32 Day = FCString::Atoi(*FString(KV.Key.ToView()));
		if (S.Seasons[S.SeasonOf(Day)] != KV.Value->AsString()) { AddError(FString::Printf(TEXT("day %d: %s, the calendar says %s"), Day, *S.Seasons[S.SeasonOf(Day)], *KV.Value->AsString())); ++Wrong; }
	}
	for (const auto& KV : Want->GetObjectField(TEXT("seasons"))->Values)
	{
		const FString Name(KV.Key.ToView());
		const int32 Season = S.Seasons.IndexOfByKey(Name);
		if (Season == INDEX_NONE) { AddError(Name + TEXT(": no such season")); ++Wrong; continue; }
		const TSharedPtr<FJsonObject> W = KV.Value->AsObject();
		if (FMath::Abs(FMath::RadiansToDegrees(S.Declination(Season)) - W->GetNumberField(TEXT("declination"))) > 1e-6)
		{
			AddError(Name + TEXT(": the sun's height differs from the Python's")); ++Wrong;
		}
		for (const auto& P : W->GetObjectField(TEXT("places"))->Values)
		{
			const double Minute = FCString::Atod(*FString(P.Key.ToView()));
			const TArray<TSharedPtr<FJsonValue>>& A = P.Value->AsArray();
			const FVector3d Got = S.SunPlace(Season, S.HourAngle(Season, Minute)), Exp(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber());
			if (!Got.Equals(Exp, 1e-6)) { AddError(FString::Printf(TEXT("%s at minute %.0f: the sun at %s, the Python says %s"), *Name, Minute, *Got.ToString(), *Exp.ToString())); ++Wrong; }
		}
	}
	TestEqual(TEXT("places of the sun that differ from the Python's"), Wrong, 0);
	return Wrong == 0;
}

// CITY-022: a resident's position at any time follows from the resident, the world time and data alone. The day the game
// reads (Game/Data/day.json) puts every sampled resident where citysim City.at() does (Tests/values/residents_city_01.json):
// away or in a vehicle when it says so, otherwise at its position, to the data's rounding: at a place within 0.08 m (each
// coordinate is rounded to 0.1 m); on a walk within 2 m (each piece's start is rounded to 0.1 game minute, and at walking
// pace a game minute is some 14 m; strolls are sampled each minute). A wrong piece or route is tens of metres out.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY022_ResidentsFollowTheirRoutine, "BrixTown.World.CITY022_residents_are_where_their_routine_puts_them",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY022_ResidentsFollowTheirRoutine::RunTest(const FString&)
{
	FBrixDay Day;
	FString Error;
	const FString DayPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/day.json")));
	if (!TestTrue(TEXT("the day loads: ") + Error, LoadBrixDay(DayPath, Day, Error))) return false;
	FString Text;
	const FString Values = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../Tests/values/residents_city_01.json")));
	if (!TestTrue(TEXT("the test values are there: ") + Values, FFileHelper::LoadFileToString(Text, *Values))) return false;
	TSharedPtr<FJsonObject> Want;
	if (!TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Want) && Want.IsValid())) return false;
	TestEqual(TEXT("the same day"), Day.Day, (int32)Want->GetNumberField(TEXT("day")));
	int32 Checked = 0, Wrong = 0;
	for (const auto& MV : Want->GetObjectField(TEXT("at"))->Values)
	{
		const double Minute = FCString::Atod(*FString(MV.Key.ToView()));
		for (const auto& RV : MV.Value->AsObject()->Values)
		{
			const FString Id(RV.Key.ToView());
			const int32 i = Day.Ids.IndexOfByKey(Id);
			if (i == INDEX_NONE) { AddError(Id + TEXT(": not in the day")); ++Wrong; continue; }
			const TSharedPtr<FJsonObject> W = RV.Value->AsObject();
			const TOptional<FBrixWhere> Got = Day.Where(i, Minute);
			const TArray<TSharedPtr<FJsonValue>>* Pos = nullptr;
			const bool bSeen = W->TryGetArrayField(TEXT("pos"), Pos) && Pos->Num() == 2;
			++Checked;
			if (bSeen != Got.IsSet())
			{
				AddError(FString::Printf(TEXT("%s at minute %.0f: %s, the routine says %s"), *Id, Minute, Got.IsSet() ? TEXT("in the town") : TEXT("not in the town"), *W->GetStringField(TEXT("state"))));
				++Wrong;
				continue;
			}
			if (!bSeen) continue;
			const FVector2D Exp((*Pos)[0]->AsNumber(), (*Pos)[1]->AsNumber());
			const double Off = FVector2D::Distance(Got->Pos, Exp), Room = Got->bWalking ? 2.0 : 0.08;
			if (Off > Room) { AddError(FString::Printf(TEXT("%s at minute %.0f (%s): %.2f m from where the routine puts them"), *Id, Minute, *W->GetStringField(TEXT("state")), Off)); ++Wrong; }
		}
	}
	TestTrue(TEXT("residents checked"), Checked > 100);
	TestEqual(TEXT("residents not where their routine puts them"), Wrong, 0);
	return Wrong == 0;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS
// CITY-041: the trams run to their timetable. At 3,000 moments through a day every tram is where the slice's tramAt()
// (play.js, run by Tools/unreal/people_values.py into Tests/values/talk_city_01.json) has it: the same runs on the map,
// each within a centimetre along the line, standing at a stop when it does.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY041_TramsKeepTheTimetable, "BrixTown.World.CITY041_trams_are_where_the_timetable_puts_them",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY041_TramsKeepTheTimetable::RunTest(const FString&)
{
	FBrixCity City;
	FString Error, Text;
	if (!TestTrue(TEXT("the starting city loads: ") + Error, LoadBrixCity(StartingCityDir(), City, Error))) return false;
	const FString Values = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../Tests/values/talk_city_01.json")));
	if (!TestTrue(TEXT("the test values are there"), FFileHelper::LoadFileToString(Text, *Values))) return false;
	TSharedPtr<FJsonObject> Want;
	if (!TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Want) && Want.IsValid())) return false;
	int32 Moments = 0, Trams = 0, Wrong = 0;
	for (const TSharedPtr<FJsonValue>& V : Want->GetArrayField(TEXT("tram")))
	{
		const TArray<TSharedPtr<FJsonValue>>& P = V->AsArray();
		const double M = P[0]->AsNumber();
		const TArray<TSharedPtr<FJsonValue>>& Exp = P[1]->AsArray();
		const TArray<FBrixTram::FAt> Got = City.Tram.At(M);
		++Moments;
		bool bSame = Got.Num() == Exp.Num();
		for (int32 k = 0; bSame && k < Exp.Num(); ++k)
		{
			const TArray<TSharedPtr<FJsonValue>>& E = Exp[k]->AsArray();
			bSame = FMath::Abs(Got[k].X - E[0]->AsNumber()) < .01 && Got[k].bStopped == E[1]->AsBool() && FMath::IsNearlyEqual(Got[k].Run, E[2]->AsNumber());
			++Trams;
		}
		if (!bSame) { if (Wrong < 8) AddError(FString::Printf(TEXT("minute %.2f: not the trams the slice has (%d, the slice %d)"), M, Got.Num(), Exp.Num())); ++Wrong; }
	}
	TestTrue(TEXT("trams were checked"), Moments > 1000 && Trams > 500);
	return Wrong == 0;
}

// CITY-041: players can ride the tram, as a bus is ridden (CITY-092). Through a whole day, at every moment a tram stands at a
// stop on its way through town: someone beside it on the platform can get on, someone across the street or along it beyond
// its ends cannot, nor can anyone while it moves, nor at the last stop (it goes on out of town). Once on, its run is found
// at each later moment, carrying them east; it next stands at the next stop, where they can get off onto that stop's
// platform; after the last stop it can no longer be ridden. The player's bench seat is one of the twelve and no rider's.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY041_PlayersRideTheTram, "BrixTown.World.CITY041_players_ride_the_tram",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY041_PlayersRideTheTram::RunTest(const FString&)
{
	FBrixCity City;
	FString Error;
	if (!TestTrue(TEXT("the starting city loads: ") + Error, LoadBrixCity(StartingCityDir(), City, Error))) return false;
	const FBrixTram& T = City.Tram;
	if (!TestTrue(TEXT("the line has its stops"), T.Stops.Num() >= 2)) return false;
	TArray<FBox2D> Platforms;   // the tram stops' lots
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) if (L.Kind == TEXT("stop")) Platforms.Add(FBox2D(L.Min, L.Max));
	auto OnPlatform = [&](const FVector2D& P) { for (const FBox2D& Bx : Platforms) if (Bx.IsInside(P)) return true; return false; };
	int32 Boarded = 0, Rides = 0, Wrong = 0;
	TSet<double> Done;
	for (double M = T.First - 30; M < T.Last + 60; M += .05)
	{
		double D = 0;
		for (const FBrixTram::FAt& A : T.At(M))
		{
			const FVector2D Beside(A.X + 1.5, T.Z - T.HalfWidth - .8), Across(A.X, T.Z + 8), Beyond(A.X + T.HalfLength + 4, T.Z);
			const bool bLast = FMath::Abs(A.X - T.Stops.Last()) < .01;
			const int32 S = T.Standing(M, Beside, 1.5, D);
			const bool bCan = S >= 0 && FMath::IsNearlyEqual(T.At(M)[S].Run, A.Run);
			if (bCan != (A.bStopped && !bLast)) { if (++Wrong <= 6) AddError(FString::Printf(TEXT("minute %.2f, the tram of run %.0f at x %.1f %s: %s"), M, A.Run, A.X, A.bStopped ? TEXT("standing") : TEXT("moving"), bCan ? TEXT("could be boarded") : TEXT("could not be boarded"))); continue; }
			if (T.Standing(M, Across, 1.5, D) >= 0 || T.Standing(M, Beyond, 1.5, D) >= 0) { if (++Wrong <= 6) AddError(FString::Printf(TEXT("minute %.2f: boarded from across the street or past its end"), M)); }
			if (!bCan || Done.Contains(A.Run)) continue;
			Done.Add(A.Run);   // ride this run from here to the next stop
			++Boarded;
			const int32 From = T.Stops.IndexOfByPredicate([&](double X) { return FMath::Abs(X - A.X) < .01; });
			double Mi = M, LastX = A.X;
			bool bLeft = false, bArrived = false;
			while (Mi < M + 60)
			{
				Mi += .02;
				if (!T.Rideable(Mi, A.Run)) break;
				const TOptional<FBrixTram::FAt> R = T.Find(Mi, A.Run);
				if (!R || R->X < LastX - 1e-6) { if (++Wrong <= 6) AddError(TEXT("the tram ridden went back or was lost")); break; }
				LastX = R->X;
				if (!R->bStopped) { bLeft = true; continue; }
				if (bLeft)
				{
					bArrived = From >= 0 && T.Stops.IsValidIndex(From + 1) && FMath::Abs(R->X - T.Stops[From + 1]) < .01;
					if (!bArrived && ++Wrong <= 6) AddError(FString::Printf(TEXT("run %.0f: boarded at stop %d, next stood at x %.1f, not the next stop"), A.Run, From, R->X));
					if (!OnPlatform(T.Alight(R->X)) && ++Wrong <= 6) AddError(FString::Printf(TEXT("getting off at x %.1f: (%.1f, %.1f) is not on the stop"), R->X, T.Alight(R->X).X, T.Alight(R->X).Y));
					break;
				}
			}
			if (bArrived) ++Rides;
		}
	}
	// every stop's platform is where you step down, and the player's seat is a bench seat apart from the riders' (0 to 10)
	for (const double X : T.Stops) if (!OnPlatform(T.Alight(X)) && ++Wrong <= 8) AddError(FString::Printf(TEXT("the stop at x %.1f: stepping down at (%.1f, %.1f) is not on it"), X, T.Alight(X).X, T.Alight(X).Y));
	for (int32 k = 0; k < 11; ++k) if (FVector2D::Distance(T.Seat(0, k), T.Seat(0, T.PlayerSeat)) < .5) { AddError(TEXT("the player's seat is a rider's")); ++Wrong; }
	const FVector2D Ps = T.Seat(0, T.PlayerSeat);
	TestTrue(TEXT("the player's seat is inside the tram"), FMath::Abs(Ps.X) < T.HalfLength - .5 && FMath::Abs(Ps.Y - T.Z) < T.HalfWidth);
	TestTrue(FString::Printf(TEXT("trams boarded (%d) and ridden to the next stop (%d) all day"), Boarded, Rides), Boarded > 50 && Rides == Boarded);
	TestEqual(TEXT("a moving tram is never ridden after the last stop"), T.Rideable(T.First + 30, -1), false);
	return Wrong == 0;
}
#endif
