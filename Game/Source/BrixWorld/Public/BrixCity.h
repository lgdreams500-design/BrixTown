#pragma once

#include "CoreMinimal.h"

// The city as the files in World/ describe it (docs/specs/domains/WORLD_MAP.md). Units are those of the data: metres,
// x east, z south, angles in degrees. Nothing here knows what is in the map (WLD-020); it only reads what the files say.

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

	int32 NumLots() const;
	int32 NumTrees() const;
};

// Reads <CityDir>/city.json and every block file it lists. On failure returns false and says why in OutError.
BRIXWORLD_API bool LoadBrixCity(const FString& CityDir, FBrixCity& OutCity, FString& OutError);

// Checks what the game relies on (WLD-022): every lot has a unique id, a kind, a name, a footprint with area, a door,
// and a building at least one floor high unless it is open ground. Returns the problems found (empty when sound).
BRIXWORLD_API TArray<FString> CheckBrixCity(const FBrixCity& City);
