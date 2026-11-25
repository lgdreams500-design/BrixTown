// FURN-051, FURN-052, FURN-055: furnishing a storey in the designer in the Unreal build follows Tools/citysim/furnish.py.
// inside.json carries furnish.check_script() and what Python gives for it (build_play.py builder_data); here the C++ runs
// the same edits: the problems, the cost line by line and the belongings after each.
#include "BrixFurnishPlan.h"
#include "BrixInside.h"
#include "BrixSave.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool FpAlike(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, FString& Where, const FString& At = FString())
	{
		const bool bAn = !A.IsValid() || A->IsNull(), bBn = !B.IsValid() || B->IsNull();
		if (bAn || bBn) { if (bAn != bBn) Where = At; return bAn == bBn; }
		if (A->Type != B->Type) { Where = At + TEXT(" type"); return false; }
		switch (A->Type)
		{
		case EJson::Number: if (FMath::Abs(A->AsNumber() - B->AsNumber()) > 1e-6) { Where = At + FString::Printf(TEXT(" %g vs %g"), A->AsNumber(), B->AsNumber()); return false; } return true;
		case EJson::String: if (A->AsString() != B->AsString()) { Where = At + TEXT(" '") + A->AsString() + TEXT("' vs '") + B->AsString() + TEXT("'"); return false; } return true;
		case EJson::Boolean: if (A->AsBool() != B->AsBool()) { Where = At; return false; } return true;
		case EJson::Array:
		{
			const TArray<TSharedPtr<FJsonValue>>& X = A->AsArray(), & Y = B->AsArray();
			if (X.Num() != Y.Num()) { Where = At + FString::Printf(TEXT(" length %d vs %d"), X.Num(), Y.Num()); return false; }
			for (int32 i = 0; i < X.Num(); ++i) if (!FpAlike(X[i], Y[i], Where, At + FString::Printf(TEXT("[%d]"), i))) return false;
			return true;
		}
		case EJson::Object:
		{
			const TSharedPtr<FJsonObject> X = A->AsObject(), Y = B->AsObject();
			if (X->Values.Num() != Y->Values.Num()) { Where = At + TEXT(" keys"); return false; }
			for (const auto& KV : Y->Values) if (!FpAlike(X->TryGetField(FString(KV.Key)), KV.Value, Where, At + TEXT(".") + FString(KV.Key))) return false;
			return true;
		}
		default: return true;
		}
	}
	FVector4f Rect4(const TArray<TSharedPtr<FJsonValue>>& A) { return FVector4f(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber(), A[3]->AsNumber()); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFURN051_FurnishingMatchesPython, "BrixTown.World.FURN051_furnishing_matches_the_python",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FFURN051_FurnishingMatchesPython::RunTest(const FString&)
{
	FBrixInsides Ins;
	FString Error;
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/inside.json")));
	if (!TestTrue(TEXT("inside.json loads: ") + Error, LoadBrixInside(Path, Ins, Error))) return false;
	if (!TestTrue(TEXT("the furnishing rules and fixed run are there"), Ins.Builder.Furnish.IsValid())) return false;
	FBrixFurnishRulesD F;
	TestTrue(TEXT("the rules load"), F.Load(Ins.Builder.Furnish->GetObjectField(TEXT("rules"))));
	const TSharedPtr<FJsonObject> C = Ins.Builder.Furnish->GetObjectField(TEXT("checks")), S = C->GetObjectField(TEXT("script"));
	const FVector4f Room = Rect4(S->GetArrayField(TEXT("room")));
	TArray<FVector4f> Keep;
	for (const auto& K : S->GetArrayField(TEXT("keep"))) Keep.Add(Rect4(K->AsArray()));
	FBrixLayout Before, Belongings;
	for (const auto& V : S->GetArrayField(TEXT("before"))) Before.Add(BrixFurnishPlan::FromJson(V->AsObject()));
	for (const auto& V : S->GetArrayField(TEXT("belongings"))) Belongings.Add(BrixFurnishPlan::FromJson(V->AsObject()));
	const TSharedPtr<FJsonObject> Where = S->GetObjectField(TEXT("where"));
	const FString Lot = Where->GetStringField(TEXT("lot"));
	const int32 Level = (int32)Where->GetNumberField(TEXT("level"));
	const TArray<TSharedPtr<FJsonValue>>& Cases = S->GetArrayField(TEXT("cases")), & Want = C->GetArrayField(TEXT("want"));
	TestEqual(TEXT("a result for every case"), Cases.Num(), Want.Num());
	for (int32 k = 0; k < Cases.Num() && k < Want.Num(); ++k)
	{
		const TArray<TSharedPtr<FJsonValue>>& Cs = Cases[k]->AsArray();
		const double Ceiling = Cs[0]->AsNumber();
		const FBrixLayout After = BrixFurnishPlan::Apply(F, Before, Cs[1]->AsObject(), Belongings);
		TOptional<int64> Funds;
		if (!Cs[2]->IsNull()) Funds = (int64)Cs[2]->AsNumber();
		TArray<FBrixFurnLine> Lines;
		const int64 Total = BrixFurnishPlan::Cost(F, Before, After, Belongings, Lines);
		const TArray<FString> Probs = BrixFurnishPlan::Check(F, Room, Keep, Ceiling, Before, After, Funds, Belongings);
		const TSharedPtr<FJsonObject> W = Want[k]->AsObject();
		const FString Label = FString::Printf(TEXT("case %d"), k);
		TestEqual(Label + TEXT(": the total (FURN-052)"), Total, (int64)W->GetNumberField(TEXT("total")));
		TArray<FString> WP;
		for (const auto& V : W->GetArrayField(TEXT("problems"))) WP.Add(V->AsString());
		TestEqual(Label + TEXT(": the problems (FURN-051)"), FString::Join(Probs, TEXT("; ")), FString::Join(WP, TEXT("; ")));
		TArray<TSharedPtr<FJsonValue>> GotLines, GotB;
		for (const FBrixFurnLine& L : Lines) GotLines.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ MakeShared<FJsonValueString>(L.What), MakeShared<FJsonValueNumber>(L.Qty), MakeShared<FJsonValueNumber>((double)L.Chips) }));
		FString At;
		if (!FpAlike(MakeShared<FJsonValueArray>(GotLines), W->TryGetField(TEXT("lines")), At)) AddError(Label + TEXT(": the lines differ at") + At);
		for (const FBrixLayoutPiece& P : BrixFurnishPlan::Stow(Before, After, Belongings, Lot, Level)) GotB.Add(MakeShared<FJsonValueObject>(BrixFurnishPlan::ToJson(P, false)));
		if (!FpAlike(MakeShared<FJsonValueArray>(GotB), W->TryGetField(TEXT("belongings")), At)) AddError(Label + TEXT(": the belongings differ (INV-001) at") + At);
	}
	return true;
}

// QST-008, FURN-051: a storey's furniture paid for is kept in the save (life.js furnishings), and the pieces taken away with
// the belongings (life.js belongings, INV-001); both come back piece for piece
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQST008_FurnishingsAreKept, "BrixTown.World.QST008_furnishings_are_kept_in_the_save",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FQST008_FurnishingsAreKept::RunTest(const FString&)
{
	FBrixFurnishRulesD F;
	F.Pieces.Add(TEXT("k17:sofa"), { TEXT("Sofa"), FString(), FVector3f(2.0f, 0.92f, 1.0f), 18000, false });
	FBrixLayoutPiece T; T.Id = TEXT("t0"); T.Name = TEXT("Cabinet"); T.W = .9; T.D = .5; T.H = 1.2; T.X = 2.0; T.Z = 3.0; T.Yaw = 90;
	const FBrixLayout Lay = { T, BrixFurnishPlan::KitPiece(F, TEXT("k17:sofa"), TEXT("k1"), 3.0, 2.5, 180, TEXT("oldtown")) };
	FBrixLayoutPiece B = T; B.Id = TEXT("b:b_9_6_l2:0:t1"); B.FromLot = TEXT("b_9_6_l2"); B.FromLevel = 0; B.FromId = TEXT("t1");
	FBrixSaveGame S;
	S.Furnishings = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> ByFloor = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Ps;
	for (const FBrixLayoutPiece& P : Lay) Ps.Add(MakeShared<FJsonValueObject>(BrixFurnishPlan::ToJson(P)));
	ByFloor->SetArrayField(TEXT("0"), Ps);
	S.Furnishings->SetObjectField(TEXT("b_9_6_l2"), ByFloor);
	S.Furniture.Add(MakeShared<FJsonValueObject>(BrixFurnishPlan::ToJson(B, false)));
	TSharedPtr<FJsonObject> Back;
	if (!TestTrue(TEXT("the save is JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S.ToString()), Back) && Back.IsValid())) return false;
	FBrixLayout Got;
	for (const auto& V : Back->GetObjectField(TEXT("furnishings"))->GetObjectField(TEXT("b_9_6_l2"))->GetArrayField(TEXT("0"))) Got.Add(BrixFurnishPlan::FromJson(V->AsObject()));
	TestTrue(TEXT("the storey's pieces come back where they stood"), Got == Lay);
	TestEqual(TEXT("the sofa keeps its line"), Got.Num() > 1 && Got[1].Line ? *Got[1].Line : FString(), FString(TEXT("oldtown")));
	const TArray<TSharedPtr<FJsonValue>>& Bl = Back->GetArrayField(TEXT("belongings"));
	if (TestEqual(TEXT("the furniture among the belongings"), Bl.Num(), 1))
	{
		const FBrixLayoutPiece G = BrixFurnishPlan::FromJson(Bl[0]->AsObject());
		TestEqual(TEXT("its id"), G.Id, B.Id);
		TestEqual(TEXT("where it came from"), G.FromLot ? *G.FromLot : FString(), FString(TEXT("b_9_6_l2")));
	}
	return true;
}

#endif
