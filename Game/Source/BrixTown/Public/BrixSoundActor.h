#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixCity.h"
#include "BrixSound.h"
#include "BrixSoundActor.generated.h"

class ABrixPeople;
class UAudioComponent;
class USoundAttenuation;
class USoundWaveProcedural;
struct FBrixCity;
struct FBrixDay;
struct FBrixTraffic;

// The town's sound (SND-003, SND-007, SND-017): the slice's own sounds (Prototype/play/sound.js), recorded by
// Tools/unreal/sound_values.py into Game/Data/sounds and played here with no editor import: each WAV is read once and
// fed to the engine through a procedural sound wave, as the talk's voices are. What is heard, from where and how loud is
// decided by BrixWorld's port of sound.js (FBrixPeopleSounds, FBrixPlayerSounds, BrixHear) from World/rules/sound.json;
// the engine only pans each sound from where it is (its own distance fall-off is off, the slice's is applied instead).
// So far: the player's steps and landings, and the steps of the residents near, by surface (stone, wood, grass), louder
// running and lighter for children, and the sound of a resident sitting down, lying down or getting up; the engines of
// the nearest cars on a trip and of the player's car (its start-up, skids and bumps), the trams' hum, whine, rumble and
// bell; the street's air by time of day, crickets at night, the nearest fountain, birds by day; the interface and
// interactions: a prompt appearing, a talk opening, an answer chosen, a talk closing, a jump, getting into a car (the
// door and the starter) and out of it. Continuous sounds are loops that play only while they can be heard.
// N turns all sound off and on (SND-013), remembered between visits in the user's settings (GameUserSettings.ini).
// -BrixSoundToggle turns it off and on again during a test shot, to check the key's work in the log.
// A test shot logs how many of each sound were played.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixSound : public AActor
{
	GENERATED_BODY()

public:
	ABrixSound();
	void Start(const FBrixCity& City, TSharedPtr<FBrixDay> InDay, ABrixPeople* InPeople, TSharedPtr<FBrixTraffic> InTraffic);
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(Config) FString RulesFile = TEXT("../World/rules/sound.json");   // from the project folder (Game/)
	UPROPERTY(Config) FString SoundDir = TEXT("Data/sounds");
	UPROPERTY(Config) float HearPeople = 20.f;   // m: residents this near are listened to (play.js: heard)
	UPROPERTY(Config) float SourceUp = 1.f;      // m above the street a sound in the town is played from
	UPROPERTY(Config) float CarsNear = 75.f;     // m: cars on a trip this near along and across are listened to (cars.js near())

	// Plays a one-shot on a bus (the rules' World, Voice or Action share): a variant of the named recording, where the
	// event says, at the slice's level for the listener (BrixHear) unless it is the listener's own.
	void Play(const FBrixSoundEvent& E, double Bus);
	void Act(const FString& Name);       // an action sound of sound.json (act_<name>), the listener's own
	bool IsOn() const { return bOn; }
	void SetOn(bool bNow);               // all sound on or off (SND-013), and remembered

private:
	struct FClip { TArray<uint8> Pcm; int32 Rate = 22050; double Scale = 1; double Seconds = 0; bool bLoop = false; };
	TSharedPtr<FClip> Pick(const FString& Name) const;   // the recording, or one of its variants (name_0, name_1, ...) at random
	USoundWaveProcedural* MakeWave(const TSharedPtr<FClip>& C);
	FString LotKindAt(const FVector2D& P) const;
	FVector2D Listener() const;
	// A loop: a component that plays the recording over and over while it is loud enough to hear, stopped when it is not.
	struct FLoop { int32 Comp = -1; TSharedPtr<FClip> Clip; bool bPlaced = true; };
	FLoop MakeLoop(const FString& Name, bool bPlaced);
	void SetLoop(FLoop& L, double Vol, double Pitch, const FVector2D& Pos);   // Vol: the slice's level for the listener, before the mix
	void Engine(FLoop& Lo, FLoop& Hi, double Hz, double Vol, const FVector2D& Pos);   // an engine's note from the two recordings
	void Vehicles(double Now, const FVector2D& Here);

	FBrixSoundRules Rules;
	TMap<FString, TSharedPtr<FClip>> Clips;
	TMap<FString, int32> Variants;       // a recording's name without its variant -> how many variants
	FBrixPeopleSounds PeopleSounds;
	FBrixPlayerSounds PlayerSounds;
	FBrixEngineSounds EngineSounds;
	FBrixTramSounds TramSounds;
	FBrixAmbience Ambience;
	FRandomStream Rng;
	TArray<FVector2D> Fountains, Gardens;
	double SunriseH = 6, SunsetH = 20;   // the calendar's, for the day's season
	FLoop StreetDayL, StreetNightL, RoomL, CricketsL, FountainL;
	// what the interface and the player were doing last frame, for the sounds of a change
	bool bWasTalking = false, bWasRiding = false, bOn = true;
	int32 WasAnswered = 0, WasDriving = -1;
	FString WasPrompt;
	void Interactions();
	TSharedPtr<FBrixDay> Day;
	TSharedPtr<FBrixTraffic> Traffic;
	FBrixTram Tram;
	double Pace = 10;                    // real seconds a game minute (the calendar's)
	TArray<double> EngineHz;             // the pitches the engine loops were recorded at (low, high)
	UPROPERTY() TArray<TObjectPtr<UAudioComponent>> LoopComps;
	TArray<FLoop> CarLo, CarHi;          // per engine voice, the low and high recordings
	FLoop MineLo, MineHi, Skid;          // the player's car
	FLoop Hum[2], Whine[2], Rumble[2];   // per tram voice
	TArray<FBrixCarHeard> CarsHeard;
	TArray<FBrixTramHeard> TramsHeard;
	TMap<FString, double> LoopTime;      // seconds each loop has been heard, for the log
	UPROPERTY() TObjectPtr<ABrixPeople> People;
	UPROPERTY() TObjectPtr<USoundAttenuation> Placed;   // spatialised, with the engine's distance fall-off off
	struct FLotArea { FVector2D C, Half; double Cos = 1, Sin = 0; FString Kind; };
	TArray<FLotArea> OpenLots;           // gardens, the market and stops: what a step out of doors is on
	int32 ChildRole = -1;
	TArray<FBrixSoundEvent> Events;
	TArray<FBrixHeardPerson> Heard;
	TMap<FString, int32> Played;         // how many of each sound, for the log
	double LogAt = 0;
	void LogPlayed(const TCHAR* When);
};
