#include "BrixMallActor.h"

#include "BrixCarsActor.h"
#include "BrixGameMode.h"
#include "BrixStreets.h"
#include "ProceduralMeshComponent.h"
#include "BrixRooms.h"
#include "BrixTownActor.h"
#include "BrixUnits.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "BrixDay.h"
#include "BrixGoods.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

using BrixUnits::CmPerM;

namespace
{
	// the slice's frame (x east, y up, z south, metres) to Unreal's
	FVector3f U(const FVector3d& Q) { return FVector3f((float)Q.X, (float)Q.Z, (float)Q.Y) * CmPerM; }
	FVector3f UN(const FVector3d& N) { const FVector3d n = N.GetSafeNormal(); return FVector3f((float)n.X, (float)n.Z, (float)n.Y); }

	// play.js Mesh.box()'s faces: four corners (0 the low end, 1 the high end of x, y, z) and the way out
	const int32 FACES[6][5][3] = {
		{ { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 1 }, { 0, 1, 0 }, { -1, 0, 0 } }, { { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 }, { 1, 0, 1 }, { 1, 0, 0 } }, { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 }, { 0, -1, 0 } },
		{ { 0, 1, 0 }, { 0, 1, 1 }, { 1, 1, 1 }, { 1, 1, 0 }, { 0, 1, 0 } }, { { 0, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } }, { { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 }, { 0, 0, 1 } } };

	enum EBucket : int32 { Matte, Panel, LanternGlass, Lattice, Room, NumBuckets };

	// The faces of one material in one square, made into a static mesh at the end
	struct FSink
	{
		TArray<FVector3f> P, N;
		TArray<FLinearColor> C;
		TArray<FVector2f> UV0, UV1;
	};
}

ABrixMall::ABrixMall()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ABrixMall::Start(TSharedPtr<FBrixMall> InMall, ABrixTownActor* Town)
{
	Mall = InMall;
	if (!Mall) return;
	const double T0 = FPlatformTime::Seconds();
	UMaterialInterface* Facets = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Greybox/M_Facets.M_Facets"));
	UMaterialInterface* LitM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mall/M_MallLit.M_MallLit"));
	UMaterialInterface* LatticeM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mall/M_MallLattice.M_MallLattice"));
	UMaterialInterface* RoomM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mall/M_MallRoom.M_MallRoom"));
	if (RoomM) { RoomMat = UMaterialInstanceDynamic::Create(RoomM, this); RoomMat->SetScalarParameterValue(TEXT("Glow"), RoomLight); RoomMat->SetScalarParameterValue(TEXT("Mottle"), RoomMottle); RoomMat->SetScalarParameterValue(TEXT("Under"), RoomUnder); }
	if (!LitM || !LatticeM) UE_LOG(LogTemp, Warning, TEXT("Brix Town: /Game/Mall's materials are missing (run Tools/unreal/make_mall_materials.py)"));
	if (LitM)
	{
		PanelMat = UMaterialInstanceDynamic::Create(LitM, this); PanelMat->SetScalarParameterValue(TEXT("Glow"), LitGlow);
		LanternMat = UMaterialInstanceDynamic::Create(LitM, this); LanternMat->SetScalarParameterValue(TEXT("Glow"), LanternGlow);
	}
	UMaterialInterface* Mats[NumBuckets] = { Facets, PanelMat ? (UMaterialInterface*)PanelMat : Facets, LanternMat ? (UMaterialInterface*)LanternMat : Facets, LatticeM ? LatticeM : Facets, RoomMat ? (UMaterialInterface*)RoomMat : Facets };

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Blockers = NewObject<UInstancedStaticMeshComponent>(this, TEXT("MallBlockers"));
	Blockers->SetupAttachment(RootComponent);
	Blockers->SetStaticMesh(Cube);
	Blockers->SetCollisionProfileName(TEXT("BlockAll"));
	Blockers->SetHiddenInGame(true);
	Blockers->SetVisibility(false);
	Blockers->SetCastShadow(false);
	Blockers->RegisterComponent();
	AddInstanceComponent(Blockers);
	TArray<FTransform> Solids;
	auto Solid = [&](double x0, double y0, double z0, double x1, double y1, double z1)
	{
		const FVector Mid((x0 + x1) / 2, (z0 + z1) / 2, (y0 + y1) / 2), Size(FMath::Abs(x1 - x0), FMath::Abs(z1 - z0), FMath::Abs(y1 - y0));
		Solids.Add(FTransform(FQuat::Identity, Mid * CmPerM, Size));   // (the engine's cube is a metre a side)
	};
	Solid(Mall->X0, 0, Mall->Z0, Mall->X1, Mall->F, Mall->Z1);   // the ground floor stands on the hall's own slab, at its finished height

	// the faces, by material and square
	TMap<int32, FSink> Sinks;   // key: bucket + 4 * square
	const int32 Nx = FMath::Max(1, FMath::CeilToInt((Mall->X1 - Mall->X0 + 200) / CellMetres));
	auto SinkAt = [&](int32 Bucket, const FVector3d& Mid) -> FSink&
	{
		const int32 cx = FMath::Clamp(FMath::FloorToInt((Mid.X - Mall->X0 + 100) / CellMetres), 0, Nx - 1), cz = FMath::Clamp(FMath::FloorToInt((Mid.Z - Mall->Z0 + 100) / CellMetres), 0, 999);
		return Sinks.FindOrAdd(Bucket + NumBuckets * (cx + Nx * cz));
	};
	auto Lin = [](const FVector3d& C) { return FLinearColor((float)C.X, (float)C.Y, (float)C.Z); };
	auto Bucket = [](int32 Mat) { return Mat == 1 ? Panel : Mat == 2 ? LanternGlass : (Mat == 25 || Mat == 27) ? Lattice : (Mat == 3 || Mat == 15 || Mat == 16 || Mat == 19 || Mat == 28) ? Room : Matte; };
	// what a face shows: its colour, or for the slice's coded faces what the code stands for
	auto Colour = [&](int32 Mat, const FVector3d& C) -> FLinearColor
	{
		if (Mat == 1) return C.Z > .9988 ? CoolLit : WarmLit;
		if (Mat == 2) return FLinearColor(1.f, .55f, .22f);
		if (Mat == 28) return TreadColour;
		if (Mat == 4) return DarkGlass;   // a window not lit (play.js material 4: the glass dark by day)
		return Lin(C);
	};
	auto Tri = [&](FSink& S, const FVector3d& a, const FVector3d& b, const FVector3d& c, const FVector3f& na, const FVector3f& nb, const FVector3f& nc, const FLinearColor& ca, const FLinearColor& cb, const FLinearColor& cc,
		const FVector2f* uv = nullptr, const FVector2f& uv1 = FVector2f::ZeroVector)
	{
		S.P.Append({ U(a), U(b), U(c) });
		S.N.Append({ na, nb, nc });
		S.C.Append({ ca, cb, cc });
		if (uv) S.UV0.Append({ uv[0], uv[1], uv[2] }); else S.UV0.Append({ FVector2f::ZeroVector, FVector2f::ZeroVector, FVector2f::ZeroVector });
		S.UV1.Append({ uv1, uv1, uv1 });
		Tris++;
	};
	const bool bCourt = Mall->Moving.Num() > 0;                  // Eastgate; the furniture outlet's hall has no moving stairs
	const bool bTiles = !bCourt || FloorTiles, bSteps = bCourt && StairSteps;
	auto Flat4 = [&](int32 Mat, const FVector3d& p0, const FVector3d& p1, const FVector3d& p2, const FVector3d& p3, const FVector3d* Hint, const FLinearColor& Col)
	{
		FVector3d n = FVector3d::CrossProduct(p1 - p0, p2 - p0);
		if (n.SizeSquared() < 1e-18) n = FVector3d::CrossProduct(p2 - p0, p3 - p0);
		if (Hint && n.Dot(*Hint) < 0) n = -n;
		const FVector3f nf = UN(n);
		FSink& S = SinkAt(Bucket(Mat), (p0 + p2) / 2);
		const FVector2f Kind(0.f, Mat == 16 && bTiles ? 1.f : 0.f);   // tiles: their pattern (M_MallRoom); a court mall's floor is smooth stone, its joints hidden
		Tri(S, p0, p1, p2, nf, nf, nf, Col, Col, Col, nullptr, Kind);
		if (!p2.Equals(p3, 1e-9) && !p3.Equals(p0, 1e-9)) Tri(S, p0, p2, p3, nf, nf, nf, Col, Col, Col, nullptr, Kind);
	};
	int32 Uses = 0, Parked = 0, Pieces = 0, Goods = 0;
	TArray<FVector3d> GP;
	TArray<FVector3f> GN, GC;
	TMap<FString, TArray<FTransform>> PieceAts;   // the furniture kits' pieces, by mesh (as ABrixRooms lays a home's)
	TArray<FBrixLiftCall> Lifts;
	for (const FBrixMallCall& Cl : Mall->Calls)
	{
		const TArray<double, TInlineAllocator<17>>& A = Cl.A;
		switch (Cl.Kind)
		{
		case FBrixMallCall::Quad:
		{
			const FVector3d p0(A[0], A[1], A[2]), p1(A[3], A[4], A[5]), p2(A[6], A[7], A[8]), p3(A[9], A[10], A[11]), h(A[12], A[13], A[14]);
			Flat4(Cl.Mat, p0, p1, p2, p3, A[15] > .5 ? &h : nullptr, Colour(Cl.Mat, Cl.Col));
			break;
		}
		case FBrixMallCall::QuadN:
		{
			FVector3d P[4], C[4];
			FVector3f N[4];
			for (int32 i = 0; i < 4; ++i) { P[i] = FVector3d(A[i * 3], A[i * 3 + 1], A[i * 3 + 2]); N[i] = UN(FVector3d(A[12 + i * 3], A[13 + i * 3], A[14 + i * 3])); C[i] = FVector3d(A[24 + i * 3], A[25 + i * 3], A[26 + i * 3]); }
			if (Cl.Mat == 28 && bSteps) break;   // the moving stairs' slope: drawn as steps below
			FSink& S = SinkAt(Bucket(Cl.Mat), (P[0] + P[2]) / 2);
			if (Cl.Mat == 25 || Cl.Mat == 27)   // the pierced shell: (round, down, openings + 1000 x tone) for the lattice
			{
				const int32 Kd = (int32)FMath::FloorToDouble(C[0].Z / 1000 + .01);
				const FLinearColor Tone = Lin(BrixMallShellTone(Kd));
				const FVector2f uv1((float)(C[0].Z - Kd * 1000), Cl.Mat == 27 ? 1.f : 0.f);
				const FVector2f uv[4] = { FVector2f((float)C[0].X, (float)C[0].Y), FVector2f((float)C[1].X, (float)C[1].Y), FVector2f((float)C[2].X, (float)C[2].Y), FVector2f((float)C[3].X, (float)C[3].Y) };
				const FVector2f t0[3] = { uv[0], uv[1], uv[2] }, t1[3] = { uv[0], uv[2], uv[3] };
				Tri(S, P[0], P[1], P[2], N[0], N[1], N[2], Tone, Tone, Tone, t0, uv1);
				Tri(S, P[0], P[2], P[3], N[0], N[2], N[3], Tone, Tone, Tone, t1, uv1);
				break;
			}
			FLinearColor K[4];
			for (int32 i = 0; i < 4; ++i)
			{
				if (Cl.Mat == 26)   // a ribbon wall: (band / 64, -, tone + 8 x second tone): each band in one of its two tones
				{
					const int32 k1 = (int32)FMath::FloorToDouble(C[i].Z / 8 + .01), k0 = (int32)(C[i].Z - k1 * 8), Band = (int32)FMath::FloorToDouble(C[i].X * 64);
					K[i] = Lin(BrixMallShellTone(BrixSliceHash({ FString::FromInt(Band), TEXT("band") }) < .5 ? k0 : k1));
				}
				else K[i] = Colour(Cl.Mat, C[i]);
			}
			Tri(S, P[0], P[1], P[2], N[0], N[1], N[2], K[0], K[1], K[2]);
			if (!P[2].Equals(P[3], 1e-9) && !P[3].Equals(P[0], 1e-9)) Tri(S, P[0], P[2], P[3], N[0], N[2], N[3], K[0], K[2], K[3]);
			break;
		}
		case FBrixMallCall::Box:
		{
			const double X[2] = { A[0], A[3] }, Y[2] = { A[1], A[4] }, Z[2] = { A[2], A[5] };
			for (int32 k = 0; k < 6; ++k)
			{
				FVector3d Col = Cl.Col;
				int32 Mt = Cl.Mat;
				bool bOver = false, bSkip = k == 2;
				for (const TArray<double, TInlineAllocator<5>>& Fc : Cl.Faces)
					if ((int32)Fc[0] == k) { bOver = true; bSkip = Fc.Num() < 5; if (!bSkip) { Col = FVector3d(Fc[1], Fc[2], Fc[3]); Mt = (int32)Fc[4]; } }
				if (bSkip) continue;
				const int32 (&F)[5][3] = FACES[k];
				auto Pt = [&](int32 i) { return FVector3d(X[F[i][0]], Y[F[i][1]], Z[F[i][2]]); };
				const FVector3d h(F[4][0], F[4][1], F[4][2]);
				Flat4(Mt, Pt(0), Pt(1), Pt(2), Pt(3), &h, Colour(Mt, Col));
				(void)bOver;
			}
			break;
		}
		case FBrixMallCall::YBox:   // Mesh.ybox(): obox() by M4.t(cx, y0, cz) x M4.ry(yaw), all six faces
		{
			const double cx = A[0], y0 = A[1], cz = A[2], hx = A[3] / 2, hy = A[4] / 2, hz = A[5] / 2, c = FMath::Cos(A[6]), s = FMath::Sin(A[6]);
			FVector3d q[8];
			for (int32 k = 0; k < 8; ++k)
			{
				const double x = k & 1 ? hx : -hx, y = hy + (k & 2 ? hy : -hy), z = k & 4 ? hz : -hz;
				q[k] = FVector3d(cx + c * x + s * z, y0 + y, cz - s * x + c * z);
			}
			const FVector3d o(cx, y0 + hy, cz);
			const int32 Fq[6][4] = { { 0, 4, 6, 2 }, { 1, 3, 7, 5 }, { 0, 1, 5, 4 }, { 2, 6, 7, 3 }, { 0, 2, 3, 1 }, { 4, 5, 7, 6 } };
			const FLinearColor Col = Colour(Cl.Mat, Cl.Col);
			for (const auto& f : Fq) { const FVector3d mid = (q[f[0]] + q[f[2]]) / 2 - o; Flat4(Cl.Mat, q[f[0]], q[f[1]], q[f[2]], q[f[3]], &mid, Col); }
			break;
		}
		case FBrixMallCall::OBox:   // Mesh.obox(): a box placed by the slice's matrix
		{
			FBrixM4 Mx;
			for (int32 j = 0; j < 16; ++j) Mx.M[j] = A[j];
			const double cx = A[16], cy = A[17], cz = A[18], hx = A[19] / 2, hy = A[20] / 2, hz = A[21] / 2;
			FVector3d q[8];
			for (int32 k = 0; k < 8; ++k) q[k] = Mx.Pt(cx + (k & 1 ? hx : -hx), cy + (k & 2 ? hy : -hy), cz + (k & 4 ? hz : -hz));
			const FVector3d o = Mx.Pt(cx, cy, cz);
			const int32 Fq[6][4] = { { 0, 4, 6, 2 }, { 1, 3, 7, 5 }, { 0, 1, 5, 4 }, { 2, 6, 7, 3 }, { 0, 2, 3, 1 }, { 4, 5, 7, 6 } };
			const FLinearColor Col = Colour(Cl.Mat, Cl.Col);
			for (const auto& f : Fq) { const FVector3d mid = (q[f[0]] + q[f[2]]) / 2 - o; Flat4(Cl.Mat, q[f[0]], q[f[1]], q[f[2]], q[f[3]], &mid, Col); }
			break;
		}
		case FBrixMallCall::Car:   // a car left in a car park (cars.js parked()): one of the traffic's own models in its seed's colours
		{
			const ABrixGameMode* GM = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
			if (!GM || !GM->Cars) break;
			UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this);
			PM->SetupAttachment(RootComponent);
			PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PM->RegisterComponent();
			AddInstanceComponent(PM);
			if (GM->Cars->BuildParked(PM, Cl.Text).IsZero()) { PM->DestroyComponent(); break; }
			PM->SetWorldLocationAndRotation(FVector(A[0], A[1], A[3]) * CmPerM, FRotator(0.0, FMath::RadiansToDegrees(A[2]), 0.0));
			++Parked;
			break;
		}
		case FBrixMallCall::Piece:   // a furniture kit's piece in its line: x across it, its back toward +y in Blender, which the importer turns to -y
		{
			FString Kind, Line;
			if (!Cl.Text.Split(TEXT("|"), &Kind, &Line)) break;
			const FMatrix Mx(FVector(A[3], A[4], 0), FVector(-A[5], -A[6], 0), FVector(0, 0, 1), FVector(A[0], A[2], A[1]) * CmPerM);
			PieceAts.FindOrAdd(TEXT("SM_") + Kind.Replace(TEXT(":"), TEXT("_")) + TEXT("_") + Line).Add(FTransform(Mx));
			++Pieces;
			break;
		}
		case FBrixMallCall::Good:   // a good for the home an outlet sells, as made in Blender (goods.js put(), FURN-065), in the room's light
		{
			FString Kind, Seed;
			const FBrixGoodsModel* Model = GoodsModels && Cl.Text.Split(TEXT("|"), &Kind, &Seed) ? GoodsModels->Find(Kind) : nullptr;
			if (!Model) break;
			GP.Reset(); GN.Reset(); GC.Reset();
			BrixGoodPut(*GoodsModels, *Model, A[0], A[1], A[2], A[3], Seed, (int32)A[4], GP, GN, GC);
			FSink& S = SinkAt(Room, FVector3d(A[0], A[1], A[2]));
			for (int32 k = 0; k + 2 < GP.Num(); k += 3)
				Tri(S, GP[k], GP[k + 1], GP[k + 2], UN(FVector3d(GN[k])), UN(FVector3d(GN[k + 1])), UN(FVector3d(GN[k + 2])),
					FLinearColor(GC[k].X, GC[k].Y, GC[k].Z), FLinearColor(GC[k + 1].X, GC[k + 1].Y, GC[k + 1].Z), FLinearColor(GC[k + 2].X, GC[k + 2].Y, GC[k + 2].Z));
			++Goods;
			break;
		}
		case FBrixMallCall::Solid:
			Solid(A[0], A[1], A[2], A[3], A[4], A[5]);
			break;
		case FBrixMallCall::Use:
			++Uses;
			if (Cl.Text.StartsWith(TEXT("lift|")) && A.Num() >= 7)
			{
				FBrixLiftCall L;
				L.At = FVector(A[0], A[2], A[1]); L.To = FVector(A[4], A[6], A[5]);
				L.Text = Cl.Text.Mid(5);
				L.Floor = FMath::RoundToInt(A[1] / Mall->StoreyHeight);
				Lifts.Add(L);
			}
			else if ((Cl.Text.StartsWith(TEXT("sit|")) || Cl.Text.StartsWith(TEXT("lie|"))) && A.Num() >= 4)
			{
				FSeat Se;
				Se.At = FVector(A[0], A[2], A[1]); Se.Yaw = A[3]; Se.bLie = Cl.Text.StartsWith(TEXT("lie|")); Se.Label = Cl.Text.Mid(4);
				SeatList.Add(Se);
			}
			break;
		case FBrixMallCall::Name:   // an outlet's name, lit on its sign band, facing the walk
		{
			UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
			T->SetupAttachment(RootComponent);
			T->SetText(FText::FromString(Cl.Text));
			T->SetHorizontalAlignment(EHTA_Center);
			T->SetVerticalAlignment(EVRTA_TextCenter);
			T->SetTextRenderColor(FColor(255, 236, 200));
			const float Wide = Cl.Text.Len() * NameSize * 0.55f;
			T->SetWorldSize(NameSize * FMath::Min(1.f, 5.2f / FMath::Max(Wide, 0.01f)) * CmPerM);
			T->SetWorldLocationAndRotation(FVector(A[0], A[2], A[1]) * CmPerM, FQuat(FVector::UpVector, FMath::Atan2(A[5], A[3])));
			T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			T->SetCastShadow(false);
			T->RegisterComponent();
			AddInstanceComponent(T);
			break;
		}
		default:
			break;
		}
	}
	Blockers->AddInstances(Solids, false);
	TArray<FString> NoMesh;
	// The moving stairs as their approved reference draws them (Art/Reference/Malls/MallsEastgateEscalators; Tools/blender/
	// hours_50.py mall_escalator()), laid over the slice's plain slope: the thirty steps a walker's feet already climb
	// (BrixMallStepUnder), each a dark ribbed tread with a brass nose and a darker riser; a steel floor plate with a brass
	// edge and a comb at the foot and the head; steel skirts with a line of light along the steps; a brass handrail along
	// each parapet, turned down at each end. Not here: the glass sides and newels, the timber cladding (Game/README.md, Not yet).
	if (bSteps)
		for (const FBrixMovingStair& R : Mall->Moving)
		{
			const int32 N = 30;
			const double Rise = (R.Yb - R.Ya) / N, Go = R.Lr / N, MID = .15, In = R.Hw - .3, RAIL = 1.1, Top = R.Yb - R.Ya;
			const FVector3d T(R.T.X, 0, R.T.Y), S(R.S.X, 0, R.S.Y), Up(0, 1, 0), A(R.A.X, R.Ya, R.A.Y);
			auto At = [&](double Along, double Side, double Y) { return A + T * Along + S * Side + Up * Y; };
			const FLinearColor Tread = TreadColour, Riser = TreadColour * 0.45f, Rib = TreadColour * 1.9f;
			const FVector3d Back = -T, Fwd = T;
			for (const double Sg : { -1.0, 1.0 })
			{
				const double s0 = Sg > 0 ? MID : -In, s1 = Sg > 0 ? In : -MID;
				for (int32 i = 0; i < N; ++i)
				{
					const double a0 = i * Go, a1 = (i + 1) * Go, y = (i + 1) * Rise;
					Flat4(3, At(a0, s0, y), At(a0, s1, y), At(a1, s1, y), At(a1, s0, y), &Up, Tread);
					for (int32 k = 1; k < StairRibs; ++k)   // the tread's ribs, along the way it runs
					{
						const double c = s0 + (s1 - s0) * k / StairRibs;
						Flat4(3, At(a0 + StairNose, c - .012, y + .002), At(a0 + StairNose, c + .012, y + .002), At(a1, c + .012, y + .002), At(a1, c - .012, y + .002), &Up, Rib);
					}
					Flat4(3, At(a0, s0, y + .003), At(a0, s1, y + .003), At(a0 + StairNose, s1, y + .003), At(a0 + StairNose, s0, y + .003), &Up, StairBrass);   // its brass nose
					Flat4(3, At(a0, s0, y - Rise), At(a0, s1, y - Rise), At(a0, s1, y), At(a0, s0, y), &Back, Riser);
					Flat4(3, At(a0 - .002, s0, y - .03), At(a0 - .002, s1, y - .03), At(a0 - .002, s1, y), At(a0 - .002, s0, y), &Back, StairBrass);
				}
				// the floor plates: steel, a brass edge round them, the comb's teeth where the steps slide under
				for (const double End : { 0.0, 1.0 })
				{
					const double a0 = End > 0 ? R.Lr : -(double)StairPlate, a1 = a0 + StairPlate, y = (End > 0 ? Top : 0.0) + .012, e = .04;
					Flat4(3, At(a0, s0, y), At(a0, s1, y), At(a1, s1, y), At(a1, s0, y), &Up, StairBrass);
					Flat4(3, At(a0 + e, s0 + e, y + .002), At(a0 + e, s1 - e, y + .002), At(a1 - e, s1 - e, y + .002), At(a1 - e, s0 + e, y + .002), &Up, StairPlateColour);
					const double c0 = End > 0 ? a0 + e : a1 - e - .14, c1 = c0 + .14;   // the comb, on the side the steps are
					for (double c = s0 + e + .02; c < s1 - e - .02; c += .09)
						Flat4(3, At(c0, c, y + .004), At(c0, c + .045, y + .004), At(c1, c + .045, y + .004), At(c1, c, y + .004), &Up, Riser);
				}
				// the skirts: polished steel up each side of the lane over the steps' noses, a line of light along their foot
				for (const double Side : { s0, s1 })
				{
					const double In1 = Side == s0 ? 1.0 : -1.0, q = Side + In1 * .004, l = Side + In1 * .007;
					const FVector3d Into = S * In1;
					Flat4(3, At(0, q, 0), At(R.Lr, q, Top), At(R.Lr, q, Top + .28 + Rise), At(0, q, .28 + Rise), &Into, StairSteel);
					Flat4(1, At(0, l, Rise + .05), At(R.Lr, l, Top + Rise + .05), At(R.Lr, l, Top + Rise + .08), At(0, l, Rise + .08), &Into, FLinearColor(1.f, .8f, .6f));
				}
			}
			// a brass handrail along the top of each parapet (the two outer ones and the middle one's two sides), turned down at each end
			for (const double Side : { -R.Hw + .15, -MID, MID, R.Hw - .15 })
			{
				const double w = .05, h0 = (FMath::Abs(Side) < .5 ? RAIL - .2 : RAIL) + .02, h1 = h0 + .07, r0 = Side - w, r1 = Side + w;
				Flat4(3, At(0, r0, h1), At(0, r1, h1), At(R.Lr, r1, Top + h1), At(R.Lr, r0, Top + h1), &Up, StairBrass);
				for (const double Sd : { r0, r1 })
				{
					const FVector3d Out = S * (Sd == r0 ? -1.0 : 1.0);
					Flat4(3, At(0, Sd, h0), At(R.Lr, Sd, Top + h0), At(R.Lr, Sd, Top + h1), At(0, Sd, h1), &Out, StairBrass);
				}
				Flat4(3, At(0, r0, h0 - .5), At(0, r1, h0 - .5), At(0, r1, h1), At(0, r0, h1), &Back, StairBrass);
				Flat4(3, At(R.Lr, r0, Top + h0 - .5), At(R.Lr, r1, Top + h0 - .5), At(R.Lr, r1, Top + h1), At(R.Lr, r0, Top + h1), &Fwd, StairBrass);
			}
		}

	for (TPair<FString, TArray<FTransform>>& P : PieceAts)
	{
		UStaticMesh* PM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Furniture/%s.%s"), *P.Key, *P.Key));
		if (!PM) { NoMesh.Add(P.Key); continue; }
		UInstancedStaticMeshComponent* G = NewObject<UInstancedStaticMeshComponent>(this, *P.Key);
		G->SetupAttachment(RootComponent);
		G->SetStaticMesh(PM);
		G->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // (the slice's solids are what is in the way)
		G->RegisterComponent();
		AddInstanceComponent(G);
		G->AddInstances(P.Value, false);
	}
	if (NoMesh.Num()) UE_LOG(LogTemp, Warning, TEXT("Brix Town: %s: no furniture mesh for %s (BRIX_FURNITURE_EXTRA=... Tools/unreal/import_furniture_kit.py)"), *Mall->Name, *FString::Join(NoMesh, TEXT(", ")));

	// the meshes
	int32 Meshes = 0;
	for (TPair<int32, FSink>& Sk : Sinks)
	{
		FSink& S = Sk.Value;
		if (!S.P.Num()) continue;
		const int32 B = Sk.Key % NumBuckets;
		FMeshDescription MD;
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Nm = Attr.GetVertexInstanceNormals(), Tg = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> Bs = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector4f> Vc = Attr.GetVertexInstanceColors();
		TVertexInstanceAttributesRef<FVector2f> UV = Attr.GetVertexInstanceUVs();
		UV.SetNumChannels(2);
		const FPolygonGroupID G = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[G] = TEXT("mall");
		MD.ReserveNewVertices(S.P.Num()); MD.ReserveNewVertexInstances(S.P.Num()); MD.ReserveNewTriangles(S.P.Num() / 3);
		for (int32 t = 0; t + 2 < S.P.Num(); t += 3)
		{
			FVertexInstanceID Vi[3];
			for (int32 k = 0; k < 3; ++k)
			{
				const FVector3f n = S.N[t + k];
				const FVector3f tg = (FMath::Abs(n.Z) < .9f ? FVector3f::CrossProduct(n, FVector3f::UpVector) : FVector3f::CrossProduct(n, FVector3f::ForwardVector)).GetSafeNormal();
				const FLinearColor Lc(FLinearColor(S.C[t + k].R, S.C[t + k].G, S.C[t + k].B).ToFColor(false));   // (stored as sRGB bytes and read as they are: see ABrixTownActor::FacetMesh)
				const FVertexID V = MD.CreateVertex();
				Pos[V] = S.P[t + k];
				Vi[k] = MD.CreateVertexInstance(V);
				Nm[Vi[k]] = n; Tg[Vi[k]] = tg; Bs[Vi[k]] = 1.f;
				Vc[Vi[k]] = FVector4f(Lc.R, Lc.G, Lc.B, 1.f);
				UV.Set(Vi[k], 0, S.UV0[t + k]); UV.Set(Vi[k], 1, S.UV1[t + k]);
			}
			MD.CreateTriangle(G, Vi);
		}
		UStaticMesh* M = NewObject<UStaticMesh>(this, *FString::Printf(TEXT("SM_Mall_%d"), Sk.Key));
		M->GetStaticMaterials().Add(FStaticMaterial(Mats[B], TEXT("mall"), TEXT("mall")));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bBuildSimpleCollision = false;
		Params.bFastBuild = true;
		M->BuildFromMeshDescriptions({ &MD }, Params);
		Made.Add(M);
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Mall_%d"), Sk.Key));
		C->SetupAttachment(RootComponent);
		C->SetStaticMesh(M);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (B == Panel || B == LanternGlass) C->SetCastShadow(false);
		C->RegisterComponent();
		AddInstanceComponent(C);
		++Meshes;
	}
	for (int32 i = 0; i < NearLights; ++i)
	{
		UPointLightComponent* L = nullptr;
		if (i < LightShadows)   // the nearest: down out of its shade, with shadows
		{
			USpotLightComponent* Sp = NewObject<USpotLightComponent>(this);
			Sp->OuterConeAngle = FMath::Min(LightCone, 80.f);
			Sp->InnerConeAngle = FMath::Min(LightCone, 80.f) - 6.f;
			Sp->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
			Sp->SourceRadius = 12.f;
			L = Sp;
		}
		else L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(RootComponent);
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(LightCandela);
		L->SetAttenuationRadius(LightReach * CmPerM);
		L->SetLightColor(LightColour);
		L->SetCastShadows(i < LightShadows);
		L->SetVisibility(false);
		L->RegisterComponent();
		AddInstanceComponent(L);
		Lights.Add(L);
	}
	PeopleLight = NewObject<UDirectionalLightComponent>(this);   // every property before it is registered: a directional light is the air's sun by default
	PeopleLight->SetupAttachment(RootComponent);
	PeopleLight->Mobility = EComponentMobility::Movable;
	PeopleLight->Intensity = 0.f;
	PeopleLight->LightColor = LightColour.ToFColor(true);
	PeopleLight->CastShadows = false;
	PeopleLight->bAtmosphereSunLight = false;
	PeopleLight->ForwardShadingPriority = -2;   // never the one light of the sky for clear glass and the fog (the engine warns across the picture when two could be)
	PeopleLight->bAffectsWorld = true;
	PeopleLight->LightingChannels.bChannel0 = false;
	PeopleLight->LightingChannels.bChannel1 = true;
	PeopleLight->SetVisibility(false);
	PeopleLight->RegisterComponent();
	AddInstanceComponent(PeopleLight);
	if (const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this)))
		if (ABrixRooms* R = Mode->RoomsActor()) R->AddLiftCalls(Lifts);
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %s built as the slice builds it: %d calls, %d triangles in %d meshes, %d solids, %d outlets, %d lifts' calls, %d other places to use, %d lamps, %d parked cars, %d pieces of furniture, %d goods for the home (%.2f s)"),
		*Mall->Name, Mall->Calls.Num(), Tris, Meshes, Solids.Num(), Mall->Rooms.Num(), Lifts.Num(), Uses - Lifts.Num(), Mall->Lamps.Num(), Parked, Pieces, Goods, FPlatformTime::Seconds() - T0);
}

bool ABrixMall::Contains(const FVector& WorldCm) const
{
	if (!Mall) return false;
	const FVector P = WorldCm / CmPerM;
	return P.X > Mall->X0 && P.X < Mall->X1 && P.Y > Mall->Z0 && P.Y < Mall->Z1 && P.Z < Mall->H - .1;
}

int32 ABrixMall::SeatNear(const FVector& PawnCm, double& OutMetres) const
{
	const FVector Me(PawnCm.X / CmPerM, PawnCm.Y / CmPerM, PawnCm.Z / CmPerM - 0.9);   // the player's feet
	int32 Best = INDEX_NONE;
	OutMetres = SeatReach;
	for (int32 i = 0; i < SeatList.Num(); ++i)
	{
		const FSeat& Se = SeatList[i];
		if (FMath::Abs(Se.At.X - Me.X) > 2 || FMath::Abs(Se.At.Y - Me.Y) > 2 || FMath::Abs(Se.At.Z - Me.Z) > 1.2) continue;
		const double D = FVector2D::Distance(FVector2D(Se.At), FVector2D(Me));
		if (D < OutMetres) { OutMetres = D; Best = i; }
	}
	return Best;
}

const FBrixOutletRoom* ABrixMall::OutletAt(const FVector& WorldCm) const
{
	if (!Mall) return nullptr;
	const FVector P = WorldCm / CmPerM;
	return BrixOutletAt(*Mall, P.X, P.Z, P.Y);
}

void ABrixMall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Mall) return;
	// the moving stairs carry whoever stands on them (court.js carry())
	if (ACharacter* Me = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		if (Contains(Me->GetActorLocation()))
		{
			const FVector At = Me->GetActorLocation() / CmPerM;
			const float Half = Me->GetCapsuleComponent() ? Me->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() / CmPerM : .9f;
			FVector3d P(At.X, At.Z - Half, At.Y);
			if (BrixMallCarry(*Mall, P, DeltaSeconds))   // along, and up onto the next step as it comes (the slice's pushOut lifts them)
			{
				const double Step = BrixMallStepUnder(*Mall, P), Feet = At.Z - Half;
				const bool bLift = Step > Feet + .01 && Step - Feet < .5;
				Me->SetActorLocation(FVector(P.X, P.Z, bLift ? Step + Half + .02 : At.Z) * CmPerM, !bLift, nullptr, bLift ? ETeleportType::TeleportPhysics : ETeleportType::None);
			}
		}
	if (PeopleLight)
		if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
			PeopleLevel = FMath::FInterpConstantTo(PeopleLevel, Contains(Cam->GetCameraLocation()) ? 1.f : 0.f, DeltaSeconds, 2.f);
			const float Lux = PeopleLux * PeopleLevel * FMath::Lerp(1.f, PeopleLuxDay, Mode ? FMath::SmoothStep(.5f, 1.f, Mode->Daylight) : 1.f);   // the day's light comes in only with the sun well up: at dusk people were dim again
			if (!FMath::IsNearlyEqual(PeopleLight->Intensity, Lux, .01f)) PeopleLight->SetIntensity(Lux);
			if (PeopleLight->IsVisible() != (Lux > 0.f)) PeopleLight->SetVisibility(Lux > 0.f);
			if (Lux > 0.f) PeopleLight->SetWorldRotation(FRotator(-62.f, Cam->GetCameraRotation().Yaw + 25.f, 0.f));   // from above and a little to one side: a lit side and a shaded one
		}
	LightWait -= DeltaSeconds;
	if (LightWait <= 0.f)
	{
		LightWait = LightSeconds;
		if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0)) PlaceLights(Cam->GetCameraLocation());
	}
}

// the mall's lanterns and shop lights nearest the camera on its floor, as the slice's court light lays its pools under them
void ABrixMall::PlaceLights(const FVector& Eye)
{
	const bool bIn = Contains(Eye) || (FVector2D(Eye.X / CmPerM, Eye.Y / CmPerM) - FVector2D((Mall->X0 + Mall->X1) / 2, (Mall->Z0 + Mall->Z1) / 2)).Size() < 60 + FMath::Max(Mall->X1 - Mall->X0, Mall->Z1 - Mall->Z0) / 2 && Eye.Z / CmPerM < Mall->H;
	TArray<TPair<double, int32>> Near;
	if (bIn)
	{
		const FVector E = Eye / CmPerM;
		const int32 Floor = FMath::Clamp(FMath::FloorToInt((E.Z - .2) / Mall->StoreyHeight), 0, Mall->Levels - 1);
		for (int32 i = 0; i < Mall->Lamps.Num(); ++i)
		{
			const FVector4d& L = Mall->Lamps[i];
			if (FMath::Clamp(FMath::FloorToInt((L.Y - .2) / Mall->StoreyHeight), 0, Mall->Levels - 1) != Floor) continue;
			const double D = FVector2D::Distance(FVector2D(L.X, L.Z), FVector2D(E.X, E.Y));
			if (D < 44) Near.Add({ D, i });
		}
		Near.Sort([](const TPair<double, int32>& a, const TPair<double, int32>& b) { return a.Key < b.Key; });
	}
	for (int32 i = 0; i < Lights.Num(); ++i)
	{
		if (!Near.IsValidIndex(i)) { Lights[i]->SetVisibility(false); continue; }
		const FVector4d& L = Mall->Lamps[Near[i].Value];
		Lights[i]->SetWorldLocation(FVector(L.X, L.Z, L.Y - .3) * CmPerM);
		Lights[i]->SetVisibility(true);
	}
}
