#include "BrixLamps.h"

#include "BrixGameMode.h"
#include "BrixTownActor.h"
#include "BrixUnits.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

using BrixUnits::ToWorld;
using BrixUnits::CmPerM;

ABrixLamps::ABrixLamps()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ABrixLamps::Start(ABrixTownActor* InTown)
{
	Town = InTown;
	for (int32 k = 0; k < MaxLights; ++k)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(RootComponent);
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetLightColor(Warm);
		L->SetAttenuationRadius(Reach * CmPerM);
		L->SetSourceRadius(15.f);
		L->SetCastShadows(false);   // twelve moving shadowed lights would cost more than the town
		L->SetVisibility(false);
		L->RegisterComponent();
		AddInstanceComponent(L);
		Lights.Add(L);
	}
}

void ABrixLamps::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!Town || !Mode || !PC || !PC->PlayerCameraManager) return;
	const float Night = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(1.f - Mode->Daylight / FMath::Max(NightBelow, .01f), 0.f, 1.f));
	if (Town->GlassMat && !FMath::IsNearlyEqual(Night, LastNight, .002f)) Town->GlassMat->SetScalarParameterValue(TEXT("Glow"), GlassGlow * Night);
	LastNight = Night;
	if (Night <= .01f) { for (UPointLightComponent* L : Lights) L->SetVisibility(false); return; }
	// the nearest lamps to the camera, as the slice picks its twelve
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation() / CmPerM;
	TArray<TPair<float, int32>> Near;
	const TArray<FVector4f>& All = Town->LampLights;
	for (int32 i = 0; i < All.Num(); ++i)
	{
		const float D2 = FMath::Square(All[i].X - (float)Eye.X) + FMath::Square(All[i].Z - (float)Eye.Y);
		if (D2 < 120.f * 120.f && All[i].W > 0) Near.Add({ D2, i });   // a broken lamp is out (CITY-073)
	}
	Near.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	for (int32 k = 0; k < Lights.Num(); ++k)
	{
		UPointLightComponent* L = Lights[k];
		if (k >= Near.Num()) { L->SetVisibility(false); continue; }
		const FVector4f& P = All[Near[k].Value];
		L->SetWorldLocation(ToWorld(FVector2D(P.X, P.Z), P.Y));
		L->SetIntensity(Candela * P.W * Night);
		L->SetVisibility(true);
	}
}
