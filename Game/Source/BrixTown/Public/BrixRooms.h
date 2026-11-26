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
	// BUILD-020: the storey the building designer is looking into. That building's rooms are drawn however far the camera is,
	// and only that storey's (the storeys over it would hide it); Floor below 0: the designer is not looking inside
	void SetDesign(int32 Lot, int32 Floor);
	// the outlines round the piece or wall picked in the designer and the one under the mouse (play.js designMarks()): boxes
	// in Unreal's frame, each its own colour, lit
	void SetMarks(const TArray<FTransform>& Boxes, const TArray<FLinearColor>& Colours);
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
	// A home furnished stays so until it is KeepMetres further than NearMetres, and homes are dropped DropAtOnce
	// together (or when one comes near): furnishing is everything near cleared and made again, a long frame, and it
	// ran at every few steps along a street of houses.
	UPROPERTY(Config) float KeepMetres = 20.f;
	UPROPERTY(Config) int32 DropAtOnce = 8;
	UPROPERTY(Config) FLinearColor PartitionColour = FLinearColor(0.8f, 0.76f, 0.68f);   // homes.js PLASTER
	UPROPERTY(Config) FLinearColor NoMeshColour = FLinearColor(0.45f, 0.38f, 0.3f);   // a piece with no mesh of its own yet: a box its size
	UPROPERTY(Config) FLinearColor FrameColour = FLinearColor(0.36f, 0.24f, 0.15f);      // a doorway's frame
	UPROPERTY(Config) FLinearColor BoardsColour = FLinearColor(0.42f, 0.29f, 0.18f);     // a room's boards
	UPROPERTY(Config) FLinearColor TilesColour = FLinearColor(0.7f, 0.68f, 0.64f);       // a wet room's tiles
	UPROPERTY(Config) float DoorHeight = 2.15f;         // an inner doorway (homes.js DOOR_H)
	UPROPERTY(Config) float MinBlockHeight = 0.9f;      // a piece is in the way at least this high (homes.js one())
	UPROPERTY(Config) float HideHungWithin = 1.0f;      // a hung piece (a pendant, a wall cupboard) is not drawn with the camera this near it (m)
	// the rooms' own light (homes.js: a lamp in each room near the camera, 0.4 m under its ceiling): the rooms nearest the
	// camera in the home it is in (each light costs GPU time with Lumen: 6 cost some 15 ms on the owner's laptop)
	UPROPERTY(Config) int32 RoomLights = 3;
	// A room's lamp is the lamp of the Blender reference rooms (Tools/blender/furniture_kit_17.py and _18.py; ADR-0055):
	// their pendant of 70 W in the lantern's colour, a real lamp whose light falls off with the square of the distance,
	// shining down out of its shade (RoomLampCone: the half angle; 0 all round). CandelaPerWatt turns the references' watts
	// into the engine's candela where the sun is SunLux. By the arithmetic it is 0.075 (a Blender point light of P watts
	// lights a facing surface d away with P / (4 pi d2) W/m2, and the references' sun is 8.5 W/m2 where the game's is 8);
	// measured in the reference rooms built in Unreal (ABrixLightRoom, Tools/unreal/light_reference.py) a third of that
	// matches, as a pendant's shade takes part of its light and the engine's picture keeps more colour in what is bright.
	UPROPERTY(Config) float RoomLampWatts = 70.f;
	UPROPERTY(Config) float CandelaPerWatt = 0.026f;
	UPROPERTY(Config) FLinearColor RoomLampColour = FLinearColor(1.f, 0.6f, 0.3f);
	UPROPERTY(Config) float RoomLampCone = 80.f;
	UPROPERTY(Config) float RoomLampReach = 9.f;        // m
	UPROPERTY(Config) float RoomLampDrop = 0.7f;        // m under the ceiling, where a planned home's room has no lamp of its own to hang it from
	UPROPERTY(Config) int32 RoomLampShadows = 1;        // how many of the nearest lamps cast shadows
	// A floor with no plan (a shop, a cafe, the bank) hangs many lamps (play.js lamp()); the RoomLights nearest the camera
	// are the real ones.
	// The room's own light. The engine brings almost none of the day indoors (in the reference rooms built in Unreal,
	// with the lamps out, the walls came 7 stops under Blender's, which are lit mostly by the sky and by sunlight off the
	// floor), so a room has an even warm light of its own, as the slice's has (play.js roomLight(): its room light, weaker
	// by night): RoomFill strong where the sun is SunLux, RoomFillNight of that with the day gone, in RoomFillColour,
	// fading out at RoomFillReach of the floor's diagonal. It also stands in for a hall's lamps too far to be real ones.
	UPROPERTY(Config) float RoomFill = 0.45f;
	UPROPERTY(Config) float RoomFillNight = 0.4f;
	UPROPERTY(Config) FLinearColor RoomFillColour = FLinearColor(1.f, 0.75f, 0.48f);
	UPROPERTY(Config) float RoomFillReach = 0.7f;
	float FillNow(float Daylight) const { return RoomFill * FMath::Lerp(RoomFillNight, 1.f, FMath::Clamp(Daylight, 0.f, 1.f)); }
	// A room's lights come up and go down over LightFadeSeconds, and a lamp is not moved while it is lit: switched and
	// moved in one frame they were a jump in the picture at every doorway and as the nearest lamps of a hall changed.
	UPROPERTY(Config) float LightFadeSeconds = 0.4f;
	// a lamp as the rooms make it, for anything else that wants the same lamp (the reference room, ABrixLightRoom)
	static class UPointLightComponent* MakeLamp(AActor* Owner, USceneComponent* Root, float Watts, float ConeDeg, bool bShadows);
	UPROPERTY(Config) float MarkGlow = 2.5f;            // how brightly the designer's outlines are lit (M_Glow's Glow)
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
	int32 DesignLot = -1, DesignFloor = -1;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> MarkBoxes;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> FurnBoxes;   // the furniture of buildings with no plan: the slice's boxes
	UPROPERTY() TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Groups;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Boxes;      // walls, frames, floors: M_Greybox boxes that block
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Blocks;     // unseen: what the pieces stand in the way of
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> CamBlocks;  // unseen: what only the camera keeps out of (a pendant's shade, a wall cupboard)
	TMap<FString, TArray<float>> Hung;                                  // those pieces' boxes, x0 x1 y0 y1 z0 z1 (homes.js CAM_ONLY)
	UInstancedStaticMeshComponent* CamGroup();
	struct FHungShown { TWeakObjectPtr<UInstancedStaticMeshComponent> G; int32 Index = -1; FTransform T; FBox Box; bool bHidden = false; };
	TArray<FHungShown> HungShown;                                       // the hung pieces drawn, to leave out next to the camera
	void HideHungNearCamera();
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UMaterialInterface> Greybox;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> OwnColour;
	TSet<FString> Missing;
	TArray<FBrixLiftCall> LiftCalls;
	UPROPERTY() TObjectPtr<ABrixTownActor> TownRef;
	UPROPERTY() TArray<TObjectPtr<class UPointLightComponent>> Lights;
	UPROPERTY() TObjectPtr<class UPointLightComponent> Fill;      // the room's own light
	// The room's own light hangs at the floor's middle, so someone between it and the camera showed the camera their
	// unlit side: in a tower's hall the player was 2.2 stops under the walls. People (lighting channel 1) have the same
	// light again from the camera, PeopleFill of it, so that a body is lit like the surfaces round it (the look
	// gate's G6) and the rooms' own measured light is not changed.
	UPROPERTY() TObjectPtr<class UPointLightComponent> FillPeople;
	UPROPERTY(Config) float PeopleFill = 0.7f;
	// how much of a lamp's light the engine bounces (1 all of it): a few small bright pools bounced by a coarse
	// bounce were dark blotches on a hall's ceiling and floor
	UPROPERTY(Config) float RoomLampBounce = 1.f;
	void PlaceLights(const FVector2D& Eye, float EyeUp, float Daylight);
	void FadeLights(float DeltaSeconds);
	TArray<FVector> LampWant;      // where lamps should hang now, the nearest to the camera first
	TArray<FVector> LampAt;        // where each of Lights hangs
	TArray<float> LampLevel;       // and how far up it is, 0 to 1
	TArray<bool> LampHeld;         // whether it hangs anywhere
	float FillLevel = 0.f, FillFull = 0.f;
	bool bFillWant = false;
};
