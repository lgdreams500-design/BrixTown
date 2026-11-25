#pragma once

#include "CoreMinimal.h"
#include "BrixBody.h"

// The weather (WLD-014, WLD-037; ADR-0016) and people in it (CITY-077 to CITY-080; ADR-0020). The day's weather is
// citysim.weather() for each six-hour slot (Game/Data/day.json "weather"); what it does to the light and the paving is
// worked out from the clock and World/rules/sky.json alone (BrixWeatherNow, a port of world.weather_now(), checked by
// WLD037_the_weather_is_worked_out_as_world_py_does). What each resident wears and carries today is citysim's
// (day.json "wear", City.wear()); BrixWet shows it: a coat, and out in the rain an umbrella up, else a hood, else a hand
// over the head when walking, and a quicker walk.

struct BRIXWORLD_API FBrixWeatherKind { double Sun = 1, Haze = 1, Veil = 0, Grey = 0, Rain = 0; };

struct BRIXWORLD_API FBrixWeatherRules
{
	TMap<FString, FBrixWeatherKind> Kinds;   // clear, cloud, rain, fog
	TMap<FString, double> Cover;             // how much of the sky the clouds cover, by kind
	double BlendMinutes = 40, OvercastFill = .2;
	bool bFollow = true;
	// rain: how the drops look and how wet the paving gets (ADR-0016)
	double Alpha = .26, Slant = .1, Speed = 9, Length = .8, WetDark = .4, WetSheen = .3, Puddles = .85, Ripples = 1, DryMinutes = 90;
};

BRIXWORLD_API bool LoadBrixWeatherRules(const FString& SkyPath, FBrixWeatherRules& Out, FString& OutError);

struct BRIXWORLD_API FBrixWeatherNow
{
	FString Kind = TEXT("clear");
	double Sun = 1, Haze = 1, Veil = 0, Grey = 0, Rain = 0, Wet = 0;
};

// world.weather_now(): DayWeather the day's four slots; Trying one kind for the whole day (a look at it), or empty.
BRIXWORLD_API FBrixWeatherNow BrixWeatherNow(const FBrixWeatherRules& R, const TArray<FString>& DayWeather, double Minute, const FString& Trying = FString());

// What the residents wear and carry today (day.json "wear"): per resident a coat's colour (or none), a hood, an
// umbrella's colour (or none), on the day as it is and on a wet day (shown when rain is tried on a dry day); the pace
// factors in the rain.
struct BRIXWORLD_API FBrixWear
{
	TArray<FString> Colours;                 // palette names
	struct FRow { int32 Coat = -1; bool bHood = false; int32 Umbrella = -1; };
	TArray<FRow> Today, Wet;
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

// Resident i (who walks by SigName, of height S) at minute M: Rain how hard it rains; bTrying rain tried on a dry day;
// bCovered indoors, sheltering, or under a roof; bWalking and Piece (their walk under way: its start and end minute
// and length) for the pace. Returns what to draw.
BRIXWORLD_API FBrixWet BrixWetOf(const FBrixPeopleRules& People, const FBrixWear& W, int32 i, double Rain, bool bTrying, bool bCovered,
	bool bWalking, bool bOtherWalk, const FString& SigName, double S, double M, double WalkT0, double WalkT1, double WalkLen);
