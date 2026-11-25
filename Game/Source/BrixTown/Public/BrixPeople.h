#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixBody.h"
#include "BrixCity.h"
#include "BrixDodge.h"
#include "BrixPlay.h"
#include "BrixPeople.generated.h"

struct FBrixCity;
struct FBrixDay;
struct FBrixTraffic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UTextRenderComponent;

// A body in Unreal's frame and centimetres, ready for a procedural mesh: made on any thread, uploaded on the game thread.
struct BRIXTOWN_API FBrixMeshArrays
{
	TArray<FVector> V, N;
	TArray<FLinearColor> C;
	TArray<int32> Tri;
	void From(const FBrixBodyMesh& Body, bool bFlip);
	// into M's one section (updated in place when it had as many vertices); returns the vertices it has now
	int32 Upload(UProceduralMeshComponent* M, int32 Had, UMaterialInterface* Material) const;
};

// A body from BrixBody (the slice's frame, metres) into M's one section, in Unreal's frame and centimetres. Had: the
// triangles' indices the section had (it is updated in place when the count is the same). Returns what it has now.
BRIXTOWN_API int32 BrixBodyToMesh(UProceduralMeshComponent* M, const FBrixBodyMesh& Body, int32 Had, bool bFlip, UMaterialInterface* Material);

// The town's residents, where their routine puts them at the clock's minute (CITY-021, CITY-022): worked out each frame
// from the day (Game/Data/day.json) and the clock, never stepped or stored. Only those out of doors within ShowDistance of
// the player are drawn (CITY-024), up to MaxBodies. They are the slice's option B people (ART-004), dressed by their role
// and walking by their role's movement signature (ANIM-002), built by BrixWorld's port of people.js (BrixBody): within
// MidDistance a body with a face, to NearDistance a simpler body whose face fades into the skin, the nearest FullCount
// within FullDistance at full detail, and further out a figure of a few blocks, as the slice draws them.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixPeople : public AActor
{
	GENERATED_BODY()

public:
	ABrixPeople();
	void Start(const FBrixCity& InCity, TSharedPtr<FBrixDay> InDay, TSharedPtr<FBrixTraffic> InTraffic = nullptr);
	int32 CameraLot = INDEX_NONE;   // the building the camera is in (the game mode's RoomAt): its residents inside are drawn (CITY-013)
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Config) float ShowDistance = 90.f;    // m; residents further from the player are not drawn (CITY-024)
	UPROPERTY(Config) int32 MaxBodies = 400;
	UPROPERTY(Config) float LaneWidth = 1.0f;       // m to either side of a pavement's line (the slice's lane)
	UPROPERTY(Config) float FullDistance = 5.f;     // m; the slice's FULL_M: full detail (25,000 vertices) ...
	UPROPERTY(Config) int32 FullCount = 1;          // ... for this many of the nearest
	UPROPERTY(Config) float MidDistance = 16.f;     // the slice's LOD1_M
	UPROPERTY(Config) float NearDistance = 35.f;    // the slice's LOD3_M: blocks beyond
	UPROPERTY(Config) float FaceOutDistance = 30.f; // the slice's FACE_OUT_M: the face is gone here
	UPROPERTY(Config) int32 MaxMeshBodies = 48;     // bodies built as meshes at once; the rest of those within NearDistance are blocks
	UPROPERTY(Config) TArray<int32> RebuildEvery;   // frames between rebuilding a body, by detail (0 full, 1, 2); moved every frame
	UPROPERTY(Config) bool bFlipWinding = false;    // the triangles' turn as Unreal culls them
	UPROPERTY(Config) float RideDistance = 70.f;    // m: people in vehicles are drawn this near (cars.js seat())

	int32 Shown() const { return LastShown; }

	// The player on the playground (WLD-041; play.js findTarget()): the nearest place to have a go within 1.7 m of the player
	// (2 m either way, level with them to 1.2 m) that no child is on, or INDEX_NONE; and the playground itself
	int32 PlayTarget(const FVector& MeCm, double& OutD) const;
	FBrixPlay& PlayArea() { return Play; }

	// Talking (CITY-007; conversations.js consider(), where()): the nearest resident the player could talk to this frame,
	// within TalkReach of them and level with them, out of doors and not in a vehicle; and while a talk is under way, the
	// one held: stopped where they were, turning to the player, their face as they speak, always at full detail. After a
	// talk they run late by the time it took until they next stand still at a place they would be at anyway.
	struct FCandidate { int32 Resident = -1; FVector2D Pos = FVector2D::ZeroVector; float Up = 0.f; double Yaw = 0; int32 Pose = 0; int32 Act = 0; int32 Lot = -1; bool bWalking = false; double D = 0; };
	const FCandidate& Candidate() const { return Cand; }
	float WetClothes() const { return WetOnPeople; }   // how much darker someone out in the rain is now (WLD-037)
	FCandidate Held;                    // Held.Resident -1: nobody held
	const FBrixFace* HeldFace = nullptr;
	TMap<int32, double> Late;           // resident -> minutes behind their day
	UPROPERTY(Config) float TalkReach = 2.2f;   // m
	// every resident near this frame, for the sound (ABrixSound: their steps, sitting down and getting up; SND-017)
	struct FHeard { int32 Resident; FVector2D Pos; int32 Pose; double Phase; FString Shout; bool bRun = false; float Up = 0.f; double S = 1; };   // a shout at the player's car (CITY-071), voiced; Up, S: their feet and height (the names over them)
	TArray<FHeard> Heard;
	// Out of the way of the player's car (CITY-070, CITY-071; dodge.js): residents it heads at scramble aside, then turn on
	// the driver and shout, the line shown over them within the rules' tag_m (World/rules/dodge.json)
	FBrixDodge Dodge;
	UPROPERTY(Config) float ShoutSize = 0.22f;       // m, the shout's letters
	UPROPERTY(Config) FLinearColor ShoutColour = FLinearColor(0.16f, 0.12f, 0.10f);
	UPROPERTY(Config) float ShoutGap = 1.5f;         // m: of shouters closer than this, only the nearest's shout is shown
	int32 Dodging() const { return Dodge.Live(); }
	int32 DodgeLogged = 0;
	TArray<FBrixLook> CrewLooks;        // the repair crews' two looks (smash.js crewLook)
	TArray<FBrixLook> CoatLooks;        // per resident, today's coat over their clothes (CITY-077), made when first needed
	TArray<int8> CoatMade;              // 0 not yet, 1 today's, 2 a wet day's
	// snow (WLD-070, WLD-071): how much settles on people out in it while it snows; their footprints in lying snow
	UPROPERTY(Config) float PeopleSnow = 0.55f;
	UPROPERTY(Config) float PeopleWet = 0.3f;    // out in the rain: darker by wet x sky.json wet_dark x this (the slice's .3; WLD-037)
	UPROPERTY(Config) float PrintDistance = 30.f;
	UPROPERTY(Config) float MarkLift = 0.012f;   // m: marks in the snow this far above the ground      // m: footprints drawn behind walkers this near the camera
	UPROPERTY(Config) int32 MaxPlayerSteps = 400;      // the player's own footprints kept this visit
	UPROPERTY(Config) FLinearColor SnowTone = FLinearColor(0.80f, 0.88f, 1.0f);      // lying snow (as M_Greybox's) ...
	UPROPERTY(Config) FLinearColor PressedSnow = FLinearColor(0.42f, 0.48f, 0.60f);  // ... and pressed down by a foot or a tyre
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Prints;
	TArray<struct FBrixPrint> PlayerSteps;            // where the player stepped, Age the minute they did
	FVector2D LastStep = FVector2D::ZeroVector;
	TArray<float> SlotSnow, SlotWet;
	float WetOnPeople = 0.f, FigureWet = -1.f;
	float SnowOnPeople = 0.f, FigureSnow = -1.f;
	double TickMs() const { return TickFrames ? TickSeconds * 1000.0 / TickFrames : 0.0; }   // the people's own time a frame, on average
	void ResetTickTime() { TickSeconds = 0; TickFrames = 0; for (double& P : Parts) P = 0; }
	// the time a frame by part: where everyone is, placing and the far figures' boxes, building bodies, uploading them, the far mesh
	FString TickParts() const { return TickFrames ? FString::Printf(TEXT("where %.2f, place %.2f, build %.2f, upload %.2f, far %.2f ms"), Parts[0] * 1e3 / TickFrames, Parts[1] * 1e3 / TickFrames, Parts[2] * 1e3 / TickFrames, Parts[3] * 1e3 / TickFrames, Parts[4] * 1e3 / TickFrames) : FString(); }

private:
	float GroundAt(const FVector2D& P) const;

	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Figures;   // the far figures' boxes, all in one mesh (moving instances cost the render thread far more)
	// children on the playgrounds (WLD-032; dressing.js): who is on which swing, slide, seesaw, sandpit or climbing frame, and
	// the swing seats and seesaw planks near the camera, moving with them (one small mesh made again each frame)
	FBrixPlay Play;
	TArray<int32> ReadActs;             // the day's acts of reading the notices and buying a paper
	TArray<int32> PlayUse;              // by the day's act: which play place it is (FBrixPlay::EUse), -1 none
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> PlayParts;
	UPROPERTY() TObjectPtr<UMaterialInterface> FacetsMat;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
	UPROPERTY() TObjectPtr<UMaterialInterface> BodyMaterial;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> ShoutTags;   // shouts over residents (CITY-071)
	TSharedPtr<FBrixDay> Day;
	FBrixPeopleRules Rules;
	TArray<FBrixLook> Looks;            // each resident's, from their role (people.js look()), worked out once
	TSharedPtr<FBrixTraffic> Traffic;   // the day's vehicles, for whoever rides in them (CITY-040, CITY-094)
	TArray<FBrixLook> Drivers, Passengers;
	// residents' tram rides (cars.js tramRiders()): out from a stop or back to one, on which run
	struct FTramRide { int32 Resident; double T0, T1; TOptional<double> Out, Back; };
	TArray<FTramRide> TramRides;
	FBrixTram Tram;   // per vehicle: someone from out of town at its wheel, and beside them (cars.js stranger())
	TArray<FBox2D> Paving;              // the blocks' paving, a step above the street
	float Slab = 0.12f;
	TArray<float> Lane;                 // each resident's own, from the slice's hash
	struct FSlot { int32 Resident = -1; int32 Lod = -1; int32 Verts = 0; int32 Age = 0; };
	TArray<FSlot> Slots;
	FBrixBodyMesh Scratch;
	struct FBuilt { FBrixBodyMesh Body; FBrixMeshArrays Arrays; };
	TArray<FBuilt> Built;               // per slot: the body last built for it
	TArray<FBrixFarBox> BoxScratch;
	int32 FigureCap = 0;                // boxes the far figures' mesh has room for (grows and shrinks in steps, so it is mostly updated in place)
	FCandidate Cand;
	int32 Frame = 0, LastShown = 0;
	double TickSeconds = 0, Parts[5] = { 0, 0, 0, 0, 0 };
	int32 TickFrames = 0;
};
