#include "BrixSoundActor.h"

#include "BrixCars.h"
#include "BrixCarsActor.h"
#include "BrixCharacter.h"
#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixGameMode.h"
#include "BrixPeople.h"
#include "BrixTalkActor.h"
#include "AudioDevice.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "TimerManager.h"
#include "BrixUnits.h"
#include "Components/AudioComponent.h"
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

void ABrixSound::Start(const FBrixCity& City, TSharedPtr<FBrixDay> InDay, ABrixPeople* InPeople, TSharedPtr<FBrixTraffic> InTraffic)
{
	Day = InDay;
	People = InPeople;
	Traffic = InTraffic;
	Tram = City.Tram;
	Pace = City.Sky.RealSecondsPerMinute;
	FString Error;
	if (!LoadBrixSoundRules(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), RulesFile)), Rules, Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("Brix Town: no sound (%s)"), *Error);
		return;
	}
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), SoundDir));
	TMap<FString, FBrixClipInfo> Index;
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
	for (const FBrixBlock& B : City.Blocks)
	{
		for (const FBrixLot& L : B.Lots) if (L.IsOpen())
		{
			const double A = FMath::DegreesToRadians(L.AngleDeg);
			OpenLots.Add({ L.Centre(), L.Size() * 0.5, FMath::Cos(A), FMath::Sin(A), L.Kind });
			if (L.Kind == TEXT("garden")) Gardens.Add(L.Centre());
		}
		for (const FBrixProp& P : B.Props) if (P.Kind == TEXT("fountain")) Fountains.Add(P.Pos);
	}
	if (Day)
	{
		const int32 Season = City.Sky.SeasonOf(Day->Day);
		if (City.Sky.Sunrise.IsValidIndex(Season)) SunriseH = City.Sky.Sunrise[Season];
		if (City.Sky.Sunset.IsValidIndex(Season)) SunsetH = City.Sky.Sunset[Season];
	}
	Rng.Initialize(20261125);
	StreetDayL = MakeLoop(TEXT("street_day"), false);
	StreetNightL = MakeLoop(TEXT("street_night"), false);
	RoomL = MakeLoop(TEXT("room"), false);
	CricketsL = MakeLoop(TEXT("crickets"), false);
	FountainL = MakeLoop(TEXT("fountain"), true);
	// on or off as the player last left it (SND-013)
	bool bSaved = true;
	GConfig->GetBool(TEXT("/Script/BrixTown.BrixSound"), TEXT("bSoundOn"), bSaved, GGameUserSettingsIni);
	SetOn(bSaved);
	if (FParse::Param(FCommandLine::Get(), TEXT("BrixSoundToggle")))   // for a test shot: off at 40 % of the wait, on again at 70 %
		if (const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			FTimerHandle A, B;
			GetWorldTimerManager().SetTimer(A, [this]() { SetOn(!bOn); }, Mode->ShotDelaySeconds * .4f, false);
			GetWorldTimerManager().SetTimer(B, [this]() { SetOn(!bOn); }, Mode->ShotDelaySeconds * .7f, false);
		}
	if (Day) ChildRole = Day->Roles.IndexOfByKey(FString(TEXT("child")));
	Placed = NewObject<USoundAttenuation>(this);
	Placed->Attenuation.bAttenuate = false;   // the slice's own fall-off (BrixHear) is in each sound's volume
	Placed->Attenuation.bSpatialize = true;
	if (EngineHz.Num() != 2) EngineHz = { 36, 76 };
	for (int32 k = 0; k < Rules.EngineVoices; ++k) { CarLo.Add(MakeLoop(TEXT("engine_lo"), true)); CarHi.Add(MakeLoop(TEXT("engine_hi"), true)); }
	MineLo = MakeLoop(TEXT("engine_lo"), false);
	MineHi = MakeLoop(TEXT("engine_hi"), false);
	Skid = MakeLoop(TEXT("car_skid"), false);
	for (int32 k = 0; k < 2; ++k) { Hum[k] = MakeLoop(TEXT("tram_hum"), true); Whine[k] = MakeLoop(TEXT("tram_whine"), true); Rumble[k] = MakeLoop(TEXT("tram_rumble"), true); }
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

ABrixSound::FLoop ABrixSound::MakeLoop(const FString& Name, bool bPlaced)
{
	FLoop L;
	L.Clip = Clips.FindRef(Name);
	L.bPlaced = bPlaced;
	if (!L.Clip) UE_LOG(LogTemp, Warning, TEXT("Brix Town: no recording %s; run Tools/unreal/sound_values.py"), *Name);
	return L;
}

void ABrixSound::SetLoop(FLoop& L, double Vol, double Pitch, const FVector2D& Pos)
{
	if (!L.Clip) return;
	UAudioComponent* C = LoopComps.IsValidIndex(L.Comp) ? LoopComps[L.Comp].Get() : nullptr;
	if (Vol < 1e-4)   // out of hearing: stopped, so it takes no voice
	{
		if (C && C->IsPlaying()) C->Stop();
		return;
	}
	if (!C)
	{
		C = NewObject<UAudioComponent>(this);
		C->bAutoActivate = false;
		C->bAutoDestroy = false;
		C->bAllowSpatialization = L.bPlaced;
		if (L.bPlaced) C->AttenuationSettings = Placed;
		C->RegisterComponent();
		L.Comp = LoopComps.Add(C);
	}
	if (L.bPlaced) C->SetWorldLocation(ToWorld(Pos, SourceUp));
	C->SetVolumeMultiplier((float)(Vol * Rules.Master * Rules.World / L.Clip->Scale));
	C->SetPitchMultiplier((float)FMath::Clamp(Pitch, .4, 2.0));
	if (!C->IsPlaying())
	{
		C->SetSound(MakeWave(L.Clip));   // a fresh wave each time it starts, its queue full from the loop's start
		C->Play();
	}
	const FString* Name = Clips.FindKey(L.Clip);
	if (Name) LoopTime.FindOrAdd(*Name) += GetWorld()->GetDeltaSeconds();
}

void ABrixSound::Engine(FLoop& Lo, FLoop& Hi, double Hz, double Vol, const FVector2D& Pos)
{
	// sound.js pitches one voice; here two recordings at a low and a high note share it, each pitched to the note, so the
	// timbre stays near the slice's (its filter does not move with the pitch) across the whole range
	const double W = FMath::Clamp((Hz - EngineHz[0]) / (EngineHz[1] - EngineHz[0]), 0.0, 1.0);
	SetLoop(Lo, Vol * FMath::Cos(W * HALF_PI), Hz / EngineHz[0], Pos);
	SetLoop(Hi, Vol * FMath::Sin(W * HALF_PI), Hz / EngineHz[1], Pos);
}

void ABrixSound::Vehicles(double Now, const FVector2D& Here)
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode) return;
	const double Minute = Mode->ClockMinute(), SecondsPerMinute = Pace / FMath::Max(.001f, Mode->ClockRate);
	const ABrixCars::FDrive* Drive = Mode->Cars ? &Mode->Cars->Driving() : nullptr;
	// the cars on a trip near, with their speed (cars.js near()); the car the player drives is their own; a bus they ride is heard as any other
	CarsHeard.Reset();
	if (Traffic)
		for (int32 k = 0; k < Traffic->Num(); ++k)
		{
			if (Drive && Drive->K == k && !Drive->bRide) continue;
			const TOptional<FBrixCarAt> C = Traffic->At(k, Minute);
			if (!C || !C->bTrip || FMath::Abs(C->Pos.X - Here.X) > CarsNear || FMath::Abs(C->Pos.Y - Here.Y) > CarsNear) continue;
			const double Dm = .02;
			const TOptional<FBrixCarAt> C2 = Traffic->At(k, Minute + Dm);
			CarsHeard.Add({ C->Pos, C2 ? FVector2D::Distance(C2->Pos, C->Pos) / (Dm * SecondsPerMinute) : 0.0, Traffic->Vehicles[k].Kind != 0 });
		}
	FBrixDriveHeard D;
	const bool bDriving = Drive && Drive->K >= 0 && !Drive->bRide;
	if (bDriving)
		if (const ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
		{
			D.V = Drive->V;
			D.bGas = Me->DriveKeys().Y > 0;
			D.bBrake = Me->Handbrake() || (Me->DriveKeys().Y < 0 && Drive->V > 0);
		}
	EngineSounds.Frame(Rules, CarsHeard, Here, bDriving ? &D : nullptr, Now, Events);
	for (int32 k = 0; k < EngineSounds.Pool.Num() && k < CarLo.Num(); ++k)
	{
		const FBrixEngineVoice& V = EngineSounds.Pool[k];
		Engine(CarLo[k], CarHi[k], V.Hz, V.bOn ? V.Gain * BrixHear(Rules, Here, 0, V.Pos, V.Far).X : 0, V.Pos);
	}
	Engine(MineLo, MineHi, EngineSounds.Mine.Hz, EngineSounds.Mine.Gain, Here);
	SetLoop(Skid, EngineSounds.Skid, 1, Here);
	// the trams
	TramsHeard.Reset();
	for (const FBrixTram::FAt& T : Tram.At(Minute)) TramsHeard.Add({ T.X, T.bStopped, T.Run });
	TramSounds.Frame(Rules, TramsHeard, Tram.Z, Here, Now, Events);
	for (int32 k = 0; k < 2; ++k)
	{
		const FBrixTramVoice& V = TramSounds.Voices[k];
		const double A = V.bOn ? V.Gain * BrixHear(Rules, Here, 0, V.Pos, V.Far).X : 0;
		SetLoop(Hum[k], A, 1, V.Pos);
		SetLoop(Whine[k], A * V.Whine, V.WhineHz / Rules.WhineHz, V.Pos);
		SetLoop(Rumble[k], A * V.Rumble, 1, V.Pos);
	}
}

void ABrixSound::SetOn(bool bNow)
{
	bOn = bNow;
	if (FAudioDeviceHandle Dev = GetWorld()->GetAudioDevice()) Dev->SetTransientPrimaryVolume(bOn ? 1.f : 0.f);   // everything, the voices too
	GConfig->SetBool(TEXT("/Script/BrixTown.BrixSound"), TEXT("bSoundOn"), bOn, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	UE_LOG(LogTemp, Display, TEXT("Brix Town: sound %s (N turns it %s)"), bOn ? TEXT("on") : TEXT("off"), bOn ? TEXT("off") : TEXT("on"));
}

void ABrixSound::Act(const FString& Name)
{
	FBrixSoundEvent E;
	E.Sound = Name.StartsWith(TEXT("car_")) ? Name : TEXT("act_") + Name;
	E.bHere = true;
	Play(E, Rules.Action);
}

// The sounds of the interface and of what the player does (SND-009), from what changed since last frame: a prompt
// appearing, a talk opening, an answer chosen, a talk closing; getting into a car (its door and starter) and out.
void ABrixSound::Interactions()
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode) return;
	if (const ABrixTalk* T = Mode->Talk)
	{
		if (T->IsTalking() && !bWasTalking) Act(TEXT("talk_open"));
		else if (!T->IsTalking() && bWasTalking) Act(TEXT("talk_close"));
		if (T->Answered != WasAnswered && T->IsTalking()) Act(TEXT("talk_choose"));
		if (!T->Offer().IsEmpty() && T->Offer() != WasPrompt && !T->IsTalking()) Act(TEXT("prompt"));
		bWasTalking = T->IsTalking();
		WasAnswered = T->Answered;
		WasPrompt = T->Offer();
	}
	if (const ABrixCars* C = Mode->Cars)
	{
		const ABrixCars::FDrive& D = C->Driving();
		if (D.K >= 0 && WasDriving < 0 && !D.bRide) Act(TEXT("car_in"));   // the door, then the starter (sound.js car("in"))
		else if (D.K < 0 && WasDriving >= 0 && !bWasRiding) Act(TEXT("car_door"));
		WasDriving = D.K;
		bWasRiding = D.bRide;
	}
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
	// engines and the trams (SND-002, SND-004, SND-009)
	Vehicles(Now, Here);
	// the town's air, crickets, fountains, birds (SND-001)
	Ambience.Frame(Rules, Mode ? Mode->ClockMinute() : 720, SunriseH, SunsetH, false, bDriving, Here, Fountains, Gardens, Dt, Rng, Events);
	SetLoop(StreetDayL, Ambience.Street * FMath::Sin(Ambience.StreetDay * HALF_PI), 1, Here);
	SetLoop(StreetNightL, Ambience.Street * FMath::Cos(Ambience.StreetDay * HALF_PI), 1, Here);
	SetLoop(RoomL, Ambience.Room, 1, Here);
	SetLoop(CricketsL, Ambience.Crickets, 1, Here);
	SetLoop(FountainL, Ambience.Fountain.bOn ? Ambience.Fountain.Gain * BrixHear(Rules, Here, 0, Ambience.Fountain.Pos, Ambience.Fountain.Far).X : 0, 1, Ambience.Fountain.Pos);
	Interactions();
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
		if (PC->WasInputKeyJustPressed(EKeys::N)) SetOn(!bOn);   // all sound off and on (SND-013; the slice's N)
	for (const FBrixSoundEvent& E : Events) Play(E, E.Sound.StartsWith(TEXT("act_")) || E.Sound == TEXT("car_bump") ? Rules.Action : Rules.World);
	if (Now > LogAt) { LogAt = Now + 10; LogPlayed(TEXT("so far")); }
}

void ABrixSound::LogPlayed(const TCHAR* When)
{
	TArray<FString> Names;
	Played.GetKeys(Names);
	Names.Sort();
	FString S;
	for (const FString& N : Names) S += FString::Printf(TEXT("%s%s %d"), S.IsEmpty() ? TEXT("") : TEXT(", "), *N, Played[N]);
	if (S.Len()) UE_LOG(LogTemp, Display, TEXT("Brix Town: sounds played %s: %s"), When, *S);
	LoopTime.GetKeys(Names);
	Names.Sort();
	S.Reset();
	for (const FString& N : Names) S += FString::Printf(TEXT("%s%s %.1f s"), S.IsEmpty() ? TEXT("") : TEXT(", "), *N, LoopTime[N]);
	if (S.Len()) UE_LOG(LogTemp, Display, TEXT("Brix Town: loops heard %s: %s"), When, *S);
}

void ABrixSound::EndPlay(const EEndPlayReason::Type Reason)
{
	LogPlayed(TEXT("in all"));
	Super::EndPlay(Reason);
}
