#include "BrixDesignerActor.h"

#include "BrixCharacter.h"
#include "BrixGameMode.h"
#include "BrixLifeActor.h"
#include "BrixTalkActor.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "BrixLook.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SViewport.h"

using namespace BrixLook;

namespace
{
	FString Proj(const TCHAR* P) { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), P)); }
	FLinearColor Lin(const FVector3f& C) { return FLinearColor(C.X, C.Y, C.Z, 1.f); }   // the designer's colours are linear (life.js rgb())
	FString Capital(const FString& S) { return S.IsEmpty() ? S : S.Left(1).ToUpper() + S.Mid(1); }
}

ABrixDesigner::ABrixDesigner()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;   // it is open while the clock stands still
}

void ABrixDesigner::Start(const FString& InCityName)
{
	CityName = InCityName;
	FString Error;
	bReady = LoadBrixDesigner(Proj(TEXT("../World/rules/designer.json")), Proj(TEXT("../World/rules/settings.json")), D, Error);
	if (!bReady) { UE_LOG(LogTemp, Error, TEXT("Brix Town: the character designer: %s"), *Error); return; }
	Look = D.Default;
	FString GarmentError;   // the clothes the outlets sell, for the wardrobe (CHR-016)
	if (!LoadBrixGarments(Proj(TEXT("Data/life.json")), Garments, GarmentError)) UE_LOG(LogTemp, Warning, TEXT("Brix Town: the wardrobe: %s"), *GarmentError);
	FString Set;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixDesignSet="), Set, false))   // key:value;key:value
	{
		TArray<FString> Parts;
		Set.ParseIntoArray(Parts, TEXT(";"));
		for (const FString& P : Parts)
		{
			FString K, V;
			if (!P.Split(TEXT(":"), &K, &V) || !SetValue(K.TrimStartAndEnd(), V.TrimStartAndEnd())) UE_LOG(LogTemp, Warning, TEXT("Brix Town: -BrixDesignSet: cannot set %s"), *P);
		}
		const TArray<FString> Problems = BrixLookProblems(D, Look);
		if (Problems.Num()) UE_LOG(LogTemp, Warning, TEXT("Brix Town: -BrixDesignSet: %s"), *FString::Join(Problems, TEXT("; ")));
	}
	FString Keep;   // -BrixOutfit=<name>,<name>: what is worn saved as outfits (CHR-015), for test shots of the wardrobe
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixOutfit="), Keep, false))
	{
		TArray<FString> Names;
		Keep.ParseIntoArray(Names, TEXT(","));
		for (const FString& N : Names) { FString Why; if (!Wardrobe.Save(D, Look, N, Why)) UE_LOG(LogTemp, Warning, TEXT("Brix Town: -BrixOutfit: %s"), *Why); }
	}
	FString SlotText;
	if (FFileHelper::LoadFileToString(SlotText, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BrixTown"), TEXT("slot.txt")))) Slot = FMath::Clamp(FCString::Atoi(*SlotText), 1, 9);
	FString ShotFile;
	bTransient = FParse::Value(FCommandLine::Get(), TEXT("BrixShot="), ShotFile) || FParse::Param(FCommandLine::Get(), TEXT("BrixNoSave"));
	int32 LoadK = 0;   // -BrixLoad=<slot>: carry on from that slot at once (a test shot of a loaded game)
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixLoad="), LoadK)) Slot = LoadK;
	bOffer = (!bTransient || FParse::Param(FCommandLine::Get(), TEXT("BrixDesigner"))) && ReadSlot(Slot).IsSet();   // a shot of the designer shows the offer too
	float SpinDeg = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixDesignSpin="), SpinDeg)) Spin = SpinDeg;
}

ABrixCharacter* ABrixDesigner::Me() const { return Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)); }

bool ABrixDesigner::SetValue(const FString& K, const FString& V)
{
	if (D.SliderRule(K)) { Look.Sliders.Add(K, FCString::Atod(*V)); return true; }
	if (K == TEXT("skin")) { Look.Skin = FCString::Atoi(*V); return true; }
	if (K == TEXT("hair_color")) { Look.HairColor = FCString::Atoi(*V); return true; }
	if (K == TEXT("eyes")) { Look.Eyes = FCString::Atoi(*V); return true; }
	if (K == TEXT("bag") || K == TEXT("tash")) { (K == TEXT("bag") ? Look.bBag : Look.bTash) = V == TEXT("1") || V == TEXT("true") || V == TEXT("yes"); return true; }
	const FString Val = V == TEXT("none") ? FString() : V;
	FString* S = K == TEXT("name") ? &Look.Name : K == TEXT("hair") ? &Look.Hair : K == TEXT("garment") ? &Look.Garment : K == TEXT("top") ? &Look.Top
		: K == TEXT("bottom_type") ? &Look.BottomType : K == TEXT("bottom") ? &Look.Bottom : K == TEXT("shoes") ? &Look.Shoes : K == TEXT("walk") ? &Look.Walk
		: K == TEXT("scarf") ? &Look.Scarf : K == TEXT("cap") ? &Look.Cap : K == TEXT("apron") ? &Look.Apron : nullptr;
	if (!S) return false;
	*S = Val;
	return true;
}

void ABrixDesigner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bReady) return;
	ABrixCharacter* C = Me();
	if (!bStarted && C)
	{
		bStarted = true;
		Apply();   // a test shot starts as this character, the default (life.js ?auto=1)
		FString Shot;
		const bool bShot = FParse::Value(FCommandLine::Get(), TEXT("BrixShot="), Shot);
		int32 LoadK = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("BrixLoad="), LoadK)) { FString Why; UE_LOG(LogTemp, Display, TEXT("Brix Town: -BrixLoad=%d: %s"), LoadK, LoadSlot(LoadK, Why) ? TEXT("loaded") : *Why); bBegun = true; }
		else if (FParse::Param(FCommandLine::Get(), TEXT("BrixDesigner")) || (!bShot && !FParse::Param(FCommandLine::Get(), TEXT("BrixNoDesigner"))))
		{
			Open();
			if (FParse::Param(FCommandLine::Get(), TEXT("BrixDesignBegin"))) BeginAt = FPlatformTime::Seconds() + 3.0;   // Begin pressed; the game's own shot after
			else if (bShot) ShotAt = FPlatformTime::Seconds() + ShotSeconds;
		}
		else Begun();
	}
	if (bOpen && C)
	{
		C->SetDesignView(true, Spin, ShareLeftOf(Body));
	}
	if (BeginAt > 0 && FPlatformTime::Seconds() >= BeginAt && bOpen)
	{
		BeginAt = -1;
		Look.Name = Look.Name.TrimStartAndEnd();
		UE_LOG(LogTemp, Display, TEXT("Brix Town: the character designer: Begin as %s"), *Look.Name);
		if (!BrixLookProblems(D, Look).Num()) Close();
	}
	if (bWardrobe && C) C->SetDesignView(true, WardrobeSpin, ShareLeftOf(WardrobeBox), false, WardrobeCameraM);   // the wardrobe: all of you, beside the page
	if (bBegun && !bOpen && CanSave() && FPlatformTime::Seconds() - SavedAt > AutosaveSeconds)   // life.js: saved as it goes
	{
		FString Why;
		if (!SaveSlot(Slot, Why)) UE_LOG(LogTemp, Warning, TEXT("Brix Town: the game could not save itself: %s"), *Why);
	}
	int32 SaveAs = 0;   // -BrixSaveAs=<slot>: a test shot saves itself there once, 6 s into the game (shots save nothing else)
	if (bBegun && !bSavedAs && FParse::Value(FCommandLine::Get(), TEXT("BrixSaveAs="), SaveAs) && FPlatformTime::Seconds() - BegunAt > 6.0)
	{
		bSavedAs = true;
		const bool bWas = bTransient;
		bTransient = false;
		FString Why;
		UE_LOG(LogTemp, Display, TEXT("Brix Town: -BrixSaveAs=%d: %s"), SaveAs, SaveSlot(SaveAs, Why) ? TEXT("saved") : *Why);
		bTransient = bWas;
	}
	if (ShotAt > 0 && FPlatformTime::Seconds() >= ShotAt)
	{
		ShotAt = -1;
		FString Shot;
		if (FParse::Value(FCommandLine::Get(), TEXT("BrixShot="), Shot)) FScreenshotRequest::RequestScreenshot(Shot, true, false);
		UE_LOG(LogTemp, Display, TEXT("Brix Town: the character designer, shot: %s, %s hair, %s; problems: %s"), *Look.Name, *Look.Hair, *Look.Garment,
			*FString::Join(BrixLookProblems(D, Look), TEXT("; ")));
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { FPlatformMisc::RequestExit(false); return false; }), 2.f);
	}
}

void ABrixDesigner::Begun()   // who the player is from now: their name in the talks and the menu (life.js vars())
{
	bBegun = true;
	BegunAt = FPlatformTime::Seconds();
	SavedAt = 0;   // a new game is saved at once in the playing slot (life.js newGame())
	if (const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		if (Mode->Life) Mode->Life->SetPlayerName(Look.Name);
		if (Mode->Talk) Mode->Talk->SetPlayerName(Look.Name);
	}
	OnBegin.Broadcast(Look);
}

void ABrixDesigner::CarryOn()
{
	FString Why;
	bOffer = false;
	if (!LoadSlot(Slot, Why)) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: the save could not be read, starting afresh: %s"), *Why); Look = D.Default; Apply(); Build(); return; }
	bOpen = false;
	if (GEngine && GEngine->GameViewport && Root) GEngine->GameViewport->RemoveViewportWidgetContent(Root.ToSharedRef());
	Root.Reset();
	Body.Reset();
	UGameplayStatics::SetGamePaused(this, false);
	if (ABrixCharacter* C = Me()) { C->SetDesignView(false, 0, 0); C->EndPanel(); }
	bBegun = true;
	SavedAt = FPlatformTime::Seconds();
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (Mode && Mode->Life) { Mode->Life->ShowHud(true); Mode->Life->Toast(TEXT("Welcome back, ") + Look.Name + TEXT(".")); }
}

FString ABrixDesigner::SlotPath(int32 K) const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BrixTown"), FString::Printf(TEXT("save_%d.json"), K));
}

void ABrixDesigner::UseSlot(int32 K)
{
	Slot = K;
	FFileHelper::SaveStringToFile(FString::FromInt(K), *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BrixTown"), TEXT("slot.txt")));
}

bool ABrixDesigner::CanSave() const { return !bTransient && bBegun && !bOpen; }

TOptional<FBrixSaveGame> ABrixDesigner::ReadSlot(int32 K) const
{
	FString Text, Why;
	FBrixSaveGame S;
	if (!FFileHelper::LoadFileToString(Text, *SlotPath(K)) || !S.FromString(Text, D, Why)) return {};
	return S;
}

bool ABrixDesigner::SaveSlot(int32 K, FString& OutWhy)   // life.js save()
{
	if (bTransient) { OutWhy = TEXT("a test run saves nothing"); return false; }
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Mode || !Mode->Life || !P) { OutWhy = TEXT("there is no game to save"); return false; }
	FBrixSaveGame S;
	S.Look = Look;
	const FVector At = P->GetActorLocation();
	S.Pos = FVector(At.X / 100.0, At.Z / 100.0, At.Y / 100.0);   // metres: x, up, the slice's z
	S.Yaw = FMath::DegreesToRadians(P->GetActorRotation().Yaw);
	Mode->Life->SaveTo(S);
	S.Outfits = Wardrobe.ToJson();
	S.At = FDateTime::UtcNow();
	SavedAt = FPlatformTime::Seconds();
	if (!FFileHelper::SaveStringToFile(S.ToString(), *SlotPath(K))) { OutWhy = TEXT("the save file could not be written"); return false; }
	UseSlot(K);
	return true;
}

bool ABrixDesigner::LoadSlot(int32 K, FString& OutWhy)   // life.js loadSave()
{
	FString Text;
	FBrixSaveGame S;
	if (!FFileHelper::LoadFileToString(Text, *SlotPath(K))) { OutWhy = TEXT("the slot is empty"); return false; }
	if (!S.FromString(Text, D, OutWhy)) return false;
	ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode || !Mode->Life || !Mode->Life->LoadFrom(S, OutWhy)) { if (OutWhy.IsEmpty()) OutWhy = TEXT("there is no game to load into"); return false; }
	Mode->SetClock(S.Day * 1440.0 + S.Minute);
	Look = S.Look;
	Apply();
	Wardrobe.Load(D, S.Outfits);
	if (Mode->Talk) Mode->Talk->SetPlayerName(Look.Name);
	Mode->Life->SetPlayerName(Look.Name);
	if (APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		P->SetActorLocationAndRotation(FVector(S.Pos.X * 100.0, S.Pos.Z * 100.0, S.Pos.Y * 100.0 + 10.0), FRotator(0, FMath::RadiansToDegrees(S.Yaw), 0), false, nullptr, ETeleportType::TeleportPhysics);
		if (APlayerController* PC = Cast<APlayerController>(P->GetController())) PC->SetControlRotation(FRotator(-12.0, FMath::RadiansToDegrees(S.Yaw), 0));
	}
	UseSlot(K);
	SavedAt = FPlatformTime::Seconds();
	return true;
}

void ABrixDesigner::EndPlay(const EEndPlayReason::Type Reason)
{
	if (CanSave()) { FString Why; SaveSlot(Slot, Why); }   // leaving: the game is kept as it is
	Super::EndPlay(Reason);
}

FString ABrixDesigner::SaveHeadline() const
{
	return bTransient ? FString(TEXT("A test run: nothing is saved")) : FString::Printf(TEXT("Playing slot %d · saves as you go"), Slot);
}

namespace
{
	FString When(const FDateTime& Utc)   // as the slice shows it: 25 Nov 13:23, in local time
	{
		static const TCHAR* Months[12] = { TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"), TEXT("May"), TEXT("Jun"), TEXT("Jul"), TEXT("Aug"), TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec") };
		const FDateTime L = Utc + (FDateTime::Now() - FDateTime::UtcNow());
		return FString::Printf(TEXT("%d %s %02d:%02d"), L.GetDay(), Months[FMath::Clamp(L.GetMonth(), 1, 12) - 1], L.GetHour(), L.GetMinute());
	}
}

void ABrixDesigner::SavePage(TSharedRef<SVerticalBox> Page, TFunction<void()> Redraw, TFunction<void()> CloseMenu)   // life.js saveTab() (UI-007, UI-013)
{
	if (bTransient) Page->AddSlot().AutoHeight().Padding(4, 0, 0, 10)[ Text(TEXT("This is a test run (a shot): nothing is saved."), 13, Bad()) ];
	if (!SaveMsg.IsEmpty()) Page->AddSlot().AutoHeight().Padding(4, 0, 0, 10)[ Text(SaveMsg, 13, Good()) ];
	for (int32 K = 1; K <= D.SaveSlots; ++K)
	{
		const TOptional<FBrixSaveGame> S = ReadSlot(K);
		const FString What = S ? FString::Printf(TEXT("%s · day %d · saved %s"), *S->Look.Name, S->Day + 1, *When(S->At)) : FString(TEXT("Empty"));
		TSharedRef<SHorizontalBox> Acts = SNew(SHorizontalBox);
		auto Act = [&Acts](TSharedRef<SWidget> B) { Acts->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)[ B ]; };
		Act(Button(K == Slot ? TEXT("Save now") : TEXT("Save here"), [this, K, Redraw]() { FString Why; SaveMsg = SaveSlot(K, Why) ? FString::Printf(TEXT("Saved in slot %d."), K) : Capital(Why) + TEXT("."); Redraw(); }, true, !bTransient));
		if (S) Act(Button(TEXT("Load"), [this, K, CloseMenu]() { FString Why; CloseMenu(); if (LoadSlot(K, Why)) { if (const ABrixGameMode* M = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this))) if (M->Life) M->Life->Toast(FString::Printf(TEXT("Loaded slot %d."), K)); } else UE_LOG(LogTemp, Warning, TEXT("Brix Town: load: %s"), *Why); }, false, !bTransient));
		if (Confirm == K)
			Act(Button(S ? TEXT("Yes: delete it and start a new character here") : TEXT("Yes: start a new character here"), [this, K]()
			{
				IFileManager::Get().Delete(*SlotPath(K));
				UseSlot(K);
				bTransient = true;   // the game being left is not saved over the slot made empty
				UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));   // a new game starts clean (QST-008)
			}, true, true, Bad(), Ink()));
		else Act(Button(S ? TEXT("New game here") : TEXT("New game"), [this, K, Redraw]() { Confirm = K; Redraw(); }, false, !bTransient));
		Page->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
		[
			Card(SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ Text(FString::Printf(TEXT("Slot %d%s"), K, K == Slot ? TEXT(" (playing)") : TEXT("")), 16, Cream(), true) ]
					+ SVerticalBox::Slot().AutoHeight()[ Text(What, 12, Dim()) ]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Acts ])
		];
	}
	Page->AddSlot().AutoHeight()[ Text(TEXT("The game saves itself in the slot you are playing as you go, and when you leave. A new game in a slot that holds a save deletes it, so it asks twice. Your settings are kept on this device, not in a save."), 13, Dim()) ];
}

void ABrixDesigner::Apply()
{
	if (ABrixCharacter* C = Me()) C->SetLook(BrixDesignLook(Look, D));
}

void ABrixDesigner::SetDesign(const FBrixDesign& V)
{
	Look = V;
	Apply();
}

void ABrixDesigner::Open()
{
	if (bOpen || !GEngine || !GEngine->GameViewport) return;
	bOpen = true;
	if (ABrixCharacter* C = Me()) C->BeginPanel();
	UGameplayStatics::SetGamePaused(this, true);   // the game has not begun: the clock waits
	SAssignNew(Root, SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SAssignNew(Body, SBox).WidthOverride(PanelWidth).Padding(FMargin(18, 18, 0, 18))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)   // the picture beside it: drag to turn the character
			[
				SNew(SBorder).BorderImage(Round(FLinearColor::Transparent, 0.f)).Padding(0)
				.OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent& E) { bDrag = true; DragAt = E.GetScreenSpacePosition(); return FReply::Handled(); })
				.OnMouseButtonUp_Lambda([this](const FGeometry&, const FPointerEvent&) { bDrag = false; return FReply::Handled(); })
				.OnMouseMove_Lambda([this](const FGeometry&, const FPointerEvent& E)
				{
					if (!bDrag || !E.IsMouseButtonDown(EKeys::LeftMouseButton)) { bDrag = false; return FReply::Unhandled(); }
					Spin += (E.GetScreenSpacePosition().X - DragAt.X) * SpinPerPixel;
					DragAt = E.GetScreenSpacePosition();
					return FReply::Handled();
				})
				.VAlign(VAlign_Bottom).HAlign(HAlign_Center)
				[
					SNew(SBox).Padding(FMargin(0, 0, 0, 22))[ Text(TEXT("Drag to turn round"), 11, Dim(), false, false) ]
				]
			]
		];
	GEngine->GameViewport->AddViewportWidgetContent(Root.ToSharedRef(), 40);
	if (const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this))) if (Mode->Life) Mode->Life->ShowHud(false);
	Build();
}

void ABrixDesigner::Close()
{
	if (!bOpen) return;
	bOpen = false;
	if (GEngine && GEngine->GameViewport && Root) GEngine->GameViewport->RemoveViewportWidgetContent(Root.ToSharedRef());
	Root.Reset();
	Body.Reset();
	UGameplayStatics::SetGamePaused(this, false);
	if (ABrixCharacter* C = Me()) { C->SetDesignView(false, 0, 0); C->EndPanel(); }
	Begun();
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (Mode && Mode->Life) Mode->Life->ShowHud(true);
	if (Mode && Mode->Life) Mode->Life->Toast(TEXT("Welcome to ") + CityName + TEXT(", ") + Look.Name + TEXT("."));
}

void ABrixDesigner::Surprise()   // life.js dz-rand
{
	auto Pick = [](int32 N) { return FMath::Min(N - 1, (int32)(FMath::FRand() * N)); };
	for (const FBrixDesignerSlider& S : D.Sliders) Look.Sliders.Add(S.Key, FMath::RoundToDouble((S.Min + (S.Max - S.Min) * FMath::FRand()) * 1000.0) / 1000.0);
	const auto PalName = [&]() { return D.Palette[Pick(D.Palette.Num())].Key; };
	Look.Skin = Pick(D.Skin.Num());
	Look.Hair = D.Hair[Pick(D.Hair.Num())];
	Look.HairColor = Pick(D.HairColor.Num());
	Look.Eyes = Pick(D.Eyes.Num());
	Look.Garment = D.Garment[Pick(D.Garment.Num())].Key;
	Look.Top = PalName();
	Look.BottomType = D.BottomType[Pick(D.BottomType.Num())].Key;
	Look.Bottom = PalName();
	Look.Shoes = PalName();
	Look.Scarf = FMath::FRand() < .4f ? PalName() : FString();
	Look.Cap = FMath::FRand() < .2f ? PalName() : FString();
	Look.Apron.Reset();
	Look.bBag = FMath::FRand() < .3f;
	Look.bTash = FMath::FRand() < .15f;
	Look.Walk = D.Walk[Pick(D.Walk.Num())].Key;
}

TSharedRef<SWidget> ABrixDesigner::Foot()
{
	const TArray<FString> Problems = BrixLookProblems(D, Look);
	TSharedRef<SVerticalBox> F = SNew(SVerticalBox);
	if (Problems.Num()) F->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[ Text(Capital(Problems[0]) + TEXT("."), 12, Bad()) ];
	F->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[ Button(TEXT("Surprise me"), [this]() { Surprise(); Apply(); Build(); }, false) ]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		+ SHorizontalBox::Slot().AutoWidth()[ Button(TEXT("Begin"), [this]() { Look.Name = Look.Name.TrimStartAndEnd(); if (!BrixLookProblems(D, Look).Num()) Close(); }, true, Problems.Num() == 0, Lantern(), Ink(), 14) ]
	];
	return F;
}

// ---- the pieces the designer and the wardrobe are both made of (life.js row(), sw(), pal(), chips())
TSharedRef<SWidget> ABrixDesigner::Row(const FString& Label, TSharedRef<SWidget> Content) const
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(SBox).WidthOverride(LabelWidth)[ Text(Label, 12, Dim(), false, false) ] ]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[ Content ];
}

TSharedRef<SWidget> ABrixDesigner::Swatch(const FLinearColor& Fill, bool bOn, TFunction<void()> Fn, const FString& Mark) const
{
	return SNew(SBox).Padding(FMargin(0, 0, 6, 6))
	[
		SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).IsFocusable(false).ContentPadding(0)
		.OnClicked_Lambda([Fn]() { Fn(); return FReply::Handled(); })
		[
			SNew(SBox).WidthOverride(26.f).HeightOverride(26.f)
			[
				SNew(SBorder).BorderImage(Round(Fill, 8.f, bOn ? Cream() : Srgb(239, 228, 204, 40), bOn ? 2.5f : 1.f)).Padding(0).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[ Text(Mark, 12, Dim(), true, false) ]
			]
		]
	];
}

TSharedRef<SWidget> ABrixDesigner::Chip(const FString& Label, bool bOn, TFunction<void()> Fn) const
{
	return SNew(SBox).Padding(FMargin(0, 0, 6, 6))[ Button(Label, Fn, bOn, true, Cream(), Ink(), 11) ];
}

TSharedRef<SWrapBox> ABrixDesigner::Wrap(float Wide) const { return SNew(SWrapBox).UseAllottedSize(false).PreferredSize(Wide); }

TSharedRef<SWidget> ABrixDesigner::Swatches(int32& Field, const TArray<FVector3f>& List, float Wide, TFunction<void()> Changed)
{
	TSharedRef<SWrapBox> W = Wrap(Wide);
	for (int32 n = 0; n < List.Num(); ++n) W->AddSlot()[ Swatch(Lin(List[n]), Field == n, [&Field, n, Changed]() { Field = n; Changed(); }) ];
	return W;
}

TSharedRef<SWidget> ABrixDesigner::Colours(FString& Field, bool bNone, float Wide, TFunction<void()> Changed)
{
	TSharedRef<SWrapBox> W = Wrap(Wide);
	if (bNone) W->AddSlot()[ Swatch(StoneDeep(), Field.IsEmpty(), [&Field, Changed]() { Field.Reset(); Changed(); }, TEXT("×")) ];
	for (const auto& P : D.Palette) { const FString Name = P.Key; W->AddSlot()[ Swatch(Lin(P.Value), Field == Name, [&Field, Name, Changed]() { Field = Name; Changed(); }) ]; }
	return W;
}

TSharedRef<SWidget> ABrixDesigner::Chips(FString& Field, const TArray<TPair<FString, FString>>& Opts, float Wide, TFunction<void()> Changed)
{
	TSharedRef<SWrapBox> W = Wrap(Wide);
	for (const auto& O : Opts) { const FString V = O.Key; W->AddSlot()[ Chip(O.Value, Field == V, [&Field, V, Changed]() { Field = V; Changed(); }) ]; }
	return W;
}

FString ABrixDesigner::ExtraLabel(const TCHAR* K) const
{
	for (const auto& E : D.Extras) if (E.Key == K) return E.Value;
	return Capital(K);
}

// the hair and the clothes, as the designer and the wardrobe both offer them (never the body or the face)
void ABrixDesigner::WearRows(TSharedRef<SVerticalBox> V, float Wide, TFunction<void()> Changed, bool bHeads)
{
	auto Head = [&](const FString& S) { if (bHeads) V->AddSlot().AutoHeight().Padding(0, 14, 0, 6)[ Caps(S, Lantern()) ]; };
	auto Add = [&](TSharedRef<SWidget> W) { V->AddSlot().AutoHeight().Padding(0, 2)[ W ]; };
	TArray<TPair<FString, FString>> Hairs;
	for (const FString& H : D.Hair) Hairs.Emplace(H, Capital(H));
	Head(TEXT("Hair"));
	Add(Row(bHeads ? TEXT("Style") : TEXT("Hair"), Chips(Look.Hair, Hairs, Wide, Changed)));
	Add(Row(bHeads ? TEXT("Colour") : TEXT("Hair colour"), Swatches(Look.HairColor, D.HairColor, Wide, Changed)));
	Head(TEXT("Clothes"));
	Add(Row(TEXT("Top"), Chips(Look.Garment, D.Garment, Wide, Changed)));
	Add(Row(TEXT(""), Colours(Look.Top, false, Wide, Changed)));
	Add(Row(TEXT("Bottom"), Chips(Look.BottomType, D.BottomType, Wide, Changed)));
	Add(Row(TEXT(""), Colours(Look.Bottom, false, Wide, Changed)));
	Add(Row(TEXT("Shoes"), Colours(Look.Shoes, false, Wide, Changed)));
	Head(TEXT("Extras"));
	Add(Row(ExtraLabel(TEXT("scarf")), Colours(Look.Scarf, true, Wide, Changed)));
	Add(Row(ExtraLabel(TEXT("cap")), Colours(Look.Cap, true, Wide, Changed)));
	Add(Row(ExtraLabel(TEXT("apron")), Colours(Look.Apron, true, Wide, Changed)));
	TSharedRef<SWrapBox> W = Wrap(Wide);
	W->AddSlot()[ Chip(ExtraLabel(TEXT("bag")), Look.bBag, [this, Changed]() { Look.bBag = !Look.bBag; Changed(); }) ];
	W->AddSlot()[ Chip(ExtraLabel(TEXT("tash")), Look.bTash, [this, Changed]() { Look.bTash = !Look.bTash; Changed(); }) ];
	Add(Row(TEXT(""), W));
}

void ABrixDesigner::Build()   // life.js buildDesigner(): drawn again after each choice
{
	if (!Body) return;
	if (bOffer)   // life.js go(): a save in the slot played last: carry on, or a new character
	{
		const TOptional<FBrixSaveGame> S = ReadSlot(Slot);
		if (S)
		{
			Body->SetContent(SNew(SBox).VAlign(VAlign_Center)[ Card(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ Caps(FString::Printf(TEXT("Slot %d"), Slot)) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 4)[ Text(TEXT("Welcome back"), 22, Cream(), true) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)[ Text(FString::Printf(TEXT("%s, day %d in %s."), *S->Look.Name, S->Day + 1, *CityName), 14, Dim()) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[ Button(TEXT("Carry on as ") + S->Look.Name, [this]() { CarryOn(); }, true, true, Lantern(), Ink(), 14) ]
				+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("New character"), [this]() { bOffer = false; Look = D.Default; Apply(); Build(); }, false) ],
				FMargin(20, 16)) ]);
			return;
		}
		bOffer = false;
	}
	const float Scrolled = Scroll ? Scroll->GetScrollOffset() : 0.f;
	TFunction<void()> Again = [this]() { Apply(); Build(); };
	const float Wide = PanelWidth - 18.f - 40.f - 10.f - LabelWidth - 8.f;   // what a row leaves its choices: the card's padding, the scroll bar, the label
	auto Slider = [&](const FBrixDesignerSlider& S) -> TSharedRef<SWidget>
	{
		const FString Key = S.Key, Unit = S.Unit;
		TSharedRef<SHorizontalBox> H = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SSlider).MinValue((float)S.Min).MaxValue((float)S.Max).StepSize((float)S.Step).Value((float)Look.Slider(Key)).IsFocusable(false)
				.SliderBarColor(StoneLift()).SliderHandleColor(Cream())
				.OnValueChanged_Lambda([this, Key](float V) { Look.Sliders.Add(Key, FMath::RoundToDouble(V * 1000.0) / 1000.0); Apply(); })
			];
		if (!Unit.IsEmpty())
			H->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
			[
				SNew(SBox).WidthOverride(52.f)[ SNew(STextBlock).Font(Font(11)).ColorAndOpacity(Cream()).Text_Lambda([this, Key, Unit]() { return FText::FromString(FString::Printf(TEXT("%.2f %s"), Look.Slider(Key), *Unit)); }) ]
			];
		return Row(S.Label, H);
	};

	TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
	auto Head = [&](const FString& S) { V->AddSlot().AutoHeight().Padding(0, 14, 0, 6)[ Caps(S, Lantern()) ]; };
	auto Add = [&](TSharedRef<SWidget> W) { V->AddSlot().AutoHeight().Padding(0, 2)[ W ]; };
	Add(Row(TEXT("Name"), SNew(SBorder).BorderImage(Round(StoneDeep(), 8.f, Srgb(239, 228, 204, 60), 1.f)).Padding(FMargin(10, 5))
		[
			SNew(SEditableText).Text(FText::FromString(Look.Name)).Font(Font(14, true)).ColorAndOpacity(Cream()).HintText(FText::FromString(TEXT("Your name")))
			.OnTextChanged_Lambda([this](const FText& T) { Look.Name = T.ToString().Left(D.NameMax); if (FootBox) FootBox->SetContent(Foot()); })
		]));
	Head(TEXT("Body"));
	for (int32 k = 0; k < D.Sliders.Num() && k < 2; ++k) Add(Slider(D.Sliders[k]));
	Add(Row(TEXT("Skin"), Swatches(Look.Skin, D.Skin, Wide, Again)));
	Head(TEXT("Face"));
	for (int32 k = 2; k < D.Sliders.Num(); ++k) Add(Slider(D.Sliders[k]));
	Add(Row(TEXT("Eyes"), Swatches(Look.Eyes, D.Eyes, Wide, Again)));
	WearRows(V, Wide, Again, true);
	Head(TEXT("Walk"));
	Add(Row(TEXT(""), Chips(Look.Walk, D.Walk, Wide, Again)));

	Body->SetContent(Card(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Caps(TEXT("New character")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 6)[ Text(TEXT("Who are you?"), 22, Cream(), true) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[ Rule() ]
		+ SVerticalBox::Slot().FillHeight(1.f)[ SAssignNew(Scroll, SScrollBox) + SScrollBox::Slot().Padding(FMargin(0, 0, 10, 0))[ V ] ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)[ Rule() ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)[ SAssignNew(FootBox, SBox)[ Foot() ] ],
		FMargin(20, 16)));
	Scroll->SetScrollOffset(Scrolled);
}

// ---- the wardrobe (CHR-014 to CHR-016; life.js wardrobeTab()): the game menu's Wardrobe page
FString ABrixDesigner::WardrobeHeadline() const
{
	const int32 N = Wardrobe.Outfits.Num();
	return FString::Printf(TEXT("%d outfit%s · changing is free"), N, N == 1 ? TEXT("") : TEXT("s"));
}

void ABrixDesigner::WardrobePage(TSharedRef<SVerticalBox> Page, const TMap<FString, int32>& Owned, TFunction<void()> Redraw)
{
	const float Wide = WardrobeWidth - 2 * 18.f - LabelWidth - 20.f;
	TFunction<void()> Changed = [this, Redraw]() { WardMsg.Reset(); Apply(); Redraw(); };
	auto Say = [this, Redraw](const FString& S) { WardMsg = S; Apply(); Redraw(); };   // what was done, and the character drawn so
	auto Section = [&Page](const FString& Title, TSharedRef<SWidget> Content)
	{
		Page->AddSlot().AutoHeight().Padding(0, 0, 0, 14)
		[ Card(SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[ Caps(Title) ] + SVerticalBox::Slot().AutoHeight()[ Content ]) ];
	};
	if (!WardMsg.IsEmpty()) Page->AddSlot().AutoHeight().Padding(4, 0, 0, 10)[ Text(WardMsg, 13, Good()) ];

	// outfits: wear one, forget one, save what you wear under a name (CHR-015)
	TSharedRef<SVerticalBox> O = SNew(SVerticalBox);
	for (const FString& Name : Wardrobe.Names())
		O->AddSlot().AutoHeight().Padding(0, 3)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[ Text(Name, 14, Cream(), true, false) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0)[ Button(TEXT("Wear"), [this, Name, Say]() { if (const FJsonObject* X = Wardrobe.Find(Name)) { BrixWearOutfit(D, *X, Look); Say(TEXT("You change into ") + Name + TEXT(".")); } }, true, true, Cream(), Ink(), 11) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Button(TEXT("Forget"), [this, Name, Say]() { Wardrobe.Forget(Name); Say(TEXT("Forgotten.")); }, false, true, Cream(), Ink(), 11) ]
		];
	if (!Wardrobe.Outfits.Num()) O->AddSlot().AutoHeight().Padding(0, 0, 0, 6)[ Text(TEXT("None saved yet."), 13, Dim()) ];
	O->AddSlot().AutoHeight().Padding(0, 8, 0, 0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 8, 0)
		[
			SNew(SBorder).BorderImage(Round(StoneDeep(), 8.f, Srgb(239, 228, 204, 60), 1.f)).Padding(FMargin(10, 5))
			[
				SNew(SEditableText).Text(FText::FromString(OutfitName)).Font(Font(13)).ColorAndOpacity(Cream()).HintText(FText::FromString(TEXT("Name this outfit")))
				.OnTextChanged_Lambda([this](const FText& T) { OutfitName = T.ToString().Left(D.NameMax); })
			]
		]
		+ SHorizontalBox::Slot().AutoWidth()[ Button(TEXT("Save what you wear"), [this, Say]()
		{
			FString Why;
			const FString N = OutfitName.TrimStartAndEnd();
			if (Wardrobe.Save(D, Look, N, Why)) { OutfitName.Reset(); Say(TEXT("Saved as ") + N + TEXT(".")); }
			else Say(Capital(Why) + TEXT("."));
		}, true, true, Lantern(), Ink(), 12) ]
	];
	Section(FString::Printf(TEXT("Outfits · %d of %d"), Wardrobe.Outfits.Num(), D.MaxOutfits), O);

	// clothes bought at the mall's outlets: put one on (CHR-016)
	TSharedRef<SVerticalBox> C = SNew(SVerticalBox);
	TArray<FString> Ids;
	Owned.GetKeys(Ids);
	Ids.Sort([this](const FString& A, const FString& B) { const FBrixGarment* GA = Garments.Find(A); const FBrixGarment* GB = Garments.Find(B); return (GA ? GA->Name : A) < (GB ? GB->Name : B); });
	for (const FString& Id : Ids)
	{
		const FBrixGarment* G = Garments.Find(Id);
		if (!G || Owned[Id] <= 0) continue;
		const TArray<FString> Words = BrixGarmentWords(D, *G);   // life.js partWords()
		const bool bOn = BrixIsWearing(Look, *G);
		const FString Name = G->Name;
		const FBrixGarment Copy = *G;
		C->AddSlot().AutoHeight().Padding(0, 3)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ Text(Name, 14, Cream(), true, false) ]
				+ SVerticalBox::Slot().AutoHeight()[ Text((Owned[Id] > 1 ? FString::Printf(TEXT("× %d · "), Owned[Id]) : FString()) + FString::Join(Words, TEXT(", ")), 12, Dim()) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				bOn ? (TSharedRef<SWidget>)Text(TEXT("on"), 13, Good(), true, false)
				: Button(TEXT("Put on"), [this, Copy, Name, Say]() { BrixPutOn(Copy, Look); Say(TEXT("You put on the ") + Name.ToLower() + TEXT(".")); }, true, true, Cream(), Ink(), 11)
			]
		];
	}
	if (!C->NumSlots()) C->AddSlot().AutoHeight()[ Text(TEXT("None yet. Clothes and shoes bought at the mall's outlets are kept here."), 13, Dim()) ];
	Section(TEXT("Clothes you own"), C);

	// what you wear now, changed for free (CHR-014)
	TSharedRef<SVerticalBox> W = SNew(SVerticalBox);
	WearRows(W, Wide, Changed, false);
	W->AddSlot().AutoHeight().Padding(0, 8, 0, 0)[ Text(TEXT("Changing is free, anywhere (for now: later, clothes are bought in shops and kept in a wardrobe at home)."), 12, Dim()) ];
	Section(TEXT("What you wear"), W);
}

void ABrixDesigner::WardrobeView(bool bOn, TSharedPtr<SWidget> Page)
{
	WardrobeBox = bOn ? Page : nullptr;
	if (bWardrobe == bOn) return;
	bWardrobe = bOn;
	if (ABrixCharacter* C = Me()) C->SetDesignView(bOn, WardrobeSpin, 0.f, false, WardrobeCameraM);   // the menu's keys still work
}

float ABrixDesigner::ShareLeftOf(const TSharedPtr<SWidget>& W) const   // how much of the picture's width, from the left, a widget's right edge takes
{
	if (!W || !GEngine || !GEngine->GameViewport) return 0.f;
	const TSharedPtr<SViewport> Vp = GEngine->GameViewport->GetGameViewportWidget();
	if (!Vp) return 0.f;
	const FGeometry& G = W->GetTickSpaceGeometry();
	const FGeometry& V = Vp->GetTickSpaceGeometry();
	const float Width = (float)V.GetAbsoluteSize().X;
	return Width > 760.f ? FMath::Clamp((float)(G.GetAbsolutePosition().X + G.GetAbsoluteSize().X - V.GetAbsolutePosition().X) / Width, 0.f, .7f) : 0.f;
}
