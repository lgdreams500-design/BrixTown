#pragma once

#include "CoreMinimal.h"

// Residents getting out of the way of a car a player drives (CITY-070), then telling the driver off (CITY-071): a port of
// the slice's Prototype/play/dodge.js (F-NPC-DODGE level 2), with its names kept. Presented state only (LIVING_CITY §1):
// an offset and a pose laid over where the resident's day puts them, kept for a few seconds near the player and then
// forgotten; the day itself is never changed. Numbers from World/rules/dodge.json. Checked against dodge.js itself by
// CITY070_residents_dodge_and_scold_as_the_slice_does (values from Tools/unreal/people_values.py).
//
// The slice's frame: metres, x east, z south; a yaw faces (sin yaw, -cos yaw).

struct BRIXWORLD_API FBrixDodgeRules
{
	double MinSpeed = 2, SeeS = 1.7, SeeM = 26, LanePadM = .9, CarHalfWM = .9, CarFrontM = 2.3, ClearM = 1.7, StartleS = .14,
		ScrambleS = .5, HopM = .22, ScoldS = 2.4, ReturnS = 1.4, CooldownS = 1, FistShare = .55, TagM = 22;
	TArray<FString> Shouts;
};

BRIXWORLD_API bool LoadBrixDodgeRules(const FString& Path, FBrixDodgeRules& Out, FString& OutError);

// A resident as drawn (dodge.js apply()'s q): where their day has them, with any dodge laid on top.
struct BRIXWORLD_API FBrixDodged
{
	FVector2D Pos = FVector2D::ZeroVector;   // x, z
	double Hop = 0;                          // above the ground at Pos (the scramble's hop)
	double Yaw = 0;
	int32 Pose = 0;                          // 0 standing, 1 walking
	bool bWalking = false;
	double Phase = 0;
	FString Sig;                             // another walk (the scramble's Run), or empty
	TOptional<FVector4d> Arms[2];            // the gesture layers, as FBrixDrawOptions
	FVector2D Chest = FVector2D::ZeroVector;
	FString Shout;                           // the line shown over them, or empty
};

class BRIXWORLD_API FBrixDodge
{
public:
	FBrixDodgeRules R;
	bool bOn = false;

	// once a frame (dodge.js frame()): the time in seconds and the player's car, if they are driving one: where it is, which
	// way its front faces (x, z), its height and its speed in m/s (negative backing up)
	void Frame(double T, bool bDriving, const FVector2D& CarPos, const FVector2D& CarHeading, double CarY, double Speed);
	// resident i where their day has them (dodge.js apply(r)): false when there is nothing to lay on; else Out is the
	// resident as drawn. Clear(x, z): somewhere to stand there (not a wall, not a step up).
	bool Apply(int32 i, const FVector2D& Pos, double Y, double Yaw, int32 Pose, bool bInside, TFunctionRef<bool(const FVector2D&)> Clear, FBrixDodged& Out);
	int32 Live() const { return States.Num(); }

private:
	struct FCar { double X, Z, Y, Hx, Hz, V; };
	struct FState { double T0; FVector2D From, To; int32 N; bool bFist; FString Shout; double Cx, Cz; };
	TOptional<FCar> Car;
	double Now = 0;
	TMap<int32, FState> States;
	FVector2D Off(const FState& S) const;
	bool Threat(double X, double Y, double Z, double& OutAlong, double& OutAcross) const;
	FState& Start(int32 i, const FVector2D& Pos, const FState* S, double W, TFunctionRef<bool(const FVector2D&)> Clear);
};
