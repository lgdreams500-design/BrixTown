#include "BrixHomes.h"

#include "BrixCity.h"
#include "BrixInside.h"
#include "BrixDay.h"
#include "Dom/JsonObject.h"

namespace
{
	constexpr float PART = 0.12f, DH = 0.45f, SILL_TALL = 1.05f;   // homes.js: a partition's thickness, half a doorway, the sill rule

	// homes.js / houses.js mapA(): the plan (inner faces of 0.25 m walls) to the home: within each room a point keeps its
	// distance to the nearer of the room's two walls
	float MapA(float X, const TArray<float>& O, const TArray<float>& N)
	{
		if (!O.Num() || O.Num() != N.Num()) return X;
		if (X <= O[0]) return N[0] + X - O[0];
		for (int32 i = 0; i + 1 < O.Num(); ++i)
			if (X <= O[i + 1]) return X - O[i] <= O[i + 1] - X ? N[i] + X - O[i] : N[i + 1] - (O[i + 1] - X);
		return N.Last() + X - O.Last();
	}
	float Round3(float X) { return FMath::RoundToFloat(X * 1000.f) / 1000.f; }

	TArray<float> Floats(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		TArray<float> Out;
		for (const TSharedPtr<FJsonValue>& V : A) Out.Add((float)V->AsNumber());
		return Out;
	}

	bool IsSeat(const FString& K) { return K == TEXT("k17:sofa") || K == TEXT("k17:dining") || K == TEXT("k17:armchair"); }
	bool IsWet(const FString& Name) { return Name.Contains(TEXT("Kitchen")) || Name.Contains(TEXT("Bathroom")) || Name.Contains(TEXT("WC")); }

	// a rectangle given by two opposite corners in a frame whose axes in the world are Ex, Ey (orthonormal, maybe mirrored)
	FBrixRect RectOf(const FVector2D& C0, const FVector2D& C1, const FVector2D& Ex, const FVector2D& Ey)
	{
		FBrixRect R;
		R.Centre = (C0 + C1) * 0.5;
		const FVector2D D = C1 - C0;
		const float Lx = (float)FMath::Abs(FVector2D::DotProduct(D, Ex)), Ly = (float)FMath::Abs(FVector2D::DotProduct(D, Ey));
		if (Lx >= Ly) { R.Along = Ex; R.Length = Lx; R.Width = Ly; }
		else { R.Along = Ey; R.Length = Ly; R.Width = Lx; }
		return R;
	}

	// a household's level today: the furnishing rule on its wages, or the level of a home nobody lives in
	int32 LevelOf(const FBrixInsides& Ins, const FBrixInside& In, int32 Beds, int32 Day)
	{
		return (In.Residents > 0 || Beds > 0) ? Ins.Furnish.Level(In.Wage, Day) : Ins.Furnish.EmptyLevel;
	}

	// homes.js dropped(): a tall or hung piece whose footprint stands within 0.35 m of a window of its floor, over 0.3 m of it
	bool InFrontOfWindow(const FBrixFurniturePiece& P, const FVector4f& Box, const TArray<FBrixWindowSpan>& Windows)
	{
		const FVector2D Cn[4] = { P.At + P.A * Box.X + P.B * Box.Z, P.At + P.A * Box.Y + P.B * Box.Z, P.At + P.A * Box.Y + P.B * Box.W, P.At + P.A * Box.X + P.B * Box.W };
		for (const FBrixWindowSpan& W : Windows)
		{
			if (W.Floor != P.Floor) continue;
			const FVector2D D = W.B - W.A;
			const double L = D.Size();
			if (L < 1e-3) continue;
			const FVector2D U = D / L, N(-U.Y, U.X);
			double T0 = 1e9, T1 = -1e9, Near = 1e9;
			for (const FVector2D& C : Cn)
			{
				const double T = FVector2D::DotProduct(C - W.A, U);
				T0 = FMath::Min(T0, T); T1 = FMath::Max(T1, T);
				Near = FMath::Min(Near, FMath::Abs(FVector2D::DotProduct(C - W.A, N)));
			}
			if (Near < 0.35 && T1 > 0.3 && T0 < L - 0.3) return true;
		}
		return false;
	}
}

int32 FBrixFurnishRules::Level(int64 Wage, int32 Day) const
{
	int32 Lv = 1;
	for (const TPair<int64, int32>& B : StartBands) if (Wage >= B.Key) Lv = FMath::Max(Lv, B.Value);
	const int64 Saved = (Wage * SaveBp / 10000) * FMath::Max(0, Day);
	int64 Spent = 0;
	while (Lv < Top && StepCost.IsValidIndex(Lv - 1) && Saved >= Spent + StepCost[Lv - 1]) { Spent += StepCost[Lv - 1]; ++Lv; }
	return Lv;
}

void LoadBrixHomeData(const TSharedPtr<FJsonObject>& Root, FBrixInsides& Out)
{
	const TSharedPtr<FJsonObject>* Fu = nullptr;
	if (Root->TryGetObjectField(TEXT("furnishing"), Fu))
	{
		FBrixFurnishRules& F = Out.Furnish;
		F.Top = (*Fu)->GetArrayField(TEXT("levels")).Num();
		for (const TSharedPtr<FJsonValue>& B : (*Fu)->GetArrayField(TEXT("start_bands")))
			F.StartBands.Add({ (int64)B->AsObject()->GetNumberField(TEXT("from_wage")), (int32)B->AsObject()->GetNumberField(TEXT("level")) });
		F.SaveBp = (int64)(*Fu)->GetNumberField(TEXT("save_bp"));
		for (const TSharedPtr<FJsonValue>& C : (*Fu)->GetArrayField(TEXT("step_cost"))) F.StepCost.Add((int64)C->AsNumber());
		F.LodgerLevel = (int32)(*Fu)->GetNumberField(TEXT("lodger_level"));
		F.EmptyLevel = (int32)(*Fu)->GetNumberField(TEXT("empty_level"));
	}
	const TSharedPtr<FJsonObject>* Sz = nullptr;
	if (Root->TryGetObjectField(TEXT("sizes"), Sz))
		for (const auto& KV : (*Sz)->Values) { const TArray<float> A = Floats(KV.Value->AsArray()); if (A.Num() >= 3) Out.Sizes.Add(FString(KV.Key), FVector3f(A[0], A[1], A[2])); }
	const TSharedPtr<FJsonObject>* Hg = nullptr;
	if (Root->TryGetObjectField(TEXT("hung"), Hg))
		for (const auto& KV : (*Hg)->Values) Out.Hung.Add(FString(KV.Key), Floats(KV.Value->AsArray()));
	const TSharedPtr<FJsonObject>* Pl = nullptr;
	if (!Root->TryGetObjectField(TEXT("home_plans"), Pl)) return;
	for (const auto& KV : (*Pl)->Values)
	{
		const TSharedPtr<FJsonObject> P = KV.Value->AsObject();
		const TSharedPtr<FJsonObject> H = P->GetObjectField(TEXT("house"));
		FBrixHomePlan Plan;
		Plan.Key = FString(KV.Key);
		Plan.W0 = (float)H->GetNumberField(TEXT("W0")); Plan.W1 = (float)H->GetNumberField(TEXT("W1"));
		Plan.D0 = (float)H->GetNumberField(TEXT("D0")); Plan.D1 = (float)H->GetNumberField(TEXT("D1"));
		Plan.T = (float)H->GetNumberField(TEXT("T"));
		double V = 0;
		if (H->TryGetNumberField(TEXT("XA"), V)) Plan.XA = (float)V;
		if (H->TryGetNumberField(TEXT("XB"), V)) Plan.XB = (float)V;
		if (H->TryGetNumberField(TEXT("YM"), V)) Plan.YM = (float)V;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (H->TryGetArrayField(TEXT("stair"), A)) Plan.Stair = Floats(*A);
		if (H->TryGetArrayField(TEXT("floors"), A)) for (const TSharedPtr<FJsonValue>& F : *A) Plan.Floors.Add(F->AsString());
		else Plan.Floors = { TEXT("ground"), TEXT("upstairs") };
		const TSharedPtr<FJsonObject>* O = nullptr;
		if (H->TryGetObjectField(TEXT("partitions"), O))
			for (const auto& S : (*O)->Values)
				for (const TSharedPtr<FJsonValue>& L : S.Value->AsArray())
				{
					const TArray<TSharedPtr<FJsonValue>>& Q = L->AsArray();
					Plan.Partitions.FindOrAdd(FString(S.Key)).Add({ Q[0]->AsString(), (float)Q[1]->AsNumber(), (float)Q[2]->AsNumber(), (float)Q[3]->AsNumber() });
				}
		if (H->TryGetObjectField(TEXT("doors"), O))
			for (const auto& S : (*O)->Values)
				for (const TSharedPtr<FJsonValue>& L : S.Value->AsArray())
				{
					const TArray<TSharedPtr<FJsonValue>>& Q = L->AsArray();
					Plan.Doors.FindOrAdd(FString(S.Key)).Add({ Q[0]->AsString(), (float)Q[1]->AsNumber(), (float)Q[2]->AsNumber() });
				}
		if (H->TryGetObjectField(TEXT("rooms"), O))
			for (const auto& S : (*O)->Values)
				for (const auto& R : S.Value->AsObject()->Values)
				{
					const TArray<float> Q = Floats(R.Value->AsArray());
					if (Q.Num() >= 4) Plan.Rooms.FindOrAdd(FString(S.Key)).Add({ FString(R.Key), FVector4f(Q[0], Q[1], Q[2], Q[3]) });
				}
		if (H->TryGetObjectField(TEXT("spots"), O))
			for (const auto& S : (*O)->Values)
				for (const TSharedPtr<FJsonValue>& L : S.Value->AsArray()) Plan.Spots.FindOrAdd(FString(S.Key)).Add(Floats(L->AsArray()));
		if (H->TryGetArrayField(TEXT("beds"), A)) for (const TSharedPtr<FJsonValue>& B : *A) Plan.Beds.Add(B->AsString());
		for (const TSharedPtr<FJsonValue>& I : P->GetArrayField(TEXT("items")))
		{
			const TSharedPtr<FJsonObject> It = I->AsObject();
			FBrixPlanItem Item;
			Item.Storey = It->GetStringField(TEXT("storey"));
			It->TryGetStringField(TEXT("room"), Item.Room);
			Item.Piece = It->GetStringField(TEXT("piece"));
			Item.X = (float)It->GetNumberField(TEXT("x")); Item.Y = (float)It->GetNumberField(TEXT("y"));
			Item.Yaw = (float)It->GetNumberField(TEXT("yaw"));
			Item.Level = (int32)It->GetNumberField(TEXT("level"));
			double Lift = 0; if (It->TryGetNumberField(TEXT("lift"), Lift)) Item.Lift = (float)Lift;
			Plan.Items.Add(Item);
		}
		Out.Plans.Add(Plan.Key, MoveTemp(Plan));
	}
}

bool BrixHomeOf(const FBrixInsides& Ins, const FBrixLot& Lot, const FBrixInside& In, int32 Day, const TArray<FBrixWindowSpan>& Windows, FBrixHome& Out)
{
	Out = FBrixHome();
	const FBrixHomePlan* HousePlan = Ins.Plans.Find(TEXT("house_12x9"));
	const TArray<TArray<float>>* NoSpots = nullptr;
	auto SpotsOf = [&](const FString& Kind) -> const TArray<TArray<float>>* { return HousePlan ? HousePlan->Spots.Find(Kind) : NoSpots; };   // homes.js SPOTS: the house plan's
	const int32 Beds = In.Objects.Contains(TEXT("bed")) ? In.Objects[TEXT("bed")].Num() : 0;

	// ------------------------------------------------------------ a designed house (houses.js, plan "samples_19" = house_12x9)
	if (Lot.House.IsSet())
	{
		if (!HousePlan) return false;
		const FBrixHomePlan& P = *HousePlan;
		const float HU = 6.f, HV = 4.5f, SW = 0.32f, SI = HU - SW, SJ = HV - SW, SA = P.XA, SB = P.XB, SM = P.YM;
		const float PI0 = P.W1 - P.T, PJ0 = P.D1 - P.T;
		const TArray<float> UO = { -PI0, SA, SB, PI0 }, UN = { -SI, SA, SB, SI }, VO = { -PJ0, SM, PJ0 }, VN = { -SJ, SM, SJ };
		auto mu = [&](float U) { return Round3(MapA(U, UO, UN)); };
		auto mv = [&](float V) { return Round3(MapA(V, VO, VN)); };
		const FVector2D C = Lot.Centre();
		const double T = FMath::DegreesToRadians(Lot.AngleDeg), Ct = FMath::Cos(T), St = FMath::Sin(T);
		auto Turn = [&](const FVector2D& D) { return FVector2D(D.X * Ct - D.Y * St, D.X * St + D.Y * Ct); };
		auto At = [&](float U, float V) { return C + Turn(FVector2D(-U, V)); };   // houses.js bx1/yb1: u is Blender's x, mirrored
		const FVector2D Eu = Turn(FVector2D(-1, 0)), Ev = Turn(FVector2D(0, 1));
		Out.Plan = P.Key;
		Out.Line = In.HouseLine.IsEmpty() ? FString(TEXT("oldtown")) : In.HouseLine;
		Out.Level = LevelOf(Ins, In, Beds, Day);
		auto FloorOf = [](const FString& S) { return S == TEXT("upstairs") ? 1 : 0; };
		// rooms
		for (const auto& KV : P.Rooms)
			for (const TPair<FString, FVector4f>& R : KV.Value)
			{
				FBrixRoom Rm;
				Rm.Name = R.Key; Rm.Floor = FloorOf(KV.Key); Rm.bWet = IsWet(R.Key);
				Rm.R = RectOf(At(mu(R.Value.X), mv(R.Value.Z)), At(mu(R.Value.Y), mv(R.Value.W)), Eu, Ev);
				Out.Rooms.Add(Rm);
			}
		// the inner walls at XA, XB and YM, their doorways where the plan has them (houses.js insideS)
		for (int32 f = 0; f < 2; ++f)
		{
			TArray<FBrixHomePlan::FDoor> Doors;
			for (const FBrixHomePlan::FDoor& D : P.Doors.FindRef(f ? TEXT("upstairs") : TEXT("ground")))
				Doors.Add({ D.Ax, D.Ax == TEXT("x") ? (FMath::Abs(D.At - P.XA) < 1e-6f ? SA : SB) : SM, D.Ax == TEXT("x") ? mv(D.C) : mu(D.C) });
			const struct { const TCHAR* Ax; float At, A0, A1; } Runs[4] = { { TEXT("x"), SA, -SJ, SJ }, { TEXT("x"), SB, -SJ, SJ }, { TEXT("y"), SM, -SI, SA }, { TEXT("y"), SM, SB, SI } };
			for (const auto& Run : Runs)
			{
				TArray<float> Cuts;
				for (const FBrixHomePlan::FDoor& D : Doors) if (D.Ax == Run.Ax && D.At == Run.At && D.C > Run.A0 && D.C < Run.A1) Cuts.Add(D.C);
				Cuts.Sort();
				const bool bX = FCString::Strcmp(Run.Ax, TEXT("x")) == 0;
				auto Seg = [&](float Pa, float Qa, bool bDoor)
				{
					if (Qa - Pa < 0.02f) return;
					FBrixWallSeg W;
					W.Floor = f; W.bDoorway = bDoor;
					W.R = bX ? RectOf(At(Run.At - PART / 2, Pa), At(Run.At + PART / 2, Qa), Eu, Ev) : RectOf(At(Pa, Run.At - PART / 2), At(Qa, Run.At + PART / 2), Eu, Ev);
					Out.Walls.Add(W);
				};
				float A = Run.A0;
				for (float Cc : Cuts) { Seg(A, Cc - DH, false); Seg(Cc - DH, Cc + DH, true); A = Cc + DH; }
				Seg(A, Run.A1, false);
			}
		}
		// the pieces at the household's level and below, and a bed for each resident (houses.js furnS())
		int32 Need = Beds;
		TSet<int32> Keep;
		for (int32 i = 0; i < P.Items.Num(); ++i)
			if (P.Beds.Contains(P.Items[i].Piece) && Need > 0) { Keep.Add(i); const auto* S = SpotsOf(P.Items[i].Piece); Need -= S ? S->Num() : 1; }
		for (int32 i = 0; i < P.Items.Num(); ++i)
		{
			const FBrixPlanItem& It = P.Items[i];
			if (!(It.Level <= Out.Level || Keep.Contains(i))) continue;
			const float x = mu(It.X), y = mv(It.Y), c = FMath::Cos(It.Yaw), s = FMath::Sin(It.Yaw);
			FBrixFurniturePiece Pc;
			Pc.Kind = It.Piece; Pc.Line = Out.Line; Pc.Floor = FloorOf(It.Storey); Pc.Lift = It.Lift; Pc.Item = i;
			Pc.At = At(x, y);
			Pc.A = Turn(FVector2D(-c, s)); Pc.B = Turn(FVector2D(s, c));
			if (const FVector3f* Z = Ins.Sizes.Find(It.Piece)) Pc.Size = *Z;
			Pc.bBed = P.Beds.Contains(It.Piece);
			Out.Pieces.Add(Pc);
			// where people lie and sit on it (houses.js spotsS(), placesS())
			const bool bLie = Pc.bBed;
			if (!bLie && !IsSeat(It.Piece)) continue;
			if (const auto* S = SpotsOf(It.Piece))
				for (const TArray<float>& Q : *S)
				{
					if (Q.Num() < 5) continue;
					FBrixSpot Sp;
					Sp.At = At(mu(It.X + Q[0] * c - Q[1] * s), mv(It.Y + Q[0] * s + Q[1] * c));
					const FVector2D D = Turn(FVector2D(-(Q[3] * c - Q[4] * s), Q[3] * s + Q[4] * c));
					Sp.Facing = D; Sp.Floor = Pc.Floor; Sp.Up = Q[2]; Sp.bLie = bLie;
					Out.Spots.Add(Sp);
				}
		}
		return true;
	}

	// ------------------------------------------------------------ a planned row house (homes.js home(), one())
	if (In.Interior.IsEmpty() || !In.Frame.IsSet()) return false;
	const FBrixHomePlan* PlanP = Ins.Plans.Find(In.Interior);
	if (!PlanP) return false;
	const FBrixHomePlan& P = *PlanP;
	const FBrixPlanFrame& Fr = In.Frame.GetValue();
	if (Fr.U.Num() < 2 || Fr.V.Num() < 2) return false;
	auto mu = [&](float X) { return MapA(X, Fr.U[0], Fr.U[1]); };
	auto mv = [&](float Y) { return MapA(Y, Fr.V[0], Fr.V[1]); };
	auto W = [&](float X, float Y) { return Fr.O + Fr.X * X + Fr.Y * Y; };
	auto Dv = [&](float X, float Y) { return Fr.X * X + Fr.Y * Y; };
	auto FloorOf = [&](const FString& S) { return P.Floors.IndexOfByKey(S); };
	Out.Plan = P.Key;
	Out.Line = In.Line.IsEmpty() ? FString(TEXT("oldtown")) : In.Line;
	Out.Level = LevelOf(Ins, In, Beds, Day);
	for (const auto& KV : P.Rooms)
		for (const TPair<FString, FVector4f>& R : KV.Value)
		{
			FBrixRoom Rm;
			Rm.Name = R.Key; Rm.Floor = FloorOf(KV.Key); Rm.bWet = IsWet(R.Key);
			Rm.R = RectOf(W(mu(R.Value.X), mv(R.Value.Z)), W(mu(R.Value.Y), mv(R.Value.W)), Fr.X, Fr.Y);
			if (Rm.Floor >= 0) Out.Rooms.Add(Rm);
		}
	for (const auto& KV : P.Partitions)
	{
		const int32 f = FloorOf(KV.Key);
		if (f < 0) continue;
		const TArray<FBrixHomePlan::FDoor> Doors = P.Doors.FindRef(KV.Key);
		for (const FBrixHomePlan::FLine& L : KV.Value)
		{
			TArray<float> Cuts;
			for (const FBrixHomePlan::FDoor& D : Doors) if (D.Ax == L.Ax && FMath::Abs(D.At - L.At) < 1e-3f && D.C > L.A0 && D.C < L.A1) Cuts.Add(D.C);
			Cuts.Sort();
			const bool bX = L.Ax == TEXT("x");
			auto Seg = [&](float Pa, float Qa, bool bDoor)
			{
				if (Qa - Pa < 0.02f) return;
				FBrixWallSeg S;
				S.Floor = f; S.bDoorway = bDoor;
				S.R = bX ? RectOf(W(mu(L.At) - PART / 2, mv(Pa)), W(mu(L.At) + PART / 2, mv(Qa)), Fr.X, Fr.Y)
					: RectOf(W(mu(Pa), mv(L.At) - PART / 2), W(mu(Qa), mv(L.At) + PART / 2), Fr.X, Fr.Y);
				Out.Walls.Add(S);
			};
			float A = L.A0;
			for (float Cc : Cuts) { Seg(A, Cc - DH, false); Seg(Cc - DH, Cc + DH, true); A = Cc + DH; }
			Seg(A, L.A1, false);
		}
	}
	// the pieces: their world places, then the ones in front of a window left out (dropped()), then the level (pieces())
	TArray<FBrixFurniturePiece> All;
	TSet<int32> Drop;
	for (int32 i = 0; i < P.Items.Num(); ++i)
	{
		const FBrixPlanItem& It = P.Items[i];
		const int32 f = FloorOf(It.Storey);
		if (f < 0) continue;
		const float c = FMath::Cos(It.Yaw), s = FMath::Sin(It.Yaw);
		FBrixFurniturePiece Pc;
		Pc.Kind = It.Piece; Pc.Line = Out.Line; Pc.Floor = f; Pc.Lift = It.Lift; Pc.Item = i;
		Pc.At = W(mu(It.X), mv(It.Y));
		Pc.A = Dv(c, s); Pc.B = Dv(-s, c);
		if (const FVector3f* Z = Ins.Sizes.Find(It.Piece)) Pc.Size = *Z;
		Pc.bBed = It.Piece == TEXT("k17:bed") || It.Piece == TEXT("c_bunk") || It.Piece == TEXT("r_single");   // homes.js BED_KINDS
		All.Add(Pc);
		const TArray<float>* Hung = Ins.Hung.Find(It.Piece);
		const bool bTall = !Pc.Size.IsZero() && Pc.Size.Z > SILL_TALL && It.Piece != TEXT("b_toilet");
		if (!(bTall || (Hung && It.Piece != TEXT("d_pendant"))) || Pc.bBed) continue;
		const FVector4f Box = !Pc.Size.IsZero() ? FVector4f(-Pc.Size.X / 2, Pc.Size.X / 2, -Pc.Size.Y / 2, Pc.Size.Y / 2)
			: FVector4f((*Hung)[0], (*Hung)[1], (*Hung)[2], (*Hung)[3]);
		if (InFrontOfWindow(Pc, Box, Windows)) { Drop.Add(i); ++Out.Dropped; }
	}
	int32 Need = Beds;
	TSet<int32> Keep;
	for (const FBrixFurniturePiece& Pc : All)
		if (Pc.bBed && Need > 0) { Keep.Add(Pc.Item); const auto* S = SpotsOf(Pc.Kind); Need -= S ? S->Num() : 1; }
	for (const FBrixFurniturePiece& Pc : All)
	{
		if (Drop.Contains(Pc.Item) || !(P.Items[Pc.Item].Level <= Out.Level || Keep.Contains(Pc.Item))) continue;
		Out.Pieces.Add(Pc);
		const FBrixPlanItem& It = P.Items[Pc.Item];
		if (!Pc.bBed && !IsSeat(It.Piece)) continue;
		const float c = FMath::Cos(It.Yaw), s = FMath::Sin(It.Yaw);
		if (const auto* S = SpotsOf(It.Piece))
			for (const TArray<float>& Q : *S)
			{
				if (Q.Num() < 5) continue;
				FBrixSpot Sp;
				Sp.At = W(mu(It.X + Q[0] * c - Q[1] * s), mv(It.Y + Q[0] * s + Q[1] * c));
				const FVector2D D = Dv(Q[3] * c - Q[4] * s, Q[3] * s + Q[4] * c);
				Sp.Facing = D; Sp.Floor = Pc.Floor; Sp.Up = Q[2]; Sp.bLie = Pc.bBed;
				Out.Spots.Add(Sp);
			}
	}
	return true;
}

bool BrixHouseWell(const FBrixInsides& Ins, FVector4f& Well, float& StepOff)
{
	const FBrixHomePlan* P = Ins.Plans.Find(TEXT("house_12x9"));
	if (!P || P->Stair.Num() < 4) return false;
	const float HV = 4.5f, SW = 0.32f, SJ = HV - SW, SM = P->YM, PJ0 = P->D1 - P->T;
	const TArray<float> VO = { -PJ0, SM, PJ0 }, VN = { -SJ, SM, SJ };
	Well = FVector4f(P->XB - PART / 2 - (P->Stair[1] - P->Stair[0]), P->XB - PART / 2, Round3(MapA(P->Stair[2], VO, VN)), SJ);
	StepOff = SJ - 1.1f;
	return true;
}

int32 SeatBrixDayIndoors(FBrixDay& Day, const FBrixCity& City, const FBrixInsides& Ins, const FBrixDoorRules& R)
{
	TArray<const FBrixLot*> Lots;
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) Lots.Add(&L);
	TMap<int32, TSharedPtr<FBrixHome>> Homes;
	const TArray<FBrixWindowSpan> NoWindows;
	int32 Placed = 0;
	for (TArray<FBrixDay::FPiece>& Row : Day.Plan)
		for (FBrixDay::FPiece& P : Row)
		{
			if (P.Kind != 0 || !P.bInside || !Lots.IsValidIndex(P.Lot)) continue;
			const FBrixLot& L = *Lots[P.Lot];
			P.Pose = 0;
			P.SeatUp = BrixFloorUp(L, P.Floor, R);
			if (P.Use != 0 && P.Use != 1) continue;          // build_play.py USES: 0 bed, 1 room
			TSharedPtr<FBrixHome>* Found = Homes.Find(P.Lot);
			if (!Found)
			{
				TSharedPtr<FBrixHome> H;
				if (Ins.Lots.IsValidIndex(P.Lot) && Ins.Lots[P.Lot].IsSet())
				{
					H = MakeShared<FBrixHome>();
					if (!BrixHomeOf(Ins, L, Ins.Lots[P.Lot].GetValue(), Day.Day, NoWindows, *H)) H.Reset();
				}
				Found = &Homes.Add(P.Lot, H);
			}
			if (!Found->IsValid()) continue;
			const bool bLie = P.Use == 0;
			const FBrixSpot* Best = nullptr;
			for (const FBrixSpot& S : (*Found)->Spots)
				if (S.bLie == bLie && S.Floor == P.Floor && (!Best || FVector2D::DistSquared(S.At, P.Pos) < FVector2D::DistSquared(Best->At, P.Pos))) Best = &S;
			if (!Best) continue;
			P.Pose = bLie ? 3 : 2;
			P.SeatUp += Best->Up;
			P.Facing = Best->Facing;
			P.Pos = Best->At;                                // on the piece itself (the lot's place is the same point, add_homes_01.py)
			++Placed;
		}
	return Placed;
}
