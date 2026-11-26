#include "BrixSoundActor.h"

#include "BrixCars.h"
#include "BrixCarsActor.h"
#include "BrixCharacter.h"
#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixGameMode.h"
#include "BrixLifeActor.h"
#include "Decoders/VorbisAudioInfo.h"
#include "Interfaces/IAudioFormat.h"
#include "BrixPeople.h"
#include "BrixTalkActor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
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
		bool bRead = FFileHelper::LoadFileToArray(File, *FPaths::Combine(Dir, KV.Value.File));
		if (bRead && KV.Value.File.EndsWith(TEXT(".ogg")))   // the music and the rewards: Ogg Vorbis, decoded once as the talk's voices are
		{
			LoadVorbisLibraries();
			FVorbisAudioInfo Info;
			FSoundQualityInfo Q;
			bRead = Info.ReadCompressedInfo(File.GetData(), File.Num(), &Q);
			if (bRead) { C->Pcm.SetNumZeroed(Q.SampleDataSize); Info.ExpandFile(C->Pcm.GetData(), &Q); C->Rate = Q.SampleRate; Ch = Q.NumChannels; }
		}
		else if (bRead) bRead = ReadBrixWav(File, C->Pcm, C->Rate, Ch);
		if (!bRead || Ch != 1)
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
	int32 LotIndex = 0;
	for (const FBrixBlock& B : City.Blocks)
	{
		for (const FBrixLot& L : B.Lots)   // the lots that can play music, in the city's order (CITY-013's RoomAt numbers them so)
			if (Rules.Places.Contains(L.Kind)) MusicLots.Add({ LotIndex++, L.Kind, L.Min, L.Max, L.Door }); else ++LotIndex;
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
		bMarketDay = BrixIsMarketDay(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/calendar.json"))), Day->Weekday);
	}
	for (const auto& KV : Clips)   // every mood and style of the music (recorded at the music bus's level: no share of the mix on top)
		if (KV.Key.StartsWith(TEXT("music_")))
		{
			const FString Style = KV.Key.Mid(6);
			bool bPlace = false;   // a place's music comes from its door; the score and the radio are the listener's own
			for (const auto& P : Rules.Places) bPlace |= P.Value.Style == Style;
			Tunes.Add(Style).Loop = MakeLoop(KV.Key, bPlace, false, &MixMusic);
		}
	Rng.Initialize(20261125);
	StreetDayL = MakeLoop(TEXT("street_day"), false, true);   // the street, crickets and fountains are out of doors (sound.js outside)
	StreetNightL = MakeLoop(TEXT("street_night"), false, true);
	RoomL = MakeLoop(TEXT("room"), false);
	CricketsL = MakeLoop(TEXT("crickets"), false, true);
	FountainL = MakeLoop(TEXT("fountain"), true, true);
	// on or off, and the levels, are the menu's settings (kept by ABrixLife between visits)
	if (FParse::Param(FCommandLine::Get(), TEXT("BrixSoundToggle")))   // for a test shot: off at 40 % of the wait, on again at 70 %
		if (const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			FTimerHandle A, B;
			GetWorldTimerManager().SetTimer(A, [this]() { Toggle(); }, Mode->ShotDelaySeconds * .4f, false);
			GetWorldTimerManager().SetTimer(B, [this]() { Toggle(); }, Mode->ShotDelaySeconds * .7f, false);
		}
	if (Day) ChildRole = Day->Roles.IndexOfByKey(FString(TEXT("child")));
	LoadBrixSyllableIndex(FPaths::Combine(Dir, TEXT("index.json")), SylHz, SylLen);
	{
		FBrixDialogue Unused;
		FString E2;
		bSpeech = LoadBrixTalk(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules"))),
			FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/talk.json"))), Speech, Unused, E2);
		if (!bSpeech) UE_LOG(LogTemp, Warning, TEXT("Brix Town: shouts and podcasts unvoiced (%s)"), *E2);
	}
	CrowdL = MakeLoop(TEXT("crowd_bed"), false, false, &MixVoice);
	HornL = MakeLoop(TEXT("car_horn"), false, false, &MixAction);
	BuildCaption();
	FParse::Value(FCommandLine::Get(), TEXT("BrixRadio="), Station);   // for a test shot: the car radio already on that station
	bTestHorn = FParse::Param(FCommandLine::Get(), TEXT("BrixHorn"));
	Station = FMath::Clamp(Station, 0, Rules.Radio.Num());
	Placed = NewObject<USoundAttenuation>(this);
	Placed->Attenuation.bAttenuate = false;   // the slice's own fall-off (BrixHear) is in each sound's volume
	Placed->Attenuation.bSpatialize = true;
	if (EngineHz.Num() != 2) EngineHz = { 36, 76 };
	for (int32 k = 0; k < Rules.EngineVoices; ++k) { CarLo.Add(MakeLoop(TEXT("engine_lo"), true, true)); CarHi.Add(MakeLoop(TEXT("engine_hi"), true, true)); }
	MineLo = MakeLoop(TEXT("engine_lo"), false);
	MineHi = MakeLoop(TEXT("engine_hi"), false);
	Skid = MakeLoop(TEXT("car_skid"), false);
	for (int32 k = 0; k < 2; ++k) { Hum[k] = MakeLoop(TEXT("tram_hum"), true, true); Whine[k] = MakeLoop(TEXT("tram_whine"), true, true); Rumble[k] = MakeLoop(TEXT("tram_rumble"), true, true); }
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

void ABrixSound::Play(const FBrixSoundEvent& E, double Bus, bool bOutside)
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
	const bool bMuffle = bOutside && InLot != INDEX_NONE;
	if (bMuffle) Vol *= Rules.MuffleGain;
	USoundWaveProcedural* W = MakeWave(C);
	UAudioComponent* A = E.bHere ? UGameplayStatics::SpawnSound2D(this, W, (float)Vol) : UGameplayStatics::SpawnSoundAtLocation(this, W, ToWorld(E.Pos, SourceUp), FRotator::ZeroRotator, (float)Vol, 1.f, 0.f, Placed);
	if (A && bMuffle) { A->SetLowPassFilterEnabled(true); A->SetLowPassFilterFrequency((float)Rules.MuffleHz); }
	Played.FindOrAdd(E.Sound)++;
}

ABrixSound::FLoop ABrixSound::MakeLoop(const FString& Name, bool bPlaced, bool bOutside, const double* Bus)
{
	FLoop L;
	L.Clip = Clips.FindRef(Name);
	L.bPlaced = bPlaced;
	L.bOutside = bOutside;
	L.Bus = Bus;
	if (!L.Clip) UE_LOG(LogTemp, Warning, TEXT("Brix Town: no recording %s; run Tools/unreal/sound_values.py"), *Name);
	return L;
}

void ABrixSound::SetLoop(FLoop& L, double Vol, double Pitch, const FVector2D& Pos, double Lowpass)
{
	if (!L.Clip) return;
	if (L.bOutside && InLot != INDEX_NONE)   // the listener is indoors: what is outside comes muffled through the walls (SND-007)
	{
		Vol *= Rules.MuffleGain;
		Lowpass = Lowpass > 0 ? FMath::Min(Lowpass, Rules.MuffleHz) : Rules.MuffleHz;
	}
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
		if (L.Bus == &MixMusic) { C->bIsMusic = true; C->bAlwaysPlay = true; }   // music is never the voice given up to a louder sound
		if (L.bPlaced) C->AttenuationSettings = Placed;
		C->RegisterComponent();
		L.Comp = LoopComps.Add(C);
	}
	if (L.bPlaced) C->SetWorldLocation(ToWorld(Pos, SourceUp));
	C->SetVolumeMultiplier((float)(Vol * Rules.Master * (L.Bus ? *L.Bus : MixWorld) / L.Clip->Scale));
	C->SetPitchMultiplier((float)FMath::Clamp(Pitch, .4, 2.0));
	C->SetLowPassFilterEnabled(Lowpass > 0);
	if (Lowpass > 0) C->SetLowPassFilterFrequency((float)Lowpass);
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

void ABrixSound::Toggle()
{
	if (const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this)))
		if (Mode->Life) Mode->Life->ToggleSound();   // the menu applies it to everything (the engine's primary volume) and keeps it
}

// The menu's levels for each part of the mix (sound.js mix(): a share of each bus's level from World/rules/sound.json)
void ABrixSound::ReadMix()
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const FBrixSettings* S = Mode && Mode->Life ? &Mode->Life->Settings : nullptr;
	auto Share = [S](const TCHAR* Key) { return S && S->Find(TEXT("audio"), Key) ? FMath::Clamp(S->Number(TEXT("audio"), Key) / 100.0, 0.0, 1.0) : 1.0; };
	MixWorld = Rules.World * Share(TEXT("world"));
	MixVoice = Rules.Voice * Share(TEXT("voice"));
	MixAction = Rules.Action * Share(TEXT("action"));
	MixMusic = Share(TEXT("music"));
}

void ABrixSound::Act(const FString& Name)
{
	FBrixSoundEvent E;
	E.Sound = Name.StartsWith(TEXT("car_")) ? Name : TEXT("act_") + Name;
	E.bHere = true;
	Play(E, MixAction);
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
		else if (!T->IsTalking() && bWasTalking) { Pending.RemoveAll([](const FPendingSyllable& P) { return P.bLine; }); Act(TEXT("talk_close")); }   // hush()
		if (T->Murmured != WasMurmured && T->IsTalking())   // a line with no voice yet, murmured (sound.js line(): the last one hushed first)
		{
			Pending.RemoveAll([](const FPendingSyllable& P) { return P.bLine; });
			Say(BrixMurmur(Rules, T->MurmurLine, T->MurmurWho, T->MurmurAt), GetWorld()->GetTimeSeconds(), false, true);
			Played.FindOrAdd(TEXT("(talk) lines murmured"))++;
		}
		WasMurmured = T->Murmured;
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

// Money coming in (SND-019 to SND-021): the quest and bank widgets' moments (ABrixLife::OnRewardSound), as sound.js reward()
void ABrixSound::OnReward(const FString& What, int32 I, int32 N)
{
	FBrixSoundEvent E;
	E.Sound = BrixRewardSound(Rules, What, I, N);
	E.bHere = true;
	Play(E, MixAction);
}

// The music (SND-010 to SND-012; sound.js music()): the score's mood, the nearest place playing, the car radio. Each
// is a loop eased to its level as the slice's bands fade; the score goes under a talk and under nearer music.
void ABrixSound::Music(double Dt, const FVector2D& Here)
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode || !Tunes.Num()) return;
	const double Minute = Mode->ClockMinute();
	// only the level asked for is cleared: a tune that is fading out keeps the distance and the muffling it was heard
	// with (cleared too, a cafe's music at the edge of its reach came back at full strength and clear for the seconds
	// of its fade each time the cafe was left behind)
	for (auto& KV : Tunes) KV.Value.Target = 0;
	double Near = 0;
	// music from places: the nearest open café or shop, the busker on a market day; clear inside, muffled from the street
	const FBrixMusicPlace P = BrixMusicPlace(Rules, MusicLots, Here, InLot, Minute, bMarketDay);
	if (FTune* T = P.Lot >= 0 ? Tunes.Find(P.Style) : nullptr)
	{
		T->Target = 1;
		T->Tc = .8;
		T->Pos = P.Pos;
		T->Hear = P.bInside ? 1.0 : BrixHear(Rules, Here, 0, P.Pos, P.Far).X;
		T->Lowpass = P.bInside || (P.bOpenAir && InLot == INDEX_NONE) ? 0.0 : Rules.MuffleHz * 1.6;
		// the score goes under this by how much of it is heard now, not by how near its door is: a muffled cafe that
		// has hardly begun took the score down to a tenth, and driving past one was a hole in the music
		Near = FMath::Max(Near, T->HearNow * T->Level * (T->Lowpass > 0 ? Rules.MuffleGain : 1.0));
	}
	const FString Place = P.Lot >= 0 ? P.Style + TEXT(" at lot ") + FString::FromInt(P.Lot) : FString();
	if (Place != WasPlace) { UE_LOG(LogTemp, Display, TEXT("Brix Town: music from a place: %s"), Place.IsEmpty() ? TEXT("none") : *Place); WasPlace = Place; }
	// the car radio (R while driving)
	const bool bDriving = Mode->Cars && Mode->Cars->Driving().K >= 0 && !Mode->Cars->Driving().bRide;
	if (bDriving)
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (PC->WasInputKeyJustPressed(EKeys::R))
			{
				Station = BrixNextStation(Rules, Station);
				Act(TEXT("radio"));
				UE_LOG(LogTemp, Display, TEXT("Brix Town: radio: %s"), Station ? *Rules.Radio[Station - 1].Name : TEXT("off"));
				CaptionUntil = 0;
			}
	if (bDriving && Station > 0)
	{
		const FBrixSoundRules::FStation& St = Rules.Radio[Station - 1];
		const FBrixSoundRules::FShow* Show = Rules.Shows.Find(St.Podcast);
		// a music station; or a podcast's bed under its hosts (sound.js: the bed at bed_gain x 2)
		if (FTune* T = Tunes.Find(Show ? Show->Bed : St.Style)) { T->Target = Show ? Rules.PodBedGain * 2 : 1; T->Tc = .3; T->Pos = Here; T->Hear = 1; T->Lowpass = 0; Near = 1; }
	}
	// the score by the time of day, under a talk and under nearer music
	const FString Mood = BrixMoodAt(Rules, Minute);
	if (Mood != WasMood) { UE_LOG(LogTemp, Display, TEXT("Brix Town: the score: %s"), *Mood); WasMood = Mood; }
	if (FTune* T = Tunes.Find(Mood)) { T->Target = BrixScoreLevel(Rules, Mode->Talk && Mode->Talk->IsTalking(), Near); T->Tc = 1.2; T->Pos = Here; T->Hear = 1; T->Lowpass = 0; }
	const double Ramp = 1 - FMath::Exp(-Dt / .2);
	for (auto& KV : Tunes)
	{
		FTune& T = KV.Value;
		T.Level += (T.Target - T.Level) * (1 - FMath::Exp(-Dt / FMath::Max(.05, T.Tc)));
		if (T.Level < 1e-3 && T.Target > 0) { T.HearNow = T.Hear; T.LowNow = T.Lowpass > 0 ? T.Lowpass : 18000.0; }   // starting: nothing to ramp from
		T.HearNow += (T.Hear - T.HearNow) * Ramp;
		T.LowNow += ((T.Lowpass > 0 ? T.Lowpass : 18000.0) - T.LowNow) * Ramp;
		SetLoop(T.Loop, T.Level * T.HearNow, 1, T.Pos, T.LowNow < 17000 ? T.LowNow : 0);
	}
}

// Says a speaker's syllables, each at its moment (from Now), from where they stand or through the car radio.
void ABrixSound::Say(const FBrixVoiceOut& V, double Now, bool bRadio, bool bLine)
{
	for (const FBrixSyllable& S : V.Syllables) Pending.Add({ Now + S.At, V.Pos, V.Far, S, bRadio, bLine });
}

// One syllable: the recording of its mouth shape at the nearest pitch and length, pitched the rest of the way.
void ABrixSound::PlaySyllable(const FPendingSyllable& P, const FVector2D& Here)
{
	if (!SylHz.Num() || !SylLen.Num()) return;
	double Hz = SylHz[0], Len = SylLen[0];
	for (double H : SylHz) if (FMath::Abs(FMath::Loge(H / P.S.Hz)) < FMath::Abs(FMath::Loge(Hz / P.S.Hz))) Hz = H;
	for (double L : SylLen) if (FMath::Abs(L - P.S.Len) < FMath::Abs(Len - P.S.Len)) Len = L;
	const TSharedPtr<FClip> C = Clips.FindRef(FString::Printf(TEXT("syl_%s_%d_%d"), *P.S.Shape, FMath::RoundToInt(Hz), FMath::RoundToInt(Len * 100)));
	if (!C) return;
	double Vol = Rules.Master * MixVoice * P.S.Gain / C->Scale;
	if (!P.bRadio)
	{
		const double G = BrixHear(Rules, Here, 0, P.Pos, P.Far).X;
		if (G < .004) return;
		Vol *= G;
	}
	const float Pitch = (float)FMath::Clamp(P.S.Hz / Hz, .4, 2.0);
	USoundWaveProcedural* W = MakeWave(C);
	UAudioComponent* A = P.bRadio ? UGameplayStatics::SpawnSound2D(this, W, (float)Vol, Pitch) : UGameplayStatics::SpawnSoundAtLocation(this, W, ToWorld(P.Pos, SourceUp + .6f), FRotator::ZeroRotator, (float)Vol, Pitch, 0.f, Placed);
	if (A && P.bRadio)   // through the radio's small speaker (sound.js: 160 Hz high-pass, 4.6 kHz low-pass)
	{
		A->SetHighPassFilterEnabled(true); A->SetHighPassFilterFrequency(160.f);
		A->SetLowPassFilterEnabled(true); A->SetLowPassFilterFrequency(4600.f);
	}
	++Spoken;
}

void ABrixSound::OffAir()
{
	Pending.RemoveAll([](const FPendingSyllable& P) { return P.bRadio; });
	OnAir.Reset();
}

// The car radio's caption and podcasts (SND-012, SND-014; sound.js podcast(), caption()): a music station's name shows a
// moment as it is tuned; a podcast's show and episode stay, with the host's line as they say it (or "…" in its intro).
void ABrixSound::Radio(double Now, bool bDriving)
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const FBrixSoundRules::FStation* St = bDriving && Station > 0 ? &Rules.Radio[Station - 1] : nullptr;
	const FBrixSoundRules::FShow* Show = St ? Rules.Shows.Find(St->Podcast) : nullptr;
	if (Show && Mode && bSpeech)
	{
		const FBrixOnAir A = BrixPodcastAt(Rules, Speech, St->Podcast, Mode->ClockMinute(), Pace);
		const FString Key = St->Podcast + TEXT("/") + A.Key;
		if (Key != OnAir)
		{
			OffAir();
			OnAir = Key;
			if (!A.bIntro)
			{
				FBrixVoiceOut V;
				BrixVoiceLine(Rules, A.Line, .02, A.Pitch, Rules.PodVoiceGain, A.From, V.Syllables);
				Say(V, Now, true);
			}
			else if (A.From < .6) { FBrixSoundEvent E; E.Sound = TEXT("podcast_intro"); E.bHere = true; Play(E, MixVoice); }
			UE_LOG(LogTemp, Display, TEXT("Brix Town: on the radio: %s / %s"), *A.Head, *A.Text);
		}
		CaptionHead = TEXT("Radio \u00b7 ") + A.Head;
		CaptionText = A.Text;
		CaptionUntil = 0;
		return;
	}
	if (!OnAir.IsEmpty()) OffAir();
	if (St)   // a music station: its name a moment (2.5 s)
	{
		if (CaptionUntil == 0) CaptionUntil = Now + 2.5;
		CaptionHead = Now < CaptionUntil ? TEXT("Radio \u00b7 ") + St->Name : FString();
		CaptionText.Reset();
	}
	else { CaptionHead.Reset(); CaptionText.Reset(); CaptionUntil = 0; }
}

void ABrixSound::BuildCaption()
{
	if (!GEngine || !GEngine->GameViewport || CaptionBox) return;
	const FSlateFontInfo Head = FCoreStyle::GetDefaultFontStyle("Regular", 9), Body = FCoreStyle::GetDefaultFontStyle("Regular", 12);
	SAssignNew(CaptionBox, SBox)
		.HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0, 0, 0, 150))
		.Visibility_Lambda([this]() { return CaptionHead.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
		[
			SNew(SBox).MaxDesiredWidth(640)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(FColor(23, 23, 28, 184))).Padding(FMargin(16, 8))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[ SNew(STextBlock).Font(Head).ColorAndOpacity(FLinearColor(FColor(167, 159, 140))).Text_Lambda([this]() { return FText::FromString(CaptionHead.ToUpper()); }) ]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 2, 0, 0)
					[
						SNew(STextBlock).Font(Body).ColorAndOpacity(FLinearColor(FColor(217, 207, 182))).AutoWrapText(true).Justification(ETextJustify::Center)
						.Visibility_Lambda([this]() { return CaptionText.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
						.Text_Lambda([this]() { return FText::FromString(CaptionText); })
					]
				]
			]
		];
	GEngine->GameViewport->AddViewportWidgetContent(CaptionBox.ToSharedRef(), 40);
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
	ReadMix();
	// indoors? (the slice: a lot under the listener, below 30 m)
	InLot = INDEX_NONE;
	if (const APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0)) if (Mode) InLot = Mode->RoomAt(Me->GetActorLocation());
	if (!bRewardsBound && Mode && Mode->Life) { Mode->Life->OnRewardSound.AddUObject(this, &ABrixSound::OnReward); bRewardsBound = true; }
	// the player's steps and landings (SND-003), on foot
	if (const ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
		if (!bDriving)
		{
			const double Speed = Me->GetVelocity().Size2D() / CmPerM;
			const UCharacterMovementComponent* M = Me->GetCharacterMovement();
			PlayerSounds.Frame(Rules, Speed > .1, M && M->IsMovingOnGround(), Me->WalkPhase(), Speed > (Me->WalkSpeed + Me->RunSpeed) * .5,
				Me->GetVelocity().Z / CmPerM, BrixStepSurface(InLot != INDEX_NONE, LotKindAt(Here)), Events);
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
			P.Shout = H.Shout;   // at the player's car (CITY-071)
			P.bRun = H.bRun;
			Heard.Add(P);
		}
		PeopleSounds.Frame(Rules, Heard, Here, Now, Events);
		// what they do and say (SND-006, SND-015, SND-016): the day's act and role of each
		const double Minute = Mode ? Mode->ClockMinute() : 720;
		for (int32 k = 0; k < Heard.Num(); ++k)
		{
			FBrixHeardPerson& P = Heard[k];
			if (Day->Role.IsValidIndex(P.Id) && Day->Roles.IsValidIndex(Day->Role[P.Id])) P.Role = Day->Roles[Day->Role[P.Id]];
			const TOptional<FBrixWhere> Wh = Day->Where(P.Id, Minute);
			if (Wh && Day->Acts.IsValidIndex(Wh->Act)) P.Act = Day->Acts[Wh->Act];
		}
		const ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
		const bool bGreet = !bDriving && Me && Me->GetVelocity().Size2D() > 10 && !(Mode && Mode->Talk && Mode->Talk->IsTalking());
		TArray<FBrixVoiceOut> Voices;
		PeopleTalk.Frame(Rules, Heard, Here, Now, Dt, bGreet, bSpeech ? &Speech : nullptr, Rng, Events, Voices);
		for (const FBrixVoiceOut& V : Voices) Say(V, Now, false);
		SetLoop(CrowdL, PeopleTalk.CrowdBed, 1, Here);
	}
	// engines and the trams (SND-002, SND-004, SND-009)
	Vehicles(Now, Here);
	// the town's air, crickets, fountains, birds (SND-001)
	Ambience.Frame(Rules, Mode ? Mode->ClockMinute() : 720, SunriseH, SunsetH, InLot != INDEX_NONE, bDriving, Here, Fountains, Gardens, Dt, Rng, Events);
	SetLoop(StreetDayL, Ambience.Street * FMath::Sin(Ambience.StreetDay * HALF_PI), 1, Here);
	SetLoop(StreetNightL, Ambience.Street * FMath::Cos(Ambience.StreetDay * HALF_PI), 1, Here);
	SetLoop(RoomL, Ambience.Room, 1, Here);
	SetLoop(CricketsL, Ambience.Crickets, 1, Here);
	SetLoop(FountainL, Ambience.Fountain.bOn ? Ambience.Fountain.Gain * BrixHear(Rules, Here, 0, Ambience.Fountain.Pos, Ambience.Fountain.Far).X : 0, 1, Ambience.Fountain.Pos);
	Interactions();
	Music(Dt, Here);
	Radio(Now, Mode && Mode->Cars && Mode->Cars->Driving().K >= 0 && !Mode->Cars->Driving().bRide);
	// the horn (SND-009): F held while driving
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const bool bInCar = Mode && Mode->Cars && Mode->Cars->Driving().K >= 0 && !Mode->Cars->Driving().bRide;   // no horn from a bus seat
		SetLoop(HornL, bInCar && (bTestHorn || (PC && PC->IsInputKeyDown(EKeys::F))) ? Rules.HornGain : 0, 1, Here);
	}
	// the syllables due now
	for (int32 k = 0; k < Pending.Num();)
		if (Pending[k].At <= Now) { PlaySyllable(Pending[k], Here); Pending.RemoveAtSwap(k); }
		else ++k;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
		if (PC->WasInputKeyJustPressed(EKeys::N)) Toggle();   // all sound off and on (SND-013; the slice's N)
	for (const FBrixSoundEvent& E : Events) Play(E, E.Sound.StartsWith(TEXT("act_")) || E.Sound == TEXT("car_bump") ? MixAction : MixWorld, E.Sound == TEXT("bird"));
	Played.FindOrAdd(TEXT("(people) groups talking")) = FMath::Max(Played.FindRef(TEXT("(people) groups talking")), PeopleTalk.Chat);
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
	if (Spoken) UE_LOG(LogTemp, Display, TEXT("Brix Town: syllables said %s: %d"), When, Spoken);
}

void ABrixSound::EndPlay(const EEndPlayReason::Type Reason)
{
	LogPlayed(TEXT("in all"));
	if (CaptionBox && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(CaptionBox.ToSharedRef());
	CaptionBox.Reset();
	Super::EndPlay(Reason);
}
