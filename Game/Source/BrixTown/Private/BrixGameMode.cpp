#include "BrixGameMode.h"

#include "BrixCharacter.h"
#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixPeople.h"
#include "BrixTownActor.h"
#include "BrixUnits.h"
#include "Camera/CameraActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "RHI.h"
#include "UnrealClient.h"

using BrixUnits::ToWorld;

ABrixGameMode::ABrixGameMode()
{
	DefaultPawnClass = ABrixCharacter::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void ABrixGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	City = MakeShared<FBrixCity>();
	FString Error;
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), CityDir));
	if (!LoadBrixCity(Dir, *City, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Brix Town: the city did not load from %s: %s"), *Dir, *Error);
		City.Reset();
		return;
	}
	for (const FString& Problem : CheckBrixCity(*City)) UE_LOG(LogTemp, Warning, TEXT("Brix Town data: %s"), *Problem);
	// the clock: the city's spawn time on StartDay, or what the command line asks for
	Day = StartDay;
	FParse::Value(FCommandLine::Get(), TEXT("BrixDay="), Day);
	FString Time = City->SpawnTime;
	FParse::Value(FCommandLine::Get(), TEXT("BrixTime="), Time);
	FString Hh, Mm;
	if (Time.Split(TEXT(":"), &Hh, &Mm)) StartMinute = FCString::Atoi(*Hh) * 60.0 + FCString::Atoi(*Mm);
	// the residents' day: the same day the slice is built for
	Today = MakeShared<FBrixDay>();
	FString DayError;
	if (!LoadBrixDay(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), DayFile)), *Today, DayError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Brix Town: no residents (%s): run Tools/worldgen/build_play.py"), *DayError);
		Today.Reset();
	}
	else if (Today->Day != Day)
		UE_LOG(LogTemp, Warning, TEXT("Brix Town: the residents' routines are for day %d, the clock is on day %d"), Today->Day, Day);
}

AActor* ABrixGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (!City) return Super::ChoosePlayerStart_Implementation(Player);
	if (!Start)
	{
		const FVector At = ToWorld(City->SpawnPos, 1.2f);
		const FRotator Facing(0.f, FMath::RadiansToDegrees(FMath::Atan2(City->SpawnFacing.Y, City->SpawnFacing.X)), 0.f);
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Start = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), At, Facing, P);
	}
	return Start;
}

void ABrixGameMode::StartPlay()
{
	if (City)
	{
		ABrixTownActor* Town = GetWorld()->SpawnActor<ABrixTownActor>();
		Town->Build(*City);
		if (Today)
		{
			People = GetWorld()->SpawnActor<ABrixPeople>();
			People->Start(*City, Today);
			UE_LOG(LogTemp, Display, TEXT("Brix Town: %d residents, their day %d (%s, %s)"), Today->Num(), Today->Day, *Today->Weekday, *Today->Season);
		}
	}
	SpawnLight();
	Super::StartPlay();

	FString Cam;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixCam="), Cam, false))   // false: the value has commas
	{
		TArray<FString> P;
		Cam.ParseIntoArray(P, TEXT(","));
		if (P.Num() == 5)
		{
			const FVector At = ToWorld(FVector2D(FCString::Atof(*P[0]), FCString::Atof(*P[1])), FCString::Atof(*P[2]));
			Eye = GetWorld()->SpawnActor<ACameraActor>(At, FRotator(FCString::Atof(*P[4]), FCString::Atof(*P[3]), 0.f));
			// The player is possessed after this and would take the view back: look through the camera a moment later.
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, [this]()
			{
				if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
				{
					PC->bAutoManageActiveCameraTarget = false;
					PC->SetViewTarget(Eye);
				}
			}, 1.f, false);
		}
	}
	FString Shot;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixShot="), Shot))
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, this, &ABrixGameMode::ShootAndQuit, ShotDelaySeconds, false);
		FTimerHandle M;   // the frame rate is counted over the second half of the wait, after start-up work has settled
		GetWorldTimerManager().SetTimer(M, [this]() { MarkFrame = GFrameCounter; MarkSeconds = FPlatformTime::Seconds(); if (People) People->ResetTickTime(); }, ShotDelaySeconds * 0.5f, false);
	}
}

void ABrixGameMode::SpawnLight()
{
	// Lights are set up before they are first registered (deferred spawn): a sky light made movable and real-time after
	// registering keeps its first, empty capture and leaves the shade black. The sun is then turned by the clock (UpdateSun).
	UWorld* W = GetWorld();
	const FTransform SunAt(FRotator(-30.f, 180.f, 0.f));
	ADirectionalLight* Sun = W->SpawnActorDeferred<ADirectionalLight>(ADirectionalLight::StaticClass(), SunAt);
	UDirectionalLightComponent* SunLight = CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent());
	SunLight->Mobility = EComponentMobility::Movable;
	SunLight->Intensity = SunLux;
	SunLight->LightColor = SunColour.ToFColor(true);
	SunLight->bAtmosphereSunLight = true;
	Sun->FinishSpawning(SunAt);
	SunActor = Sun;
	StartSeconds = W->GetTimeSeconds();
	UpdateSun();
	if (City)
	{
		const FBrixSky& S = City->Sky;
		UE_LOG(LogTemp, Display, TEXT("Brix Town: day %d (%s), %02d:%02d"), Day, *S.Seasons[S.SeasonOf(Day)], (int32)StartMinute / 60, (int32)StartMinute % 60);
	}

	ASkyAtmosphere* Atmosphere = W->SpawnActor<ASkyAtmosphere>();
	USkyAtmosphereComponent* Air = Atmosphere->GetComponent();
	Air->SetMieScatteringScale(Air->MieScatteringScale * SkyHaziness);
	ASkyLight* Sky = W->SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity);
	USkyLightComponent* SkyLight = Sky->GetLightComponent();
	SkyLight->Mobility = EComponentMobility::Movable;
	SkyLight->bRealTimeCapture = true;
	SkyLight->Intensity = SkyLightIntensity;
	Sky->FinishSpawning(FTransform::Identity);
	SkyFill = SkyLight;
	SkyLight->RecaptureSky();
	FTimerHandle H;   // once the sky has been drawn: capture it again, and say what the sky light holds
	GetWorldTimerManager().SetTimer(H, [SkyLight]()
	{
		SkyLight->RecaptureSky();
		UE_LOG(LogTemp, Display, TEXT("Brix Town: sky light movable %d, real-time %d, intensity %.2f, visible %d"),
			SkyLight->Mobility == EComponentMobility::Movable, SkyLight->bRealTimeCapture, SkyLight->Intensity, SkyLight->IsVisible());
	}, 2.f, false);

	AExponentialHeightFog* Haze = W->SpawnActor<AExponentialHeightFog>();
	Haze->GetComponent()->SetFogDensity(HazeDensity);
	Haze->GetComponent()->SetFogHeightFalloff(HazeFalloff);
	Haze->GetComponent()->SetFogInscatteringColor(HazeColour);
	HazeFog = Haze->GetComponent();
	UpdateSun();
}

double ABrixGameMode::ClockMinutes() const
{
	const float Pace = City ? City->Sky.RealSecondsPerMinute : 10.f;
	return StartMinute + (GetWorld()->GetTimeSeconds() - StartSeconds) / FMath::Max(0.01, (double)Pace) * ClockRate;
}

void ABrixGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateSun();
}

// The sun where the calendar puts it for this minute of this day (WLD-043, ADR-0024; play.js lighting()): worked out from the
// clock each frame, never stored. Its light comes from no lower than sun_lift; it reddens as it sinks; it fades through the
// calendar's hour of dawn and dusk.
void ABrixGameMode::UpdateSun()
{
	if (!SunActor || !City) return;
	const FBrixSky& S = City->Sky;
	const double Minutes = ClockMinutes();
	const int32 DayNow = Day + (int32)FMath::Floor(Minutes / 1440.0);
	const double Minute = FMath::Fmod(Minutes, 1440.0);
	const int32 Season = S.SeasonOf(DayNow);
	FVector3d P = S.SunPlace(Season, S.HourAngle(Season, Minute));      // east +x, up +y, south +z
	P.Y = FMath::Max(P.Y, (double)S.SunLift);
	P.Normalize();
	const FVector ToSun(P.X, P.Z, P.Y);                                  // in Unreal: x east, y south, z up
	UDirectionalLightComponent* Light = CastChecked<UDirectionalLightComponent>(SunActor->GetLightComponent());
	SunActor->SetActorRotation((-ToSun).Rotation());
	const float Low = FMath::SmoothStep(0.05f, 0.5f, (float)P.Y);
	Light->SetLightColor(FMath::Lerp(SunDawnColour, SunColour, Low));
	const float Light01 = (float)S.Daylight(Season, Minute);
	Light->SetIntensity(SunLux * Light01);
	// the sky's fill and the haze's glow go down with the day, and the haze takes the low sun's colour (lighting(): fog and sky
	// mix towards the night's as the day goes)
	const float Fill = FMath::Lerp(NightFill, 1.f, Light01);
	if (SkyFill) SkyFill->SetIntensity(SkyLightIntensity * Fill);
	if (HazeFog) HazeFog->SetFogInscatteringColor(HazeColour * Fill * FMath::Lerp(SunDawnColour, FLinearColor::White, Low));
}

void ABrixGameMode::ShootAndQuit()
{
	FString Shot;
	FParse::Value(FCommandLine::Get(), TEXT("BrixShot="), Shot);
	FScreenshotRequest::RequestScreenshot(Shot, false, false);
	const double Secs = FPlatformTime::Seconds() - MarkSeconds;
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %.1f frames a second over the last %.0f s before the screenshot"), Secs > 0 ? (GFrameCounter - MarkFrame) / Secs : 0.0, Secs);
	UE_LOG(LogTemp, Display, TEXT("Brix Town: at the shot the game thread took %.1f ms, the render thread %.1f ms, the GPU %.1f ms"),
		FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
	if (People) UE_LOG(LogTemp, Display, TEXT("Brix Town: %d residents with a body at %02d:%02d, %.2f ms a frame working them out"), People->Shown(), (int32)ClockMinute() / 60, (int32)ClockMinute() % 60, People->TickMs());
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, []() { FPlatformMisc::RequestExit(false); }, 3.f, false);
}
