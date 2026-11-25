// UI-012, UI-018, QST-005: the maps are drawn from the town's data as play.js drawBase() draws them, the big map zooms and
// pans as mapZoom() does, the mark is set, cleared and reached as life.js markAt() and waypointTag() do, and the people and
// places a quest wants are those of life.js targets().
#include "BrixCity.h"
#include "BrixEconomy.h"
#include "BrixMap.h"
#include "BrixQuests.h"
#include "BrixStreets.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString MapCityDir() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/city_01"))); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUI018_TheBigMapIsTheTown, "BrixTown.World.UI018_the_big_map_is_drawn_from_the_town",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FUI018_TheBigMapIsTheTown::RunTest(const FString&)
{
	FBrixCity City;
	FBrixMapExtras X;
	FString Error;
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(MapCityDir(), City, Error))) return false;
	if (!TestTrue(TEXT("the map's own data loads: ") + Error, LoadBrixMapExtras(MapCityDir(), City, X, Error))) return false;
	TestTrue(TEXT("the roads out of town are read"), X.Links.Num() > 0 && X.Gates.Num() == 4);
	TestEqual(TEXT("the old town wall's cells"), X.Wall, FIntRect(6, 4, 14, 12));
	const FBrixMapImage M = BrixDrawMap(City, X, 2700);
	TestEqual(TEXT("the slice's scale: 2700 px over the town and 40 m"), M.K, 2700.0 / (20 * 96.0 + 40));
	// every lot shows its kind's colour at its middle, unless a tree, the tram line or a later lot covers it there
	int32 Lots = 0, Right = 0;
	TArray<FString> Wrong;
	for (const FBrixBlock& B : City.Blocks)
		for (const FBrixLot& L : B.Lots)
		{
			++Lots;
			if (M.At(L.Centre()) == BrixMapKindColour(L.Kind)) ++Right;
			else if (Wrong.Num() < 8) Wrong.Add(L.Id + TEXT(" ") + L.Kind);
		}
	AddInfo(FString::Printf(TEXT("%d of %d lots show their kind's colour at their middle"), Right, Lots));
	TestTrue(TEXT("at least 97% of the lots show their kind's colour (not: ") + FString::Join(Wrong, TEXT(", ")) + TEXT(")"), Right >= Lots * 97 / 100);
	// a street of Old Town: between the first two blocks of a row, on the street's middle line
	const double P = City.Pitch;
	TestEqual(TEXT("an Old Town street is the street's colour"), M.At(FVector2D(8 * P + 40, 6 * P)), FColor::FromHex(TEXT("4b4745")));
	TestEqual(TEXT("the tram line is drawn along Tram Street"), M.At(FVector2D(10 * P + 30, City.Tram.Z)), FColor::FromHex(TEXT("2a2a30")));
	TestEqual(TEXT("outside the town is the ground's colour"), M.At(FVector2D(-15, -15)), FColor::FromHex(TEXT("3a3d2c")));
	// the open ground (WLD-056): a wood's ground is the woods' colour, its trees darker
	int32 Woods = 0, WoodsRight = 0;
	for (int32 b = 0; b < City.Blocks.Num(); ++b)
	{
		const FBrixBlock& B = City.Blocks[b];
		if (B.Template != TEXT("woods")) continue;
		for (int32 k = 1; k < 10; ++k)
		{
			const FColor C = M.At(B.Origin + B.Extent * (k / 10.0));
			++Woods;
			if (C == FColor::FromHex(TEXT("4f6b3a")) || C.R < 0x4f) ++WoodsRight;
		}
	}
	TestTrue(TEXT("the woods are the woods' green (or a tree, or a path)"), Woods > 0 && WoodsRight >= Woods * 8 / 10);
	TestTrue(TEXT("the legend has every kind of lot and the open ground"), BrixMapLegend().Num() == 18 + 5 + 2);

	// the zoom about the pointer (mapZoom): 1 to 6 times, the point under the pointer stays put, never past the edge
	FBrixMapView V;
	const FVector2D At(300, 200), Under = V.MapAt(At);
	V.Zoom(1.25, At);
	TestTrue(TEXT("zooming keeps the point under the pointer"), V.MapAt(At).Equals(Under, 1e-6));
	for (int32 k = 0; k < 20; ++k) V.Zoom(1.25, At);
	TestEqual(TEXT("no more than six times"), V.Z, 6.0);
	V.Cx = -100;
	V.Clamp();
	TestEqual(TEXT("never past the map's left edge"), V.Cx, 450.0 / 6);
	V.Whole();
	TestTrue(TEXT("0 shows the whole town again"), V.Z == 1 && V.Cx == 450 && V.Cy == 450);
	for (int32 k = 0; k < 20; ++k) V.Zoom(1 / 1.25, At);
	TestEqual(TEXT("no less than the whole town"), V.Z, 1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUI012_TheMark, "BrixTown.World.UI012_the_mark_is_set_cleared_and_reached",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FUI012_TheMark::RunTest(const FString&)
{
	FBrixMark K;
	TestEqual(TEXT("a click marks the place"), K.MarkAt(FVector2D(100, 100), TEXT("The Bank"), false), FString(TEXT("Marked: The Bank.")));
	TestEqual(TEXT("with how far it is"), K.TagText(FVector2D(100, 220)), FString(TEXT("◆ The Bank · 120 m")));
	TestEqual(TEXT("in km beyond 1000 m"), K.TagText(FVector2D(100, 1350)), FString(TEXT("◆ The Bank · 1.3 km")));
	TestEqual(TEXT("a click again on it clears it"), K.MarkAt(FVector2D(104, 100), FString(), false), FString(TEXT("Mark cleared.")));
	TestFalse(TEXT("cleared"), K.bSet);
	K.MarkAt(FVector2D(100, 100), FString(), false);
	TestEqual(TEXT("a place with no name is the player's mark"), K.Name, FString(TEXT("Your mark")));
	TestEqual(TEXT("marked from Properties, a mark near the old one moves it"), K.MarkAt(FVector2D(103, 100), TEXT("Home"), true), FString(TEXT("Marked: Home.")));
	TestFalse(TEXT("not reached 5 m off"), K.Reached(FVector2D(108, 100)));
	TestTrue(TEXT("reached within 4 m"), K.Reached(FVector2D(106, 100)));
	TestFalse(TEXT("and then gone"), K.bSet || K.Reached(FVector2D(103, 100)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQST005_QuestPeopleAreMarked, "BrixTown.World.QST005_the_people_and_places_a_quest_wants_are_marked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FQST005_QuestPeopleAreMarked::RunTest(const FString&)
{
	FBrixCity City;
	FBrixLifeData Life;
	TArray<FBrixEconLot> Lots;
	FBrixQuestRules QR;
	TArray<FBrixQuestResident> Res;
	FString Error;
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(MapCityDir(), City, Error))) return false;
	if (!TestTrue(TEXT("life.json loads: ") + Error, LoadBrixLife(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/life.json"))), Life, Error))) return false;
	if (!TestTrue(TEXT("the lots: ") + Error, BrixEconLots(City, Life, Lots, Error))) return false;
	if (!TestTrue(TEXT("the quests: ") + Error, LoadBrixQuests(Life.Json, QR, Res, Error))) return false;
	FBrixEconomy E(Life, Lots);
	FBrixQuests Q(QR, E, Res);
	TestTrue(TEXT("nothing wanted before a game starts"), BrixQuestTargets(Q, QR, E).FilterByPredicate([](const FBrixMapTarget& T) { return !T.bOffer; }).Num() == 0);
	TestTrue(TEXT("a new game starts the first line"), Q.NewGame());
	const TArray<FBrixMapTarget> T = BrixQuestTargets(Q, QR, E);
	const FBrixQuestStage* S = Q.Active.Num() ? Q.Stage(Q.Active[0].Key) : nullptr;
	if (!TestNotNull(TEXT("its first stage"), S)) return false;
	const FBrixGoal& G = S->Goal;
	bool bFound = false;
	for (const FBrixMapTarget& X : T)
	{
		if (G.Kind == TEXT("talk")) bFound |= X.Resident == Q.ResidentIndex(G.Value) && !X.bOffer;
		else if (G.Kind == TEXT("go")) bFound |= X.Lot == E.LotIndex.FindRef(G.Value, -1);
		else if (G.Kind == TEXT("bank")) bFound |= X.Lot == E.BankLot;
		else if (G.Kind == TEXT("near")) bFound |= X.Pos.Equals(G.Near);
		else bFound = true;
	}
	TestTrue(FString::Printf(TEXT("the first stage's goal (%s %s) is marked"), *G.Kind, *G.Value), bFound);
	for (const FBrixMapTarget& X : T)
		if (X.bOffer) TestTrue(TEXT("one with a line to give offers it"), X.Resident >= 0 && QR.Quests.ContainsByPredicate([&](const FBrixQuestLine& L) { return Q.ResidentIndex(L.Giver) == X.Resident && Q.Offers(L.Giver).Contains(L.Id); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUI018_TheMapsTreesAreToday, "BrixTown.World.UI018_the_maps_trees_are_todays_crowns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FUI018_TheMapsTreesAreToday::RunTest(const FString&)
{
	// play.js drawBase(): every tree of D.trees that has not fallen, a disc of max(.9, r * k * .8) pixels in its crown's radius today
	FBrixCity City;
	FBrixMapExtras X;
	FBrixStreetsToday Today;
	FString Error;
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(MapCityDir(), City, Error))) return false;
	if (!TestTrue(TEXT("the map's own data loads: ") + Error, LoadBrixMapExtras(MapCityDir(), City, X, Error))) return false;
	if (!TestTrue(TEXT("today's trees load (Game/Data/streets.json): ") + Error,
		LoadBrixStreetsToday(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/streets.json"))), Today, Error))) return false;
	const FBrixMapImage Fixed = BrixDrawMap(City, X, 2700);
	BrixMapTreesToday(Today.Trees, X);
	int32 Standing = 0;
	for (const FBrixTreeToday& T : Today.Trees) Standing += T.bFallen ? 0 : 1;
	TestEqual(TEXT("every standing tree is on the map, no fallen one"), X.Trees.Num(), Standing);
	const FBrixMapImage M = BrixDrawMap(City, X, 2700);
	const FColor Tree = FColor::FromHex(TEXT("2f4524"));
	int32 Big = 0, Reach = 0, FixedReach = 0;
	for (const FBrixTreeToday& T : Today.Trees)
	{
		if (T.bFallen || T.R < 5) continue;
		++Big;
		const FVector2D Edge(T.X + T.R * .8 * .65, T.Z);   // well inside the crown's disc on the map, outside the old 2.5 m one
		Reach += M.At(Edge) == Tree ? 1 : 0;
		FixedReach += Fixed.At(Edge) == Tree ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("%d big crowns: %d reach their own size on the map, %d did at the one 2.5 m"), Big, Reach, FixedReach));
	TestTrue(TEXT("there are big crowns today"), Big > 100);
	TestTrue(TEXT("a big crown is drawn as big as it is today"), Reach >= Big * 97 / 100);
	TestTrue(TEXT("(and not at the one radius as before)"), FixedReach < Big * 80 / 100);
	return true;
}

#endif
