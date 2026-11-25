// UI-021: what a thing shows in a buy menu in the Unreal build is what world.buy_menu_shows (Tools/worldgen/world.py) says,
// for every thing a menu can show (Game/Data/life.json buy_menu.checks); every stand-in it names can be drawn.
#include "BrixBuyMenu.h"
#include "BrixEconomy.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUI021_ThingsShowWhatPythonSays, "BrixTown.World.UI021_every_thing_shows_what_python_says",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FUI021_ThingsShowWhatPythonSays::RunTest(const FString&)
{
	FBrixLifeData Life;
	FBrixBuyMenuRules B;
	FString Error;
	if (!TestTrue(TEXT("life.json loads: ") + Error, LoadBrixLife(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/life.json"))), Life, Error))) return false;
	if (!TestTrue(TEXT("the buy menu's rules load: ") + Error, LoadBrixBuyMenu(Life.Json, B, Error))) return false;
	int32 Checked = 0, Drawn = 0;
	for (const auto& V : Life.Json->GetObjectField(TEXT("buy_menu"))->GetObjectField(TEXT("checks"))->Values)
	{
		const FString Thing(V.Key.ToView());
		const FBrixShows S = B.Shows(Thing);
		FString Want, Got = S.Kind.IsEmpty() ? FString(TEXT("null")) : S.Kind + TEXT(" ") + (S.Name.IsEmpty() ? FString(TEXT("null")) : S.Name);
		if (S.Kind == TEXT("shape")) Got += TEXT(" ") + (S.Colour ? *S.Colour : FString(TEXT("null")));
		if (V.Value->IsNull()) Want = TEXT("null");
		else for (const auto& X : V.Value->AsArray()) Want += (Want.IsEmpty() ? TEXT("") : TEXT(" ")) + (X->IsNull() ? FString(TEXT("null")) : X->AsString());
		if (Got != Want) AddError(FString::Printf(TEXT("%s shows %s, Python says %s"), *Thing, *Got, *Want));
		++Checked;
		if (S.Kind == TEXT("shape"))
		{
			const TOptional<FBrixThingMesh> M = B.MeshFor(Thing, TEXT("olive"));
			if (!M || M->Num() < 12) AddError(Thing + TEXT(": its stand-in ") + S.Name + TEXT(" cannot be drawn"));
			else
			{
				++Drawn;
				if (M->R > 1.f) AddError(Thing + TEXT(": a stand-in more than a metre round"));
			}
		}
	}
	TestTrue(TEXT("every thing a menu can show was checked"), Checked > 200);
	TestTrue(TEXT("stand-ins drawn"), Drawn > 100);
	// a parcel is four boxes, twelve triangles each (buy_menu.json shapes)
	const FBrixThingMesh Parcel = B.ShapeMesh(TEXT("parcel"), FLinearColor::Red, FLinearColor::White);
	TestEqual(TEXT("a parcel's triangles"), Parcel.Num(), 4 * 12);
	TestTrue(TEXT("a parcel stands on the floor"), FMath::IsNearlyZero(Parcel.Lo.Y, 1e-4f));
	return true;
}

#endif
