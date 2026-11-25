// WLD-058 to WLD-068: Eastgate Mall's inside built as the slice builds it. The slice's own court.js, shell.js and outlets.js
// run in headless Edge (Tools/unreal/mall_values.py) give the values in Tests/values/mall_city_01.json.
#include "BrixMall.h"
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
	FString MallRoot(const TCHAR* Rel) { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Rel)); }

	bool LoadMallCase(FAutomationTestBase& T, FBrixMall& M, TSharedPtr<FJsonObject>& Values)
	{
		FString Error, Text;
		if (!T.TestTrue(TEXT("Eastgate Mall is built from its block file: ") + Error, LoadBrixMall(MallRoot(TEXT("../World/city_01")), M, Error))) return false;
		if (!T.TestTrue(TEXT("the slice's values are there (run Tools/unreal/mall_values.py)"), FFileHelper::LoadFileToString(Text, *MallRoot(TEXT("../Tests/values/mall_city_01.json"))))) return false;
		return T.TestTrue(TEXT("the slice's values read"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Values) && Values.IsValid());
	}

	bool Near(double A, double B, double Tol = 1e-6) { return FMath::Abs(A - B) <= Tol * FMath::Max(1.0, FMath::Abs(B)); }

	// the mall's index in the city's lots (its blocks' order, the slice's LOTS), and the day placed in it
	bool LoadMallDay(FAutomationTestBase& T, const FBrixMall& M, FBrixDay& Day, int32& Mi)
	{
		FString Error, Text;
		if (!T.TestTrue(TEXT("the day loads (Game/Data/day.json, build_play.py): ") + Error, LoadBrixDay(MallRoot(TEXT("Data/day.json")), Day, Error))) return false;
		TSharedPtr<FJsonObject> City;
		if (!T.TestTrue(TEXT("city.json reads"), FFileHelper::LoadFileToString(Text, *MallRoot(TEXT("../World/city_01/city.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), City) && City)) return false;
		int32 k = 0;
		Mi = -1;
		for (const TSharedPtr<FJsonValue>& BV : City->GetArrayField(TEXT("blocks")))
		{
			FString BText;
			TSharedPtr<FJsonObject> Blk;
			if (!FFileHelper::LoadFileToString(BText, *MallRoot(*(TEXT("../World/city_01/") + BV->AsObject()->GetStringField(TEXT("file"))))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BText), Blk) || !Blk) return false;
			for (const TSharedPtr<FJsonValue>& LV : Blk->GetArrayField(TEXT("lots"))) { if (LV->AsObject()->GetStringField(TEXT("id")) == M.LotId) Mi = k; ++k; }
		}
		if (!T.TestTrue(TEXT("the mall is one of the city's lots"), Mi >= 0)) return false;
		SeatBrixDayInMall(Day, M, Mi);
		return true;
	}
	// the day's stays in the mall at a place (x, z) on a floor, of a use (2 work, 3 a seat, 4 before a counter)
	TArray<const FBrixDay::FPiece*> StaysAt(const FBrixDay& Day, int32 Mi, double X, double Z, int32 Floor, int32 Use, TArray<int32>* Who = nullptr)
	{
		TArray<const FBrixDay::FPiece*> Out;
		for (int32 i = 0; i < Day.Plan.Num(); ++i)
			for (const FBrixDay::FPiece& P : Day.Plan[i])
				if (P.Kind == 0 && P.Lot == Mi && P.Use == Use && P.Floor == Floor && FMath::Abs(P.Pos.X - X) < .2 && FMath::Abs(P.Pos.Y - Z) < .2) { Out.Add(&P); if (Who) Who->AddUnique(i); }
		return Out;
	}
}

// WLD-057 to WLD-061, WLD-063, WLD-067, WLD-068: every face, solid, place, light and sign of the
// mall's hall, shell, floors, parapets, bridges, moving stairs, lifts, egg halls, court and outlets, and every place for its
// keepers, guards, janitors and visitors, call by call as the slice
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD058EastgateIsBuilt, "BrixTown.World.WLD058_eastgate_is_built_as_the_slice_builds_it",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWLD058EastgateIsBuilt::RunTest(const FString& Parameters)
{
	FBrixMall M;
	TSharedPtr<FJsonObject> V;
	if (!LoadMallCase(*this, M, V)) return false;
	TestEqual(TEXT("the mall is the slice's"), M.LotId, V->GetStringField(TEXT("lot")));
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
		bool bOk = Near(Sum, E->GetNumberField(TEXT("sum")), 1e-7);
		FString Why = bOk ? FString() : FString::Printf(TEXT("checksum %.6f, the slice %.6f"), Sum, E->GetNumberField(TEXT("sum")));
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
					if (!Near(C.A[j], A[j]->AsNumber())) { Why = FString::Printf(TEXT("call %d (kind %d), number %d: %.6f, the slice %.6f"), From + i, (int32)C.Kind, j, C.A[j], A[j]->AsNumber()); break; }
				if (Why.IsEmpty() && R[2]->Type == EJson::Array)
				{
					const TArray<TSharedPtr<FJsonValue>>& Cv = R[2]->AsArray();
					for (int32 j = 0; j < 3; ++j) if (!Near(C.Col[j], Cv[j]->AsNumber())) Why = FString::Printf(TEXT("call %d: colour"), From + i);
				}
				if (Why.IsEmpty() && (int32)R[3]->AsNumber() != C.Mat) Why = FString::Printf(TEXT("call %d: material %d, the slice's %d"), From + i, C.Mat, (int32)R[3]->AsNumber());
				if (Why.IsEmpty() && R[5]->Type == EJson::String && R[5]->AsString() != C.Text) Why = FString::Printf(TEXT("call %d: \"%s\", the slice's \"%s\""), From + i, *C.Text, *R[5]->AsString());
			}
		if (!Why.IsEmpty() && ++Bad <= 12) AddError(FString::Printf(TEXT("run %d (calls %d on): "), r, From) + Why);
	}
	TestEqual(TEXT("runs of calls unlike the slice's"), Bad, 0);
	TestTrue(TEXT("calls compared one by one"), Checked > 9000);
	// the names on the outlets' signs, the lifts' and seats' words
	const TArray<TSharedPtr<FJsonValue>>& Texts = V->GetArrayField(TEXT("texts"));
	TArray<FString> Mine;
	for (const FBrixMallCall& C : M.Calls) if (!C.Text.IsEmpty()) Mine.Add(C.Text);
	if (TestEqual(TEXT("as many words as the slice"), Mine.Num(), Texts.Num()))
		for (int32 i = 0; i < Mine.Num(); ++i) if (Mine[i] != Texts[i]->AsString()) { AddError(FString::Printf(TEXT("words %d: \"%s\", the slice's \"%s\""), i, *Mine[i], *Texts[i]->AsString())); break; }
	// the lamps (Court.light())
	const TArray<TSharedPtr<FJsonValue>>& Lamps = V->GetArrayField(TEXT("lamps"));
	if (TestEqual(TEXT("as many lamps as the slice"), M.Lamps.Num(), Lamps.Num()))
		for (int32 i = 0; i < Lamps.Num(); ++i)
		{
			const TArray<TSharedPtr<FJsonValue>>& Lp = Lamps[i]->AsArray();
			if (!Near(M.Lamps[i].X, Lp[0]->AsNumber()) || !Near(M.Lamps[i].Y, Lp[1]->AsNumber()) || !Near(M.Lamps[i].Z, Lp[2]->AsNumber()) || !Near(M.Lamps[i].W, Lp[3]->AsNumber()))
			{ AddError(FString::Printf(TEXT("lamp %d is not where the slice has it"), i)); break; }
		}
	return true;
}

// WLD-066: each outlet's room as the slice keeps it for buying in it, and the point-in-outlet test (Outlets.at())
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD066OutletRooms, "BrixTown.World.WLD066_every_outlet_has_its_room",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWLD066OutletRooms::RunTest(const FString& Parameters)
{
	FBrixMall M;
	TSharedPtr<FJsonObject> V;
	if (!LoadMallCase(*this, M, V)) return false;
	const TArray<TSharedPtr<FJsonValue>>& Rooms = V->GetArrayField(TEXT("rooms"));
	if (!TestEqual(TEXT("as many outlet rooms as the slice"), M.Rooms.Num(), Rooms.Num())) return false;
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		const TArray<TSharedPtr<FJsonValue>>& R = Rooms[i]->AsArray();
		const FBrixOutletRoom& Mr = M.Rooms[i];
		const double Want[] = { R[3]->AsNumber(), R[4]->AsNumber(), R[5]->AsNumber(), R[6]->AsNumber(), R[7]->AsNumber(), R[8]->AsNumber(), R[9]->AsNumber(), R[10]->AsNumber(), R[11]->AsNumber(), R[12]->AsNumber(), R[13]->AsNumber() };
		const double Got[] = { (double)Mr.Level, Mr.Fy, Mr.X, Mr.Z, Mr.T.X, Mr.T.Y, Mr.N.X, Mr.N.Y, Mr.A0, Mr.A1, Mr.D };
		bool bOk = Mr.Id == R[0]->AsString() && Mr.Name == R[1]->AsString() && Mr.Kind == (R[2]->Type == EJson::String ? R[2]->AsString() : FString());
		for (int32 j = 0; j < 11; ++j) bOk = bOk && Near(Got[j], Want[j]);
		if (!bOk) { AddError(FString::Printf(TEXT("outlet room %d (%s) is not the slice's"), i, *Mr.Id)); break; }
		// a point in the middle of its floor is in it; a step out in front is not
		const double a = (Mr.A0 + Mr.A1) / 2, v = Mr.D / 2;
		const FBrixOutletRoom* In = BrixOutletAt(M, Mr.X + Mr.T.X * a - Mr.N.X * v, Mr.Fy + 1, Mr.Z + Mr.T.Y * a - Mr.N.Y * v);
		TestTrue(FString::Printf(TEXT("the middle of %s is in an outlet"), *Mr.Id), In != nullptr);
	}
	return true;
}

// WLD-062: the moving stairs carry whoever stands on them as court.js carry() does; the lifts take you between the floors
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD062MovingStairs, "BrixTown.World.WLD062_moving_stairs_carry_and_lifts_go_between_floors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWLD062MovingStairs::RunTest(const FString& Parameters)
{
	FBrixMall M;
	TSharedPtr<FJsonObject> V;
	if (!LoadMallCase(*this, M, V)) return false;
	int32 On = 0, Bad = 0;
	for (const TSharedPtr<FJsonValue>& Cv : V->GetArrayField(TEXT("carry")))
	{
		const TArray<TSharedPtr<FJsonValue>>& C = Cv->AsArray();
		FVector3d P(C[0]->AsNumber(), C[1]->AsNumber(), C[2]->AsNumber());
		const bool b = BrixMallCarry(M, P, .25);
		On += b;
		if (b != (C[3]->AsNumber() > .5) || !Near(P.X, C[4]->AsNumber()) || !Near(P.Z, C[5]->AsNumber())) ++Bad;
	}
	TestEqual(TEXT("points carried unlike the slice"), Bad, 0);
	TestTrue(TEXT("some points are on the stairs"), On > 20);
	// every floor has a lift up or down, and a lift takes you to a floor of the mall
	TArray<int32> PerFloor; PerFloor.Init(0, M.Levels);
	for (const FBrixMallCall& C : M.Calls)
		if (C.Kind == FBrixMallCall::Use && C.Text.StartsWith(TEXT("lift|")))
		{
			const int32 From = FMath::RoundToInt((C.A[1] < 1 ? 0 : C.A[1]) / M.StoreyHeight);
			if (PerFloor.IsValidIndex(From)) ++PerFloor[From];
			TestTrue(TEXT("a lift goes to a floor"), C.A.Num() == 7 && FMath::Abs(C.A[5] - (C.A[5] < 1 ? M.F : FMath::RoundToDouble(C.A[5] / M.StoreyHeight) * M.StoreyHeight)) < 1e-6);
		}
	for (int32 k = 0; k < M.Levels; ++k) TestTrue(FString::Printf(TEXT("floor %d has its lift"), k), PerFloor[k] >= 2);
	return true;
}

// WLD-058 (the owner, 2026-11-25): only Eastgate has an inside; Westgate and Northgate stay as they are, solid from outside
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD058OnlyEastgate, "BrixTown.World.WLD058_only_eastgate_has_an_inside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWLD058OnlyEastgate::RunTest(const FString& Parameters)
{
	FString Text;
	TSharedPtr<FJsonObject> City;
	if (!TestTrue(TEXT("city.json reads"), FFileHelper::LoadFileToString(Text, *MallRoot(TEXT("../World/city_01/city.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), City) && City)) return false;
	TArray<FString> Can, Malls;
	for (const TSharedPtr<FJsonValue>& BV : City->GetArrayField(TEXT("blocks")))
	{
		FString BText;
		TSharedPtr<FJsonObject> Blk;
		if (!FFileHelper::LoadFileToString(BText, *MallRoot(*(TEXT("../World/city_01/") + BV->AsObject()->GetStringField(TEXT("file")))))) continue;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BText), Blk) || !Blk) continue;
		for (const TSharedPtr<FJsonValue>& LV : Blk->GetArrayField(TEXT("lots")))
		{
			const TSharedPtr<FJsonObject> L = LV->AsObject();
			TArray<FString> Tags;
			L->TryGetStringArrayField(TEXT("tags"), Tags);
			if (Tags.Contains(TEXT("mall"))) Malls.Add(L->GetStringField(TEXT("id")));
			if (BrixMallCan(*L)) Can.Add(L->GetStringField(TEXT("id")));
		}
	}
	TestTrue(TEXT("the three malls are in the town"), Malls.Contains(TEXT("b_16_5_l0")) && Malls.Contains(TEXT("b_1_5_l0")) && Malls.Contains(TEXT("b_7_0_l0")));
	TestEqual(TEXT("one mall has its inside built"), Can.Num(), 1);
	TestTrue(TEXT("it is Eastgate (b_16_5_l0)"), Can.Contains(TEXT("b_16_5_l0")));
	return true;
}

// CITY-105, WLD-067: every outlet has one keeper's place behind its counter, in its room on its own floor, and the counter is
// drawn because today's keeper works there; each keeper of the day stands at it, at the floor's height, facing out over the
// counter to the walk. The whole mall's six guards and six janitors stand at their posts.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY105Keepers, "BrixTown.World.CITY105_every_outlet_has_a_counter_its_keeper_stands_at",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCITY105Keepers::RunTest(const FString& Parameters)
{
	FBrixMall M;
	TSharedPtr<FJsonObject> V;
	if (!LoadMallCase(*this, M, V)) return false;
	FBrixDay Day;
	int32 Mi = -1;
	if (!LoadMallDay(*this, M, Day, Mi)) return false;
	TArray<const FBrixMallCall*> Spots;
	for (const FBrixMallCall& C : M.Calls) if (C.Kind == FBrixMallCall::Spot) Spots.Add(&C);
	int32 Rooms = 0, Kept = 0, Bad = 0;
	TArray<int32> Keepers;
	for (const FBrixOutletRoom& R : M.Rooms)
	{
		++Rooms;
		// its keeper's place: a standing place inside the room, on its floor, 2.2 m or more in from the front (behind the counter)
		const FBrixMallCall* Mine = nullptr;
		int32 Count = 0;
		for (const FBrixMallCall* S : Spots)
		{
			if (S->A[2] != 0 || FMath::Abs(S->A[4] - R.Fy) > .01) continue;
			const FBrixOutletRoom* In = BrixOutletAt(M, S->A[0], R.Fy + 1, S->A[1]);
			const double v = -((S->A[0] - R.X) * R.N.X + (S->A[1] - R.Z) * R.N.Y);
			if (In == &R && v > 2.9) { Mine = S; ++Count; }
		}
		if (!Mine || Count != 1) { if (++Bad <= 8) AddError(FString::Printf(TEXT("%s (%s, floor %d): %d keeper's places behind its counter"), *R.Name, *R.Id, R.Level, Count)); continue; }
		if (!M.Staffed.Contains(R.Id)) { if (++Bad <= 8) AddError(R.Name + TEXT(": no counter (its keeper does not work here)")); continue; }
		const TArray<const FBrixDay::FPiece*> Here = StaysAt(Day, Mi, Mine->A[0], Mine->A[1], R.Level, 2, &Keepers);
		if (!Here.Num()) { if (++Bad <= 8) AddError(R.Name + TEXT(": nobody works at its counter today")); continue; }
		for (const FBrixDay::FPiece* P : Here)
		{
			const FVector2D Out(R.N.X, R.N.Y);
			if (P->Pose != 0 || FMath::Abs(P->SeatUp - R.Fy) > .01 || P->Facing.Dot(Out) < .5) { if (++Bad <= 8) AddError(FString::Printf(TEXT("%s: its keeper is not stood behind the counter facing out (pose %d, %.2f m up, facing %.2f)"), *R.Name, P->Pose, P->SeatUp, P->Facing.Dot(Out))); break; }
		}
		++Kept;
	}
	TestEqual(TEXT("outlets"), Rooms, 189);
	TestEqual(TEXT("outlets with a keeper at their counter today"), Kept, Rooms);
	TestEqual(TEXT("outlets wrong"), Bad, 0);
	TestEqual(TEXT("one keeper to each outlet's counter"), Keepers.Num(), Rooms);
	TSet<FString> Careers;
	for (int32 i : Keepers) Careers.Add(Day.Careers.IsValidIndex(i) ? Day.Careers[i] : FString());
	TestTrue(TEXT("the keepers are shop owners and food outlet owners"), Careers.Num() == 2 && Careers.Contains(TEXT("shop owner")) && Careers.Contains(TEXT("food outlet owner")));
	// the guards and the janitors: the spots after every unit's stand, one each at its post (the plan's staff)
	TMap<FString, int32> Posts;
	for (int32 i = 0; i < Day.Plan.Num(); ++i)
	{
		const FString C = Day.Careers.IsValidIndex(i) ? Day.Careers[i] : FString();
		if (C != TEXT("security guard") && C != TEXT("janitor")) continue;
		for (const FBrixDay::FPiece& P : Day.Plan[i])
			if (P.Kind == 0 && P.Lot == Mi && P.Use == 2)
			{
				bool bAt = false;
				for (const FBrixMallCall* S : Spots) if (S->A[2] == 0 && FMath::Abs(S->A[0] - P.Pos.X) < .2 && FMath::Abs(S->A[1] - P.Pos.Y) < .2 && FMath::Abs(P.SeatUp - S->A[4]) < .01) bAt = true;
				TestTrue(C + TEXT(" stands at a post of the mall"), bAt);
				++Posts.FindOrAdd(C);
				break;
			}
	}
	TestEqual(TEXT("security guards at the mall today"), Posts.FindRef(TEXT("security guard")), 6);
	TestEqual(TEXT("janitors at the mall today"), Posts.FindRef(TEXT("janitor")), 6);
	return true;
}

// CITY-106, WLD-068: the residents who come to the mall to shop stand before an outlet's counter, facing it; those who come
// to eat sit at a food outlet's table or on a bench of the court, facing it; all on their own floor, inside the mall
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY106Visitors, "BrixTown.World.CITY106_visitors_shop_at_the_counters_and_eat_at_the_tables",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCITY106Visitors::RunTest(const FString& Parameters)
{
	FBrixMall M;
	TSharedPtr<FJsonObject> V;
	if (!LoadMallCase(*this, M, V)) return false;
	FBrixDay Day;
	int32 Mi = -1;
	if (!LoadMallDay(*this, M, Day, Mi)) return false;
	int32 Shop = 0, Eat = 0, InOutlet = 0, Bad = 0, Floors[3] = { 0, 0, 0 };
	for (const TArray<FBrixDay::FPiece>& Row : Day.Plan)
		for (const FBrixDay::FPiece& P : Row)
		{
			if (P.Kind != 0 || P.Lot != Mi || (P.Use != 3 && P.Use != 4)) continue;
			const bool bEat = P.Use == 3;
			(bEat ? Eat : Shop)++;
			if (P.Floor >= 0 && P.Floor < 3) ++Floors[P.Floor];
			const bool bKiosk = !bEat && P.Pose == 0 && P.Floor == 0 && FMath::Abs(P.SeatUp - M.F) < .01;   // at a kiosk of the court floor (the lot's own places)
			const bool bDoor = !(P.Pos.X > M.X0 && P.Pos.X < M.X1 && P.Pos.Y > M.Z0 && P.Pos.Y < M.Z1);   // no place left in the mall: at its door, as in the slice
			if (bDoor) { if ((P.Pose != 0 || P.SeatUp >= 0) && ++Bad <= 8) AddError(TEXT("a visitor at the door is not stood on the ground")); continue; }
			const double Up = P.Floor ? P.Floor * M.StoreyHeight : M.F;
			const bool bOk = bEat ? (P.Pose == 2 && P.SeatUp > Up + .4 && P.SeatUp < Up + .5) : (bKiosk || (P.Pose == 0 && FMath::Abs(P.SeatUp - Up) < .01));
			if (!bOk && ++Bad <= 8) AddError(FString::Printf(TEXT("a visitor to %s at (%.1f, %.1f) on floor %d: pose %d, %.2f m up"), bEat ? TEXT("eat") : TEXT("shop"), P.Pos.X, P.Pos.Y, P.Floor, P.Pose, P.SeatUp));
			if (!bEat)   // before a counter: in the outlet, facing in, towards its keeper
			{
				const FBrixOutletRoom* R = BrixOutletAt(M, P.Pos.X, Up + 1, P.Pos.Y);
				if (R) { ++InOutlet; if (P.Facing.Dot(FVector2D(-R->N.X, -R->N.Y)) < .5 && ++Bad <= 8) AddError(R->Name + TEXT(": a shopper does not face the counter")); }
			}
		}
	AddInfo(FString::Printf(TEXT("%d stays shopping, %d eating, %d before an outlet's counter; by floor %d, %d, %d"), Shop, Eat, InOutlet, Floors[0], Floors[1], Floors[2]));
	TestTrue(TEXT("residents come to shop"), Shop > 20);
	TestTrue(TEXT("residents come to eat"), Eat > 100);
	TestTrue(TEXT("most shoppers are before an outlet's counter"), InOutlet * 2 > Shop);
	TestTrue(TEXT("visitors on the upper floors too"), Floors[1] > 0 && Floors[2] > 0);
	TestEqual(TEXT("visitors not at their place"), Bad, 0);
	return true;
}

#endif
