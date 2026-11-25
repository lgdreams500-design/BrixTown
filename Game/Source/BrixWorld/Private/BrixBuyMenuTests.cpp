// UI-021: what a thing shows in a buy menu in the Unreal build is what world.buy_menu_shows (Tools/worldgen/world.py) says,
// for every thing a menu can show (Game/Data/life.json buy_menu.checks); every stand-in it names can be drawn.
#include "BrixBuyMenu.h"
#include "BrixEconomy.h"
#include "BrixGoods.h"

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
	TSharedPtr<FBrixGoodsModels> Models = MakeShared<FBrixGoodsModels>();
	if (!TestTrue(TEXT("the goods' models load (Game/Data/goods_models.json): ") + Error, LoadBrixGoodsModels(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/goods_models.json"))), *Models, Error))) return false;
	B.Models = Models;
	int32 Checked = 0, Drawn = 0, Modelled = 0;
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
		if (S.Kind == TEXT("piece") && Models->Find(S.Name))   // a good with its model shows the model (FURN-065; buymenu.js meshOf())
		{
			const TOptional<FBrixThingMesh> M = B.MeshFor(Thing, TEXT("olive"));
			const FBrixGoodsModel& G = *Models->Find(S.Name);
			if (!M || M->bStandIn || M->Num() != G.Lods[0].I.Num() / 3) AddError(Thing + TEXT(": does not show its model ") + S.Name);
			else
			{
				++Modelled;
				const FVector3f Fit = M->Hi - M->Lo;   // within its size (u across, v front to back, y up), as mall_goods_31.py made it
				if (Fit.X > G.Size.X + .004 || Fit.Z > G.Size.Y + .004 || Fit.Y > G.Size.Z + .004) AddError(FString::Printf(TEXT("%s: %.3f x %.3f x %.3f m, bigger than its size"), *Thing, Fit.X, Fit.Z, Fit.Y));
				if (M->Lo.Y < -.004f || M->Lo.Y > .01f) AddError(Thing + TEXT(": its model does not stand on the floor"));
			}
		}
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
	TestEqual(TEXT("the mall's goods for the home shown as their models"), Modelled, 49);
	// two colours of a model are the piece's own (buymenu.js: hash(piece, "a0"), hash(piece, "a1")): never the same two
	const FBrixThingMesh Guitar = B.ModelMesh(TEXT("g_guitar"));
	TestTrue(TEXT("the guitar is drawn from its model"), Guitar.Num() > 100 && !Guitar.bStandIn);
	const TArray<FVector3f> Cg = Models->Colours(TEXT("g_guitar"));
	TestTrue(TEXT("its two open colours differ"), Models->A0 >= 0 && Models->A1 >= 0 && !Cg[Models->A0].Equals(Cg[Models->A1]));
	// a parcel is four boxes, twelve triangles each (buy_menu.json shapes)
	const FBrixThingMesh Parcel = B.ShapeMesh(TEXT("parcel"), FLinearColor::Red, FLinearColor::White);
	TestEqual(TEXT("a parcel's triangles"), Parcel.Num(), 4 * 12);
	TestTrue(TEXT("a parcel stands on the floor"), FMath::IsNearlyZero(Parcel.Lo.Y, 1e-4f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUI020_DotAndChipIcons, "BrixTown.World.UI020_dots_and_chips_show_the_slices_marks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FUI020_DotAndChipIcons::RunTest(const FString&)
{
	FBrixLifeData Life;
	FBrixBuyMenuRules B;
	FString Error;
	if (!TestTrue(TEXT("life.json loads: ") + Error, LoadBrixLife(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/life.json"))), Life, Error))) return false;
	if (!TestTrue(TEXT("the buy menu's rules load: ") + Error, LoadBrixBuyMenu(Life.Json, B, Error))) return false;
	for (const auto& G : B.Goes)   // every place a thing goes has its mark, as buymenu.js ICON has one for each
		if (!B.Icons.Contains(G.Key)) AddError(TEXT("no mark for ") + G.Key);
	TestEqual(TEXT("the marks of buymenu.js"), B.Icons.Num(), 5);
	const int32 Px = 32;
	auto At = [Px](const TArray<float>& C, float x, float y) { return C[FMath::Clamp((int32)(y / 16.f * Px), 0, Px - 1) * Px + FMath::Clamp((int32)(x / 16.f * Px), 0, Px - 1)]; };
	// points inside and outside each mark, read off its path in buymenu.js (16 units across)
	struct FPt { const TCHAR* Mark; float X, Y; bool bIn; const TCHAR* What; };
	const FPt Pts[] = {
		{ TEXT("home"), 8, 4, true, TEXT("the roof") }, { TEXT("home"), 5, 12, true, TEXT("the wall") }, { TEXT("home"), 8, 12.5f, false, TEXT("the door") }, { TEXT("home"), 1.5f, 1.5f, false, TEXT("the sky") },
		{ TEXT("wear"), 8, 10, true, TEXT("the body") }, { TEXT("wear"), 2.2f, 4.8f, true, TEXT("a sleeve") }, { TEXT("wear"), 8, 2.5f, false, TEXT("the neck") }, { TEXT("wear"), 2, 12, false, TEXT("under a sleeve") },
		{ TEXT("carry"), 8, 10, true, TEXT("the bag") }, { TEXT("carry"), 8, 3, true, TEXT("the top of the handle") }, { TEXT("carry"), 8, 4.6f, false, TEXT("inside the handle") }, { TEXT("carry"), 1, 2, false, TEXT("beside it") },
		{ TEXT("eat"), 7, 9, true, TEXT("the cup") }, { TEXT("eat"), 14, 7.7f, true, TEXT("the handle") }, { TEXT("eat"), 12.6f, 7.7f, false, TEXT("inside the handle") }, { TEXT("eat"), 7, 2, false, TEXT("over the cup") },
		{ TEXT("place"), 3.5f, 5, true, TEXT("the back") }, { TEXT("place"), 8, 9.5f, true, TEXT("the seat") }, { TEXT("place"), 8, 5, false, TEXT("over the seat") }, { TEXT("place"), 8, 13, false, TEXT("between the legs") },
	};
	for (const FPt& P : Pts)
	{
		const TArray<FBrixIconPath>* I = B.Icons.Find(P.Mark);
		if (!I) continue;
		const TArray<float> C = BrixIconCoverage(*I, Px);
		const float V = At(C, P.X, P.Y);
		if (P.bIn ? V < .75f : V > .25f) AddError(FString::Printf(TEXT("%s: %s (%.1f, %.1f) is %.2f covered"), P.Mark, P.What, P.X, P.Y, V));
	}
	// the outlines: the house is one closed ring of eleven corners and back to the first; the bag's handle one open curve
	const TArray<TArray<FVector2f>> House = BrixIconOutlines(B.Icons.FindRef(TEXT("home")).Num() ? B.Icons[TEXT("home")][0].D : FString());
	TestEqual(TEXT("the house is one ring"), House.Num(), 1);
	if (House.Num()) { TestEqual(TEXT("of eleven corners, closed"), House[0].Num(), 12); TestTrue(TEXT("back to its start"), House[0][0].Equals(House[0].Last())); }
	if (const TArray<FBrixIconPath>* Bag = B.Icons.Find(TEXT("carry")); Bag && Bag->Num() == 2)
	{
		TestEqual(TEXT("the handle is stroked 1.6 wide"), (*Bag)[1].Stroke, 1.6f);
		const TArray<TArray<FVector2f>> H = BrixIconOutlines((*Bag)[1].D);
		TestTrue(TEXT("the handle's curve peaks at y 3 over x 8"), H.Num() == 1 && H[0].ContainsByPredicate([](const FVector2f& P) { return P.Equals(FVector2f(8, 3), .01f); }));
	}
	return true;
}

#endif
