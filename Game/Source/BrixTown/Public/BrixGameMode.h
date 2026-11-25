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
//   -BrixCounter=bank (with -BrixLife=<ops>)   stand at a counter and open it (ABrixLife)
//   -BrixInside=<lot id>              put the player inside that building, a way in from its doorway, facing in (CITY-013)
//   -BrixWalkIn=<lot id>              put the player outside that building's doorway and walk them in through it
//   -BrixStand=<x>,<z>,<yaw>          put the player there (metres, degrees; yaw 0 = east), the camera behind them
//   -BrixWalk=<x>,<z>;<x>,<z>;...   then walk the player through those points (up a stair, say); the log says how high they got
//   -BrixShut                         every building solid, as before the insides (to compare frame rates)
//   -BrixDesigner (-BrixDesignSet=hair:afro;skin:7) the character designer open for the shot; -BrixNoDesigner skips it in play
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
	void SetClock(double Minutes);               // a loaded save's day and time (QST-008): the clock runs on from there
	float Daylight = 1.f;                         // the sun's share of its light now, 0 at night (FBrixSky::Daylight)
	UPROPERTY() TObjectPtr<class ABrixTalk> Talk;    // conversations with residents (CITY-007)
	UPROPERTY() TObjectPtr<class ABrixCars> Cars;
	UPROPERTY() TObjectPtr<class ABrixCrash> Crash;
	UPROPERTY() TObjectPtr<class ABrixWeather> Weather;   // the day's weather: the light, the rain, wet streets (WLD-037)  // what the player's car breaks, and its repairs (CITY-072 to CITY-076)    // the day's vehicles, and the one the player drives
	UPROPERTY() TObjectPtr<class ABrixDesigner> Designer;   // the character designer, and who the player is (CHR-001)
	UPROPERTY() TObjectPtr<class ABrixLife> Life;    // the player's money, the bank, shops and quests (ADR-0012)
	UPROPERTY() TObjectPtr<class ABrixMap> Map;
	UPROPERTY() TObjectPtr<class ABrixMall> Mall;   // Eastgate Mall, inside and out (WLD-058)
	UPROPERTY() TObjectPtr<class ABrixMall> OutletHall;   // the furniture outlet's hall and car park (FURN-020)
	class ABrixPeople* PeopleActor() const { return People; }      // the minimap, the big map and the words over the street (UI-012, UI-018)

	UPROPERTY(Config) FString CityDir = TEXT("../World/city_01");   // from the project folder (Game/)
	UPROPERTY(Config) FString DayFile = TEXT("Data/day.json");      // the residents' day, written by Tools/worldgen/build_play.py
	UPROPERTY(Config) int32 StartDay = 16;        // the day the game starts on (the slice is built for day 16); its time is the city's spawn time
	UPROPERTY(Config) float ClockRate = 1.f;      // times the calendar's pace (real_seconds_per_minute)
	UPROPERTY(Config) float SunLux = 8.f;         // in full day
	UPROPERTY(Config) float MallInside = 30.f;    // m in from Eastgate's door that -BrixInside= puts the player
	UPROPERTY(Config) FLinearColor SunColour = FLinearColor(1.f, 0.86f, 0.68f);   // ART_DIRECTION 2.4, kit daylight
	UPROPERTY(Config) FLinearColor SunDawnColour = FLinearColor(1.f, 0.42f, 0.22f); // a low sun's (the slice's lighting())
	UPROPERTY(Config) float SkyLightIntensity = 1.f;  // the sky's fill in shade (with Lumen, also what it bounces)
	UPROPERTY(Config) float HazeDensity = 0.02f;
	UPROPERTY(Config) float NightFill = 0.15f;        // the sky light and the haze's glow at night, as a share of the day's (lighting(): the night fill)
	UPROPERTY(Config) float HazeFalloff = 0.2f;
	UPROPERTY(Config) FLinearColor OvercastHaze = FLinearColor(0.9f, 0.95f, 1.05f);   // the haze's tint under a covered sky (ADR-0016, ADR-0051)       // how fast the haze thins with height
	UPROPERTY(Config) FLinearColor HazeColour = FLinearColor(0.45f, 0.45f, 0.45f);
	UPROPERTY(Config) float SkyHaziness = 1.f;        // the sky's Mie scattering, x the engine's: higher is paler, greyer
	UPROPERTY(Config) float ShotDelaySeconds = 12.f;
	UPROPERTY(Config) float IndoorHaze = 0.f;        // the haze's density indoors, as a share of the street's (CITY-013)
	// the lot (in the city's order) whose inside a point is in, or INDEX_NONE (CITY-013)
	int32 RoomAt(const FVector& WorldCm) const;
	const FBrixCity* CityData() const { return City.Get(); }
	class ABrixRooms* RoomsActor() const { return Furnisher; }   // the furnished rooms near the camera, and the towers' lifts
	struct FBrixInsides* InsidesData() const { return Insides.Get(); }   // what is inside the buildings, and their designs (BUILD-001)
	class ABrixTownActor* Town() const { return TownActor; }          // the town, which draws a remodelled building again

private:
	void SpawnLight();
	void UpdateSun();
	void ShootAndQuit();

	TSharedPtr<FBrixCity> City;
	TSharedPtr<struct FBrixDay> Today;           // the residents' routines (Game/Data/day.json)
	UPROPERTY() TObjectPtr<class ABrixPeople> People;
	UPROPERTY() TObjectPtr<class ABrixTownActor> TownActor;
	UPROPERTY() TObjectPtr<class ABrixRooms> Furnisher;      // the homes near the camera, furnished (FURN-060)
	TSharedPtr<struct FBrixTraffic> Traffic;     // the day's vehicles (Game/Data/cars.json)
	TSharedPtr<struct FBrixInsides> Insides;     // what is inside the buildings (Game/Data/inside.json; CITY-013)
	void PutInside(const FString& LotId, bool bWalk);
	struct FRoom { int32 Lot = -1; FVector2D Centre = FVector2D::ZeroVector, Half = FVector2D::ZeroVector; double Cos = 1, Sin = 0; float Top = 3.f; };
	TArray<FRoom> Rooms;                         // every hollow building's inside, for RoomAt
	TOptional<FVector2D> WalkTo;                 // -BrixWalkIn: where the player is walked to, and from where
	FVector2D WalkFrom = FVector2D::ZeroVector;
	FString WalkLot;
	float WalkSeconds = 0.f;
	TArray<FVector2D> WalkPoints;                // -BrixWalk: the points still to walk to
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
