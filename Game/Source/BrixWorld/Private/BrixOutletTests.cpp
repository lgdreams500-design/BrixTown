// FURN-020, FURN-021: the furniture outlet built as the slice builds it. The slice's own outlet.js run in headless Edge
// (Tools/unreal/outlet_values.py) gives the values in Tests/values/outlet_city_01.json.
#include "BrixOutlet.h"
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
	FString OutletRoot(const TCHAR* Rel) { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Rel)); }
	bool OutletNear(double A, double B, double Tol = 1e-6) { return FMath::Abs(A - B) <= Tol * FMath::Max(1.0, FMath::Abs(B)); }

	bool LoadOutletCase(FAutomationTestBase& T, FBrixMall& M, TSharedPtr<FJsonObject>& Values)
	{
		FString Error, Text;
		if (!T.TestTrue(TEXT("the furniture outlet is built from its block file: ") + Error, LoadBrixOutlet(OutletRoot(TEXT("../World/city_01")), OutletRoot(TEXT("Data/car_models.json")), M, Error))) return false;
		if (!T.TestTrue(TEXT("the slice's values are there (run Tools/unreal/outlet_values.py)"), FFileHelper::LoadFileToString(Text, *OutletRoot(TEXT("../Tests/values/outlet_city_01.json"))))) return false;
		return T.TestTrue(TEXT("the slice's values read"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Values) && Values.IsValid());
	}
}

// FURN-020: the hall (stone and plaster under timber trusses and skylights, its arched windows, sign, door and loading doors),
// the walking route, the room sets with their furniture in its line, tills, desks, the cafe, the racks of boxed stock, the
// lanterns, and the car park with its cars, call by call as the slice
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFURN020OutletIsBuilt, "BrixTown.World.FURN020_the_outlet_is_built_as_the_slice_builds_it",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFURN020OutletIsBuilt::RunTest(const FString& Parameters)
{
	FBrixMall M;
	TSharedPtr<FJsonObject> V;
	if (!LoadOutletCase(*this, M, V)) return false;
	TestEqual(TEXT("the outlet is the slice's"), M.LotId, V->GetStringField(TEXT("lot")));
	TestEqual(TEXT("as many calls as the slice makes"), M.Calls.Num(), (int32)V->GetNumberField(TEXT("calls")));
	const TArray<TSharedPtr<FJsonValue>>& Kinds = V->GetArrayField(TEXT("kinds"));
	for (int32 k = 0; k < Kinds.Num(); ++k)
	{
		int32 n = 0;
		for (const FBrixMallCall& C : M.Calls) n += C.Kind == k;
		TestEqual(FString::Printf(TEXT("calls of kind %d"), k), n, (int32)Kinds[k]->AsNumber());
	}
	const int32 Run = (int32)V->GetNumberField(TEXT("run"));
	const TArray<TSharedPtr<FJsonValue>>& Runs = V->GetArrayField(TEXT("runs"));
	int32 Bad = 0, Checked = 0;
	for (int32 r = 0; r < Runs.Num(); ++r)
	{
		const TSharedPtr<FJsonObject> E = Runs[r]->AsObject();
		const int32 From = r * Run, N = (int32)E->GetNumberField(TEXT("n"));
		if (From + N > M.Calls.Num()) { AddError(FString::Printf(TEXT("run %d: past the end of the calls"), r)); break; }
		double Sum = 0;
		for (int32 i = From; i < From + N; ++i) Sum += BrixMallChecksum(M.Calls[i]);
		FString Why = OutletNear(Sum, E->GetNumberField(TEXT("sum")), 1e-7) ? FString() : FString::Printf(TEXT("checksum %.6f, the slice %.6f"), Sum, E->GetNumberField(TEXT("sum")));
		const TArray<TSharedPtr<FJsonValue>>* Calls = nullptr;
		if (E->TryGetArrayField(TEXT("calls"), Calls))
			for (int32 i = 0; i < Calls->Num() && Why.IsEmpty(); ++i)
			{
				const TArray<TSharedPtr<FJsonValue>>& R = (*Calls)[i]->AsArray();
				const FBrixMallCall& C = M.Calls[From + i];
				const TArray<TSharedPtr<FJsonValue>>& A = R[1]->AsArray();
				++Checked;
				if ((int32)R[0]->AsNumber() != (int32)C.Kind || A.Num() != C.A.Num()) { Why = FString::Printf(TEXT("call %d: kind %d with %d numbers, the slice's %d with %d"), From + i, (int32)C.Kind, C.A.Num(), (int32)R[0]->AsNumber(), A.Num()); break; }
				for (int32 j = 0; j < A.Num(); ++j)
					if (!OutletNear(C.A[j], A[j]->AsNumber())) { Why = FString::Printf(TEXT("call %d (kind %d), number %d: %.6f, the slice %.6f"), From + i, (int32)C.Kind, j, C.A[j], A[j]->AsNumber()); break; }
				if (Why.IsEmpty() && R[2]->Type == EJson::Array)
				{
					const TArray<TSharedPtr<FJsonValue>>& Cv = R[2]->AsArray();
					for (int32 j = 0; j < 3; ++j) if (!OutletNear(C.Col[j], Cv[j]->AsNumber())) Why = FString::Printf(TEXT("call %d: colour"), From + i);
				}
				if (Why.IsEmpty() && (int32)R[3]->AsNumber() != C.Mat) Why = FString::Printf(TEXT("call %d: material %d, the slice's %d"), From + i, C.Mat, (int32)R[3]->AsNumber());
				if (Why.IsEmpty() && R[5]->Type == EJson::String && R[5]->AsString() != C.Text) Why = FString::Printf(TEXT("call %d: \"%s\", the slice's \"%s\""), From + i, *C.Text, *R[5]->AsString());
			}
		if (!Why.IsEmpty() && ++Bad <= 12) AddError(FString::Printf(TEXT("run %d (calls %d on): "), r, From) + Why);
	}
	TestEqual(TEXT("runs of calls unlike the slice's"), Bad, 0);
	TestTrue(TEXT("calls compared one by one"), Checked > 500);
	const TArray<TSharedPtr<FJsonValue>>& Texts = V->GetArrayField(TEXT("texts"));
	TArray<FString> Mine;
	for (const FBrixMallCall& C : M.Calls) if (!C.Text.IsEmpty()) Mine.Add(C.Text);
	if (TestEqual(TEXT("as many words as the slice (names, uses, cars)"), Mine.Num(), Texts.Num()))
		for (int32 i = 0; i < Mine.Num(); ++i) if (Mine[i] != Texts[i]->AsString()) { AddError(FString::Printf(TEXT("words %d: \"%s\", the slice's \"%s\""), i, *Mine[i], *Texts[i]->AsString())); break; }
	return true;
}

// FURN-021: the outlet's staff work at its tills, sales desks, cafe and packing tables, facing the door; the residents who
// come to browse stand by the room sets or the racks, and those at the cafe sit at its tables
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFURN021OutletPeople, "BrixTown.World.FURN021_staff_and_browsers_are_at_the_outlets_places",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFURN021OutletPeople::RunTest(const FString& Parameters)
{
	FBrixMall M;
	TSharedPtr<FJsonObject> V;
	if (!LoadOutletCase(*this, M, V)) return false;
	FBrixDay Day;
	FString Error, Text;
	if (!TestTrue(TEXT("the day loads: ") + Error, LoadBrixDay(OutletRoot(TEXT("Data/day.json")), Day, Error))) return false;
	TSharedPtr<FJsonObject> City;
	if (!TestTrue(TEXT("city.json reads"), FFileHelper::LoadFileToString(Text, *OutletRoot(TEXT("../World/city_01/city.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), City) && City)) return false;
	int32 k = 0, Oi = -1;
	for (const TSharedPtr<FJsonValue>& BV : City->GetArrayField(TEXT("blocks")))
	{
		FString BText;
		TSharedPtr<FJsonObject> Blk;
		if (!FFileHelper::LoadFileToString(BText, *OutletRoot(*(TEXT("../World/city_01/") + BV->AsObject()->GetStringField(TEXT("file"))))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BText), Blk) || !Blk) return false;
		for (const TSharedPtr<FJsonValue>& LV : Blk->GetArrayField(TEXT("lots"))) { if (LV->AsObject()->GetStringField(TEXT("id")) == M.LotId) Oi = k; ++k; }
	}
	if (!TestTrue(TEXT("the outlet is one of the city's lots"), Oi >= 0)) return false;
	SeatBrixDayInMall(Day, M, Oi);
	int32 Work = 0, Browse = 0, Sit = 0, Bad = 0;
	TSet<int32> Staff;
	for (int32 i = 0; i < Day.Plan.Num(); ++i)
		for (const FBrixDay::FPiece& P : Day.Plan[i])
		{
			if (P.Kind != 0 || P.Lot != Oi || P.Use < 0) continue;
			const bool bIn = P.Pos.X > M.X0 && P.Pos.X < M.X1 && P.Pos.Y > M.Z0 && P.Pos.Y < M.Z1;
			if (!bIn) continue;
			if (P.Use == 2) { ++Work; Staff.Add(i); if ((P.Pose != 0 || FMath::Abs(P.SeatUp - M.F) > .01 || P.Facing.Y > -.99) && ++Bad <= 8) AddError(TEXT("a member of staff is not stood facing the door")); }
			else if (P.Pose == 2) { ++Sit; if (FMath::Abs(P.SeatUp - (M.F + .45)) > .01 && ++Bad <= 8) AddError(TEXT("a visitor at the cafe is not on its chair")); }
			else { ++Browse; if ((P.Pose != 0 || FMath::Abs(P.SeatUp - M.F) > .01) && ++Bad <= 8) AddError(TEXT("a browser is not stood on the floor")); }
		}
	AddInfo(FString::Printf(TEXT("%d stays at work (%d staff), %d browsing, %d at the cafe"), Work, Staff.Num(), Browse, Sit));
	TestTrue(TEXT("the outlet has its staff today"), Staff.Num() >= 5);
	TestTrue(TEXT("residents come to browse"), Browse > 0);
	TestEqual(TEXT("people not at their place"), Bad, 0);
	return true;
}

#endif
