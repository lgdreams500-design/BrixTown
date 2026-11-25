#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BrixGameMode.generated.h"

struct FBrixCity;

// Starts on the engine's empty map and builds everything from data: the town (ABrixTownActor), the light, the player at
// the city's spawn. For checking without a person at the keys:
//   -BrixShot=<file.png>              take a screenshot after the town has settled, then quit
//   -BrixCam=<x>,<z>,<height>,<yaw>,<pitch>   look from there instead of the player (metres, degrees; yaw 0 = east)
//   -BrixDay=<day> -BrixTime=<hh:mm>           start on another day or at another time (the sun follows)
UCLASS(Config = Game)
class BRIXTOWN_API ABrixGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABrixGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void Tick(float DeltaSeconds) override;
	// the clock: minutes since midnight of StartDay (past 1440 on the days after); everything that moves with time reads it
	double ClockMinutes() const;
	double ClockMinute() const { return FMath::Fmod(ClockMinutes(), 1440.0); }
	float Daylight = 1.f;                         // the sun's share of its light now, 0 at night (FBrixSky::Daylight)
	UPROPERTY() TObjectPtr<class ABrixTalk> Talk;    // conversations with residents (CITY-007)

	UPROPERTY(Config) FString CityDir = TEXT("../World/city_01");   // from the project folder (Game/)
	UPROPERTY(Config) FString DayFile = TEXT("Data/day.json");      // the residents' day, written by Tools/worldgen/build_play.py
	UPROPERTY(Config) int32 StartDay = 16;        // the day the game starts on (the slice is built for day 16); its time is the city's spawn time
	UPROPERTY(Config) float ClockRate = 1.f;      // times the calendar's pace (real_seconds_per_minute)
	UPROPERTY(Config) float SunLux = 8.f;         // in full day
	UPROPERTY(Config) FLinearColor SunColour = FLinearColor(1.f, 0.86f, 0.68f);   // ART_DIRECTION 2.4, kit daylight
	UPROPERTY(Config) FLinearColor SunDawnColour = FLinearColor(1.f, 0.42f, 0.22f); // a low sun's (the slice's lighting())
	UPROPERTY(Config) float SkyLightIntensity = 1.f;  // the sky's fill in shade (with Lumen, also what it bounces)
	UPROPERTY(Config) float HazeDensity = 0.02f;
	UPROPERTY(Config) float NightFill = 0.15f;        // the sky light and the haze's glow at night, as a share of the day's (lighting(): the night fill)
	UPROPERTY(Config) float HazeFalloff = 0.2f;       // how fast the haze thins with height
	UPROPERTY(Config) FLinearColor HazeColour = FLinearColor(0.45f, 0.45f, 0.45f);
	UPROPERTY(Config) float SkyHaziness = 1.f;        // the sky's Mie scattering, x the engine's: higher is paler, greyer
	UPROPERTY(Config) float ShotDelaySeconds = 12.f;

private:
	void SpawnLight();
	void UpdateSun();
	void ShootAndQuit();

	TSharedPtr<FBrixCity> City;
	TSharedPtr<struct FBrixDay> Today;           // the residents' routines (Game/Data/day.json)
	UPROPERTY() TObjectPtr<class ABrixPeople> People;
	TSharedPtr<struct FBrixTraffic> Traffic;     // the day's vehicles (Game/Data/cars.json)
	UPROPERTY() TObjectPtr<class ABrixCars> Cars;
	UPROPERTY() TObjectPtr<class ADirectionalLight> SunActor;
	UPROPERTY() TObjectPtr<class USkyLightComponent> SkyFill;
	UPROPERTY() TObjectPtr<class UExponentialHeightFogComponent> HazeFog;
	int32 Day = 16;
	double StartMinute = 440.0, StartSeconds = 0.0;
	uint64 MarkFrame = 0;
	double MarkSeconds = 0.0;
	UPROPERTY() TObjectPtr<AActor> Start;
	UPROPERTY() TObjectPtr<AActor> Eye;
};
