#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BrixGameMode.generated.h"

struct FBrixCity;

// Starts on the engine's empty map and builds everything from data: the town (ABrixTownActor), the light, the player at
// the city's spawn. For checking without a person at the keys:
//   -BrixShot=<file.png>              take a screenshot after the town has settled, then quit
//   -BrixCam=<x>,<z>,<height>,<yaw>,<pitch>   look from there instead of the player (metres, degrees; yaw 0 = east)
UCLASS(Config = Game)
class BRIXTOWN_API ABrixGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABrixGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	UPROPERTY(Config) FString CityDir = TEXT("../World/city_01");   // from the project folder (Game/)
	UPROPERTY(Config) float SunPitch = -18.f;     // degrees; the morning sun of the spawn (07:20), low in the east
	UPROPERTY(Config) float SunYaw = 200.f;       // degrees; the way the light travels (0 = east)
	UPROPERTY(Config) float SunLux = 8.f;
	UPROPERTY(Config) FLinearColor SunColour = FLinearColor(1.f, 0.86f, 0.68f);   // ART_DIRECTION 2.4, kit daylight
	UPROPERTY(Config) float SkyLightIntensity = 1.f;  // the sky's fill in shade (with Lumen, also what it bounces)
	UPROPERTY(Config) float HazeDensity = 0.02f;
	UPROPERTY(Config) float HazeFalloff = 0.2f;       // how fast the haze thins with height
	UPROPERTY(Config) FLinearColor HazeColour = FLinearColor(0.45f, 0.45f, 0.45f);
	UPROPERTY(Config) float SkyHaziness = 1.f;        // the sky's Mie scattering, x the engine's: higher is paler, greyer
	UPROPERTY(Config) float ShotDelaySeconds = 12.f;

private:
	void SpawnLight();
	void ShootAndQuit();

	TSharedPtr<FBrixCity> City;
	uint64 MarkFrame = 0;
	double MarkSeconds = 0.0;
	UPROPERTY() TObjectPtr<AActor> Start;
	UPROPERTY() TObjectPtr<AActor> Eye;
};
