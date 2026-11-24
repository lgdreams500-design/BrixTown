// ART-004, ANIM-001, ANIM-002: the people of the Unreal build are the slice's. Run headless with the other tests:
//   UnrealEditor-Cmd.exe Game/BrixTown.uproject -ExecCmds="Automation RunTests BrixTown.World; Quit" -unattended -nullrhi
#include "BrixBody.h"
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
	FVector3f Col(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A;
		if (!O->TryGetArrayField(Key, A) || A->Num() < 3) return FVector3f(-1);
		return FVector3f((float)(*A)[0]->AsNumber(), (float)(*A)[1]->AsNumber(), (float)(*A)[2]->AsNumber());
	}
}

// Prototype/play/people.js, run by Tools/unreal/people_values.py, gives the looks of a set of residents and the bodies it
// draws for them walking and standing at every distance (Tests/values/people_city_01.json). The C++ port gives the same
// look (sizes, colours, clothes, hair, walk) and the same body: as many vertices, and every sampled vertex within 1 mm of
// where people.js puts it (the slice stores them as 32-bit floats); the far figures' boxes the same.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FART004_PeopleAreTheSlices, "BrixTown.World.ART004_people_are_drawn_as_the_slice_draws_them",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FART004_PeopleAreTheSlices::RunTest(const FString&)
{
	FBrixPeopleRules Rules;
	FString Error, Text;
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/people.json")));
	if (!TestTrue(TEXT("the people's tables load: ") + Error, LoadBrixPeopleRules(Path, Rules, Error))) return false;
	const FString Values = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../Tests/values/people_city_01.json")));
	if (!TestTrue(TEXT("the test values are there: ") + Values, FFileHelper::LoadFileToString(Text, *Values))) return false;
	TSharedPtr<FJsonObject> Want;
	if (!TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Want) && Want.IsValid())) return false;

	TArray<FBrixLook> Looks;
	int32 Wrong = 0;
	auto Near = [](const FVector3f& A, const FVector3f& B) { return A.Equals(B, 1e-4f); };
	for (const TSharedPtr<FJsonValue>& V : Want->GetArrayField(TEXT("looks")))
	{
		const TSharedPtr<FJsonObject> W = V->AsObject();
		const int32 Id = (int32)W->GetNumberField(TEXT("i"));
		const FBrixLook L = BrixLookOf(Rules, Id, W->GetStringField(TEXT("role")), W->GetStringField(TEXT("sigName")));
		Looks.Add(L);
		const FString Who = FString::Printf(TEXT("%d (%s)"), Id, *W->GetStringField(TEXT("role")));
		auto Opt = [&](const TCHAR* Key, const TOptional<FVector3f>& Got) { const FVector3f E = Col(W, Key); return E.X < 0 ? !Got.IsSet() : Got.IsSet() && Near(*Got, E); };
		bool bBag = false, bTash = false;
		W->TryGetBoolField(TEXT("bag"), bBag);
		W->TryGetBoolField(TEXT("tash"), bTash);
		const bool bSame = FMath::IsNearlyEqual(L.S, W->GetNumberField(TEXT("S")), 1e-9) && FMath::IsNearlyEqual(L.W, W->GetNumberField(TEXT("W")), 1e-9)
			&& Near(L.Skin, Col(W, TEXT("skin"))) && Near(L.HairC, Col(W, TEXT("hairC"))) && Near(L.Eyes, Col(W, TEXT("eyes")))
			&& Near(L.Top, Col(W, TEXT("top"))) && Near(L.Bottom, Col(W, TEXT("bottom"))) && Near(L.Shoes, Col(W, TEXT("shoes")))
			&& Opt(TEXT("apron"), L.Apron) && Opt(TEXT("cap"), L.Cap) && Opt(TEXT("scarf"), L.Scarf)
			&& L.Hair == W->GetStringField(TEXT("hair")) && L.Garment == W->GetStringField(TEXT("g")) && L.Legs == W->GetStringField(TEXT("legs"))
			&& L.bBag == bBag && L.bTash == bTash && L.SigName == W->GetStringField(TEXT("sig")) && L.Seed == (int32)W->GetNumberField(TEXT("seed"));
		if (!bSame) { AddError(TEXT("resident ") + Who + TEXT(": not the look people.js gives them")); ++Wrong; }
	}

	ResetBrixWalkHeights();   // drawn in the order people_values.py drew them
	int32 Bodies = 0;
	FBrixBodyMesh Mesh;
	TArray<FBrixFarBox> Boxes;
	for (const TSharedPtr<FJsonValue>& V : Want->GetArrayField(TEXT("bodies")))
	{
		const TSharedPtr<FJsonObject> B = V->AsObject();
		const int32 k = (int32)B->GetNumberField(TEXT("look"));
		FBrixDrawOptions O;
		O.Pose = (int32)B->GetNumberField(TEXT("pose"));
		O.Phase = B->GetNumberField(TEXT("ph"));
		O.Seconds = B->GetNumberField(TEXT("t"));
		O.Lod = (int32)B->GetNumberField(TEXT("lod"));
		O.FaceFade = B->GetNumberField(TEXT("ff"));
		Mesh.Reset();
		Boxes.Reset();
		DrawBrixPerson(Rules, Looks[k], FVector(B->GetNumberField(TEXT("x")), B->GetNumberField(TEXT("y")), B->GetNumberField(TEXT("z"))), B->GetNumberField(TEXT("yaw")), O, Mesh, &Boxes);
		const FString What = FString::Printf(TEXT("look %d, pose %d, lod %d"), k, O.Pose, O.Lod);
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
			Worst = FMath::Max(Worst, (double)FVector3f::Distance(Mesh.Pos[Mesh.Index[(int32)S[0]->AsNumber()]], E));   // the k-th vertex of people.js's triangle list
		}
		if (Worst > 1e-3) { AddError(FString::Printf(TEXT("%s: a vertex %.4f m from where people.js puts it"), *What, Worst)); ++Wrong; }
		const TArray<TSharedPtr<FJsonValue>>& WB = B->GetArrayField(TEXT("boxes"));
		bool bBoxes = WB.Num() == Boxes.Num();
		for (int32 i = 0; bBoxes && i < WB.Num(); ++i)
		{
			const TArray<TSharedPtr<FJsonValue>>& X = WB[i]->AsArray();
			auto F = [&](int32 j) { return (float)X[j]->AsNumber(); };
			bBoxes = Boxes[i].Min.Equals(FVector3f(F(0), F(1), F(2)), 1e-4f) && Boxes[i].Max.Equals(FVector3f(F(3), F(4), F(5)), 1e-4f) && Near(Boxes[i].Colour, FVector3f(F(6), F(7), F(8)));
		}
		if (!bBoxes) { AddError(What + TEXT(": not the far figure people.js draws")); ++Wrong; }
	}
	TestTrue(TEXT("looks and bodies were checked"), Looks.Num() > 10 && Bodies > 40);
	return Wrong == 0;
}


// CITY-017: residents use a garden's benches. The day's stays at a garden's bench seats and fountain rims are found as the
// slice finds them (play.js resident(), openLot()) and sat on: each such stay is inside its garden (so the lots are read in
// the slice's order), on a seat a bench's height up, facing along a street's axes towards the garden's middle (a bench) or
// out of the fountain (a rim); and some of the day's stays are seated.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY017_ResidentsSitOnGardenBenches, "BrixTown.World.CITY017_residents_sit_on_garden_benches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY017_ResidentsSitOnGardenBenches::RunTest(const FString&)
{
	FBrixCity City;
	FBrixDay Day;
	FString Error;
	const FString CityDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/city_01")));
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(CityDir, City, Error))) return false;
	if (!TestTrue(TEXT("the day loads: ") + Error, LoadBrixDay(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/day.json"))), Day, Error))) return false;
	const int32 Seated = SeatBrixDay(Day, City);
	TArray<const FBrixLot*> Lots;
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) Lots.Add(&L);
	int32 Wrong = 0, Checked = 0;
	for (int32 i = 0; i < Day.Num(); ++i)
		for (const FBrixDay::FPiece& P : Day.Plan[i])
		{
			if (P.Kind != 0 || P.Pose != 2) continue;
			++Checked;
			const FBrixLot* L = Lots.IsValidIndex(P.Lot) ? Lots[P.Lot] : nullptr;
			const bool bIn = L && L->Kind == TEXT("garden") && P.Pos.X >= L->Min.X - .5 && P.Pos.X <= L->Max.X + .5 && P.Pos.Y >= L->Min.Y - .5 && P.Pos.Y <= L->Max.Y + .5;
			const bool bUp = P.SeatUp > .5f && P.SeatUp < .7f;
			const FVector2D ToMid = L ? L->Centre() - P.Pos : FVector2D::ZeroVector;
			const bool bFace = FMath::IsNearlyEqual(P.Facing.Size(), 1.0, 1e-3) && (FVector2D::DotProduct(P.Facing, ToMid) > 0 || FVector2D::DotProduct(P.Facing, -ToMid) > .99 * ToMid.Size());
			if (!bIn || !bUp || !bFace)
			{
				if (Wrong < 10) AddError(FString::Printf(TEXT("%s: a seat at (%.1f, %.1f) %s"), *Day.Ids[i], P.Pos.X, P.Pos.Y, !bIn ? TEXT("not in its garden") : !bUp ? TEXT("not a seat's height up") : TEXT("facing neither the middle nor out of the rim")));
				++Wrong;
			}
		}
	TestEqual(TEXT("every seat counted once"), Checked, Seated);
	TestTrue(TEXT("some of the day's stays are on a garden seat"), Seated > 20);
	return Wrong == 0;
}

#endif
