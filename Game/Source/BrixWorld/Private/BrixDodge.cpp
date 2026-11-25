#include "BrixDodge.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	double sm(double a, double b, double v) { const double t = FMath::Clamp((v - a) / (b - a), 0.0, 1.0); return t * t * (3 - 2 * t); }
	double yawOf(double dx, double dz) { return FMath::Atan2(dx, -dz); }
	double rnd(double i, double n) { const double s = FMath::Sin(i * 12.9898 + n * 78.233) * 43758.5453; return s - FMath::FloorToDouble(s); }
	double fmod1(double x) { return x - FMath::FloorToDouble(x); }
}

bool LoadBrixDodgeRules(const FString& Path, FBrixDodgeRules& Out, FString& OutError)
{
	FString Text;
	TSharedPtr<FJsonObject> O;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = TEXT("Cannot read ") + Path; return false; }
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) || !O.IsValid()) { OutError = Path + TEXT(" is not valid JSON"); return false; }
	auto N = [&](const TCHAR* K, double& V) { O->TryGetNumberField(K, V); };
	N(TEXT("min_speed"), Out.MinSpeed); N(TEXT("see_s"), Out.SeeS); N(TEXT("see_m"), Out.SeeM); N(TEXT("lane_pad_m"), Out.LanePadM);
	N(TEXT("car_half_w_m"), Out.CarHalfWM); N(TEXT("car_front_m"), Out.CarFrontM); N(TEXT("clear_m"), Out.ClearM); N(TEXT("startle_s"), Out.StartleS);
	N(TEXT("scramble_s"), Out.ScrambleS); N(TEXT("hop_m"), Out.HopM); N(TEXT("scold_s"), Out.ScoldS); N(TEXT("return_s"), Out.ReturnS);
	N(TEXT("cooldown_s"), Out.CooldownS); N(TEXT("fist_share"), Out.FistShare); N(TEXT("tag_m"), Out.TagM);
	O->TryGetStringArrayField(TEXT("shouts"), Out.Shouts);
	if (!Out.Shouts.Num()) { OutError = Path + TEXT(" has no shouts"); return false; }
	return true;
}

void FBrixDodge::Frame(double T, bool bDriving, const FVector2D& CarPos, const FVector2D& CarHeading, double CarY, double Speed)
{
	Now = T;
	Car.Reset();
	if (!bOn || !bDriving || FMath::Abs(Speed) < R.MinSpeed) return;
	const double s = FMath::Sign(Speed);   // reversing counts too
	Car = FCar{ CarPos.X, CarPos.Y, CarY, CarHeading.X * s, CarHeading.Y * s, FMath::Abs(Speed) };
}

bool FBrixDodge::Threat(double x, double y, double z, double& OutA, double& OutW) const
{
	if (!Car || FMath::Abs(y - Car->Y) > 1.5) return false;
	const double dx = x - Car->X, dz = z - Car->Z, a = dx * Car->Hx + dz * Car->Hz, w = -dx * Car->Hz + dz * Car->Hx;
	if (a < -.5 || a > R.SeeM || a > Car->V * R.SeeS + R.CarFrontM) return false;
	OutA = a; OutW = w;
	return FMath::Abs(w) < R.CarHalfWM + R.LanePadM;
}

FVector2D FBrixDodge::Off(const FState& S) const
{
	const double e = Now - S.T0, a = R.StartleS, b = a + R.ScrambleS, c = b + R.ScoldS;
	if (e < a) return S.From;
	if (e < b) { const double u = sm(0, 1, (e - a) / R.ScrambleS); return S.From + (S.To - S.From) * u; }
	if (e < c) return S.To;
	const double u = 1 - sm(0, 1, (e - c) / R.ReturnS);
	return S.To * u;
}

FBrixDodge::FState& FBrixDodge::Start(int32 i, const FVector2D& Pos, const FState* S, double w, TFunctionRef<bool(const FVector2D&)> Clear)
{
	// seen it: pick the side with room and scramble that way
	const FVector2D o = S ? Off(*S) : FVector2D::ZeroVector, n(-Car->Hz, Car->Hx);
	const double want = R.CarHalfWM + R.LanePadM + R.ClearM;
	const double first = FMath::Abs(w) > .15 ? FMath::Sign(w) : rnd(i, 1) < .5 ? -1 : 1;
	TOptional<FVector2D> To;
	for (const double side : { first, -first })
	{
		const FVector2D t = o + n * (side * want - w);
		if (Clear(Pos + t)) { To = t; break; }
	}
	if (!To) To = o + n * (first * want - w);   // nowhere good: go anyway, as people do
	const int32 N = S ? S->N + 1 : 0;
	FState St{ Now, o, *To, N, rnd(i, N + 2) < R.FistShare, R.Shouts[FMath::FloorToInt(rnd(i, N + 3) * R.Shouts.Num())], Car->X, Car->Z };
	return States.Add(i, St);
}

bool FBrixDodge::Apply(int32 i, const FVector2D& Pos, double Y, double Yaw, int32 Pose, bool bInside, TFunctionRef<bool(const FVector2D&)> Clear, FBrixDodged& q)
{
	if (!bOn) return false;
	FState* s = States.Find(i);
	if (bInside || Pose == 2 || Pose == 3) { if (s) States.Remove(i); return false; }
	const double total = R.StartleS + R.ScrambleS + R.ScoldS + R.ReturnS;
	if (s && Now - s->T0 > total) { States.Remove(i); s = nullptr; }
	if (Car && (!s || Now - s->T0 > R.StartleS + R.ScrambleS + R.CooldownS))   // not already mid-scramble
	{
		const FVector2D o = s ? Off(*s) : FVector2D::ZeroVector;
		double a, w;
		if (Threat(Pos.X + o.X, Y, Pos.Y + o.Y, a, w)) s = &Start(i, Pos, s, w, Clear);
	}
	if (!s) return false;
	if (Car) { s->Cx = Car->X; s->Cz = Car->Z; }   // keep an eye on the car while it is about
	const double e = Now - s->T0, a = R.StartleS, b = a + R.ScrambleS, c = b + R.ScoldS;
	const FVector2D o = Off(*s);
	q.Pos = Pos + o;
	q.Hop = 0;
	q.Sig.Reset();
	q.Arms[0].Reset(); q.Arms[1].Reset();
	q.Chest = FVector2D::ZeroVector;
	q.Shout.Reset();
	const double atCar = yawOf(s->Cx - q.Pos.X, s->Cz - q.Pos.Y);
	if (e < a)   // startle: a flinch towards the car, hands up, leaning back
	{
		const double k = sm(0, a, e);
		q.Pose = 0; q.Yaw = atCar;   // dodge.js keeps the day's walking flag
		q.Arms[0] = q.Arms[1] = FVector4d(k, 55, 25, 95);
		q.Chest = FVector2D(-9 * k, 0);
	}
	else if (e < b)   // the scramble: quick running steps sideways and a small hop
	{
		const double u = (e - a) / R.ScrambleS;
		const FVector2D d = s->To - s->From;
		q.Hop = R.HopM * FMath::Sin(UE_DOUBLE_PI * FMath::Min(1.0, u * 1.15));
		q.Pose = 1; q.bWalking = true; q.Phase = fmod1(e * 2.6 + i * .37); q.Yaw = yawOf(d.X, d.Y); q.Sig = TEXT("Run");
		q.Arms[0] = FVector4d(.8, 40, 30, 100);
		q.Arms[1] = FVector4d(.8, 30, 34, 105);
		q.Chest = FVector2D(10, 0);
	}
	else if (e < c)   // safe: turn on the driver, fist or both arms up, and shout
	{
		const double f = (e - b) / R.ScoldS, k = sm(0, .12, f) * (1 - sm(.82, 1, f)), shake = FMath::Sin(Now * 15 + i) * 9 * k;
		if (s->bFist) { q.Arms[0] = FVector4d(k * .5, 10, 14, 70); q.Arms[1] = FVector4d(k, 168 + shake, 8, 62 + shake); }
		else { q.Arms[0] = FVector4d(k, 140 + shake, 38, 25); q.Arms[1] = FVector4d(k, 140 - shake, 38, 25); }
		q.Pose = 0; q.Yaw = atCar;   // dodge.js keeps the day's walking flag
		q.Chest = FVector2D(s->bFist ? 7 * k : -5 * k, 0);
		if (f < .85) q.Shout = s->Shout;
	}
	else if (o.Size() > .05)   // back to where their day has them
	{
		q.Pose = 1; q.bWalking = true; q.Phase = fmod1(e * 1.7 + i * .37); q.Yaw = yawOf(-s->To.X, -s->To.Y);
	}
	return true;
}
