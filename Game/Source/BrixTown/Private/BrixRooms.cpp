#include "BrixRooms.h"

#include "BrixCity.h"
#include "BrixFurnish.h"
#include "BrixGameMode.h"
#include "BrixHomes.h"
#include "BrixFurnishPlan.h"
#include "BrixInside.h"
#include "BrixTownActor.h"
#include "BrixUnits.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

using BrixUnits::CmPerM;

namespace
{
	FTransform BoxAt(const FBrixRect& R, float Z0, float Z1)
	{
		const double Yaw = FMath::Atan2(R.Along.Y, R.Along.X);
		return FTransform(FQuat(FVector::UpVector, Yaw), FVector(R.Centre.X, R.Centre.Y, (Z0 + Z1) * 0.5) * CmPerM, FVector(R.Length, R.Width, Z1 - Z0));
	}
	// a rectangle less a hole lying the same way (homes.js without()): up to four strips
	TArray<FBrixRect> Without(const FBrixRect& R, const TOptional<FBrixRect>& Hole)
	{
		if (!Hole) return { R };
		const FVector2D Ac(-R.Along.Y, R.Along.X);
		double U0 = 1e9, U1 = -1e9, V0 = 1e9, V1 = -1e9;
		const FVector2D Ha(-Hole->Along.Y, Hole->Along.X);
		for (const double a : { -0.5, 0.5 }) for (const double b : { -0.5, 0.5 })
		{
			const FVector2D P = Hole->Centre + Hole->Along * (a * Hole->Length) + Ha * (b * Hole->Width) - R.Centre;
			const double U = FVector2D::DotProduct(P, R.Along), V = FVector2D::DotProduct(P, Ac);
			U0 = FMath::Min(U0, U); U1 = FMath::Max(U1, U); V0 = FMath::Min(V0, V); V1 = FMath::Max(V1, V);
		}
		const double L0 = -R.Length / 2, L1 = R.Length / 2, W0 = -R.Width / 2, W1 = R.Width / 2;
		if (U1 <= L0 || U0 >= L1 || V1 <= W0 || V0 >= W1) return { R };
		TArray<FBrixRect> Out;
		auto Strip = [&](double a0, double a1, double b0, double b1)
		{
			if (a1 - a0 < 0.01 || b1 - b0 < 0.01) return;
			FBrixRect Q = R;
			Q.Centre = R.Centre + R.Along * ((a0 + a1) / 2) + Ac * ((b0 + b1) / 2);
			Q.Length = (float)(a1 - a0); Q.Width = (float)(b1 - b0);
			Out.Add(Q);
		};
		Strip(L0, L1, W0, FMath::Max(W0, V0));
		Strip(L0, L1, FMath::Min(W1, V1), W1);
		Strip(L0, FMath::Max(L0, U0), FMath::Max(W0, V0), FMath::Min(W1, V1));
		Strip(FMath::Min(L1, U1), L1, FMath::Max(W0, V0), FMath::Min(W1, V1));
		return Out;
	}

	FBrixRect Part(const FBrixRect& R, float A0, float A1)   // a stretch of a rectangle along its length (from its middle)
	{
		FBrixRect Q = R;
		Q.Centre = R.Centre + R.Along * ((A0 + A1) * 0.5);
		Q.Length = A1 - A0;
		return Q;
	}
}

ABrixRooms::ABrixRooms()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GreyboxMat(TEXT("/Game/Greybox/M_Greybox.M_Greybox"));
	Cube = CubeMesh.Object;
	Greybox = GreyboxMat.Object;
}

void ABrixRooms::Start(const FBrixCity& City, const FBrixInsides& Ins, ABrixTownActor* Town, int32 Day)
{
	if (UStaticMesh* KitBox = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Kit/SM_Kit_Box.SM_Kit_Box"))) Cube = KitBox;
	TownRef = Town;
	int32 k = -1, Pieces = 0, Dropped = 0, Other = 0, BoxCount = 0, Flats = 0;
	const FBrixDoorRules DR = FBrixDoorRules::FromConfig();
	TArray<int32> ByLevel = { 0, 0, 0, 0, 0 };
	for (const FBrixBlock& B : City.Blocks)
		for (const FBrixLot& L : B.Lots)
		{
			++k;
			if (!Ins.Lots.IsValidIndex(k) || !Ins.Lots[k].IsSet() || !BrixIsHollow(L, &Ins.Lots[k].GetValue())) continue;
			if (Ins.Lots[k]->Lift && L.Floors >= 2 && Ins.Lots[k]->Stair.IsSet() && L.AngleDeg == 0.f && Town)   // a tower's lift (WLD-051)
				LiftCalls.Append(BrixLiftCalls(L, Ins.Lots[k].GetValue(), DR, Town->BlockSlabHeight + L.Floors * Town->StoreyHeight + Town->ParapetHeight + Town->CopingHeight));
			TArray<FBrixWindowSpan> Wins;
			for (int32 f = 0; Town && f < 64; ++f)
				if (const TArray<FVector4f>* S = Town->WindowSpans.Find(k * 64 + f))
					for (const FVector4f& W : *S) { FBrixWindowSpan Sp; Sp.A = FVector2D(W.X, W.Y); Sp.B = FVector2D(W.Z, W.W); Sp.Floor = f; Wins.Add(Sp); }
			TSharedPtr<FBrixHome> H = MakeShared<FBrixHome>();
			const bool bPlanned = BrixHomeOf(Ins, L, Ins.Lots[k].GetValue(), Day, Wins, *H);
			FLotHome LH;
			LH.bPlanned = bPlanned;
			LH.Lot = k;
			LH.Centre = L.Centre();
			LH.Reach = (float)L.Size().Size() * 0.5f;
			// the floors' heights as the town builds them (WLD-052); a building whose storeys are not built has its ground floor only
			const FBrixInside& In = Ins.Lots[k].GetValue();
			const int32 NFl = L.House.IsSet() ? 2 : (L.Floors >= 2 && In.Stair.IsSet() && L.AngleDeg == 0.f ? L.Floors : 1);
			for (int32 f = 0; f < NFl; ++f) { LH.FloorUp.Add(BrixFloorUp(L, f, DR)); LH.CeilUp.Add(BrixCeilingUp(L, f, DR)); }
			// the hole in each floor above the ground that the stair comes up through (as the town cuts it)
			LH.Holes.SetNum(NFl);
			FVector4f Well; float StepOff = 0.f;
			for (int32 f = 1; f < NFl; ++f)
			{
				FBrixRect Hole;
				if (L.House.IsSet())
				{
					if (!BrixHouseWell(Ins, Well, StepOff)) continue;
					const double A = FMath::DegreesToRadians(L.AngleDeg);
					const FVector2D Eu(-FMath::Cos(A), -FMath::Sin(A)), Ev(-FMath::Sin(A), FMath::Cos(A));   // houses.js: u mirrored, the lot turned
					Hole.Centre = L.Centre() + Eu * ((Well.X + Well.Y) / 2) + Ev * ((Well.Z + Well.W) / 2);
					Hole.Along = Eu; Hole.Length = Well.Y - Well.X; Hole.Width = Well.W - Well.Z;
				}
				else
				{
					const FBrixStairGeo G = BrixStairOf(*In.Stair, Ins.Rules);
					const FBrixStairGeo::FFlight Fb = G.Flight(f - 1);
					const float Head = Town ? Town->StairHeadroom : 0.3f;
					const bool bHigh = G.bStraight && FMath::Abs(G.Open - Fb.Lane.Y) < 1e-3f, bLow = G.bStraight && !bHigh;
					const float V0 = Fb.Lane.X - (bLow ? Head : 0.f), V1 = Fb.Lane.Y + (bHigh ? Head : 0.f);
					Hole.Centre = (G.P(G.ULo, V0) + G.P(G.UHi, V1)) * 0.5;
					Hole.Along = G.bAlongX ? FVector2D(1, 0) : FVector2D(0, 1); Hole.Length = G.UHi - G.ULo; Hole.Width = V1 - V0;
				}
				LH.Holes[f] = Hole;
			}
			if (!bPlanned)
			{
				// a building with no plan: each floor furnished at its places (play.js furnish(); BrixFurnish), and one room a floor
				// for its lamps
				bool bAny = false;
				LH.Furn.SetNum(NFl);
				for (int32 f = 0; f < NFl; ++f)
				{
					bAny |= BrixFurnishFloor(Ins, L, In, f, LH.Furn[f]);
					FBrixRoom Rm;
					Rm.Name = TEXT("Room"); Rm.Floor = f; Rm.bFloor = false;
					Rm.R.Centre = L.Centre(); Rm.R.Along = FVector2D(1, 0); Rm.R.Length = (float)L.Size().X - 0.7f; Rm.R.Width = (float)L.Size().Y - 0.7f;
					H->Rooms.Add(Rm);
					BoxCount += LH.Furn[f].Boxes.Num();
				}
				// a building of flats: every flat's and store's walls with their doorways (storeys.js partitions()), and each planned
				// flat furnished from its plan at its household's level (homes.js flats())
				for (int32 f = 0; f < NFl && In.Flats.Num(); ++f)
					for (const FBrixPartition& Pw : BrixFlatWalls(L, In, f, Ins.Rules))
					{
						FBrixWallSeg W;
						W.Floor = f; W.bDoorway = Pw.bDoorway; W.bPartition = true;
						W.R.Centre = (Pw.Min + Pw.Max) * 0.5;
						const FVector2D Sz = Pw.Max - Pw.Min;
						W.R.Along = Sz.X >= Sz.Y ? FVector2D(1, 0) : FVector2D(0, 1);
						W.R.Length = (float)FMath::Max(Sz.X, Sz.Y); W.R.Width = (float)FMath::Min(Sz.X, Sz.Y);
						H->Walls.Add(W);
					}
				for (int32 q = 0; q < In.Flats.Num(); ++q)
				{
					FBrixHome Fh;
					if (In.Flats[q].Interior.IsEmpty() || !BrixFlatHomeOf(Ins, In, q, Day, Wins, Fh)) continue;
					H->Pieces.Append(Fh.Pieces); H->Walls.Append(Fh.Walls); H->Rooms.Append(Fh.Rooms); H->Spots.Append(Fh.Spots);
					Pieces += Fh.Pieces.Num();
					++Flats;
					bAny = true;
				}
				if (!bAny) continue;
				++Other;
			}
			LH.Home = H;
			if (bPlanned)
			{
				Pieces += H->Pieces.Num();
				Dropped += H->Dropped;
				ByLevel[FMath::Clamp(H->Level, 0, 4)]++;
			}
			Homes.Add(LH);
		}
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %d other buildings furnished at their places (%d boxes), %d court flats from their plans"), Other, BoxCount, Flats);
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %d homes furnished from their plans (%d Bare, %d Basic, %d Homely, %d Full), %d pieces, %d left out in front of a window"),
		Homes.Num() - Other, ByLevel[1], ByLevel[2], ByLevel[3], ByLevel[4], Pieces, Dropped);
}

UInstancedStaticMeshComponent* ABrixRooms::MeshGroup(const FString& Name)
{
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Groups.Find(Name)) return *Found;
	UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Furniture/%s.%s"), *Name, *Name));
	if (!M)
	{
		if (!Missing.Contains(Name)) UE_LOG(LogTemp, Warning, TEXT("Brix Town: no furniture piece %s (run Tools/unreal/import_furniture_kit.py)"), *Name);
		Missing.Add(Name);
		Groups.Add(Name, nullptr);
		return nullptr;
	}
	UInstancedStaticMeshComponent* G = NewObject<UInstancedStaticMeshComponent>(this, *Name);
	G->SetupAttachment(RootComponent);
	G->SetStaticMesh(M);
	G->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // the unseen blocks below are what is in the way
	G->RegisterComponent();
	AddInstanceComponent(G);
	Groups.Add(Name, G);
	return G;
}

UInstancedStaticMeshComponent* ABrixRooms::FurnGroup()
{
	if (FurnBoxes) return FurnBoxes;
	FurnBoxes = NewObject<UInstancedStaticMeshComponent>(this, TEXT("FurnitureBoxes"));
	FurnBoxes->SetupAttachment(RootComponent);
	FurnBoxes->SetStaticMesh(Cube);
	FurnBoxes->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // what is in the way is the unseen blocks
	FurnBoxes->NumCustomDataFloats = 12;
	if (!OwnColour) { OwnColour = UMaterialInstanceDynamic::Create(Greybox, this); OwnColour->SetScalarParameterValue(TEXT("InstanceColour"), 1.f); }
	for (int32 i = 0; i < FurnBoxes->GetNumMaterials(); ++i) FurnBoxes->SetMaterial(i, OwnColour);
	FurnBoxes->RegisterComponent();
	AddInstanceComponent(FurnBoxes);
	return FurnBoxes;
}

UInstancedStaticMeshComponent* ABrixRooms::BoxGroup(bool bCollide)
{
	TObjectPtr<UInstancedStaticMeshComponent>& G = bCollide ? Boxes : Blocks;
	if (G) return G;
	G = NewObject<UInstancedStaticMeshComponent>(this, bCollide ? TEXT("RoomBoxes") : TEXT("PieceBlocks"));
	G->SetupAttachment(RootComponent);
	G->SetStaticMesh(Cube);
	G->SetCollisionProfileName(TEXT("BlockAll"));
	if (bCollide)
	{
		G->NumCustomDataFloats = 12;   // M_Greybox: colour, pattern, its turn (as the town's boxes)
		if (!OwnColour) { OwnColour = UMaterialInstanceDynamic::Create(Greybox, this); OwnColour->SetScalarParameterValue(TEXT("InstanceColour"), 1.f); }
		for (int32 i = 0; i < G->GetNumMaterials(); ++i) G->SetMaterial(i, OwnColour);
	}
	else { G->SetHiddenInGame(true); G->SetVisibility(false); G->SetCastShadow(false); G->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); }   // the camera looks past furniture
	G->RegisterComponent();
	AddInstanceComponent(G);
	return G;
}

void ABrixRooms::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Wait -= DeltaSeconds;
	if (Wait > 0.f) return;
	Wait = CheckSeconds;
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) return;
	const FVector2D Eye(Cam->GetCameraLocation().X / CmPerM, Cam->GetCameraLocation().Y / CmPerM);
	if (TownRef)   // the windows lit by night, as the lamps light (ABrixLamps: night below NightBelow of the day's light)
		if (const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this)))
			TownRef->SetNight(FMath::SmoothStep(0.f, 1.f, FMath::Clamp(1.f - Mode->Daylight / FMath::Max(NightBelow, .01f), 0.f, 1.f)));
	PlaceLights(Eye, (float)(Cam->GetCameraLocation().Z / CmPerM));
	TSet<int32> Want;
	for (int32 i = 0; i < Homes.Num(); ++i)
		if (FVector2D::Distance(Eye, Homes[i].Centre) - Homes[i].Reach < NearMetres || (DesignFloor >= 0 && Homes[i].Lot == DesignLot)) Want.Add(i);
	// the building the camera is in and its floor: there the floors from one below to one above it are built, elsewhere the
	// ground floor and, in a house of two, its upstairs (play.js nearRooms: the camera's floor and its neighbours)
	int32 InHome = INDEX_NONE, OnFloor = 0;
	const float EyeUp = (float)(Cam->GetCameraLocation().Z / CmPerM);
	for (const int32 i : Want)
	{
		const FLotHome& LH = Homes[i];
		if (FVector2D::Distance(Eye, LH.Centre) > LH.Reach) continue;
		bool bIn = false;
		for (const FBrixRoom& R : LH.Home->Rooms)
		{
			const FVector2D D = Eye - R.R.Centre, Ac(-R.R.Along.Y, R.R.Along.X);
			if (FMath::Abs(FVector2D::DotProduct(D, R.R.Along)) <= R.R.Length / 2 && FMath::Abs(FVector2D::DotProduct(D, Ac)) <= R.R.Width / 2) bIn = true;
		}
		if (!bIn) continue;
		InHome = i;
		for (int32 f = 0; f < LH.FloorUp.Num(); ++f) if (EyeUp > LH.FloorUp[f] - 0.1f) OnFloor = f;
	}
	if (Want.Num() == Near.Num() && Want.Includes(Near) && InHome == CamHome && OnFloor == CamFloor) return;
	Near = MoveTemp(Want);
	CamHome = InHome;
	CamFloor = OnFloor;
	Rebuild();
}

bool ABrixRooms::SetPlan(int32 Lot, int32 Floor, const TArray<FBrixPartition>& Segs, const TArray<TOptional<FVector3f>>& Colours)
{
	FLotHome* LH = Homes.FindByPredicate([Lot](const FLotHome& X) { return X.Lot == Lot; });
	if (!LH || !LH->Home) return false;
	LH->Home->Walls.RemoveAll([Floor](const FBrixWallSeg& W) { return W.Floor == Floor && W.bPartition; });
	for (int32 i = 0; i < Segs.Num(); ++i)
	{
		const FBrixPartition& Pw = Segs[i];
		FBrixWallSeg W;
		W.Floor = Floor; W.bDoorway = Pw.bDoorway; W.bPartition = true;
		if (Colours.IsValidIndex(i)) W.Colour = Colours[i];
		W.R.Centre = (Pw.Min + Pw.Max) * 0.5;
		const FVector2D Sz = Pw.Max - Pw.Min;
		W.R.Along = Sz.X >= Sz.Y ? FVector2D(1, 0) : FVector2D(0, 1);
		W.R.Length = (float)FMath::Max(Sz.X, Sz.Y); W.R.Width = (float)FMath::Min(Sz.X, Sz.Y);
		LH->Home->Walls.Add(W);
	}
	Near.Reset();   // drawn again at the next look round
	Wait = 0.f;
	return true;
}

bool ABrixRooms::TownLayout(int32 Lot, int32 Floor, TArray<FBrixLayoutPiece>& Out) const
{
	const FLotHome* LH = Homes.FindByPredicate([Lot](const FLotHome& X) { return X.Lot == Lot; });
	if (!LH || !LH->Furn.IsValidIndex(Floor)) return false;
	const FBrixFurnished* Orig = LH->TownFurn.Find(Floor);
	Out = BrixFurnishPlan::TownPieces(Orig ? *Orig : LH->Furn[Floor]).Pieces;
	return true;
}

bool ABrixRooms::SetLayout(int32 Lot, int32 Floor, const TArray<FBrixLayoutPiece>& Layout)
{
	FLotHome* LH = Homes.FindByPredicate([Lot](const FLotHome& X) { return X.Lot == Lot; });
	if (!LH || !LH->Home || !LH->Furn.IsValidIndex(Floor)) return false;
	if (!LH->TownFurn.Contains(Floor)) LH->TownFurn.Add(Floor, LH->Furn[Floor]);
	const FBrixFurnished& Orig = LH->TownFurn[Floor];
	const BrixFurnishPlan::FTown Town = BrixFurnishPlan::TownPieces(Orig);
	FBrixFurnished& Now = LH->Furn[Floor];
	BrixFurnishPlan::LayoutBoxes(Orig, Town, Layout, Now.Boxes);
	Now.Solids.Reset();
	LH->Home->Pieces.RemoveAll([Floor](const FBrixFurniturePiece& P) { return P.bLayout && P.Floor == Floor; });
	for (const FBrixLayoutPiece& P : Layout)
	{
		const bool bTown = P.Kind == TEXT("town") && Town.Pieces.ContainsByPredicate([&](const FBrixLayoutPiece& X) { return X.Id == P.Id; });
		if (!P.bRug)   // in the way: its footprint (homes.js one())
		{
			FBrixFurnSolid So;
			So.Min = FVector2D(1e9, 1e9); So.Max = FVector2D(-1e9, -1e9);
			for (const FVector2D& C : BrixFurnishPlan::Corners(P)) { So.Min = FVector2D::Min(So.Min, C); So.Max = FVector2D::Max(So.Max, C); }
			So.Y0 = 0.f; So.Y1 = (float)P.H;
			Now.Solids.Add(So);
		}
		if (bTown) continue;
		const double Y = FMath::DegreesToRadians(P.Yaw), c = FMath::Cos(Y), s = FMath::Sin(Y);
		FBrixFurniturePiece K;
		K.Kind = P.Kind == TEXT("town") ? FString(TEXT("town")) : P.Kind;
		K.Line = P.Line ? *P.Line : FString(TEXT("plain"));
		K.Floor = Floor;
		K.At = FVector2D(P.X, P.Z);
		K.A = FVector2D(c, -s); K.B = FVector2D(s, c);
		K.Size = FVector3f(P.W, P.D, P.H);
		K.bLayout = true;
		LH->Home->Pieces.Add(K);
	}
	Near.Reset();   // drawn again at the next look round
	Wait = 0.f;
	return true;
}

void ABrixRooms::SetDesign(int32 Lot, int32 Floor)
{
	if (Floor < 0) Lot = -1;
	if (Lot == DesignLot && Floor == DesignFloor) return;
	DesignLot = Lot;
	DesignFloor = Floor;
	Near.Reset();   // drawn again at the next look round
	Wait = 0.f;
}

void ABrixRooms::SetMarks(const TArray<FTransform>& At, const TArray<FLinearColor>& Colours)
{
	if (!MarkBoxes)
	{
		if (!At.Num()) return;
		MarkBoxes = NewObject<UInstancedStaticMeshComponent>(this, TEXT("DesignMarks"));
		MarkBoxes->SetupAttachment(RootComponent);
		MarkBoxes->SetStaticMesh(Cube);
		MarkBoxes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MarkBoxes->SetCastShadow(false);
		MarkBoxes->NumCustomDataFloats = 12;
		UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Greybox/M_Glow.M_Glow"));   // lit in its own colour, as a lamp's glass
		if (UMaterialInstanceDynamic* Lit = Glow ? UMaterialInstanceDynamic::Create(Glow, this) : nullptr)
		{
			Lit->SetScalarParameterValue(TEXT("Glow"), MarkGlow);
			for (int32 i = 0; i < MarkBoxes->GetNumMaterials(); ++i) MarkBoxes->SetMaterial(i, Lit);
		}
		else
		{
			if (!OwnColour) { OwnColour = UMaterialInstanceDynamic::Create(Greybox, this); OwnColour->SetScalarParameterValue(TEXT("InstanceColour"), 1.f); }
			for (int32 i = 0; i < MarkBoxes->GetNumMaterials(); ++i) MarkBoxes->SetMaterial(i, OwnColour);
		}
		MarkBoxes->RegisterComponent();
		AddInstanceComponent(MarkBoxes);
	}
	MarkBoxes->ClearInstances();
	MarkBoxes->AddInstances(At, false);
	for (int32 n = 0; n < At.Num(); ++n)
	{
		const FLinearColor C = Colours.IsValidIndex(n) ? Colours[n] : FLinearColor::White;
		MarkBoxes->SetCustomData(n, { C.R, C.G, C.B, -1, 0, 0, 0, 0, 0, 0, 0, 0 }, false);
	}
	MarkBoxes->MarkRenderStateDirty();
}

bool ABrixRooms::HasRooms(int32 Lot) const { return Homes.ContainsByPredicate([Lot](const FLotHome& X) { return X.Lot == Lot; }); }

void ABrixRooms::Rebuild()
{
	for (TPair<FString, TObjectPtr<UInstancedStaticMeshComponent>>& G : Groups) if (G.Value) G.Value->ClearInstances();
	UInstancedStaticMeshComponent* Box = BoxGroup(true);
	UInstancedStaticMeshComponent* Block = BoxGroup(false);
	Box->ClearInstances();
	Block->ClearInstances();
	ABrixTownActor* TownDefaults = GetMutableDefault<ABrixTownActor>();
	TMap<UInstancedStaticMeshComponent*, TArray<FTransform>> Add;
	TArray<FTransform> BoxAts, BlockAts;
	TArray<TArray<float>> BoxData, FurnData;
	TArray<FTransform> FurnAts;
	auto AddBox = [&](const FTransform& T, const FLinearColor& C, const FVector4& A, const FVector4& B)
	{
		BoxAts.Add(T);
		BoxData.Add({ C.R, C.G, C.B, (float)A.X, (float)A.Y, (float)A.Z, (float)A.W, (float)B.X, (float)B.Y, (float)B.Z, (float)B.W, FMath::DegreesToRadians((float)T.GetRotation().Rotator().Yaw) });
	};
	const FVector4 Plaster(-1, 0, 0, 0), None(0, 0, 0, 0);
	Shown = 0;
	for (const int32 i : Near)
	{
		const FLotHome& LH = Homes[i];
		const FBrixHome& H = *LH.Home;
		const bool bHere = i == CamHome;
		auto Up = [&](int32 f)   // a built floor near the camera, or -1
		{
			if (!LH.FloorUp.IsValidIndex(f)) return -1.f;
			if (DesignFloor >= 0 && LH.Lot == DesignLot) return f == DesignFloor ? LH.FloorUp[f] : -1.f;   // the designer's storey, cut open
			if (LH.FloorUp.Num() > 2 && (bHere ? FMath::Abs(f - CamFloor) > 1 : f > 0)) return -1.f;
			return LH.FloorUp[f];
		};
		auto Ceil = [&](int32 f) { return LH.CeilUp.IsValidIndex(f) ? LH.CeilUp[f] : -1.f; };
		for (int32 f = 0; f < LH.Furn.Num(); ++f)   // a building furnished at its places: the slice's boxes
		{
			if (Up(f) < 0.f) continue;
			for (const FBrixFurnBox& Fb : LH.Furn[f].Boxes)
			{
				// the slice turns a box by M4.ry(yaw): its x to (cos, -sin) on the plan, which is Unreal's yaw -yaw
				const FTransform T(FQuat(FVector::UpVector, -Fb.Yaw), FVector(Fb.At.X, Fb.At.Y, Up(f) + Fb.Y0 + Fb.Size.Y / 2) * CmPerM, FVector(Fb.Size.X, Fb.Size.Z, Fb.Size.Y));
				FurnAts.Add(T);
				FurnData.Add({ Fb.Colour.X, Fb.Colour.Y, Fb.Colour.Z, -1, 0, 0, 0, 0, 0, 0, 0, -Fb.Yaw });
				++Shown;
			}
			for (const FBrixFurnSolid& So : LH.Furn[f].Solids)
			{
				FBrixRect Q;
				Q.Centre = (So.Min + So.Max) * 0.5; Q.Along = FVector2D(1, 0); Q.Length = (float)(So.Max.X - So.Min.X); Q.Width = (float)(So.Max.Y - So.Min.Y);
				BlockAts.Add(BoxAt(Q, Up(f) + So.Y0, Up(f) + FMath::Max(So.Y1, MinBlockHeight)));
			}
		}
		for (const FBrixRoom& R : H.Rooms)
		{
			if (Up(R.Floor) < 0.f || !R.bFloor) continue;   // a storey not built; a building's whole storey keeps its own floor
			const float Floor0 = Up(R.Floor);
			const bool bWet = R.bWet;
			for (const FBrixRect& Q : Without(R.R, LH.Holes.IsValidIndex(R.Floor) ? LH.Holes[R.Floor] : TOptional<FBrixRect>()))   // open over the stair
				AddBox(BoxAt(Q, Floor0 - 0.02f, Floor0 + 0.006f), bWet ? TilesColour : BoardsColour,
					bWet ? TownDefaults->FloorFlagsA : TownDefaults->FloorBoardsA, bWet ? TownDefaults->FloorFlagsB : TownDefaults->FloorBoardsB);
		}
		for (const FBrixWallSeg& W : H.Walls)
		{
			if (Up(W.Floor) < 0.f) continue;
			const float Floor0 = Up(W.Floor), Ceil0 = Ceil(W.Floor);
			const FLinearColor Wc = W.Colour ? FLinearColor(W.Colour->X, W.Colour->Y, W.Colour->Z) : PartitionColour;
			if (!W.bDoorway) { AddBox(BoxAt(W.R, Floor0, Ceil0), Wc, Plaster, None); continue; }
			AddBox(BoxAt(W.R, Floor0 + DoorHeight, Ceil0), Wc, Plaster, None);
			for (float E : { -W.R.Length / 2, W.R.Length / 2 })   // the doorway's frame: two posts and a lintel
			{
				FBrixRect Post = Part(W.R, E - 0.05f, E + 0.05f);
				Post.Width += 0.04f;
				AddBox(BoxAt(Post, Floor0, Floor0 + DoorHeight + 0.08f), FrameColour, None, None);
			}
			FBrixRect Lintel = Part(W.R, -W.R.Length / 2 - 0.05f, W.R.Length / 2 + 0.05f);
			Lintel.Width += 0.04f;
			AddBox(BoxAt(Lintel, Floor0 + DoorHeight, Floor0 + DoorHeight + 0.08f), FrameColour, None, None);
		}
		for (const FBrixFurniturePiece& P : H.Pieces)
		{
			if (Up(P.Floor) < 0.f) continue;
			const float Floor0 = Up(P.Floor);
			UInstancedStaticMeshComponent* G = P.Kind == TEXT("town") ? nullptr : MeshGroup(TEXT("SM_") + P.Kind.Replace(TEXT(":"), TEXT("_")) + TEXT("_") + P.Line);
			if (!G && P.bLayout)   // a piece placed in the designer with no mesh of its own yet (a mall good, a town piece from elsewhere): a box its size
			{
				const float Yw = FMath::Atan2(-P.A.Y, P.A.X);
				FurnAts.Add(FTransform(FQuat(FVector::UpVector, -Yw), FVector(P.At.X, P.At.Y, Floor0 + P.Size.Z / 2) * CmPerM, FVector(P.Size.X, P.Size.Y, P.Size.Z)));
				FurnData.Add({ NoMeshColour.R, NoMeshColour.G, NoMeshColour.B, -1, 0, 0, 0, 0, 0, 0, 0, -Yw });
				++Shown;
				continue;
			}
			if (!G) continue;
			// the kit's piece: x across it, its back toward +y in Blender, which the importer turns to -y
			const FVector Org(P.At.X * CmPerM, P.At.Y * CmPerM, (Floor0 + P.Lift) * CmPerM);
			const FMatrix M(FVector(P.A.X, P.A.Y, 0), FVector(-P.B.X, -P.B.Y, 0), FVector(0, 0, 1), Org);
			Add.FindOrAdd(G).Add(FTransform(M));
			++Shown;
			if (P.Lift == 0.f && !P.Size.IsZero())   // homes.js one(): its footprint, a little in, at least 0.9 m high
			{
				FBrixRect Foot;
				Foot.Centre = P.At; Foot.Along = P.A; Foot.Length = FMath::Max(0.05f, P.Size.X - 0.1f); Foot.Width = FMath::Max(0.05f, P.Size.Y - 0.1f);
				BlockAts.Add(BoxAt(Foot, Floor0, Floor0 + FMath::Max(P.Size.Z, MinBlockHeight)));
			}
		}
	}
	for (TPair<UInstancedStaticMeshComponent*, TArray<FTransform>>& A : Add) A.Key->AddInstances(A.Value, false);
	Box->AddInstances(BoxAts, false);
	for (int32 n = 0; n < BoxData.Num(); ++n) Box->SetCustomData(n, BoxData[n], false);
	Box->MarkRenderStateDirty();
	Block->AddInstances(BlockAts, false);
	UInstancedStaticMeshComponent* Furn = FurnGroup();
	Furn->ClearInstances();
	Furn->AddInstances(FurnAts, false);
	for (int32 n = 0; n < FurnData.Num(); ++n) Furn->SetCustomData(n, FurnData[n], false);
	Furn->MarkRenderStateDirty();
}

void ABrixRooms::PlaceLights(const FVector2D& Eye, float EyeUp)
{
	if (Lights.Num() != RoomLights)
	{
		for (UPointLightComponent* L : Lights) if (L) L->DestroyComponent();
		Lights.Reset();
		for (int32 n = 0; n < RoomLights; ++n)
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(this);
			L->SetupAttachment(RootComponent);
			L->SetIntensityUnits(ELightUnits::Candelas);
			L->SetIntensity(RoomLightCandela);
			L->SetAttenuationRadius(RoomLightReach * CmPerM);
			L->SetLightColor(RoomLightColour);
			L->SetCastShadows(false);
			L->SetSourceRadius(30.f);
			L->SetVisibility(false);
			L->RegisterComponent();
			AddInstanceComponent(L);
			Lights.Add(L);
		}
	}
	// the rooms of the home the camera is in, nearest it first (out of doors, none: they cost GPU time and are not seen)
	auto InRect = [](const FBrixRect& R, const FVector2D& P)
	{
		const FVector2D D = P - R.Centre, Ac(-R.Along.Y, R.Along.X);
		return FMath::Abs(FVector2D::DotProduct(D, R.Along)) <= R.Length / 2 && FMath::Abs(FVector2D::DotProduct(D, Ac)) <= R.Width / 2;
	};
	int32 In = INDEX_NONE, OnFloor = 0;
	for (const int32 i : Near)
		for (const FBrixRoom& R : Homes[i].Home->Rooms)
			if (Homes[i].FloorUp.IsValidIndex(R.Floor) && EyeUp > Homes[i].FloorUp[R.Floor] - 0.1f && EyeUp < Homes[i].CeilUp[R.Floor] && InRect(R.R, Eye)) { In = i; OnFloor = R.Floor; }
	TArray<TPair<double, FVector>> Rooms;
	if (In != INDEX_NONE)
		for (const FBrixRoom& R : Homes[In].Home->Rooms)
			if (R.Floor == OnFloor) Rooms.Add({ FVector2D::Distance(Eye, R.R.Centre), FVector(R.R.Centre.X * CmPerM, R.R.Centre.Y * CmPerM, (Homes[In].CeilUp[OnFloor] - 0.4f) * CmPerM) });
	Rooms.Sort([](const TPair<double, FVector>& A, const TPair<double, FVector>& B) { return A.Key < B.Key; });
	for (int32 n = 0; n < Lights.Num(); ++n)
	{
		const bool bOn = Rooms.IsValidIndex(n);
		if (bOn) Lights[n]->SetWorldLocation(Rooms[n].Value);
		if (Lights[n]->IsVisible() != bOn) Lights[n]->SetVisibility(bOn);
	}
}

int32 ABrixRooms::LampsOn() const
{
	int32 N = 0;
	for (const UPointLightComponent* L : Lights) if (L && L->IsVisible()) ++N;
	return N;
}

bool ABrixRooms::LiftOffer(const FVector& PawnCm, FString& Text, double& Metres) const
{
	const FVector Me(PawnCm.X / CmPerM, PawnCm.Y / CmPerM, PawnCm.Z / CmPerM - 0.85);   // the feet (the capsule's half height)
	const FBrixLiftCall* Best = nullptr;
	double BestD = LiftReach;
	for (const FBrixLiftCall& C : LiftCalls)
	{
		if (FMath::Abs(Me.Z - C.At.Z) > 1.0) continue;
		const double D = FVector2D::Distance(FVector2D(Me.X, Me.Y), FVector2D(C.At.X, C.At.Y));
		if (D < BestD) { BestD = D; Best = &C; }
	}
	if (!Best) return false;
	Text = Best->Text;
	Metres = BestD;
	return true;
}

bool ABrixRooms::UseLift(APawn* Pawn)
{
	if (!Pawn) return false;
	FString Text;
	double D = 0;
	if (!LiftOffer(Pawn->GetActorLocation(), Text, D)) return false;
	const FVector Me(Pawn->GetActorLocation().X / CmPerM, Pawn->GetActorLocation().Y / CmPerM, Pawn->GetActorLocation().Z / CmPerM - 0.85);
	for (const FBrixLiftCall& C : LiftCalls)
		if (C.Text == Text && FMath::Abs(Me.Z - C.At.Z) <= 1.0 && FVector2D::Distance(FVector2D(Me.X, Me.Y), FVector2D(C.At.X, C.At.Y)) <= D + 1e-6)
		{
			Pawn->SetActorLocation(FVector(C.To.X, C.To.Y, C.To.Z + 0.95) * CmPerM, false, nullptr, ETeleportType::TeleportPhysics);
			UE_LOG(LogTemp, Display, TEXT("Brix Town: %s: now %.2f m up"), *Text, C.To.Z);
			return true;
		}
	return false;
}
