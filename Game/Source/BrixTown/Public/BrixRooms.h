#pragma once

#include "CoreMinimal.h"
#include "BrixHomes.h"
#include "BrixFurnish.h"
#include "BrixInside.h"
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
	// BUILD-021: a storey's inner walls drawn from a room plan (each piece's own colour, or none for the walls' plaster)
	bool SetPlan(int32 Lot, int32 Floor, const TArray<struct FBrixPartition>& Segs, const TArray<TOptional<FVector3f>>& Colours);
	bool HasRooms(int32 Lot) const;
	// FURN-051 (play.js townLayout(), showLayout()): a storey's furniture as the town laid it, as pieces that can be moved;
	// and the storey drawn from a layout (the town's pieces moved and turned, pieces of furnishing.json as the kits' meshes)
	bool TownLayout(int32 Lot, int32 Floor, TArray<struct FBrixLayoutPiece>& Out) const;
	bool SetLayout(int32 Lot, int32 Floor, const TArray<struct FBrixLayoutPiece>& Layout);   // whether the building's rooms are drawn (furnished or with walls of its own)
	int32 Furnished() const { return Near.Num(); }
	int32 PiecesShown() const { return Shown; }
	int32 LampsOn() const;
	// A tower's lift (WLD-051): the call within reach of where the player stands, and what E offers there (false: none); and
	// taking it (true if there was one).
	bool LiftOffer(const FVector& PawnCm, FString& Text, double& Metres) const;
	bool UseLift(class APawn* Pawn);
	void AddLiftCalls(const TArray<FBrixLiftCall>& More) { LiftCalls.Append(More); }   // another building's lifts (Eastgate Mall, ABrixMall)
	UPROPERTY(Config) float LiftReach = 0.9f;          // m from a call (play.js use points)

	UPROPERTY(Config) float NearMetres = 30.f;          // homes within this of the camera are furnished (the slice's near rooms)
	UPROPERTY(Config) float CheckSeconds = 0.25f;
	UPROPERTY(Config) FLinearColor PartitionColour = FLinearColor(0.8f, 0.76f, 0.68f);   // homes.js PLASTER
	UPROPERTY(Config) FLinearColor NoMeshColour = FLinearColor(0.45f, 0.38f, 0.3f);   // a piece with no mesh of its own yet: a box its size
	UPROPERTY(Config) FLinearColor FrameColour = FLinearColor(0.36f, 0.24f, 0.15f);      // a doorway's frame
	UPROPERTY(Config) FLinearColor BoardsColour = FLinearColor(0.42f, 0.29f, 0.18f);     // a room's boards
	UPROPERTY(Config) FLinearColor TilesColour = FLinearColor(0.7f, 0.68f, 0.64f);       // a wet room's tiles
	UPROPERTY(Config) float DoorHeight = 2.15f;         // an inner doorway (homes.js DOOR_H)
	UPROPERTY(Config) float MinBlockHeight = 0.9f;      // a piece is in the way at least this high (homes.js one())
	// the rooms' own light (homes.js: a lamp in each room near the camera, 0.4 m under its ceiling): the rooms nearest the
	// camera in the home it is in (each light costs GPU time with Lumen: 6 cost some 15 ms on the owner's laptop)
	UPROPERTY(Config) int32 RoomLights = 3;
	UPROPERTY(Config) float RoomLightLevel = 1.6f;      // a lamp's light on what faces it close by (the sun's is SunLux)
	UPROPERTY(Config) float RoomLightFalloff = 2.f;     // how it fades towards its reach: 1 slowly, higher sooner
	UPROPERTY(Config) float RoomLightReach = 7.f;       // m, a room's lamp
	UPROPERTY(Config) float HallLightReach = 0.7f;      // the one lamp of a floor with no plan reaches this share of the floor's diagonal
	UPROPERTY(Config) float RoomLightDrop = 0.7f;       // m under the ceiling (play.js lamp(): its shade hangs there)
	UPROPERTY(Config) FLinearColor RoomLightColour = FLinearColor(1.f, 0.86f, 0.68f);
	UPROPERTY(Config) float NightBelow = 0.35f;        // the windows are lit as the day's light falls below this (as ABrixLamps)

private:
	struct FLotHome { int32 Lot = -1; FVector2D Centre = FVector2D::ZeroVector; float Reach = 0.f; TArray<float> FloorUp, CeilUp; TArray<TOptional<FBrixRect>> Holes; TSharedPtr<FBrixHome> Home; bool bPlanned = true; TArray<struct FBrixFurnished> Furn; TMap<int32, struct FBrixFurnished> TownFurn; };   // heights of its built floors
	TArray<FLotHome> Homes;
	TSet<int32> Near;                                   // indexes into Homes, furnished now
	int32 CamHome = INDEX_NONE, CamFloor = 0;          // the home the camera is in and its floor
	float Wait = 0.f;
	int32 Shown = 0;
	void Rebuild();
	UInstancedStaticMeshComponent* MeshGroup(const FString& Name);
	UInstancedStaticMeshComponent* BoxGroup(bool bCollide);
	UInstancedStaticMeshComponent* FurnGroup();
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> FurnBoxes;   // the furniture of buildings with no plan: the slice's boxes
	UPROPERTY() TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Groups;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Boxes;      // walls, frames, floors: M_Greybox boxes that block
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Blocks;     // unseen: what the pieces stand in the way of
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UMaterialInterface> Greybox;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> OwnColour;
	TSet<FString> Missing;
	TArray<FBrixLiftCall> LiftCalls;
	UPROPERTY() TObjectPtr<ABrixTownActor> TownRef;
	UPROPERTY() TArray<TObjectPtr<class UPointLightComponent>> Lights;
	void PlaceLights(const FVector2D& Eye, float EyeUp);
};
