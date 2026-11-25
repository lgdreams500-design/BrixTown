// CITY-017, WLD-040, WLD-056, ART-001: the gardens, lawns and open ground drawn as the slice draws them. The slice's own
// functions run in headless Edge (Tools/unreal/streets_values.py) give the values in Tests/values/streets_city_01.json.
#include "BrixStreets.h"
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
	FString Root(const TCHAR* Rel) { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Rel)); }

	struct FStreetsCase
	{
		FBrixCity City;
		FBrixStreetsToday Today;
		TSharedPtr<FJsonObject> Values;
	};

	bool Load(FAutomationTestBase& T, FStreetsCase& C)
	{
		FString Error, Text;
		if (!T.TestTrue(TEXT("the starting city loads: ") + Error, LoadBrixCity(Root(TEXT("../World/city_01")), C.City, Error))) return false;
		if (!T.TestTrue(TEXT("today's gardens load (Game/Data/streets.json, build_play.py): ") + Error, LoadBrixStreetsToday(Root(TEXT("Data/streets.json")), C.Today, Error))) return false;
		if (!T.TestTrue(TEXT("the slice's values are there (run Tools/unreal/streets_values.py)"), FFileHelper::LoadFileToString(Text, *Root(TEXT("../Tests/values/streets_city_01.json"))))) return false;
		return T.TestTrue(TEXT("the slice's values read"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), C.Values) && C.Values.IsValid());
	}

	// One thing's calls against the slice's: as many, the same checksum, and call by call where the slice gave them in full
	void Compare(FAutomationTestBase& T, const FString& What, const TArray<FBrixShape>& Got, const TSharedPtr<FJsonObject>& Want, int32& Bad)
	{
		const int32 N = (int32)Want->GetNumberField(TEXT("n"));
		const double Sum = Want->GetNumberField(TEXT("sum")), Mine = BrixShapesChecksum(Got);
		bool bOk = Got.Num() == N && FMath::Abs(Mine - Sum) <= 1e-6 * FMath::Max(1.0, FMath::Abs(Sum));
		FString Why = bOk ? FString() : FString::Printf(TEXT("%d calls (the slice %d), checksum %.6f (the slice %.6f)"), Got.Num(), N, Mine, Sum);
		const TArray<TSharedPtr<FJsonValue>>* Calls = nullptr;
		if (bOk && Want->TryGetArrayField(TEXT("calls"), Calls))
			for (int32 i = 0; i < Calls->Num() && bOk; ++i)
			{
				const TArray<TSharedPtr<FJsonValue>>& R = (*Calls)[i]->AsArray();
				const FBrixShape& S = Got[i];
				const TArray<TSharedPtr<FJsonValue>>& A = R[1]->AsArray();
				if ((int32)R[0]->AsNumber() != (int32)S.Kind || A.Num() != S.A.Num()) { bOk = false; Why = FString::Printf(TEXT("call %d: kind %d with %d numbers, the slice's %d with %d"), i, (int32)S.Kind, S.A.Num(), (int32)R[0]->AsNumber(), A.Num()); break; }
				for (int32 j = 0; j < A.Num(); ++j)
					if (FMath::Abs(A[j]->AsNumber() - S.A[j]) > 1e-6) { bOk = false; Why = FString::Printf(TEXT("call %d (kind %d), number %d: %.6f, the slice %.6f"), i, (int32)S.Kind, j, S.A[j], A[j]->AsNumber()); break; }
				if (bOk && R[2]->Type == EJson::Array)
				{
					const TArray<TSharedPtr<FJsonValue>>& Cv = R[2]->AsArray();
					for (int32 j = 0; j < 3; ++j) if (FMath::Abs(Cv[j]->AsNumber() - S.Col[j]) > 1e-6) { bOk = false; Why = FString::Printf(TEXT("call %d: colour"), i); }
				}
				if (bOk && R.Num() > 4 && R[4]->Type == EJson::String && R[4]->AsString() != S.Text) { bOk = false; Why = FString::Printf(TEXT("call %d: the text %s, the slice's %s"), i, *S.Text, *R[4]->AsString()); }
			}
		if (!bOk && ++Bad <= 12) T.AddError(What + TEXT(": ") + Why);
	}

	FBrixGround GroundAt(const FBrixBlock& B, const FBrixProp& P)
	{
		// as streets_values.py sets them: the open ground -0.03, an open lot 0.16, the paving 0.12
		FBrixGround G;
		bool bOpen = false;
		for (const FBrixLot& L : B.Lots) if (L.IsOpen() && P.Pos.X >= L.Min.X && P.Pos.X <= L.Max.X && P.Pos.Y >= L.Min.Y && P.Pos.Y <= L.Max.Y) bOpen = true;
		G.Here = P.Set == TEXT("wild") ? G.Wild : bOpen ? .16 : .12;
		return G;
	}
}

// CITY-017, WLD-040: every garden plant of the block files (hedges, bushes, borders and drifts) is drawn where its block file
// puts it, as play.js plant() draws it with today's season: the same boxes, crowns, flower heads and clumps with the same
// sizes, turns and colours, from the slice's own hash of the same numbers. Also: a JavaScript number is written as the
// slice writes it, so the hash is fed the same strings.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY017_EveryPlantIsDrawn, "BrixTown.World.CITY017_every_plant_is_drawn_as_the_slice_draws_it",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY017_EveryPlantIsDrawn::RunTest(const FString&)
{
	using BrixSlice::Js;
	TestEqual(TEXT("1"), Js(1.0), FString(TEXT("1"))); TestEqual(TEXT("-0.22"), Js(-.22), FString(TEXT("-0.22")));
	TestEqual(TEXT("0.1 + 0.2"), Js(.1 + .2), FString(TEXT("0.30000000000000004"))); TestEqual(TEXT("1e21"), Js(1e21), FString(TEXT("1e+21")));
	TestEqual(TEXT("1e-7"), Js(1e-7), FString(TEXT("1e-7"))); TestEqual(TEXT("0.000001"), Js(1e-6), FString(TEXT("0.000001")));
	TestEqual(TEXT("199795"), Js(6445.0 * 31), FString(TEXT("199795"))); TestEqual(TEXT("1157.75"), Js(1157.75), FString(TEXT("1157.75")));
	FStreetsCase C;
	if (!Load(*this, C)) return false;
	const TSharedPtr<FJsonObject> Want = C.Values->GetObjectField(TEXT("props"));
	int32 Plants = 0, Bad = 0, Missing = 0;
	for (const FBrixBlock& B : C.City.Blocks)
		for (int32 i = 0; i < B.Props.Num(); ++i)
		{
			const FBrixProp& P = B.Props[i];
			if (P.Kind != TEXT("plant")) continue;
			++Plants;
			const FString Key = B.Id + TEXT("/") + FString::FromInt(i);
			const TSharedPtr<FJsonObject>* W = nullptr;
			if (!Want->TryGetObjectField(Key, W)) { if (++Missing <= 5) AddError(Key + TEXT(": the slice drew nothing for it")); continue; }
			TArray<FBrixShape> Got;
			BrixPlantShapes(P, GroundAt(B, P), C.Today, Got);
			Compare(*this, Key + TEXT(" (") + P.Id + TEXT(")"), Got, *W, Bad);
		}
	TestTrue(TEXT("the block files have their plants (2,735 when written)"), Plants > 2000);
	TestEqual(TEXT("plants drawn otherwise than in the slice"), Bad, 0);
	return Missing == 0;
}

// WLD-056, ART-001: the open ground beyond Old Town (woods' leaf litter, the commons', orchards', fields' and meadows' grass,
// their tracks) and its own things (fences, "For sale" boards, pegs, logs, rocks, picnic tables, hay bales, crops, ponds),
// and every garden's lawn with its longer edge and the tufts the mower missed (none on a path or under a thing), are drawn
// as play.js greenGround(), wildProp() and lawn() draw them.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD056_OpenGroundIsDrawn, "BrixTown.World.WLD056_the_open_ground_is_drawn_as_the_slice_draws_it",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD056_OpenGroundIsDrawn::RunTest(const FString&)
{
	FStreetsCase C;
	if (!Load(*this, C)) return false;
	const TSharedPtr<FJsonObject> Props = C.Values->GetObjectField(TEXT("props")), Lawns = C.Values->GetObjectField(TEXT("lawns")), Ground = C.Values->GetObjectField(TEXT("ground"));
	int32 Things = 0, Blocks = 0, Gardens = 0, Bad = 0;
	for (const FBrixBlock& B : C.City.Blocks)
	{
		if (BrixIsWildTemplate(B.Template))
		{
			++Blocks;
			const TSharedPtr<FJsonObject>* W = nullptr;
			if (!TestTrue(B.Id + TEXT(": the slice drew its ground"), Ground->TryGetObjectField(B.Id, W))) continue;
			TArray<FBrixShape> Got;
			FBrixGround G;
			BrixGreenGroundShapes(B, G, C.Today, Got);
			Compare(*this, B.Id + TEXT(" ground"), Got, *W, Bad);
		}
		for (const FBrixLot& L : B.Lots)
		{
			if (L.Kind != TEXT("garden")) continue;
			++Gardens;
			const TSharedPtr<FJsonObject>* W = nullptr;
			if (!TestTrue(L.Id + TEXT(": the slice drew its lawn"), Lawns->TryGetObjectField(L.Id, W))) continue;
			TArray<FBrixShape> Got;
			BrixLawnShapes(L.Min.X, L.Min.Y, L.Max.X, L.Max.Y, .15 + .02, BrixSlice::Grass * 1.08, B, true, C.Today, Got);
			Compare(*this, L.Id + TEXT(" lawn"), Got, *W, Bad);
		}
		for (int32 i = 0; i < B.Props.Num(); ++i)
		{
			const FBrixProp& P = B.Props[i];
			if (P.Set != TEXT("wild") || P.Kind == TEXT("plant")) continue;
			TArray<FBrixShape> Got;
			const bool bMine = BrixWildShapes(P, GroundAt(B, P), Got);
			const TSharedPtr<FJsonObject>* W = nullptr;
			const bool bSlice = Props->TryGetObjectField(B.Id + TEXT("/") + FString::FromInt(i), W);
			if (!TestEqual(B.Id + TEXT("/") + FString::FromInt(i) + TEXT(" (") + P.Kind + TEXT("): drawn as one of the open ground's own things"), bMine, bSlice) || !bMine) continue;
			++Things;
			Compare(*this, B.Id + TEXT("/") + FString::FromInt(i) + TEXT(" (") + P.Kind + TEXT(")"), Got, *W, Bad);
		}
	}
	TestTrue(TEXT("blocks of open ground (17 when written)"), Blocks >= 10);
	TestTrue(TEXT("gardens (33 when written)"), Gardens >= 20);
	TestTrue(TEXT("the open ground's things (600 or so when written)"), Things > 400);
	TestEqual(TEXT("drawn otherwise than in the slice"), Bad, 0);
	return true;
}

#endif
