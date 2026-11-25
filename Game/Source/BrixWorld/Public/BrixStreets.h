#pragma once

#include "CoreMinimal.h"

struct FBrixProp;
struct FBrixBlock;

// The streets' and gardens' things as the slice draws them (Prototype/play/play.js plant(), lawn(), greenGround(),
// wildProp(); dressing.js): a port that gives the same drawing calls with the same numbers, so the Unreal town can lay
// them as its own shapes. Everything is in the slice's frame (x east, y up, z south, metres) and in doubles, as the slice
// works in JavaScript numbers: its hash() is fed the same strings, so a bush gets the same flowers here as there.
// Checked against the slice's own functions by CITY017_every_plant_is_drawn_as_the_slice_draws_it and
// WLD056_the_open_ground_is_drawn_as_the_slice_draws_it (Tools/unreal/streets_values.py runs them in headless Edge).

// One drawing call of the slice's Mesh (or one of its helpers that the Unreal town draws its own way)
struct BRIXWORLD_API FBrixShape
{
	enum EKind : uint8
	{
		Box,      // A: x0, y0, z0, x1, y1, z1 (corner to corner)
		YBox,     // A: cx, y0, cz, w, h, d, yaw (standing on y0, turned by yaw as the slice's M4.ry)
		Flat,     // A: x0, z0, x1, z1, y (a level rectangle)
		Dome,     // A: x, y0, z, R, H, seed (play.js dome(): a low faceted crown)
		Quad,     // A: the four corners (x, y, z) x 4, then the hint normal (x, y, z): one face, its side chosen by the hint
		Ribbon,   // A: width, y; Pts: the line (play.js ribbon())
		Disc,     // A: x, z, r, y (play.js disc(): 18 wedges)
		Label,    // A: x, y, z (its middle), nx, nz (the way it faces), h (letter height); Text
		Solid,    // A: x0, y0, z0, x1, y1, z1: in the way, never drawn (play.js solid())
		OBox,     // A: the slice's matrix m (16, column by column), then the box's middle cx, cy, cz and size sx, sy, sz in its frame (Mesh.obox())
	};
	EKind Kind = Box;
	FVector3d Col = FVector3d::ZeroVector;
	TArray<double, TInlineAllocator<16>> A;
	TArray<FVector2D> Pts;
	FString Text;
	FString WrapKind;                        // part of a thing of its own that a car can break (crates by an arch: "crates")
	FVector2D WrapAt = FVector2D::ZeroVector;
};

// The wall a board is on (dressing.js board()): its lot's footprint, whether its ground storey is the old quarters' stone
// (play.js l.old: the kit's stone districts), its height (play.js building(): max(floors x 3 + 0.4, 3.6))
struct BRIXWORLD_API FBrixBoardWall
{
	FVector2D Min = FVector2D::ZeroVector, Max = FVector2D::ZeroVector;
	bool bOld = false;
	double H = 0;
};

// How one kind of garden plant looks today (world.plants_today(): CITY-017, WLD-040)
struct BRIXWORLD_API FBrixPlantLook
{
	double Size = 1, Leaf = 1, Bloom = 0, Spent = 0, Height = .5;
	FVector3d Color = FVector3d(.3, .33, .2), BloomColor = FVector3d::ZeroVector, SpentColor = FVector3d::ZeroVector;
	bool bEvergreen = false;
};

// A tree as it is today (build_play.py tree_rows(), the slice's D.trees: WLD-056): where, its crown's radius, how much leaf,
// its leaves' colour, in blossom, fallen, and its crown's shape (0 round, 1 a pine's cone, 2 a birch's slim crown)
struct BRIXWORLD_API FBrixTreeToday
{
	double X = 0, Z = 0, R = 2.5, Leaf = 1;
	FVector3d Col = FVector3d(.3, .33, .2);
	bool bBlossom = false, bFallen = false;
	int32 Crown = 0;
};

// The road network as the slice draws it (build_play.py roads(), the slice's D.roads: CITY-043, CITY-045, CITY-091)
struct BRIXWORLD_API FBrixRoads
{
	struct FMark { double X = 0, Z = 0; bool bAlongX = false; };     // a zebra (bAlongX: on an east-west street) or a lay-by
	struct FStop { double X = 0, Z = 0, Ux = 1, Uz = 0; bool bTown = true; TArray<int32> Lines; };   // a bus stop, the way the bus goes
	struct FSignal { double X = 0, Z = 0, Offset = 0; };              // a junction with traffic lights, and where in its cycle it starts
	TArray<FMark> Crossings, Bays;
	TArray<FStop> Stops;
	TArray<FString> LineColours;                                      // each bus line's colour's name
	TArray<FSignal> Signals;
	double GreenS = 20, AmberS = 3, AllRedS = 2;                      // traffic.json "signals"
	double Width = 3, BayLength = 6;                                  // a zebra's width, a lay-by's length
};

// What the streets and gardens look like today: Game/Data/streets.json, written by Tools/worldgen/build_play.py for the
// same day as the slice's city_data.js (D.plants, D.flowers, D.lawn, D.dressing)
struct BRIXWORLD_API FBrixStreetsToday
{
	TMap<FString, FBrixPlantLook> Plants;
	double FlowerSize = 0, FlowerBloom = 0;   // the flowerbeds and planters (D.flowers)
	TArray<FVector3d> FlowerColors;
	// the lawn (World/rules/vegetation.json "lawn", ART-001)
	TArray<FVector3d> LawnTones;
	double EdgeM = .45, TuftsPer100m2 = 7, TreeRingM = 1.2;
	FVector3d EdgeColor = FVector3d(.23, .31, .16), TuftColor = FVector3d(.24, .32, .16), MulchColor = FVector3d(.2, .15, .11);
	// the street dressing today (world.dressing_today(): WLD-034): each board's advert, the bunting on market days
	TSharedPtr<class FJsonObject> Dressing;
	bool bBunting = false;
	TArray<FBrixTreeToday> Trees;   // every tree today (D.trees)
	FBrixRoads Roads;               // the road network (D.roads)
	double Pace = 10;               // the clock's pace: real seconds a game minute (D.pace)
	int32 Day = 16;                 // the day it is written for
};
BRIXWORLD_API bool LoadBrixStreetsToday(const FString& Path, FBrixStreetsToday& Out, FString& OutError);

// play.js's M4: 4 x 4 matrices, column by column, as the slice's Mesh.obox() takes them
struct BRIXWORLD_API FBrixM4
{
	double M[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	static FBrixM4 T(double X, double Y, double Z);
	static FBrixM4 Rx(double A);
	static FBrixM4 Ry(double A);
	static FBrixM4 Rz(double A);
	FBrixM4 operator*(const FBrixM4& B) const;   // M4.mul(this, b)
	FVector3d Pt(double X, double Y, double Z) const;
};

// play.js's palette (C) and the open ground's own colours
namespace BrixSlice
{
	BRIXWORLD_API extern const FVector3d Stone, StoneDark, Paving, Path, Grass, Water, Brown, Olive, Plum, Charcoal, Cream, Ochre,
		Terracotta, Teal, Blue, Lantern, Metal, Soil, Wood;
	// String(v) of a JavaScript number, so hash() is fed what the slice feeds it
	BRIXWORLD_API FString Js(double V);
	// play.js hash() of the parts as the slice writes them (numbers through Js())
	BRIXWORLD_API double Hash(std::initializer_list<FString> Parts);
	inline double JsRound(double V) { return FMath::FloorToDouble(V + 0.5); }   // Math.round
}

// The heights the slice's functions read (play.js Y_WILD, Y_GRASS and groundBase()): the Unreal town passes its own
struct BRIXWORLD_API FBrixGround
{
	double Here = .12;     // groundBase() at the thing
	double Wild = -.03;    // the open ground (Y_WILD)
	double Grass = .15;    // a lawn in town (Y_GRASS)
};

// play.js plant(): a hedge, a bush, a border or a drift as today's season has it (CITY-017, WLD-040)
BRIXWORLD_API void BrixPlantShapes(const FBrixProp& P, const FBrixGround& G, const FBrixStreetsToday& Today, TArray<FBrixShape>& Out);
// play.js wildProp(): the open ground's own things (WLD-056): fences, "For sale" boards, pegs, logs, rocks, picnic tables,
// hay bales, crops, ponds. False for any other kind.
BRIXWORLD_API bool BrixWildShapes(const FBrixProp& P, const FBrixGround& G, TArray<FBrixShape>& Out);
// play.js lawn() (ART-001): the grass, its longer edge, and tufts the mower missed, none on a path or under a thing
BRIXWORLD_API void BrixLawnShapes(double X0, double Z0, double X1, double Z1, double Y, const FVector3d& Col, const FBrixBlock& B, bool bTufts,
	const FBrixStreetsToday& Today, TArray<FBrixShape>& Out);
// play.js greenGround() (WLD-056): a wood's leaf litter or a lawn over the whole block, and its tracks
BRIXWORLD_API void BrixGreenGroundShapes(const FBrixBlock& B, const FBrixGround& G, const FBrixStreetsToday& Today, TArray<FBrixShape>& Out);
// dressing.js prop() for the playgrounds' still parts (WLD-032): the soft ground, the swings' frames, the slides, the
// seesaws' pivots, the sandpits and the climbing frames (their seats and planks move: BrixPlay). False for any other kind.
BRIXWORLD_API bool BrixDressShapes(const FBrixProp& P, const FBrixBlock& B, double G, const FBrixStreetsToday& Today, TArray<FBrixShape>& Out);
// The rest of dressing.js prop() (WLD-028, WLD-031, WLD-034): the bandstand, notice boards, sundials, wells, pergolas, statues,
// the news kiosk, bunting on market days, planters, bins, bike racks, post and telephone boxes, bollards, timetables,
// adverts on blank walls and hoardings (on Wall, the wall of the lot the block file names). False for any other kind.
BRIXWORLD_API bool BrixMoreDressShapes(const FBrixProp& P, const FBrixBlock& B, double G, const FBrixStreetsToday& Today, const FBrixBoardWall* Wall, TArray<FBrixShape>& Out);
// The wall of the lot a board names (its 'lot'), or unset; OldDistricts: those whose ground storey is stone
BRIXWORLD_API TOptional<FBrixBoardWall> BrixBoardWallOf(const FBrixBlock& B, const FBrixProp& P, const TArray<FString>& OldDistricts);
// dressing.js board(): where an advert or a hoarding is on its wall: the side (n, s, w, e), its ends along the wall and its
// bottom and top (metres from the street). False for any other kind.
struct BRIXWORLD_API FBrixBoardSpan { TCHAR Side = 'n'; double A0 = 0, A1 = 0, Y0 = 0, Y1 = 0; };
BRIXWORLD_API bool BrixBoardSpanOf(const FBrixProp& P, const FBrixBoardWall& Wall, FBrixBoardSpan& Out);
// play.js prop() for an arch across an alley (and the crates by some), on ground G
BRIXWORLD_API void BrixArchShapes(const FBrixProp& P, const FBrixBlock& B, double G, TArray<FBrixShape>& Out);
// play.js crates(): goods stacked by a door, from the seed's hash, marked as a thing of their own ("crates")
BRIXWORLD_API void BrixCratesShapes(double X, double Z, double G, const FString& Seed, TArray<FBrixShape>& Out);
// Mesh.obox() as a shape
BRIXWORLD_API void BrixOBox(TArray<FBrixShape>& Out, const FBrixM4& M, double Cx, double Cy, double Cz, double Sx, double Sy, double Sz, const FVector3d& Col);
// The open ground's templates (play.js GREEN_KINDS)
BRIXWORLD_API bool BrixIsWildTemplate(const FString& Template);
// The slice's dome() as a mesh: rings of points (M + 1 rings of N), for a crown R = 1, H = 1 standing at the origin, with
// the seed's own jitter but not its turn (the town turns each one by its seed), and each facet's shade (0.74..1.38)
BRIXWORLD_API void BrixDomeMesh(double Seed, TArray<FVector3d>& OutTris, TArray<double>& OutShade);
// A lawn as the slice keeps it for the rings round trunks (play.js LAWNS): its rectangle and its height
struct BRIXWORLD_API FBrixLawn { double X0 = 0, Z0 = 0, X1 = 0, Z1 = 0, Y = 0; };
// The lawns the slice lays, in its order (play.js drawBase(): the open ground's grass over a whole block but in the woods,
// a terrace's back gardens, a park, each garden lot), at the heights given
BRIXWORLD_API TArray<FBrixLawn> BrixLawnsOf(const TArray<FBrixBlock>& Blocks, double Pitch, double YWild, double YGrass, double YGarden);
// play.js tree() (WLD-056): a trunk and a faceted crown (round, a pine's tiers of cones, a birch's slim crown on a white trunk
// with a dark band), a fallen tree as a log, and the ring of mulch round a trunk standing in a lawn; on ground G. Lite: a
// tree in the woods, with fewer facets and no ring.
BRIXWORLD_API void BrixTreeShapes(const FBrixTreeToday& T, double G, bool bLite, const TArray<FBrixLawn>& Lawns, double YWild, double YGrass,
	const FBrixStreetsToday& Today, TArray<FBrixShape>& Out);

// roads.js (CITY-043, CITY-045, CITY-046, CITY-091): the block squares (x, z of the pitch's grid) that hold any of the road
// network, and in each its zebras, lay-bys, bus stops and traffic-light poles with their dark heads; Y the paving's height,
// Ground the ground under a bus stop's shelter out of town. Heads: each signal head's place, for its lit lamp.
struct BRIXWORLD_API FBrixSignalHead { double X = 0, Z = 0; bool bEw = false; int32 Signal = 0; double Ux = 0, Uz = 0; };
BRIXWORLD_API TArray<FIntPoint> BrixRoadCells(const FBrixRoads& R, double Pitch);
BRIXWORLD_API void BrixRoadShapes(const FBrixRoads& R, double Pitch, double StreetWidth, const FIntPoint& Cell, double Y,
	TFunctionRef<double(double, double)> Ground, TArray<FBrixShape>& Out, TArray<FBrixSignalHead>& Heads);
// traffic.py signal() as roads.js phase(): 0 green, 1 amber, 2 red for a head on the east-west (bEw) or north-south street
// at the clock's Minutes of day Day (past 1440 on the days after)
BRIXWORLD_API int32 BrixSignalPhase(const FBrixRoads& R, int32 Signal, bool bEw, int32 Day, double Minutes, double Pace);
// roads.js draw(): a head's lit lamp in that phase (a box, in the lamp's own colour)
BRIXWORLD_API FBrixShape BrixSignalLamp(const FBrixSignalHead& H, int32 Phase);

// A sum of everything a list of shapes says (corners, sizes, colours, in order), to compare two lists in one number
BRIXWORLD_API double BrixShapesChecksum(const TArray<FBrixShape>& Shapes);
