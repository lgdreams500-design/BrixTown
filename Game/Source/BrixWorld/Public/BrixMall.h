#pragma once

#include "CoreMinimal.h"

class FJsonObject;

// Eastgate Mall's inside as the slice builds it (WLD-058 to WLD-068; ADR-0038, ADR-0039, ADR-0041): a port of
// Prototype/play/court.js, shell.js and outlets.js that gives the same drawing calls with the same numbers, so the Unreal
// town can lay them as its own faces, solids and places. What is where is data: the lot's `plan` (concept "court") and
// `shell` in its block file (World/city_01/blocks/b_16_5.json). Only a mall whose plan is a court with its entrances has
// an inside (court.js can()): the other malls are left as they are. Everything is in the slice's frame (x east, y up,
// z south, metres) and in doubles, its hash() fed the same strings. Checked against the slice's own code by
// WLD058_eastgate_is_built_as_the_slice_builds_it (Tools/unreal/mall_values.py runs it in headless Edge).

// One call of the slice's mesh or of the lot's hooks (play.js mallShell()'s `o`)
struct BRIXWORLD_API FBrixMallCall
{
	enum EKind : uint8
	{
		Quad,    // A: four corners (x, y, z) x 4, the hint normal (x, y, z), 1 if there is a hint, 1 if it is a lit window's face (uv)
		QuadN,   // A: four corners, four normals, four "colours" (x 3 each): a face of a curved surface (Mesh.quadN())
		Box,     // A: x0, y0, z0, x1, y1, z1; Faces: [face, r, g, b, material] or [face] (left out), as Mesh.box()'s `o`
		YBox,    // A: cx, y0, cz, sx, sy, sz, yaw (Mesh.ybox())
		Solid,   // A: x0, y0, z0, x1, y1, z1: in the way, never drawn
		Use,     // A: x, y, z, yaw, then where it takes you (x, y, z) for a lift; Text: "kind|label" (play.js use())
		Spot,    // A: x, z, pose (0 stand, 2 sit), yaw, height (-1: none): a place for a resident (play.js spot())
		Lamp,    // A: x, y, z, strength: a lamp that lights the room (play.js ROOM_REC lamps; the furniture outlet's)
		Light,   // A: x, y, z, reach, strength: a lantern's light outside (shell.js)
		Name,    // A: the middle (x, y, z), the way it faces (x, y, z), the letters' height; Text: the name (an outlet's sign)
		Occl,    // A: x0, z0, x1, z1, height, 0: ground that sees less sky
		OBox,    // A: the slice's matrix (16, column by column), then the box's middle and size in its frame (Mesh.obox())
		Car,     // A: x, z, yaw, y; Text: the seed its model and paints come from (cars.js parked())
		Piece,   // A: x, y, z, the piece's across axis (x, z), its back axis (x, z); Text: "piece|line": a furniture kit's piece
		Good,    // A: x, y, z, yaw, lod (0 near, 1 far); Text: "piece|seed": a mall good for the home as made in Blender (goods.js put())
	};
	EKind Kind = Quad;
	TArray<double, TInlineAllocator<17>> A;
	FVector3d Col = FVector3d::ZeroVector;
	bool bCol = false;
	int32 Mat = 0;                                         // the slice's material (play.js Mesh's list)
	TArray<TArray<double, TInlineAllocator<5>>> Faces;
	FString Text;
};

// An outlet's room, for buying in it (outlets.js Outlets.at, WLD-066)
struct BRIXWORLD_API FBrixOutletRoom
{
	FString Id, Name, Kind;     // the unit, what it is called, its outlet kind (World/rules/mall_outlets.json)
	int32 Level = 0;
	double Fy = 0, X = 0, Z = 0;   // its floor; the start of its front
	FVector2D T = FVector2D::ZeroVector, N = FVector2D::ZeroVector;   // along the front; out of the room
	double A0 = 0, A1 = 0, D = 0;  // its side walls (metres along), its depth
};

// A moving stair (court.js carry())
struct BRIXWORLD_API FBrixMovingStair
{
	FVector2D A = FVector2D::ZeroVector, T = FVector2D::ZeroVector, S = FVector2D::ZeroVector;
	double Lr = 0, Ya = 0, Yb = 0, Hw = 0, V = 0;
};

struct BRIXWORLD_API FBrixMall
{
	FString LotId, Name;
	double X0 = 0, Z0 = 0, X1 = 0, Z1 = 0;   // the hall's rect
	double H = 18, StoreyHeight = 6, F = .2; // its height, its storeys, the ground floor's top (play.js Y_FLOOR)
	int32 Levels = 3;
	TArray<FBrixMallCall> Calls;
	TArray<FBrixOutletRoom> Rooms;
	TArray<FVector4d> Lamps;                 // every lantern and shop light (x, y, z, strength), court.js lamps
	TArray<FBrixMovingStair> Moving;
	TSet<FString> Staffed;                     // the outlets whose keeper works here (the lot's places to work): each has its counter
	TMap<FString, FString> KindLabel, KindSign;   // each outlet kind's label and sign colour (plan.outlets)
};

struct FBrixEconomyRules;
struct FBrixGoodsModels;
// What outlets.js front() reads to stand an outlet's goods for the home in its windows and on its floor (FURN-065): the
// economy's rules for what each outlet sells (Econ.outletRange, World/rules/mall_goods.json in life.json) and the goods'
// models (their sizes). Without it the windows show their stand-ins, as the slice does without its life data.
struct BRIXWORLD_API FBrixMallGoods
{
	const FBrixEconomyRules* Rules = nullptr;
	const FBrixGoodsModels* Models = nullptr;
};

// court.js can(): the lot's plan is a court with its entrances, and it has a ribbon shell (shell.js can())
BRIXWORLD_API bool BrixMallCan(const FJsonObject& Lot);
// play.js mallShell() for such a lot: Court.hall(), Shell.draw(), Court.draw()
BRIXWORLD_API bool BuildBrixMall(const FJsonObject& Lot, FBrixMall& Out, FString& OutError, const FBrixMallGoods* Goods = nullptr);
// The block files' one mall with its inside built (the first lot BrixMallCan() takes), built. False when there is none.
BRIXWORLD_API bool LoadBrixMall(const FString& CityDir, FBrixMall& Out, FString& OutError, const FBrixMallGoods* Goods = nullptr);
struct FBrixDay;
// The day's stays in the mall at its places (play.js resident(): the spot on the stay's floor, by the mall's own storey
// height; CITY-105, CITY-106, WLD-067, WLD-068): a keeper stands behind their outlet's counter, a guard or a janitor at their
// post, a visitor before a counter or on a food outlet's chair or a bench; each faces the way the plan has them face and is
// at that floor's height. A stay with no place there stands on the ground, as the slice's STAND (in the hall on its ground floor: the
// court's kiosks; or at the door). MallLot is the mall's
// index in the city's lots. Returns how many stays it placed.
BRIXWORLD_API int32 SeatBrixDayInMall(FBrixDay& Day, const FBrixMall& M, int32 MallLot);
// court.js carry(): someone at P (slice frame) on a moving stair is carried along it, up on the right-hand flight and down
// on the left; true when they were
BRIXWORLD_API bool BrixMallCarry(const FBrixMall& M, FVector3d& P, double Dt);
// The top of the moving stair's step under P (slice frame: its thirty low steps, court.js), or -1 off the stairs
BRIXWORLD_API double BrixMallStepUnder(const FBrixMall& M, const FVector3d& P);
// outlets.js at(): the outlet room a point is in, or null
BRIXWORLD_API const FBrixOutletRoom* BrixOutletAt(const FBrixMall& M, double X, double Y, double Z);
// A colour a mall's shell may name, by its number (shell.js TONES: ochre, cream, terracotta, teal, plaster)
BRIXWORLD_API FVector3d BrixMallShellTone(int32 Tone);
// The sum mall_values.py takes of a call
BRIXWORLD_API double BrixMallChecksum(const FBrixMallCall& C);
