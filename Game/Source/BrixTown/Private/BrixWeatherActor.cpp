#include "BrixWeatherActor.h"

#include "BrixCity.h"
#include "BrixGameMode.h"
#include "BrixTownActor.h"
#include "BrixUnits.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

using BrixUnits::CmPerM;

namespace
{
	const double Cell = 16;
	FIntPoint CellOf(const FVector2D& P) { return FIntPoint(FMath::FloorToInt(P.X / Cell), FMath::FloorToInt(P.Y / Cell)); }
	FString Data(const TCHAR* P) { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), P)); }
}

ABrixWeather::ABrixWeather()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Rain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Rain"));
	Rain->SetupAttachment(RootComponent);
	Rain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rain->SetCastShadow(false);
	RootComponent->SetMobility(EComponentMobility::Movable);
	Rain->SetMobility(EComponentMobility::Movable);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Weather/M_Rain.M_Rain"));   // Tools/unreal/make_rain_material.py
	RainMaterial = Mat.Object;
	Clouds = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(RootComponent);
	Clouds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Clouds->SetCastShadow(false);
	Clouds->SetMobility(EComponentMobility::Movable);
}

void ABrixWeather::Start(const FBrixCity& City, ABrixTownActor* InTown)
{
	Town = InTown;
	const ABrixGameMode* Mode0 = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	FString Error;
	bOn = LoadBrixWeatherRules(Data(TEXT("../World/rules/sky.json")), Rules, Error)
		&& LoadBrixWear(Data(TEXT("Data/day.json")), Data(TEXT("../World/rules/signatures.json")), Wear, Error);
	if (!bOn) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: %s; the weather stays clear"), *Error); return; }
	Wear.Pace = City.Tram.Pace;
	FParse::Value(FCommandLine::Get(), TEXT("BrixWeather="), Trying);
	if (!Trying.IsEmpty() && !Rules.Kinds.Contains(Trying)) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: no weather called %s"), *Trying); Trying.Reset(); }
	// the roofs: every building's footprint (not the open lots), turned as its lot is
	for (const FBrixBlock& B : City.Blocks)
		for (const FBrixLot& L : B.Lots)
		{
			if (L.IsOpen()) continue;
			const double A = FMath::DegreesToRadians(L.AngleDeg);
			const int32 k = Roofs.Add({ L.Centre(), L.Size() / 2, FMath::Cos(A), FMath::Sin(A) });
			const double Reach = L.Size().Size() / 2;
			const FIntPoint Lo = CellOf(L.Centre() - FVector2D(Reach)), Hi = CellOf(L.Centre() + FVector2D(Reach));
			for (int32 x = Lo.X; x <= Hi.X; ++x) for (int32 y = Lo.Y; y <= Hi.Y; ++y) RoofGrid.Add(FIntPoint(x, y), k);
		}
	for (const FBrixBlock& B : City.Blocks) Paving.Add(FBox2D(B.Origin, B.Origin + B.Extent));
	StartDay = Mode0 ? Mode0->StartDay : 16;
	// the drops: spread evenly over the rings' area, each with its own place in the fall, speed, length and brightness;
	// the nearest first, so those near enough to splash are the first SplashDrops
	FRandomStream Rn(16);
	for (int32 k = 0; k < MaxDrops; ++k)
	{
		const float R = FMath::Sqrt(FMath::Lerp(NearDrops * NearDrops, FarDrops * FarDrops, Rn.FRand()));
		Drops.Add({ Rn.FRand() * 2 * PI, R, Rn.FRand(), .85f + .3f * Rn.FRand(), .6f + .6f * Rn.FRand(), .7f + .6f * Rn.FRand() });
	}
	Drops.Sort([](const FDrop& A, const FDrop& B) { return A.R < B.R; });
	SplashDrops = 0;
	while (SplashDrops < Drops.Num() && Drops[SplashDrops].R < SplashReach) ++SplashDrops;
	// the clouds' dome: an upper half sphere round the camera, its inside drawn by M_Clouds
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Weather/M_Clouds.M_Clouds")))   // Tools/unreal/make_clouds_material.py
	{
		CloudMat = UMaterialInstanceDynamic::Create(M, this);
		TArray<FVector> V, N;
		TArray<int32> Tri;
		const int32 Around = 48, Rings = 16;
		for (int32 j = 0; j <= Rings; ++j)
			for (int32 i = 0; i <= Around; ++i)
			{
				const double el = (j / (double)Rings) * HALF_PI * .999 - .05, az = i / (double)Around * 2 * PI;   // a little below the horizon too
				const FVector D(FMath::Cos(el) * FMath::Cos(az), FMath::Cos(el) * FMath::Sin(az), FMath::Sin(el));
				V.Add(D * CloudDome * CmPerM);
				N.Add(-D);
			}
		for (int32 j = 0; j < Rings; ++j)
			for (int32 i = 0; i < Around; ++i)
			{
				const int32 a = j * (Around + 1) + i, b = a + 1, c = a + Around + 1, d = c + 1;
				Tri.Append({ a, c, b, b, c, d });
			}
		Clouds->CreateMeshSection_LinearColor(0, V, Tri, N, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false, false);
		Clouds->SetMaterial(0, CloudMat);
	}
	else UE_LOG(LogTemp, Warning, TEXT("Brix Town: /Game/Weather/M_Clouds is missing (run Tools/unreal/make_clouds_material.py); no clouds"));
	UE_LOG(LogTemp, Display, TEXT("Brix Town: the day's weather %s%s"), *FString::Join(Wear.Weather, TEXT(", ")), Trying.IsEmpty() ? TEXT("") : *(TEXT("; trying ") + Trying));
}

bool ABrixWeather::Roofed(const FVector2D& P) const
{
	TArray<int32> In;
	RoofGrid.MultiFind(CellOf(P), In);
	for (const int32 k : In)
	{
		const FRoof& R = Roofs[k];
		const FVector2D D = P - R.C, L(D.X * R.Cos + D.Y * R.Sin, -D.X * R.Sin + D.Y * R.Cos);   // into the lot's own frame
		if (FMath::Abs(L.X) <= R.Half.X && FMath::Abs(L.Y) <= R.Half.Y) return true;
	}
	return false;
}

void ABrixWeather::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!bOn || !Mode || !PC || !PC->PlayerCameraManager) return;
	Now = BrixWeatherNow(Rules, Wear.Weather, Mode->ClockMinute(), Trying);
	// the paving wet in the rain, drying after it, rings where the drops land (WLD-037)
	const float Wet = (float)Now.Wet, Rip = (float)(Now.Rain * Rules.Ripples);
	if (Town && (FMath::Abs(Wet - LastWet) > .004f || FMath::Abs(Rip - LastRip) > .004f))
	{
		Town->SetWet(Wet * Rules.WetDark, Wet * Rules.WetSheen, Wet * Rules.Puddles, Rip);
		LastWet = Wet; LastRip = Rip;
	}
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const float Day = FMath::Clamp(Mode->Daylight, 0.f, 1.f);
	// the clouds (WLD-035, WLD-036; ADR-0014): where the clock and the wind put them, lit by the sun of the hour
	if (CloudMat)
	{
		const double Mins = Mode->ClockMinutes();
		const FBrixCloudsNow Cl = BrixCloudsNow(Rules, Wear.Weather, StartDay + (int32)FMath::FloorToDouble(Mins / 1440.0), Mode->ClockMinute(), Trying);
		if (!SunLight) for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It) { SunLight = *It; break; }
		const FVector ToSun = SunLight ? -SunLight->GetActorForwardVector() : FVector::UpVector;
		const FLinearColor SunC = SunLight && SunLight->GetLightComponent() ? SunLight->GetLightComponent()->GetLightColor() : FLinearColor::White;
		const float SunLet = (float)Now.Sun;
		CloudMat->SetVectorParameterValue(TEXT("Clouds"), FLinearColor((float)Cl.Cover, (float)Rules.Soft, (float)Rules.Scale, (float)Cl.Phase));
		CloudMat->SetVectorParameterValue(TEXT("Offset"), FLinearColor((float)Cl.Offset.X, (float)Cl.Offset.Y, 0, 0));
		CloudMat->SetVectorParameterValue(TEXT("Sun"), FLinearColor(ToSun.X, ToSun.Y, ToSun.Z, 0));
		CloudMat->SetVectorParameterValue(TEXT("SunColour"), SunC * (Day * (.35f + .65f * SunLet)));   // the light that reaches them: less under a grey sky
		// the sky's light in their shade goes down with the day faster than the sun's on their lit side, and takes the low
		// sun's colour, so they turn pink at dusk and dark at night (ADR-0014)
		const float Sky = .03f + .97f * Day * Day;
		const FLinearColor Tint = FMath::Lerp(FLinearColor::White, SunC, .6f);
		CloudMat->SetVectorParameterValue(TEXT("Zenith"), CloudZenith * Sky);
		CloudMat->SetVectorParameterValue(TEXT("Horizon"), FMath::Lerp(CloudHorizon, CloudGrey, (float)Now.Grey) * Tint * Sky);
		CloudMat->SetVectorParameterValue(TEXT("LitShade"), FLinearColor((float)Rules.Lit, (float)Rules.Shade, 0, 0));
		CloudMat->SetScalarParameterValue(TEXT("Brightness"), CloudBrightness);
		Clouds->SetWorldLocation(FVector(Eye.X, Eye.Y, 0));
		Clouds->SetVisibility(Cl.Cover > 0);
	}
	// the rain: drops round the camera, not under a roof, none while the camera is indoors (ADR-0016). Each is a thin streak,
	// clear at its top and full at its foot, as a falling drop is seen; near the camera they fade, so none is a bar across
	// the picture; where one lands near by it throws up a small splash
	const int32 Count = FMath::Clamp(FMath::RoundToInt(Drops.Num() * Now.Rain), 0, Drops.Num());
	if (Count == 0 || Mode->RoomAt(Eye) != INDEX_NONE) { Rain->SetVisibility(false); return; }
	const FVector2D Here(Eye.X / CmPerM, Eye.Y / CmPerM);
	const double Up = Eye.Z / CmPerM, T = GetWorld()->GetTimeSeconds(), H = FallHeight, Len = Rules.Length;
	const float Glow = .3f + .7f * Day;
	const float A0 = (float)(Rules.Alpha * (2.f - Day));   // by night the drops catch the lamplight and stand out more
	TArray<FVector> V, N;
	TArray<FLinearColor> C;
	TArray<int32> Tri;
	const int32 Splashing = FMath::Min(SplashDrops, Count);
	V.Reserve(Count * 4 + Splashing * 12); N.Reserve(V.Max()); C.Reserve(V.Max()); Tri.Reserve(Count * 6 + Splashing * 18);
	const FVector Slant = FVector(Rules.Slant, 0, 0);   // the wind leans the drops a little (east)
	const FVector EyeM = Eye / CmPerM;
	auto Quad = [&](const FVector& A, const FVector& B, float W, float AlphaA, float AlphaB, float Bri)
	{
		const FVector To = (EyeM - A).GetSafeNormal2D(), Side = FVector(-To.Y, To.X, 0) * (W / 2);
		const FLinearColor Ca(DropColour.R * Glow * Bri, DropColour.G * Glow * Bri, DropColour.B * Glow * Bri, AlphaA), Cb(Ca.R, Ca.G, Ca.B, AlphaB);
		const int32 b = V.Num();
		V.Add((A - Side) * CmPerM); V.Add((A + Side) * CmPerM); V.Add((B + Side) * CmPerM); V.Add((B - Side) * CmPerM);
		N.Add(To); N.Add(To); N.Add(To); N.Add(To);
		C.Add(Ca); C.Add(Ca); C.Add(Cb); C.Add(Cb);
		for (const int32 i : { 0, 1, 2, 0, 2, 3 }) Tri.Add(b + i);
	};
	for (int32 k = 0; k < Count; ++k)
	{
		const FDrop& D = Drops[k];
		const FVector2D P = Here + FVector2D(FMath::Cos(D.A), FMath::Sin(D.A)) * D.R;
		const double Fall = FMath::Fmod(D.Phase * H + T * Rules.Speed * D.Speed, H), Bottom = Up + H * .6 - Fall;
		double G = 0;
		for (const FBox2D& B : Paving) if (B.IsInside(P)) { G = Town ? Town->BlockSlabHeight : .12; break; }
		const bool bRoof = Roofed(P), bFalling = !bRoof && Bottom > G;
		const float Near = FMath::SmoothStep(NearDrops, NearDrops + 2.f, D.R), W = DropWidth * (.7f + D.R / 8.f);
		const double L = Len * D.Len;
		const FVector Lo(P.X, P.Y, FMath::Max(Bottom, G)), Hi = Lo + (FVector(0, 0, 1) + Slant) * (bFalling ? L : 0.0);
		Quad(Lo, Hi, W, bFalling ? A0 * Near * D.Bri : 0.f, 0.f, D.Bri);
		if (k >= Splashing) continue;
		// the splash: three short jets thrown up and out where it landed, for SplashSeconds
		const double Since = bRoof ? 1e9 : (G - Bottom) / (Rules.Speed * D.Speed);
		const float u = (float)FMath::Clamp(Since / SplashSeconds, 0.0, 1.0), On = Since >= 0 && Since < SplashSeconds ? 1.f - u : 0.f;
		const FVector Foot(P.X, P.Y, G + .005);
		const FVector Out = (EyeM - Foot).GetSafeNormal2D(), Across(-Out.Y, Out.X, 0);
		const double Rise = (.03 + .05 * u) * On, Spread = (.02 + .07 * u) * On;
		for (const double sd : { -1.0, 0.0, 1.0 })
			Quad(Foot + Across * (sd * Spread * .3), Foot + Across * (sd * Spread) + FVector(0, 0, Rise * (sd == 0 ? 1.2 : .8)), W * .8f, A0 * 1.6f * On * D.Bri, 0.f, D.Bri * 1.2f);
	}
	static const TArray<FVector2D> NoUV;
	static const TArray<FProcMeshTangent> NoT;
	if (Tri.Num() != RainTris) { Rain->CreateMeshSection_LinearColor(0, V, Tri, N, NoUV, C, NoT, false, false); Rain->SetMaterial(0, RainMaterial); RainTris = Tri.Num(); }
	else Rain->UpdateMeshSection_LinearColor(0, V, N, NoUV, C, NoT, false);
	Rain->SetVisibility(true);
}
