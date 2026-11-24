// Automation tests for the map as data. Run headless:
//   UnrealEditor-Cmd.exe Game/BrixTown.uproject -ExecCmds="Automation RunTests BrixTown.World; Quit" -unattended -nullrhi
#include "BrixCity.h"

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

#endif
