#pragma once

#include "CoreMinimal.h"

class FJsonObject;
struct FBrixFurnished;
struct FBrixFurnBox;

// Furnishing a storey in the building designer (FURN-051, FURN-052, FURN-055; F-BLD-DESIGN, F-PROP-FURNISH): a port of the
// slice's builder.js Furnish, itself a port of Tools/citysim/furnish.py, rule for rule and message for message. A layout
// is the storey's pieces: the town's own (kind "town", found from its furniture by play.js townPieces()) and pieces of
// World/rules/furnishing.json; each moved and turned on the grid, kept in the room, under the ceiling, out of the way of
// the door and the stairs and of each other; a piece taken away goes to the player's belongings (INV-001) and can be
// placed again. FURN051_furnishing_matches_the_python runs furnish.check_script() (inside.json builder.furnish.checks).

struct BRIXWORLD_API FBrixLayoutPiece
{
	FString Id, Kind = TEXT("town"), Name;
	double W = 1, D = 1, H = 1, X = 0, Z = 0, Yaw = 0;    // metres, degrees
	bool bRug = false;
	TOptional<FString> Line;
	TOptional<FString> FromLot;                           // a town piece taken from a storey: where it came from (lot, level, its id)
	int32 FromLevel = 0;
	FString FromId;
	bool operator==(const FBrixLayoutPiece& O) const { return Id == O.Id && Kind == O.Kind && X == O.X && Z == O.Z && Yaw == O.Yaw; }
};
using FBrixLayout = TArray<FBrixLayoutPiece>;

struct BRIXWORLD_API FBrixFurnishRulesD
{
	struct FPiece { FString Name, From; FVector3f Size = FVector3f(1); int64 Price = 0; bool bRug = false; };
	double Grid = 0.05, Turn = 15, RugH = 0.006;
	TArray<FString> Lines;
	TArray<TPair<FString, TArray<FString>>> Rooms;        // room kind -> the pieces it offers, in the file's order
	TMap<FString, FPiece> Pieces;
	bool Load(const TSharedPtr<FJsonObject>& Rules);
};

struct BRIXWORLD_API FBrixFurnLine { FString What; double Qty = 1; int64 Chips = 0; };

namespace BrixFurnishPlan
{
	BRIXWORLD_API TArray<FVector2D> Corners(const FBrixLayoutPiece& P);
	BRIXWORLD_API TArray<TPair<FString, FString>> Problems(const FBrixFurnishRulesD& F, const FVector4f& Room, const TArray<FVector4f>& Keep, double Ceiling, const FBrixLayout& Pieces);
	BRIXWORLD_API int64 Cost(const FBrixFurnishRulesD& F, const FBrixLayout& Before, const FBrixLayout& After, const FBrixLayout& Belongings, TArray<FBrixFurnLine>& OutLines);
	BRIXWORLD_API TArray<FString> Check(const FBrixFurnishRulesD& F, const FVector4f& Room, const TArray<FVector4f>& Keep, double Ceiling, const FBrixLayout& Before, const FBrixLayout& After, TOptional<int64> Funds, const FBrixLayout& Belongings);
	BRIXWORLD_API FBrixLayoutPiece KitPiece(const FBrixFurnishRulesD& F, const FString& Kind, const FString& Id, double X, double Z, double Yaw, const FString& Line);
	// the belongings after a change (INV-001): less what was placed, with what was taken away (ids "b:<lot>:<level>:<id>")
	BRIXWORLD_API FBrixLayout Stow(const FBrixLayout& Before, const FBrixLayout& After, const FBrixLayout& Belongings, const FString& Lot, int32 Level);
	BRIXWORLD_API FBrixLayout Apply(const FBrixFurnishRulesD& F, const FBrixLayout& Before, const TSharedPtr<FJsonObject>& Edit, const FBrixLayout& Belongings);   // {remove, move, add, place}
	BRIXWORLD_API TSharedPtr<FJsonObject> ToJson(const FBrixLayoutPiece& P, bool bWhere = true);
	BRIXWORLD_API FBrixLayoutPiece FromJson(const TSharedPtr<FJsonObject>& O);
	BRIXWORLD_API double Snap(const FBrixFurnishRulesD& F, double V);   // on the grid

	// play.js townPieces(): a storey's furniture (BrixFurnishFloor's boxes) as the pieces that can be moved, each with the
	// boxes that make it; pieces on a wall or hung from the ceiling stay (Fixed)
	struct FTown { FBrixLayout Pieces; TArray<TArray<int32>> Boxes; TArray<FVector2D> Centre; TArray<int32> Fixed; };
	BRIXWORLD_API FTown TownPieces(const FBrixFurnished& Furn);
	// the storey's boxes as a layout puts them: each town piece's boxes moved to where it now stands and turned; the fixed
	// ones as they were; pieces taken away left out
	BRIXWORLD_API void LayoutBoxes(const FBrixFurnished& Furn, const FTown& Town, const FBrixLayout& Layout, TArray<FBrixFurnBox>& OutBoxes);
}
