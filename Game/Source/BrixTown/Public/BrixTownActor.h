#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixTownActor.generated.h"

struct FBrixCity;
struct FBrixLot;
struct FBrixBlock;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;

// The colour of one material slot of the building kit's pieces (slot names as in Tools/blender/unreal_kit_36.py).
USTRUCT()
struct FBrixSlotColour
{
	GENERATED_BODY()
	UPROPERTY(Config) FName Slot;
	UPROPERTY(Config) FLinearColor Colour = FLinearColor::White;
};

// The town, built from the city data at load time (WLD-020): no place is laid in code or by hand. Each building follows
// the building kit (Tools/blender/building_kit_07.py): a core set back by the wall's depth, front walls laid between real
// openings, the kit's windows, doors and shutters (/Game/Kit, from Tools/blender/unreal_kit_36.py) in bays along every
// facade, a stone ground storey in the old districts, a tiled gable roof or a parapet. Every number is in DefaultGame.ini.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixTownActor : public AActor
{
	GENERATED_BODY()

public:
	ABrixTownActor();
	void Build(const FBrixCity& City);

	UPROPERTY(Config) float StoreyHeight = 3.f;         // m a floor (WORLD_MAP; kit floor_h)
	UPROPERTY(Config) float BlockSlabHeight = 0.12f;    // m, the block's paving above the street (slice Y_PAVE)
	UPROPERTY(Config) TArray<FString> GreenTemplates;   // block templates that are open green ground
	UPROPERTY(Config) float WallThickness = 0.32f;      // kit wall_t: the depth of every reveal
	UPROPERTY(Config) float Bay = 3.2f;                 // kit bay: target width of one window bay
	UPROPERTY(Config) float MinFacadeForOpenings = 2.f; // m; a narrower facade is left plain
	UPROPERTY(Config) float GroundSill = 0.95f;         // kit win_sill
	UPROPERTY(Config) float UpperSill = 0.8f;           // kit up_sill, above each floor
	UPROPERTY(Config) float CafeSill = 0.55f;
	UPROPERTY(Config) float CivicSill = 0.9f;
	UPROPERTY(Config) TArray<FString> StoneBaseDistricts; // districts whose ground storey is stone (kit base_h)
	UPROPERTY(Config) float StoneBaseHeight = 3.4f;
	UPROPERTY(Config) float BandHeight = 0.25f;         // the stone string course on top of a stone ground storey
	UPROPERTY(Config) float BandDepth = 0.12f;
	UPROPERTY(Config) float RoofPitch = 24.f;           // degrees (kit roof_pitch)
	UPROPERTY(Config) float Eave = 0.55f;               // kit eave: overhang along the slopes
	UPROPERTY(Config) float RoofSideOverhang = 0.3f;    // overhang past the gables
	UPROPERTY(Config) float RoofThickness = 0.12f;
	UPROPERTY(Config) float ParapetHeight = 0.75f;
	UPROPERTY(Config) float CopingHeight = 0.17f;
	UPROPERTY(Config) float ShutterShare = 0.75f;       // kit shutter_share (homes, cafes and shops only)
	UPROPERTY(Config) float ShutterCullDistance = 180.f; // m; shutters fade out beyond this
	UPROPERTY(Config) float OpeningCullDistance = 450.f; // m; windows and doors fade out beyond this
	UPROPERTY(Config) float TreeTrunkHeight = 3.f;
	UPROPERTY(Config) float TreeTrunkWidth = 0.35f;
	UPROPERTY(Config) float TreeCrownWidth = 4.f;
	UPROPERTY(Config) FLinearColor GroundColour = FLinearColor(0.34f, 0.315f, 0.31f);
	UPROPERTY(Config) FLinearColor BlockColour = FLinearColor(0.395f, 0.37f, 0.365f);
	UPROPERTY(Config) FLinearColor GreenColour = FLinearColor(0.30f, 0.33f, 0.20f);
	UPROPERTY(Config) TArray<FLinearColor> PlasterColours;  // one per building, picked from its lot id
	UPROPERTY(Config) FLinearColor StoneColour = FLinearColor(0.395f, 0.37f, 0.365f);
	UPROPERTY(Config) FLinearColor RoofColour = FLinearColor(0.43f, 0.24f, 0.165f);
	UPROPERTY(Config) TArray<FLinearColor> FrameColours;    // window frames: cream, wood, teal
	UPROPERTY(Config) TArray<FLinearColor> ShutterColours;  // teal, wood red, ochre, dusty blue
	UPROPERTY(Config) TArray<FLinearColor> DoorColours;     // wood red, wood, teal
	UPROPERTY(Config) TArray<FBrixSlotColour> SlotColours;  // the fixed slots: stone_dark, stone_a, glass, iron, ...
	UPROPERTY(Config) FLinearColor TrunkColour = FLinearColor(0.24f, 0.16f, 0.11f);
	UPROPERTY(Config) FLinearColor CrownColour = FLinearColor(0.30f, 0.33f, 0.20f);

private:
	struct FOpening { float S, Z0, W, H; UStaticMesh* Mesh; UStaticMesh* Shutters; };

	UInstancedStaticMeshComponent* Layer(const FString& Key, UStaticMesh* Mesh, const FLinearColor& Colour, bool bCollide = true);
	UInstancedStaticMeshComponent* PieceLayer(UStaticMesh* Mesh, int32 Frame, int32 Shutter, int32 Door, float CullM);
	UMaterialInstanceDynamic* Colour(const FLinearColor& C);
	void BuildLot(const FBrixLot& L, const FBrixBlock& B);
	UStaticMesh* Kit(const TCHAR* Name);

	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
	UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
	UPROPERTY() TObjectPtr<UMaterialInterface> Greybox;
	UPROPERTY() TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Parts;
	UPROPERTY() TMap<FString, TObjectPtr<UStaticMesh>> KitMeshes;
	UPROPERTY() TMap<uint32, TObjectPtr<UMaterialInstanceDynamic>> Colours;
	TMap<FString, TArray<FTransform>> Pending;   // instances gathered per layer, added in one go
	int32 Buildings = 0, Openings = 0;
};
