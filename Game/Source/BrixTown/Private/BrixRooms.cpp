#include "BrixRooms.h"

#include "BrixCity.h"
#include "BrixHomes.h"
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
	int32 k = -1, Pieces = 0, Dropped = 0;
	TArray<int32> ByLevel = { 0, 0, 0, 0, 0 };
	for (const FBrixBlock& B : City.Blocks)
		for (const FBrixLot& L : B.Lots)
		{
			++k;
			if (!Ins.Lots.IsValidIndex(k) || !Ins.Lots[k].IsSet() || !BrixIsHollow(L, &Ins.Lots[k].GetValue())) continue;
			TArray<FBrixWindowSpan> Wins;
			if (Town) if (const TArray<FVector4f>* S = Town->WindowSpans.Find(k))
				for (const FVector4f& W : *S) { FBrixWindowSpan Sp; Sp.A = FVector2D(W.X, W.Y); Sp.B = FVector2D(W.Z, W.W); Wins.Add(Sp); }
			TSharedPtr<FBrixHome> H = MakeShared<FBrixHome>();
			if (!BrixHomeOf(Ins, L, Ins.Lots[k].GetValue(), Day, Wins, *H)) continue;
			FLotHome LH;
			LH.Lot = k;
			LH.Centre = L.Centre();
			LH.Reach = (float)L.Size().Size() * 0.5f;
			const bool bHouse = L.House.IsSet();
			const float Base = Town ? Town->BlockSlabHeight : 0.12f;
			LH.Floor0 = Base + (Town ? (bHouse ? Town->HousePlinth : Town->InsideFloor) : 0.08f);
			LH.Storey = Town ? (bHouse ? Town->HouseStorey : Town->StoreyHeight) : 3.f;
			LH.Ceil0 = Base + (Town ? (bHouse ? Town->HousePlinth + Town->HouseStorey : Town->StoreyHeight) - Town->CeilingDrop : 2.88f);
			LH.Home = H;
			Pieces += H->Pieces.Num();
			Dropped += H->Dropped;
			ByLevel[FMath::Clamp(H->Level, 0, 4)]++;
			Homes.Add(LH);
		}
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %d homes furnished from their plans (%d Bare, %d Basic, %d Homely, %d Full), %d pieces, %d left out in front of a window"),
		Homes.Num(), ByLevel[1], ByLevel[2], ByLevel[3], ByLevel[4], Pieces, Dropped);
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
	PlaceLights(Eye, (float)(Cam->GetCameraLocation().Z / CmPerM));
	TSet<int32> Want;
	for (int32 i = 0; i < Homes.Num(); ++i)
		if (FVector2D::Distance(Eye, Homes[i].Centre) - Homes[i].Reach < NearMetres) Want.Add(i);
	if (Want.Num() == Near.Num() && Want.Includes(Near)) return;
	Near = MoveTemp(Want);
	Rebuild();
}

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
	TArray<TArray<float>> BoxData;
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
		// only the ground floor is built inside yet (the storeys above come with their stairs)
		for (const FBrixRoom& R : H.Rooms)
		{
			if (R.Floor != 0) continue;
			const bool bWet = R.bWet;
			AddBox(BoxAt(R.R, LH.Floor0 - 0.02f, LH.Floor0 + 0.006f), bWet ? TilesColour : BoardsColour,
				bWet ? TownDefaults->FloorFlagsA : TownDefaults->FloorBoardsA, bWet ? TownDefaults->FloorFlagsB : TownDefaults->FloorBoardsB);
		}
		for (const FBrixWallSeg& W : H.Walls)
		{
			if (W.Floor != 0) continue;
			if (!W.bDoorway) { AddBox(BoxAt(W.R, LH.Floor0, LH.Ceil0), PartitionColour, Plaster, None); continue; }
			AddBox(BoxAt(W.R, LH.Floor0 + DoorHeight, LH.Ceil0), PartitionColour, Plaster, None);
			for (float E : { -W.R.Length / 2, W.R.Length / 2 })   // the doorway's frame: two posts and a lintel
			{
				FBrixRect Post = Part(W.R, E - 0.05f, E + 0.05f);
				Post.Width += 0.04f;
				AddBox(BoxAt(Post, LH.Floor0, LH.Floor0 + DoorHeight + 0.08f), FrameColour, None, None);
			}
			FBrixRect Lintel = Part(W.R, -W.R.Length / 2 - 0.05f, W.R.Length / 2 + 0.05f);
			Lintel.Width += 0.04f;
			AddBox(BoxAt(Lintel, LH.Floor0 + DoorHeight, LH.Floor0 + DoorHeight + 0.08f), FrameColour, None, None);
		}
		for (const FBrixFurniturePiece& P : H.Pieces)
		{
			if (P.Floor != 0) continue;
			UInstancedStaticMeshComponent* G = MeshGroup(TEXT("SM_") + P.Kind.Replace(TEXT(":"), TEXT("_")) + TEXT("_") + P.Line);
			if (!G) continue;
			// the kit's piece: x across it, its back toward +y in Blender, which the importer turns to -y
			const FVector Org(P.At.X * CmPerM, P.At.Y * CmPerM, (LH.Floor0 + P.Lift) * CmPerM);
			const FMatrix M(FVector(P.A.X, P.A.Y, 0), FVector(-P.B.X, -P.B.Y, 0), FVector(0, 0, 1), Org);
			Add.FindOrAdd(G).Add(FTransform(M));
			++Shown;
			if (P.Lift == 0.f && !P.Size.IsZero())   // homes.js one(): its footprint, a little in, at least 0.9 m high
			{
				FBrixRect Foot;
				Foot.Centre = P.At; Foot.Along = P.A; Foot.Length = FMath::Max(0.05f, P.Size.X - 0.1f); Foot.Width = FMath::Max(0.05f, P.Size.Y - 0.1f);
				BlockAts.Add(BoxAt(Foot, LH.Floor0, LH.Floor0 + FMath::Max(P.Size.Z, MinBlockHeight)));
			}
		}
	}
	for (TPair<UInstancedStaticMeshComponent*, TArray<FTransform>>& A : Add) A.Key->AddInstances(A.Value, false);
	Box->AddInstances(BoxAts, false);
	for (int32 n = 0; n < BoxData.Num(); ++n) Box->SetCustomData(n, BoxData[n], false);
	Box->MarkRenderStateDirty();
	Block->AddInstances(BlockAts, false);
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
	int32 In = INDEX_NONE;
	for (const int32 i : Near)
		for (const FBrixRoom& R : Homes[i].Home->Rooms)
			if (R.Floor == 0 && EyeUp < Homes[i].Ceil0 && InRect(R.R, Eye)) In = i;
	TArray<TPair<double, FVector>> Rooms;
	if (In != INDEX_NONE)
		for (const FBrixRoom& R : Homes[In].Home->Rooms)
			if (R.Floor == 0) Rooms.Add({ FVector2D::Distance(Eye, R.R.Centre), FVector(R.R.Centre.X * CmPerM, R.R.Centre.Y * CmPerM, (Homes[In].Ceil0 - 0.4f) * CmPerM) });
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
