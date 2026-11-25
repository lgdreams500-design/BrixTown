// BUILD-002, BUILD-004 to BUILD-007, BUILD-016 to BUILD-019: the building designer's rules in the Unreal build follow
// Tools/citysim/builder.py. Game/Data/inside.json carries every lot's design (lot.dsg), the kit and planning rules and
// builder.check_script() with what Python gives for it (build_play.py builder_data); here the C++ runs the same remodels.
#include "BrixBuilder.h"
#include "BrixCity.h"
#include "BrixInside.h"
#include "BrixSave.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString BCityDir() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/city_01"))); }
	FString BInsidePath() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/inside.json"))); }

	// two JSON values the same, numbers to within a hair (Python's floats against the C++ doubles)
	bool Alike(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, FString& Where, const FString& At = FString())
	{
		const bool bAn = !A.IsValid() || A->IsNull(), bBn = !B.IsValid() || B->IsNull();
		if (bAn || bBn) { if (bAn != bBn) Where = At; return bAn == bBn; }
		if (A->Type != B->Type) { Where = At; return false; }
		switch (A->Type)
		{
		case EJson::Number: if (FMath::Abs(A->AsNumber() - B->AsNumber()) > 1e-6) { Where = At + FString::Printf(TEXT(" %g vs %g"), A->AsNumber(), B->AsNumber()); return false; } return true;
		case EJson::String: if (A->AsString() != B->AsString()) { Where = At + TEXT(" '") + A->AsString() + TEXT("' vs '") + B->AsString() + TEXT("'"); return false; } return true;
		case EJson::Boolean: if (A->AsBool() != B->AsBool()) { Where = At; return false; } return true;
		case EJson::Array:
		{
			const TArray<TSharedPtr<FJsonValue>>& X = A->AsArray(), & Y = B->AsArray();
			if (X.Num() != Y.Num()) { Where = At + FString::Printf(TEXT(" length %d vs %d"), X.Num(), Y.Num()); return false; }
			for (int32 i = 0; i < X.Num(); ++i) if (!Alike(X[i], Y[i], Where, At + FString::Printf(TEXT("[%d]"), i))) return false;
			return true;
		}
		case EJson::Object:
		{
			const TSharedPtr<FJsonObject> X = A->AsObject(), Y = B->AsObject();
			if (X->Values.Num() != Y->Values.Num()) { Where = At + TEXT(" keys"); return false; }
			for (const auto& KV : Y->Values) if (!Alike(X->TryGetField(FString(KV.Key)), KV.Value, Where, At + TEXT(".") + FString(KV.Key))) return false;
			return true;
		}
		default: return true;
		}
	}
	TSharedPtr<FJsonValue> Strs(const TArray<FString>& S) { TArray<TSharedPtr<FJsonValue>> A; for (const FString& x : S) A.Add(MakeShared<FJsonValueString>(x)); return MakeShared<FJsonValueArray>(A); }
	TSharedPtr<FJsonValue> V3(const FVector3f& C) { return MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ MakeShared<FJsonValueNumber>(C.X), MakeShared<FJsonValueNumber>(C.Y), MakeShared<FJsonValueNumber>(C.Z) }); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBUILD005_RemodelsMatchPython, "BrixTown.World.BUILD005_remodels_match_the_python",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FBUILD005_RemodelsMatchPython::RunTest(const FString&)
{
	FBrixCity City;
	FBrixInsides Ins;
	FString Error;
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(BCityDir(), City, Error))) return false;
	if (!TestTrue(TEXT("inside.json loads: ") + Error, LoadBrixInside(BInsidePath(), Ins, Error))) return false;
	const FBrixBuilderRules& R = Ins.Builder;
	if (!TestTrue(TEXT("the designer's rules are in inside.json"), R.IsSet() && R.Checks.IsValid())) return false;
	TMap<FString, TPair<const FBrixLot*, const FBrixBlock*>> ById;
	TMap<FString, int32> Index;
	int32 k = 0;
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) { ById.Add(L.Id, { &L, &B }); Index.Add(L.Id, k++); }
	const TArray<TSharedPtr<FJsonValue>>& Script = R.Checks->GetArrayField(TEXT("script")), & Want = R.Checks->GetArrayField(TEXT("want"));
	TestEqual(TEXT("a result for every remodel"), Script.Num(), Want.Num());
	for (int32 n = 0; n < Script.Num() && n < Want.Num(); ++n)
	{
		const TArray<TSharedPtr<FJsonValue>>& Op = Script[n]->AsArray();
		const FString Id = Op[0]->AsString();
		const FString Label = FString::Printf(TEXT("remodel %d (%s)"), n, *Id);
		const auto* Lb = ById.Find(Id);
		const int32* Li = Index.Find(Id);
		if (!TestTrue(Label + TEXT(": the lot is in the town with a design"), Lb && Li && Ins.Lots.IsValidIndex(*Li) && Ins.Lots[*Li].IsSet() && Ins.Lots[*Li]->Design.IsValid())) continue;
		const FBrixDesignLot L = BrixBuilder::LotOf(*Lb->Key, *Lb->Value);
		const TSharedPtr<FJsonObject> Old = Ins.Lots[*Li]->Design;
		FBrixDesignChanges Ch;
		for (const auto& KV : Op[1]->AsObject()->Values) Ch.Add({ FString(KV.Key), KV.Value });
		const TSharedPtr<FJsonObject> Owner = Op[2]->AsObject();
		bool bWhole = false;
		Owner->TryGetBoolField(TEXT("whole"), bWhole);
		TArray<int32> Held;
		const TArray<TSharedPtr<FJsonValue>>* Fl = nullptr;
		if (Owner->TryGetArrayField(TEXT("floors"), Fl)) for (const auto& F : *Fl) Held.Add((int32)F->AsNumber());
		TOptional<int64> Funds;
		if (!Op[3]->IsNull()) Funds = (int64)Op[3]->AsNumber();
		const TSharedPtr<FJsonObject> New = BrixBuilder::Remodel(Old, Ch);
		TArray<FBrixCostLine> Lines;
		const int64 Total = BrixBuilder::Cost(R, L, Old, New, Lines);
		const TSharedPtr<FJsonObject> W = Want[n]->AsObject();
		TestEqual(Label + TEXT(": the total (BUILD-005)"), Total, (int64)W->GetNumberField(TEXT("total")));
		TArray<TSharedPtr<FJsonValue>> GotLines;
		for (const FBrixCostLine& Ln : Lines) GotLines.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ MakeShared<FJsonValueString>(Ln.What), MakeShared<FJsonValueNumber>(Ln.Qty), MakeShared<FJsonValueNumber>((double)Ln.Chips) }));
		FString Where;
		if (!Alike(MakeShared<FJsonValueArray>(GotLines), W->TryGetField(TEXT("lines")), Where)) AddError(Label + TEXT(": the cost's lines differ at") + Where);
		const TSharedPtr<FJsonObject> Opts = MakeShared<FJsonObject>();
		for (const auto& P : BrixBuilder::Options(R, L, Old))
		{
			TArray<TSharedPtr<FJsonValue>> Ids;
			for (const FBrixDesignOption& O : P.Value) Ids.Add(O.Id);
			Opts->SetArrayField(P.Key, Ids);
		}
		if (!Alike(MakeShared<FJsonValueObject>(Opts), W->TryGetField(TEXT("options")), Where)) AddError(Label + TEXT(": what is offered differs (BUILD-017) at") + Where);
		const TArray<FString> Probs = BrixBuilder::Check(R, L, Old, New, bWhole, Held, Funds);
		if (!Alike(Strs(Probs), W->TryGetField(TEXT("problems")), Where)) AddError(Label + TEXT(": the problems differ (BUILD-006, BUILD-007) at") + Where + TEXT(": ") + FString::Join(Probs, TEXT("; ")));
		FString Side;
		Old->GetObjectField(TEXT("shell"))->TryGetStringField(TEXT("side"), Side);
		if (!Alike(MakeShared<FJsonValueObject>(BrixBuilder::BuiltFacadeJson(R, New, Side)), W->TryGetField(TEXT("front")), Where)) AddError(Label + TEXT(": the built front differs (BUILD-016, BUILD-018) at") + Where);
		const FBrixBuiltLook B = BrixBuilder::Built(R, New);
		const TSharedPtr<FJsonObject> WIn = W->GetObjectField(TEXT("inside"));
		const TArray<TSharedPtr<FJsonValue>>& WF = WIn->GetArrayField(TEXT("floors")), & WW = WIn->GetArrayField(TEXT("walls"));
		if (TestEqual(Label + TEXT(": storeys inside (BUILD-019)"), B.FloorFinish.Num(), WF.Num()) && TestEqual(Label + TEXT(": walls inside"), B.WallColour.Num(), WW.Num()))
			for (int32 f = 0; f < WF.Num(); ++f)
			{
				TestEqual(Label + FString::Printf(TEXT(": floor %d's finish"), f), B.FloorFinish[f], (int32)WF[f]->AsArray()[0]->AsNumber());
				if (!Alike(V3(B.FloorColour[f]), WF[f]->AsArray()[1], Where)) AddError(Label + FString::Printf(TEXT(": floor %d's colour"), f));
				FString W2;
				if (!Alike(V3(B.WallColour[f]), WW[f], W2)) { const TArray<TSharedPtr<FJsonValue>>& A = WW[f]->AsArray(); if (FVector3f::Dist(B.WallColour[f], FVector3f(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber())) > 1e-5f) AddError(Label + FString::Printf(TEXT(": floor %d's walls"), f)); }
			}
	}
	return true;
}

// BUILD-002, BUILD-016: every building is built from its design; the inside the C++ works out from each design is the one
// Python wrote for it, and the designs name their choices from the kit.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBUILD002_EveryBuildingFromItsDesign, "BrixTown.World.BUILD002_every_building_is_built_from_its_design",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FBUILD002_EveryBuildingFromItsDesign::RunTest(const FString&)
{
	FBrixCity City;
	FBrixInsides Ins;
	FString Error;
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(BCityDir(), City, Error))) return false;
	if (!TestTrue(TEXT("inside.json loads: ") + Error, LoadBrixInside(BInsidePath(), Ins, Error))) return false;
	int32 k = -1, Designs = 0, Bad = 0, Unknown = 0;
	for (const FBrixBlock& B : City.Blocks)
		for (const FBrixLot& L : B.Lots)
		{
			++k;
			if (!Ins.Lots.IsValidIndex(k) || !Ins.Lots[k].IsSet()) continue;
			const FBrixInside& In = Ins.Lots[k].GetValue();
			if (!In.Design.IsValid()) continue;
			++Designs;
			const FBrixBuiltLook Bt = BrixBuilder::Built(Ins.Builder, In.Design);
			bool bSame = Bt.FloorFinish == In.FloorFinish && Bt.WallColour.Num() == In.WallColour.Num() && Bt.FloorColour.Num() == In.FloorColour.Num();
			for (int32 f = 0; bSame && f < Bt.WallColour.Num(); ++f) bSame = FVector3f::Dist(Bt.WallColour[f], In.WallColour[f]) < 1e-5f && FVector3f::Dist(Bt.FloorColour[f], In.FloorColour[f]) < 1e-5f;
			if (!bSame && Bad++ < 5) AddError(TEXT("the inside built from the design is not Python's: ") + L.Id);
			const TSharedPtr<FJsonObject> Cols = Ins.Builder.Kit->GetObjectField(TEXT("colours"));
			for (const TCHAR* F : BrixBuilder::Facades)
				if (!Cols->HasField(In.Design->GetObjectField(TEXT("walls"))->GetObjectField(F)->GetStringField(TEXT("plaster")))) ++Unknown;
		}
	TestTrue(TEXT("the town's buildings have designs"), Designs > 900);
	TestEqual(TEXT("insides that differ from Python's"), Bad, 0);
	TestEqual(TEXT("facade plasters not in the kit (BUILD-016)"), Unknown, 0);
	return true;
}

// QST-008, BUILD-005: a remodel paid for is kept in the save (life.js remodels): the design comes back as it was written
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQST008_RemodelsAreKept, "BrixTown.World.QST008_remodels_are_kept_in_the_save",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FQST008_RemodelsAreKept::RunTest(const FString&)
{
	FBrixInsides Ins;
	FString Error;
	if (!TestTrue(TEXT("inside.json loads: ") + Error, LoadBrixInside(BInsidePath(), Ins, Error))) return false;
	const FBrixInside* In = nullptr;
	for (const TOptional<FBrixInside>& L : Ins.Lots) if (L.IsSet() && L->Design.IsValid()) { In = &L.GetValue(); break; }
	if (!TestNotNull(TEXT("a building with a design"), In)) return false;
	const FString Tiles = In->Design->GetObjectField(TEXT("roof"))->GetStringField(TEXT("tiles")) == TEXT("slate") ? TEXT("umber") : TEXT("slate");
	const TSharedPtr<FJsonObject> New = BrixBuilder::Remodel(In->Design, { { TEXT("walls.*.plaster"), MakeShared<FJsonValueString>(TEXT("peach")) }, { TEXT("roof.tiles"), MakeShared<FJsonValueString>(Tiles) } });
	FBrixSaveGame S;
	S.Remodels = MakeShared<FJsonObject>();
	S.Remodels->SetObjectField(In->Id, New);
	TSharedPtr<FJsonObject> Back;
	if (!TestTrue(TEXT("the save is JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S.ToString()), Back) && Back.IsValid())) return false;
	const TSharedPtr<FJsonObject>* R = nullptr;
	if (!TestTrue(TEXT("the save has its remodels"), Back->TryGetObjectField(TEXT("remodels"), R))) return false;
	const TSharedPtr<FJsonObject>* D = nullptr;
	if (!TestTrue(TEXT("the building's design is in it"), (*R)->TryGetObjectField(In->Id, D))) return false;
	TestEqual(TEXT("nothing changed between the remodel and the save"), BrixBuilder::Changed(New, *D).Num(), 0);
	TestEqual(TEXT("the remodel's changes are there"), FString::Join(BrixBuilder::Changed(In->Design, *D), TEXT(",")).Contains(TEXT("roof.tiles")), true);
	return true;
}

#endif
