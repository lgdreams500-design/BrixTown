#include "BrixLight.h"

#include "BrixCity.h"
#include "BrixGameMode.h"
#include "BrixInside.h"
#include "BrixLightRoom.h"
#include "BrixMall.h"
#include "BrixMallActor.h"
#include "BrixNight.h"
#include "BrixUnits.h"
#include "BrixWeatherActor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

using BrixUnits::ToWorld;

bool UBrixLight::SetsHaze()
{
	static const bool bSets = []()
	{
		FString No;
		FParse::Value(FCommandLine::Get(), TEXT("BrixLightNo="), No);
		return !FParse::Param(FCommandLine::Get(), TEXT("BrixLightOff")) && !No.Contains(TEXT("h"));
	}();
	return bSets;
}

void UBrixLight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* W = GetWorld();
	ABrixGameMode* Mode = W ? Cast<ABrixGameMode>(W->GetAuthGameMode()) : nullptr;
	if (!Mode || !Mode->CityData() || !W->HasBegunPlay()) return;
	if (!bLooked)
	{
		bLooked = true;
		bOff = FParse::Param(FCommandLine::Get(), TEXT("BrixLightOff"));
		FParse::Value(FCommandLine::Get(), TEXT("BrixLightNo="), Without);
		FString File;
		TArray<FString> Lines;
		if (FParse::Value(FCommandLine::Get(), TEXT("BrixTour="), File) && FFileHelper::LoadFileToStringArray(Lines, *File))
		{
			TourOut = FPaths::GetPath(File);
			FParse::Value(FCommandLine::Get(), TEXT("BrixTourOut="), TourOut);
			for (const FString& Line : Lines)
			{
				TArray<FString> P;
				Line.TrimStartAndEnd().ParseIntoArray(P, TEXT("|"), false);
				if (P.Num() < 4 || Line.TrimStart().StartsWith(TEXT("#"))) continue;
				FStop S;
				S.Name = P[0].TrimStartAndEnd();
				FString Hh, Mm;
				if (P[1].TrimStartAndEnd() == TEXT("ref")) S.bRefTime = true;
				else if (P[1].TrimStartAndEnd().Split(TEXT(":"), &Hh, &Mm)) S.Minute = FCString::Atoi(*Hh) * 60.0 + FCString::Atoi(*Mm);
				S.Weather = P[2].TrimStartAndEnd();
				FString Kind, Numbers;
				TArray<FString> N;
				if (!P[3].TrimStartAndEnd().Split(TEXT(":"), &Kind, &Numbers)) continue;
				Numbers.ParseIntoArray(N, TEXT(","));
				S.bCam = Kind == TEXT("cam");
				if (Kind == TEXT("in")) { S.Lot = N.Num() ? N[0] : Numbers; S.Pitch = N.Num() > 1 ? FCString::Atof(*N[1]) : -12.f; }   // in:<lot>[,<pitch>]
				else if (Kind == TEXT("refroom")) { S.RefRoom = Numbers; S.bCam = true; }
				else
				{
					if (N.Num() != (S.bCam ? 5 : 4)) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: the tour's stop %s has no place"), *S.Name); continue; }
					S.At = FVector(FCString::Atof(*N[0]), FCString::Atof(*N[1]), FCString::Atof(*N[2]));   // x, z, and how high
					S.Yaw = FCString::Atof(*N[3]);
					if (S.bCam) S.Pitch = FCString::Atof(*N[4]);
				}
				float EV = 0.f;
				if (P.Num() > 4 && FParse::Value(*P[4], TEXT("ev="), EV)) S.EV = EV;
				if (P.Num() > 4)   // the reference rooms' numbers to try: lamp=<times> fill=<level> tint=<r>/<g>/<b>
				{
					FParse::Value(*P[4], TEXT("lamp="), S.Lamp);
					FParse::Value(*P[4], TEXT("fill="), S.Fill);
					FParse::Value(*P[4], TEXT("win="), S.Win);
					FParse::Value(*P[4], TEXT("back="), S.Back);
					FParse::Value(*P[4], TEXT("pt="), S.PathSeconds);
					FString Tint;
					TArray<FString> C;
					if (FParse::Value(*P[4], TEXT("tint="), Tint) && Tint.ParseIntoArray(C, TEXT("/")) == 3) S.Tint = FLinearColor(FCString::Atof(*C[0]), FCString::Atof(*C[1]), FCString::Atof(*C[2]));
					if (FParse::Value(*P[4], TEXT("filltint="), Tint) && Tint.ParseIntoArray(C, TEXT("/")) == 3) S.FillTint = FLinearColor(FCString::Atof(*C[0]), FCString::Atof(*C[1]), FCString::Atof(*C[2]));
				}
				Stops.Add(S);
			}
			UE_LOG(LogTemp, Display, TEXT("Brix Town: a tour of %d stops, shots into %s"), Stops.Num(), *TourOut);
		}
	}
	FString RoomColours;
	if (!RefRooms && (FParse::Param(FCommandLine::Get(), TEXT("BrixLightRoom")) || FParse::Value(FCommandLine::Get(), TEXT("BrixLightRoom="), RoomColours)))
	{
		RefRooms = W->SpawnActor<ABrixLightRoom>();
		RefRooms->Start(*Mode, RoomColours == TEXT("game"));
	}
	if (RefRooms) RefRooms->Try(Mode->Daylight, TryLamp, TryTint, TryFill, TryFillTint, TryWin, TryBack, LowSunNow);   // their own light follows the day, as the rooms' does
	if (!bOff) Light(*Mode, DeltaSeconds);
	if (Stops.Num()) Tour(*Mode, DeltaSeconds);
}

void UBrixLight::Light(ABrixGameMode& Mode, float DeltaSeconds)
{
	UWorld* W = GetWorld();
	if (!Fog) for (TActorIterator<AExponentialHeightFog> It(W); It; ++It) { Fog = It->GetComponent(); break; }
	if (!Air) for (TActorIterator<ASkyAtmosphere> It(W); It; ++It) { Air = It->GetComponent(); break; }
	if (!Fog || !Air) return;   // the game mode has not made its light yet
	if (!Sun)
	{
		for (TActorIterator<ADirectionalLight> It(W); It; ++It) { Sun = *It; break; }
		DayGlow = Fog->DirectionalInscatteringLuminance;   // as the game mode's haze was made
	}
	const bool bEye = !Without.Contains(TEXT("e")), bHaze = !Without.Contains(TEXT("h")), bNight = !Without.Contains(TEXT("n"));
	if (!Holder)
	{
		Holder = W->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"));
		Root->SetMobility(EComponentMobility::Movable);
		Holder->SetRootComponent(Root);
		Root->RegisterComponent();
	}
	if (!Eye && bEye)
	{
		// a component, not an APostProcessVolume actor: the eye has no place, and needs no brush in the world
		Eye = NewObject<UPostProcessComponent>(Holder, TEXT("Eye"));
		Eye->SetupAttachment(Holder->GetRootComponent());
		Eye->bUnbound = true;
		Eye->Settings.bOverride_AutoExposureMinBrightness = true;
		Eye->RegisterComponent();
		Holder->AddInstanceComponent(Eye);
	}
	if (!NightLight && bNight)
	{
		// A component of its own, every property set before it is registered. (An ADirectionalLight actor registers as it
		// is spawned, deferred or not, with the engine's defaults: a white 10 lux sun, the air's first light. Properties set
		// on it afterwards reach the picture only when one of them is next changed through its setter, so such a light
		// shone as a second sun from the start of play until dusk first changed its strength.)
		NightLight = NewObject<UDirectionalLightComponent>(Holder, TEXT("NightLight"));
		NightLight->SetupAttachment(Holder->GetRootComponent());
		NightLight->Mobility = EComponentMobility::Movable;
		NightLight->Intensity = 0.f;
		NightLight->LightColor = NightColour.ToFColor(true);
		NightLight->bAtmosphereSunLight = true;                        // it lights the air: the night sky's blue, which the sky light gathers
		NightLight->AtmosphereSunLightIndex = 1;                       // (the sun is light 0)
		NightLight->AtmosphereSunDiskColorScale = FLinearColor::Black;   // no disc: the moon itself is not drawn yet (WLD-042)
		NightLight->SetRelativeRotation((-NightFrom.GetSafeNormal()).Rotation());
		NightLight->RegisterComponent();
		Holder->AddInstanceComponent(NightLight);
		UE_LOG(LogTemp, Display, TEXT("Brix Town: the eye opens %.1f stops at night (%.1f indoors) past EV %.1f; the night's light %.2f lux"), NightOpen, NightOpenIndoors, DayEV, NightLux);
	}
	// what the picture needs now (BrixWorld/BrixNight, WLD-042): worked out from the day's light, the sun's height, the
	// weather and whether the camera is in a room
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(W, 0);
	const bool bInRef = Cam && RefRooms && RefRooms->Contains(Cam->GetCameraLocation());
	const bool bIn = bInRef || (Cam && Mode.RoomAt(Cam->GetCameraLocation()) != INDEX_NONE);
	if (bInRef && Fog->FogDensity != 0.f) Fog->SetFogDensity(0.f);   // no haze in a room (the game mode knows only its own rooms)
	Indoors = FMath::FInterpConstantTo(Indoors, bIn ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(IndoorSeconds, .05f));
	FBrixNightRules R;
	R.DayEV = DayEV; R.DayOpen = DayOpen; R.DayOpenIndoors = DayOpenIndoors; R.DuskOpen = DuskOpen; R.NightOpen = NightOpen; R.NightOpenIndoors = NightOpenIndoors; R.NightBelow = NightBelow;
	R.NightHaze = NightHaze; R.LowSunBelow = LowSunBelow; R.LowSunHaze = LowSunHaze; R.LowSunGlow = LowSunGlow; R.NightLux = NightLux; R.NightSky = NightSky;
	FBrixLightNowIn In;
	In.Daylight = Mode.Daylight;
	In.SunUp = Sun ? (float)-Sun->GetActorForwardVector().Z : 1.f;
	In.SunLet = Mode.Weather && Mode.Weather->bOn ? (float)Mode.Weather->Now.Sun : 1.f;
	In.Indoors = Indoors;
	In.HazeColour = Mode.HazeColour; In.SunDawnColour = Mode.SunDawnColour; In.OvercastHaze = Mode.OvercastHaze;
	In.SunColourNow = Sun ? Sun->GetLightComponent()->GetLightColor() : FLinearColor::White;
	In.DayGlow = DayGlow;
	const FBrixLightNow N = BrixLightNow(R, In);
	LowSunNow = N.LowSun;

	// the eye
	if (Eye)
	{
		Eye->Settings.AutoExposureMinBrightness = PinEV ? *PinEV : N.MinEV;
		Eye->Settings.bOverride_AutoExposureMaxBrightness = PinEV.IsSet();
		if (PinEV) Eye->Settings.AutoExposureMaxBrightness = *PinEV;
	}
	// the haze: the game mode sets its own colour each frame before this ticks, so this is set after it, each frame it differs
	if (bHaze && !Fog->FogInscatteringLuminance.Equals(N.Haze, 0.f)) Fog->SetFogInscatteringColor(N.Haze);
	if (bHaze && !Fog->DirectionalInscatteringLuminance.Equals(N.Glow, .0005f)) Fog->SetDirectionalInscatteringColor(N.Glow);
	// the night's light and its sky
	if (!NightLight) return;
	if (!FMath::IsNearlyEqual(NightLight->Intensity, N.NightLux, .0005f)) NightLight->SetIntensity(N.NightLux);
	// Only the stronger of the sun and the night's light casts shadows: through all of dusk both were lit, and two
	// shadows of the whole town were drawn each frame. They change over where the two are equal, and both are faint.
	if (Sun && bOneSkyShadow)
	{
		ULightComponent* Day = Sun->GetLightComponent();
		const bool bNightCasts = N.NightLux > Day->Intensity * (NightLight->CastShadows ? .8f : 1.25f);
		if ((bool)NightLight->CastShadows != bNightCasts) NightLight->SetCastShadows(bNightCasts);
		if ((bool)Day->CastShadows == bNightCasts) Day->SetCastShadows(!bNightCasts);
	}
	if (!N.Sky.Equals(SkySet, .002f) && !Without.Contains(TEXT("s"))) { SkySet = N.Sky; Air->SetSkyLuminanceFactor(N.Sky); }
}

// -BrixTour: one stop after another, a shot at each
void UBrixLight::Tour(ABrixGameMode& Mode, float DeltaSeconds)
{
	UWorld* W = GetWorld();
	APlayerController* PC = W->GetFirstPlayerController();
	APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me || W->GetTimeSeconds() < 1.5) return;
	const double Now = FPlatformTime::Seconds();
	if (bPathing) DeltaSeconds = (float)(Now - LastReal);   // paused: the real time
	LastReal = Now;
	if (bPathing && !bShot && Now < PathUntil) return;
	StopWait -= DeltaSeconds;
	if (StopAt >= 0 && !bShot && StopWait < 2.f) { StopTime += DeltaSeconds; StopWorst = FMath::Max(StopWorst, DeltaSeconds); ++StopFrames; }
	if (StopWait > 0.f) return;
	if (StopAt >= 0 && !bShot && !bPathing && Stops[StopAt].PathSeconds > 0.f)   // a pt= stop, settled: the path tracer, the world held still
	{
		PC->SetPause(true);
		PC->ConsoleCommand(FString::Printf(TEXT("r.PathTracing.SamplesPerPixel %d"), PathSamples));
		PC->ConsoleCommand(TEXT("ShowFlag.PathTracing 1"));
		bPathing = true;
		PathUntil = Now + Stops[StopAt].PathSeconds;
		UE_LOG(LogTemp, Display, TEXT("Brix Town: tour stop %s: path tracing for %.0f s"), *Stops[StopAt].Name, Stops[StopAt].PathSeconds);
		return;
	}
	if (StopAt >= 0 && !bShot)   // this stop has had its wait: its shot, then a moment for it to be written
	{
		const FStop& S = Stops[StopAt];
		const FString File = FPaths::Combine(TourOut, S.Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(File, false, false);
		const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(W, 0);
		UE_LOG(LogTemp, Display, TEXT("Brix Town: tour stop %s at %02d:%02d, weather %s, the camera %s, daylight %.2f, %.1f frames a second"), *S.Name,
			(int32)Mode.ClockMinute() / 60, (int32)Mode.ClockMinute() % 60, Mode.Weather ? *Mode.Weather->Now.Kind : TEXT("-"),
			Cam && Mode.RoomAt(Cam->GetCameraLocation()) != INDEX_NONE ? TEXT("indoors") : TEXT("out of doors"), Mode.Daylight, StopTime > 0 ? StopFrames / StopTime : 0.f);
		UE_LOG(LogTemp, Display, TEXT("Brix Town: tour stop %s: the longest of its last %d frames %.0f ms"), *S.Name, StopFrames, StopWorst * 1000.f);
		// a golden room's grey card and ball: where they are in this picture, as shares of its width and height, for
		// Tools/unreal/card_read.py to read the light on them (ADR-0073)
		TArray<FVector> Corners;
		FVector Ball;
		float BallRadius = 0.f;
		int32 ViewX = 0, ViewY = 0;
		PC->GetViewportSize(ViewX, ViewY);
		if (!S.RefRoom.IsEmpty() && RefRooms && Cam && ViewX > 0 && ViewY > 0 && RefRooms->Card(S.RefRoom, Corners, Ball, BallRadius))
		{
			FBox2D On(ForceInit);
			FVector2D P, Mid, Rim;
			bool bAll = true;
			for (const FVector& C : Corners) { if (PC->ProjectWorldLocationToScreen(C, P)) On += P; else bAll = false; }
			bAll &= PC->ProjectWorldLocationToScreen(Ball, Mid) && PC->ProjectWorldLocationToScreen(Ball + Cam->GetCameraRotation().RotateVector(FVector::RightVector) * BallRadius, Rim);
			if (bAll)
				UE_LOG(LogTemp, Display, TEXT("Brix Town: tour stop %s: card %.4f,%.4f,%.4f,%.4f ball %.4f,%.4f,%.4f"), *S.Name,
					On.Min.X / ViewX, On.Min.Y / ViewY, On.Max.X / ViewX, On.Max.Y / ViewY, Mid.X / ViewX, Mid.Y / ViewY, FVector2D::Distance(Mid, Rim) / ViewX);
		}
		bShot = true;
		StopWait = 0.7f;
		return;
	}
	if (bPathing)   // the path-traced shot is written: back to the live light
	{
		PC->ConsoleCommand(TEXT("ShowFlag.PathTracing 0"));
		PC->SetPause(false);
		bPathing = false;
	}
	if (++StopAt >= Stops.Num())
	{
		UE_LOG(LogTemp, Display, TEXT("Brix Town: the tour is done, %d shots in %s"), Stops.Num(), *TourOut);
		Stops.Reset();
		FPlatformMisc::RequestExit(false);
		return;
	}
	const FStop& S = Stops[StopAt];
	PC->SetIgnoreLookInput(true);   // nobody is playing: the keys and mouse of whoever is at this machine do not move it
	PC->SetIgnoreMoveInput(true);
	if (S.bRefTime && RefRooms) Mode.SetClock(RefRooms->RefMinute);
	else if (S.Minute >= 0) Mode.SetClock(S.Minute);
	if (Mode.Weather) Mode.Weather->Trying = Mode.Weather->Rules.Kinds.Contains(S.Weather) ? S.Weather : FString();
	PinEV = S.EV;
	TryLamp = S.Lamp; TryFill = S.Fill; TryTint = S.Tint; TryFillTint = S.FillTint; TryWin = S.Win; TryBack = S.Back;
	if (S.bCam)
	{
		if (!TourCam)
		{
			TourCam = W->SpawnActor<ACameraActor>();
			TourCam->GetCameraComponent()->bConstrainAspectRatio = false;   // the whole window, no black bars
		}
		FTransform RefCam;
		float RefFov = ABrixLightRoom::Fov;
		if (!S.RefRoom.IsEmpty() && RefRooms && RefRooms->Camera(S.RefRoom, RefCam, &RefFov))
		{
			TourCam->SetActorTransform(RefCam);
			TourCam->GetCameraComponent()->SetFieldOfView(RefFov);
		}
		else
		{
			TourCam->SetActorLocationAndRotation(ToWorld(FVector2D(S.At.X, S.At.Y), (float)S.At.Z), FRotator(S.Pitch, S.Yaw, 0.f));
			TourCam->GetCameraComponent()->SetFieldOfView(90.f);
		}
		PC->bAutoManageActiveCameraTarget = false;
		PC->SetViewTarget(TourCam);
	}
	else
	{
		FVector2D At(S.At.X, S.At.Y);
		float Up = (float)S.At.Z, Yaw = S.Yaw;
		if (!S.Lot.IsEmpty())   // inside that building, where -BrixInside= puts the player: a way in from its doorway, facing in
		{
			const FBrixCity& City = *Mode.CityData();
			for (const FBrixBlock& B : City.Blocks)
				for (const FBrixLot& L : B.Lots)
					if (L.Id == S.Lot)
					{
						const FBrixDoorway Dw = BrixDoorwayOf(City, L, B.District, FBrixDoorRules::FromConfig());
						const bool bMall = Mode.Mall && Mode.Mall->Data() && Mode.Mall->Data()->LotId == S.Lot;
						At = Dw.Centre - Dw.Out * (bMall ? Mode.MallInside : FMath::Clamp((float)FMath::Min(L.Size().X, L.Size().Y) * 0.45f, 1.5f, 5.f));
						Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Dw.Out.Y, -Dw.Out.X));
						Up = 0.f;
					}
		}
		Me->SetActorLocationAndRotation(ToWorld(At, Up + 1.f), FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(FRotator(S.Lot.IsEmpty() ? -12.f : S.Pitch, Yaw, 0.f));
		PC->SetViewTarget(Me);
	}
	bShot = false;
	StopWait = TourSeconds;
	StopTime = StopWorst = 0.f;
	StopFrames = 0;
}
