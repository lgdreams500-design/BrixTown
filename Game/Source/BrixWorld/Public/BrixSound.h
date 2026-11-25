#pragma once

#include "CoreMinimal.h"

// The town's sound as the slice decides it (Prototype/play/sound.js, from World/rules/sound.json): what is heard, from
// where, how loud and at what pitch. Only the deciding is here, ported from sound.js and checked against it by the SND
// tests (Tests/values/sound_city_01.json, sound.js's own functions traced by Tools/unreal/sound_values.py). The sounds
// themselves are the slice's own, recorded by that script into Game/Data/sounds; BrixTown's ABrixSound plays them.

// World/rules/sound.json: what the logic below reads (SND-008: the tuning is data).
struct BRIXWORLD_API FBrixSoundRules
{
	// the mix: each bus's share before the master
	double Master = .8, World = .85, Voice = .7, Action = .6, Music = .4;
	// hearing (place()): full within Near, then falling by Rolloff a metre, gone by Far
	double Near = 2.5, Rolloff = .9, Far = 70, Pan = .8;
	// footsteps (SND-003)
	double PlayerStepGain = .16, ResidentStepGain = .09, StepFar = 9, LandGain = .3;
	int32 StepMax = 6;
	TArray<FString> Surfaces;            // stone, wood, grass
	// people (SND-017)
	double PeopleFar = 18, RunLouder = 1.5, ChildSofter = .7, ChildPitch = 1.3;
	FString PoseSit = TEXT("sit"), PoseStand = TEXT("stand"), PoseLie = TEXT("lie");
};

// Reads World/rules/sound.json. On failure returns false and says why.
BRIXWORLD_API bool LoadBrixSoundRules(const FString& Path, FBrixSoundRules& Out, FString& OutError);

// How loud and where a sound at At is for a listener at Here facing Yaw (sound.js place()): (gain 0..1, pan -1..1).
// Far <= 0: the rules' hearing distance.
BRIXWORLD_API FVector2D BrixHear(const FBrixSoundRules& R, const FVector2D& Here, double Yaw, const FVector2D& At, double Far = 0);

// A sound to play once (a step, a resident sitting down): which recording, where, at what level before hearing
// (the recording's own level times this), heard to Far metres; Pitch as sound.js gives it (a child's steps are recorded
// higher, so the game plays every recording at its own pitch).
struct BRIXWORLD_API FBrixSoundEvent
{
	FString Sound;                       // a recording's name without its variant (step_stone_child, person_sit)
	FVector2D Pos = FVector2D::ZeroVector;
	double Gain = 1, Far = 0, Pitch = 1;
	bool bHere = false;                  // the listener's own sound: not placed (the player's steps, landing)
};

// A resident near the listener this frame, as play.js hands them to sound.js (heard.push()).
struct BRIXWORLD_API FBrixHeardPerson
{
	int32 Id = 0;
	FVector2D Pos = FVector2D::ZeroVector;
	bool bWalking = false;
	int32 Pose = 0;                      // 0 standing, 1 walking, 2 sitting, 3 lying
	double Phase = 0;                    // the walk's cycle, 0..1: a step at each half
	bool bInside = false, bRun = false, bChild = false;
	FString LotKind;                     // the kind of lot they are on (a garden is grass)
};

// The surface a step is heard on (sound.js people() and play.js's walk.surface): wood indoors, grass in a garden, else stone.
BRIXWORLD_API FString BrixStepSurface(bool bInside, const FString& LotKind);

// People's movements (SND-003, SND-017; sound.js people()): each near resident's steps in time with their walk, louder
// running, lighter and higher for a child, at most StepMax a frame and none beyond StepFar; the sound of sitting down,
// lying down and getting up within 10 m. Keeps per resident only their last half-step and pose, forgotten 30 s after
// they were last near.
class BRIXWORLD_API FBrixPeopleSounds
{
public:
	void Frame(const FBrixSoundRules& R, const TArray<FBrixHeardPerson>& People, const FVector2D& Here, double Now, TArray<FBrixSoundEvent>& Out);

private:
	struct FState { int32 Half = -1; bool bHalf = false; int32 Pose = 0; double Seen = 0; };
	TMap<int32, FState> St;
	double PruneT = 0;
};

// The player's own steps and landing (SND-003; sound.js frame()): a step at each half of the walk's cycle while moving on
// the ground, louder running; a thud on landing from a fall faster than 3 m/s.
class BRIXWORLD_API FBrixPlayerSounds
{
public:
	void Frame(const FBrixSoundRules& R, bool bMoving, bool bOnGround, double Phase, bool bRun, double Vz, const FString& Surface, TArray<FBrixSoundEvent>& Out);

private:
	int32 LastHalf = -1;
	bool bLast = false, bPrevGround = true;
	double PrevVz = 0;
};

// A recording in Game/Data/sounds/index.json.
struct BRIXWORLD_API FBrixClipInfo
{
	FString File;
	bool bLoop = false;
	double Seconds = 0, Scale = 1;       // recorded at Scale times the slice's level: play at 1 / Scale
};

// Reads Game/Data/sounds/index.json: every recording by name, and the pitches the engine loops were recorded at.
BRIXWORLD_API bool LoadBrixSoundIndex(const FString& Path, TMap<FString, FBrixClipInfo>& Out, TArray<double>& OutEngineHz, FString& OutError);

// The 16-bit PCM of a WAV file (RIFF, format 1): its samples, rate and channels. False when it is not one.
BRIXWORLD_API bool ReadBrixWav(const TArray<uint8>& File, TArray<uint8>& OutPcm, int32& OutRate, int32& OutChannels);
