#include "BrixSoundActor.h"

#include "BrixCarsActor.h"
#include "BrixCharacter.h"
#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixGameMode.h"
#include "BrixPeople.h"
#include "BrixUnits.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"

using namespace BrixUnits;

ABrixSound::ABrixSound()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // after the people and the player have moved this frame
}

void ABrixSound::Start(const FBrixCity& City, TSharedPtr<FBrixDay> InDay, ABrixPeople* InPeople)
{
	Day = InDay;
	People = InPeople;
	FString Error;
	if (!LoadBrixSoundRules(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), RulesFile)), Rules, Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("Brix Town: no sound (%s)"), *Error);
		return;
	}
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), SoundDir));
	TMap<FString, FBrixClipInfo> Index;
	TArray<double> EngineHz;
	if (!LoadBrixSoundIndex(FPaths::Combine(Dir, TEXT("index.json")), Index, EngineHz, Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("Brix Town: no sound (%s); run Tools/unreal/sound_values.py"), *Error);
		return;
	}
	int32 Bytes = 0;
	for (const auto& KV : Index)
	{
		TArray<uint8> File;
		TSharedPtr<FClip> C = MakeShared<FClip>();
		int32 Ch = 0;
		if (!FFileHelper::LoadFileToArray(File, *FPaths::Combine(Dir, KV.Value.File)) || !ReadBrixWav(File, C->Pcm, C->Rate, Ch) || Ch != 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("Brix Town: the recording %s does not read (is Git LFS installed?)"), *KV.Value.File);
			continue;
		}
		C->Scale = KV.Value.Scale;
		C->Seconds = (double)C->Pcm.Num() / (2.0 * C->Rate);
		C->bLoop = KV.Value.bLoop;
		Bytes += C->Pcm.Num();
		Clips.Add(KV.Key, C);
		int32 Cut = INDEX_NONE;
		if (KV.Key.FindLastChar(TEXT('_'), Cut) && KV.Key.Mid(Cut + 1).IsNumeric()) Variants.FindOrAdd(KV.Key.Left(Cut))++;
	}
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) if (L.IsOpen())
	{
		const double A = FMath::DegreesToRadians(L.AngleDeg);
		OpenLots.Add({ L.Centre(), L.Size() * 0.5, FMath::Cos(A), FMath::Sin(A), L.Kind });
	}
	if (Day) ChildRole = Day->Roles.IndexOfByKey(FString(TEXT("child")));
	Placed = NewObject<USoundAttenuation>(this);
	Placed->Attenuation.bAttenuate = false;   // the slice's own fall-off (BrixHear) is in each sound's volume
	Placed->Attenuation.bSpatialize = true;
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %d sounds recorded from the slice (%.1f MB)"), Clips.Num(), Bytes / 1048576.0);
}

TSharedPtr<ABrixSound::FClip> ABrixSound::Pick(const FString& Name) const
{
	if (const int32* N = Variants.Find(Name)) return Clips.FindRef(FString::Printf(TEXT("%s_%d"), *Name, FMath::RandRange(0, *N - 1)));
	return Clips.FindRef(Name);
}

USoundWaveProcedural* ABrixSound::MakeWave(const TSharedPtr<FClip>& C)
{
	USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
	W->SetSampleRate(C->Rate);
	W->NumChannels = 1;
	W->SoundGroup = SOUNDGROUP_Effects;
	if (C->bLoop)
	{
		W->Duration = INDEFINITELY_LOOPING_DURATION;
		W->bLooping = true;
		TSharedPtr<FClip> Keep = C;   // the audio thread asks for more when the queue runs low: the loop again
		W->OnSoundWaveProceduralUnderflow.BindLambda([Keep](USoundWaveProcedural* Wave, int32) { Wave->QueueAudio(Keep->Pcm.GetData(), Keep->Pcm.Num()); });
	}
	else
	{
		W->Duration = (float)C->Seconds;
		W->bLooping = false;
	}
	W->QueueAudio(C->Pcm.GetData(), C->Pcm.Num());
	return W;
}

FVector2D ABrixSound::Listener() const
{
	const APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0);
	return Me ? FVector2D(Me->GetActorLocation().X, Me->GetActorLocation().Y) / CmPerM : FVector2D::ZeroVector;
}

FString ABrixSound::LotKindAt(const FVector2D& P) const
{
	for (const FLotArea& L : OpenLots)
	{
		const FVector2D D = P - L.C, Q(D.X * L.Cos + D.Y * L.Sin, -D.X * L.Sin + D.Y * L.Cos);   // into the lot's own turn
		if (FMath::Abs(Q.X) <= L.Half.X && FMath::Abs(Q.Y) <= L.Half.Y) return L.Kind;
	}
	return FString();
}

void ABrixSound::Play(const FBrixSoundEvent& E, double Bus)
{
	const TSharedPtr<FClip> C = Pick(E.Sound);
	if (!C) { Played.FindOrAdd(TEXT("(missing) ") + E.Sound)++; return; }
	double Vol = Rules.Master * Bus * E.Gain / C->Scale;
	if (!E.bHere)
	{
		const FVector2D G = BrixHear(Rules, Listener(), 0, E.Pos, E.Far);
		if (G.X < .004) return;   // as sound.js once(): too faint to start
		Vol *= G.X;
	}
	USoundWaveProcedural* W = MakeWave(C);
	if (E.bHere) UGameplayStatics::PlaySound2D(this, W, (float)Vol);
	else UGameplayStatics::PlaySoundAtLocation(this, W, ToWorld(E.Pos, SourceUp), (float)Vol, 1.f, 0.f, Placed);
	Played.FindOrAdd(E.Sound)++;
}

void ABrixSound::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Clips.Num()) return;
	const UWorld* W = GetWorld();
	const double Now = W->GetTimeSeconds();
	const FVector2D Here = Listener();
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const bool bDriving = Mode && Mode->Cars && Mode->Cars->Driving().K >= 0;
	Events.Reset();
	// the player's steps and landings (SND-003), on foot
	if (const ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
		if (!bDriving)
		{
			const double Speed = Me->GetVelocity().Size2D() / CmPerM;
			const UCharacterMovementComponent* M = Me->GetCharacterMovement();
			PlayerSounds.Frame(Rules, Speed > .1, M && M->IsMovingOnGround(), Me->WalkPhase(), Speed > (Me->WalkSpeed + Me->RunSpeed) * .5,
				Me->GetVelocity().Z / CmPerM, BrixStepSurface(false, LotKindAt(Here)), Events);
		}
	// the residents near: their steps, sitting down and getting up (SND-017)
	if (People && Day)
	{
		Heard.Reset();
		for (const ABrixPeople::FHeard& H : People->Heard)
		{
			if (FVector2D::DistSquared(H.Pos, Here) > (double)HearPeople * HearPeople) continue;
			FBrixHeardPerson P;
			P.Id = H.Resident;
			P.Pos = H.Pos;
			P.Pose = H.Pose;
			P.bWalking = H.Pose == 1;
			P.Phase = H.Phase;
			P.bChild = ChildRole >= 0 && Day->Role.IsValidIndex(H.Resident) && Day->Role[H.Resident] == ChildRole;
			P.LotKind = LotKindAt(H.Pos);
			Heard.Add(P);
		}
		PeopleSounds.Frame(Rules, Heard, Here, Now, Events);
	}
	for (const FBrixSoundEvent& E : Events) Play(E, E.Sound.StartsWith(TEXT("act_")) ? Rules.Action : Rules.World);
	if (Now > LogAt) { LogAt = Now + 10; LogPlayed(TEXT("so far")); }
}

void ABrixSound::LogPlayed(const TCHAR* When)
{
	if (!Played.Num()) return;
	TArray<FString> Names;
	Played.GetKeys(Names);
	Names.Sort();
	FString S;
	for (const FString& N : Names) S += FString::Printf(TEXT("%s%s %d"), S.IsEmpty() ? TEXT("") : TEXT(", "), *N, Played[N]);
	UE_LOG(LogTemp, Display, TEXT("Brix Town: sounds played %s: %s"), When, *S);
}

void ABrixSound::EndPlay(const EEndPlayReason::Type Reason)
{
	LogPlayed(TEXT("in all"));
	Super::EndPlay(Reason);
}
