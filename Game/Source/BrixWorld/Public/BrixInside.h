#pragma once

#include "CoreMinimal.h"

struct FBrixCity;
struct FBrixLot;
struct FBrixHousePlan;

// What is inside the town's buildings (CITY-013, WLD-050 to WLD-053): Game/Data/inside.json, written by
// Tools/worldgen/build_play.py from World/ (the block files' stair, lift, storeys, levels, flats and planned rooms, and each
// building's design: its inside finishes per storey, BUILD-019). Lots are the city's in its blocks' order (the slice's LOTS).

// World/rules/storeys.json: the heights and the stair's sizes (WLD-050, WLD-052).
struct BRIXWORLD_API FBrixStoreyRules
{
	float Storey = 3.f, Slab = 0.2f, Wall = 0.35f;
	float StairWidth = 1.1f, Spine = 0.15f, Landing = 1.2f, Run = 4.5f, KeepClear = 0.7f;
	int32 LiftFromFloors = 6;
};

// A building's stairwell (lot.stair; Tools/worldgen/add_storeys_01.py): its rectangle, the way the flights run, the wall
// the first flight is against, which way it rises, and a row house's one straight flight a storey.
struct BRIXWORLD_API FBrixStair
{
	FVector2D Min = FVector2D::ZeroVector, Max = FVector2D::ZeroVector;
	bool bAlongX = false;
	FString Wall;
	int32 Up0 = 1;
	bool bStraight = false;
	float Width = 0.f;                       // 0: the rules' StairWidth
};

// How a home plan of World/rules/home_plans.json lies in its home (lot.interior_frame; Tools/worldgen/add_homes_01.py): the
// plan's origin and axes in the world, and per storey the plan's wall lines (U, V) mapped to the home's.
struct BRIXWORLD_API FBrixPlanFrame
{
	FVector2D O = FVector2D::ZeroVector, X = FVector2D(1, 0), Y = FVector2D(0, 1);
	TArray<TArray<float>> U, V;
};

// One flat of a building divided into flats (lot.flats; CITY_BALANCE BAL-006).
struct BRIXWORLD_API FBrixFlat
{
	FString Id, Name, Interior, Line;
	int32 Floor = 0;
	FVector2D Min = FVector2D::ZeroVector, Max = FVector2D::ZeroVector, Door = FVector2D::ZeroVector;
	TArray<FString> Household;
	TOptional<FBrixPlanFrame> Frame;
};

struct BRIXWORLD_API FBrixInside
{
	FString Id, Special;                     // the design's special name (mall, works, depot, ...), empty for none
	float DoorWidth = 1.7f;                  // the design's door (the slice's doorway)
	TArray<FVector3f> FloorColour;           // per storey (BUILD-019)
	TArray<int32> FloorFinish;               // the slice's finish number per storey (15 boards, 16 stone, ...)
	TArray<FVector3f> WallColour;            // per storey
	TArray<FString> Uses;                    // what each storey is (lot.storeys: main, bedrooms, store, flat, office, ...)
	TOptional<FBrixStair> Stair;
	TOptional<FVector2D> Lift;               // a tower's lift shaft (WLD-051)
	bool bTower = false;
	TMap<FString, TArray<int32>> Levels;     // the floor of each of the lot's places, by kind (lot.levels)
	TArray<FBrixFlat> Flats;
	FString Interior, Line;                  // a planned home: its plan and the line it is furnished in
	TOptional<FBrixPlanFrame> Frame;
	int32 Residents = 0;
	int64 Wage = 0;                          // the household's daily wages (FURN-063; furnishing.js earlyLevel())
	FString HouseLine;                       // a designed house's furniture line (lot.house.furniture)
	FVector3f Accent = FVector3f(0.20f, 0.36f, 0.37f);   // the design's accent (its awning's colour): a counter's top
	TMap<FString, TArray<FVector2D>> Objects;   // the lot's places by kind (bed, room, work, seat, ...)
};

#include "BrixHomes.h"

struct BRIXWORLD_API FBrixInsides
{
	FBrixStoreyRules Rules;
	TMap<FString, FBrixHomePlan> Plans;      // World/rules/home_plans.json (FURN-060)
	FBrixFurnishRules Furnish;               // World/rules/furnishing_levels.json (FURN-063)
	TMap<FString, FVector3f> Sizes;          // the pieces' sizes, w d h (furniture.js SIZE)
	TMap<FString, TArray<float>> Hung;       // pieces on a wall or from the ceiling: x0 x1 y0 y1 z0 z1 (homes.js CAM_ONLY)
	TArray<TOptional<FBrixInside>> Lots;     // per lot in the city's order; unset for open ground
};

// A stairwell worked out as the slice does it (storeys.js stairOf()): u along the flights, v across them; lane 0 against the
// wall (the first flight's), lane 1 beside it past the spine wall (a switchback's second flight); landings at both ends.
struct BRIXWORLD_API FBrixStairGeo
{
	bool bAlongX = false, bStraight = false;
	float U0 = 0, U1 = 0, V0 = 0, V1 = 0, ULo = 0, UHi = 0, Open = 0;
	FVector2f Lane0 = FVector2f::ZeroVector, Lane1 = FVector2f::ZeroVector, Spine = FVector2f::ZeroVector;   // (v from, v to)
	struct FFlight { FVector2f Lane; int32 Dir = 1; float Ub = 0, Ut = 0; };
	int32 Up0 = 1;
	FFlight Flight(int32 Floor) const;                        // the flight up from that floor
	FVector2D P(float U, float V) const { return bAlongX ? FVector2D(U, V) : FVector2D(V, U); }   // to the world (x, z)
	FVector2D UV(const FVector2D& Xz) const { return bAlongX ? Xz : FVector2D(Xz.Y, Xz.X); }
	FVector2D FootAt(int32 Floor) const;                      // where you step on to go up from that floor
	// What stands on the ground floor of the well, in (u0, u1, v0, v1): the first flight's steps, a switchback's spine wall
	TArray<FVector4f> GroundSolids() const;
};
BRIXWORLD_API FBrixStairGeo BrixStairOf(const FBrixStair& S, const FBrixStoreyRules& R);

// Reads Game/Data/inside.json. On failure returns false and says why.
BRIXWORLD_API bool LoadBrixInside(const FString& Path, FBrixInsides& Out, FString& OutError);

// The doorway of an enterable building (CITY-013), where the Unreal town cuts it: on the facade nearest the lot's door, at
// the door (a designed house: its plan's door; a shopfront: the pane nearest the door), as wide and high as the kit's open
// door there. In the data's frame (metres, x east, z south); heights above the building's ground.
struct BRIXWORLD_API FBrixDoorway
{
	FVector2D Centre = FVector2D::ZeroVector;   // on the outer face of the wall
	FVector2D Out = FVector2D(0, -1);           // the way out of the building
	float Width = 1.3f, Height = 2.3f;
	int32 Side = 0;                             // the facade: 0 front (-v), 1 back, 2 left (-u), 3 right, in the lot's own frame
	float Along = 0.f;                          // along that facade from its middle, in the facade's own direction
	enum class EKind : uint8 { Plain, Arch, Civic, Shop, House } Kind = EKind::Plain;
	bool bArchHouse = false;                    // a designed house of an arched line (its door is the arched one)
};

// The kit's numbers the doorway is worked out with: [/Script/BrixTown.BrixTownActor] of DefaultGame.ini, as the actor reads them.
struct BRIXWORLD_API FBrixDoorRules
{
	float Wall = 0.32f, Bay = 3.2f, MinFacade = 2.f, ShopPane = 2.2f, ShopPier = 0.32f;
	float DoorPlain = 1.3f, DoorCivic = 1.5f, PlainHeight = 2.3f, ArchHeight = 2.5f, CivicHeight = 2.6f, ShopDoorHeight = 2.3f;
	float HousePlinth = 0.3f, HouseStorey = 3.3f;
	float Base = 0.12f, InsideFloor = 0.08f, Storey = 3.f, CeilingDrop = 0.12f;   // the block's paving; a floor's top over a storey's line; a storey; a ceiling under the next line
	TArray<FString> OutskirtDistricts = { TEXT("Outskirts") };   // districts that are not old: doors are plain there
	static FBrixDoorRules FromConfig();       // from GConfig (Game ini), falling back to the numbers above
};

// Whether the Unreal town builds this lot hollow, with a doorway (every enterable building but those drawn their own way:
// the furniture outlet's hall and the malls).
BRIXWORLD_API bool BrixIsHollow(const FBrixLot& Lot, const FBrixInside* Inside);
// How high above the street the top of a building's floor f is, as the Unreal town builds it (WLD-052): the ground floor
// InsideFloor over the paving, floor f InsideFloor over its storey's line; a designed house on its plinth, storeys of 3.3 m.
BRIXWORLD_API float BrixFloorUp(const FBrixLot& Lot, int32 Floor, const FBrixDoorRules& R);
// and its ceiling (the top floor's under the roof)
BRIXWORLD_API float BrixCeilingUp(const FBrixLot& Lot, int32 Floor, const FBrixDoorRules& R);
// A call of a tower's lift (WLD-051; storeys.js shell()): where it is (on the plan and above the street), what E offers there
// and where it takes the player. Floor -1: on the roof.
struct BRIXWORLD_API FBrixLiftCall
{
	FVector At = FVector::ZeroVector, To = FVector::ZeroVector;   // x, z, height above the street
	FString Text;
	int32 Floor = 0;
};
// Every call of a lot's lift on every floor and on the roof (RoofUp: the height the roof is stood on), or none without one.
BRIXWORLD_API TArray<FBrixLiftCall> BrixLiftCalls(const FBrixLot& Lot, const FBrixInside& In, const FBrixDoorRules& R, float RoofUp);
BRIXWORLD_API FBrixDoorway BrixDoorwayOf(const FBrixCity& City, const FBrixLot& Lot, const FString& District, const FBrixDoorRules& R);
