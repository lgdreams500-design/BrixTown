// CHR-001, CHR-002, CHR-013, CHR-014: the character designer's values make the slice's look, stay in their ranges, and an
// outfit changes only what the wardrobe may. Values from Tests/values/designer_city_01.json (Tools/unreal/people_values.py:
// people.js design() and draw() run in headless Edge, world.look_problems() and world.outfit_problems() in Python).
#include "BrixDesigner.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString Proj(const TCHAR* P) { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), P)); }
	bool LoadDesignerFor(FAutomationTestBase& T, FBrixDesignerRules& D, TSharedPtr<FJsonObject>& Want)
	{
		FString Error, Text;
		if (!T.TestTrue(TEXT("the designer's rules load: ") + Error, LoadBrixDesigner(Proj(TEXT("../World/rules/designer.json")), Proj(TEXT("../World/rules/settings.json")), D, Error))) return false;
		if (!T.TestTrue(TEXT("the test values are there"), FFileHelper::LoadFileToString(Text, *Proj(TEXT("../Tests/values/designer_city_01.json"))))) return false;
		return T.TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Want) && Want.IsValid());
	}
	FVector3f Col(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A;
		if (!O->TryGetArrayField(Key, A) || A->Num() < 3) return FVector3f(-1);
		return FVector3f((float)(*A)[0]->AsNumber(), (float)(*A)[1]->AsNumber(), (float)(*A)[2]->AsNumber());
	}
	TArray<FString> Strings(const TSharedPtr<FJsonValue>& V)
	{
		TArray<FString> Out;
		for (const TSharedPtr<FJsonValue>& X : V->AsArray()) Out.Add(X->AsString());
		return Out;
	}
	void ReadFace(const TSharedPtr<FJsonObject>& Fo, FBrixFace& Face)
	{
		const TSharedPtr<FJsonObject> Mo = Fo->GetObjectField(TEXT("mouth"));
		Face.Mouth.Width = Mo->GetNumberField(TEXT("width")); Face.Mouth.Opening = Mo->GetNumberField(TEXT("opening"));
		Face.Mouth.Round = Mo->GetNumberField(TEXT("round")); Face.Mouth.Teeth = (int32)Mo->GetNumberField(TEXT("teeth"));
		Mo->TryGetBoolField(TEXT("tongue"), Face.Mouth.bTongue);
		Face.Curve = Fo->GetNumberField(TEXT("curve")); Face.Lid = Fo->GetNumberField(TEXT("lid"));
		Face.Squint = Fo->GetNumberField(TEXT("squint")); Face.Open = Fo->GetNumberField(TEXT("open")); Face.Nod = Fo->GetNumberField(TEXT("nod"));
		auto Pair = [&](const TCHAR* K, double Out[2]) { const TArray<TSharedPtr<FJsonValue>>& A = Fo->GetArrayField(K); Out[0] = A[0]->AsNumber(); Out[1] = A[1]->AsNumber(); };
		Pair(TEXT("brow_l"), Face.BrowL); Pair(TEXT("brow_r"), Face.BrowR); Pair(TEXT("gaze"), Face.Gaze);
	}
}

// CHR-001, CHR-002: a character is designer values; BrixDesignLook makes the look people.js design() makes from them (sizes,
// colours, clothes, hair, the walk and the face values face_w, eye_size, eye_open, brow, nose and lips), and the body drawn
// from it standing, walking, further off and talking is people.js's to 1 mm. The values go to JSON and back unchanged.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCHR001_DesignerMakesTheSlicesLook, "BrixTown.World.CHR001_the_designer_makes_the_slices_look",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCHR001_DesignerMakesTheSlicesLook::RunTest(const FString&)
{
	FBrixDesignerRules D;
	TSharedPtr<FJsonObject> Want;
	if (!LoadDesignerFor(*this, D, Want)) return false;
	FBrixPeopleRules People;
	FString Error;
	if (!TestTrue(TEXT("the people's tables load: ") + Error, LoadBrixPeopleRules(Proj(TEXT("Data/people.json")), People, Error))) return false;

	const TArray<TSharedPtr<FJsonValue>>& Designs = Want->GetArrayField(TEXT("designs"));
	const TArray<TSharedPtr<FJsonValue>>& LooksW = Want->GetArrayField(TEXT("looks"));
	TArray<FBrixLook> Looks;
	int32 Wrong = 0;
	auto Near = [](const FVector3f& A, const FVector3f& B) { return A.Equals(B, 1e-4f); };
	for (int32 n = 0; n < Designs.Num(); ++n)
	{
		FBrixDesign V;
		if (!TestTrue(FString::Printf(TEXT("design %d reads"), n), BrixDesignFromJson(*Designs[n]->AsObject(), V))) { ++Wrong; Looks.Add(FBrixLook()); continue; }
		FBrixDesign Back;
		BrixDesignFromJson(*BrixDesignToJson(V), Back);
		if (!(Back == V)) { AddError(FString::Printf(TEXT("design %d: not the same after JSON and back"), n)); ++Wrong; }
		const FBrixLook L = BrixDesignLook(V, D);
		Looks.Add(L);
		const TSharedPtr<FJsonObject> W = LooksW[n]->AsObject();
		auto Opt = [&](const TCHAR* Key, const TOptional<FVector3f>& Got) { const FVector3f E = Col(W, Key); return E.X < 0 ? !Got.IsSet() : Got.IsSet() && Near(*Got, E); };
		bool bBag = false, bTash = false;
		W->TryGetBoolField(TEXT("bag"), bBag);
		W->TryGetBoolField(TEXT("tash"), bTash);
		const TSharedPtr<FJsonObject> Fm = W->GetObjectField(TEXT("fm"));
		const bool bFace = FMath::IsNearlyEqual(L.FaceW, Fm->GetNumberField(TEXT("face_w"))) && FMath::IsNearlyEqual(L.EyeSize, Fm->GetNumberField(TEXT("eye_size")))
			&& FMath::IsNearlyEqual(L.EyeOpen, Fm->GetNumberField(TEXT("eye_open"))) && FMath::IsNearlyEqual(L.Brow, Fm->GetNumberField(TEXT("brow")))
			&& FMath::IsNearlyEqual(L.Nose, Fm->GetNumberField(TEXT("nose"))) && FMath::IsNearlyEqual(L.Lips, Fm->GetNumberField(TEXT("lips")));
		const bool bSame = FMath::IsNearlyEqual(L.S, W->GetNumberField(TEXT("S")), 1e-9) && FMath::IsNearlyEqual(L.W, W->GetNumberField(TEXT("W")), 1e-9)
			&& Near(L.Skin, Col(W, TEXT("skin"))) && Near(L.HairC, Col(W, TEXT("hairC"))) && Near(L.Eyes, Col(W, TEXT("eyes")))
			&& Near(L.Top, Col(W, TEXT("top"))) && Near(L.Bottom, Col(W, TEXT("bottom"))) && Near(L.Shoes, Col(W, TEXT("shoes")))
			&& Opt(TEXT("apron"), L.Apron) && Opt(TEXT("cap"), L.Cap) && Opt(TEXT("scarf"), L.Scarf)
			&& L.Hair == W->GetStringField(TEXT("hair")) && L.Garment == W->GetStringField(TEXT("g")) && L.Legs == W->GetStringField(TEXT("legs"))
			&& L.bBag == bBag && L.bTash == bTash && L.SigName == W->GetStringField(TEXT("sig")) && L.Seed == (int32)W->GetNumberField(TEXT("seed"));
		if (!bSame || !bFace) { AddError(FString::Printf(TEXT("design %d: not the look people.js design() gives (%s)"), n, !bFace ? TEXT("the face values") : TEXT("sizes, colours or clothes"))); ++Wrong; }
	}

	ResetBrixWalkHeights();   // people_values.py drew these from a fresh start too
	FBrixBodyMesh Mesh;
	TArray<FBrixFarBox> Boxes;
	int32 Bodies = 0, Faces = 0;
	for (const TSharedPtr<FJsonValue>& BV : Want->GetArrayField(TEXT("bodies")))
	{
		const TSharedPtr<FJsonObject> B = BV->AsObject();
		const int32 k = (int32)B->GetNumberField(TEXT("design"));
		FBrixDrawOptions O;
		O.Pose = (int32)B->GetNumberField(TEXT("pose"));
		O.Phase = B->GetNumberField(TEXT("ph"));
		O.Seconds = B->GetNumberField(TEXT("t"));
		O.Lod = (int32)B->GetNumberField(TEXT("lod"));
		O.FaceFade = B->GetNumberField(TEXT("ff"));
		FBrixFace Face;
		const TSharedPtr<FJsonObject>* Fo = nullptr;
		if (B->TryGetObjectField(TEXT("face"), Fo)) { ReadFace(*Fo, Face); O.Face = &Face; ++Faces; }
		Mesh.Reset();
		Boxes.Reset();
		DrawBrixPerson(People, Looks[k], FVector(B->GetNumberField(TEXT("x")), B->GetNumberField(TEXT("y")), B->GetNumberField(TEXT("z"))), B->GetNumberField(TEXT("yaw")), O, Mesh, &Boxes);
		const FString What = FString::Printf(TEXT("design %d, pose %d, lod %d%s"), k, O.Pose, O.Lod, O.Face ? TEXT(", talking") : TEXT(""));
		++Bodies;
		if (Mesh.Index.Num() != (int32)B->GetNumberField(TEXT("vertices")))
		{
			AddError(FString::Printf(TEXT("%s: %d vertices, people.js draws %d"), *What, Mesh.Index.Num(), (int32)B->GetNumberField(TEXT("vertices"))));
			++Wrong;
			continue;
		}
		double Worst = 0;
		for (const TSharedPtr<FJsonValue>& SV : B->GetArrayField(TEXT("samples")))
		{
			const TArray<TSharedPtr<FJsonValue>>& S = SV->AsArray();
			const FVector3f E((float)S[1]->AsNumber(), (float)S[2]->AsNumber(), (float)S[3]->AsNumber());
			Worst = FMath::Max(Worst, (double)FVector3f::Distance(Mesh.Pos[Mesh.Index[(int32)S[0]->AsNumber()]], E));
		}
		if (Worst > 1e-3) { AddError(FString::Printf(TEXT("%s: a vertex %.4f m from where people.js puts it"), *What, Worst)); ++Wrong; }
		if (B->GetArrayField(TEXT("boxes")).Num() != Boxes.Num()) { AddError(What + TEXT(": not the far figure people.js draws")); ++Wrong; }
	}
	TestTrue(TEXT("every hair style was designed, and drawn standing, walking and talking"), Looks.Num() > D.Hair.Num() && Bodies >= 4 * Looks.Num() && Faces == Looks.Num());
	return Wrong == 0;
}

// CHR-013: the designer keeps every value inside designer.json's ranges and choices; what is wrong with a character is
// found as world.look_problems() finds it, in its words (the default and every designed case have nothing wrong).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCHR013_DesignerValuesStayInRange, "BrixTown.World.CHR013_designer_values_stay_in_range",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCHR013_DesignerValuesStayInRange::RunTest(const FString&)
{
	FBrixDesignerRules D;
	TSharedPtr<FJsonObject> Want;
	if (!LoadDesignerFor(*this, D, Want)) return false;
	TestEqual(TEXT("the default can be played"), BrixLookProblems(D, D.Default).Num(), 0);
	const FBrixDesignerSlider* H = D.SliderRule(TEXT("height"));
	TestTrue(TEXT("height stays in the cast range 1.58 to 1.80 m (ART_DIRECTION 3.2)"), H && H->Min >= 1.58 && H->Max <= 1.80);
	int32 Wrong = 0, Bad = 0;
	for (const TSharedPtr<FJsonValue>& CV : Want->GetArrayField(TEXT("checks")))
	{
		const TArray<TSharedPtr<FJsonValue>>& C = CV->AsArray();
		FBrixDesign V;
		BrixDesignFromJson(*C[0]->AsObject(), V);
		const TArray<FString> Got = BrixLookProblems(D, V), Exp = Strings(C[1]);
		Bad += Exp.Num() > 0;
		if (Got != Exp) { AddError(FString::Printf(TEXT("%s: found [%s], Python finds [%s]"), *V.Name, *FString::Join(Got, TEXT("; ")), *FString::Join(Exp, TEXT("; ")))); ++Wrong; }
	}
	TestTrue(TEXT("values wrong in every way were tried"), Bad > 25);
	return Wrong == 0;
}

// CHR-014: an outfit is only the clothes and hair the designer offers (settings.json wardrobe.parts), and a name; putting
// one on changes those parts and nothing else (never the body, the face or the name).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCHR014_AnOutfitIsOnlyClothes, "BrixTown.World.CHR014_an_outfit_is_only_clothes_and_hair_the_designer_offers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCHR014_AnOutfitIsOnlyClothes::RunTest(const FString&)
{
	FBrixDesignerRules D;
	TSharedPtr<FJsonObject> Want;
	if (!LoadDesignerFor(*this, D, Want)) return false;
	int32 Wrong = 0;
	for (const TSharedPtr<FJsonValue>& CV : Want->GetArrayField(TEXT("outfits")))
	{
		const TArray<TSharedPtr<FJsonValue>>& C = CV->AsArray();
		const TArray<FString> Got = BrixOutfitProblems(D, *C[0]->AsObject()), Exp = Strings(C[1]);
		if (Got != Exp) { AddError(FString::Printf(TEXT("an outfit: found [%s], Python finds [%s]"), *FString::Join(Got, TEXT("; ")), *FString::Join(Exp, TEXT("; ")))); ++Wrong; }
	}
	// wear one: the parts change, the rest stays
	const TArray<TSharedPtr<FJsonValue>>& Designs = Want->GetArrayField(TEXT("designs"));
	FBrixDesign A = D.Default, B;
	BrixDesignFromJson(*Designs.Last()->AsObject(), B);
	const TSharedRef<FJsonObject> Outfit = BrixOutfitOf(D, B, TEXT("Hers"));
	TestEqual(TEXT("an outfit made from what is worn is sound"), BrixOutfitProblems(D, *Outfit).Num(), 0);
	BrixWearOutfit(D, *Outfit, A);
	FBrixDesign Expect = B;
	Expect.Name = D.Default.Name;
	Expect.Sliders = D.Default.Sliders;
	Expect.Skin = D.Default.Skin; Expect.Eyes = D.Default.Eyes; Expect.Walk = D.Default.Walk;
	if (!TestTrue(TEXT("wearing it changes only its parts"), A == Expect)) ++Wrong;
	return Wrong == 0;
}

#endif
