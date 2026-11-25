#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixRooms.generated.h"

struct FBrixCity;
struct FBrixInsides;
struct FBrixHome;
class ABrixTownActor;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;

// The rooms of the homes near the camera, furnished (FURN-060 to FURN-063; F-HOME-FURNISH 3; ADR-0035, ADR-0036): a planned
// row house's and a designed house's inner walls with their doorways, its rooms' floors (tiles in the wet rooms), and the
// plan's pieces at the household's furnishing level, each the furniture kit's own piece in the home's line
// (/Game/Furniture/SM_<piece>_<line>, Tools/blender/unreal_furniture_37.py). Where everything stands is BrixWorld's
// BrixHomeOf(), a port of homes.js and houses.js; as in the slice the rooms are built only near the camera (play.js
// nearRooms), rebuilt when the set of near homes changes.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixRooms : public AActor
{
	GENERATED_BODY()

public:
	ABrixRooms();
	void Start(const FBrixCity& City, const FBrixInsides& Insides, ABrixTownActor* Town, int32 Day);
	virtual void Tick(float DeltaSeconds) override;
	int32 Furnished() const { return Near.Num(); }
	int32 PiecesShown() const { return Shown; }
	int32 LampsOn() const;

	UPROPERTY(Config) float NearMetres = 30.f;          // homes within this of the camera are furnished (the slice's near rooms)
	UPROPERTY(Config) float CheckSeconds = 0.25f;
	UPROPERTY(Config) FLinearColor PartitionColour = FLinearColor(0.8f, 0.76f, 0.68f);   // homes.js PLASTER
	UPROPERTY(Config) FLinearColor FrameColour = FLinearColor(0.36f, 0.24f, 0.15f);      // a doorway's frame
	UPROPERTY(Config) FLinearColor BoardsColour = FLinearColor(0.42f, 0.29f, 0.18f);     // a room's boards
	UPROPERTY(Config) FLinearColor TilesColour = FLinearColor(0.7f, 0.68f, 0.64f);       // a wet room's tiles
	UPROPERTY(Config) float DoorHeight = 2.15f;         // an inner doorway (homes.js DOOR_H)
	UPROPERTY(Config) float MinBlockHeight = 0.9f;      // a piece is in the way at least this high (homes.js one())
	// the rooms' own light (homes.js: a lamp in each room near the camera, 0.4 m under its ceiling): the rooms nearest the
	// camera in the home it is in (each light costs GPU time with Lumen: 6 cost some 15 ms on the owner's laptop)
	UPROPERTY(Config) int32 RoomLights = 3;
	UPROPERTY(Config) float RoomLightCandela = 6.f;
	UPROPERTY(Config) float RoomLightReach = 7.f;       // m
	UPROPERTY(Config) FLinearColor RoomLightColour = FLinearColor(1.f, 0.86f, 0.68f);

private:
	struct FLotHome { int32 Lot = -1; FVector2D Centre = FVector2D::ZeroVector; float Reach = 0.f; float Floor0 = 0.f, Storey = 3.f, Ceil0 = 3.f; TSharedPtr<FBrixHome> Home; };
	TArray<FLotHome> Homes;
	TSet<int32> Near;                                   // indexes into Homes, furnished now
	float Wait = 0.f;
	int32 Shown = 0;
	void Rebuild();
	UInstancedStaticMeshComponent* MeshGroup(const FString& Name);
	UInstancedStaticMeshComponent* BoxGroup(bool bCollide);
	UPROPERTY() TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Groups;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Boxes;      // walls, frames, floors: M_Greybox boxes that block
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Blocks;     // unseen: what the pieces stand in the way of
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UMaterialInterface> Greybox;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> OwnColour;
	TSet<FString> Missing;
	UPROPERTY() TArray<TObjectPtr<class UPointLightComponent>> Lights;
	void PlaceLights(const FVector2D& Eye, float EyeUp);
};
