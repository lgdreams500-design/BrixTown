#include "BrixDesignerActor.h"

#include "BrixCharacter.h"
#include "BrixGameMode.h"
#include "BrixLifeActor.h"
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
		if (FParse::Param(FCommandLine::Get(), TEXT("BrixDesigner")) || (!bShot && !FParse::Param(FCommandLine::Get(), TEXT("BrixNoDesigner"))))
		{
			Open();
			if (FParse::Param(FCommandLine::Get(), TEXT("BrixDesignBegin"))) BeginAt = FPlatformTime::Seconds() + 3.0;   // Begin pressed; the game's own shot after
			else if (bShot) ShotAt = FPlatformTime::Seconds() + ShotSeconds;
		}
		else OnBegin.Broadcast(Look);
	}
	if (bOpen && C)
	{
		FVector2D Vp(1600, 900);
		if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Vp);
		const float PanelPx = Body ? (float)Body->GetTickSpaceGeometry().GetAbsoluteSize().X : PanelWidth;
		C->SetDesignView(true, Spin, Vp.X > 760 ? FMath::Clamp(PanelPx / (float)Vp.X, 0.f, .6f) : 0.f);
	}
	if (BeginAt > 0 && FPlatformTime::Seconds() >= BeginAt && bOpen)
	{
		BeginAt = -1;
		Look.Name = Look.Name.TrimStartAndEnd();
		UE_LOG(LogTemp, Display, TEXT("Brix Town: the character designer: Begin as %s"), *Look.Name);
		if (!BrixLookProblems(D, Look).Num()) Close();
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
	OnBegin.Broadcast(Look);
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

void ABrixDesigner::Build()   // life.js buildDesigner(): drawn again after each choice
{
	if (!Body) return;
	const float Scrolled = Scroll ? Scroll->GetScrollOffset() : 0.f;
	auto Again = [this]() { Apply(); Build(); };
	auto Row = [](const FString& Label, TSharedRef<SWidget> Content) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(SBox).WidthOverride(106.f)[ Text(Label, 12, Dim(), false, false) ] ]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[ Content ];
	};
	auto Swatch = [](const FLinearColor& Fill, bool bOn, TFunction<void()> Fn, const FString& Mark = FString()) -> TSharedRef<SWidget>
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
	};
	const float Wide = PanelWidth - 18.f - 40.f - 10.f - 106.f - 8.f;   // what a row leaves its choices: the card's padding, the scroll bar, the label
	auto Wrap = [Wide]() { return SNew(SWrapBox).UseAllottedSize(false).PreferredSize(Wide); };
	auto Chip = [](const FString& Label, bool bOn, TFunction<void()> Fn) -> TSharedRef<SWidget>
	{
		return SNew(SBox).Padding(FMargin(0, 0, 6, 6))[ Button(Label, Fn, bOn, true, Cream(), Ink(), 11) ];
	};
	auto Swatches = [&](int32& Field, const TArray<FVector3f>& List)
	{
		TSharedRef<SWrapBox> W = Wrap();
		for (int32 n = 0; n < List.Num(); ++n) W->AddSlot()[ Swatch(Lin(List[n]), Field == n, [this, &Field, n, Again]() { Field = n; Again(); }) ];
		return W;
	};
	auto Palette = [&](FString& Field, bool bNone)
	{
		TSharedRef<SWrapBox> W = Wrap();
		if (bNone) W->AddSlot()[ Swatch(StoneDeep(), Field.IsEmpty(), [&Field, Again]() { Field.Reset(); Again(); }, TEXT("×")) ];
		for (const auto& P : D.Palette) { const FString Name = P.Key; W->AddSlot()[ Swatch(Lin(P.Value), Field == Name, [&Field, Name, Again]() { Field = Name; Again(); }) ]; }
		return W;
	};
	auto Chips = [&](FString& Field, const TArray<TPair<FString, FString>>& Opts)
	{
		TSharedRef<SWrapBox> W = Wrap();
		for (const auto& O : Opts) { const FString V = O.Key; W->AddSlot()[ Chip(O.Value, Field == V, [&Field, V, Again]() { Field = V; Again(); }) ]; }
		return W;
	};
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
	TArray<TPair<FString, FString>> Hairs;
	for (const FString& H : D.Hair) Hairs.Emplace(H, Capital(H));

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
	Add(Row(TEXT("Skin"), Swatches(Look.Skin, D.Skin)));
	Head(TEXT("Face"));
	for (int32 k = 2; k < D.Sliders.Num(); ++k) Add(Slider(D.Sliders[k]));
	Add(Row(TEXT("Eyes"), Swatches(Look.Eyes, D.Eyes)));
	Head(TEXT("Hair"));
	Add(Row(TEXT("Style"), Chips(Look.Hair, Hairs)));
	Add(Row(TEXT("Colour"), Swatches(Look.HairColor, D.HairColor)));
	Head(TEXT("Clothes"));
	Add(Row(TEXT("Top"), Chips(Look.Garment, D.Garment)));
	Add(Row(TEXT(""), Palette(Look.Top, false)));
	Add(Row(TEXT("Bottom"), Chips(Look.BottomType, D.BottomType)));
	Add(Row(TEXT(""), Palette(Look.Bottom, false)));
	Add(Row(TEXT("Shoes"), Palette(Look.Shoes, false)));
	Head(TEXT("Extras"));
	auto Label = [&](const TCHAR* K) { for (const auto& E : D.Extras) if (E.Key == K) return E.Value; return Capital(K); };
	Add(Row(Label(TEXT("scarf")), Palette(Look.Scarf, true)));
	Add(Row(Label(TEXT("cap")), Palette(Look.Cap, true)));
	Add(Row(Label(TEXT("apron")), Palette(Look.Apron, true)));
	{
		TSharedRef<SWrapBox> W = Wrap();
		W->AddSlot()[ Chip(Label(TEXT("bag")), Look.bBag, [this, Again]() { Look.bBag = !Look.bBag; Again(); }) ];
		W->AddSlot()[ Chip(Label(TEXT("tash")), Look.bTash, [this, Again]() { Look.bTash = !Look.bTash; Again(); }) ];
		Add(Row(TEXT(""), W));
	}
	Head(TEXT("Walk"));
	Add(Row(TEXT(""), Chips(Look.Walk, D.Walk)));

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
