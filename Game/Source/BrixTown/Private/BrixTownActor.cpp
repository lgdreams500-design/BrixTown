#include "BrixTownActor.h"

#include "BrixCity.h"
#include "BrixUnits.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABrixTownActor::ABrixTownActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Cube = CubeMesh.Object;
	Cylinder = CylinderMesh.Object;
	Sphere = SphereMesh.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GreyboxMat(TEXT("/Game/Greybox/M_Greybox.M_Greybox"));   // Tools/unreal/make_greybox_material.py
	Greybox = GreyboxMat.Object;
}

UInstancedStaticMeshComponent* ABrixTownActor::AddLayer(const TCHAR* Name, UStaticMesh* Mesh, const FLinearColor& Colour)
{
	UInstancedStaticMeshComponent* Layer = NewObject<UInstancedStaticMeshComponent>(this, Name);
	Layer->SetupAttachment(RootComponent);
	Layer->SetStaticMesh(Mesh);
	if (Greybox)
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(Greybox, this);
		Mat->SetVectorParameterValue(TEXT("Color"), Colour);
		Layer->SetMaterial(0, Mat);
	}
	Layer->SetCollisionProfileName(TEXT("BlockAll"));
	Layer->RegisterComponent();
	AddInstanceComponent(Layer);
	return Layer;
}

void ABrixTownActor::Build(const FBrixCity& City)
{
	using namespace BrixUnits;
	// The engine's basic shapes are 1 m across, centred on their middle.
	UInstancedStaticMeshComponent* Ground = AddLayer(TEXT("Ground"), Cube, GroundColour);
	UInstancedStaticMeshComponent* Blocks = AddLayer(TEXT("Blocks"), Cube, BlockColour);
	UInstancedStaticMeshComponent* Greens = AddLayer(TEXT("Greens"), Cube, GreenColour);
	UInstancedStaticMeshComponent* Walls = AddLayer(TEXT("Buildings"), Cube, WallColour);
	UInstancedStaticMeshComponent* Trunks = AddLayer(TEXT("TreeTrunks"), Cylinder, TrunkColour);
	UInstancedStaticMeshComponent* Crowns = AddLayer(TEXT("TreeCrowns"), Sphere, CrownColour);
	Trunks->SetCollisionProfileName(TEXT("BlockAll"));
	Crowns->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// The ground under the whole map, its top at the street's level (0).
	const FVector2D MapSize(City.Columns * City.Pitch, City.Rows * City.Pitch);
	const float GroundDepth = 1.f;
	Ground->AddInstance(FTransform(FQuat::Identity, ToWorld(MapSize * 0.5, -GroundDepth * 0.5f), FVector(MapSize.X, MapSize.Y, GroundDepth)));

	TArray<FTransform> BlockT, GreenT, WallT, TrunkT, CrownT;
	for (const FBrixBlock& B : City.Blocks)
	{
		const bool bGreen = GreenTemplates.Contains(B.Template);
		(bGreen ? GreenT : BlockT).Add(FTransform(FQuat::Identity, ToWorld(B.Origin + B.Extent * 0.5, BlockSlabHeight * 0.5f), FVector(B.Extent.X, B.Extent.Y, BlockSlabHeight)));
		for (const FBrixLot& L : B.Lots)
		{
			const FQuat Turn(FVector::UpVector, FMath::DegreesToRadians(L.AngleDeg));   // x east, z south -> UE X, Y: same turn
			if (L.IsOpen())
			{
				if (L.Kind == TEXT("garden")) GreenT.Add(FTransform(Turn, ToWorld(L.Centre(), BlockSlabHeight + 0.02f), FVector(L.Size().X, L.Size().Y, 0.04f)));
				continue;
			}
			const float H = FMath::Max(L.Floors * StoreyHeight + RoofAllowance, MinBuildingHeight);
			WallT.Add(FTransform(Turn, ToWorld(L.Centre(), BlockSlabHeight + H * 0.5f), FVector(L.Size().X, L.Size().Y, H)));
		}
		for (const FBrixTree& T : B.Trees)
		{
			TrunkT.Add(FTransform(FQuat::Identity, ToWorld(T.Pos, TreeTrunkHeight * 0.5f), FVector(TreeTrunkWidth, TreeTrunkWidth, TreeTrunkHeight)));
			CrownT.Add(FTransform(FQuat::Identity, ToWorld(T.Pos, TreeTrunkHeight + TreeCrownWidth * 0.4f), FVector(TreeCrownWidth)));
		}
	}
	// Scales above are in metres; the shapes are 1 m, so a scale in metres is the size in metres.
	Blocks->AddInstances(BlockT, false);
	Greens->AddInstances(GreenT, false);
	Walls->AddInstances(WallT, false);
	Trunks->AddInstances(TrunkT, false);
	Crowns->AddInstances(CrownT, false);
	UE_LOG(LogTemp, Display, TEXT("Brix Town: built %s from data: %d blocks, %d buildings, %d trees"), *City.Name, City.Blocks.Num(), WallT.Num(), TrunkT.Num());
}
