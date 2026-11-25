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
// door and the starter) and out of it; money coming in (the quest and bank widgets' reward moments, SND-019 to SND-021);
// the music (SND-010 to SND-012): the score by time of day, ducked under a talk and under nearer music, the music of an
// open café or shop and of the market's busker on market days, muffled from the street and clear inside, and the car
// radio's music stations (R while driving). While the player is in a building what is outside is muffled and the room
// has its tone (SND-007). Continuous sounds are loops that play only while they can be heard. The long recordings (the
// music, the rewards) are Ogg Vorbis, read with the engine's own decoder.
// What the residents near do and say (SND-006, SND-015, SND-016): the sounds of what each is doing, sellers calling,
// people together taking turns to talk in wordless syllables and laughing, a crowd's hum, a passer-by's hello, a shout
// at the player's car, each syllable a recording of the slice's voice at the nearest of four pitches, pitched the rest of
// the way. The radio's podcasts (SND-014), their hosts voiced the same way over the show's bed, with a caption; the
// horn (F while driving).
// N turns all sound off and on (SND-013): the game menu's own Sound switch (World/rules/settings.json audio), kept with the
// menu's settings between visits; the menu's levels (All sound, Music, Street and weather, Voices, Actions and the
// interface) scale each part of the mix as the slice's Sound.mix() does (UI-009).
// -BrixSoundToggle turns it off and on again during a test shot, to check the key's work in the log; -BrixRadio=<k> has the
// car radio on its k-th station (with -BrixDrive).
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
	void Play(const FBrixSoundEvent& E, double Bus, bool bOutside = false);   // bOutside: muffled while the listener is indoors
	void Act(const FString& Name);       // an action sound of sound.json (act_<name>), the listener's own
	void Toggle();                       // all sound off or on (SND-013): the menu's Sound switch

private:
	struct FClip { TArray<uint8> Pcm; int32 Rate = 22050; double Scale = 1; double Seconds = 0; bool bLoop = false; };
	TSharedPtr<FClip> Pick(const FString& Name) const;   // the recording, or one of its variants (name_0, name_1, ...) at random
	USoundWaveProcedural* MakeWave(const TSharedPtr<FClip>& C);
	FString LotKindAt(const FVector2D& P) const;
	FVector2D Listener() const;
	// A loop: a component that plays the recording over and over while it is loud enough to hear, stopped when it is not.
	// bOutside: one of the sounds of out of doors, muffled while the listener is in a building; Bus: its share of the mix
	struct FLoop { int32 Comp = -1; TSharedPtr<FClip> Clip; bool bPlaced = true; bool bOutside = false; const double* Bus = nullptr; };
	FLoop MakeLoop(const FString& Name, bool bPlaced, bool bOutside = false, const double* Bus = nullptr);   // Bus: one of the Mix levels (World when none)
	// each part of the mix now: the rules' level times the menu's share (Music: the share alone, its level is in its recordings)
	double MixWorld = .85, MixVoice = .7, MixAction = .6, MixMusic = 1;
	void ReadMix();
	// Vol: the slice's level for the listener, before the mix; Lowpass: Hz, 0 for none
	void SetLoop(FLoop& L, double Vol, double Pitch, const FVector2D& Pos, double Lowpass = 0);
	void Music(double Dt, const FVector2D& Here);
	// a syllable to be said: when (the world's seconds), where, which mouth shape at which pitch; bRadio: through the radio
	// bLine: a talk's murmured line, hushed when the next line starts or the talk closes (sound.js hush())
	struct FPendingSyllable { double At = 0; FVector2D Pos = FVector2D::ZeroVector; double Far = 30; FBrixSyllable S; bool bRadio = false; bool bLine = false; };
	TArray<FPendingSyllable> Pending;
	void Say(const FBrixVoiceOut& V, double Now, bool bRadio, bool bLine = false);
	void PlaySyllable(const FPendingSyllable& P, const FVector2D& Here);
	void Radio(double Now, bool bDriving);   // the podcast on air, its caption
	void OffAir();
	void BuildCaption();
	void OnReward(const FString& What, int32 I, int32 N);
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
	int32 InLot = INDEX_NONE;            // the building the listener is in, or none (CITY-013's rooms)
	// the music: one loop a style or mood, each at its own level, eased toward where it should be
	// Hear and Lowpass are what the place it plays from asks for; HearNow and LowNow are those eased (sound.js ramps
	// them over 0.2 s), so a doorway is not a step in level and tone
	struct FTune { FLoop Loop; double Level = 0, Target = 0, Tc = 1, Lowpass = 0, Hear = 1, HearNow = 1, LowNow = 18000; FVector2D Pos = FVector2D::ZeroVector; };
	TMap<FString, FTune> Tunes;
	TArray<FBrixMusicLot> MusicLots;
	bool bMarketDay = false, bRewardsBound = false;
	int32 Station = 0;                   // the car radio: 0 off, k the k-th station of sound.json
	FBrixPeopleTalk PeopleTalk;
	FBrixSpeech Speech;                  // how a line is mouthed (World/rules/speech.json), for shouts and the podcasts' hosts
	bool bSpeech = false;
	TArray<double> SylHz, SylLen;        // the pitches and lengths the syllables were recorded at
	FLoop CrowdL, HornL;
	FString OnAir;                       // the podcast part on air
	FString CaptionHead, CaptionText;    // the radio's caption (sound.js caption()); hidden when CaptionHead is empty
	double CaptionUntil = 0;             // a music station's name shows a moment; a podcast's caption stays
	TSharedPtr<class SWidget> CaptionBox;
	int32 Spoken = 0;                    // syllables said, for the log
	bool bTestHorn = false;              // -BrixHorn: the horn held in a test shot
	FString WasMood, WasPlace;
	// what the interface and the player were doing last frame, for the sounds of a change
	bool bWasTalking = false, bWasRiding = false;
	int32 WasAnswered = 0, WasDriving = -1, WasMurmured = 0;
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
