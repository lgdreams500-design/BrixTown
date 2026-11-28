#include "BrixTubes.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	TSharedPtr<FJsonObject> TubeReadJson(const FString& Path, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = TEXT("cannot read ") + Path; return nullptr; }
		TSharedPtr<FJsonObject> J;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid()) { OutError = TEXT("not JSON: ") + Path; return nullptr; }
		return J;
	}

	bool TubeNum(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double& V) { return O.IsValid() && O->TryGetNumberField(K, V); }

	FVector2D TubeVec2(const TArray<TSharedPtr<FJsonValue>>& A) { return A.Num() >= 2 ? FVector2D(A[0]->AsNumber(), A[1]->AsNumber()) : FVector2D::ZeroVector; }
	FVector TubeVec3(const TArray<TSharedPtr<FJsonValue>>& A) { return A.Num() >= 3 ? FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber()) : FVector::ZeroVector; }

	FLinearColor TubeColour(const TSharedPtr<FJsonObject>& O, const TCHAR* K, const FLinearColor& Else)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O.IsValid() || !O->TryGetArrayField(K, A) || A->Num() < 3) return Else;
		return FLinearColor((float)(*A)[0]->AsNumber(), (float)(*A)[1]->AsNumber(), (float)(*A)[2]->AsNumber());
	}

	double TubeSmooth(double F) { F = FMath::Clamp(F, 0.0, 1.0); return F * F * (3 - 2 * F); }

	double TubeLineLength(const TArray<FVector>& L)
	{
		double S = 0;
		for (int32 i = 1; i < L.Num(); ++i) S += FVector::Dist(L[i - 1], L[i]);
		return S;
	}

	struct FTubeEdge { FString To; double Length; TArray<FVector> Line; };

	TMap<FString, TArray<FTubeEdge>> TubeEdges(const FBrixTubeNet& Net, double Bend)
	{
		TMap<FString, TArray<FTubeEdge>> E;
		for (const FBrixTube& T : Net.Tubes)
		{
			TArray<FVector> L = BrixTubeRounded(T.Line, Bend);
			const double Len = TubeLineLength(L);
			TArray<FVector> Back = L;
			Algo::Reverse(Back);
			E.FindOrAdd(T.A).Add({ T.B, Len, L });
			E.FindOrAdd(T.B).Add({ T.A, Len, Back });
		}
		return E;
	}
}

bool LoadBrixTubeNet(const FString& Path, FBrixTubeNet& Out, FString& OutError)
{
	const TSharedPtr<FJsonObject> J = TubeReadJson(Path, OutError);
	if (!J) return false;
	Out = FBrixTubeNet();
	const TSharedPtr<FJsonObject>* Stops = nullptr;
	if (!J->TryGetObjectField(TEXT("stops"), Stops)) { OutError = TEXT("no stops in ") + Path; return false; }
	for (const auto& KV : (*Stops)->Values)
	{
		const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
		FBrixTubeStop S;
		S.Id = KV.Key;
		O->TryGetStringField(TEXT("name"), S.Name);
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O->TryGetArrayField(TEXT("pos"), A)) S.Pos = TubeVec2(*A);
		if (O->TryGetArrayField(TEXT("face"), A)) S.Face = TubeVec2(*A).GetSafeNormal();
		Out.Stops.Add(S.Id, S);
		Out.StopOrder.Add(S.Id);
	}
	const TSharedPtr<FJsonObject>* Js = nullptr;
	if (J->TryGetObjectField(TEXT("junctions"), Js))
	{
		for (const auto& KV : (*Js)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (KV.Value->AsObject()->TryGetArrayField(TEXT("pos"), A)) Out.Junctions.Add(FString(KV.Key), TubeVec3(*A));
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Tubes = nullptr;
	if (!J->TryGetArrayField(TEXT("tubes"), Tubes)) { OutError = TEXT("no tubes in ") + Path; return false; }
	for (const TSharedPtr<FJsonValue>& V : *Tubes)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FBrixTube T;
		T.A = O->GetStringField(TEXT("a"));
		T.B = O->GetStringField(TEXT("b"));
		const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
		if (O->TryGetArrayField(TEXT("line"), L)) for (const TSharedPtr<FJsonValue>& P : *L) T.Line.Add(TubeVec3(P->AsArray()));
		Out.Tubes.Add(T);
	}
	return true;
}

bool LoadBrixTubeRules(const FString& Path, FBrixTubeRules& Out, FString& OutError)
{
	const TSharedPtr<FJsonObject> J = TubeReadJson(Path, OutError);
	if (!J) return false;
	Out = FBrixTubeRules();
	TubeNum(J, TEXT("clearance_m"), Out.ClearanceM);
	TubeNum(J, TEXT("tube_inner_m"), Out.TubeInnerM);
	const TSharedPtr<FJsonObject>* O = nullptr;
	if (!J->TryGetObjectField(TEXT("ride"), O)) { OutError = TEXT("no ride in ") + Path; return false; }
	TubeNum(*O, TEXT("draw_in_s"), Out.DrawInS);
	TubeNum(*O, TEXT("draw_in_height_m"), Out.DrawInHeightM);
	TubeNum(*O, TEXT("speed_mps"), Out.SpeedMps);
	TubeNum(*O, TEXT("ease_s"), Out.EaseS);
	TubeNum(*O, TEXT("pop_out_s"), Out.PopOutS);
	TubeNum(*O, TEXT("pop_out_arc_m"), Out.PopOutArcM);
	TubeNum(*O, TEXT("floor_m"), Out.FloorM);
	TubeNum(*O, TEXT("turn_window_m"), Out.TurnWindowM);
	TubeNum(*O, TEXT("bend_r_m"), Out.BendRM);
	TubeNum(*O, TEXT("junction_bend_r_m"), Out.JunctionBendRM);
	if (J->TryGetObjectField(TEXT("camera"), O))
	{
		TubeNum(*O, TEXT("behind_m"), Out.BehindM);
		TubeNum(*O, TEXT("above_m"), Out.AboveM);
		TubeNum(*O, TEXT("follow_s"), Out.FollowS);
		TubeNum(*O, TEXT("max_jump_m"), Out.MaxJumpM);
	}
	if (J->TryGetObjectField(TEXT("look"), O))
	{
		TubeNum(*O, TEXT("booth_r"), Out.BoothR);
		TubeNum(*O, TEXT("booth_h"), Out.BoothH);
		TubeNum(*O, TEXT("plinth_r"), Out.PlinthR);
		TubeNum(*O, TEXT("plinth_h"), Out.PlinthH);
		TubeNum(*O, TEXT("funnel_h"), Out.FunnelH);
		TubeNum(*O, TEXT("ring_every_m"), Out.RingEveryM);
		TubeNum(*O, TEXT("post_every_m"), Out.PostEveryM);
		TubeNum(*O, TEXT("door_deg"), Out.DoorDeg);
		TubeNum(*O, TEXT("glass_alpha"), Out.GlassAlpha);
		TubeNum(*O, TEXT("glass_edge"), Out.GlassEdge);
		Out.Glass = TubeColour(*O, TEXT("glass"), Out.Glass);
		Out.Brass = TubeColour(*O, TEXT("brass"), Out.Brass);
	}
	return true;
}

TArray<FVector> BrixTubeRounded(const TArray<FVector>& Line, double R, double StepDeg)
{
	if (Line.Num() < 3 || R <= 0) return Line;
	TArray<FVector> Out = { Line[0] };
	for (int32 i = 1; i + 1 < Line.Num(); ++i)
	{
		const FVector P = Out.Last(), C = Line[i], Q = Line[i + 1];
		const double A = FVector::Dist(P, C), B = FVector::Dist(C, Q);
		if (A < 1e-9 || B < 1e-9) continue;
		const FVector U = (P - C) / A, V = (Q - C) / B;     // back along the leg in, on along the leg out
		const double Turn = PI - FMath::Acos(FMath::Clamp(FVector::DotProduct(U, V), -1.0, 1.0));
		if (Turn < FMath::DegreesToRadians(1.0)) { Out.Add(C); continue; }
		const double T = FMath::Min3(R * FMath::Tan(Turn / 2), A * 0.5, B * 0.5);
		const FVector S0 = C + U * T, S1 = C + V * T;
		const int32 N = FMath::Max(2, FMath::CeilToInt(FMath::RadiansToDegrees(Turn) / StepDeg));
		for (int32 k = 0; k <= N; ++k)                       // a quadratic arc through the corner, tangent to both legs
		{
			const double F = (double)k / N;
			Out.Add((1 - F) * (1 - F) * S0 + 2 * (1 - F) * F * C + F * F * S1);
		}
	}
	Out.Add(Line.Last());
	return Out;
}

FVector BrixTubeMouth(const FBrixTubeNet& Net, const FBrixTubeRules& R, const FString& Node)
{
	if (const FBrixTubeStop* S = Net.Stops.Find(Node)) return FVector(S->Pos.X, S->Pos.Y, R.DrawInHeightM);
	if (const FVector* J = Net.Junctions.Find(Node)) return *J;
	return FVector(NAN);
}

TArray<FString> BrixTubeProblems(const FBrixTubeNet& Net, const FBrixTubeRules& R)
{
	TArray<FString> Out;
	for (int32 i = 0; i < Net.Tubes.Num(); ++i)
	{
		const FBrixTube& T = Net.Tubes[i];
		bool bNodes = true;
		for (const FString& E : { T.A, T.B })
		{
			if (!Net.Stops.Contains(E) && !Net.Junctions.Contains(E)) { Out.Add(FString::Printf(TEXT("tube %d: %s is not a stop or a junction"), i, *E)); bNodes = false; }
		}
		if (!bNodes) continue;
		if (T.Line.Num() < 2) { Out.Add(FString::Printf(TEXT("tube %d: fewer than two points"), i)); continue; }
		if (FVector::Dist(T.Line[0], BrixTubeMouth(Net, R, T.A)) > 0.01 || FVector::Dist(T.Line.Last(), BrixTubeMouth(Net, R, T.B)) > 0.01)
			Out.Add(FString::Printf(TEXT("tube %d: does not start and end at its nodes' mouths"), i));
		double Low = TNumericLimits<double>::Max();
		for (const FVector& P : T.Line) Low = FMath::Min(Low, P.Z);
		if (Low < R.ClearanceM) Out.Add(FString::Printf(TEXT("tube %d: lower than %.1f m"), i, R.ClearanceM));
	}
	for (const FString& S : Net.StopOrder)
	{
		if (!Net.Tubes.ContainsByPredicate([&](const FBrixTube& T) { return T.A == S || T.B == S; })) Out.Add(TEXT("stop ") + S + TEXT(": no tube"));
	}
	return Out;
}

TArray<FString> BrixTubeRoute(const FBrixTubeNet& Net, const FString& A, const FString& B)
{
	const TMap<FString, TArray<FTubeEdge>> E = TubeEdges(Net, 0);
	TMap<FString, double> Best;
	TMap<FString, FString> From;
	TSet<FString> Done;
	Best.Add(A, 0);
	while (true)
	{
		FString N;
		double D = TNumericLimits<double>::Max();
		for (const auto& KV : Best) if (!Done.Contains(KV.Key) && KV.Value < D) { D = KV.Value; N = KV.Key; }
		if (N.IsEmpty()) return {};
		if (N == B) break;
		Done.Add(N);
		if (N != A && Net.Stops.Contains(N)) continue;      // a stop on the way is an end, not a way through
		if (const TArray<FTubeEdge>* Out = E.Find(N))
		{
			for (const FTubeEdge& X : *Out)
			{
				const double* Was = Best.Find(X.To);
				if (!Done.Contains(X.To) && (!Was || D + X.Length < *Was)) { Best.Add(X.To, D + X.Length); From.Add(X.To, N); }
			}
		}
	}
	TArray<FString> Path = { B };
	while (Path[0] != A) Path.Insert(From[Path[0]], 0);
	return Path;
}

FBrixRide BrixMakeRide(const FBrixTubeNet& Net, const FBrixTubeRules& R, const FString& A, const FString& B)
{
	FBrixRide W;
	W.R = R;
	if (!Net.Stops.Contains(A) || !Net.Stops.Contains(B) || A == B) return W;
	W.Nodes = BrixTubeRoute(Net, A, B);
	if (!W.IsValid()) return W;
	const TMap<FString, TArray<FTubeEdge>> E = TubeEdges(Net, R.BendRM);
	for (int32 i = 1; i < W.Nodes.Num(); ++i)
	{
		const FTubeEdge* Pick = nullptr;
		for (const FTubeEdge& X : E[W.Nodes[i - 1]]) if (X.To == W.Nodes[i] && (!Pick || X.Length < Pick->Length)) Pick = &X;
		for (int32 k = W.Run.Num() ? 1 : 0; k < Pick->Line.Num(); ++k) W.Run.Add(Pick->Line[k]);
	}
	if (W.Nodes.Num() > 2) W.Run = BrixTubeRounded(W.Run, R.JunctionBendRM);   // the turns where one tube meets the next
	W.Cum.Add(0);
	for (int32 i = 1; i < W.Run.Num(); ++i) W.Cum.Add(W.Cum.Last() + FVector::Dist(W.Run[i - 1], W.Run[i]));
	const FBrixTubeStop& SA = Net.Stops[A];
	const FBrixTubeStop& SB = Net.Stops[B];
	W.FloorA = FVector(SA.Pos, R.FloorM);                   // the booths' floors
	W.FloorB = FVector(SB.Pos, R.FloorM);
	W.FaceA = SA.Face;
	W.FaceB = SB.Face;
	W.OutB = FVector(SB.Pos + SB.Face * R.PopOutArcM, R.FloorM);
	// the run's speed: up to SpeedMps over EaseS, then down again; shorter rides never reach full speed
	const double L = W.Cum.Last(), V = R.SpeedMps, Ea = R.EaseS;
	W.Acc = V / Ea;
	if (W.Acc * Ea * Ea >= L) { W.Te = FMath::Sqrt(L / W.Acc); W.VTop = FMath::Sqrt(L * W.Acc); W.Tc = 0; }
	else { W.Te = Ea; W.VTop = V; W.Tc = (L - W.Acc * Ea * Ea) / V; }
	W.Way.Add(W.FloorA);
	W.Way.Append(W.Run);
	W.Way.Add(W.FloorB);
	W.Way.Add(W.OutB);
	W.WayCum.Add(0);
	for (int32 i = 1; i < W.Way.Num(); ++i) W.WayCum.Add(W.WayCum.Last() + FVector::Dist(W.Way[i - 1], W.Way[i]));
	W.UpM = FVector::Dist(W.FloorA, W.Run[0]);
	W.DownM = FVector::Dist(W.Run.Last(), W.FloorB) + FVector::Dist(W.FloorB, W.OutB);
	auto Across = [&W](bool bFirst)                         // across the first (or last) level stretch: along the street
	{
		const int32 N = W.Run.Num();
		for (int32 k = 1; k < N; ++k)
		{
			const FVector P = bFirst ? W.Run[k - 1] : W.Run[N - k - 1], Q = bFirst ? W.Run[k] : W.Run[N - k];
			const FVector2D F(Q.X - P.X, Q.Y - P.Y);
			if (F.Size() > .05) return FVector2D(-F.Y, F.X).GetSafeNormal();
		}
		return FVector2D(1, 0);
	};
	W.SideA = Across(true);
	W.SideB = Across(false);
	W.TRun = 2 * W.Te + W.Tc;
	W.T0 = R.DrawInS;
	W.T1 = R.DrawInS + W.TRun;
	W.Duration = W.T1 + R.PopOutS;
	return W;
}

EBrixRidePhase FBrixRide::Phase(double S) const
{
	return S < T0 ? EBrixRidePhase::DrawIn : S < T1 ? EBrixRidePhase::Run : S < Duration ? EBrixRidePhase::PopOut : EBrixRidePhase::Out;
}

double FBrixRide::RunDist(double U) const
{
	U = FMath::Clamp(U, 0.0, TRun);
	if (U < Te) return 0.5 * Acc * U * U;
	if (U < Te + Tc) return 0.5 * Acc * Te * Te + VTop * (U - Te);
	const double W = TRun - U;
	return Cum.Last() - 0.5 * Acc * W * W;
}

FVector FBrixRide::OnRun(double D) const
{
	int32 i = 0;
	while (i < Cum.Num() - 2 && Cum[i + 1] < D) ++i;
	const double Seg = Cum[i + 1] - Cum[i];
	return FMath::Lerp(Run[i], Run[i + 1], Seg > 1e-9 ? (D - Cum[i]) / Seg : 0.0);
}

FVector FBrixRide::At(double S) const
{
	if (S <= 0) return FloorA;
	if (S < T0) { const double F = S / T0; return FMath::Lerp(FloorA, Run[0], F * F); }   // slow off the floor, fast into the tube
	if (S < T1) return OnRun(RunDist(S - T0));
	if (S < Duration)                                       // shot out and down, landing on their feet past the door
	{
		const double F = (S - T1) / R.PopOutS;
		const double G = 1 - (1 - F) * (1 - F);
		const FVector Down = FMath::Lerp(Run.Last(), FloorB, FMath::Min(1.0, G * 1.25));
		return FVector(Down.X + (OutB.X - FloorB.X) * TubeSmooth(F), Down.Y + (OutB.Y - FloorB.Y) * TubeSmooth(F), Down.Z);
	}
	return OutB;
}

FVector FBrixRide::Heading(double S) const
{
	if (Phase(S) != EBrixRidePhase::Run) return FVector::UpVector;
	const double D = RunDist(S - T0);
	const double Wn = FMath::Min3(R.TurnWindowM, D, Cum.Last() - D);   // narrower at the ends, which go straight up and down
	const FVector A = OnRun(D - Wn), B = OnRun(D + Wn);
	const FVector H(B.X - A.X, B.Y - A.Y, FMath::Abs(B.Z - A.Z));   // head first where level, upright going up or down
	return H.IsNearlyZero() ? FVector::UpVector : H.GetUnsafeNormal();
}

double FBrixRide::Lean(double S) const
{
	if (Phase(S) != EBrixRidePhase::Run) return 0;
	const FVector H = Heading(S);
	return FMath::RadiansToDegrees(FMath::Atan2(FVector2D(H.X, H.Y).Size(), H.Z));
}

double FBrixRide::Along(double S) const
{
	if (S <= 0) return 0;
	if (S < T0) return FVector::Dist(FloorA, At(S));
	if (S < T1) return UpM + RunDist(S - T0);
	return UpM + Cum.Last() + DownM * FMath::Min(1.0, (S - T1) / R.PopOutS);
}

FVector FBrixRide::OnWay(double D) const
{
	D = FMath::Clamp(D, 0.0, WayCum.Last());
	int32 i = 0;
	while (i < WayCum.Num() - 2 && WayCum[i + 1] < D) ++i;
	const double Seg = WayCum[i + 1] - WayCum[i];
	return FMath::Lerp(Way[i], Way[i + 1], Seg > 1e-9 ? (D - WayCum[i]) / Seg : 0.0);
}

FVector FBrixRide::Camera(double S) const
{
	// along a level tube: behind the rider along their way and over it; where the way goes up or down: beside the booth,
	// along the street, at the rider's height (tubes.py camera())
	const double D = Along(S);
	const FVector T = OnWay(FMath::Max(0.0, D - R.BehindM));
	const FVector On = OnWay(FMath::Min(D, WayCum[WayCum.Num() - 2]));   // short of the step out of the booth
	const double Flat = FVector2D(On.X - T.X, On.Y - T.Y).Size(), Rise = FMath::Abs(On.Z - T.Z);
	const double Wl = Flat + Rise > 1e-9 ? Flat / FMath::Sqrt(Flat * Flat + Rise * Rise) : 0;   // 1 along a level tube, 0 up or down
	const FVector2D Side = D < WayCum.Last() / 2 ? SideA : SideB;
	const double K = (1 - Wl) * R.BehindM;
	const FVector Rider = At(S);
	return FVector(T.X + Side.X * K, T.Y + Side.Y * K, T.Z * Wl + Rider.Z * (1 - Wl) + R.AboveM);
}

FString BrixTubeStopAt(const FBrixTubeNet& Net, const FBrixTubeRules& R, const FVector2D& Pos)
{
	for (const FString& Id : Net.StopOrder) if (FVector2D::Distance(Net.Stops[Id].Pos, Pos) <= R.BoothR) return Id;
	return FString();
}

FString BrixAskRide(const FBrixTubeNet& Net, const FBrixTubeRules& R, const FVector2D& StandingAt, bool bRiding, const FString& To, FBrixRide& OutRide)
{
	OutRide = FBrixRide();
	if (bRiding) return TEXT("riding");
	const FString From = BrixTubeStopAt(Net, R, StandingAt);
	if (From.IsEmpty()) return TEXT("not_in_a_stop");
	if (!Net.Stops.Contains(To)) return TEXT("unknown_stop");
	if (To == From) return TEXT("same_stop");
	OutRide = BrixMakeRide(Net, R, From, To);
	return OutRide.IsValid() ? FString() : TEXT("no_way");
}
