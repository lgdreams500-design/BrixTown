#pragma once

#include "CoreMinimal.h"

struct FBrixInsides;
struct FBrixInside;
struct FBrixLot;
class FJsonObject;

// Homes furnished from their plans (FURN-060 to FURN-063, F-HOME-FURNISH level 3; ADR-0035, ADR-0036): a port of the
// slice's homes.js (row houses and court flats: lot.interior and lot.interior_frame) and houses.js insideS() (the designed
// houses: plan house_12x9). The plans are World/rules/home_plans.json, packed into Game/Data/inside.json by build_play.py.
// Everything here is laid out in the data's frame (metres, x east, z south); Unreal places the kit's pieces from it.

// One piece of a plan (home_plans.json items).
struct BRIXWORLD_API FBrixPlanItem
{
	FString Storey, Room, Piece;
	float X = 0, Y = 0, Yaw = 0, Lift = 0;
	int32 Level = 1;
};

struct BRIXWORLD_API FBrixHomePlan
{
	FString Key;
	float W0 = 0, W1 = 0, D0 = 0, D1 = 0, T = 0.25f;
	float XA = 0, XB = 0, YM = 0;                            // house_12x9: its inner walls' lines
	TArray<float> Stair;                                    // house_12x9: x0, x1, y0, y1 of the stair
	TArray<FString> Floors;                                 // the storeys' names in order (row plans; house_12x9: ground, upstairs)
	struct FLine { FString Ax; float At = 0, A0 = 0, A1 = 0; };
	struct FDoor { FString Ax; float At = 0, C = 0; };
	TMap<FString, TArray<FLine>> Partitions;                // per storey
	TMap<FString, TArray<FDoor>> Doors;                     // per storey
	TMap<FString, TArray<TPair<FString, FVector4f>>> Rooms; // per storey: name, (x0, x1, y0, y1)
	TMap<FString, TArray<TArray<float>>> Spots;             // house_12x9: where people lie and sit on a piece (x, y, height, facing x, facing y)
	TArray<FString> Beds;                                   // kinds that are beds
	TArray<FBrixPlanItem> Items;
};

// FURN-063 (World/rules/furnishing_levels.json): a household's level from its daily wages and the clock day, never ticked.
struct BRIXWORLD_API FBrixFurnishRules
{
	int32 Top = 4;
	TArray<TPair<int64, int32>> StartBands;                 // from wage, level
	int64 SaveBp = 1000;
	TArray<int64> StepCost;
	int32 LodgerLevel = 2, EmptyLevel = 1;
	int32 Level(int64 Wage, int32 Day) const;               // estate.js furnish.level(F, wage, day)
};

// A piece where it stands: its kind and line (the mesh), its floor, its middle in the world, its own axes in the world (A
// across it, B from its front to its back, as the kit's pieces are made: x and y), and how far above the floor it is.
struct BRIXWORLD_API FBrixFurniturePiece
{
	FString Kind, Line;
	int32 Floor = 0;
	FVector2D At = FVector2D::ZeroVector, A = FVector2D(1, 0), B = FVector2D(0, 1);
	float Lift = 0.f;
	FVector3f Size = FVector3f::ZeroVector;                 // furniture.js SIZE (w, d, h); zero when the slice has none
	int32 Item = -1;                                        // into the plan's items
	bool bBed = false;
	bool bLayout = false;                                   // placed in the designer (FURN-051): a storey's layout, not the town's
};

// A rectangle on the floor, turned: its middle, the way its length runs, its length and its width (a designed house is
// turned with its lot).
struct BRIXWORLD_API FBrixRect
{
	FVector2D Centre = FVector2D::ZeroVector, Along = FVector2D(1, 0);
	float Length = 0.f, Width = 0.f;
};

// A plan's inner wall, cut at its doorways (a doorway's piece is the wall over the door).
struct BRIXWORLD_API FBrixWallSeg
{
	FBrixRect R;
	int32 Floor = 0;
	bool bDoorway = false;
	bool bPartition = false;                 // a storey's own inner walls (storeys.js partitions(), or the room plan: BUILD-021)
	TOptional<FVector3f> Colour;             // its own colour (BUILD-022), else the plaster of the walls
};

struct BRIXWORLD_API FBrixRoom
{
	FString Name;
	int32 Floor = 0;
	FBrixRect R;
	bool bWet = false;                                      // a kitchen, bathroom or WC: tiled (homes.js)
	bool bFloor = true;                                     // its floor is laid (false: a building's whole storey, for its lamps only)
};

// A place a resident lies or sits (homes.js spots(), houses.js spotsS()): where, its height above the floor, the way the
// person faces sitting, or the way their head lies (toward the bed's head) lying; lying or sitting.
struct BRIXWORLD_API FBrixSpot
{
	FVector2D At = FVector2D::ZeroVector, Facing = FVector2D(1, 0);
	int32 Floor = 0;
	float Up = 0.f;
	bool bLie = false;
};

struct BRIXWORLD_API FBrixHome
{
	FString Plan, Line;
	int32 Level = 1;                                        // FURN-063 today
	TArray<FBrixFurniturePiece> Pieces;                              // at the level and below, a bed for each resident, none in front of a window
	TArray<FBrixWallSeg> Walls;
	TArray<FBrixRoom> Rooms;
	TArray<FBrixSpot> Spots;
	int32 Dropped = 0;                                      // pieces left out in front of a window
};

// A window of the building's own facade on a floor, as a span on the inside face of its wall (world).
struct BRIXWORLD_API FBrixWindowSpan
{
	FVector2D A = FVector2D::ZeroVector, B = FVector2D::ZeroVector;
	int32 Floor = 0;
};

// Reads the plans, the furnishing rules, the pieces' sizes and the hung pieces from inside.json's root (LoadBrixInside).
BRIXWORLD_API void LoadBrixHomeData(const TSharedPtr<FJsonObject>& Root, FBrixInsides& Out);

// The home of a lot furnished on day Day: a planned row house (lot.interior) or a designed house (lot.house, plan
// house_12x9), or false. Windows: the building's real windows (tall pieces in front of them are left out, homes.js dropped()).
BRIXWORLD_API bool BrixHomeOf(const FBrixInsides& Ins, const FBrixLot& Lot, const FBrixInside& In, int32 Day,
	const TArray<FBrixWindowSpan>& Windows, FBrixHome& Out);

// A planned court flat (lot.flats with an interior, homes.js flats()): laid out from its plan on its own floor, at its own
// household's furnishing level.
BRIXWORLD_API bool BrixFlatHomeOf(const FBrixInsides& Ins, const FBrixInside& In, int32 FlatIndex, int32 Day, const TArray<FBrixWindowSpan>& Windows, FBrixHome& Out);

// A designed house's stair well in the house's own frame (houses.js SWELL: u0, u1, v0, v1; u as the plan's x, mirrored into
// the house as everything of houses.js) and where you step off it upstairs (SOFF).
BRIXWORLD_API bool BrixHouseWell(const FBrixInsides& Ins, FVector4f& Well, float& StepOff);

struct FBrixDay;
struct FBrixCity;
struct FBrixDoorRules;
// The day's stays at home inside (CITY-013, WLD-053): at a bed a resident lies on the nearest bed place of their floor, at a
// room place they sit on the nearest seat (homes.js one(): the lot's places sit on the plan's pieces); any other stay
// inside stands on its floor. Returns how many lie and sit.
BRIXWORLD_API int32 SeatBrixDayIndoors(FBrixDay& Day, const FBrixCity& City, const FBrixInsides& Ins, const FBrixDoorRules& R);
