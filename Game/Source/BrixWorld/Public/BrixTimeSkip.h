#pragma once

#include "CoreMinimal.h"

// Skip time (WLD-073, WLD-074; F-TIME-SKIP level 1; Tools/citysim/time_skip.py). A skip moves the world clock forward and
// nothing else: everything that changes with time is read from the clock. Steps and limits are World/rules/time_skip.json.
//
// The Unreal build shows one day's weather (Game/Data/day.json "weather", four six-hour slots) on every day, so "the next
// weather" here is the next slot of that day, going round it, whose weather is not now's: the weather the player will see.

struct BRIXWORLD_API FBrixSkipStep
{
	FString Id, Label;
	double Minutes = 0;            // a fixed step; 0 with bNextWeather
	bool bNextWeather = false;     // to the start of the next change of weather
};

struct BRIXWORLD_API FBrixTimeSkipRules
{
	TArray<FBrixSkipStep> Steps;
	double SlotMinutes = 360, MaxDays = 7;
	FString Who = TEXT("host");    // who may skip on a shared server
	TArray<FString> NotDuring;     // what stops a skip: "ride", "conversation"
};

BRIXWORLD_API bool LoadBrixTimeSkipRules(const FString& Path, FBrixTimeSkipRules& Out, FString& OutError);

// The start of the first slot after Minutes (counted from midnight of the first day) whose weather differs from now's,
// DayWeather going round; Minutes + MaxDays days if it never changes.
BRIXWORLD_API double BrixNextWeather(const FBrixTimeSkipRules& R, const TArray<FString>& DayWeather, double Minutes);

// The clock after a step, or a negative number for a step the rules do not have.
BRIXWORLD_API double BrixSkipTarget(const FBrixTimeSkipRules& R, const TArray<FString>& DayWeather, double Minutes, const FString& Step);

// The server's answer to a skip request (WLD-074): empty and OutMinutes the new clock, or why not ("not_host",
// "busy:<what>", "unknown_step") and OutMinutes the clock as it was.
BRIXWORLD_API FString BrixAskSkip(const FBrixTimeSkipRules& R, const TArray<FString>& DayWeather, double Minutes, const FString& Step,
	bool bAskerIsHost, const TArray<FString>& Busy, double& OutMinutes);
