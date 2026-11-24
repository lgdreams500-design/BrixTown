#pragma once

#include "CoreMinimal.h"

// The city as the files in World/ describe it (docs/specs/domains/WORLD_MAP.md). Units are those of the data: metres,
// x east, z south, angles in degrees. Nothing here knows what is in the map (WLD-020); it only reads what the files say.

// One stone, brick or paving pattern of World/rules/surfaces.json (ADR-0017).
struct BRIXWORLD_API FBrixPattern
{
	FString Name;
	FString For;                 // "wall" or "pavement"
	FString Type;                // bond, flemish, rubble, flags, herringbone, basket
	FVector2D Unit = FVector2D(1, 0.5);
	float Joint = 0.02f, Vary = 0.8f, Bevel = 0.f, Dome = 0.f, Mortar = 0.f, Upper = 0.f;
	TArray<TOptional<FVector3f>> Colors;   // unset: the town's own stone or paving
	int32 TypeIndex() const;     // the slice's numbering: bond 0, flemish 1, rubble 2, flags 3, herringbone 4, basket 5
};

// What one surface uses (world.surfaces()): a pattern, its colour (unset: the town's own), and for a wall whether the
// pattern runs on up the plaster storeys to the eaves.
struct BRIXWORLD_API FBrixSurface
{
	int32 Pattern = INDEX_NONE;  // into FBrixCity::Patterns
	TOptional<FVector3f> Color;
	bool bUpper = false;
};

// One rule of surfaces.json: if every field named matches (template, district, kind, name, tags...), use one pattern or
// pick among several by weight.
struct BRIXWORLD_API FBrixSurfaceRule
{
	TMap<FString, TArray<FString>> If;
	FString Use;                                 // one pattern, or empty when Weights is used
	TArray<TPair<FString, double>> Weights;      // sorted by name, as Python's sorted()
};

// A designed house's line and colourway (lot.house; World/rules/house_styles.json, ADR-0032), resolved to the colours and
// trim the house is built with (houses.js lookS()).
struct BRIXWORLD_API FBrixHouseLook
{
	FString Plan, Line, Style;
	bool bStone = false, bQuoins = false, bBoxes = false, bArches = false, bHood = false;
	FVector3f Wall = FVector3f(0.56f, 0.53f, 0.50f);   // the plaster's light tone, or the line's stone
	FVector3f Shutter = FVector3f::ZeroVector, Door = FVector3f::ZeroVector, Frame = FVector3f::ZeroVector, Roof = FVector3f::ZeroVector;
	FVector4f Garden = FVector4f(-7.5f, -9.5f, 7.5f, 8.0f);   // lot.house.garden: its left, front, right, back (u, v from the house's centre)
};

// A window of a house plan (World/rules/home_plans.json): which face, where along it in the built house (metres from the
// house's centre, u across the front or v from front to back), how wide.
struct BRIXWORLD_API FBrixPlanWindow
{
	FString Side;      // front, back, left, right
	float At = 0.f, Width = 1.f;
};

// The outside of a designed house's plan (houses.js: the plan's 0.25 m walls mapped to the house's 0.32 m ones)
struct BRIXWORLD_API FBrixHousePlan
{
	FString Name;
	float HalfU = 6.f, HalfV = 4.5f;     // the house is 2 HalfU across the front, 2 HalfV deep
	float DoorAt = 0.f, DoorWidth = 1.f;
	TArray<FBrixPlanWindow> Windows;
};

struct BRIXWORLD_API FBrixLot
{
	FString Id;
	FString Kind;
	FString Name;
	FVector2D Min = FVector2D::ZeroVector;   // footprint before turning (rect x0, z0)
	FVector2D Max = FVector2D::ZeroVector;   // (rect x1, z1)
	FVector2D Door = FVector2D::ZeroVector;
	int32 Floors = 0;
	float AngleDeg = 0.f;                    // turned about the footprint's centre
	bool bEnterable = false;
	TArray<FString> Tags;
	TMap<FString, FString> OwnSurface;       // the lot's 'surface' field: slot -> pattern, beats the rules
	FBrixSurface Surface;                    // a building's wall, or an open lot's ground (none for a garden)
	TOptional<FBrixHouseLook> House;         // a designed house (lot.house): its plan, line and colourway

	FVector2D Centre() const { return (Min + Max) * 0.5; }
	FVector2D Size() const { return Max - Min; }
	// Open ground with no building on it (the slice's isOpen: gardens, the market, tram stops).
	bool IsOpen() const { return Kind == TEXT("garden") || Kind == TEXT("market") || Kind == TEXT("stop"); }
};

struct BRIXWORLD_API FBrixTree
{
	FString Id;
	FString Species;
	FVector2D Pos = FVector2D::ZeroVector;
};

struct BRIXWORLD_API FBrixBlock
{
	FString Id;
	FString Template;
	FString District;
	FVector2D Origin = FVector2D::ZeroVector;   // north-west corner
	FVector2D Extent = FVector2D::ZeroVector;   // size, over every grid cell it spans
	TArray<FBrixLot> Lots;
	TArray<FBrixTree> Trees;
	TMap<FString, FString> OwnSurface;
	FBrixSurface Pavement;
};

// The calendar and the sky the sun is worked out from (World/rules/calendar.json, sky.json; ADR-0024, WLD-043). Nothing
// about the sun is stored or ticked: its place follows from the day and the minute.
struct BRIXWORLD_API FBrixSky
{
	TArray<FString> Seasons;
	TArray<float> Sunrise, Sunset;     // hours, per season
	int32 DaysPerSeason = 12;
	float RealSecondsPerMinute = 10.f; // the clock's pace (WLD-026)
	float LatitudeDeg = 40.f;          // sky.json night.latitude
	float SunLift = 0.08f;             // the light comes from no lower than this (paths.sun_lift)

	int32 SeasonOf(int32 Day) const;   // citysim clock.info(): (day mod year) // days per season
	double Declination(int32 Season) const;                 // radians; world.sun_heights()
	double HourAngle(int32 Season, double Minute) const;    // radians, 0 at the middle of the calendar's day (play.js sunHour())
	FVector3d SunPlace(int32 Season, double HourAngle) const;   // east +x, up +y, south +z; world.sun_place()
	double Daylight(int32 Season, double Minute) const;     // 0 at night to 1 in full day, an hour of dawn and dusk (play.js lighting())
};

struct BRIXWORLD_API FBrixCity
{
	FString Id;
	FString Name;
	int32 Columns = 0;
	int32 Rows = 0;
	float Pitch = 0.f;          // metres from one street's middle to the next
	float StreetWidth = 0.f;
	float BlockSize = 0.f;
	FVector2D SpawnPos = FVector2D::ZeroVector;
	FVector2D SpawnFacing = FVector2D(1, 0);
	FString SpawnTime;
	TArray<FBrixBlock> Blocks;
	TArray<FBrixPattern> Patterns;          // in the order of surfaces.json
	TMap<FString, TArray<FBrixSurfaceRule>> SurfaceRules;   // slot -> rules, first match wins
	TMap<FString, FBrixHousePlan> HousePlans;  // World/rules/home_plans.json, the plans designed houses are built to
	FBrixSky Sky;

	int32 NumLots() const;
	int32 FindPattern(const FString& Name) const;
	int32 NumTrees() const;
};

// Reads <CityDir>/city.json and every block file it lists. On failure returns false and says why in OutError.
BRIXWORLD_API bool LoadBrixCity(const FString& CityDir, FBrixCity& OutCity, FString& OutError);

// The pattern one slot ("wall", "pavement", "ground") of a block (Lot null) or lot uses, by the rules of
// World/rules/surfaces.json or the surface's own 'surface' field: a port of world.surface_pick() (ADR-0017). LoadBrixCity
// fills every lot's Surface and every block's Pavement with it; this is exposed for tests.
BRIXWORLD_API int32 PickBrixSurface(const FBrixCity& City, const FString& Slot, const FBrixBlock& Block, const FBrixLot* Lot);
// world._surface_hash(): the first 8 hex digits of the MD5 of the parts joined by "|", as a number in 0..1.
BRIXWORLD_API double BrixSurfaceHash(const TArray<FString>& Parts);

// Checks what the game relies on (WLD-022): every lot has a unique id, a kind, a name, a footprint with area, a door,
// and a building at least one floor high unless it is open ground. Returns the problems found (empty when sound).
BRIXWORLD_API TArray<FString> CheckBrixCity(const FBrixCity& City);
