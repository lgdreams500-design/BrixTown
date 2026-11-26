#pragma once

#include "CoreMinimal.h"
#include "BrixHomes.h"

struct FBrixInsides;
struct FBrixInside;
struct FBrixLot;
struct FBrixDoorRules;

// The rooms of the buildings that have no plan of their own furnished at their places (CITY-013, F-PROP-FURNISH): a port of
// the places part of the slice's play.js furnish(): a bed with its nightstand and lamp at each bed place, a chair at each
// room place and chair place, a table with chairs at each seat (a cafe's for two), a cinema's seats and a museum's benches
// facing in, a desk with its lamp and chair or a counter with its till, espresso machine or ledger at each work place, all
// in the slice's colours and its room styles (play.js ROOM_STYLE). The slice draws these as boxes; so does Unreal. Then, but
// in a building of flats, the room is dressed as the slice dresses it (play.js room() and layout()): each kind of building's
// arrangement round its places (a cafe's coffee bar, a grocer's aisles and fridges, a home's wardrobe, rug and kitchenette,
// a bank's counter, a library's stacks, ...), pieces along the walls and pictures over them, what stands on the open floor
// and the lamps hanging from the beams.

// A box of furniture: its middle on the floor plan, its bottom above the floor, its size (x across, y up, z deep in its own
// frame), turned by Yaw as the slice turns it (M4.ry), its colour; bGlow: a lamp's shade or a lit sign.
struct BRIXWORLD_API FBrixFurnBox
{
	FVector2D At = FVector2D::ZeroVector;
	float Y0 = 0.f;
	FVector3f Size = FVector3f::OneVector;
	float Yaw = 0.f;
	FVector3f Colour = FVector3f(0.5f);
	bool bGlow = false;
};

// What is in the way (play.js hard()): a box on the floor plan, from Y0 to Y1 above the floor.
struct BRIXWORLD_API FBrixFurnSolid
{
	FVector2D Min = FVector2D::ZeroVector, Max = FVector2D::ZeroVector;
	float Y0 = 0.f, Y1 = 0.f;
};

struct BRIXWORLD_API FBrixFurnished
{
	TArray<FBrixFurnBox> Boxes;
	TArray<FBrixFurnSolid> Solids;
	TArray<FBrixSpot> Spots;        // play.js spot(): who is at each place lies, sits or stands there, facing this way (Up 0: standing)
	TArray<FBrixFurniturePiece> Pieces;   // a fit-out's meshes (FBrixFitout), on this floor
	TArray<FBrixWallSeg> Walls;           // a fit-out's partitions, cut at their doorways (a doorway's piece is the wall over it)
};

// Floor f of a lot furnished at its places, unless it is a planned home (BrixHomes) or a building drawn its own way. False
// when there is nothing to furnish. Nothing is put in the stairwell or 0.6 m round it, nor on the way from the door to the
// foot of the stair (storeys.js keepOut()). bDress false: the places' furniture only, the room left undressed.
// A lot with a fit-out (lot.fitout) is furnished on the fit-out's floor from it alone: its pieces as meshes, its glow and its
// lamps; the slice's places' furniture and dressing are not put there (CITY-013, ADR-0059).
BRIXWORLD_API bool BrixFurnishFloor(const FBrixInsides& Ins, const FBrixLot& Lot, const FBrixInside& In, int32 Floor, FBrixFurnished& Out, bool bDress = true);
