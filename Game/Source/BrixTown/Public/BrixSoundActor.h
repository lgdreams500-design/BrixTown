#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixSound.h"
#include "BrixSoundActor.generated.h"

class ABrixPeople;
class USoundAttenuation;
class USoundWaveProcedural;
struct FBrixCity;
struct FBrixDay;

// The town's sound (SND-003, SND-007, SND-017): the slice's own sounds (Prototype/play/sound.js), recorded by
// Tools/unreal/sound_values.py into Game/Data/sounds and played here with no editor import: each WAV is read once and
// fed to the engine through a procedural sound wave, as the talk's voices are. What is heard, from where and how loud is
// decided by BrixWorld's port of sound.js (FBrixPeopleSounds, FBrixPlayerSounds, BrixHear) from World/rules/sound.json;
// the engine only pans each sound from where it is (its own distance fall-off is off, the slice's is applied instead).
// So far: the player's steps and landings, and the steps of the residents near, by surface (stone, wood, grass), louder
// running and lighter for children, and the sound of a resident sitting down, lying down or getting up.
// A test shot logs how many of each sound were played.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixSound : public AActor
{
	GENERATED_BODY()

public:
	ABrixSound();
	void Start(const FBrixCity& City, TSharedPtr<FBrixDay> InDay, ABrixPeople* InPeople);
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(Config) FString RulesFile = TEXT("../World/rules/sound.json");   // from the project folder (Game/)
	UPROPERTY(Config) FString SoundDir = TEXT("Data/sounds");
	UPROPERTY(Config) float HearPeople = 20.f;   // m: residents this near are listened to (play.js: heard)
	UPROPERTY(Config) float SourceUp = 1.f;      // m above the street a sound in the town is played from

	// Plays a one-shot on a bus (the rules' World, Voice or Action share): a variant of the named recording, where the
	// event says, at the slice's level for the listener (BrixHear) unless it is the listener's own.
	void Play(const FBrixSoundEvent& E, double Bus);

private:
	struct FClip { TArray<uint8> Pcm; int32 Rate = 22050; double Scale = 1; double Seconds = 0; bool bLoop = false; };
	TSharedPtr<FClip> Pick(const FString& Name) const;   // the recording, or one of its variants (name_0, name_1, ...) at random
	USoundWaveProcedural* MakeWave(const TSharedPtr<FClip>& C);
	FString LotKindAt(const FVector2D& P) const;
	FVector2D Listener() const;

	FBrixSoundRules Rules;
	TMap<FString, TSharedPtr<FClip>> Clips;
	TMap<FString, int32> Variants;       // a recording's name without its variant -> how many variants
	FBrixPeopleSounds PeopleSounds;
	FBrixPlayerSounds PlayerSounds;
	TSharedPtr<FBrixDay> Day;
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
