#pragma once

#include "CoreMinimal.h"
#include "BrixBody.h"

struct FBrixDay;
struct FBrixTraffic;

// The weather (WLD-014, WLD-037; ADR-0016) and people in it (CITY-077 to CITY-080; ADR-0020). The day's weather is
// citysim.weather() for each six-hour slot (Game/Data/day.json "weather"); what it does to the light and the paving is
// worked out from the clock and World/rules/sky.json alone (BrixWeatherNow, a port of world.weather_now(), checked by
// WLD037_the_weather_is_worked_out_as_world_py_does). What each resident wears and carries today is citysim's
// (day.json "wear", City.wear()); BrixWet shows it: a coat, and out in the rain an umbrella up, else a hood, else a hand
// over the head when walking, and a quicker walk.

struct BRIXWORLD_API FBrixWeatherKind { double Sun = 1, Haze = 1, Veil = 0, Grey = 0, Rain = 0, Snow = 0; };

struct BRIXWORLD_API FBrixWeatherRules
{
	TMap<FString, FBrixWeatherKind> Kinds;   // clear, cloud, rain, fog
	TMap<FString, double> Cover;             // how much of the sky the clouds cover, by kind
	// the clouds (WLD-035, WLD-036; ADR-0014): the size of their noise, how soft their edges, the wind, how fast they drift
	// and change shape, how bright their sunny side and their shade
	double Scale = 2.4, Soft = .07, CloudSpeed = .012, Change = .003, Lit = .42, Shade = .8;
	FVector2D Wind = FVector2D(1, .35);
	double BlendMinutes = 40, OvercastFill = .2;
	bool bFollow = true;
	// rain: how the drops look and how wet the paving gets (ADR-0016)
	double Alpha = .26, Slant = .1, Speed = 9, Length = .8, WetDark = .4, WetSheen = .3, Puddles = .85, Ripples = 1, DryMinutes = 90;
	// snow (WLD-070, ADR-0051): how the flakes look and fall, how fast it settles and melts, how white a full cover is
	double SnowAlpha = .75, SnowSpeed = 1.1, SnowSway = .25, FlakeSize = .022, SettleMinutes = 120, MeltMinutes = 360, SnowCover = .9;
	// tracks in the snow (WLD-071): how far behind a walker and a vehicle they show, how fast falling snow fills them in,
	// how fast they fade, the least lying snow that takes them
	double TrailM = 14, CarTrailM = 60, FillMinutes = 20, FadeMinutes = 90, MinLying = .15;
};

BRIXWORLD_API bool LoadBrixWeatherRules(const FString& SkyPath, FBrixWeatherRules& Out, FString& OutError);

struct BRIXWORLD_API FBrixWeatherNow
{
	FString Kind = TEXT("clear");
	double Sun = 1, Haze = 1, Veil = 0, Grey = 0, Rain = 0, Snow = 0, Wet = 0;
};

// world.snow_now() (WLD-070): how hard it snows (0 to 1) and how much snow lies (0 to SnowCover), built while it snows and
// melting after, from the clock and the day's weather slots alone
struct BRIXWORLD_API FBrixSnowNow { double Falling = 0, Lying = 0; };
BRIXWORLD_API FBrixSnowNow BrixSnowNow(const FBrixWeatherRules& R, const TArray<FString>& DayWeather, double Minute, const FString& Trying = FString());

// WLD-071: a mark in the snow, a footprint or a stretch of a tyre's track: where (x, z), the way it points, how long ago it
// was made (game minutes) and how plain it still is (0 to 1: filled in by falling snow, faded with time)
struct BRIXWORLD_API FBrixPrint { FVector2D Pos = FVector2D::ZeroVector, Dir = FVector2D(1, 0); double Age = 0, Strength = 1; };
// how plain a mark Age minutes old is with snow Falling now and Lying (0 when too little lies)
BRIXWORLD_API double BrixPrintStrength(const FBrixWeatherRules& R, double Age, double Falling, double Lying);
// the footprints behind resident i, walking at minute M: one each Stride metres back along their route, left and right in
// turn (Side metres off the line), as far as TrailM or the walk's start; none if they are not walking
BRIXWORLD_API void BrixFootprints(const FBrixWeatherRules& R, const FBrixDay& Day, int32 i, double M, double Stride, double Side, double Falling, double Lying, TArray<FBrixPrint>& Out);
// the tracks behind vehicle k at minute M: for each of its two wheel lines (Gauge metres apart), the points along where it
// went, as far as CarTrailM, in pairs (each two a stretch of track)
BRIXWORLD_API void BrixTyreTracks(const FBrixWeatherRules& R, const FBrixTraffic& Traffic, int32 k, double M, double Gauge, double Falling, double Lying, TArray<FBrixPrint>& Out);

// world.weather_now(): DayWeather the day's four slots; Trying one kind for the whole day (a look at it), or empty.
BRIXWORLD_API FBrixWeatherNow BrixWeatherNow(const FBrixWeatherRules& R, const TArray<FString>& DayWeather, double Minute, const FString& Trying = FString());

// world.clouds_now() (WLD-036): how much sky the clouds cover, where the wind has carried them and the phase of their
// shapes, at minute M of day Day (counted from day 0)
struct BRIXWORLD_API FBrixCloudsNow { double Cover = 0; FVector2D Offset = FVector2D::ZeroVector; double Phase = 0; };
BRIXWORLD_API FBrixCloudsNow BrixCloudsNow(const FBrixWeatherRules& R, const TArray<FString>& DayWeather, int32 Day, double Minute, const FString& Trying = FString());

// What the residents wear and carry today (day.json "wear"): per resident a coat's colour (or none), a hood, an
// umbrella's colour (or none), on the day as it is and on a wet day (shown when rain is tried on a dry day); the pace
// factors in the rain.
struct BRIXWORLD_API FBrixWear
{
	TArray<FString> Colours;                 // palette names
	struct FRow { int32 Coat = -1; bool bHood = false; int32 Umbrella = -1; };
	TArray<FRow> Today, Wet, Snowy;          // today; on a day of rain; on a day of snow (CITY-107)
	double SnowPace = .85;                   // the careful pace in snow
	double Run = 2, Hurry = 1.3, Umbrella = 1.1;
	TArray<FString> Weather;                 // the day's four slots
	TMap<FString, double> SigSpeed;          // each movement signature's own walking speed, m/s (World/rules/signatures.json)
	double Pace = 10;                        // real seconds a game minute
};

BRIXWORLD_API bool LoadBrixWear(const FString& DayPath, const FString& SignaturesPath, FBrixWear& Out, FString& OutError);

// What a resident out of doors shows in the weather (wet.js apply(), the drawing ADR-0020 asks for)
struct BRIXWORLD_API FBrixWet
{
	TOptional<FVector3f> Coat;               // their coat's colour over the role's clothes
	TOptional<FVector3f> Hood, Umbrella;
	FString Sig;                             // a hurried walk, or running for cover
	double Phase = 0;
	TOptional<FVector4d> Arms[2];
	FVector2D Chest = FVector2D::ZeroVector;
};

// Resident i (who walks by SigName, of height S) at minute M: Rain and Snow how hard it rains and snows; Wardrobe what
// they wear: 0 today's, 1 a rain day's, 2 a snow day's (a kind of weather tried on a day without it); bCovered indoors,
// sheltering, or under a roof; bWalking and Piece (their walk under way: its start and end minute and length) for the
// pace. In snow (CITY-107): the hood up, else bare-headed, never an umbrella, and a careful walk. Returns what to draw.
BRIXWORLD_API FBrixWet BrixWetOf(const FBrixPeopleRules& People, const FBrixWear& W, int32 i, double Rain, double Snow, int32 Wardrobe, bool bCovered,
	bool bWalking, bool bOtherWalk, const FString& SigName, double S, double M, double WalkT0, double WalkT1, double WalkLen);
