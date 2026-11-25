#include "BrixCrash.h"

#include "Dom/JsonObject.h"
#include "Math/RandomStream.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	double sm(double a, double b, double v) { const double t = FMath::Clamp((v - a) / (b - a), 0.0, 1.0); return t * t * (3 - 2 * t); }
	double rnd(double a, double b) { const double s = FMath::Sin(a * 12.9898 + b * 78.233) * 43758.5453; return s - FMath::FloorToDouble(s); }
	double HM(const FString& S) { return FCString::Atod(*S.Left(2)) * 60 + FCString::Atod(*S.Mid(3, 2)); }
}

FString FBrixCrashRules::KindOf(const FString& Kind) const
{
	if (Kinds.Contains(Kind)) return Kind;
	if (const FString* A = Aliases.Find(Kind)) return *A;
	return FString();
}

bool LoadBrixCrashRules(const FString& Path, FBrixCrashRules& Out, FString& OutError)
{
	FString Text;
	TSharedPtr<FJsonObject> O;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = TEXT("Cannot read ") + Path; return false; }
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) || !O.IsValid()) { OutError = Path + TEXT(" is not valid JSON"); return false; }
	Out = FBrixCrashRules();
	O->TryGetNumberField(TEXT("break_kmh"), Out.BreakKmh);
	for (const auto& KV : O->GetObjectField(TEXT("kinds"))->Values)
	{
		const TSharedPtr<FJsonObject> K = KV.Value->AsObject();
		FBrixCrashKind& C = Out.Kinds.Add(FString(KV.Key));
		C.How = K->GetStringField(TEXT("how"));
		K->TryGetNumberField(TEXT("slow"), C.Slow);
		K->TryGetNumberField(TEXT("work_min"), C.WorkMin);
	}
	const TSharedPtr<FJsonObject>* A = nullptr;
	if (O->TryGetObjectField(TEXT("aliases"), A)) for (const auto& KV : (*A)->Values) Out.Aliases.Add(FString(KV.Key), KV.Value->AsString());
	const TSharedPtr<FJsonObject> D = O->GetObjectField(TEXT("debris"));
	Out.Pieces = (int32)D->GetNumberField(TEXT("pieces")); Out.RestPieces = (int32)D->GetNumberField(TEXT("rest_pieces"));
	Out.LifeS = D->GetNumberField(TEXT("life_s")); Out.Gravity = D->GetNumberField(TEXT("gravity")); Out.Bounce = D->GetNumberField(TEXT("bounce"));
	O->TryGetNumberField(TEXT("topple_s"), Out.ToppleS); O->TryGetNumberField(TEXT("corner_m"), Out.CornerM);
	O->TryGetNumberField(TEXT("react_m"), Out.ReactM); O->TryGetNumberField(TEXT("react_s"), Out.ReactS);
	O->TryGetStringArrayField(TEXT("shouts"), Out.Shouts);
	const TSharedPtr<FJsonObject> R = O->GetObjectField(TEXT("repair"));
	Out.CrewsPerDepot = (int32)R->GetNumberField(TEXT("crews_per_depot"));
	Out.Start = HM(R->GetStringField(TEXT("start"))); Out.End = HM(R->GetStringField(TEXT("end")));
	Out.ReportMin = R->GetNumberField(TEXT("report_min")); Out.WalkMps = R->GetNumberField(TEXT("walk_mps")); Out.Detour = R->GetNumberField(TEXT("detour"));
	if (!Out.Kinds.Num() || !Out.Shouts.Num()) { OutError = Path + TEXT(" has no kinds or no shouts"); return false; }
	return true;
}

TMap<FString, FBrixRepair> BrixRepairs(const TArray<FBrixDamage>& Damage, TArray<TPair<FString, FVector2D>> Depots, const FBrixCrashRules& R, double Pace)
{
	TMap<FString, FBrixRepair> Plan;
	const double speed = R.WalkMps * Pace;   // metres a game minute
	auto into = [&](double t)   // the first moment at or after t within working hours
	{
		const double day = FMath::FloorToDouble(t / 1440), tod = t - day * 1440;
		return tod < R.Start ? day * 1440 + R.Start : tod >= R.End ? (day + 1) * 1440 + R.Start : t;
	};
	struct FCrew { FString Id; double Free; FVector2D Pos; };
	TArray<FCrew> Crews;
	Depots.Sort([](const TPair<FString, FVector2D>& A, const TPair<FString, FVector2D>& B) { return FCString::Strcmp(*A.Key, *B.Key) < 0; });
	for (const TPair<FString, FVector2D>& D : Depots) for (int32 n = 0; n < R.CrewsPerDepot; ++n) Crews.Add({ D.Key + TEXT("/") + FString::FromInt(n), -1e18, D.Value });
	if (!Crews.Num()) return Plan;
	TArray<const FBrixDamage*> Order;
	for (const FBrixDamage& D : Damage) Order.Add(&D);
	Order.Sort([](const FBrixDamage& A, const FBrixDamage& B) { return A.At != B.At ? A.At < B.At : FCString::Strcmp(*A.Id, *B.Id) < 0; });
	for (const FBrixDamage* D : Order)
	{
		const FBrixCrashKind* K = R.Kinds.Find(D->Kind);
		const double work = K ? K->WorkMin : 30;
		FCrew* Best = nullptr;
		double bDone = 0, bLeave = 0, bWalk = 0;
		for (FCrew& C : Crews)
		{
			const double walk = FVector2D::Distance(C.Pos, D->Pos) * R.Detour / speed;
			double leave = into(FMath::Max(C.Free, D->At + R.ReportMin));
			if (FMath::Fmod(leave, 1440.0) + walk + work > R.End) leave = (FMath::FloorToDouble(leave / 1440) + 1) * 1440 + R.Start;   // not done by the end of the day: tomorrow morning
			const double done = leave + walk + work;
			if (!Best || done < bDone - 1e-9) { Best = &C; bDone = done; bLeave = leave; bWalk = walk; }
		}
		Plan.Add(D->Id, { Best->Id, bLeave, bLeave + bWalk, bDone });
		Best->Free = bDone;
		Best->Pos = D->Pos;
	}
	return Plan;
}

FString BrixDamageToJson(const TArray<FBrixDamage>& Damage)
{
	TArray<TSharedPtr<FJsonValue>> A;
	for (const FBrixDamage& D : Damage)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), D.Id);
		O->SetStringField(TEXT("kind"), D.Kind);
		O->SetNumberField(TEXT("at"), D.At);
		auto Pair = [&](const TCHAR* K, const FVector2D& V) { O->SetArrayField(K, { MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y) }); };
		Pair(TEXT("pos"), D.Pos); Pair(TEXT("dir"), D.Dir); Pair(TEXT("out"), D.Out);
		A.Add(MakeShared<FJsonValueObject>(O));
	}
	FString Text;
	const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(A, W);
	return Text;
}

bool BrixDamageFromJson(const FString& Text, TArray<FBrixDamage>& Out)
{
	Out.Reset();
	TArray<TSharedPtr<FJsonValue>> A;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), A)) return false;
	for (const TSharedPtr<FJsonValue>& V : A)
	{
		const TSharedPtr<FJsonObject>* O = nullptr;
		if (!V->TryGetObject(O)) return false;
		FBrixDamage D;
		D.Id = (*O)->GetStringField(TEXT("id"));
		D.Kind = (*O)->GetStringField(TEXT("kind"));
		D.At = (*O)->GetNumberField(TEXT("at"));
		auto Pair = [&](const TCHAR* K, FVector2D& P) { const TArray<TSharedPtr<FJsonValue>>* X = nullptr; if ((*O)->TryGetArrayField(K, X) && X->Num() == 2) P = FVector2D((*X)[0]->AsNumber(), (*X)[1]->AsNumber()); };
		Pair(TEXT("pos"), D.Pos); Pair(TEXT("dir"), D.Dir); Pair(TEXT("out"), D.Out);
		Out.Add(D);
	}
	return true;
}

double BrixToppleAngle(const FBrixCrashRules& R, double E)
{
	const double T = R.ToppleS, u = E < 0 ? 1 : FMath::Min(1.0, E / T), fall = UE_DOUBLE_PI / 2 - .06;
	return fall * u * u - (u >= 1 && E >= 0 ? .07 * FMath::Abs(FMath::Sin((E - T) * 9)) * FMath::Exp(-(E - T) * 4) : 0);
}

void BrixBurst(TArray<FBrixPiece>& Out, const FVector& At, double vx, double vz, double size, const TArray<FVector3f>& Cols, int32 N, int32 Seed)
{
	FRandomStream Rn(Seed);   // smash.js takes Math.random(); a seed here, so a test can repeat it
	for (int32 k = 0; k < N; ++k)
	{
		const double r1 = Rn.FRand(), r2 = Rn.FRand(), r3 = Rn.FRand(), s = size * (.45 + .7 * Rn.FRand());
		FBrixPiece P;
		P.P = FVector(At.X + (r1 - .5) * size * 2, At.Y + r2 * size * 2, At.Z + (r3 - .5) * size * 2);
		P.V = FVector(vx * (.35 + .5 * r1) + (r2 - .5) * 3, 2 + 4 * r3, vz * (.35 + .5 * r2) + (r1 - .5) * 3);
		P.S = s; P.H = s * (.5 + .6 * r2); P.Yaw = r3 * 6.28; P.Spin = (r1 - .5) * 14;
		P.C = Cols.Num() ? Cols[k % Cols.Num()] : FVector3f(.5f);
		Out.Add(P);
	}
}

void BrixRubble(TArray<FBrixPiece>& Out, const FBrixCrashRules& R, double x, double z, double size, const TArray<FVector3f>& Cols, double seed, TFunctionRef<double(double, double)> Ground)
{
	for (int32 k = 0; k < R.RestPieces; ++k)
	{
		const double a = rnd(seed, k) * 6.28, d = .3 + 1.4 * rnd(seed, k + 50), s = size * (.5 + .6 * rnd(seed, k + 90)), px = x + FMath::Cos(a) * d, pz = z + FMath::Sin(a) * d;
		FBrixPiece P;
		P.P = FVector(px, Ground(px, pz) + s * .25, pz);
		P.S = s; P.H = s * .5; P.Yaw = a * 3;
		P.C = Cols.Num() ? Cols[k % Cols.Num()] : FVector3f(.5f);
		P.bRest = true;
		Out.Add(P);
	}
}

void BrixStepPieces(TArray<FBrixPiece>& Ps, const FBrixCrashRules& R, double dt, TFunctionRef<double(double, double)> Ground)
{
	for (FBrixPiece& p : Ps)
	{
		if (p.bRest) continue;
		p.V.Y -= R.Gravity * dt;
		p.P += p.V * dt;
		p.Yaw += p.Spin * dt;
		const double g = Ground(p.P.X, p.P.Z) + p.H / 2;
		if (p.P.Y < g)
		{
			p.P.Y = g; p.V.Y = -p.V.Y * R.Bounce; p.V.X *= .6; p.V.Z *= .6; p.Spin *= .6;
			if (FMath::Abs(p.V.Y) < .6 && FMath::Sqrt(p.V.X * p.V.X + p.V.Z * p.V.Z) < .4) p.bRest = true;
		}
	}
}

bool BrixCrashReact(const FBrixCrashRules& R, int32 i, const FVector2D& Pos, double Sx, double Sz, double Since, double SmashT, double Now, FBrixDodged& q)
{
	if (Since < 0 || Since >= R.ReactS || FVector2D::Distance(FVector2D(Sx, Sz), Pos) >= R.ReactM || !R.Shouts.Num()) return false;
	const double f = Since / R.ReactS, k = sm(0, .1, f) * (1 - sm(.85, 1, f)), shake = FMath::Sin(Now * 15 + i) * 9 * k;
	q.Pos = Pos; q.Hop = 0; q.Pose = 0; q.bWalking = false; q.Sig.Reset();
	q.Yaw = FMath::Atan2(Sx - Pos.X, -(Sz - Pos.Y));
	q.Arms[0] = FVector4d(k, 140 + shake, 38, 25);
	q.Arms[1] = FVector4d(k, 140 - shake, 38, 25);
	q.Chest = FVector2D(-5 * k, 0);
	q.Shout = f < .85 ? R.Shouts[FMath::FloorToInt(rnd(i, FMath::FloorToDouble(SmashT)) * R.Shouts.Num())] : FString();
	return true;
}
