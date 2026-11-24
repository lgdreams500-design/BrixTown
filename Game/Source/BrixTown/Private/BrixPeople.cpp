#include "BrixPeople.h"

#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixGameMode.h"
#include "BrixUnits.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

using BrixUnits::ToWorld;
using BrixUnits::CmPerM;

ABrixPeople::ABrixPeople()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Game/Kit/SM_Kit_Cylinder.SM_Kit_Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HeadMesh(TEXT("/Game/Kit/SM_Kit_Sphere.SM_Kit_Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Greybox(TEXT("/Game/Greybox/M_Greybox.M_Greybox"));
	auto Make = [&](const TCHAR* Name, UStaticMesh* Mesh)
	{
		UInstancedStaticMeshComponent* G = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		G->SetupAttachment(RootComponent);
		G->SetStaticMesh(Mesh);
		G->NumCustomDataFloats = 3;
		G->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		G->SetMobility(EComponentMobility::Movable);
		return G;
	};
	Bodies = Make(TEXT("Bodies"), BodyMesh.Object);
	Heads = Make(TEXT("Heads"), HeadMesh.Object);
	if (Greybox.Object)
	{
		// each body's colour from its custom data (M_Greybox's InstanceColour), as the town's instances
		UMaterialInstanceDynamic* Own = UMaterialInstanceDynamic::Create(Greybox.Object, this);
		Own->SetScalarParameterValue(TEXT("InstanceColour"), 1.f);
		Bodies->SetMaterial(0, Own);
		Heads->SetMaterial(0, Own);
	}
}

void ABrixPeople::Start(const FBrixCity& City, TSharedPtr<FBrixDay> InDay)
{
	Day = InDay;
	for (const FBrixBlock& B : City.Blocks) Blocks.Add(FBox2D(B.Origin, B.Origin + B.Extent));
	for (int32 i = 0; i < Day->Num(); ++i)
	{
		const FString Id = FString::FromInt(i);
		Lane.Add((float)(BrixSliceHash({ Id, TEXT("lane") }) * 2 - 1));
		Tall.Add(1.58f + 0.24f * (float)BrixSliceHash({ Id, TEXT("tall") }));   // the cast's range of heights (ART_DIRECTION 3.2)
		Wear.Add(Clothes.Num() ? (int32)(BrixSliceHash({ Id, TEXT("clothes") }) * Clothes.Num()) % Clothes.Num() : 0);
	}
	// a pool of bodies, all out of sight until the clock places someone in them
	TArray<FTransform> Hidden;
	Hidden.Init(FTransform(FQuat::Identity, FVector(0, 0, -10000), FVector(0.01f)), MaxBodies);
	Bodies->AddInstances(Hidden, false);
	Heads->AddInstances(Hidden, false);
	SlotOf.Init(-1, MaxBodies);
}

float ABrixPeople::GroundAt(const FVector2D& P) const
{
	for (const FBox2D& B : Blocks) if (B.IsInside(P)) return Slab;
	return 0.f;
}

void ABrixPeople::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!Day || !Mode || !PC) return;
	// where the player is looking from (the camera when a test camera is in use)
	FVector Eye = FVector::ZeroVector;
	if (PC->PlayerCameraManager) Eye = PC->PlayerCameraManager->GetCameraLocation();
	else if (const APawn* Me = PC->GetPawn()) Eye = Me->GetActorLocation();
	const FVector2D Here(Eye.X / CmPerM, Eye.Y / CmPerM);
	const double Minute = Mode->ClockMinute();

	// everyone out of doors within reach, nearest first
	struct FNear { int32 i; double D2; FBrixWhere W; };
	TArray<FNear> Near;
	const double Reach2 = (double)ShowDistance * ShowDistance;
	for (int32 i = 0; i < Day->Num(); ++i)
	{
		const TOptional<FBrixWhere> W = Day->Where(i, Minute);
		if (!W || W->bInside) continue;
		const double D2 = FVector2D::DistSquared(W->Pos, Here);
		if (D2 < Reach2) Near.Add({ i, D2, *W });
	}
	if (Near.Num() > MaxBodies)
	{
		Near.Sort([](const FNear& A, const FNear& B) { return A.D2 < B.D2; });
		Near.SetNum(MaxBodies);
	}

	TArray<FTransform> BodyT, HeadT;
	BodyT.Reserve(MaxBodies);
	HeadT.Reserve(MaxBodies);
	for (int32 s = 0; s < MaxBodies; ++s)
	{
		if (s >= Near.Num())
		{
			BodyT.Add(FTransform(FQuat::Identity, FVector(0, 0, -10000), FVector(0.01f)));
			HeadT.Add(BodyT.Last());
			SlotOf[s] = -1;
			continue;
		}
		const FNear& N = Near[s];
		const FBrixWhere& W = N.W;
		// a little to one side of the pavement's line, never at a door (the slice's lane, CITY-043)
		FVector2D P = W.Pos;
		if (W.bWalking) P += FVector2D(-W.Heading.Y, W.Heading.X) * Lane[N.i] * LaneWidth * FMath::Clamp(FMath::Min(W.WalkDone, W.WalkLeft) / 5.f, 0.f, 1.f);
		const float Height = Tall[N.i];
		const float Bob = W.bWalking ? 0.035f * FMath::Abs(FMath::Sin(UE_PI * W.Along / StepLength)) : 0.f;
		const float Up = GroundAt(P) + (W.Floor > 0 ? W.Floor * 3.f : 0.f) + Bob;
		const FQuat Turn(FVector::UpVector, FMath::Atan2(W.Heading.Y, W.Heading.X));
		const float BodyH = Height * 0.78f;
		BodyT.Add(FTransform(Turn, ToWorld(P, Up + BodyH / 2), FVector(0.32f, 0.42f, BodyH)));
		HeadT.Add(FTransform(Turn, ToWorld(P, Up + BodyH + Height * 0.11f), FVector(Height * 0.16f)));
		if (SlotOf[s] != N.i)
		{
			SlotOf[s] = N.i;
			const FLinearColor C = Clothes.Num() ? Clothes[Wear[N.i]] : FLinearColor::Gray;
			Bodies->SetCustomData(s, { C.R, C.G, C.B }, false);
			Heads->SetCustomData(s, { Skin.R, Skin.G, Skin.B }, false);
		}
	}
	Bodies->BatchUpdateInstancesTransforms(0, BodyT, true, true);
	Heads->BatchUpdateInstancesTransforms(0, HeadT, true, true);
	LastShown = Near.Num();
}
