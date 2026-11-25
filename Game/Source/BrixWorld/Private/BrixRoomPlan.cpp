#include "BrixRoomPlan.h"

#include "BrixInside.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
	const double Tol = 1e-6;
	double Q2(double X) { return FMath::FloorToDouble(X * 100 + .5) / 100; }
	int64 Chips(double X) { return (int64)FMath::FloorToDouble(X + .5); }
	bool Meet(const FVector4f& A, const FVector4f& B) { return A.X < B.Z - Tol && B.X < A.Z - Tol && A.Y < B.W - Tol && B.Y < A.W - Tol; }
	double Fixed3(double V) { return FCString::Atod(*FString::Printf(TEXT("%.3f"), V)); }   // +v.toFixed(3)
	int64 Key(int32 i, int32 j) { return (int64)i * 100000 + j; }

	struct FBlocked { int32 Nx = 0, Nz = 0; TSet<int64> Cells; };
	FBlocked Blocked(const FBrixRoomPlanRules& R, const FVector4f& Room, const FBrixPlan& Walls)
	{
		FBlocked B;
		const double c = R.Cell, h = R.Thick / 2 + c / 2;
		B.Nx = (int32)FMath::FloorToDouble((Room.Z - Room.X) / c + Tol);
		B.Nz = (int32)FMath::FloorToDouble((Room.W - Room.Y) / c + Tol);
		for (const FBrixPlanWall& W : Walls)
			for (int32 i = 0; i < B.Nx; ++i)
				for (int32 j = 0; j < B.Nz; ++j)
				{
					const double x = Room.X + (i + .5) * c, z = Room.Y + (j + .5) * c, Along = W.Ax == TEXT("x") ? x : z, Across = W.Ax == TEXT("x") ? z : x;
					if (FMath::Abs(Across - W.Fix) < h - Tol && Along > W.A0 - c / 2 && Along < W.A1 + c / 2)
						if (!W.Door || FMath::Abs(Along - *W.Door) > R.DoorW / 2 - c / 2 + Tol) B.Cells.Add(Key(i, j));
				}
		return B;
	}
	// roomplan.reach(): whether each entry is reached from the first, and the rooms left over (m2, largest first)
	void Reach(const FBrixRoomPlanRules& R, const FVector4f& Room, const FBrixPlan& Walls, const TArray<FVector2D>& Entries, TArray<bool>& Got, TArray<double>& Pockets)
	{
		const double c = R.Cell;
		const FBlocked B = Blocked(R, Room, Walls);
		auto Cell = [&](const FVector2D& P) { return FIntPoint(FMath::Clamp((int32)FMath::FloorToDouble((P.X - Room.X) / c), 0, B.Nx - 1), FMath::Clamp((int32)FMath::FloorToDouble((P.Y - Room.Y) / c), 0, B.Nz - 1)); };
		TSet<int64> Seen;
		const FIntPoint St = Cell(Entries[0]);
		const int32 Nb[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		if (!B.Cells.Contains(Key(St.X, St.Y)))
		{
			TArray<FIntPoint> Todo = { St };
			Seen.Add(Key(St.X, St.Y));
			while (Todo.Num())
			{
				const FIntPoint P = Todo.Pop();
				for (const auto& N : Nb)
				{
					const int32 a = P.X + N[0], b = P.Y + N[1];
					if (a >= 0 && a < B.Nx && b >= 0 && b < B.Nz && !B.Cells.Contains(Key(a, b)) && !Seen.Contains(Key(a, b))) { Seen.Add(Key(a, b)); Todo.Add(FIntPoint(a, b)); }
				}
			}
		}
		Got.Reset();
		for (const FVector2D& E : Entries) { const FIntPoint P = Cell(E); Got.Add(Seen.Contains(Key(P.X, P.Y))); }
		TArray<int64> Order;   // the cells left, in the slice's order (i, then j)
		TSet<int64> Left;
		for (int32 i = 0; i < B.Nx; ++i) for (int32 j = 0; j < B.Nz; ++j) if (!B.Cells.Contains(Key(i, j)) && !Seen.Contains(Key(i, j))) { Left.Add(Key(i, j)); Order.Add(Key(i, j)); }
		Pockets.Reset();
		for (const int64 A : Order)
		{
			if (!Left.Contains(A)) continue;
			Left.Remove(A);
			TArray<FIntPoint> Todo = { FIntPoint((int32)(A / 100000), (int32)(A % 100000)) };
			int32 n = 1;
			while (Todo.Num())
			{
				const FIntPoint P = Todo.Pop();
				for (const auto& N : Nb) { const int64 K = Key(P.X + N[0], P.Y + N[1]); if (Left.Contains(K)) { Left.Remove(K); Todo.Add(FIntPoint(P.X + N[0], P.Y + N[1])); ++n; } }
			}
			Pockets.Add(FMath::RoundHalfFromZero(n * c * c * 10000) / 10000);
		}
		Pockets.Sort([](double a, double b) { return a > b; });
	}
	FString Fmt1(double V) { return FString::Printf(TEXT("%.1f"), V); }
}

bool FBrixRoomPlanRules::Load(const TSharedPtr<FJsonObject>& J)
{
	if (!J.IsValid()) return false;
	J->TryGetNumberField(TEXT("grid_m"), Grid); J->TryGetNumberField(TEXT("min_len"), MinLen); J->TryGetNumberField(TEXT("thick"), Thick);
	J->TryGetNumberField(TEXT("door_w"), DoorW); J->TryGetNumberField(TEXT("door_margin"), DoorMargin); J->TryGetNumberField(TEXT("cell"), Cell);
	J->TryGetNumberField(TEXT("pocket_m2"), PocketM2);
	const TSharedPtr<FJsonObject>* P = nullptr;
	if (J->TryGetObjectField(TEXT("prices"), P))
	{
		(*P)->TryGetNumberField(TEXT("wall_m2"), WallM2); (*P)->TryGetNumberField(TEXT("take_down_m2"), TakeDownM2); (*P)->TryGetNumberField(TEXT("move_m2"), MoveM2);
		(*P)->TryGetNumberField(TEXT("doorway"), Doorway); (*P)->TryGetNumberField(TEXT("close_doorway"), CloseDoorway); (*P)->TryGetNumberField(TEXT("paint_m2"), PaintM2);
	}
	return true;
}

namespace BrixRoomPlan
{
	FVector4f Box(const FBrixRoomPlanRules& R, const FBrixPlanWall& W)
	{
		const float h = (float)R.Thick / 2;
		return W.Ax == TEXT("x") ? FVector4f(W.A0, W.Fix - h, W.A1, W.Fix + h) : FVector4f(W.Fix - h, W.A0, W.Fix + h, W.A1);
	}

	double Gridded(const FBrixRoomPlanRules& R, double V) { return FCString::Atod(*FString::Printf(TEXT("%.2f"), FMath::RoundHalfFromZero(V / R.Grid) * R.Grid)); }

	TArray<TPair<FString, FString>> Problems(const FBrixRoomPlanRules& R, const FBrixRoomSpec& S, const TArray<FString>& Colours, const FBrixPlan& Walls)
	{
		TArray<TPair<FString, FString>> Out;
		const double g = R.Grid;
		auto OffGrid = [g](double v) { return FMath::Abs(v / g - FMath::RoundHalfFromZero(v / g)) > 1e-6; };
		const FVector4f& Rm = S.Room;
		for (const FBrixPlanWall& W : Walls)
		{
			const FString K = W.Id + TEXT("|");
			if (W.Ax != TEXT("x") && W.Ax != TEXT("z")) { Out.Add({ K + TEXT("ax"), TEXT("a wall must run along x or z") }); continue; }
			if (W.A1 - W.A0 < R.MinLen - Tol) Out.Add({ K + TEXT("short"), TEXT("a wall is shorter than ") + Fmt1(R.MinLen) + TEXT(" m") });
			if (OffGrid(W.Fix) || OffGrid(W.A0) || OffGrid(W.A1) || (W.Door && OffGrid(*W.Door))) Out.Add({ K + TEXT("grid"), FString::Printf(TEXT("a wall is off the %d cm grid"), (int32)FMath::RoundHalfFromZero(g * 100)) });
			const bool bX = W.Ax == TEXT("x");
			const double Lo = bX ? Rm.X : Rm.Y, Hi = bX ? Rm.Z : Rm.W, Flo = bX ? Rm.Y : Rm.X, Fhi = bX ? Rm.W : Rm.Z;
			if (W.A0 < Lo - Tol || W.A1 > Hi + Tol || !(Flo + Tol < W.Fix && W.Fix < Fhi - Tol)) Out.Add({ K + TEXT("room"), TEXT("a wall runs out of the room") });
			if (W.Door && !(W.A0 + R.DoorW / 2 + R.DoorMargin - Tol <= *W.Door && *W.Door <= W.A1 - R.DoorW / 2 - R.DoorMargin + Tol)) Out.Add({ K + TEXT("door"), TEXT("a doorway does not fit in its wall") });
			if (!W.Colour.IsEmpty() && !Colours.Contains(W.Colour)) Out.Add({ K + TEXT("colour"), TEXT("that wall colour is not one of the district's") });
			const FVector4f Bx = Box(R, W);
			if (S.Keep.ContainsByPredicate([&](const FVector4f& Kp) { return Meet(Bx, Kp); })) Out.Add({ K + TEXT("keep"), TEXT("a wall stands in the way of the door or the stairs") });
		}
		FBrixPlan Ok;
		for (const FBrixPlanWall& W : Walls) if (W.Ax == TEXT("x") || W.Ax == TEXT("z")) Ok.Add(W);
		for (int32 i = 0; i < Ok.Num(); ++i)
			for (int32 j = i + 1; j < Ok.Num(); ++j)
				if (Meet(Box(R, Ok[i]), Box(R, Ok[j])) && Ok[i].Ax == Ok[j].Ax)
				{
					const bool bLess = FCString::Strcmp(*Ok[i].Id, *Ok[j].Id) < 0;
					Out.Add({ (bLess ? Ok[i].Id : Ok[j].Id) + TEXT("|") + (bLess ? Ok[j].Id : Ok[i].Id) + TEXT("|clash"), TEXT("two walls stand in each other") });
				}
		if (S.Entries.Num())
		{
			TArray<bool> Got;
			TArray<double> Pockets;
			Reach(R, Rm, Ok, S.Entries, Got, Pockets);
			bool bAll = true;
			for (int32 k = 1; k < Got.Num(); ++k) bAll &= Got[k];
			if (!bAll) Out.Add({ TEXT("reach|stairs"), TEXT("the stairs cannot be reached from the way in") });
			if (!Got[0]) Out.Add({ TEXT("reach|door"), TEXT("a wall stands where you come in") });
			if (Pockets.ContainsByPredicate([&](double a) { return a >= R.PocketM2 - Tol; })) Out.Add({ TEXT("reach|room"), TEXT("a room cannot be reached from the way in") });
		}
		return Out;
	}

	int64 Cost(const FBrixRoomPlanRules& R, const FBrixPlan& Before, const FBrixPlan& After, double Height, TArray<FBrixPlanLine>& Lines)
	{
		Lines.Reset();
		auto Area = [Height](const FBrixPlanWall& W) { return Q2((W.A1 - W.A0) * Height); };
		auto Paint = [&](const FBrixPlanWall& W) { Lines.Add({ TEXT("Paint a wall"), Q2(Area(W) * 2), Chips(Area(W) * 2 * R.PaintM2) }); };
		for (const FBrixPlanWall& W : After)
		{
			const FBrixPlanWall* O = Before.FindByPredicate([&](const FBrixPlanWall& X) { return X.Id == W.Id; });
			if (!O)
			{
				Lines.Add({ TEXT("New wall"), Area(W), Chips(Area(W) * R.WallM2) });
				if (W.Door) Lines.Add({ TEXT("New doorway"), 1, (int64)R.Doorway });
				if (!W.Colour.IsEmpty()) Paint(W);
				continue;
			}
			if (O->Ax != W.Ax || O->Fix != W.Fix || O->A0 != W.A0 || O->A1 != W.A1) Lines.Add({ TEXT("Move a wall"), Area(W), Chips(Area(W) * R.MoveM2) });
			if (O->Door != W.Door) Lines.Add(W.Door ? FBrixPlanLine{ TEXT("New doorway"), 1, (int64)R.Doorway } : FBrixPlanLine{ TEXT("Close a doorway"), 1, (int64)R.CloseDoorway });
			if (O->Colour != W.Colour) Paint(W);
		}
		for (const FBrixPlanWall& W : Before)
			if (!After.ContainsByPredicate([&](const FBrixPlanWall& X) { return X.Id == W.Id; })) Lines.Add({ TEXT("Take down a wall"), Area(W), Chips(Area(W) * R.TakeDownM2) });
		int64 T = 0;
		for (const FBrixPlanLine& L : Lines) T += L.Chips;
		return T;
	}

	TArray<FString> Check(const FBrixRoomPlanRules& R, const FBrixRoomSpec& S, const TArray<FString>& Colours, const FBrixPlan& Before, const FBrixPlan& After, TOptional<int64> Funds)
	{
		TSet<FString> Old;
		for (const auto& P : Problems(R, S, Colours, Before)) Old.Add(P.Key);
		TArray<FString> Out;
		for (const auto& P : Problems(R, S, Colours, After)) if (!Old.Contains(P.Key)) Out.AddUnique(P.Value);
		TArray<FBrixPlanLine> L;
		const int64 Total = Cost(R, Before, After, S.Ceiling, L);
		if (Funds && Total > *Funds) Out.Add(FString::Printf(TEXT("you cannot pay %lld.%02lld brix"), Total / 100, Total % 100));
		return Out;
	}

	FBrixPlanWall WallFromJson(const TSharedPtr<FJsonObject>& O)
	{
		FBrixPlanWall W;
		O->TryGetStringField(TEXT("id"), W.Id); O->TryGetStringField(TEXT("ax"), W.Ax);
		O->TryGetNumberField(TEXT("fix"), W.Fix); O->TryGetNumberField(TEXT("a0"), W.A0); O->TryGetNumberField(TEXT("a1"), W.A1);
		double D = 0;
		if (O->TryGetNumberField(TEXT("door"), D)) W.Door = D;
		O->TryGetStringField(TEXT("colour"), W.Colour);
		return W;
	}

	TSharedPtr<FJsonObject> WallToJson(const FBrixPlanWall& W)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), W.Id); O->SetStringField(TEXT("ax"), W.Ax);
		O->SetNumberField(TEXT("fix"), W.Fix); O->SetNumberField(TEXT("a0"), W.A0); O->SetNumberField(TEXT("a1"), W.A1);
		if (W.Door) O->SetNumberField(TEXT("door"), *W.Door); else O->SetField(TEXT("door"), MakeShared<FJsonValueNull>());
		if (!W.Colour.IsEmpty()) O->SetStringField(TEXT("colour"), W.Colour); else O->SetField(TEXT("colour"), MakeShared<FJsonValueNull>());
		return O;
	}

	FBrixPlan Apply(const FBrixPlan& Before, const TSharedPtr<FJsonObject>& Edit)
	{
		TArray<FString> Remove;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (Edit->TryGetArrayField(TEXT("remove"), A)) for (const auto& V : *A) Remove.Add(V->AsString());
		const TSharedPtr<FJsonObject>* Set = nullptr;
		Edit->TryGetObjectField(TEXT("set"), Set);
		FBrixPlan Out;
		for (const FBrixPlanWall& W : Before)
		{
			if (Remove.Contains(W.Id)) continue;
			FBrixPlanWall N = W;
			const TSharedPtr<FJsonObject>* Ch = nullptr;
			if (Set && (*Set)->TryGetObjectField(W.Id, Ch))
				for (const auto& KV : (*Ch)->Values)
				{
					const FString K(KV.Key);
					const bool bNull = !KV.Value.IsValid() || KV.Value->IsNull();
					if (K == TEXT("fix")) N.Fix = KV.Value->AsNumber();
					else if (K == TEXT("a0")) N.A0 = KV.Value->AsNumber();
					else if (K == TEXT("a1")) N.A1 = KV.Value->AsNumber();
					else if (K == TEXT("ax")) N.Ax = KV.Value->AsString();
					else if (K == TEXT("door")) N.Door = bNull ? TOptional<double>() : TOptional<double>(KV.Value->AsNumber());
					else if (K == TEXT("colour")) N.Colour = bNull ? FString() : KV.Value->AsString();
				}
			Out.Add(N);
		}
		if (Edit->TryGetArrayField(TEXT("add"), A)) for (const auto& V : *A) Out.Add(WallFromJson(V->AsObject()));
		return Out;
	}

	FBrixPlan PlanOf(const TArray<FBrixPartition>& Segs)
	{
		struct FSeg { double A0, A1; bool bDoor; };
		TArray<FString> Keys;
		TMap<FString, TArray<FSeg>> Runs;
		for (const FBrixPartition& P : Segs)
		{
			const FVector2D Sz = P.Max - P.Min;
			const bool bX = Sz.X >= Sz.Y;
			const double Fix = bX ? (P.Min.Y + P.Max.Y) / 2 : (P.Min.X + P.Max.X) / 2;
			const FString K = FString::Printf(TEXT("%s:%.3f"), bX ? TEXT("x") : TEXT("z"), Fix);
			if (!Runs.Contains(K)) Keys.Add(K);
			Runs.FindOrAdd(K).Add({ bX ? P.Min.X : P.Min.Y, bX ? P.Max.X : P.Max.Y, P.bDoorway });
		}
		FBrixPlan Out;
		for (const FString& K : Keys)
		{
			FString Ax, Fx;
			K.Split(TEXT(":"), &Ax, &Fx);
			const double Fix = FCString::Atod(*Fx);
			TArray<FSeg> L = Runs[K];
			L.StableSort([](const FSeg& a, const FSeg& b) { return a.A0 < b.A0; });
			bool bRun = false;
			double RA0 = 0, RA1 = 0;
			TArray<double> Doors;
			auto Flush = [&]()
			{
				if (!bRun) return;
				if (Doors.Num() <= 1) { FBrixPlanWall W; W.Ax = Ax; W.Fix = Fix; W.A0 = RA0; W.A1 = RA1; if (Doors.Num()) W.Door = Doors[0]; Out.Add(W); }
				else
				{
					double a = RA0;
					for (int32 i = 0; i < Doors.Num(); ++i)
					{
						const double b = i + 1 < Doors.Num() ? (Doors[i] + Doors[i + 1]) / 2 : RA1;
						FBrixPlanWall W; W.Ax = Ax; W.Fix = Fix; W.A0 = a; W.A1 = b; W.Door = Doors[i]; Out.Add(W);
						a = b;
					}
				}
				bRun = false;
				Doors.Reset();
			};
			for (const FSeg& S : L)
			{
				if (bRun && S.A0 - RA1 > .02) Flush();
				if (!bRun) { bRun = true; RA0 = S.A0; RA1 = S.A1; }
				RA1 = FMath::Max(RA1, S.A1);
				if (S.bDoor) Doors.Add(Fixed3((S.A0 + S.A1) / 2));
			}
			Flush();
		}
		for (int32 i = 0; i < Out.Num(); ++i) { Out[i].Id = FString::Printf(TEXT("w%d"), i + 1); Out[i].Fix = Fixed3(Out[i].Fix); Out[i].A0 = Fixed3(Out[i].A0); Out[i].A1 = Fixed3(Out[i].A1); }
		return Out;
	}

	TArray<FBrixPartition> SegsOf(const FBrixRoomPlanRules& R, const FBrixPlan& Plan)
	{
		TArray<FBrixPartition> Out;
		const double h = R.Thick / 2;
		auto Seg = [&](const FBrixPlanWall& W, double A, double B, bool bDoor)
		{
			FBrixPartition P;
			P.bDoorway = bDoor;
			P.Min = W.Ax == TEXT("x") ? FVector2D(A, W.Fix - h) : FVector2D(W.Fix - h, A);
			P.Max = W.Ax == TEXT("x") ? FVector2D(B, W.Fix + h) : FVector2D(W.Fix + h, B);
			Out.Add(P);
		};
		for (const FBrixPlanWall& W : Plan)
		{
			if (!W.Door) { Seg(W, W.A0, W.A1, false); continue; }
			const double G0 = *W.Door - R.DoorW / 2, G1 = *W.Door + R.DoorW / 2;
			if (G0 - W.A0 > .02) Seg(W, W.A0, G0, false);
			Seg(W, G0, G1, true);
			if (W.A1 - G1 > .02) Seg(W, G1, W.A1, false);
		}
		return Out;
	}
}
