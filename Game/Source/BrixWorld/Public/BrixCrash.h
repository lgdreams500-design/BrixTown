#pragma once

#include "CoreMinimal.h"
#include "BrixDodge.h"

// Crash damage (LIVING_CITY CITY-072 to CITY-076; F-VEH-CRASH level 2): the rules of the slice's Prototype/play/smash.js
// in C++, with its names kept, and Tools/citysim/repairs.py's repair rule. Numbers from World/rules/crash.json.
// A street thing, a tree or a building corner the player's car hits breaks at any speed (break_kmh 0): tall things topple
// the way they were hit, low things burst into pieces in their own colours that bounce and settle as rubble, a corner
// loses chips of stone. Damage is stored state (CITY-074): a list of what broke, where and when; when each is mended is
// worked out from that list alone (CITY-075). Residents near a smash turn to it and shout (CITY-076).
//
// The slice's frame: metres, x east, y up, z south.

struct BRIXWORLD_API FBrixCrashKind
{
	FString How;            // topple, shatter or chip
	double Slow = 1;        // the share of its speed the car keeps
	double WorkMin = 30;    // minutes a crew works at it
};

struct BRIXWORLD_API FBrixCrashRules
{
	double BreakKmh = 0;
	TMap<FString, FBrixCrashKind> Kinds;
	TMap<FString, FString> Aliases;
	int32 Pieces = 14, RestPieces = 6;
	double LifeS = 2.2, Gravity = 9.8, Bounce = .35, ToppleS = 1.4, CornerM = 1.3, ReactM = 18, ReactS = 3;
	TArray<FString> Shouts;
	int32 CrewsPerDepot = 2;
	double Start = 420, End = 1140, ReportMin = 10, WalkMps = 1.14, Detour = 1.35;   // minutes of the day; m/s
	// smash.js kindOf(): a prop's kind as crash.json names it, or empty when it does not break
	FString KindOf(const FString& Kind) const;
};

BRIXWORLD_API bool LoadBrixCrashRules(const FString& Path, FBrixCrashRules& Out, FString& OutError);

// One thing broken (smash.js damage): its id (kind@x,z, or corner@lot:corner), kind, game minute since day 0's midnight,
// where it stood, the way the car went (a topple's direction), and for a corner, which way out of the building it faces.
struct BRIXWORLD_API FBrixDamage
{
	FString Id, Kind;
	double At = 0;
	FVector2D Pos = FVector2D::ZeroVector, Dir = FVector2D(1, 0), Out = FVector2D::ZeroVector;
};

// When a crew mends it (repairs.py repairs(), smash.js replan()): game minutes since day 0.
struct BRIXWORLD_API FBrixRepair
{
	FString Crew;
	double Leave = 0, Arrive = 0, Done = 0;
};

// CITY-075: Depots are (lot id, door), the town's enterable utility lots; Pace real seconds a game minute.
BRIXWORLD_API TMap<FString, FBrixRepair> BrixRepairs(const TArray<FBrixDamage>& Damage, TArray<TPair<FString, FVector2D>> Depots, const FBrixCrashRules& R, double Pace);

// CITY-074: the damage list as it is kept (a JSON array) and read back.
BRIXWORLD_API FString BrixDamageToJson(const TArray<FBrixDamage>& Damage);
BRIXWORLD_API bool BrixDamageFromJson(const FString& Text, TArray<FBrixDamage>& Out);

// smash.js draw(): how far a toppled thing has fallen, E seconds after the hit (radians from upright; it eases over,
// lies a little short of flat and bounces once). A negative E: long fallen (from a save).
BRIXWORLD_API double BrixToppleAngle(const FBrixCrashRules& R, double E);

// A piece of a broken thing (smash.js burst(), rubble(), pieces()): a small turned box flying, bouncing, then at rest.
struct BRIXWORLD_API FBrixPiece
{
	FVector P = FVector::ZeroVector, V = FVector::ZeroVector;   // x, y, z and its speed
	double S = .2, H = .1, Yaw = 0, Spin = 0;
	FVector3f C = FVector3f(.5f);
	bool bRest = false;
};

// smash.js burst(): N pieces flying from (x, y, z) with the car's (vx, vz), of about Size, in the colours Cols, from Seed
BRIXWORLD_API void BrixBurst(TArray<FBrixPiece>& Out, const FVector& At, double Vx, double Vz, double Size, const TArray<FVector3f>& Cols, int32 N, int32 Seed);
// smash.js rubble(): RestPieces at rest around (x, z), the same every time for the same seed (a damage from a save)
BRIXWORLD_API void BrixRubble(TArray<FBrixPiece>& Out, const FBrixCrashRules& R, double X, double Z, double Size, const TArray<FVector3f>& Cols, double Seed, TFunctionRef<double(double, double)> Ground);
// smash.js pieces(): Dt seconds of flight, bouncing on the ground until they rest
BRIXWORLD_API void BrixStepPieces(TArray<FBrixPiece>& P, const FBrixCrashRules& R, double Dt, TFunctionRef<double(double, double)> Ground);

// CITY-076 (smash.js apply()): resident i at Pos, a smash at (Sx, Sz) S seconds ago: turned to it, both arms flung up and
// shaken, and a shout while it lasts. False when they are too far or it is too long ago.
BRIXWORLD_API bool BrixCrashReact(const FBrixCrashRules& R, int32 i, const FVector2D& Pos, double Sx, double Sz, double Since, double SmashT, double Now, FBrixDodged& Q);
