#include "BrixGameMode.h"

#include "BrixCharacter.h"
#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixCars.h"
#include "BrixCarsActor.h"
#include "BrixLamps.h"
#include "BrixLifeActor.h"
#include "BrixPeople.h"
#include "BrixTalkActor.h"
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
#include "Kismet/GameplayStatics.h"
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
		GetWorld()->SpawnActor<ABrixLamps>()->Start(Town);   // the lamps light at night (WLD-028)
		Traffic = MakeShared<FBrixTraffic>();
		FString CarError;
		const FString Data = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data")));
		if (LoadBrixTraffic(FPaths::Combine(Data, TEXT("cars.json")), FPaths::Combine(Data, TEXT("car_models.json")), *Traffic, CarError))
		{
			Cars = GetWorld()->SpawnActor<ABrixCars>();
			TArray<FBox2D> Paving;
			for (const FBrixBlock& B : City->Blocks) Paving.Add(FBox2D(B.Origin, B.Origin + B.Extent));
			Cars->Start(Traffic, City->Tram, Paving);
			UE_LOG(LogTemp, Display, TEXT("Brix Town: %d vehicles today"), Traffic->Num());
		}
		else UE_LOG(LogTemp, Warning, TEXT("Brix Town: no vehicles (%s); run Tools/worldgen/build_play.py"), *CarError);
		if (Today)
		{
			People = GetWorld()->SpawnActor<ABrixPeople>();
			People->Start(*City, Today, Cars ? Traffic : nullptr);
			UE_LOG(LogTemp, Display, TEXT("Brix Town: %d residents, their day %d (%s, %s)"), Today->Num(), Today->Day, *Today->Weekday, *Today->Season);
			Talk = GetWorld()->SpawnActor<ABrixTalk>();
			Talk->Start(*City, Today, People);
		}
	}
	if (City)
	{
		Life = GetWorld()->SpawnActor<ABrixLife>();   // money and the bank (ADR-0012)
		Life->Start(*City);
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
	// -BrixTalk=<resident> (with -BrixAnswers=1,2,...): to see a talk without anyone at the keys, the player is put beside
	// that resident a third of the way into a test shot's wait, the talk opened, and the answers given a second apart
	int32 TalkTo = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixTalk="), TalkTo) && TalkTo >= 0 && Today)
	{
		FString Answers;
		FParse::Value(FCommandLine::Get(), TEXT("BrixAnswers="), Answers, false);
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, [this, TalkTo, Answers]()
		{
			const TOptional<FBrixWhere> W = Today->Where(TalkTo, ClockMinute());
			APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0);
			if (!W || !Me) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: resident %d is not out to talk to"), TalkTo); return; }
			const FVector2D P = W->Pos + W->Heading * 1.4;   // in front of them
			Me->SetActorLocation(ToWorld(P, 1.f), false, nullptr, ETeleportType::TeleportPhysics);
			FTimerHandle O;
			GetWorldTimerManager().SetTimer(O, [this, Answers]()
			{
				if (!Talk) return;
				Talk->Open();
				UE_LOG(LogTemp, Display, TEXT("Brix Town: talking: %s"), Talk->IsTalking() ? TEXT("yes") : TEXT("nobody near enough"));
				TArray<FString> Picks;
				Answers.ParseIntoArray(Picks, TEXT(","));
				for (int32 k = 0; k < Picks.Num(); ++k)
				{
					FTimerHandle A;
					const int32 n = FCString::Atoi(*Picks[k]) - 1;
					GetWorldTimerManager().SetTimer(A, [this, n]() { if (Talk) Talk->Choose(n); }, 1.5f * (k + 1), false);
				}
			}, 0.3f, false);
		}, ShotDelaySeconds / 3, false);
	}
	// -BrixDrive=<vehicle> (with -BrixHold=steer,throttle): a third of the way into a test shot's wait the player is put by
	// that parked car's door, gets in as with E, and holds the keys from then on
	int32 DriveK = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixDrive="), DriveK) && DriveK >= 0 && Traffic)
	{
		FString Hold;
		FParse::Value(FCommandLine::Get(), TEXT("BrixHold="), Hold, false);
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, [this, DriveK, Hold]()
		{
			const TOptional<FBrixCarAt> C = Traffic->At(DriveK, ClockMinute());
			ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
			if (!C || !Me) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: vehicle %d is not in the town to drive"), DriveK); return; }
			const FVector3f D = Traffic->Dims(DriveK);
			const FVector2D Door = C->Pos - FVector2D(-C->Heading.Y, C->Heading.X) * (D.Y / 2 + .6);
			Me->SetActorLocation(ToWorld(Door, 1.f), false, nullptr, ETeleportType::TeleportPhysics);
			FTimerHandle O;
			GetWorldTimerManager().SetTimer(O, [this, Me, Hold]()
			{
				Me->TestPress();
				UE_LOG(LogTemp, Display, TEXT("Brix Town: driving: %s"), Cars && Cars->Driving().K >= 0 ? TEXT("yes") : TEXT("no car near enough"));
				TArray<FString> V;
				Hold.ParseIntoArray(V, TEXT(","));
				if (V.Num() == 2) Me->TestHold = FVector2D(FCString::Atof(*V[0]), FCString::Atof(*V[1]));
			}, 0.3f, false);
		}, ShotDelaySeconds / 3, false);
	}
	// -BrixCounter=bank (with -BrixLife=open,current;loan,5000,3...): a third of the way into a test shot's wait the ops are
	// done as the player would at the counter, then the player is put at its door and the counter opened
	FString CounterAt;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixCounter="), CounterAt) && Life)
	{
		FString Ops;
		FParse::Value(FCommandLine::Get(), TEXT("BrixLife="), Ops, false);
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, [this, CounterAt, Ops]()
		{
			if (!Life) return;
			if (!Ops.IsEmpty()) Life->RunOps(Ops);
			UE_LOG(LogTemp, Display, TEXT("Brix Town: the counter at %s: %s"), *CounterAt, Life->OpenCounterAt(CounterAt) ? TEXT("open") : TEXT("not there"));
		}, ShotDelaySeconds / 3, false);
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
	Daylight = Light01;
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
	FScreenshotRequest::RequestScreenshot(Shot, true, false);   // with what is on screen over the view (a talk's box)
	const double Secs = FPlatformTime::Seconds() - MarkSeconds;
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %.1f frames a second over the last %.0f s before the screenshot"), Secs > 0 ? (GFrameCounter - MarkFrame) / Secs : 0.0, Secs);
	UE_LOG(LogTemp, Display, TEXT("Brix Town: at the shot the game thread took %.1f ms, the render thread %.1f ms, the GPU %.1f ms"),
		FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
	if (Cars) UE_LOG(LogTemp, Display, TEXT("Brix Town: %d vehicles drawn"), Cars->Shown());
	if (Cars && Cars->Driving().K >= 0) UE_LOG(LogTemp, Display, TEXT("Brix Town: the player's car at (%.1f, %.1f) going %.1f m/s"), Cars->Driving().X, Cars->Driving().Z, Cars->Driving().V);
	if (People) UE_LOG(LogTemp, Display, TEXT("Brix Town: %d residents with a body at %02d:%02d, %.2f ms a frame working them out (%s)"), People->Shown(), (int32)ClockMinute() / 60, (int32)ClockMinute() % 60, People->TickMs(), *People->TickParts());
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, []() { FPlatformMisc::RequestExit(false); }, 3.f, false);
}
