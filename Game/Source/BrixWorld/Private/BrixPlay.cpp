#include "BrixPlay.h"

#include "BrixBody.h"
#include "BrixCity.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <cmath>

using BrixSlice::Js;

namespace
{
	double H(std::initializer_list<FString> P) { return BrixSlice::Hash(P); }
	FString Nn(double V) { return Js(V); }
	double YawOf(double dx, double dz) { return FMath::Atan2(dx, -dz); }   // play.js yawOf()
	double Mix(double a, double b, double t) { return a + (b - a) * t; }
	const double SwingL = 1.84, SeatTop = .025, PlankTop = .035, FrameH = 1.0, Outside = .16, Ladder = -2.13, Deck = 1.58, SlideTopS = .95, SlideOff = 2.9;
	const int32 Faces[4][2] = { { -1, 0 }, { 1, 0 }, { 0, 1 }, { 0, -1 } };
	const FVector3d Iron(.17, .18, .21);
	FString Key(double x, double z) { return Nn(BrixSlice::JsRound(x * 5)) + TEXT(",") + Nn(BrixSlice::JsRound(z * 5)); }
	void Reach(FBrixPlayPose& O, double ph, double up)   // hands up on the rungs, one then the other
	{
		const double s = FMath::Sin(ph * 6.283) * 18;
		O.Arms[0] = FVector4d(1, up + s, 10, 30);
		O.Arms[1] = FVector4d(1, up - s, 10, 30);
	}
}

const TCHAR* FBrixPlay::UseName(int32 U)
{
	static const TCHAR* Names[NumUses] = { TEXT("swing"), TEXT("slide"), TEXT("seesaw"), TEXT("sandpit"), TEXT("climb") };
	return U >= 0 && U < NumUses ? Names[U] : TEXT("");
}

void FBrixPlay::AddPiece(const FBrixProp& P, double G)
{
	const int32 U = P.Kind == TEXT("swing") ? Swing : P.Kind == TEXT("slide") ? Slide : P.Kind == TEXT("seesaw") ? Seesaw : P.Kind == TEXT("sandpit") ? Sandpit : P.Kind == TEXT("climber") ? Climb : -1;
	if (P.Kind == TEXT("noticeboard") || P.Kind == TEXT("kiosk")) Boards.Add(P.Pos);
	if (U < 0) return;
	FPiece& Pc = Pieces.AddDefaulted_GetRef();
	Pc.Id = P.Id; Pc.Use = U; Pc.X = P.Pos.X; Pc.Z = P.Pos.Y; Pc.G = G;
	double Seats = 0;
	if (P.Json.IsValid()) P.Json->TryGetNumberField(TEXT("seats"), Seats);
	Pc.N = (int32)Seats;
}

void FBrixPlay::Init(const FBrixCity& City)
{
	Spots.Reset();
	Grid.Reset();
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) for (int32 u = 0; u < NumUses; ++u)
	{
		const TArray<FVector2D>* At = L.Objects.Find(UseName(u));
		if (!At) continue;
		for (int32 k = 0; k < At->Num(); ++k)
		{
			const FVector2D s = (*At)[k];
			int32 Best = -1;
			double Bd = 1e9;
			for (int32 p = 0; p < Pieces.Num(); ++p)
			{
				if (Pieces[p].Use != u) continue;
				const double d = FMath::Sqrt(FMath::Square(Pieces[p].X - s.X) + FMath::Square(Pieces[p].Z - s.Y));
				if (d < Bd) { Bd = d; Best = p; }
			}
			if (Best < 0 || Bd >= 8) continue;
			const int32 n = Spots.Add({ Best, k, u, s.X, s.Y });
			Grid.FindOrAdd(FIntPoint((int32)FMath::FloorToDouble(s.X), (int32)FMath::FloorToDouble(s.Y))).Add(n);
		}
	}
}

const FBrixPlay::FSpot* FBrixPlay::SpotAt(double X, double Z) const
{
	// the day plan keeps places to a tenth of a metre: the spot within 0.3 m
	const FSpot* Best = nullptr;
	double Bd = .3;
	for (int32 a = -1; a <= 1; ++a) for (int32 b = -1; b <= 1; ++b)
		if (const TArray<int32>* L = Grid.Find(FIntPoint((int32)FMath::FloorToDouble(X + a), (int32)FMath::FloorToDouble(Z + b))))
			for (const int32 n : *L)
			{
				const double d = FMath::Sqrt(FMath::Square(Spots[n].X - X) + FMath::Square(Spots[n].Z - Z));
				if (d < Bd) { Bd = d; Best = &Spots[n]; }
			}
	return Best;
}

double FBrixPlay::SwingAngle(const FPiece& P, int32 K, double T) const
{
	const double A = Motion.SwingAngleDeg * UE_DOUBLE_PI / 180 * (.75 + .25 * H({ P.Id, Nn(K) }));
	return A * FMath::Sin(T * 6.283 / (Motion.SwingPeriodS * (.92 + .16 * H({ Nn(K), P.Id }))) + H({ P.Id, Nn(K), TEXT("ph") }) * 6.283);
}

double FBrixPlay::SeesawAngle(const FPiece& P, double T) const
{
	return Motion.SeesawAngleDeg * UE_DOUBLE_PI / 180 * FMath::Sin(T * 6.283 / Motion.SeesawPeriodS + H({ P.Id }) * 6.283);
}

TArray<FVector2D> FBrixPlay::BackTo(const FPiece& P) const   // round the side, to the ladder's foot facing it
{
	return { FVector2D(P.X + SlideOff, P.Z), FVector2D(P.X + SlideOff, P.Z + .8), FVector2D(P.X + .5, P.Z + 1.0), FVector2D(P.X - 2.6, P.Z + .8), FVector2D(P.X - 2.8, P.Z + .25), FVector2D(P.X + Ladder, P.Z) };
}

bool FBrixPlay::Apply(int32 i, int32 Use, double X, double Z, double T, double StepLen, FBrixPlayPose& O)
{
	if (Use < 0 || Use >= NumUses) return false;
	const FSpot* sp = SpotAt(X, Z);
	if (!sp) return false;
	const FPiece& pc = Pieces[sp->Piece];
	const double g = pc.G;
	Busy.Add(pc.Id + TEXT(":") + Nn(sp->K)); Busy.Add(pc.Id); Busy.Add(Key(sp->X, sp->Z));   // the seat moves
	O = FBrixPlayPose();
	O.X = X; O.Z = Z;
	if (pc.Use == Swing)   // on the seat: it hangs under the beam and turns with the chains
	{
		const double th = SwingAngle(pc, sp->K, T);
		O.X = pc.X + (sp->K - (pc.N - 1) / 2.0) * 1.1; O.Z = pc.Z + FMath::Sin(th) * SwingL; O.Y = g + 2.36 - FMath::Cos(th) * SwingL + SeatTop; O.Yaw = YawOf(0, 1); O.Pose = 2;
		return true;
	}
	if (pc.Use == Seesaw)   // on the plank 1.6 m out, behind the handle, facing the middle; a leg each side
	{
		const double s = sp->X < pc.X ? -1 : 1, phi = SeesawAngle(pc, T);
		O.X = pc.X + s * 1.6 * FMath::Cos(phi); O.Z = pc.Z; O.Y = g + .52 + s * 1.6 * FMath::Sin(phi) + PlankTop; O.Yaw = YawOf(-s, 0); O.Pose = 2; O.bStraddle = true;
		return true;
	}
	if (pc.Use == Slide)   // up the ladder, down the chute, round the back to the ladder again
	{
		const double Tc = Motion.SlideCycleS, e0 = std::fmod((T / Tc) + sp->K / 4.0 + H({ Nn(i), TEXT("sl") }) * .1, 1.0) * Tc, c = .26 * Tc, dn = .14 * Tc;
		const double x = pc.X, z = pc.Z, f = YawOf(1, 0);
		double e = e0;
		O.Z = z; O.Yaw = f;
		if (e < c) { const double q = e / c, ph = std::fmod(q * 5, 1.0); O.X = x + Ladder; O.Y = g + q * 1.5; O.Pose = 1; O.Phase = ph; O.bClimb = true; Reach(O, ph, 150); return true; }
		e -= c;
		if (e < .45) { const double q = e / .45; O.X = Mix(x + Ladder, x - 1.55, q); O.Y = g + Mix(1.5, Deck, FMath::Min(1.0, q * 2)); O.Pose = 1; O.Phase = q * .5; return true; }
		e -= .45;
		if (e < SlideTopS - .45) { O.X = x - 1.3; O.Y = g + Deck; O.Pose = 2; return true; }
		e -= SlideTopS - .45;
		if (e < dn)
		{
			const double q = e / dn, px = Mix(x - 1.3, x + 2.1, q * q), a = x - 1.15, b = x + 2.1;
			O.X = px; O.Y = px <= a ? g + Deck : g + Mix(1.55, .32, FMath::Min(1.0, (px - a) / (b - a))) + .05; O.Pose = 2;
			return true;
		}
		const double e2 = e0 - (c + SlideTopS + dn);
		if (e2 < .5) { O.X = x + SlideOff; O.Y = g; O.Pose = 0; return true; }
		// walking back round
		const TArray<FVector2D> Pts = BackTo(pc);
		TArray<double> Len = { 0 };
		for (int32 k = 1; k < Pts.Num(); ++k) Len.Add(Len[k - 1] + FVector2D::Distance(Pts[k], Pts[k - 1]));
		const double q = FMath::Min(1.0, (e2 - .5) / FMath::Max(.5, Tc - (c + SlideTopS + dn + .5))), d = q * Len.Last();
		int32 k = 1;
		while (k < Pts.Num() - 1 && Len[k] < d) ++k;
		const FVector2D a = Pts[k - 1], b = Pts[k];
		const double fr = FMath::Clamp((d - Len[k - 1]) / ((Len[k] - Len[k - 1]) != 0 ? Len[k] - Len[k - 1] : 1), 0.0, 1.0);
		O.X = Mix(a.X, b.X, fr); O.Z = Mix(a.Y, b.Y, fr); O.Y = g; O.Yaw = YawOf(b.X - a.X, b.Y - a.Y); O.Pose = 1; O.bWalking = true; O.Phase = std::fmod(d / StepLen, 1.0);
		return true;
	}
	if (pc.Use == Sandpit) { O.Y = g + .3; O.Yaw = YawOf(pc.X - X, pc.Z - Z); O.Pose = 2; return true; }
	if (pc.Use == Climb)   // on the outside of the frame, each child on their own side: up, a look over the top, down, a rest
	{
		const int32 face = sp->K % 4;
		const double tx = -Faces[face][1], tz = Faces[face][0], nx = Faces[face][0], nz = Faces[face][1];
		const double lat = FMath::Clamp((sp->X - pc.X) * tx + (sp->Z - pc.Z) * tz, -.6, .6), u = std::fmod((T / Motion.ClimbCycleS) + H({ Nn(i), TEXT("cl") }), 1.0);
		const double dd = FrameH + Outside;
		O.X = pc.X + nx * dd + tx * lat; O.Z = pc.Z + nz * dd + tz * lat; O.Yaw = YawOf(-nx, -nz);
		if (u < .3) { const double q = u / .3, ph = std::fmod(q * 4, 1.0); O.Y = g + q * 1.1; O.Pose = 1; O.Phase = ph; O.bClimb = true; Reach(O, ph, 150); }
		else if (u < .6) { O.Y = g + 1.1; O.Pose = 0; O.Arms[0] = FVector4d(1, 45, 12, 15); O.Arms[1] = FVector4d(1, 45, 12, 15); }   // feet on the middle bar, hands on the top one
		else if (u < .85) { const double q = (u - .6) / .25, ph = std::fmod((1 - q) * 4, 1.0); O.Y = g + (1 - q) * 1.1; O.Pose = 1; O.Phase = ph; O.bClimb = true; Reach(O, ph, 150); }
		else { O.Y = g; O.Pose = 0; }
		return true;
	}
	return false;
}

void FBrixPlay::KeepOut(double& X, double& Z) const
{
	for (const FPiece& pc : Pieces)
	{
		if (pc.Use != Climb && pc.Use != Slide) continue;
		const double x0 = pc.Use == Climb ? pc.X - 1.05 : pc.X - 2.0, z0 = pc.Use == Climb ? pc.Z - 1.05 : pc.Z - .4, x1 = pc.Use == Climb ? pc.X + 1.05 : pc.X + 2.6, z1 = pc.Use == Climb ? pc.Z + 1.05 : pc.Z + .4;
		if (X <= x0 || X >= x1 || Z <= z0 || Z >= z1) continue;
		const double d[4] = { X - x0, x1 - X, Z - z0, z1 - Z };
		int32 k = 0;
		for (int32 j = 1; j < 4; ++j) if (d[j] < d[k]) k = j;
		if (k == 0) X = x0; else if (k == 1) X = x1; else if (k == 2) Z = z0; else Z = z1;
	}
}

void FBrixPlay::Draw(double EyeX, double EyeZ, double T, TArray<FBrixShape>& Out)
{
	static const FVector3d Paint[4] = { BrixSlice::Teal, BrixSlice::Ochre, BrixSlice::Terracotta, BrixSlice::Blue };
	for (const FPiece& pc : Pieces)
	{
		if (FMath::Abs(pc.X - EyeX) > 70 || FMath::Abs(pc.Z - EyeZ) > 70) continue;
		if (pc.Use == Swing)
			for (int32 k = 0; k < pc.N; ++k)
			{
				const bool on = Busy.Contains(pc.Id + TEXT(":") + Nn(k));
				const double th = on ? SwingAngle(pc, k, T) : .04 * FMath::Sin(T * .9 + k), a = pc.X + (k - (pc.N - 1) / 2.0) * 1.1;
				const FBrixM4 m = FBrixM4::T(a, pc.G + 2.36, pc.Z) * FBrixM4::Rx(-th);   // swings the seat towards +z as th grows
				for (const double s : { -.2, .2 }) BrixOBox(Out, m, s, -.9, 0, .02, 1.8, .02, Iron);
				BrixOBox(Out, m, 0, -1.84, 0, .48, .05, .2, FVector3d(.18, .14, .1));
			}
		else if (pc.Use == Seesaw)
		{
			const double phi = Busy.Contains(pc.Id) ? SeesawAngle(pc, T) : -.24;   // anyone on either end rocks it
			const FBrixM4 m = FBrixM4::T(pc.X, pc.G + .52, pc.Z) * FBrixM4::Rz(phi);
			BrixOBox(Out, m, 0, 0, 0, 3.6, .07, .3, Paint[FMath::Min(3, (int32)FMath::FloorToDouble(H({ pc.Id, TEXT("ss") }) * 4))]);
			for (const double s : { -1.45, 1.45 }) { BrixOBox(Out, m, s, .2, 0, .05, .35, .05, Iron); BrixOBox(Out, m, s, .38, 0, .05, .05, .4, Iron); }
		}
	}
	Busy.Reset();
}

bool LoadBrixPlayMotion(const FString& File, FBrixPlayMotion& Out, FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *File)) { OutError = TEXT("Cannot read ") + File; return false; }
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) { OutError = File + TEXT(" is not valid JSON"); return false; }
	const TSharedPtr<FJsonObject>* D = nullptr, * M = nullptr;
	if (!Root->TryGetObjectField(TEXT("dressing"), D) || !(*D)->TryGetObjectField(TEXT("motion"), M)) { OutError = File + TEXT(" has no dressing motion"); return false; }
	(*M)->TryGetNumberField(TEXT("swing_period_s"), Out.SwingPeriodS); (*M)->TryGetNumberField(TEXT("swing_angle_deg"), Out.SwingAngleDeg);
	(*M)->TryGetNumberField(TEXT("seesaw_period_s"), Out.SeesawPeriodS); (*M)->TryGetNumberField(TEXT("seesaw_angle_deg"), Out.SeesawAngleDeg);
	(*M)->TryGetNumberField(TEXT("slide_cycle_s"), Out.SlideCycleS); (*M)->TryGetNumberField(TEXT("climb_cycle_s"), Out.ClimbCycleS);
	return true;
}

void BrixAddClimbSignature(FBrixPeopleRules& Rules)
{
	FBrixSignature S = Rules.Sig(TEXT("Careful"));
	S.Name = TEXT("Climb");
	S.Stride = .14; S.Duty = .5; S.Fore = .5; S.Bounce = 0; S.Sway = 0; S.HipYaw = 0; S.HipRoll = 0; S.Counter = 0;
	S.Lift = .24; S.Lean = 3; S.HeadPitch = -12; S.HeadBob = 0; S.ToeOut = 3; S.Width = .8;
	Rules.Signatures.Add(TEXT("Climb"), S);
}

bool FBrixPlay::FaceBoard(double X, double Z, double& Yaw) const
{
	const FVector2D* Best = nullptr;
	double Bd = 6;
	for (const FVector2D& Q : Boards) { const double d = FMath::Sqrt(FMath::Square(Q.X - X) + FMath::Square(Q.Y - Z)); if (d < Bd) { Bd = d; Best = &Q; } }
	if (!Best) return false;
	Yaw = YawOf(Best->X - X, Best->Y - Z);
	return true;
}
