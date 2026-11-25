#include "BrixCrashActor.h"

#include "BrixCity.h"
#include "BrixGameMode.h"
#include "BrixTownActor.h"
#include "BrixUnits.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

using BrixUnits::ToWorld;
using BrixUnits::CmPerM;

namespace
{
	const double G = 4;   // the grid's cell (smash.js G)
	double rnd(double a, double b) { const double s = FMath::Sin(a * 12.9898 + b * 78.233) * 43758.5453; return s - FMath::FloorToDouble(s); }
	FIntPoint Cell(double X, double Z) { return FIntPoint(FMath::FloorToInt(X / G), FMath::FloorToInt(Z / G)); }
	const FVector3f StoneC[3] = { FVector3f(.37f, .34f, .32f), FVector3f(.52f, .48f, .43f), FVector3f(.3f, .28f, .27f) };

	struct FMeshOut
	{
		TArray<FVector> V, N;
		TArray<FLinearColor> C;
		TArray<int32> Tri;
		// a box: its eight corners from a centre, half-sizes along three axes (Unreal's frame, centimetres), one colour
		void Box(const FVector& Mid, const FVector& Ax, const FVector& Ay, const FVector& Az, const FLinearColor& Col)
		{
			auto P = [&](int32 i) { return Mid + Ax * (i & 1 ? 1 : -1) + Ay * (i & 2 ? 1 : -1) + Az * (i & 4 ? 1 : -1); };
			static const int32 Faces[6][4] = { { 0, 2, 6, 4 }, { 1, 5, 7, 3 }, { 0, 4, 5, 1 }, { 2, 3, 7, 6 }, { 0, 1, 3, 2 }, { 4, 6, 7, 5 } };
			const FVector Out[6] = { -Ax.GetSafeNormal(), Ax.GetSafeNormal(), -Ay.GetSafeNormal(), Ay.GetSafeNormal(), -Az.GetSafeNormal(), Az.GetSafeNormal() };
			for (int32 f = 0; f < 6; ++f)
			{
				FVector Q[4] = { P(Faces[f][0]), P(Faces[f][1]), P(Faces[f][2]), P(Faces[f][3]) };
				if (FVector::DotProduct(FVector::CrossProduct(Q[1] - Q[0], Q[2] - Q[0]), Out[f]) > 0) Swap(Q[1], Q[3]);   // the people's turn (ABrixPeople's far figures)
				for (const int32 k : { 0, 1, 2, 0, 2, 3 }) { Tri.Add(V.Num()); V.Add(Q[k]); N.Add(Out[f]); C.Add(Col); }
			}
		}
		// a box in the slice's frame (x, y up, z; metres): a size standing at a point, turned by yaw
		void YBox(double x, double y0, double z, double w, double h, double d, double yaw, const FVector3f& Col)
		{
			const FVector Mid = ToWorld(FVector2D(x, z), (float)(y0 + h / 2));
			const FQuat Q(FVector::UpVector, -yaw);
			Box(Mid, Q.RotateVector(FVector(w / 2 * CmPerM, 0, 0)), Q.RotateVector(FVector(0, d / 2 * CmPerM, 0)), FVector(0, 0, h / 2 * CmPerM), FLinearColor(Col.X, Col.Y, Col.Z));
		}
		// one face, turned as Box() turns its faces (Unreal's frame, centimetres)
		void Face(const FVector& A, const FVector& B, const FVector& Cc, const FVector& Nrm, const FLinearColor& Col)
		{
			FVector Q[3] = { A, B, Cc };
			if (FVector::DotProduct(FVector::CrossProduct(Q[1] - Q[0], Q[2] - Q[0]), Nrm) > 0) Swap(Q[1], Q[2]);
			for (int32 k = 0; k < 3; ++k) { Tri.Add(V.Num()); V.Add(Q[k]); N.Add(Nrm); C.Add(Col); }
		}
		void Upload(UProceduralMeshComponent* M, UMaterialInterface* Mat, int32& Had) const
		{
			static const TArray<FVector2D> NoUV;
			static const TArray<FProcMeshTangent> NoT;
			if (Tri.Num() != Had) { M->CreateMeshSection_LinearColor(0, V, Tri, N, NoUV, C, NoT, false, false); M->SetMaterial(0, Mat); Had = Tri.Num(); }
			else M->UpdateMeshSection_LinearColor(0, V, N, NoUV, C, NoT, false);
		}
	};
}

ABrixCrash::ABrixCrash()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/People/M_People.M_People"));   // the vertex colour, matte
	Material = Mat.Object;
	Debris = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Debris"));
	Debris->SetupAttachment(RootComponent);
	Debris->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABrixCrash::Start(ABrixTownActor* InTown, const FBrixCity& City, int32 StartDay)
{
	Town = InTown;
	Day0 = StartDay;
	Pace = City.Tram.Pace;
	FString Error;
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/crash.json")));
	bOn = Town && LoadBrixCrashRules(Path, R, Error);
	if (!bOn) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: %s; nothing breaks"), *Error); return; }
	for (const FBrixBlock& B : City.Blocks)
	{
		Paving.Add(FBox2D(B.Origin, B.Origin + B.Extent));
		for (const FBrixLot& L : B.Lots)
		{
			Lots.Add({ L.Id, L.Kind, L.Min, L.Max });
			if (L.Kind == TEXT("utility") && L.bEnterable) Depots.Add({ L.Id, L.Door });
		}
	}
	TArray<FBrixBreakable>& All = Town->Breakables;
	for (int32 b = 0; b < All.Num(); ++b)
	{
		FBrixBreakable& Bk = All[b];
		Bk.Kind = R.KindOf(Bk.PropKind);
		FString Id = FString::Printf(TEXT("%s@%.1f,%.1f"), *Bk.Kind, Bk.Pos.X, Bk.Pos.Y);   // smash.js's ids
		for (int32 n = 2; ById.Contains(Id); ++n) Id = FString::Printf(TEXT("%s@%.1f,%.1f#%d"), *Bk.Kind, Bk.Pos.X, Bk.Pos.Y, n);
		ById.Add(Id, b);
		const FIntPoint A = Cell(Bk.Box.Min.X, Bk.Box.Min.Y), C = Cell(Bk.Box.Max.X, Bk.Box.Max.Y);
		for (int32 x = A.X; x <= C.X; ++x) for (int32 z = A.Y; z <= C.Y; ++z) Grid.Add(FIntPoint(x, z), b);
	}
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %d street things and trees a car can break, %d repair depots"), All.Num(), Depots.Num());
}

TSet<FString> ABrixCrash::BreakableKinds()
{
	FBrixCrashRules Rules;
	FString Error;
	TSet<FString> Out;
	if (!LoadBrixCrashRules(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/crash.json"))), Rules, Error)) return Out;
	for (const TPair<FString, FBrixCrashKind>& K : Rules.Kinds) if (K.Value.How != TEXT("chip")) Out.Add(K.Key);
	for (const TPair<FString, FString>& A : Rules.Aliases) Out.Add(A.Key);
	return Out;
}

double ABrixCrash::Now() const
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	return Day0 * 1440.0 + (Mode ? Mode->ClockMinutes() : 0.0);
}

double ABrixCrash::Ground(double X, double Z) const
{
	for (const FBox2D& B : Paving) if (B.IsInside(FVector2D(X, Z))) return Town ? Town->BlockSlabHeight : .12;
	return 0;
}

// ---- the damage list and its repairs (smash.js init(), replan(), save())

void ABrixCrash::Init()
{
	bReady = true;
	FString Text;
	const FString File = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BrixDamage.json"));
	if (FFileHelper::LoadFileToString(Text, *File) && !BrixDamageFromJson(Text, Damage)) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: %s is not a damage list; starting with none"), *File); Damage.Reset(); }
	if (FParse::Param(FCommandLine::Get(), TEXT("BrixWhole"))) Damage.Reset();   // for a test: everything whole
	const double t = Now();
	for (FBrixDamage& D : Damage) D.At = FMath::Min(D.At, t);   // a new life started since: nothing broke in its future
	Replan();
	for (const FBrixDamage& D : Damage)
	{
		const FBrixRepair* P = Plan.Find(D.Id);
		if (P && P->Done <= t) continue;
		if (P) UE_LOG(LogTemp, Display, TEXT("Brix Town: %s broken; crew %s arrives %02d:%02d, done %02d:%02d"), *D.Id, *P->Crew,
			(int32)FMath::Fmod(P->Arrive, 1440.0) / 60, (int32)FMath::Fmod(P->Arrive, 1440.0) % 60, (int32)FMath::Fmod(P->Done, 1440.0) / 60, (int32)FMath::Fmod(P->Done, 1440.0) % 60);
		if (const int32* b = ById.Find(D.Id)) { Off(*b); Effects.Add(D.Id, Rest(*b, D)); }
		else if (D.Kind == TEXT("corner")) Effects.Add(D.Id, ChipRest(D));
	}
	if (Damage.Num()) UE_LOG(LogTemp, Display, TEXT("Brix Town: %d things broken from before, still to mend"), Effects.Num());
}

void ABrixCrash::Replan() { Plan = BrixRepairs(Damage, Depots, R, Pace); }

void ABrixCrash::Save() const
{
	FFileHelper::SaveStringToFile(BrixDamageToJson(Damage), *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BrixDamage.json")));
}

void ABrixCrash::Off(int32 b) { Town->SetBroken(b, true); }
void ABrixCrash::On(int32 b) { Town->SetBroken(b, false); }

// ---- what a broken thing looks like (smash.js colours(), rest(), chipRest())

TArray<FVector3f> ABrixCrash::Colours(int32 b, int32 N, double Seed) const
{
	TArray<FVector3f> Out;
	const TArray<FBrixBreakable::FPart>& P = Town->Breakables[b].Parts;
	for (int32 k = 0; k < N; ++k) { const FLinearColor& C = P[FMath::Min(P.Num() - 1, (int32)(rnd(Seed, k) * P.Num()))].Colour; Out.Add(FVector3f(C.R, C.G, C.B)); }
	return Out;
}

double ABrixCrash::Size(int32 b) const
{
	const FBox2D& B = Town->Breakables[b].Box;
	return FMath::Clamp(FMath::Max(B.Max.X - B.Min.X, B.Max.Y - B.Min.Y) / 3, .12, .35);
}

ABrixCrash::FEffect ABrixCrash::Rest(int32 b, const FBrixDamage& D)
{
	FEffect E;
	E.B = b; E.D = D;
	const FBrixCrashKind* K = R.Kinds.Find(Town->Breakables[b].Kind);
	E.How = K ? K->How : TEXT("shatter");
	if (E.How == TEXT("topple")) { Topple(E); return E; }
	const FBrixBreakable& Bk = Town->Breakables[b];
	BrixRubble(E.P, R, Bk.Pos.X, Bk.Pos.Y, Size(b), Colours(b, 6, Bk.Pos.X + Bk.Pos.Y), Bk.Pos.X * 3 + Bk.Pos.Y, [this](double x, double z) { return Ground(x, z); });
	return E;
}

ABrixCrash::FEffect ABrixCrash::ChipRest(const FBrixDamage& D)
{
	FEffect E;
	E.How = TEXT("chip"); E.D = D;
	BrixRubble(E.P, R, D.Pos.X + D.Out.X * .8, D.Pos.Y + D.Out.Y * .8, .24, { StoneC[0], StoneC[1], StoneC[2] }, D.Pos.X + D.Pos.Y * 7, [this](double x, double z) { return Ground(x, z); });
	return E;
}

// a copy of the thing's own boxes, about its foot, to topple (smash.js emit() and rodrigues())
void ABrixCrash::Topple(FEffect& E)
{
	const FBrixBreakable& Bk = Town->Breakables[E.B];
	const FVector Foot = ToWorld(Bk.Pos, Bk.Ground);
	FMeshOut O;
	for (const FBrixBreakable::FPart& P : Bk.Parts)
	{
		const FQuat Q = P.At.GetRotation();
		const FVector S = P.At.GetScale3D() * (CmPerM / 2);
		O.Box(P.At.GetLocation() - Foot, Q.RotateVector(FVector(S.X, 0, 0)), Q.RotateVector(FVector(0, S.Y, 0)), Q.RotateVector(FVector(0, 0, S.Z)), P.Colour);
	}
	for (int32 t = 0; t < Bk.FaceN.Num(); ++t)   // a tree's crown
		O.Face(FVector(Bk.FaceP[3 * t]) - Foot, FVector(Bk.FaceP[3 * t + 1]) - Foot, FVector(Bk.FaceP[3 * t + 2]) - Foot, FVector(Bk.FaceN[t]), Bk.FaceC[t]);
	UProceduralMeshComponent* M = Pool.Num() ? Pool.Pop().Get() : nullptr;
	if (!M)
	{
		M = NewObject<UProceduralMeshComponent>(this);
		M->SetupAttachment(RootComponent);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->RegisterComponent();
		AddInstanceComponent(M);
	}
	int32 Had = -1;
	O.Upload(M, Material, Had);
	M->SetWorldLocation(Foot);
	M->SetVisibility(true);
	E.M = M;
	// the way it falls: the top towards the way the car went
	const FVector Dir(E.D.Dir.X, E.D.Dir.Y, 0);
	E.Axis = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
	if (FVector::DotProduct(FQuat(E.Axis, .5).RotateVector(FVector::UpVector), Dir) < 0) E.Axis = -E.Axis;
	M->SetWorldRotation(FQuat(E.Axis, BrixToppleAngle(R, E.T0 < 0 ? -1 : 0)));
}

// ---- the car hitting things

int32 ABrixCrash::Probe(double X, double Y, double Z) const
{
	if (!bOn) return -1;
	TArray<int32> In;
	Grid.MultiFind(Cell(X, Z), In);
	const double M = .2;   // wider than the car's own margin, so the thing is met before its solid
	for (const int32 b : In)
	{
		const FBrixBreakable& Bk = Town->Breakables[b];
		if (!Bk.bBroken && X >= Bk.Box.Min.X - M && X <= Bk.Box.Max.X + M && Z >= Bk.Box.Min.Y - M && Z <= Bk.Box.Max.Y + M && Y < Bk.Top + .2) return b;
	}
	return -1;
}

double ABrixCrash::Smash(int32 b, double vx, double vz)
{
	if (!bOn || !Town->Breakables.IsValidIndex(b) || Town->Breakables[b].bBroken) return 1;
	if (!bReady) Init();
	const FBrixBreakable& Bk = Town->Breakables[b];
	const FString* Id = ById.FindKey(b);
	const double sp = FMath::Max(FMath::Sqrt(vx * vx + vz * vz), 1e-6);
	FBrixDamage D{ Id ? *Id : Bk.Kind, Bk.Kind, Now(), Bk.Pos, FVector2D(vx / sp, vz / sp) };
	Off(b);
	const double t = GetWorld()->GetTimeSeconds();
	FEffect E;
	E.B = b; E.D = D; E.T0 = t;
	const FBrixCrashKind* K = R.Kinds.Find(Bk.Kind);
	E.How = K ? K->How : TEXT("shatter");
	if (E.How == TEXT("topple")) Topple(E);
	else BrixBurst(E.P, FVector(Bk.Pos.X, Bk.Ground + .2, Bk.Pos.Y), vx, vz, Size(b), Colours(b, 8, t), R.Pieces, (int32)(t * 1000));
	if (FEffect* Old = Effects.Find(D.Id)) if (Old->M) { Old->M->SetVisibility(false); Pool.Add(Old->M); }
	const FString How = E.How;
	Effects.Add(D.Id, MoveTemp(E));
	Damage.RemoveAll([&](const FBrixDamage& O) { return O.Id == D.Id; });
	Damage.Add(D);
	Replan();
	Save();
	Recent.Add({ Bk.Pos, t });
	bDirty = true;
	UE_LOG(LogTemp, Display, TEXT("Brix Town: the player's car broke %s (%s)"), *D.Id, *How);
	return K ? K->Slow : 1;
}

void ABrixCrash::Chip(double x, double z, double vx, double vz)
{
	if (!bOn) return;
	if (!bReady) Init();
	const FLotRect* Best = nullptr;
	int32 Ci = 0;
	double Bd = 1e9;
	for (const FLotRect& L : Lots)
	{
		if (L.Kind == TEXT("garden") || L.Kind == TEXT("market") || L.Kind == TEXT("stop")) continue;
		if (x < L.Min.X - 2 || x > L.Max.X + 2 || z < L.Min.Y - 2 || z > L.Max.Y + 2) continue;
		const FVector2D Cs[4] = { L.Min, FVector2D(L.Max.X, L.Min.Y), L.Max, FVector2D(L.Min.X, L.Max.Y) };
		for (int32 c = 0; c < 4; ++c) { const double d = FVector2D::Distance(Cs[c], FVector2D(x, z)); if (d < R.CornerM && d < Bd) { Bd = d; Best = &L; Ci = c; } }
	}
	if (!Best) return;
	const FString Id = FString::Printf(TEXT("corner@%s:%d"), *Best->Id, Ci);
	if (Damage.ContainsByPredicate([&](const FBrixDamage& O) { return O.Id == Id; })) return;
	const FVector2D Cs[4] = { Best->Min, FVector2D(Best->Max.X, Best->Min.Y), Best->Max, FVector2D(Best->Min.X, Best->Max.Y) };
	const FVector2D P = Cs[Ci], Mid = (Best->Min + Best->Max) / 2, Out(FMath::Sign(P.X - Mid.X), FMath::Sign(P.Y - Mid.Y));
	const FBrixDamage D{ Id, TEXT("corner"), Now(), P, FVector2D(vx, vz), Out };
	Damage.Add(D);
	Replan();
	Save();
	const double t = GetWorld()->GetTimeSeconds();
	FEffect E = ChipRest(D);
	E.P.Reset();
	BrixBurst(E.P, FVector(P.X + Out.X * .3, Ground(P.X, P.Y) + .7, P.Y + Out.Y * .3), -vx * .3 + Out.X * 3, -vz * .3 + Out.Y * 3, .14, { StoneC[0], StoneC[1], StoneC[2] }, 10, (int32)(t * 1000));
	E.T0 = t;
	Effects.Add(Id, MoveTemp(E));
	Recent.Add({ P, t });
	bDirty = true;
	UE_LOG(LogTemp, Display, TEXT("Brix Town: the player's car chipped %s"), *Id);
}

// ---- each frame (smash.js draw())

TArray<FVector2D> ABrixCrash::BrokenSeats() const
{
	TArray<FVector2D> Out;
	if (!bOn || !Town || !Damage.Num()) return Out;
	for (const FBrixBreakable& B : Town->Breakables) if (B.bBroken && B.PropKind == TEXT("bench")) Out.Add(B.Pos);
	return Out;
}

void ABrixCrash::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!bOn || !PC || !PC->PlayerCameraManager) return;
	if (!bReady) Init();
	const double t = Now(), tSec = GetWorld()->GetTimeSeconds(), dt = FMath::Clamp(tSec - Last, 0.0, .05);
	Last = tSec;
	// mended: whole again
	bool bFixed = false;
	for (const FBrixDamage& D : Damage)
	{
		const FBrixRepair* P = Plan.Find(D.Id);
		if (!P || P->Done > t) continue;
		if (const int32* b = ById.Find(D.Id)) On(*b);
		if (FEffect* E = Effects.Find(D.Id)) if (E->M) { E->M->SetVisibility(false); Pool.Add(E->M); }
		Effects.Remove(D.Id);
		bFixed = true;
	}
	if (bFixed)
	{
		Damage.RemoveAll([&](const FBrixDamage& D) { const FBrixRepair* P = Plan.Find(D.Id); return P && P->Done <= t; });
		Replan();
		Save();
		bDirty = true;
		UE_LOG(LogTemp, Display, TEXT("Brix Town: mended by the repair crew; %d things still broken"), Damage.Num());
	}
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FVector2D Here(Eye.X / CmPerM, Eye.Y / CmPerM);
	FVector2D Me = Here;
	if (const APawn* Pawn = PC->GetPawn()) Me = FVector2D(Pawn->GetActorLocation().X / CmPerM, Pawn->GetActorLocation().Y / CmPerM);
	auto GroundF = [this](double x, double z) { return Ground(x, z); };
	FMeshOut O;
	bool bMoving = false;
	Workers.Reset();
	int32 Tag = 0;
	uint32 NearSet = 0;   // which damage is near: when it changes, the debris is made anew
	for (TPair<FString, FEffect>& KV : Effects)
	{
		FEffect& E = KV.Value;
		const bool bNear = FVector2D::Distance(E.D.Pos, Here) <= DrawDistance;
		if (E.M) E.M->SetVisibility(bNear);
		if (!bNear) continue;
		NearSet = HashCombine(NearSet, GetTypeHash(KV.Key));
		if (E.How == TEXT("topple"))
		{
			const double e = E.T0 < 0 ? -1 : tSec - E.T0;
			if (E.M) E.M->SetWorldRotation(FQuat(E.Axis, BrixToppleAngle(R, e)));
		}
		else
		{
			if (E.How == TEXT("chip"))   // the corner's rough core showing where stones broke away, and broken edge stones
			{
				const FBrixDamage& d = E.D;
				const double g = Ground(d.Pos.X, d.Pos.Y), ox = d.Out.X, oz = d.Out.Y;
				for (int32 i = 0; i < 5; ++i)
				{
					const double y0 = .15 + i * .26 + rnd(d.Pos.X, i) * .08, s = (.34 + rnd(d.Pos.Y, i) * .22) * (1 - i * .12), h = .22 + rnd(d.Pos.X + 1, i) * .12;
					const double cx = d.Pos.X + ox * (s * .5 + .03), cz = d.Pos.Y + oz * (s * .5 + .03);
					O.YBox(cx, g + y0, cz, s + .06, h, s + .06, 0, FVector3f(.19f, .17f, .16f));
					if (i % 2 == 0) O.YBox(d.Pos.X + ox * (s * .5 + .08), g + y0 + h, d.Pos.Y + oz * (s * .5 + .08), .14, .08, .1, rnd(d.Pos.Y, i) * 6, FVector3f(.6f, .56f, .5f));
				}
			}
			BrixStepPieces(E.P, R, dt, GroundF);
			if (E.T0 >= 0 && tSec - E.T0 > R.LifeS)   // the pieces that stay, settled
			{
				E.P.SetNum(FMath::Min(E.P.Num(), R.RestPieces));
				for (FBrixPiece& p : E.P) { p.bRest = true; p.P.Y = Ground(p.P.X, p.P.Z) + p.H / 2; }
				E.T0 = -1;
				bDirty = true;
			}
			for (const FBrixPiece& p : E.P)
			{
				bMoving |= !p.bRest;
				O.YBox(p.P.X, p.P.Y - p.H / 2, p.P.Z, p.S, p.H, p.S * .8, p.Yaw, p.C);
			}
		}
		// the repair crew at work, and the tag near the player
		const FBrixRepair* P = Plan.Find(KV.Key);
		const FVector2D Off = E.D.Out.IsNearlyZero() ? FVector2D(1, .3) : E.D.Out;
		if (P && t >= P->Arrive && t < P->Done) Workers.Add({ E.D.Pos + Off * 1.1, E.D.Pos, (int32)E.D.At });
		const double pd = FVector2D::Distance(E.D.Pos, Me);
		if (P && pd < TagDistance)
		{
			auto hh = [](double q) { const int32 m = (int32)FMath::Fmod(q, 1440.0); return FString::Printf(TEXT("%02d:%02d"), m / 60, m % 60); };
			const FString Text = t >= P->Arrive ? TEXT("The repair crew is at work - done at ") + hh(P->Done)
				: TEXT("Broken - the repair crew comes at ") + hh(P->Arrive) + (FMath::FloorToDouble(P->Arrive / 1440) > FMath::FloorToDouble(t / 1440) ? TEXT(" tomorrow") : TEXT(""));
			if (Tag >= RepairTags.Num())
			{
				UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
				T->SetupAttachment(RootComponent);
				T->SetHorizontalAlignment(EHTA_Center);
				T->SetVerticalAlignment(EVRTA_TextBottom);
				T->SetWorldSize(TagSize * CmPerM);
				T->SetTextRenderColor(TagColour.ToFColor(true));
				T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				T->CastShadow = false;
				T->RegisterComponent();
				AddInstanceComponent(T);
				RepairTags.Add(T);
			}
			UTextRenderComponent* T = RepairTags[Tag++];
			const FVector At = ToWorld(E.D.Pos, (float)Ground(E.D.Pos.X, E.D.Pos.Y) + 1.4f);
			T->SetText(FText::FromString(Text));
			T->SetWorldLocationAndRotation(At, (Eye - At).Rotation());
			T->SetVisibility(true);
		}
	}
	for (; Tag < RepairTags.Num(); ++Tag) RepairTags[Tag]->SetVisibility(false);
	if (bMoving || bDirty || NearSet != LastNearSet) { O.Upload(Debris, Material, DebrisVerts); bDirty = bMoving; LastNearSet = NearSet; }
	Recent.RemoveAll([&](const FRecent& S) { return tSec - S.T > R.ReactS; });
}
