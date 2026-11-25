#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "BrixTalk.h"

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
	double CarGain = .2, StartLen = .8, SkidMps = 5, SkidGain = .18, BumpGain = .4, HornGain = .22;
	// the tram (SND-004)
	double HumHz = 48, WhineHz = 380, TramGain = .3, TramFar = 140, BellFar = 160;
	// the ambience (SND-001): the street by day and night, the room, birds by day, crickets at night, fountains
	double StreetDay = .10, StreetNight = .04, RoomGain = .035;
	double BirdsFrom = 330, BirdsTo = 1230, BirdsPerMinute = 18, GardenBirdsPerMinute = 40, GardenM = 35;   // minutes of the day
	double CricketsFrom = 1260, CricketsTo = 300, CricketsGain = .022;
	double FountainGain = .16, FountainFar = 22;
	// indoors (SND-007): what is outside is muffled while the listener is in a building
	double MuffleHz = 650, MuffleGain = .4;
	// music (SND-010 to SND-012)
	double ScoreGain = .5, TalkDuck = .25, SourceDuck = .1;
	struct FMood { FString Name; double From = 0; };
	TArray<FMood> Moods;                 // the score's moods by time of day, in order
	struct FPlace { FString Style; double From = 0, To = 1440, Far = 20; bool bMarketDaysOnly = false; };
	TMap<FString, FPlace> Places;        // by lot kind: the music a café, a shop, the market plays
	struct FStation { FString Name, Style, Podcast; };
	TArray<FStation> Radio;              // the car radio's stations in order
	// money coming in (SND-019 to SND-021)
	int32 RewardRoot = 74, InterestRoot = 79, TopStep = 9, InterestCoins = 6;
	TArray<int32> RewardScale, InterestScale;
	// voices (SND-016): wordless syllables, one per mouth shape, in a pitch that is each speaker's own
	double VoiceMin = 140, VoiceMax = 300, LineGain = .18;
	TArray<FString> VoiceShapes;         // every mouth shape that has a voice (a line's syllables)
	TArray<FString> TalkShapes;          // those a wordless phrase picks from (all but MBP)
	TMap<FString, FVector2D> RolePitch;  // child, elder: their own range
	// what people do and say (SND-015, SND-016)
	int32 Busy = 8;
	struct FActivity { TArray<FString> Acts, Roles; double Every[2] = { 5, 10 }; FString Sound; bool bCall = false; double Chance = -1; };
	TArray<FActivity> Activities;
	TMap<FString, TArray<int32>> ByAct; // act -> its activities, in the rules' order
	TSet<FString> ChatActs;
	double PairM = 3.2, ChatFar = 16, ChatGain = .07, SyllablesPerS = 5.5, Question = .2, Laugh = .1, ChildLaugh = .3, LaughGain = .06;
	int32 ChatGroups = 5, Syllables[2] = { 3, 9 };
	double TurnGap[2] = { .2, .8 }, Pause[2] = { 1.2, 3.5 };
	double CrowdMin = 6, CrowdFull = 20, CrowdFar = 16, CrowdGain = .04, CrowdPerS = .25;
	double GreetM = 2.2, GreetChance = .25, GreetEvery = 60, GreetGain = .08, CallGain = .12, ShoutGain = .2;
	// podcasts on the radio (SND-014)
	double PodGapS = .7, PodIntroS = 2.5, PodBedGain = .22, PodVoiceGain = .2;
	struct FHost { FString Name; double Pitch = 180; };
	struct FEpisode { FString Title; TArray<TPair<FString, FString>> Lines; };   // host, text
	struct FShow { FString Title, Bed; TMap<FString, FHost> Hosts; TArray<FEpisode> Episodes; };
	TMap<FString, FShow> Shows;
};

// sound.js within(): is minute m in [From, To), the window running past midnight when To is before From?
BRIXWORLD_API bool BrixWithin(double M, double From, double To);
// How much of the day it is for the street's sound (sound.js frame()): 0 by night, 1 by day, eased over 80 minutes
// either side of 40 minutes before sunrise and after sunset (hours of the calendar's season).
BRIXWORLD_API double BrixStreetDay(double Minute, double SunriseH, double SunsetH);

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
	FString Act, Role, Shout;            // what they are doing (the day's act), their role, what they shout (or empty)
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
// the ground, louder running; a thud on landing from a fall faster than 3 m/s; and the jump (SND-009, play.js's
// Sound.act("jump")) as they leave the ground going up.
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

// The town's ambience (SND-001; sound.js frame() and ambience()): the street's air louder and brighter by day (two
// recordings, the day's and the night's, crossfaded), a room tone indoors, crickets at night out of doors, the nearest
// fountain from where it is, and birds by day now and then somewhere round the listener (more near a garden; none
// while driving).
class BRIXWORLD_API FBrixAmbience
{
public:
	double Street = 0, StreetDay = 0;    // the street's level, and how much of it is the day's recording (0..1)
	double Room = 0, Crickets = 0;
	FBrixEngineVoice Fountain;           // the nearest fountain (Pos, Gain, Far; bOn when there is one)
	void Frame(const FBrixSoundRules& R, double Minute, double SunriseH, double SunsetH, bool bIndoors, bool bDriving, const FVector2D& Here,
		const TArray<FVector2D>& Fountains, const TArray<FVector2D>& Gardens, double Dt, FRandomStream& Rng, TArray<FBrixSoundEvent>& Out);

private:
	double BirdT = 0;
};

// Music (SND-010 to SND-012; sound.js music()). The score's mood at a minute (moodAt()): the last whose time has come.
BRIXWORLD_API FString BrixMoodAt(const FBrixSoundRules& R, double Minute);

// A lot that can play music, in the city's lot order (the slice's LOTS).
struct BRIXWORLD_API FBrixMusicLot { int32 Lot = -1; FString Kind; FVector2D Min = FVector2D::ZeroVector, Max = FVector2D::ZeroVector, Door = FVector2D::ZeroVector; };
// The music the listener hears from a place now: the nearest open café, shop or (on market days) the market within its
// reach, from its door (or the listener's own spot when they are in it); Lot -1 when none.
struct BRIXWORLD_API FBrixMusicPlace { int32 Lot = -1; FString Style; FVector2D Pos = FVector2D::ZeroVector; double Far = 0; bool bInside = false, bOpenAir = false; };
BRIXWORLD_API FBrixMusicPlace BrixMusicPlace(const FBrixSoundRules& R, const TArray<FBrixMusicLot>& Lots, const FVector2D& Here, int32 InLot, double Minute, bool bMarketDay);
// The score's level: under a conversation, and under nearer music (Near: how loud the nearest other music is, 0..1).
BRIXWORLD_API double BrixScoreLevel(const FBrixSoundRules& R, bool bTalking, double Near);
// The radio's next station after Station (0 off, k the k-th station: music, or a podcast).
BRIXWORLD_API int32 BrixNextStation(const FBrixSoundRules& R, int32 Station);
// Is the weekday a market day (World/rules/calendar.json week and market_days)?
BRIXWORLD_API bool BrixIsMarketDay(const FString& CalendarPath, const FString& Weekday);

// Money coming in (SND-019 to SND-021; sound.js reward()): the recording for a reward's moment, what the quest and bank
// widgets call it with (quest_start, coin_out, coin_land, coin_take i of n, quest_end, interest_start, interest_coin i,
// interest_end, achievement). Each coin taken rings a step higher, the last at the top; each interest coin a step up.
BRIXWORLD_API FString BrixRewardSound(const FBrixSoundRules& R, const FString& What, int32 I, int32 N);
// The note (MIDI) step k of a reward scale rings (stepOf()).
BRIXWORLD_API int32 BrixRewardMidi(int32 Root, const TArray<int32>& Scale, int32 K);

// People's voices (SND-016; sound.js hashOf(), pitchOf(), voiceHz()): a number in 0..1 from a resident's index, the same
// every time; and the pitch of their voice from it, in their role's own range (children higher, elders lower).
BRIXWORLD_API double BrixHashOf(int64 I);
BRIXWORLD_API double BrixVoiceHz(const FBrixSoundRules& R, int32 Resident, const FString& Role);

// One wordless syllable: when (seconds from now), which mouth shape, its pitch, how long, how loud.
struct BRIXWORLD_API FBrixSyllable { double At = 0; FString Shape; double Hz = 200, Len = .15, Gain = .1; };
// N syllables that fall to an end, or rise to a question (phrase()); returns when it ends.
BRIXWORLD_API double BrixPhrase(const FBrixSoundRules& R, double T, double Base, int32 N, double Gain, bool bAsk, FRandomStream& Rng, TArray<FBrixSyllable>& Out);
// Ha-ha-ha, falling and fading (laugh()); returns when it ends.
BRIXWORLD_API double BrixLaugh(const FBrixSoundRules& R, double T, double Base, double Gain, FRandomStream& Rng, TArray<FBrixSyllable>& Out);
// A line voiced syllable by syllable in time with its mouth shapes (voice()), from From seconds into it; T0 is when it
// starts. A syllable before a question mark rises, one in a long word is a little higher.
BRIXWORLD_API void BrixVoiceLine(const FBrixSoundRules& R, const FBrixLine& Line, double T0, double Base, double Gain, double From, TArray<FBrixSyllable>& Out);

// What one speaker says this frame: their syllables, from where, heard how far.
struct BRIXWORLD_API FBrixVoiceOut { FVector2D Pos = FVector2D::ZeroVector; double Far = 30; TArray<FBrixSyllable> Syllables; };

// What the residents near do and say (SND-006, SND-015, SND-016; the rest of sound.js people()): the sounds of what each
// is doing now and then (cutlery, the till, typing, the swings...), for the nearest Busy of them; sellers calling; people
// standing or sitting together taking turns to talk, a phrase falling to an end or rising to a question, now and then a
// laugh; the hum of a crowd and syllables from among it; a passer-by's hello to the player; a resident's shout at the
// player's car (CITY-071), voiced. Keeps per resident only their timers, forgotten 30 s after they were last near.
class BRIXWORLD_API FBrixPeopleTalk
{
public:
	int32 Chat = 0, Murmur = 0, Crowd = 0;   // groups talking, people in them, people gathered (sound.js st)
	double CrowdBed = 0;                     // the crowd's hum, its level
	void Frame(const FBrixSoundRules& R, const TArray<FBrixHeardPerson>& People, const FVector2D& Here, double Now, double Dt, bool bGreet,
		const FBrixSpeech* Speech, FRandomStream& Rng, TArray<FBrixSoundEvent>& OutSounds, TArray<FBrixVoiceOut>& OutVoices);

private:
	struct FState { FString Act, Shout; TMap<int32, double> Next; double Greet = -1e9, Seen = 0; };
	TMap<int32, FState> St;
	struct FGroup { int32 Turn = 0; double Next = 0; };
	TMap<FString, FGroup> Groups;
	double CrowdT = 0, PruneT = 0;
};

// The radio's podcasts (SND-014; sound.js plan(), podcast()): every show's episodes in order, an intro and then each line
// (as long as its mouth takes, and a gap); where a show is follows the world clock, so everyone tuned in hears the same.
struct BRIXWORLD_API FBrixOnAir
{
	FString Key;                         // the part on air ("<show>/<episode>:intro" or "<show>/<episode>:<line>")
	FString Head, Text;                  // the caption: the show and episode; the host and their line ("…" in the intro)
	bool bIntro = false;
	double From = 0;                     // seconds into this part
	double Pitch = 0;                    // the host's voice
	FBrixLine Line;
};
BRIXWORLD_API FBrixOnAir BrixPodcastAt(const FBrixSoundRules& R, const FBrixSpeech& Speech, const FString& Show, double Minute, double Pace);

// A recording in Game/Data/sounds/index.json.
struct BRIXWORLD_API FBrixClipInfo
{
	FString File;
	bool bLoop = false;
	double Seconds = 0, Scale = 1;       // recorded at Scale times the slice's level: play at 1 / Scale
};

// Reads Game/Data/sounds/index.json: every recording by name, and the pitches the engine loops were recorded at.
BRIXWORLD_API bool LoadBrixSoundIndex(const FString& Path, TMap<FString, FBrixClipInfo>& Out, TArray<double>& OutEngineHz, FString& OutError);
// The pitches and lengths the syllables were recorded at (index.json syllable_hz, syllable_len).
BRIXWORLD_API void LoadBrixSyllableIndex(const FString& Path, TArray<double>& OutHz, TArray<double>& OutLen);

// The 16-bit PCM of a WAV file (RIFF, format 1): its samples, rate and channels. False when it is not one.
BRIXWORLD_API bool ReadBrixWav(const TArray<uint8>& File, TArray<uint8>& OutPcm, int32& OutRate, int32& OutChannels);
