#pragma once

#include "CoreMinimal.h"

struct FBrixCity;
struct FBrixLot;

// Where the town has a roof overhead (WLD-037, WLD-070): every building's footprint (its lot, turned as the lot is; not the
// open lots) on a grid of Cell metres, each cell holding how high its roof is in Step metres (0: open sky). M_Greybox reads
// it as a texture, so the wet paving's darkening, sheen, puddles and rings and lying snow stop under a roof: indoor floors,
// stairs and furniture tops stay dry, roofs and everything out of doors are as before.
struct BRIXWORLD_API FBrixRoofMap
{
	FVector2D Origin = FVector2D::ZeroVector;   // the grid's corner (metres, x east, z south)
	double Cell = .5, Step = .5;
	int32 W = 0, H = 0;
	TArray<uint8> Px;                           // row by row, W x H
	double TopAt(const FVector2D& P) const;     // the roof's height over P (metres), 0 under the sky
	bool Under(const FVector2D& P, double Up) const { return Up < TopAt(P); }
};

// TopOf: the height under which a building's lot is indoors (its roof's underside, metres above the street); a lot it
// gives 0 or less for is left out. The grid has a cell of open sky all round.
BRIXWORLD_API FBrixRoofMap BrixRoofMap(const FBrixCity& City, double Cell, double Step, TFunctionRef<double(const FBrixLot&)> TopOf);
