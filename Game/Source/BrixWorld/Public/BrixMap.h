#pragma once

#include "CoreMinimal.h"
#include "BrixCity.h"

class FBrixQuests;
struct FBrixQuestRules;
class FBrixEconomy;

// The maps (UI-012, UI-018; F-UI-MENU 2): the town drawn from the city's data as the slice's play.js drawBase() draws it,
// the big map's zoom and pan (mapZoom(), mapClamp(), mapPx()), the player's mark (life.js markAt(), waypointTag()) and the
// people and places a quest wants (life.js targets(), QST-005). Only data and logic: BrixTown paints them.

// What the map needs from the data that FBrixCity does not keep: the line of the old town wall, the roads out of town
// (roads.json links and gates), and each block's lanes, paths, tracks, crops, ponds, fences, hedges, plots for sale and
// sculpted shells (WLD-056, WLD-057).
struct BRIXWORLD_API FBrixMapLine { TArray<FVector2D> Points; float Width = 1.f; };
struct BRIXWORLD_API FBrixMapShell
{
	TArray<FVector2D> Skin;                                   // the outline
	TArray<TPair<FVector2D, FVector2D>> Domes;                // centre, radii (x, z)
	TOptional<TPair<FVector2D, FVector2D>> RoofLight;
	struct FArch { FString Side; FVector2D At = FVector2D::ZeroVector; float Width = 4.f; };
	TArray<FArch> Arches;
};
struct BRIXWORLD_API FBrixMapBlock
{
	TArray<FBrixMapLine> Lanes, Tracks;
	TArray<TPair<FVector2D, FVector2D>> Paths;
	struct FCrop { FVector2D Pos, Size; FString Crop; };
	TArray<FCrop> Crops;
	TArray<TPair<FVector2D, float>> Ponds;                    // centre, radius
	TArray<TPair<FVector2D, FVector2D>> Fences, Hedges;       // ends
	TArray<FVector2D> Boards;                                 // a plot for sale
	TMap<FString, FBrixMapShell> Shells;                      // lot id -> its shell
};
struct BRIXWORLD_API FBrixMapExtras
{
	FIntRect Wall = FIntRect(0, 0, 0, 0);                     // the wall's cells (city.json wall.cells): i0, j0, i1, j1
	TArray<FBrixMapLine> Links;                               // roads.json links: out to the new areas
	TArray<TPair<FVector2D, FVector2D>> Gates;                // the roads out of Old Town: from the gate's junction to its end
	TArray<FBrixMapBlock> Blocks;                             // in the city's block order
	TArray<FVector3d> Trees;                                  // today's standing trees (the slice's D.trees): x, z, crown radius;
};                                                            // empty: the city's trees at the one TreeRadius
BRIXWORLD_API bool LoadBrixMapExtras(const FString& CityDir, const FBrixCity& City, FBrixMapExtras& Out, FString& OutError);

// today's trees for the map: every standing tree of Game/Data/streets.json with its crown's radius (fallen ones are left off)
BRIXWORLD_API void BrixMapTreesToday(const TArray<struct FBrixTreeToday>& Trees, FBrixMapExtras& X);

// The map as a picture N pixels square (the slice's 900 and its 2700 three times as fine): K pixels a metre, O metres of
// margin, pixel = (world + O) * K. Colours are the slice's (play.js KIND_CSS, MAP_GREEN, MAP_CROP).
struct BRIXWORLD_API FBrixMapImage
{
	int32 N = 0;
	double K = 1, O = 20;
	TArray<FColor> Px;                                        // row by row, N x N
	FVector2D ToPx(const FVector2D& W) const { return (W + FVector2D(O, O)) * K; }
	FVector2D ToWorld(const FVector2D& P) const { return P / K - FVector2D(O, O); }
	FColor At(const FVector2D& W) const;                      // the pixel at a place
};
BRIXWORLD_API FColor BrixMapKindColour(const FString& Kind);   // KIND_CSS, "#999" for any other
BRIXWORLD_API FBrixMapImage BrixDrawMap(const FBrixCity& City, const FBrixMapExtras& X, int32 N, double TreeRadius = 2.5);
// the legend under the big map (play.js: KIND_WORD, MAP_WORDS, the residents and you)
BRIXWORLD_API TArray<TPair<FColor, FString>> BrixMapLegend();

// The big map's view (UI-018): zoomed Z times (1 to 6) about the point (Cx, Cy) of the 900-unit map at the middle.
struct BRIXWORLD_API FBrixMapView
{
	double Z = 1, Cx = 450, Cy = 450;
	double Size = 900, MinZ = 1, MaxZ = 6;    // the canvas's units across, the zoom's bounds
	void Clamp();                                             // mapClamp(): never past the map's edge
	FVector2D MapAt(const FVector2D& Screen) const;           // mapPx(): canvas units -> map units
	FVector2D ScreenAt(const FVector2D& Map) const;           // and back
	void Zoom(double F, const FVector2D& At);                 // mapZoom(): about a point of the canvas
	void Whole() { Z = 1; Clamp(); }                          // 0, or the middle button
};

// The player's mark (UI-012): set by a click on the big map (or from Properties); a click within 6 m of it clears it; it
// goes when the player comes within 4 m of it.
struct BRIXWORLD_API FBrixMark
{
	bool bSet = false;
	FVector2D Pos = FVector2D::ZeroVector;
	FString Name;
	// returns what to tell the player ("Marked: ...", "Mark cleared.")
	FString MarkAt(const FVector2D& At, const FString& PlaceName, bool bNamed);
	bool Reached(const FVector2D& Player);                    // true once, as it is reached (and then it is gone)
	FString TagText(const FVector2D& Player) const;           // "◆ <name> · 120 m" (1.2 km beyond 1000 m)
};

// The people and places a quest or a job wants now (life.js targets(); QST-005): a resident to see (Resident >= 0, Offer
// when they have a quest line to give), or a lot to go to, or a place. Kind says what for, for its marker (BrixMarkers.h):
// "offer", "talk", "handin" (going back to someone the line started with, or to its giver), "job", "go", "bank", "stall".
// A step or a job that needs the player to have something marks every shop with it in stock today (Item says what).
struct BRIXWORLD_API FBrixMapTarget
{
	int32 Resident = -1, Lot = -1;
	FVector2D Pos = FVector2D::ZeroVector;
	bool bOffer = false;
	FString Kind, Item;
};
BRIXWORLD_API TArray<FBrixMapTarget> BrixQuestTargets(const FBrixQuests& Q, const FBrixQuestRules& Rules, const FBrixEconomy& E);
