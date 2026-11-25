#pragma once

#include "CoreMinimal.h"

class FJsonObject;
struct FBrixGoodsModels;

// Buy menus that show what is for sale as objects (UI-020, UI-021; ADR-0044): what each thing shows, and the stand-ins as
// meshes, as the slice's buymenu.js has them (World/rules/buy_menu.json, carried in Game/Data/life.json). What a thing shows
// is world.buy_menu_shows of Tools/worldgen/world.py, checked against it by UI021_every_thing_shows_what_python_says.

// A thing's picture: its own object (a piece of furniture, or a mall good for the home) or a stand-in of the rules.
struct BRIXWORLD_API FBrixShows
{
	FString Kind;                 // "piece", "shape", or empty: nothing to show
	FString Name;                 // the piece, or the stand-in's shape (empty when the rules name none)
	TOptional<FString> Colour;    // a shape's own colour: the designer's palette name of the clothing's colour
};

// Triangles of one thing to draw, x across, y up, z towards the viewer (buymenu.js boxMesh(), finish()).
struct BRIXWORLD_API FBrixThingMesh
{
	TArray<FVector3f> P, N, C;    // three of each per triangle: corners, the face's normal, linear colour
	FVector3f Lo = FVector3f::ZeroVector, Hi = FVector3f::ZeroVector, Cen = FVector3f::ZeroVector;
	float R = .01f, Foot = 0.f;   // the ball it fits in, round its middle; its footprint's reach
	bool bStandIn = false;
	int32 Num() const { return P.Num() / 3; }
};

// One path of a mark in a buy menu's dots and chips (UI-020): SVG path data on a square 16 units across, filled, or stroked
// this wide (buymenu.js ICON, carried in life.json buy_menu.icons by build_play.py).
struct BRIXWORLD_API FBrixIconPath
{
	FString D;
	float Stroke = 0.f;   // 0: filled
};
// The path's outlines, one per subpath, curves flattened to straight pieces (M, L, H, V, Q, Z, absolute, as the marks use).
BRIXWORLD_API TArray<TArray<FVector2f>> BrixIconOutlines(const FString& D);
// How much of each pixel of a Px x Px picture of the mark is covered, 0 to 1, rows from the top: fills by the nonzero rule,
// strokes as the band within half their width of each straight piece, 4 x 4 samples a pixel.
BRIXWORLD_API TArray<float> BrixIconCoverage(const TArray<FBrixIconPath>& Paths, int32 Px);

struct BRIXWORLD_API FBrixBuyMenuRules
{
	struct FGoes { FString Name, Colour, To; };
	struct FBox { float U, Y, V, Su, Sy, Sv; FString Colour; float Shade = 1.f, TurnDeg = 0.f; };
	int32 NarrowPx = 560;
	int32 WidePanelPx = 960, CellPx = 112, StagePx = 96, ShowPx = 320, ShowStagePx = 250, ListPx = 470;
	float FovDeg = 24, PitchDeg = 17, StartDeg = -34, TurnS = 16, DragDegPx = .7f, StandInFill = .8f;
	TMap<FString, FGoes> Goes;
	TMap<FString, FLinearColor> Colours;              // linear
	TMap<FString, FString> Bands;                     // a shop's kind -> a colour name
	TMap<FString, TArray<FBox>> Shapes;
	TMap<FString, FString> StandInGoes, StandInThings;
	struct FWear { FString Part, Shape, ColourPart; };
	TArray<FWear> StandInWear;
	TMap<FString, TPair<FColor, FColor>> BoardColours; // settings.json menu.colours: a board and its lettering
	TMap<FString, FLinearColor> Palette;              // the designer's palette (a piece of clothing's own colour)
	TSet<FString> Pieces;                             // furnishing.json's pieces
	TSet<FString> Items;                              // economy.json's items
	struct FGood { FString Goes, Piece; TMap<FString, FString> Parts; };
	TMap<FString, FGood> Goods;                       // mall_goods.json's goods
	TMap<FString, TArray<FBrixIconPath>> Icons;       // where a thing goes -> its mark in a dot or a chip (buymenu.js ICON)

	TSharedPtr<const FBrixGoodsModels> Models;        // the mall's goods as made in Blender (Game/Data/goods_models.json, FURN-065)

	FBrixShows Shows(const FString& Thing) const;
	// A stand-in as boxes: Band is the seller's colour, Own the clothing's (buymenu.js shapeMesh()).
	FBrixThingMesh ShapeMesh(const FString& Shape, const FLinearColor& Band, const FLinearColor& Own) const;
	// A mall good for the home as it was made in Blender, near, in its piece's own colours (buymenu.js modelMesh()); empty
	// when there is no model of it.
	FBrixThingMesh ModelMesh(const FString& Piece) const;
	// The mesh for a thing at a counter (buymenu.js meshOf()): a piece with a model shows its model, a stand-in its boxes;
	// a piece of furniture without a model is not brought over yet: none.
	TOptional<FBrixThingMesh> MeshFor(const FString& Thing, const FString& BandColour) const;
	static void Finish(FBrixThingMesh& M);
};

// Reads the buy menu's rules from Game/Data/life.json (already parsed: FBrixLifeData::Json).
BRIXWORLD_API bool LoadBrixBuyMenu(const TSharedPtr<FJsonObject>& Life, FBrixBuyMenuRules& Out, FString& OutError);
