// BUILD-021 to BUILD-023: a storey's room plan in the Unreal build follows Tools/citysim/roomplan.py. inside.json carries
// roomplan.check_script() and what Python gives for it (build_play.py builder_data); here the C++ runs the same edits.
#include "BrixCity.h"
#include "BrixInside.h"
#include "BrixRoomPlan.h"
#include "BrixSave.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString RpCityDir() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/city_01"))); }
	FString RpInsidePath() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/inside.json"))); }
	// the length of wall a storey's walls cover (lines of the same run joined where they overlap), doorways or the rest
	double Covered(const TArray<FBrixPartition>& Segs, bool bDoors)
	{
		TMap<FString, TArray<FVector2D>> Lines;
		for (const FBrixPartition& X : Segs)
		{
			if (X.bDoorway != bDoors) continue;
			const bool bX = X.Max.X - X.Min.X >= X.Max.Y - X.Min.Y;
			Lines.FindOrAdd(FString::Printf(TEXT("%d:%.2f"), bX ? 1 : 0, bX ? (X.Min.Y + X.Max.Y) / 2 : (X.Min.X + X.Max.X) / 2)).Add(bX ? FVector2D(X.Min.X, X.Max.X) : FVector2D(X.Min.Y, X.Max.Y));
		}
		double T = 0;
		for (auto& L : Lines)
		{
			L.Value.Sort([](const FVector2D& a, const FVector2D& b) { return a.X < b.X; });
			double A = -1e9, B = -1e9;
			for (const FVector2D& I : L.Value) { if (I.X > B) { T += FMath::Max(0.0, B - A); A = I.X; B = I.Y; } else B = FMath::Max(B, I.Y); }
			T += FMath::Max(0.0, B - A);
		}
		return T;
	}
	FVector4f Rect(const TArray<TSharedPtr<FJsonValue>>& A) { return FVector4f(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber(), A[3]->AsNumber()); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBUILD021_RoomPlansMatchPython, "BrixTown.World.BUILD021_room_plans_match_the_python",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FBUILD021_RoomPlansMatchPython::RunTest(const FString&)
{
	FBrixInsides Ins;
	FString Error;
	if (!TestTrue(TEXT("inside.json loads: ") + Error, LoadBrixInside(RpInsidePath(), Ins, Error))) return false;
	if (!TestTrue(TEXT("the room plan's rules and fixed run are there"), Ins.Builder.RoomPlan.IsValid())) return false;
	FBrixRoomPlanRules R;
	TestTrue(TEXT("the rules load"), R.Load(Ins.Builder.RoomPlan->GetObjectField(TEXT("rules"))));
	const TSharedPtr<FJsonObject> C = Ins.Builder.RoomPlan->GetObjectField(TEXT("checks")), S = C->GetObjectField(TEXT("script"));
	FBrixRoomSpec Spec;
	Spec.Room = Rect(S->GetArrayField(TEXT("room")));
	for (const auto& K : S->GetArrayField(TEXT("keep"))) Spec.Keep.Add(Rect(K->AsArray()));
	for (const auto& E : S->GetArrayField(TEXT("entries"))) Spec.Entries.Add(FVector2D(E->AsArray()[0]->AsNumber(), E->AsArray()[1]->AsNumber()));
	Spec.Ceiling = S->GetNumberField(TEXT("height"));
	TArray<FString> Colours;
	for (const auto& V : S->GetArrayField(TEXT("colours"))) Colours.Add(V->AsString());
	FBrixPlan Before;
	for (const auto& V : S->GetArrayField(TEXT("before"))) Before.Add(BrixRoomPlan::WallFromJson(V->AsObject()));
	const TArray<TSharedPtr<FJsonValue>>& Cases = S->GetArrayField(TEXT("cases")), & Want = C->GetArrayField(TEXT("want"));
	TestEqual(TEXT("a result for every case"), Cases.Num(), Want.Num());
	for (int32 k = 0; k < Cases.Num() && k < Want.Num(); ++k)
	{
		const TArray<TSharedPtr<FJsonValue>>& Cs = Cases[k]->AsArray();
		const FBrixPlan After = BrixRoomPlan::Apply(Before, Cs[0]->AsObject());
		TOptional<int64> Funds;
		if (!Cs[1]->IsNull()) Funds = (int64)Cs[1]->AsNumber();
		TArray<FBrixPlanLine> Lines;
		const int64 Total = BrixRoomPlan::Cost(R, Before, After, Spec.Ceiling, Lines);
		const TArray<FString> Probs = BrixRoomPlan::Check(R, Spec, Colours, Before, After, Funds);
		const TSharedPtr<FJsonObject> W = Want[k]->AsObject();
		const FString Label = FString::Printf(TEXT("case %d"), k);
		TestEqual(Label + TEXT(": the total (BUILD-023)"), Total, (int64)W->GetNumberField(TEXT("total")));
		TArray<FString> WP;
		for (const auto& V : W->GetArrayField(TEXT("problems"))) WP.Add(V->AsString());
		TestEqual(Label + TEXT(": the problems (BUILD-021)"), FString::Join(Probs, TEXT("; ")), FString::Join(WP, TEXT("; ")));
		const TArray<TSharedPtr<FJsonValue>>& WL = W->GetArrayField(TEXT("lines"));
		if (TestEqual(Label + TEXT(": lines"), Lines.Num(), WL.Num()))
			for (int32 i = 0; i < Lines.Num(); ++i)
			{
				const TArray<TSharedPtr<FJsonValue>>& L = WL[i]->AsArray();
				TestEqual(Label + FString::Printf(TEXT(": line %d"), i), Lines[i].What, L[0]->AsString());
				TestTrue(Label + FString::Printf(TEXT(": line %d's quantity"), i), FMath::Abs(Lines[i].Qty - L[1]->AsNumber()) < 1e-9);
				TestEqual(Label + FString::Printf(TEXT(": line %d's chips"), i), Lines[i].Chips, (int64)L[2]->AsNumber());
			}
	}
	// every storey of the buildings of flats: its plan as the town made it (storeys.js planOf()) draws back the same walls
	FBrixCity City;
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(RpCityDir(), City, Error))) return false;
	int32 k = -1, Storeys = 0, Bad = 0;
	for (const FBrixBlock& B : City.Blocks)
		for (const FBrixLot& L : B.Lots)
		{
			++k;
			if (!Ins.Lots.IsValidIndex(k) || !Ins.Lots[k].IsSet() || !Ins.Lots[k]->Flats.Num()) continue;
			for (int32 f = 0; f < L.Floors; ++f)
			{
				const TArray<FBrixPartition> Segs = BrixFlatWalls(L, Ins.Lots[k].GetValue(), f, Ins.Rules);
				if (!Segs.Num()) continue;
				++Storeys;
				const FBrixPlan P = BrixRoomPlan::PlanOf(Segs);
				const TArray<FBrixPartition> Back = BrixRoomPlan::SegsOf(R, P);
				const double Was = Covered(Segs, false), WasDoor = Covered(Segs, true), Got = Covered(Back, false), GotDoor = Covered(Back, true);
				if ((FMath::Abs(Got - Was) > 0.05 || FMath::Abs(GotDoor - WasDoor) > 0.05) && Bad++ < 5)
					AddError(FString::Printf(TEXT("%s floor %d: the plan draws %.2f m of wall and %.2f m of doorway, the town %.2f and %.2f"), *L.Id, f, Got, GotDoor, Was, WasDoor));
			}
		}
	TestTrue(TEXT("storeys of flats with walls"), Storeys > 100);
	TestEqual(TEXT("storeys whose plan draws other walls"), Bad, 0);
	return true;
}

// QST-008, BUILD-021: a storey's room plan paid for is kept in the save (life.js plans) and comes back wall for wall
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQST008_RoomPlansAreKept, "BrixTown.World.QST008_room_plans_are_kept_in_the_save",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FQST008_RoomPlansAreKept::RunTest(const FString&)
{
	FBrixPlanWall A; A.Id = TEXT("w1"); A.Ax = TEXT("z"); A.Fix = 5.0; A.A0 = 0.0; A.A1 = 6.0; A.Door = 3.0;
	FBrixPlanWall B; B.Id = TEXT("w2"); B.Ax = TEXT("x"); B.Fix = 3.0; B.A0 = 0.0; B.A1 = 5.0; B.Colour = TEXT("apricot");
	const FBrixPlan Plan = { A, B };
	FBrixSaveGame S;
	S.Plans = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> ByFloor = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Ws;
	for (const FBrixPlanWall& W : Plan) Ws.Add(MakeShared<FJsonValueObject>(BrixRoomPlan::WallToJson(W)));
	ByFloor->SetArrayField(TEXT("1"), Ws);
	S.Plans->SetObjectField(TEXT("b_9_6_l4"), ByFloor);
	TSharedPtr<FJsonObject> Back;
	if (!TestTrue(TEXT("the save is JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S.ToString()), Back) && Back.IsValid())) return false;
	const TSharedPtr<FJsonObject>* P = nullptr;
	if (!TestTrue(TEXT("the save has its room plans"), Back->TryGetObjectField(TEXT("plans"), P))) return false;
	FBrixPlan Got;
	for (const auto& V : (*P)->GetObjectField(TEXT("b_9_6_l4"))->GetArrayField(TEXT("1"))) Got.Add(BrixRoomPlan::WallFromJson(V->AsObject()));
	TestTrue(TEXT("the plan comes back wall for wall, doorway and colour too"), Got == Plan);
	return true;
}

#endif
