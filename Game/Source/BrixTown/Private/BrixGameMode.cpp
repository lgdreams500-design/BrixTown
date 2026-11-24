#include "BrixGameMode.h"

#include "BrixCharacter.h"
#include "BrixCity.h"
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
#include "UnrealClient.h"

using BrixUnits::ToWorld;

ABrixGameMode::ABrixGameMode()
{
	DefaultPawnClass = ABrixCharacter::StaticClass();
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
		GetWorldTimerManager().SetTimer(M, [this]() { MarkFrame = GFrameCounter; MarkSeconds = FPlatformTime::Seconds(); }, ShotDelaySeconds * 0.5f, false);
	}
}

void ABrixGameMode::SpawnLight()
{
	// Lights are set up before they are first registered (deferred spawn): a sky light made movable and real-time after
	// registering keeps its first, empty capture and leaves the shade black.
	UWorld* W = GetWorld();
	const FTransform SunAt(FRotator(SunPitch, SunYaw, 0.f));
	ADirectionalLight* Sun = W->SpawnActorDeferred<ADirectionalLight>(ADirectionalLight::StaticClass(), SunAt);
	UDirectionalLightComponent* SunLight = CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent());
	SunLight->Mobility = EComponentMobility::Movable;
	SunLight->Intensity = SunLux;
	SunLight->LightColor = SunColour.ToFColor(true);
	SunLight->bAtmosphereSunLight = true;
	Sun->FinishSpawning(SunAt);

	ASkyAtmosphere* Atmosphere = W->SpawnActor<ASkyAtmosphere>();
	USkyAtmosphereComponent* Air = Atmosphere->GetComponent();
	Air->SetMieScatteringScale(Air->MieScatteringScale * SkyHaziness);
	ASkyLight* Sky = W->SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity);
	USkyLightComponent* SkyLight = Sky->GetLightComponent();
	SkyLight->Mobility = EComponentMobility::Movable;
	SkyLight->bRealTimeCapture = true;
	SkyLight->Intensity = SkyLightIntensity;
	Sky->FinishSpawning(FTransform::Identity);
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
}

void ABrixGameMode::ShootAndQuit()
{
	FString Shot;
	FParse::Value(FCommandLine::Get(), TEXT("BrixShot="), Shot);
	FScreenshotRequest::RequestScreenshot(Shot, false, false);
	const double Secs = FPlatformTime::Seconds() - MarkSeconds;
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %.1f frames a second over the last %.0f s before the screenshot"), Secs > 0 ? (GFrameCounter - MarkFrame) / Secs : 0.0, Secs);
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, []() { FPlatformMisc::RequestExit(false); }, 3.f, false);
}
