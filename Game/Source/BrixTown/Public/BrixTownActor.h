#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixTownActor.generated.h"

struct FBrixCity;
struct FBrixLot;
struct FBrixBlock;
struct FBrixPattern;
struct FBrixSurface;
struct FBrixInsides;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;

// A street thing or a tree a car can break (CITY-072; smash.js wrap()): what it is, where it stands, the instances it is
// drawn with (so it can be hidden while broken and shown again when mended), its light and its footprint near the ground.
struct FBrixBreakable
{
	FString Kind, PropKind;             // crash.json's name for it, and the prop's own
	FVector2D Pos = FVector2D::ZeroVector;
	float Ground = 0.f, Top = 0.f;      // m
	FBox2D Box = FBox2D(ForceInit);     // its footprint near the ground (a tree's trunk, not its crown)
	struct FPart { class UInstancedStaticMeshComponent* G = nullptr; int32 Index = -1; FTransform At; FLinearColor Colour; bool bPlaced = false; };
	TArray<FPart> Parts;
	int32 Light = -1;                   // its entry in LampLights
	bool bBroken = false;
};

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
	// What is inside the buildings (Game/Data/inside.json; CITY-013): set before Build to make the enterable ones hollow,
	// with an open doorway, a floor, plastered walls and a ceiling
	const FBrixInsides* Insides = nullptr;
	// Where the lamps' and wall lanterns' light comes from (the slice's lights: x, height, z in metres and a strength), and
	// their glass, which glows by night (ABrixLamps sets its material's Glow).
	TArray<FVector4f> LampLights;
	// Everything a car can break, by the rules' kinds (CITY-072): filled while building when Breakable is set
	TArray<FBrixBreakable> Breakables;
	TSet<FString> Breakable;            // the prop kinds to keep handles for (set before Build)
	void SetBroken(int32 b, bool bBroken);   // hide it (its solids and its light too) or show it again
	// wet paving (WLD-037): M_Greybox's WetDark, WetSheen and Puddles on every surface (Tools/unreal/greybox_wet.py); 0 dry
	void SetWet(float Dark, float Sheen, float Puddles);
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GlassMat;

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
	UPROPERTY(Config) float ShopTop = 3.45f;           // kit SHOP_TOP: the shopfront's opening and the fascia's top
	UPROPERTY(Config) float ShopPier = 0.32f;          // kit shopfront() pilaster width
	UPROPERTY(Config) float ShopPane = 2.2f;           // target spacing of the piers
	UPROPERTY(Config) float AwningZ = 2.5f, AwningOut = 1.5f;          // a shop's awning (kit shopfront())
	UPROPERTY(Config) float CafeAwningZ = 3.35f, CafeAwningOut = 1.7f; // a cafe's awning over the arches (kit cafe_front())
	UPROPERTY(Config) float AwningStrip = 0.45f;       // kit awning(): width of one striped strip
	UPROPERTY(Config) TArray<FLinearColor> AwningColours;   // the two stripes: cream, terracotta
	UPROPERTY(Config) TArray<float> CrateAt;           // a shop's produce crates, metres from each end of the front
	UPROPERTY(Config) TArray<float> CafeTableAt;       // a cafe's tables, metres from the front's start
	UPROPERTY(Config) FLinearColor LetterColour = FLinearColor(0.70f, 0.64f, 0.50f);
	UPROPERTY(Config) float NameSize = 0.52f;          // m, the shop's name on its fascia (kit text size)
	UPROPERTY(Config) float BladeNameSize = 0.17f;     // m, a cafe's name on both faces of its blade sign (kit cafe_front())
	UPROPERTY(Config) FLinearColor IronColour = FLinearColor(0.10f, 0.10f, 0.11f);
	UPROPERTY(Config) float ChimneyAt = 0.7f;          // an old-district roof's chimney: share of the ridge from its start (kit tile_roof())
	UPROPERTY(Config) float ChimneyEnds = 0.9f;        // elsewhere two, this far in from the ends
	UPROPERTY(Config) float ChimneyBack = 1.0f;        // m behind the ridge (1.2 outside the old districts)
	UPROPERTY(Config) float GardenCullDistance = 150.f;   // m; a garden's small things (pickets, railings, bins) fade out beyond this
	UPROPERTY(Config) float HousePlinth = 0.3f;        // a designed house (houses.js outsideS): its ground floor's height
	UPROPERTY(Config) float HouseStorey = 3.3f;        // and each storey
	UPROPERTY(Config) FLinearColor HouseTrimStone = FLinearColor(0.62f, 0.58f, 0.52f);   // its sills, surrounds, string course
	UPROPERTY(Config) FLinearColor QuoinStone = FLinearColor(0.66f, 0.63f, 0.57f);
	UPROPERTY(Config) float InsideFloor = 0.08f;       // CITY-013: the ground floor's top above the block's paving (slice Y_FLOOR 0.2)
	UPROPERTY(Config) float CeilingDrop = 0.12f;       // a storey's ceiling this far under the next storey's line (its slab's underside)
	UPROPERTY(Config) float LiningDepth = 0.02f;       // the plaster inside the walls
	UPROPERTY(Config) float CeilingWhite = 0.25f;
	UPROPERTY(Config) FVector4 FloorBoardsA = FVector4(0, 1.2, 0.16, 0.005);   // a boarded floor: pattern type, unit, joint (as RoadPatternA)
	UPROPERTY(Config) FVector4 FloorBoardsB = FVector4(0.35, 0, 0, 0);
	UPROPERTY(Config) FVector4 FloorFlagsA = FVector4(3, 0.6, 0.6, 0.008);    // a stone floor: flags
	UPROPERTY(Config) FVector4 FloorFlagsB = FVector4(0.5, 0.01, 0, 0.3);      // a ceiling is its storey's wall colour this much toward white (play.js building())
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
	UPROPERTY(Config) FLinearColor TownStone = FLinearColor(0.52f, 0.48f, 0.43f);   // a pattern's "stone" colour (slice C.stone)
	UPROPERTY(Config) FLinearColor TownPave = FLinearColor(0.46f, 0.42f, 0.37f);    // a pattern's "pave" colour (slice C.pave)
	UPROPERTY(Config) FLinearColor RoadColour = FLinearColor(0.36f, 0.34f, 0.34f);
	UPROPERTY(Config) FVector4 RoadPatternA = FVector4(0, 0.24, 0.15, 0.012);        // the street's setts: type, unit w, unit h, joint
	UPROPERTY(Config) FVector4 RoadPatternB = FVector4(0.7, 0, 0.1, 0);               // vary, bevel, dome, mortar
	UPROPERTY(Config) FLinearColor TrunkColour = FLinearColor(0.24f, 0.16f, 0.11f);
	UPROPERTY(Config) FLinearColor CrownColour = FLinearColor(0.30f, 0.33f, 0.20f);

private:
	struct FOpening { float S, Z0, W, H; UStaticMesh* Mesh; UStaticMesh* Shutters; const FLinearColor* Paint; float MeshZ = -1.f; float SX = 1.f, SZ = 1.f; bool bDoorway = false; };   // MeshZ < 0: the piece sits at Z0; SX, SZ stretch it; bDoorway: the open way in
	struct FLook { FLinearColor Colour; FVector4f A = FVector4f(-1, 0, 0, 0), B = FVector4f(0, 0, 0, 0); };   // A.x < 0: painted plaster
	struct FPending { TArray<FTransform> At; TArray<FLook> Look; };

	// One group of instances per mesh (ADR-0046): each instance carries its own colour (custom data 0..2), so the
	// whole town is a handful of groups. A slot named frame, shutter, door or roof, and every slot of the plain shapes,
	// takes the instance's colour; the other slots (glass, stone, iron) are fixed from SlotColours.
	UInstancedStaticMeshComponent* Group(UStaticMesh* Mesh, bool bCollide, float CullM = 0.f, bool bClear = false);
	void Add(UInstancedStaticMeshComponent* G, const FTransform& At, const FLinearColor& C);
	void Add(UInstancedStaticMeshComponent* G, const FTransform& At, const FLook& Look);
	FLook Surface(const FBrixSurface& S, const FLinearColor& Town) const;
	UMaterialInstanceDynamic* Fixed(const FLinearColor& C);
	UMaterialInstanceDynamic* Own();
	void BuildLot(const FBrixLot& L, const FBrixBlock& B);
	void BuildGarden(const FBrixLot& L, const struct FBrixHousePlan& Plan, const struct FBrixHouseLook& Hs, float HouseTop);
	void BuildProp(const struct FBrixProp& P, const struct FBrixBlock& B);
	UStaticMesh* Kit(const TCHAR* Name, UStaticMesh* Fallback = nullptr);
	void Name(const FString& Text, const FVector& At, double YawRad, float Size, float MaxWidth, bool bFaceAlong = false);

	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
	UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
	UPROPERTY() TObjectPtr<UMaterialInterface> Greybox;
	UPROPERTY() TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Parts;
	UPROPERTY() TMap<FString, TObjectPtr<UStaticMesh>> KitMeshes;
	UPROPERTY() TMap<uint32, TObjectPtr<UMaterialInstanceDynamic>> Colours;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> OwnColour;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Glass;
	UInstancedStaticMeshComponent* GlassGroup();
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Blockers;
	UInstancedStaticMeshComponent* BlockerGroup();
	UPROPERTY() TObjectPtr<UMaterialInterface> ClearGlass;   // /Game/Greybox/M_Clear: a hollow storey's window glass   // unseen boxes in an enterable building's windows: only its doorway lets you through
	int32 LotIndex = 0;                             // the lot being built, in the city's order (the slice's LOTS)
	int32 Hollow = 0;
	TMap<UInstancedStaticMeshComponent*, FPending> Pending;   // instances gathered per group, added in one go
	FBrixBreakable* Wrapping = nullptr; // the breakable being built: Add() notes its instances (smash.js wrap())
	void Wrap(const FString& Kind, const FVector2D& Pos, float Ground, TFunctionRef<void()> Fn);
	const FBrixCity* CityData = nullptr;
	int32 Buildings = 0, Openings = 0;
};
