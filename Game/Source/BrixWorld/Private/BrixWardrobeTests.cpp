// CHR-015, CHR-016: the wardrobe keeps a few named outfits, and the clothes the mall's outlets sell set only their parts.
#include "BrixWardrobe.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString WProj(const TCHAR* P) { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), P)); }
	bool LoadRules(FAutomationTestBase& T, FBrixDesignerRules& D)
	{
		FString Error;
		return T.TestTrue(TEXT("the designer's rules load: ") + Error, LoadBrixDesigner(WProj(TEXT("../World/rules/designer.json")), WProj(TEXT("../World/rules/settings.json")), D, Error));
	}
}

// CHR-015: what is worn is saved as a named outfit, up to wardrobe.max_outfits; saving under a name already there changes
// that one; a name is 1 to 16 letters; one can be forgotten; and wearing one changes only its parts. The list is kept in a
// save as JSON and read back the same, dropping any that is not sound.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCHR015_TheWardrobeHoldsAFewOutfits, "BrixTown.World.CHR015_the_wardrobe_holds_a_few_outfits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCHR015_TheWardrobeHoldsAFewOutfits::RunTest(const FString&)
{
	FBrixDesignerRules D;
	if (!LoadRules(*this, D)) return false;
	FBrixWardrobe W;
	FString Why;
	FBrixDesign V = D.Default;
	TestFalse(TEXT("an outfit needs a name"), W.Save(D, V, TEXT("   "), Why));
	TestFalse(TEXT("a name of at most 16 letters"), W.Save(D, V, FString::ChrN(D.NameMax + 1, TEXT('x')), Why));
	for (int32 k = 0; k < D.MaxOutfits; ++k)
	{
		V.Top = D.Palette[k % D.Palette.Num()].Key;
		TestTrue(FString::Printf(TEXT("outfit %d is kept"), k + 1), W.Save(D, V, FString::Printf(TEXT("Look %d"), k + 1), Why));
	}
	TestFalse(TEXT("the wardrobe is full"), W.Save(D, V, TEXT("One more"), Why));
	TestEqual(TEXT("and says so"), Why, FString::Printf(TEXT("the wardrobe holds %d outfits"), D.MaxOutfits));
	V.Hair = TEXT("bob");
	TestTrue(TEXT("saving under a name already there changes that one"), W.Save(D, V, TEXT("  Look 1 "), Why));
	TestEqual(TEXT("still as many"), W.Outfits.Num(), D.MaxOutfits);
	TestEqual(TEXT("with the new hair"), W.Find(TEXT("Look 1"))->GetStringField(TEXT("hair")), FString(TEXT("bob")));
	TestTrue(TEXT("one is forgotten"), W.Forget(TEXT("Look 2")));
	TestFalse(TEXT("and is gone"), W.Find(TEXT("Look 2")) != nullptr);
	TestTrue(TEXT("so there is room again"), W.Save(D, V, TEXT("One more"), Why));

	FBrixDesign Me = D.Default;   // someone else's body and face, wearing Look 1
	Me.Sliders.Add(TEXT("height"), 1.6);
	Me.Sliders.Add(TEXT("nose"), 1.2);
	Me.Skin = 8;
	Me.Name = TEXT("Ada");
	const FBrixDesign Before = Me;
	BrixWearOutfit(D, *W.Find(TEXT("Look 1")), Me);
	TestTrue(TEXT("wearing an outfit leaves the body, face and name"), Me.Sliders.FindRef(TEXT("height")) == 1.6 && Me.Sliders.FindRef(TEXT("nose")) == 1.2 && Me.Skin == 8 && Me.Name == TEXT("Ada"));
	TestTrue(TEXT("and changes the clothes and hair"), Me.Hair == TEXT("bob") && Me.Top == V.Top && !(Me == Before));

	FBrixWardrobe Back;
	TArray<TSharedPtr<FJsonValue>> Kept = W.ToJson();
	TSharedRef<FJsonObject> Bad = MakeShared<FJsonObject>();
	Bad->SetStringField(TEXT("name"), TEXT("Bad"));
	Bad->SetNumberField(TEXT("height"), 2.5);
	Kept.Add(MakeShared<FJsonValueObject>(Bad));
	Back.Load(D, Kept);
	TestEqual(TEXT("read back from the save, the bad one left out"), Back.Names(), W.Names());
	return true;
}

// CHR-016: every good the outlets sell to wear sets only parts of the wardrobe, each a value the designer offers; putting
// it on sets those parts and nothing else, and it is then being worn.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCHR016_ClothesSetOnlyTheirParts, "BrixTown.World.CHR016_clothes_bought_set_only_their_parts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCHR016_ClothesSetOnlyTheirParts::RunTest(const FString&)
{
	FBrixDesignerRules D;
	if (!LoadRules(*this, D)) return false;
	TMap<FString, FBrixGarment> Clothes;
	FString Error;
	if (!TestTrue(TEXT("the goods load: ") + Error, LoadBrixGarments(WProj(TEXT("Data/life.json")), Clothes, Error))) return false;
	int32 Wrong = 0;
	for (const auto& KV : Clothes)
	{
		const FBrixGarment& G = KV.Value;
		for (const FString& P : BrixGarmentProblems(D, G)) { AddError(P); ++Wrong; }
		FBrixDesign V = D.Default;
		V.Garment = TEXT("coat"); V.Top = TEXT("black"); V.BottomType = TEXT("skirt"); V.Bottom = TEXT("black"); V.Shoes = TEXT("black");
		V.Scarf.Reset(); V.Cap.Reset(); V.bBag = false;
		const FBrixDesign Before = V;
		BrixPutOn(G, V);
		if (!BrixIsWearing(V, G)) { AddError(G.Id + TEXT(": not worn once put on")); ++Wrong; }
		const TSharedRef<FJsonObject> A = BrixDesignToJson(Before), B = BrixDesignToJson(V);
		for (const auto& F : A->Values)
		{
			const FString K(F.Key);
			const bool bSame = FJsonValue::CompareEqual(*F.Value, *B->TryGetField(K));
			if (G.Parts->HasField(K) ? !FJsonValue::CompareEqual(*G.Parts->TryGetField(K), *B->TryGetField(K)) : !bSame)
			{
				AddError(G.Id + TEXT(": ") + K + (G.Parts->HasField(K) ? TEXT(" is not as it says") : TEXT(" changed, and it is not one of its parts")));
				++Wrong;
			}
		}
	}
	TestTrue(TEXT("the outlets sell clothes"), Clothes.Num() > 5);
	return Wrong == 0;
}

#endif
