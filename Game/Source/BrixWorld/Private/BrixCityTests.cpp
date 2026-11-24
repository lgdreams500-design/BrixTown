// Automation tests for the map as data. Run headless:
//   UnrealEditor-Cmd.exe Game/BrixTown.uproject -ExecCmds="Automation RunTests BrixTown.World; Quit" -unattended -nullrhi
#include "BrixCity.h"

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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

#endif
