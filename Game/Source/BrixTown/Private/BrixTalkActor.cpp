#include "BrixTalkActor.h"
#include "BrixRooms.h"

#include "BrixCarsActor.h"
#include "BrixCharacter.h"
#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixGameMode.h"
#include "BrixLifeActor.h"
#include "BrixUnits.h"
#include "Components/AudioComponent.h"
#include "Decoders/VorbisAudioInfo.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/IAudioFormat.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundWaveProcedural.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

using BrixUnits::CmPerM;

namespace
{
	// the slice's colours for its talk box (index.html #talk): a dark panel, cream text, the lantern's orange for keys
	const FLinearColor Panel = FLinearColor::FromSRGBColor(FColor(23, 23, 28, 214));
	const FLinearColor Cream = FLinearColor::FromSRGBColor(FColor(240, 230, 208));
	const FLinearColor Dim = FLinearColor::FromSRGBColor(FColor(176, 168, 152));
	const FLinearColor Lantern = FLinearColor::FromSRGBColor(FColor(240, 163, 90));
	double Now(const UWorld* W) { return W->GetRealTimeSeconds(); }
	const TMap<FString, FString>& Days()
	{
		static const TMap<FString, FString> D = { { TEXT("Mon"), TEXT("Monday") }, { TEXT("Tue"), TEXT("Tuesday") }, { TEXT("Wed"), TEXT("Wednesday") },
			{ TEXT("Thu"), TEXT("Thursday") }, { TEXT("Fri"), TEXT("Friday") }, { TEXT("Sat"), TEXT("Saturday") }, { TEXT("Sun"), TEXT("Sunday") } };
		return D;
	}
}

ABrixTalk::ABrixTalk()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ABrixTalk::Start(const FBrixCity& City, TSharedPtr<FBrixDay> InDay, ABrixPeople* InPeople)
{
	Day = InDay;
	People = InPeople;
	CityName = City.Name;
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) LotNames.Add(L.Name);
	FString Error;
	const FString Rules = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules")));
	const FString Talk = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/talk.json")));
	if (!LoadBrixTalk(Rules, Talk, Speech, Dialogue, Error)) UE_LOG(LogTemp, Error, TEXT("Brix Town: no conversations (%s)"), *Error);
	Face = Speech.Rest();
	BuildUI();
}

void ABrixTalk::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GEngine && GEngine->GameViewport)
	{
		if (Box) GEngine->GameViewport->RemoveViewportWidgetContent(Box.ToSharedRef());
		if (Prompt) GEngine->GameViewport->RemoveViewportWidgetContent(Prompt.ToSharedRef());
	}
	Super::EndPlay(Reason);
}

// ---- the box at the foot of the screen, and the offer above it ("E  Talk to Cleo")

void ABrixTalk::BuildUI()
{
	if (!GEngine || !GEngine->GameViewport) return;
	auto Font = [](int32 Size) { return FCoreStyle::GetDefaultFontStyle("Regular", Size); };
	SAssignNew(Box, SBox)
		.HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(16, 0, 16, 36))
		.Visibility_Lambda([this]() { return IsTalking() ? EVisibility::Visible : EVisibility::Collapsed; })
		[
			SNew(SBox).WidthOverride(640.f)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel).Padding(FMargin(20, 16, 20, 12))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[ SNew(STextBlock).Font(Font(24)).ColorAndOpacity(Cream).Text_Lambda([this]() { return FText::FromString(Day && Day->Names.IsValidIndex(Res) ? Day->Names[Res] : FString()); }) ]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(12, 0, 0, 3)
						[ SNew(STextBlock).Font(Font(14)).ColorAndOpacity(Dim).Text_Lambda([this]() { const TMap<FString, FString> V = IsTalking() ? Vars() : TMap<FString, FString>(); const FString* R = V.Find(TEXT("role")); return FText::FromString(R ? *R : FString()); }) ]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 12)
					[ SNew(STextBlock).Font(Font(19)).ColorAndOpacity(Cream).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(Say); }) ]
					+ SVerticalBox::Slot().AutoHeight()
					[ SAssignNew(OptionList, SVerticalBox) ]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 8, 0, 0)
					[ SNew(STextBlock).Font(Font(13)).ColorAndOpacity(Dim).Text(FText::FromString(TEXT("1 to 9 or click to answer  ·  Esc leave"))) ]
				]
			]
		];
	SAssignNew(Prompt, SBox)
		.HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(16, 0, 16, 120))
		.Visibility_Lambda([this]() { return !IsTalking() && !PromptText.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel).Padding(FMargin(14, 6))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)[ SNew(STextBlock).Font(Font(16)).ColorAndOpacity(Lantern).Text(FText::FromString(TEXT("E"))) ]
				+ SHorizontalBox::Slot().AutoWidth()[ SNew(STextBlock).Font(Font(16)).ColorAndOpacity(Cream).Text_Lambda([this]() { return FText::FromString(PromptText); }) ]
			]
		];
	GEngine->GameViewport->AddViewportWidgetContent(Box.ToSharedRef(), 20);
	GEngine->GameViewport->AddViewportWidgetContent(Prompt.ToSharedRef(), 19);
}

void ABrixTalk::Refresh()
{
	if (!OptionList) return;
	OptionList->ClearChildren();
	if (!IsTalking()) return;
	const FBrixDialogue::FConversation* C = &Conv;
	const FBrixDialogue::FNode* N = C ? C->Nodes.Find(NodeId) : nullptr;
	if (!N) return;
	const TMap<FString, FString> V = Vars();
	for (int32 n = 0; n < Opts.Num(); ++n)
	{
		const FString Text = FBrixDialogue::Fill(N->Options[Opts[n]].Text, V);
		OptionList->AddSlot().AutoHeight()
		[
			SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).OnClicked_Lambda([this, n]() { Choose(n); return FReply::Handled(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 6, 10, 6)[ SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 14)).ColorAndOpacity(Lantern).Text(FText::AsNumber(n + 1)) ]
				+ SHorizontalBox::Slot().FillWidth(1).Padding(0, 6)[ SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 16)).ColorAndOpacity(Cream).AutoWrapText(true).Text(FText::FromString(Text)) ]
			]
		];
	}
}

// ---- the talk (conversations.js open(), show(), choose(), close(), step(); voices.js speak())

TMap<FString, FString> ABrixTalk::Vars() const
{
	TMap<FString, FString> V;
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Day || !Day->Names.IsValidIndex(Res) || !Mode)
	{
		if (bScene) { V.Add(TEXT("player"), Dialogue.PlayerName); if (const ABrixLife* L = Life()) V.Append(L->TalkVars()); }   // a scene: nobody to name but the player
		return V;
	}
	const double m = Mode->ClockMinute();
	const FString RoleName = Day->Roles.IsValidIndex(Day->Role[Res]) ? Day->Roles[Day->Role[Res]] : FString();
	const FString Act = Day->Acts.IsValidIndex(At.Act) ? Day->Acts[At.Act] : FString();
	const FString* Word = Dialogue.RoleWords.Find(RoleName);
	V.Add(TEXT("name"), Day->Names[Res]);
	V.Add(TEXT("role"), Day->Careers.IsValidIndex(Res) && !Day->Careers[Res].IsEmpty() ? Day->Careers[Res] : Word ? *Word : RoleName);
	V.Add(TEXT("place"), LotNames.IsValidIndex(At.Lot) && !LotNames[At.Lot].IsEmpty() ? LotNames[At.Lot] : CityName);
	const FString* What = At.bWalking ? Dialogue.Going.Find(Act) : Dialogue.Doing.Find(Act);
	V.Add(TEXT("doing"), What ? *What : At.bWalking ? FString(TEXT("on my way somewhere")) : Act);
	V.Add(TEXT("greeting"), m < 300 ? TEXT("Good evening") : m < 720 ? TEXT("Good morning") : m < 1080 ? TEXT("Good afternoon") : TEXT("Good evening"));
	const FString* WD = Days().Find(Day->Weekday);
	V.Add(TEXT("weekday"), WD ? *WD : Day->Weekday);
	V.Add(TEXT("season"), Day->Season.ToLower());
	V.Append(Conv.Vars);
	V.Add(TEXT("player"), Dialogue.PlayerName);   // the slice's Life.vars(): the designed character's name, until then "you"
	if (const ABrixLife* L = Life()) V.Append(L->TalkVars());
	return V;
}

void ABrixTalk::Open()
{
	if (IsTalking() || !People || !Day) return;
	const ABrixPeople::FCandidate C = People->Candidate();
	if (C.Resident < 0) return;
	const FString RoleName = Day->Roles.IsValidIndex(Day->Role[C.Resident]) ? Day->Roles[Day->Role[C.Resident]] : FString();
	const FString Act = Day->Acts.IsValidIndex(C.Act) ? Day->Acts[C.Act] : FString();
	const FString Cid = Dialogue.ConversationFor(C.Resident, RoleName, Act, C.bWalking);
	const FBrixDialogue::FConversation* Base = Dialogue.Conversations.Find(Cid);
	if (!Base) return;
	Conv = Life() ? Life()->Conversation(C.Resident, *Base) : *Base;   // with the quest lines and work Life adds (life.js conversation())
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	Res = C.Resident;
	At = C;
	ConvId = Cid;
	Minute0 = Mode ? Mode->ClockMinute() : 0;
	Face = Speech.Rest();
	PlayerFace = Speech.Rest();
	Quiet = Speech.Speak(TEXT(""));
	bAnswering = false;
	Flags.FindOrAdd(Res);
	ABrixPeople::FCandidate H = C;
	if (H.Pose == 1) H.Pose = 0;   // a walker stops and stands
	People->Held = H;
	People->HeldFace = &Face;
	if (ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		Me->BeginTalk(FVector(C.Pos.X, C.Pos.Y, 0) * CmPerM, CloseCamera);
		Me->SetTalkFace(&PlayerFace);
	}
	Show(Conv.Start);
}

void ABrixTalk::OpenScene(const FBrixDialogue::FConversation& Scene)
{
	if (IsTalking() || !People || !Day || !Scene.Nodes.Contains(Scene.Start)) return;
	Conv = Scene;
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	bScene = true;
	Res = -1;
	At = ABrixPeople::FCandidate();
	ConvId = TEXT("scene");
	Minute0 = Mode ? Mode->ClockMinute() : 0;
	Face = Speech.Rest();
	PlayerFace = Speech.Rest();
	Quiet = Speech.Speak(TEXT(""));
	bAnswering = false;
	if (ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		Me->BeginTalk(Me->GetActorLocation() + Me->GetActorForwardVector() * 150.f, CloseCamera);   // held where they stand, looking where they looked
		Me->SetTalkFace(&PlayerFace);
	}
	Show(Conv.Start);
}

void ABrixTalk::Show(const FString& Nid)
{
	if (Nid.IsEmpty()) { Close(); return; }
	const FBrixDialogue::FConversation* C = &Conv;
	const FBrixDialogue::FNode* N = C ? C->Nodes.Find(Nid) : nullptr;
	if (!N) { Close(); return; }
	NodeId = Nid;
	bNextPending = false;
	bAnswering = false;   // the resident speaks: the camera back over your shoulder
	if (ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0))) Me->SetAnswering(false);
	const TSet<FString>& F = Flags.FindOrAdd(Res);
	Opts.Reset();
	for (int32 k = 0; k < N->Options.Num(); ++k) if (FBrixDialogue::Allowed(N->Options[k], F) && (!Life() || Life()->Allowed(N->Options[k]))) Opts.Add(k);
	Say = FBrixDialogue::Fill(N->Say, Vars());
	Mood = N->Mood;
	Line = Speech.Speak(Say);   // the resident says the line: mouth, face and nods follow it
	T0 = Now(GetWorld());
	K = 1;
	const bool bTold = bScene || N->Who == TEXT("scene");   // a line nobody says is read, not spoken (QST-012)
	if (bTold) Line = Speech.Speak(TEXT(""));
	double Dur = -1;
	if (!bTold) SayAloud(Res, N->Say, Mood, Dur);   // spoken aloud (SND-018)
	if (Dur > 0) K = Line.End / FMath::Max(.3, Dur);   // the mouth finishes with the voice (CHR-009)
	else if (!bTold) { MurmurLine = Line; MurmurWho = Res; MurmurAt = People->Held.Pos; ++Murmured; }   // no voice yet: wordless syllables, as the slice does
	Refresh();
}

void ABrixTalk::Choose(int32 n)
{
	if (!IsTalking() || bNextPending || !Opts.IsValidIndex(n)) return;
	const FBrixDialogue::FConversation* C = &Conv;
	const FBrixDialogue::FNode* N = C ? C->Nodes.Find(NodeId) : nullptr;
	if (!N) return;
	const FBrixDialogue::FOption O = N->Options[Opts[n]];
	++Answered;
	for (const FString& X : O.Do) Flags.FindOrAdd(Res).Add(X);
	if (ABrixLife* L = Life()) L->Chose(Res, O);   // its quest acts, a job taken or handed over (life.js chose())
	double Dur = -1;
	SayAloud(-1, O.Text, TEXT("neutral"), Dur);   // you say your answer (SND-018)
	const FString Said = FBrixDialogue::Fill(O.Text, Vars());
	PlayerLine = Speech.Speak(Said);   // and your face says it: the mouth with the voice, or the text's own time when unvoiced
	PT0 = Now(GetWorld());
	PK = Dur > 0 ? PlayerLine.End / FMath::Max(.3, Dur) : 1.0;
	const double Len = Dur > 0 ? Dur : PlayerLine.End;
	if (Len <= 0) { Show(O.Next); return; }
	bAnswering = true;
	if (ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0))) Me->SetAnswering(true);
	Dur = Len;
	Opts.Reset();
	Say = TEXT("You: ") + Said;
	Mood = TEXT("neutral");
	Line = Speech.Speak(TEXT(""));   // the resident listens, mouth at rest
	T0 = Now(GetWorld());
	NextNode = O.Next;
	NextAt = T0 + Dur + AnswerPause;
	bNextPending = true;
	Refresh();
}

void ABrixTalk::Close()
{
	if (!IsTalking()) return;
	if (const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this)); Mode && Res >= 0)   // a scene holds nobody up
	{
		const double Spent = FMath::Fmod(Mode->ClockMinute() - Minute0 + 1440.0, 1440.0), Dl = People->Late.FindRef(Res) + Spent;
		if (Spent > 0 && Dl < 180) People->Late.Add(Res, Dl);   // held more than three hours: they just pick up the day where it is
		else People->Late.Remove(Res);
	}
	if (Voice) Voice->Stop();
	People->Held = ABrixPeople::FCandidate();
	People->HeldFace = nullptr;
	if (ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0))) Me->EndTalk();
	Res = -1;
	bScene = false;
	bNextPending = false;
	bAnswering = false;
	Say.Reset();
	Refresh();
	if (ABrixLife* L = Life()) L->TalkEnded();   // a counter asked for in the talk opens now
}

ABrixLife* ABrixTalk::Life() const
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	return Mode ? Mode->Life : nullptr;
}

void ABrixTalk::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!People || !Day) return;
	if (!IsTalking())
	{
		const ABrixPeople::FCandidate& C = People->Candidate();
		PromptText = C.Resident >= 0 && Day->Names.IsValidIndex(C.Resident) ? TEXT("Talk to ") + Day->Names[C.Resident] : FString();
		// a parked car nearer than anyone to talk to: the offer is to drive it; in a car, to get out
		const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
		const APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Mode && Mode->Cars && Me)
		{
			if (Mode->Cars->Driving().K >= 0)
			{
				PromptText = !Mode->Cars->Driving().bRide ? TEXT("Get out") : !Mode->Cars->CanGetOff() ? FString() : Mode->Cars->Driving().bTram ? TEXT("Get off the tram") : TEXT("Get off the bus");
				return;
			}
			double D = 0;
			const int32 CarK = Mode->Cars->TargetCar(FVector2D(Me->GetActorLocation().X, Me->GetActorLocation().Y) / CmPerM, D);
			if (CarK >= 0 && (C.Resident < 0 || D <= C.D)) PromptText = ABrixCars::IsTram(CarK) ? TEXT("Ride the tram") : Mode->Cars->IsBus(CarK) ? TEXT("Ride the bus") : Mode->Cars->IsVan(CarK) ? TEXT("Drive this van") : TEXT("Drive this car");
			if (const ABrixCharacter* Ch = Cast<ABrixCharacter>(Me))   // the playground (WLD-041): a go here, or off again
			{
				if (Ch->IsPlaying()) { PromptText = TEXT("Get off"); return; }
				if (Ch->IsSeated()) { PromptText = TEXT("Get up"); return; }
				FString Seat;   // a seat or a bed to try, in the mall or the furniture outlet (FURN-022)
				double SeatD = 0;
				if (Ch->SeatOffer(Seat, SeatD) && (C.Resident < 0 || SeatD <= C.D)) PromptText = Seat;
				double PD = 0;
				const int32 U = People->PlayTarget(Me->GetActorLocation(), PD);
				if (U != INDEX_NONE && (C.Resident < 0 || PD <= C.D) && (CarK < 0 || PD <= D)) PromptText = People->PlayArea().PlayerUses[U].Label;
			}
		}
		// a tower's lift call within reach (WLD-051): E takes it
		FString Lift;
		double LiftD = 0;
		if (Mode && Me && Mode->RoomsActor() && Mode->RoomsActor()->LiftOffer(Me->GetActorLocation(), Lift, LiftD)) PromptText = Lift;
		return;
	}
	if (bNextPending && Now(GetWorld()) >= NextAt) Show(NextNode);
	if (!IsTalking()) return;
	Speech.Blend(Face, Speech.Target(Line, Mood, (Now(GetWorld()) - T0) * K), Dt);   // the face follows the line
	Speech.Blend(PlayerFace, Speech.Target(bAnswering ? PlayerLine : Quiet, TEXT("neutral"), (Now(GetWorld()) - PT0) * PK), Dt);   // and yours, yours
	ABrixPeople::FCandidate& H = People->Held;
	if (H.Pose == 0)   // a standing resident turns to the player
		if (const APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			const FVector P = Me->GetActorLocation() / CmPerM;
			double da = FMath::Atan2(P.X - H.Pos.X, -(P.Y - H.Pos.Y)) - H.Yaw;
			da = FMath::Atan2(FMath::Sin(da), FMath::Cos(da));
			H.Yaw += da * FMath::Min(1.0, Dt * 5.0);
		}
}

void ABrixTalk::SayAloud(int32 Who, const FString& Template, const FString& InMood, double& OutDur)
{
	OutDur = -1;
	if (Voice) Voice->Stop();
	const TOptional<FBrixDialogue::FPlan> Plan = Dialogue.Plan(Who, Template, Vars(), InMood.IsEmpty() ? FString(TEXT("neutral")) : InMood);
	if (!Plan) return;   // nothing voiced: the mouth keeps the text's own time
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), Dialogue.VoiceDir));
	TArray<uint8> Pcm;
	auto Silence = [&](double Secs) { Pcm.AddZeroed(FMath::Max(0, (int32)(Secs * VoiceRate)) * 2 * VoiceChannels); };
	bool bAny = false;
	for (int32 k = 0; k < Plan->Parts.Num(); ++k)
	{
		const TPair<FString, double>& Part = Plan->Parts[k];
		if (k > 0) Silence(Dialogue.GapS);
		if (Part.Key.IsEmpty()) { Silence(Part.Value); continue; }
		TArray<uint8>* Got = Decoded.Find(Part.Key);
		if (!Got)   // a piece of Kokoro speech in Ogg Vorbis, decoded once with the engine's own decoder
		{
			TArray<uint8> Ogg, Out;
			if (FFileHelper::LoadFileToArray(Ogg, *FPaths::Combine(Root, Part.Key)))
			{
				LoadVorbisLibraries();
				FVorbisAudioInfo Info;
				FSoundQualityInfo Q;
				if (Info.ReadCompressedInfo(Ogg.GetData(), Ogg.Num(), &Q))
				{
					Out.SetNumZeroed(Q.SampleDataSize);
					Info.ExpandFile(Out.GetData(), &Q);
					VoiceRate = Q.SampleRate;
					VoiceChannels = Q.NumChannels;
				}
			}
			Got = &Decoded.Add(Part.Key, MoveTemp(Out));
		}
		if (Got->Num()) { Pcm.Append(*Got); bAny = true; }
		else Silence(Part.Value);
	}
	if (!bAny) return;
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(VoiceRate);
	Wave->NumChannels = VoiceChannels;
	Wave->Duration = (float)Pcm.Num() / (2.f * VoiceChannels * VoiceRate);
	Wave->SoundGroup = SOUNDGROUP_Voice;
	Wave->bLooping = false;
	Wave->QueueAudio(Pcm.GetData(), Pcm.Num());
	Voice = UGameplayStatics::SpawnSound2D(this, Wave, (float)Dialogue.Volume);
	OutDur = Wave->Duration;
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %s says \"%s\" aloud, %.2f s"), Who < 0 ? TEXT("the player") : *FString::FromInt(Who), *Template, OutDur);
}

double ABrixTalk::CandidateDistance() const
{
	return People && People->Candidate().Resident >= 0 ? People->Candidate().D : -1;
}
