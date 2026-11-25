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
	// engines (SND-002) and the player's car (SND-009)
	double IdleHz = 30, HzPerMps = 3.6, MaxHz = 95, EngineGain = .16, EngineFar = 70, VanPitch = .8, TyresGain = .05;
	int32 EngineVoices = 6;
	double CarGain = .2, StartLen = .8, SkidMps = 5, SkidGain = .18, BumpGain = .4;
	// the tram (SND-004)
	double HumHz = 48, WhineHz = 380, TramGain = .3, TramFar = 140, BellFar = 160;
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

// A car near the listener on a trip (cars.js near()): where it is, its speed in m/s, a van (or bus) or a car.
struct BRIXWORLD_API FBrixCarHeard { FVector2D Pos = FVector2D::ZeroVector; double V = 0; bool bVan = false; };
// An engine's voice this frame: the pitch of its note (Hz), its tyres' share, its level before hearing and its reach.
struct BRIXWORLD_API FBrixEngineVoice { bool bOn = false; FVector2D Pos = FVector2D::ZeroVector; double Hz = 0, Tyres = 0, Gain = 0, Far = 0; };
// The car the player drives this frame: its speed (m/s, back is negative), the throttle down, braking.
struct BRIXWORLD_API FBrixDriveHeard { double V = 0; bool bGas = false, bBrake = false; };

// Engines (SND-002, SND-009; sound.js engines()): a few voices given to the nearest cars on a trip within reach, each
// pitched by its speed (vans lower) and louder moving; the player's own car with its start-up, its pitch with speed and
// the throttle, a skid when braking hard and a bump when it is stopped short.
class BRIXWORLD_API FBrixEngineSounds
{
public:
	TArray<FBrixEngineVoice> Pool;       // the nearest cars, nearest first
	FBrixEngineVoice Mine;               // the player's car (Gain 0 when not driving)
	double Skid = 0;                     // the skid's level
	void Frame(const FBrixSoundRules& R, const TArray<FBrixCarHeard>& Cars, const FVector2D& Here, const FBrixDriveHeard* Drive, double Now, TArray<FBrixSoundEvent>& Out);

private:
	TOptional<double> Since, PrevV;
};

// A tram on the line this frame (FBrixTram::At): how far along, standing at a stop, which run.
struct BRIXWORLD_API FBrixTramHeard { double X = 0; bool bStopped = false; double Run = 0; };
// A tram's voice: its level before hearing (the hum's), the whine's pitch and share, the rumble's share.
struct BRIXWORLD_API FBrixTramVoice { bool bOn = false; FVector2D Pos = FVector2D::ZeroVector; double Gain = 0, Far = 0, WhineHz = 0, Whine = 0, Rumble = 0; };

// The tram (SND-004; sound.js trams()): two voices for the nearest trams, a hum always, a whine rising with its speed and
// a rumble while it moves; two bell notes when one stops and when it leaves. Its speed is from where it was a moment ago.
class BRIXWORLD_API FBrixTramSounds
{
public:
	FBrixTramVoice Voices[2];
	void Frame(const FBrixSoundRules& R, const TArray<FBrixTramHeard>& Trams, double Z, const FVector2D& Here, double Now, TArray<FBrixSoundEvent>& Out);

private:
	struct FSeen { double Run = 0, X = 0, T = 0; bool bStopped = false; };
	TArray<FSeen> Seen;                  // in the order first seen, as sound.js's Map
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
