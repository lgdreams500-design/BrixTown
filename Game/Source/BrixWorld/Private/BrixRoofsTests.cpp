// WLD-037, WLD-070: the wet paving and lying snow stop under a roof. The roof map covers every building's footprint up to
// its roof and nothing else: indoors is under it, the street at a building's door, the gardens and a roof's top are not.
#include "BrixRoofs.h"
#include "BrixCity.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD037_DryUnderRoofs, "BrixTown.World.WLD037_rain_and_snow_stay_out_from_under_roofs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD037_DryUnderRoofs::RunTest(const FString&)
{
	FBrixCity City;
	FString Error;
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/city_01"))), City, Error))) return false;
	auto Top = [](const FBrixLot& L) { return .12 + FMath::Max(1, L.Floors) * 3.0 - .25; };   // as ABrixTownActor gives it
	const FBrixRoofMap M = BrixRoofMap(City, .5, .5, Top);
	AddInfo(FString::Printf(TEXT("%d x %d cells of 0.5 m"), M.W, M.H));
	TestTrue(TEXT("a map of the town"), M.W > 3000 && M.H > 2500 && M.Px.Num() == M.W * M.H);
	int32 Buildings = 0, Inside = 0, RoofOut = 0, Doors = 0, DoorOut = 0, Gardens = 0, GardenOut = 0;
	for (const FBrixBlock& B : City.Blocks)
		for (const FBrixLot& L : B.Lots)
		{
			if (L.Kind == TEXT("garden")) { ++Gardens; GardenOut += M.Under(L.Centre(), .2) ? 0 : 1; continue; }
			if (L.IsOpen()) continue;
			++Buildings;
			const double H = Top(L);
			Inside += M.Under(L.Centre(), .2) && M.Under(L.Centre(), H - .6) ? 1 : 0;   // its ground floor and its top floor
			RoofOut += M.Under(L.Centre(), H + .3) ? 0 : 1;                           // its roof's top is out in the weather
			// a step out of the door: the lot's door is on its edge; half a metre on, away from its middle, is the street
			const FVector2D Out = L.Door + (L.Door - L.Centre()).GetSafeNormal() * .6;
			if (L.Door.IsZero()) continue;
			++Doors;
			DoorOut += M.Under(Out, .2) ? 0 : 1;
		}
	AddInfo(FString::Printf(TEXT("%d buildings: %d dry inside, %d roofs out in it; %d of %d doorsteps out; %d of %d gardens out"), Buildings, Inside, RoofOut, DoorOut, Doors, GardenOut, Gardens));
	TestTrue(TEXT("there are buildings"), Buildings > 900);
	TestEqual(TEXT("every building is dry inside, on every floor"), Inside, Buildings);
	TestEqual(TEXT("every roof's top is out in the weather"), RoofOut, Buildings);
	TestTrue(TEXT("the street outside a door is out in it (all but where buildings stand back to back)"), Doors > 900 && DoorOut >= Doors * 98 / 100);
	TestEqual(TEXT("every garden is out in it"), GardenOut, Gardens);
	TestFalse(TEXT("off the map: open sky"), M.Under(FVector2D(-500, -500), .1));
	return true;
}

#endif
