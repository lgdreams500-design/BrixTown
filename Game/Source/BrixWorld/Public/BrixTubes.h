#pragma once

#include "CoreMinimal.h"

// Glass travel tubes (CITY-108 to CITY-112; F-VEH-TUBE level 2; ADR-0076). A port of Tools/citysim/tubes.py: the network is
// map data (World/<city>/tubes.json), how a ride moves is World/rules/tubes.json. A ride is worked out from the seconds
// since it began, never stepped (TECH-004).
//
// Points are in metres as the map gives them: X and Y the map's x and z (east, south), Z the height over the ground.

struct BRIXWORLD_API FBrixTubeStop
{
	FString Id, Name;
	FVector2D Pos = FVector2D::ZeroVector;     // the middle of its floor
	FVector2D Face = FVector2D(0, 1);          // the way its door looks out, unit
	FVector2D Camera = FVector2D::ZeroVector;  // where the ride's camera stands beside the booth, metres from it (map data: clear of crowns and buildings); zero: behind_m along the street
};

struct BRIXWORLD_API FBrixTube
{
	FString A, B;                              // stop or junction ids
	TArray<FVector> Line;                      // from A's mouth to B's
};

struct BRIXWORLD_API FBrixTubeNet
{
	TMap<FString, FBrixTubeStop> Stops;
	TArray<FString> StopOrder;                 // as the file lists them
	TMap<FString, FVector> Junctions;
	TArray<FBrixTube> Tubes;
	struct FPost { FVector2D At; double Z; FVector2D Along; };   // a post on the pavement: where, the tube's middle over it, the tube's way there
	TArray<FPost> Posts;                       // map data (make_tubes_01.py: clear of the road and the street's things)
};

struct BRIXWORLD_API FBrixTubeRules
{
	double ClearanceM = 6;
	double DrawInS = 0.9, DrawInHeightM = 7, SpeedMps = 60, EaseS = 1.2, PopOutS = 1.2, PopOutArcM = 0.4, FloorM = 0.31, TurnWindowM = 2.5;
	double BendRM = 1.8, JunctionBendRM = 0.75;   // corners built and ridden as arcs; the turns from one tube into the next
	double BehindM = 6, AboveM = 2, FollowS = 0.35, MaxJumpM = 1.5;
	double TubeInnerM = 0.68;
	// the look, from the approved reference VehiclesGlassTube (Tools/blender/hours_50.py TUBE)
	double BoothR = 0.75, BoothH = 3.0, PlinthR = 1.0, PlinthH = 0.25, FunnelH = 0.7, RingEveryM = 4, PostEveryM = 14;
	double DoorDeg = 70;                       // the opening in the booth's glass, about Face
	FLinearColor Glass = FLinearColor(0.70f, 0.86f, 0.88f), Brass = FLinearColor(0.62f, 0.48f, 0.26f);
	double GlassAlpha = 0.16, GlassEdge = 0.15;
};

BRIXWORLD_API bool LoadBrixTubeNet(const FString& Path, FBrixTubeNet& Out, FString& OutError);
BRIXWORLD_API bool LoadBrixTubeRules(const FString& Path, FBrixTubeRules& Out, FString& OutError);

// The line with each corner turned on an arc of radius R (tubes.py rounded()): what a tube is built along and ridden.
BRIXWORLD_API TArray<FVector> BrixTubeRounded(const TArray<FVector>& Line, double R, double StepDeg = 10.0);

// Where a node's tube ends: over a stop's floor at DrawInHeightM, or a junction's own point.
BRIXWORLD_API FVector BrixTubeMouth(const FBrixTubeNet& Net, const FBrixTubeRules& R, const FString& Node);

// What is wrong with the network (CITY-108): empty when nothing.
BRIXWORLD_API TArray<FString> BrixTubeProblems(const FBrixTubeNet& Net, const FBrixTubeRules& R);

// The shortest way from stop A to stop B through the tubes, passing junctions and never another stop (CITY-112); empty if none.
BRIXWORLD_API TArray<FString> BrixTubeRoute(const FBrixTubeNet& Net, const FString& A, const FString& B);

enum class EBrixRidePhase : uint8 { DrawIn, Run, PopOut, Out };

struct BRIXWORLD_API FBrixRide
{
	TArray<FString> Nodes;
	TArray<FVector> Run;                       // the tube's line, stop to stop
	TArray<double> Cum;                        // metres along Run at each point
	TArray<FVector> Way;                       // the whole way: the first booth's floor, Run, the last booth's floor, where they stand
	TArray<double> WayCum;
	double UpM = 0, DownM = 0;                 // from the floor to the tube's mouth; from it to where they stand
	FVector2D SideA = FVector2D(6, 0), SideB = FVector2D(6, 0);   // where the camera stands beside the first and last booth, metres from it
	FVector FloorA, FloorB, OutB;
	FVector2D FaceA, FaceB;
	double Acc = 0, V0 = 0, V1 = 0, Ta = 0, Tc = 0, Tb = 0, VTop = 0, TRun = 0, T0 = 0, T1 = 0, Duration = 0;   // the run: on at V0, speeding up for Ta, at VTop for Tc, slowing to V1 for Tb
	FBrixTubeRules R;

	bool IsValid() const { return Nodes.Num() >= 2; }
	EBrixRidePhase Phase(double S) const;
	FVector At(double S) const;                // the rider's middle-of-the-feet, S seconds after stepping in
	double Lean(double S) const;               // degrees from upright: 0 going up or down, 90 lying along a level tube
	FVector Heading(double S) const;           // the way the rider's head points while lying (along the run), unit
	FVector Camera(double S) const;            // the camera's place (CITY-110): chasing along level tubes, beside the booths going up and down
	double Along(double S) const;              // how far along Way the rider is
	FVector OnWay(double D) const;
private:
	double RunDist(double U) const;
	FVector OnRun(double D) const;
};

// Works out the ride from stop A to stop B; !IsValid() when there is no way.
BRIXWORLD_API FBrixRide BrixMakeRide(const FBrixTubeNet& Net, const FBrixTubeRules& R, const FString& A, const FString& B);

// The stop whose booth floor Pos stands on (within BoothR of its middle), or empty.
BRIXWORLD_API FString BrixTubeStopAt(const FBrixTubeNet& Net, const FBrixTubeRules& R, const FVector2D& Pos);

// The server's answer to a rider asking for a stop (CITY-111): empty and OutRide set, or why not: "riding" (already on a
// ride), "not_in_a_stop", "unknown_stop", "same_stop", "no_way".
BRIXWORLD_API FString BrixAskRide(const FBrixTubeNet& Net, const FBrixTubeRules& R, const FVector2D& StandingAt, bool bRiding,
	const FString& To, FBrixRide& OutRide);
