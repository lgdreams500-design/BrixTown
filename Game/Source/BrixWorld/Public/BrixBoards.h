#pragma once

#include "CoreMinimal.h"
#include "BrixBody.h"

// Skateboards (F-VEH-SKATES level 2; CITY-098 to CITY-101): World/rules/boards.json, and which boards the player owns, the
// board in use, whether it is carried in the hand or ridden and how fast it goes: a port of the slice's boards.js. A manual
// board goes only while a foot pushes it (W held); the electric and Neon Boost boards drive themselves while W is held.
// The skate store and buying (CITY-102, CITY-103) come later; until then only a test (the slice's ?auto=1) gives boards.
struct BRIXWORLD_API FBrixBoardRules
{
	TArray<FString> Order, AutoOwned;          // the kinds up the store; what a test owns
	FString AutoEquipped;
	double IndoorTopKmh = 8, MallTopKmh = 30, PushM = 2.2, PushPerS = 1.3, TurnRate = 3, CamFollow = 3;
	TMap<FString, FBrixBoardKind> Kinds;
	const FBrixBoardKind* Kind(const FString& Id) const { return Kinds.Find(Id); }
};
BRIXWORLD_API bool LoadBrixBoards(const FString& Path, FBrixBoardRules& Out, FString& OutError);

struct BRIXWORLD_API FBrixBoards
{
	const FBrixBoardRules* R = nullptr;
	TArray<FString> Own;                       // in the order of the store
	FString InUse;                             // empty: none
	bool bRidingRaw = false;
	double V = 0;                              // m/s

	const FBrixBoardKind* Kind() const { return R && !InUse.IsEmpty() ? R->Kind(InUse) : nullptr; }
	bool Riding() const { return bRidingRaw && Kind(); }
	// CITY-099: the speed after Dt seconds. W speeds up to the kind's top speed, or the limit where the rider is (CapKmh: indoors,
	// higher in a mall; < 0 none), letting go glides to a stop, S brakes; going faster than allowed (just come indoors) brakes down to it.
	double Step(double Dt, bool bGo, bool bBack, double CapKmh = -1);
	void Give(const TArray<FString>& List, const FString& Use);   // boards.js give()
	void Use(const FString& K);                                   // another owned board made the one in use
	TOptional<bool> Toggle();                                     // Y: from the hand onto the board and back; unset with none
	void Slow(double F) { V *= F; }                               // a knock against a wall or a car
};
