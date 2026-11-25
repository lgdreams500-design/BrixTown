#pragma once

#include "CoreMinimal.h"
#include "BrixStreets.h"

struct FBrixCity;
struct FBrixProp;
struct FBrixSignature;
struct FBrixPeopleRules;

// Children on the playgrounds (WLD-032; dressing.js init(), apply(), draw()): which play place is on which swing, slide,
// seesaw, sandpit or climbing frame; where a child at one is and how they sit, climb or slide at a moment; and the parts
// that move with them (swing seats on their chains, seesaw planks), swinging and rocking only while someone is on them.
// All from the day's places and the clock: nothing is stepped or stored. Checked against dressing.js itself by
// WLD032_children_play_as_the_slice_puts_them (Tools/unreal/streets_values.py).

struct BRIXWORLD_API FBrixPlayMotion   // World/rules/dressing.json "motion"
{
	double SwingPeriodS = 2.9, SwingAngleDeg = 32, SeesawPeriodS = 3.4, SeesawAngleDeg = 14, SlideCycleS = 9, ClimbCycleS = 8;
};

// Where a child is and how they hold themselves (dressing.js apply()'s r)
struct BRIXWORLD_API FBrixPlayPose
{
	double X = 0, Z = 0, Y = 0, Yaw = 0;   // the slice's frame; a sitting body's origin is the seat's top
	int32 Pose = 0;                         // 0 standing, 1 walking or climbing, 2 sitting
	double Phase = 0;
	bool bWalking = false;
	bool bClimb = false;                    // climbing: the Climb walk (BrixClimbSignature)
	bool bStraddle = false;                 // astride a seesaw's plank
	TOptional<FVector4d> Arms[2];           // people.js mod.arms: [weight, raise, out, elbow]
};

class BRIXWORLD_API FBrixPlay
{
public:
	enum EUse : int32 { Swing, Slide, Seesaw, Sandpit, Climb, NumUses };
	static const TCHAR* UseName(int32 U);   // the day's acts and the lots' places: swing, slide, seesaw, sandpit, climb
	struct FPiece { FString Id; int32 Use = 0; double X = 0, Z = 0, G = 0; int32 N = 0; };
	struct FSpot { int32 Piece = -1, K = 0, Use = 0; double X = 0, Z = 0; };
	TArray<FPiece> Pieces;
	TArray<FSpot> Spots;
	FBrixPlayMotion Motion;

	// dressing.js prop(): a swing, slide, seesaw, sandpit or climbing frame of the block files, standing on ground G
	void AddPiece(const FBrixProp& P, double G);
	// dressing.js init(): each play place of the lots (their objects swing, slide, seesaw, sandpit, climb) on its nearest
	// piece of that kind within 8 m
	void Init(const FBrixCity& City);
	// dressing.js apply() for someone staying at a place for act Use (an EUse) at (X, Z), T seconds into the visit; StepLen
	// is their walk's step length (BrixStepLength). False when they are not at a play place. Marks what they are on as busy.
	bool Apply(int32 Resident, int32 Use, double X, double Z, double T, double StepLen, FBrixPlayPose& Out);
	// dressing.js keepOut(): someone walking is kept out of the climbing frames and slides (to the nearest edge)
	void KeepOut(double& X, double& Z) const;
	// dressing.js draw(): the swing seats and seesaw planks within 70 m of the eye, moving with whoever is on them since the
	// last draw; then forgets who was on them
	void Draw(double EyeX, double EyeZ, double T, TArray<FBrixShape>& Out);
	double SwingAngle(const FPiece& P, int32 K, double T) const;
	double SeesawAngle(const FPiece& P, double T) const;

private:
	TSet<FString> Busy;
	TMap<FIntPoint, TArray<int32>> Grid;   // spots by metre cell
	const FSpot* SpotAt(double X, double Z) const;
	TArray<FVector2D> BackTo(const FPiece& P) const;
};

BRIXWORLD_API bool LoadBrixPlayMotion(const FString& StreetsJson, FBrixPlayMotion& Out, FString& OutError);
// dressing.js climbSig(): a walk made into climbing (short steps, knees high, no sway), from the Careful walk; added to the
// people's rules as "Climb"
BRIXWORLD_API void BrixAddClimbSignature(FBrixPeopleRules& Rules);
