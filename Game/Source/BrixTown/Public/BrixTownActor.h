#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixTownActor.generated.h"

struct FBrixCity;
class UInstancedStaticMeshComponent;
class UStaticMesh;

// The town, built from the city data at load time (WLD-020): no place is laid in code or by hand. This first version
// is grey-box: a box for each building, a slab for each block, simple trees. Every size and colour is in DefaultGame.ini.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixTownActor : public AActor
{
	GENERATED_BODY()

public:
	ABrixTownActor();
	void Build(const FBrixCity& City);

	UPROPERTY(Config) float StoreyHeight = 3.f;        // m a floor, as the slice builds them (play.js building())
	UPROPERTY(Config) float RoofAllowance = 0.4f;      // m above the top floor
	UPROPERTY(Config) float MinBuildingHeight = 3.6f;  // m, a one-storey building's ground floor and roof
	UPROPERTY(Config) float BlockSlabHeight = 0.12f;   // m, the block's paving above the street (slice Y_PAVE)
	UPROPERTY(Config) TArray<FString> GreenTemplates;  // block templates that are open green ground (slice GREEN_KINDS, and parks)
	UPROPERTY(Config) float TreeTrunkHeight = 3.f;
	UPROPERTY(Config) float TreeTrunkWidth = 0.35f;
	UPROPERTY(Config) float TreeCrownWidth = 4.f;
	UPROPERTY(Config) FLinearColor GroundColour = FLinearColor(0.16f, 0.15f, 0.14f);
	UPROPERTY(Config) FLinearColor BlockColour = FLinearColor(0.42f, 0.38f, 0.33f);
	UPROPERTY(Config) FLinearColor GreenColour = FLinearColor(0.22f, 0.32f, 0.14f);
	UPROPERTY(Config) FLinearColor WallColour = FLinearColor(0.72f, 0.64f, 0.52f);
	UPROPERTY(Config) FLinearColor TrunkColour = FLinearColor(0.25f, 0.17f, 0.11f);
	UPROPERTY(Config) FLinearColor CrownColour = FLinearColor(0.2f, 0.33f, 0.13f);

private:
	UInstancedStaticMeshComponent* AddLayer(const TCHAR* Name, UStaticMesh* Mesh, const FLinearColor& Colour);

	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
	UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
	UPROPERTY() TObjectPtr<UMaterialInterface> Greybox;
};
