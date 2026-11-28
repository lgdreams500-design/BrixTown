#include "BrixTubesActor.h"

#include "BrixTownActor.h"
#include "BrixUnits.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace
{
	const double M = BrixUnits::CmPerM;
	FVector Cm(const FVector& P) { return P * M; }                       // the map's metres (x, z, height) to the engine's
	const FLinearColor StoneC(.52f, .48f, .43f), StoneDarkC(.37f, .34f, .32f), IronC(.17f, .18f, .21f), CreamC(.66f, .61f, .50f);
}

ABrixTubes::ABrixTubes()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Tubes"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->bUseAsyncCooking = true;
	Lanterns = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TubeLanterns"));
	Lanterns->SetupAttachment(Mesh);
	Lanterns->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lanterns->NumCustomDataFloats = 3;
	Solid = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TubeSolid"));
	Solid->SetupAttachment(Mesh);
	Solid->bUseComplexAsSimpleCollision = true;
	Solid->SetCollisionProfileName(TEXT("BlockAll"));
}

FString ABrixTubes::Choice(const FString& From, int32 Pick) const
{
	const TArray<FString>& O = TubeNet.StopOrder;
	const int32 i = O.IndexOfByKey(From);
	if (i == INDEX_NONE || O.Num() < 2) return FString();
	const int32 n = O.Num() - 1;                            // the others, in the file's order, starting after From
	const int32 k = ((Pick % n) + n) % n;
	return O[(i + 1 + k) % O.Num()];
}

bool ABrixTubes::Start(const FString& NetPath, const FString& RulesPath, ABrixTownActor* Town)
{
	TownRef = Town;
	FString Error;
	if (!LoadBrixTubeNet(NetPath, TubeNet, Error) || !LoadBrixTubeRules(RulesPath, TubeRules, Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("Brix Town: no glass tubes: %s"), *Error);
		return false;
	}
	for (const FString& P : BrixTubeProblems(TubeNet, TubeRules)) UE_LOG(LogTemp, Warning, TEXT("Brix Town: tubes.json: %s"), *P);
	for (const FString& Id : TubeNet.StopOrder) Booth(TubeNet.Stops[Id]);
	for (const FBrixTube& T : TubeNet.Tubes) Tube(T.Line);
	const double Tr = TubeRules.TubeInnerM / 2;
	for (const FBrixTubeNet::FPost& P : TubeNet.Posts)       // the posts where the map puts them: on the pavement, clear of the street's things
	{
		const FVector At(P.At, 0), Dir(P.Along, 0);
		Cylinder(IronPost, At * M, .09 * M, (P.Z - Tr - .06) * M, 14);
		Cylinder(StoneDark, At * M, .24 * M, .25 * M, 20);
		Ring(Iron, FVector(P.At, P.Z) * M, Dir, (Tr + .03) * M, .045 * M);
	}
	Flush();
	bReady = true;
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %d tube stops, %d tubes (%d glass triangles)"), TubeNet.Stops.Num(), TubeNet.Tubes.Num(), Glass.T.Num() / 3);
	return true;
}

void ABrixTubes::Sweep(FMeshPart& P, const TArray<FVector>& Pts, double R, int32 Sides, bool bClosed) const
{
	const int32 N = Pts.Num();
	if (N < 2) return;
	const int32 Base = P.V.Num();
	FVector Nrm = FVector::ZeroVector;
	for (int32 i = 0; i < N; ++i)
	{
		const FVector A = (bClosed || i > 0) ? Pts[(i - 1 + N) % N] : Pts[i];
		const FVector B = (bClosed || i < N - 1) ? Pts[(i + 1) % N] : Pts[i];
		const FVector T = (B - A).GetSafeNormal();
		if (i == 0) Nrm = FVector::CrossProduct(T, FMath::Abs(T.Z) < .95 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
		else Nrm = (Nrm - T * FVector::DotProduct(Nrm, T)).GetSafeNormal();   // carried along without a twist
		const FVector Bi = FVector::CrossProduct(T, Nrm);
		for (int32 k = 0; k < Sides; ++k)
		{
			const double A2 = 2 * PI * k / Sides;
			const FVector D = Nrm * FMath::Cos(A2) + Bi * FMath::Sin(A2);
			P.V.Add(Pts[i] + D * R);
			P.N.Add(D);
		}
	}
	const int32 Rings = bClosed ? N : N - 1;
	for (int32 i = 0; i < Rings; ++i)
		for (int32 k = 0; k < Sides; ++k)
		{
			const int32 a = Base + i * Sides + k, b = Base + i * Sides + (k + 1) % Sides;
			const int32 c = Base + ((i + 1) % N) * Sides + k, d = Base + ((i + 1) % N) * Sides + (k + 1) % Sides;
			P.T.Append({ a, c, b, b, c, d });
		}
}

void ABrixTubes::Cylinder(FMeshPart& P, const FVector& Base, double R, double H, int32 Sides, double RTop, double GapDeg, double GapYawDeg, bool bCaps) const
{
	if (RTop < 0) RTop = R;
	const bool bGap = GapDeg > 0;
	const double From = bGap ? FMath::DegreesToRadians(GapYawDeg + GapDeg / 2) : 0;
	const double Span = 2 * PI - (bGap ? FMath::DegreesToRadians(GapDeg) : 0);
	const int32 Cols = Sides;
	const int32 Base0 = P.V.Num();
	const double Slope = (R - RTop) / FMath::Max(H, 1e-6);
	for (int32 k = 0; k <= Cols; ++k)
	{
		const double A = From + Span * k / Cols;
		const FVector D(FMath::Cos(A), FMath::Sin(A), 0);
		const FVector Nn = (D + FVector(0, 0, Slope)).GetSafeNormal();
		P.V.Add(Base + D * R); P.N.Add(Nn);
		P.V.Add(Base + D * RTop + FVector(0, 0, H)); P.N.Add(Nn);
	}
	for (int32 k = 0; k < Cols; ++k)
	{
		const int32 a = Base0 + 2 * k, b = a + 1, c = a + 2, d = a + 3;
		P.T.Append({ a, b, c, c, b, d });
	}
	if (!bCaps || bGap) return;
	for (int32 End = 0; End < 2; ++End)
	{
		const double Z = End ? H : 0, Rr = End ? RTop : R;
		const int32 C = P.V.Num();
		P.V.Add(Base + FVector(0, 0, Z)); P.N.Add(FVector(0, 0, End ? 1 : -1));
		for (int32 k = 0; k < Sides; ++k)
		{
			const double A = 2 * PI * k / Sides;
			P.V.Add(Base + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, Z)); P.N.Add(FVector(0, 0, End ? 1 : -1));
		}
		for (int32 k = 0; k < Sides; ++k)
		{
			const int32 a = C + 1 + k, b = C + 1 + (k + 1) % Sides;
			if (End) P.T.Append({ C, b, a }); else P.T.Append({ C, a, b });
		}
	}
}

void ABrixTubes::Ring(FMeshPart& P, const FVector& C, const FVector& Axis, double R, double Thick) const
{
	const FVector A = Axis.GetSafeNormal();
	const FVector U = FVector::CrossProduct(A, FMath::Abs(A.Z) < .95 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
	const FVector V = FVector::CrossProduct(U, A);
	TArray<FVector> Pts;
	for (int32 k = 0; k < 32; ++k) { const double T = 2 * PI * k / 32; Pts.Add(C + (U * FMath::Cos(T) + V * FMath::Sin(T)) * R); }
	Sweep(P, Pts, Thick, 8, true);
}

void ABrixTubes::BoxAt(FMeshPart& P, const FVector& C, const FVector& H) const
{
	static const int32 F[6][4] = { {0,1,3,2}, {4,6,7,5}, {0,4,5,1}, {2,3,7,6}, {0,2,6,4}, {1,5,7,3} };
	static const FVector Nf[6] = { FVector(-1,0,0), FVector(1,0,0), FVector(0,-1,0), FVector(0,1,0), FVector(0,0,-1), FVector(0,0,1) };
	for (int32 f = 0; f < 6; ++f)
	{
		const int32 B = P.V.Num();
		for (int32 j = 0; j < 4; ++j)
		{
			const int32 c = F[f][j];
			P.V.Add(C + FVector((c & 4) ? H.X : -H.X, (c & 2) ? H.Y : -H.Y, (c & 1) ? H.Z : -H.Z));
			P.N.Add(Nf[f]);
		}
		P.T.Append({ B, B + 1, B + 2, B, B + 2, B + 3 });
	}
}

void ABrixTubes::Booth(const FBrixTubeStop& S)
{
	const FBrixTubeRules& R = TubeRules;
	const FVector O(S.Pos.X * M, S.Pos.Y * M, 0);
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(S.Face.Y, S.Face.X));
	const double Tr = R.TubeInnerM / 2;
	Cylinder(StoneDark, O, R.PlinthR * M, R.PlinthH * M, 40);
	Cylinder(Stone, O + FVector(0, 0, R.PlinthH * M), (R.PlinthR - .15) * M, .06 * M, 40);
	const double Z0 = R.PlinthH + .06, Z1 = R.PlinthH + R.BoothH;
	Cylinder(BoothWall, O + FVector(0, 0, Z0 * M), R.BoothR * M, (Z1 - Z0) * M, 40, -1, R.DoorDeg, Yaw, false);
	for (const double Z : { Z0 + .05, Z0 + 1.05 })            // brass bands round the glass, open at the door
	{
		TArray<FVector> Arc;
		for (int32 k = 0; k <= 36; ++k)
		{
			const double A = FMath::DegreesToRadians(Yaw + R.DoorDeg / 2 + (360 - R.DoorDeg) * k / 36);
			Arc.Add(O + FVector(FMath::Cos(A), FMath::Sin(A), 0) * (R.BoothR + .02) * M + FVector(0, 0, Z * M));
		}
		Sweep(Brass, Arc, .05 * M, 8, false);
	}
	Ring(Brass, O + FVector(0, 0, (Z1 - .05) * M), FVector::UpVector, (R.BoothR + .02) * M, .05 * M);
	Cylinder(Cream, O + FVector(0, 0, Z1 * M), (R.BoothR + .12) * M, .12 * M, 40);
	const double F0 = Z1 + .12, F1 = F0 + R.FunnelH;          // the booth narrows into the tube
	Cylinder(Glass, O + FVector(0, 0, F0 * M), (R.BoothR + .04) * M, R.FunnelH * M, 40, Tr * M, 0, 0, false);
	Ring(Brass, O + FVector(0, 0, F1 * M), FVector::UpVector, (Tr + .02) * M, .04 * M);
	Sweep(Glass, { O + FVector(0, 0, F1 * M), O + FVector(0, 0, R.DrawInHeightM * M) }, Tr * M, TubeSides, false);   // up to the tube's mouth
	for (const double A : { Yaw + R.DoorDeg / 2 + 3, Yaw - R.DoorDeg / 2 - 3, Yaw + 180 })   // three ribs, the door between two
	{
		const FVector D(FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A)), 0);
		Cylinder(Brass, O + D * (R.BoothR + .02) * M + FVector(0, 0, Z0 * M), .045 * M, (Z1 - Z0 - .06) * M, 10);
	}
	const FVector L(FMath::Cos(FMath::DegreesToRadians(Yaw + R.DoorDeg / 2 + 25)), FMath::Sin(FMath::DegreesToRadians(Yaw + R.DoorDeg / 2 + 25)), 0);
	const FVector At = O + L * (R.BoothR + .2) * M + FVector(0, 0, (Z1 - .25) * M);   // the lantern by the door
	LanternAt.Add(At);
	if (TownRef) TownRef->LampLights.Add(FVector4f((float)(At.X / M), (float)(At.Z / M), (float)(At.Y / M), LanternLight));   // x, up, z: lit with the street lamps
	BoxAt(Iron, At + FVector(0, 0, .16 * M), FVector(.11, .11, .03) * M);
	BoxAt(Iron, At - L * .14 * M, FVector(.05, .05, .02) * M + FVector(FMath::Abs(L.X), FMath::Abs(L.Y), 0) * .05 * M);
}

void ABrixTubes::Tube(const TArray<FVector>& Line)
{
	const FBrixTubeRules& R = TubeRules;
	const double Tr = R.TubeInnerM / 2;
	const TArray<FVector> L = BrixTubeRounded(Line, R.BendRM);
	TArray<FVector> Pts;
	for (const FVector& P : L) Pts.Add(Cm(P));
	Sweep(Glass, Pts, Tr * M, TubeSides, false);
	double ToRing = R.RingEveryM * .5;
	for (int32 i = 1; i < L.Num(); ++i)
	{
		const FVector A = L[i - 1], B = L[i];
		const double Seg = FVector::Dist(A, B);
		if (Seg < 1e-6) continue;
		const FVector Dir = (B - A) / Seg;
		for (double s = 0; s < Seg; )
		{
			const double Step = FMath::Min(Seg - s, ToRing);
			s += Step; ToRing -= Step;
			const FVector P = A + Dir * s;
			if (ToRing <= 1e-6) { Ring(Brass, Cm(P), Dir, (Tr + .015) * M, .025 * M); ToRing = R.RingEveryM; }
		}
	}
}

void ABrixTubes::Flush()
{
	UMaterialInterface* Grey = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Greybox/M_Greybox.M_Greybox"));
	UMaterialInterface* GlassM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Greybox/M_TubeGlass.M_TubeGlass"));
	if (!GlassM) UE_LOG(LogTemp, Warning, TEXT("Brix Town: /Game/Greybox/M_TubeGlass is missing (run Tools/unreal/make_tube_glass_material.py)"));
	auto Colour = [&](const FLinearColor& C) -> UMaterialInterface*
	{
		if (!Grey) return nullptr;
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Grey, this);
		D->SetVectorParameterValue(TEXT("Color"), C);
		D->SetScalarParameterValue(TEXT("InstanceColour"), 0.f);
		Mats.Add(D);
		return D;
	};
	if (GlassM)
	{
		GlassMat = UMaterialInstanceDynamic::Create(GlassM, this);
		GlassMat->SetVectorParameterValue(TEXT("Color"), TubeRules.Glass);
		GlassMat->SetScalarParameterValue(TEXT("Alpha"), (float)TubeRules.GlassAlpha);
		GlassMat->SetScalarParameterValue(TEXT("Edge"), (float)TubeRules.GlassEdge);
	}
	int32 Sec = 0;
	auto Put = [&](UProceduralMeshComponent* C, FMeshPart& P, UMaterialInterface* Mat, bool bCollide)
	{
		if (!P.T.Num()) return;
		const int32 S = C == Mesh ? Sec++ : C->GetNumSections();
		C->CreateMeshSection(S, P.V, P.T, P.N, {}, {}, {}, bCollide);
		if (Mat) C->SetMaterial(S, Mat);
		P = FMeshPart();
	};
	Put(Mesh, Glass, GlassMat, false);
	Put(Mesh, Brass, Colour(TubeRules.Brass), false);
	Put(Mesh, Iron, Colour(IronC), false);
	Put(Mesh, Cream, Colour(CreamC), false);
	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))   // lantern glass as the street lamps' (BrixTownActor glass())
	{
		Lanterns->SetStaticMesh(Cube);
		if (TownRef && TownRef->GlassMat) Lanterns->SetMaterial(0, TownRef->GlassMat);
		for (const FVector& At : LanternAt)
		{
			const int32 I = Lanterns->AddInstance(FTransform(FQuat::Identity, At, FVector(.18, .18, .26)));
			Lanterns->SetCustomData(I, { 1.f, .55f, .22f });
		}
	}
	Put(Solid, StoneDark, Colour(StoneDarkC), true);
	Put(Solid, Stone, Colour(StoneC), true);
	Put(Solid, IronPost, Colour(IronC), true);
	Put(Solid, BoothWall, GlassMat, true);
	for (UProceduralMeshComponent* C : { Mesh.Get(), Solid.Get() })
	{
		C->SetCastShadow(true);
		C->LDMaxDrawDistance = CullM * M;
		C->MarkRenderStateDirty();
	}
	Mesh->SetCastShadow(true);
}
