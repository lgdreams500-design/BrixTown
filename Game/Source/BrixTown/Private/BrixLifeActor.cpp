#include "BrixLifeActor.h"

#include "BrixCharacter.h"
#include "BrixCity.h"
#include "BrixGameMode.h"
#include "BrixTalkActor.h"
#include "BrixThingWidget.h"
#include "BrixUnits.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

using BrixUnits::CmPerM;
using BrixMoney::Format;

namespace
{
	// the slice's colours (index.html: --ink, --cream, --dim, --lantern; #life, .pbtn, .pos, .neg, .pmsg, #counter kbd)
	const FLinearColor UiInk = FLinearColor::FromSRGBColor(FColor(23, 23, 28));
	const FLinearColor UiPanel92 = FLinearColor::FromSRGBColor(FColor(23, 23, 28, 235));
	const FLinearColor UiPanel62 = FLinearColor::FromSRGBColor(FColor(23, 23, 28, 158));
	const FLinearColor UiPanel78 = FLinearColor::FromSRGBColor(FColor(23, 23, 28, 199));
	const FLinearColor UiCream = FLinearColor::FromSRGBColor(FColor(217, 207, 182));
	const FLinearColor UiDim = FLinearColor::FromSRGBColor(FColor(167, 159, 140));
	const FLinearColor UiLantern = FLinearColor::FromSRGBColor(FColor(240, 163, 90));
	const FLinearColor UiGold = FLinearColor::FromSRGBColor(FColor(240, 195, 90));
	const FLinearColor UiGoldLine = FLinearColor::FromSRGBColor(FColor(240, 195, 90, 153));
	const FLinearColor UiMsgBack = FLinearColor::FromSRGBColor(FColor(240, 195, 90, 31));
	const FLinearColor UiOff = FLinearColor::FromSRGBColor(FColor(217, 207, 182, 36));
	const FLinearColor UiDisabled = FLinearColor::FromSRGBColor(FColor(107, 101, 88));
	const FLinearColor UiRule = FLinearColor::FromSRGBColor(FColor(217, 207, 182, 31));
	const FLinearColor UiPos = FLinearColor::FromSRGBColor(FColor(159, 209, 160));
	const FLinearColor UiNeg = FLinearColor::FromSRGBColor(FColor(233, 144, 122));
	const FSlateBrush* UiWhite() { return FCoreStyle::Get().GetBrush("WhiteBrush"); }
	FSlateFontInfo UiFont(int32 Size, bool bBold = false) { return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size); }
	FString UiHhmm(double M) { const int32 m = (int32)M; return FString::Printf(TEXT("%02d:%02d"), (m / 60) % 24, m % 60); }
	TSharedRef<STextBlock> UiText(const FString& S, int32 Size = 15, FLinearColor C = UiCream, bool bBold = false)
	{
		return SNew(STextBlock).Text(FText::FromString(S)).Font(UiFont(Size, bBold)).ColorAndOpacity(C).AutoWrapText(true);
	}
	TSharedRef<SWidget> UiHeading(const FString& S)   // #life h3: small capitals, dim, spaced
	{
		return SNew(SBox).Padding(FMargin(0, 16, 0, 6))[ SNew(STextBlock).Text(FText::FromString(S.ToUpper())).Font(UiFont(10, true)).ColorAndOpacity(UiDim) ];
	}
	// .pbtn: the lantern's orange with dark letters; .off: quiet; disabled: grey
	TSharedRef<SWidget> UiButton(const FString& Label, TFunction<void()> OnClick, bool bEnabled = true, bool bQuiet = false)
	{
		return SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).IsEnabled(bEnabled).IsFocusable(false)
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(!bEnabled ? UiDisabled : bQuiet ? UiOff : UiLantern).Padding(FMargin(12, 5))
				[ SNew(STextBlock).Text(FText::FromString(Label)).Font(UiFont(11, true)).ColorAndOpacity(bQuiet ? UiCream : UiInk) ]
			];
	}
	TSharedRef<SHorizontalBox> UiRow() { return SNew(SHorizontalBox); }
	void UiAdd(TSharedRef<SHorizontalBox> R, TSharedRef<SWidget> W) { R->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[ W ]; }
}

ABrixLife::ABrixLife()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ABrixLife::Start(const FBrixCity& City)
{
	FString Error;
	if (!LoadBrixLife(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), LifeFile)), Data, Error) || !BrixEconLots(City, Data, Lots, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Brix Town: no money or shops (%s)"), *Error);
		return;
	}
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) { LotAngle.Add(L.AngleDeg); LotDoor.Add(L.Door); }
	E = MakeUnique<FBrixEconomy>(Data, Lots);
	if (!LoadBrixBuyMenu(Data.Json, BuyMenu, Error)) UE_LOG(LogTemp, Warning, TEXT("Brix Town: buy menus without pictures (%s)"), *Error);
	double Day = 0, Minute = 0;
	Frame(Day, Minute);
	E->Advance((int32)Day, Minute);
	E->NewCharacter();   // a new game (life.js newGame(): Q.newGame() pays the starting cash in)
	LastMoney = E->TotalMoney();
	UE_LOG(LogTemp, Display, TEXT("Brix Town: a new game with %s in cash; the bank is %s, the market %s"), *Format(E->L.Balance(TEXT("p:cash"))),
		E->BankLot >= 0 ? *Lots[E->BankLot].Name : TEXT("missing"), E->MarketLot >= 0 ? *Lots[E->MarketLot].Name : TEXT("missing"));
	BuildUI();
}

void ABrixLife::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GEngine && GEngine->GameViewport)
	{
		if (Hud) GEngine->GameViewport->RemoveViewportWidgetContent(Hud.ToSharedRef());
		if (PanelRoot) GEngine->GameViewport->RemoveViewportWidgetContent(PanelRoot.ToSharedRef());
	}
	Super::EndPlay(Reason);
}

// the economy's day and minute: days since the new game, by the game's clock
void ABrixLife::Frame(double& OutDay, double& OutMinute) const
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const double M = Mode ? Mode->ClockMinutes() : 0.0;
	OutDay = FMath::Floor(M / 1440.0);
	OutMinute = M - OutDay * 1440.0;
}

FVector2D ABrixLife::PlayerAt() const
{
	const APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0);
	return Me ? FVector2D(Me->GetActorLocation().X, Me->GetActorLocation().Y) / CmPerM : FVector2D(1e9, 1e9);
}

// The counter the player is at: inside the lot (play.js lotAt(), turned by its angle), or out on the street at its door.
int32 ABrixLife::CounterHere() const
{
	if (!E) return -1;
	const FVector2D P = PlayerAt();
	int32 Best = -1;
	double BestD = CounterReach;
	for (int32 n = 0; n < Lots.Num(); ++n)
	{
		if (n != E->BankLot && n != E->MarketLot && E->ShopKind(n).IsEmpty()) continue;   // the bank, the market and the shops have a counter
		const FBrixEconLot& L = Lots[n];
		const FVector2D C = L.Centre(), D = P - C;
		const double A = FMath::DegreesToRadians(-LotAngle[n]);
		const FVector2D U(D.X * FMath::Cos(A) - D.Y * FMath::Sin(A), D.X * FMath::Sin(A) + D.Y * FMath::Cos(A));
		const FVector2D Q = U + C;
		if (Q.X >= L.Min.X && Q.X <= L.Max.X && Q.Y >= L.Min.Y && Q.Y <= L.Max.Y) return n;
		const double Dd = FVector2D::Distance(P, LotDoor[n]);
		if (Dd < BestD) { BestD = Dd; Best = n; }
	}
	return Best;
}

void ABrixLife::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!E) return;
	double Day = 0, Minute = 0;
	Frame(Day, Minute);
	E->Advance((int32)Day, Minute);   // the bank settles each midnight passed (BANK-008)
	const int64 Tm = E->TotalMoney();
	if (Tm != LastMoney)
	{
		const int64 D = Tm - LastMoney;
		DeltaText = (D > 0 ? TEXT("+") : TEXT("")) + Format(D);
		bDeltaUp = D > 0;
		DeltaLeft = PlusSeconds;
		if (IsOpen()) Draw();
	}
	LastMoney = Tm;
	ToastLeft = FMath::Max(0.f, ToastLeft - Dt);
	DeltaLeft = FMath::Max(0.f, DeltaLeft - Dt);
}

void ABrixLife::Toast(const FString& T)
{
	ToastText = T;
	ToastLeft = ToastSeconds;
}

// ---- the purse, the toast and the counter's offer, always on the screen (index.html #purse, #toast, #counter)

void ABrixLife::BuildUI()
{
	if (!GEngine || !GEngine->GameViewport) return;
	SAssignNew(Hud, SOverlay)
	+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(22, 150, 0, 0))
	[
		SNew(SHorizontalBox).Visibility(EVisibility::HitTestInvisible)
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(STextBlock).Font(UiFont(16, true)).ColorAndOpacity(UiCream).ShadowOffset(FVector2D(0, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, .75f))
			.Text_Lambda([this]() { return FText::FromString(E ? Format(E->L.Balance(TEXT("p:cash"))) : FString()); }) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(STextBlock).Font(UiFont(16)).ColorAndOpacity(UiDim).ShadowOffset(FVector2D(0, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, .75f))
			.Text_Lambda([this]() { return FText::FromString(E && E->Accounts.Num() ? TEXT(" · bank ") + Format(E->L.Balance(TEXT("p:current")) + E->L.Balance(TEXT("p:savings"))) : FString()); }) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(10, 0, 0, 0)
		[ SNew(STextBlock).Font(UiFont(16, true)).ShadowOffset(FVector2D(0, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, .75f))
			.ColorAndOpacity_Lambda([this]() { FLinearColor C = bDeltaUp ? UiPos : UiNeg; C.A = FMath::Clamp(DeltaLeft / .6f, 0.f, 1.f); return FSlateColor(C); })
			.Text_Lambda([this]() { return FText::FromString(DeltaText); }) ]
	]
	+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(16, 200, 16, 0))
	[
		SNew(SBox).MaxDesiredWidth(560.f).Visibility_Lambda([this]() { return ToastLeft > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(UiPanel78).Padding(FMargin(16, 8))
			.ColorAndOpacity_Lambda([this]() { return FLinearColor(1, 1, 1, FMath::Clamp(ToastLeft / .5f, 0.f, 1.f)); })
			[ SNew(STextBlock).Font(UiFont(17)).ColorAndOpacity(UiCream).AutoWrapText(true).Justification(ETextJustify::Center).Text_Lambda([this]() { return FText::FromString(ToastText); }) ]
		]
	]
	+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(16, 0, 16, 140))
	[
		SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(UiPanel62).Padding(FMargin(14, 6))
		.Visibility_Lambda([this]()
		{
			const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
			const bool bTalk = Mode && Mode->Talk && Mode->Talk->IsTalking();
			return !IsOpen() && !bTalk && !CounterWords(CounterHere()).IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[ SNew(STextBlock).Font(UiFont(15)).ColorAndOpacity(UiGold).Text(FText::FromString(TEXT("B"))) ]
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(STextBlock).Font(UiFont(15)).ColorAndOpacity(UiCream).Text_Lambda([this]() { return FText::FromString(CounterWords(CounterHere())); }) ]
		]
	];
	SAssignNew(PanelRoot, SBox)
		.HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(16, 40))
		.Visibility_Lambda([this]() { return IsOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
		[
			SNew(SBox).WidthOverride_Lambda([this]() { return Panel == TEXT("shop") ? (float)BuyMenu.WidePanelPx : 620.f; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(UiGoldLine).Padding(0)[ SNew(SBox).HeightOverride(2.f) ] ]
				+ SVerticalBox::Slot().FillHeight(1)
				[
					SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(UiPanel92).Padding(FMargin(20, 16, 20, 12))
					[ SAssignNew(PanelBody, SBox) ]
				]
			]
		];
	GEngine->GameViewport->AddViewportWidgetContent(Hud.ToSharedRef(), 15);
	GEngine->GameViewport->AddViewportWidgetContent(PanelRoot.ToSharedRef(), 25);
}

// ---- panels (life.js open(), close(), act(), draw())

// what the offer over a counter says (life.js hud()): nothing for a mall, whose outlets each have their own
FString ABrixLife::CounterWords(int32 n) const
{
	if (!E || n < 0) return FString();
	if (n == E->BankLot) return TEXT("Bank counter");
	if (n == E->MarketLot) return TEXT("Your market stall");
	if (!E->ShopKind(n).IsEmpty() && !E->Outlets.Contains(n)) return TEXT("Buy and sell at ") + Lots[n].Name;
	return FString();
}

void ABrixLife::CounterKey()
{
	if (Panel == TEXT("bank") || Panel == TEXT("shop") || Panel == TEXT("stall")) { Close(); return; }
	const int32 n = CounterHere();
	if (n >= 0 && E->Outlets.Contains(n)) { Toast(TEXT("Walk into one of ") + Lots[n].Name + TEXT("'s outlets to buy there: each has its own goods.")); return; }
	if (n < 0) { Toast(TEXT("No counter here. Shops, cafés, the bank and the market square have one.")); return; }
	if (n == E->BankLot) Open(TEXT("bank"), n);
	else if (n == E->MarketLot) Open(TEXT("stall"), n);
	else { Side = TEXT("buy"); Open(TEXT("shop"), n); }
}

void ABrixLife::WalletKey()
{
	if (Panel == TEXT("wallet")) { Close(); return; }
	Open(TEXT("wallet"), -1);
}

void ABrixLife::Open(const FString& Kind, int32 Lot)
{
	if (!E) return;
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (Mode && Mode->Talk && Mode->Talk->IsTalking()) return;
	Panel = Kind;
	PanelLot = Lot;
	Msg.Reset();
	if (ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0))) Me->BeginPanel();
	Draw();
}

void ABrixLife::Close()
{
	if (!IsOpen()) return;
	Panel.Reset();
	LiveGrid.Reset();
	if (PanelBody) PanelBody->SetContent(SNullWidget::NullWidget);
	if (ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0))) Me->EndPanel();
}

void ABrixLife::Act(TFunction<bool(FString&)> Fn)
{
	FString Said;
	Msg = Fn(Said) ? Said : TEXT("Refused: ") + E->Why + TEXT(".");
	Draw();
}

TSharedRef<SWidget> ABrixLife::PurseLine() const
{
	FString S = TEXT("Cash ") + Format(E->L.Balance(TEXT("p:cash")));
	if (E->HasAccount(TEXT("current"))) S += TEXT("  ·  Current ") + Format(E->L.Balance(TEXT("p:current")));
	if (E->HasAccount(TEXT("savings"))) S += TEXT("  ·  Savings ") + Format(E->L.Balance(TEXT("p:savings")));
	return UiText(S, 15);
}

// ECO-009: every ledger line that touched the player's accounts, newest first; between their own accounts, how much moved
TSharedRef<SWidget> ABrixLife::StatementTable(int32 N) const
{
	const TArray<FBrixLedger::FEntry> St = E->L.Statement(BrixMoney::Player(), N);
	if (!St.Num()) return UiText(TEXT("Nothing yet."), 15, UiDim);
	TSharedRef<SVerticalBox> T = SNew(SVerticalBox);
	for (const FBrixLedger::FEntry& En : St)
	{
		const FString Ch = En.Change ? (En.Change > 0 ? TEXT("+") : TEXT("")) + Format(En.Change) : Format(E->L.MovedBetween(En.No, BrixMoney::Player()));
		T->AddSlot().AutoHeight()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(6, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 14, 0)[ SNew(SBox).WidthOverride(112.f)[ SNew(STextBlock).Font(UiFont(14)).ColorAndOpacity(UiCream).Text(FText::FromString(FString::Printf(TEXT("Day %d %s"), En.Day + 1, *UiHhmm(En.Minute)))) ] ]
				+ SHorizontalBox::Slot().FillWidth(1)[ UiText(En.Memo, 14) ]
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right)[ UiText(Ch, 14, !En.Change ? UiDim : En.Change < 0 ? UiNeg : UiPos) ]
			]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(UiRule).Padding(0)[ SNew(SBox).HeightOverride(1.f) ] ]
		];
	}
	return T;
}

void ABrixLife::Draw()
{
	if (!IsOpen() || !PanelBody || !E) return;
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	FString Title, Sub;
	LiveGrid.Reset();
	FString Hint = TEXT("Esc  close");
	if (Panel == TEXT("bank")) { Title = Lots[PanelLot].Name; Sub = TEXT("Accounts, savings and loans"); }
	else if (Panel == TEXT("shop"))
	{
		Title = Lots[PanelLot].Name;
		Sub = E->R.Shop(E->ShopKind(PanelLot))->Name + TEXT(" · restocks at ") + UiHhmm(E->R.RestockMinute);
		Hint = FString(TEXT("← → ↑ ↓  choose    Enter  ")) + (Side == TEXT("sell") ? TEXT("sell") : TEXT("buy")) + TEXT("    Esc  close");
	}
	else if (Panel == TEXT("stall")) { Title = Lots[PanelLot].Name; Sub = TEXT("Your market stall · residents shop here ") + UiHhmm(E->R.StallOpen) + TEXT(" to ") + UiHhmm(E->R.StallClose); }
	else { Title = TEXT("Money"); Sub = PlayerName + TEXT(" · I"); }
	if (!Msg.IsEmpty())
		Body->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[ SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(UiMsgBack).Padding(FMargin(10, 6))[ UiText(Msg, 15) ] ];
	if (Panel == TEXT("bank")) BankPanel(Body);
	else if (Panel == TEXT("shop")) ShopPanel(Body);
	else if (Panel == TEXT("stall")) StallPanel(Body);
	else WalletPanel(Body);
	PanelBody->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0, 0, 12, 0)[ SNew(STextBlock).Font(UiFont(24)).ColorAndOpacity(UiCream).Text(FText::FromString(Title)) ]
			+ SWrapBox::Slot().VAlign(VAlign_Bottom).Padding(0, 0, 0, 3)[ SNew(STextBlock).Font(UiFont(14)).ColorAndOpacity(UiDim).Text(FText::FromString(Sub)) ]
		]
		+ SVerticalBox::Slot().FillHeight(1)[ SNew(SScrollBox) + SScrollBox::Slot()[ Body ] ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 8, 0, 0)[ SNew(STextBlock).Font(UiFont(13)).ColorAndOpacity(UiDim).Text(FText::FromString(Hint)) ]);
}

// BANK-001, BANK-002, BANK-007 (life.js draw(), kind "bank")
void ABrixLife::BankPanel(TSharedRef<SVerticalBox> Body)
{
	const FBrixEconomyRules& R = E->R;
	auto Slot = [&](TSharedRef<SWidget> W, float Top = 6) { Body->AddSlot().AutoHeight().Padding(0, Top, 0, 0)[ W ]; };
	Slot(PurseLine(), 0);
	if (!E->HasAccount(TEXT("current")))
	{
		Slot(UiText(TEXT("Open a current account. New customers get a welcome deposit of ") + Format(R.WelcomeDeposit) + TEXT(".")));
		TSharedRef<SHorizontalBox> B = UiRow();
		UiAdd(B, UiButton(TEXT("Open a current account"), [this]() { Act([this](FString& S) { S = TEXT("Your current account is open."); return E->OpenAccount(TEXT("current")); }); }));
		Slot(B);
	}
	else
	{
		if (!E->HasAccount(TEXT("savings")))
		{
			Slot(UiText(FString::Printf(TEXT("A savings account earns %s%% a day, paid at midnight."), *FString::SanitizeFloat(R.Account(TEXT("savings"))->RateBp / 100.0))));
			TSharedRef<SHorizontalBox> B = UiRow();
			UiAdd(B, UiButton(TEXT("Open a savings account"), [this]() { Act([this](FString& S) { S = TEXT("Your savings account is open."); return E->OpenAccount(TEXT("savings")); }); }));
			Slot(B);
		}
		// move money: from, to, how much (BANK-007)
		TArray<FString> Accts = { TEXT("p:cash") };
		for (const FString& K : E->Accounts) Accts.Add(TEXT("p:") + K);
		auto Name = [&R](const FString& A) { return A == TEXT("p:cash") ? FString(TEXT("Cash")) : R.Account(A.Mid(2))->Name; };
		MoveFrom = MoveFrom % Accts.Num();
		MoveTo = MoveTo % Accts.Num();
		Slot(UiHeading(TEXT("Move money")), 0);
		TSharedRef<SHorizontalBox> M = UiRow();
		UiAdd(M, UiButton(Name(Accts[MoveFrom]) , [this, N = Accts.Num()]() { MoveFrom = (MoveFrom + 1) % N; Draw(); }, true, true));
		UiAdd(M, UiText(TEXT("to"), 15));
		UiAdd(M, UiButton(Name(Accts[MoveTo]) , [this, N = Accts.Num()]() { MoveTo = (MoveTo + 1) % N; Draw(); }, true, true));
		UiAdd(M, UiText(TEXT("Bx"), 15));
		UiAdd(M, SNew(SBox).WidthOverride(80.f)[ SNew(SEditableTextBox).Text(FText::FromString(Amount)).Font(UiFont(14))
			.OnTextChanged_Lambda([this](const FText& T) { Amount = T.ToString(); }) ]);
		UiAdd(M, UiButton(TEXT("Move"), [this, Accts]()
		{
			Act([this, Accts](FString& S)
			{
				const int64 C = FMath::RoundToInt64(FCString::Atod(*Amount) * 100.0);
				S = TEXT("Moved ") + Format(C) + TEXT(".");
				return E->Move(Accts[MoveFrom], Accts[MoveTo], C);
			});
		}));
		Slot(M);
		// loans (BANK-002)
		Slot(UiHeading(TEXT("Loans")), 0);
		if (FBrixLoan* Loan = E->ActiveLoan())
		{
			FString S = TEXT("You owe ") + Format(Loan->Owed()) + TEXT(". Instalments of ") + Format(Loan->Plan[0]) + TEXT(" are taken from your current account each midnight");
			S += Loan->Missed ? FString::Printf(TEXT("; %d missed so far (%s fee each; %d in a row and the bank repossesses your things)."), Loan->Missed, *Format(R.LateFee), R.DefaultAfterMissed) : FString(TEXT("."));
			Slot(UiText(S));
			TSharedRef<SHorizontalBox> B = UiRow();
			const int32 Id = Loan->Id;
			UiAdd(B, UiButton(TEXT("Repay it all now"), [this, Id]() { Act([this, Id](FString& S) { S = TEXT("Loan repaid."); return E->Repay(Id); }); }));
			Slot(B);
		}
		else if (E->bBarred) Slot(UiText(TEXT("After your default the bank will not lend to you again."), 15, UiNeg));
		else
		{
			TArray<int64> Sizes;
			for (int64 a = R.LoanMin; a <= R.LoanMax; a += R.LoanStep) Sizes.Add(a);
			LoanSize = FMath::Clamp(LoanSize, 0, Sizes.Num() - 1);
			LoanTerm = FMath::Clamp(LoanTerm, 0, R.LoanTerms.Num() - 1);
			const int64 Size = Sizes[LoanSize];
			const int32 Term = R.LoanTerms[LoanTerm];
			TSharedRef<SHorizontalBox> B = UiRow();
			UiAdd(B, UiText(TEXT("Borrow"), 15));
			UiAdd(B, UiButton(TEXT("◂"), [this]() { LoanSize = FMath::Max(0, LoanSize - 1); Draw(); }, LoanSize > 0, true));
			UiAdd(B, UiText(Format(Size), 15, UiCream, true));
			UiAdd(B, UiButton(TEXT("▸"), [this, N = Sizes.Num()]() { LoanSize = FMath::Min(N - 1, LoanSize + 1); Draw(); }, LoanSize < Sizes.Num() - 1, true));
			UiAdd(B, UiText(TEXT("over"), 15));
			UiAdd(B, UiButton(FString::Printf(TEXT("%d days"), Term), [this, N = R.LoanTerms.Num()]() { LoanTerm = (LoanTerm + 1) % N; Draw(); }, true, true));
			UiAdd(B, UiButton(TEXT("Borrow"), [this, Size, Term]() { Act([this, Size, Term](FString& S) { S = TEXT("The loan is in your current account."); return E->TakeLoan(Size, Term); }); }));
			Slot(B);
			const TPair<int64, TArray<int64>> T = E->LoanTerms(Size, Term);
			Slot(UiText(FString::Printf(TEXT("You pay back %s in all: %d daily instalments of about %s, from tomorrow midnight. Late fee %s."),
				*Format(T.Key), T.Value.Num(), *Format(T.Value[0]), *Format(R.LateFee)), 14, UiDim));
		}
		FString Old;
		for (const FBrixLoan& Ln : E->Loans) if (Ln.Status != TEXT("active")) Old += (Old.IsEmpty() ? TEXT("") : TEXT(" · ")) + FString::Printf(TEXT("Loan %d: %s"), Ln.Id, *Ln.Status);
		if (!Old.IsEmpty()) Slot(UiText(Old, 14, UiDim));
	}
	Slot(UiHeading(TEXT("Statement")), 0);
	Slot(StatementTable(BankStatementLines), 0);
}

// ECO-009 (life.js draw(), kind "wallet")
void ABrixLife::WalletPanel(TSharedRef<SVerticalBox> Body)
{
	Body->AddSlot().AutoHeight()[ PurseLine() ];
	if (FBrixLoan* Loan = E->ActiveLoan()) Body->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[ UiText(TEXT("Loan: ") + Format(Loan->Owed()) + TEXT(" owed to ") + Lots[E->BankLot].Name + TEXT(".")) ];
	Body->AddSlot().AutoHeight()[ UiHeading(TEXT("Statement")) ];
	Body->AddSlot().AutoHeight()[ StatementTable(WalletStatementLines) ];
}

// ---- for test shots

bool ABrixLife::RunOps(const FString& Ops)
{
	if (!E) return false;
	bool bAll = true;
	TArray<FString> List;
	Ops.ParseIntoArray(List, TEXT(";"));
	for (const FString& Op : List)
	{
		TArray<FString> A;
		Op.ParseIntoArray(A, TEXT(","));
		if (!A.Num()) continue;
		auto I = [&](int32 k) { return A.IsValidIndex(k) ? FCString::Atoi64(*A[k]) : 0; };
		auto Sv = [&](int32 k) { return A.IsValidIndex(k) ? A[k] : FString(); };
		bool bOk = true;
		if (A[0] == TEXT("open")) bOk = E->OpenAccount(Sv(1));
		else if (A[0] == TEXT("move")) bOk = E->Move(Sv(1), Sv(2), I(3));
		else if (A[0] == TEXT("loan")) bOk = E->TakeLoan(I(1), (int32)I(2));
		else if (A[0] == TEXT("rent")) bOk = E->RentStall();
		else if (A[0] == TEXT("buy") || A[0] == TEXT("sell"))
		{
			int32 n = E->LotIndex.FindRef(Sv(1), -1);
			for (int32 k = 0; n < 0 && k < Lots.Num(); ++k) if (E->ShopKind(k) == Sv(1) && !E->Outlets.Contains(k)) n = k;
			bOk = n >= 0 && (A[0] == TEXT("buy") ? E->Buy(n, Sv(2)) : E->Sell(n, Sv(2)));
		}
		else if (A[0] == TEXT("setout")) bOk = E->SetOut(Sv(1), I(2));
		else { UE_LOG(LogTemp, Warning, TEXT("Brix Town: -BrixLife does not know %s"), *A[0]); bOk = false; }
		UE_LOG(LogTemp, Display, TEXT("Brix Town: -BrixLife %s: %s%s"), *Op, bOk ? TEXT("ok") : TEXT("refused, "), bOk ? TEXT("") : *E->Why);
		bAll &= bOk;
	}
	LastMoney = E->TotalMoney();
	return bAll;
}

bool ABrixLife::OpenCounterAt(const FString& What)
{
	if (!E) return false;
	int32 n = What == TEXT("bank") ? E->BankLot : What == TEXT("market") ? E->MarketLot : E->LotIndex.FindRef(What, -1);
	for (int32 k = 0; n < 0 && k < Lots.Num(); ++k) if (E->ShopKind(k) == What && !E->Outlets.Contains(k)) n = k;   // the first shop of that kind
	if (n < 0) return false;
	APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Me) return false;
	const FVector2D Door = LotDoor[n], Out = (Door - Lots[n].Centre()).GetSafeNormal();
	Me->SetActorLocation(FVector((Door + Out * 2.0) * CmPerM, 100.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (APlayerController* PC = Cast<APlayerController>(Me->GetController()))
		PC->SetControlRotation(FRotator(-10.0, FMath::RadiansToDegrees(FMath::Atan2(-Out.Y, -Out.X)), 0));
	CounterKey();
	return IsOpen();
}

// ---- shops and the market stall (ECO-005, ECO-010, ECO-011, PROP-008; life.js draw(), kinds "shop" and "stall")

ABrixLife::FWare ABrixLife::Ware(const FString& Id, const FString& Name, int64 Price, const FString& Goes, int32 Left, const FString& Band) const
{
	FWare W;
	W.Id = Id; W.Name = Name; W.Price = Price; W.Goes = Goes; W.Left = Left; W.Band = Band;
	const int64 Funds = E->Funds();
	W.State = Left >= 0 && Left < 1 ? TEXT("out") : Price > Funds ? TEXT("short") : TEXT("ok");
	W.Short = FMath::Max<int64>(0, Price - Funds);
	return W;
}

// a shop's own things are carried; what cannot be sold on and comes in a bag or a cup is to eat (life.js shopGoes())
FString ABrixLife::ShopGoes(const FString& It) const
{
	const FBrixEconomyRules::FItem* I = E->Item(It);
	const FBrixShows S = BuyMenu.Shows(It);
	return I && !I->bSell && S.Name != BuyMenu.StandInGoes.FindRef(TEXT("carry")) ? TEXT("eat") : TEXT("carry");
}

TSharedPtr<double> ABrixLife::YawFor(const FString& Key)
{
	TSharedPtr<double>& Y = Yaws.FindOrAdd(Key);
	if (!Y) Y = MakeShared<double>(FMath::DegreesToRadians(BuyMenu.StartDeg));
	return Y;
}

TSharedPtr<const FBrixThingMesh> ABrixLife::MeshFor(const FString& Thing, const FString& Band)
{
	const FString Key = Thing + TEXT("|") + Band;
	if (const TSharedPtr<const FBrixThingMesh>* M = Meshes.Find(Key)) return *M;
	const TOptional<FBrixThingMesh> M = BuyMenu.MeshFor(Thing, Band);
	TSharedPtr<const FBrixThingMesh> Got; if (M) Got = MakeShared<const FBrixThingMesh>(*M); return Meshes.Add(Key, Got);
}

void ABrixLife::ShopPanel(TSharedRef<SVerticalBox> Body)
{
	const int32 n = PanelLot;
	const FString K = E->ShopKind(n);
	const FBrixEconomyRules::FShop& S = *E->R.Shop(K);
	const FString Band = BuyMenu.Bands.FindRef(K);
	TArray<FString> Mine;
	for (const auto& It : E->Inv) if (E->Offer(n, It.Key) > 0) Mine.Add(It.Key);
	Mine.Sort([](const FString& A, const FString& B) { return FCString::Strcmp(*A, *B) < 0; });
	const bool bSelling = Side == TEXT("sell") && S.Buys.Num() > 0;
	Body->AddSlot().AutoHeight()[ PurseLine() ];
	if (S.Buys.Num())
	{
		TSharedRef<SHorizontalBox> Tabs = UiRow();
		UiAdd(Tabs, UiButton(TEXT("Buy"), [this]() { Side = TEXT("buy"); Msg.Reset(); Draw(); }, true, bSelling));
		UiAdd(Tabs, UiButton(Mine.Num() ? FString::Printf(TEXT("Sell (%d)"), Mine.Num()) : FString(TEXT("Sell")), [this]() { Side = TEXT("sell"); Msg.Reset(); Draw(); }, true, !bSelling));
		Body->AddSlot().AutoHeight().Padding(0, 10, 0, 4)[ Tabs ];
	}
	else Body->AddSlot().AutoHeight()[ UiHeading(TEXT("Buy")) ];
	TArray<FWare> Things;
	FGridOpts O;
	if (!bSelling)
	{
		for (const FString& It : S.Sells) Things.Add(Ware(It, E->Item(It)->Name, E->Price(n, It), ShopGoes(It), E->Stock(n, It), Band));
		O.Id = FString::Printf(TEXT("shop:%d:buy"), n);
		O.StockLabel = TEXT("In stock");
		O.Buy = [this, n](const FWare& W, FString& Said)
		{
			int64 P = 0;
			if (!E->Buy(n, W.Id, &P)) return false;
			Said = TEXT("Bought ") + W.Name.ToLower() + TEXT(" for ") + Format(P) + TEXT(".");
			return true;
		};
	}
	else
	{
		for (const FString& It : Mine) { FWare W = Ware(It, E->Item(It)->Name, E->Offer(n, It), FString(), E->Inv.FindRef(It), Band); W.State = TEXT("ok"); W.Short = 0; Things.Add(W); }
		O.Id = FString::Printf(TEXT("shop:%d:sell"), n);
		O.Verb = TEXT("Sell");
		O.StockLabel = TEXT("You have");
		O.PriceLabel = TEXT("They pay");
		O.Empty = TEXT("Nothing you carry is wanted here.");
		O.bMine = true;
		O.Buy = [this, n](const FWare& W, FString& Said)
		{
			int64 P = 0;
			if (!E->Sell(n, W.Id, &P)) return false;
			Said = TEXT("Sold for ") + Format(P) + TEXT(".");
			return true;
		};
	}
	Body->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[ Grid(Things, O) ];
	if (!S.Buys.Num()) Body->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[ UiText(TEXT("Cafés do not buy anything."), 15, UiDim) ];
	const int32 r = E->RepWithLot(n);
	if (r) Body->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[ UiText(r > 0 ? TEXT("They know you here: prices are a little lower.") : TEXT("They don't trust you here: prices are higher."), 15, UiDim) ];
}

// layout B: a grid of the things, each drawn as an object with its price and where it goes, and beside it the chosen one
// large, turning, with its name, where it goes, what is left, its price, and the one button (buymenu.js grid())
TSharedRef<SWidget> ABrixLife::Grid(const TArray<FWare>& Things, const FGridOpts& O)
{
	if (!Things.Num()) return UiText(O.Empty, 15, UiDim);
	FString PickId = Chosen.FindRef(O.Id);
	const FWare* Pick = Things.FindByPredicate([&](const FWare& W) { return W.Id == PickId; });
	if (!Pick) { Pick = Things.FindByPredicate([](const FWare& W) { return W.State != TEXT("out"); }); if (!Pick) Pick = &Things[0]; Chosen.Add(O.Id, Pick->Id); }
	const FWare P = *Pick;
	const float ListW = BuyMenu.WidePanelPx - 40.f - BuyMenu.ShowPx - 16.f, Gap = 9.f;
	const int32 Cols = FMath::Max(1, (int32)((ListW + Gap) / (BuyMenu.CellPx + Gap)));
	const float CellW = (ListW - Gap * (Cols - 1)) / Cols;
	FLive Live;
	Live.Id = O.Id; Live.Things = Things; Live.O = O; Live.Columns = Cols;
	LiveGrid = Live;
	auto Board = [this](const FString& Goes) { const FBrixBuyMenuRules::FGoes* G = BuyMenu.Goes.Find(Goes); const TPair<FColor, FColor>* C = G ? BuyMenu.BoardColours.Find(G->Colour) : nullptr; return C ? *C : TPair<FColor, FColor>(FColor(59, 60, 74), FColor(232, 223, 201)); };
	auto Chip = [this, Board](const FString& Goes) -> TSharedRef<SWidget>
	{
		const FBrixBuyMenuRules::FGoes* G = BuyMenu.Goes.Find(Goes);
		if (!G) return SNullWidget::NullWidget;
		const TPair<FColor, FColor> C = Board(Goes);
		return SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(FLinearColor::FromSRGBColor(C.Key)).Padding(FMargin(8, 2))
			[ SNew(STextBlock).Font(UiFont(10, true)).ColorAndOpacity(FLinearColor::FromSRGBColor(C.Value)).Text(FText::FromString(G->Name)) ];
	};
	auto Price = [](const FWare& W) { return UiText(Format(W.Price), 14, W.State == TEXT("short") ? UiNeg : UiCream, true); };
	auto LeftWords = [this, &O](const FWare& W) -> FString
	{
		if (W.Left < 0) return FString();
		if (O.bMine) return FString::Printf(TEXT("you have %d"), W.Left);
		if (W.State == TEXT("out")) return TEXT("sold out, back at ") + UiHhmm(E->R.RestockMinute);
		return FString::Printf(TEXT("%d left"), W.Left);
	};
	auto Flag = [](bool bOut) -> TSharedRef<SWidget>
	{
		if (!bOut) return SNullWidget::NullWidget;
		return SNew(SBox).VAlign(VAlign_Center)
			[ SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(FLinearColor::FromSRGBColor(FColor(122, 66, 52))).Padding(FMargin(0, 3)).HAlign(HAlign_Center).RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(-4.f))))
				[ SNew(STextBlock).Font(UiFont(10, true)).ColorAndOpacity(FLinearColor::FromSRGBColor(FColor(251, 243, 224))).Text(FText::FromString(TEXT("SOLD OUT"))) ] ];
	};
	TSharedRef<SGridPanel> List = SNew(SGridPanel);
	for (int32 k = 0; k < Things.Num(); ++k)
	{
		const FWare W = Things[k];
		const bool bSel = W.Id == P.Id, bOut = W.State == TEXT("out");
		const TPair<FColor, FColor> Dot = Board(W.Goes);
		List->AddSlot(k % Cols, k / Cols).Padding(k % Cols ? Gap : 0.f, k / Cols ? Gap : 0.f, 0.f, 0.f)
		[
			SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).IsFocusable(false).ContentPadding(0)
			.OnClicked_Lambda([this, W, Id = O.Id]()
			{
				const double Now = FPlatformTime::Seconds();
				const bool bTwice = ClickedId == Id + TEXT(":") + W.Id && Now - ClickedAt < .45;
				ClickedId = bTwice ? FString() : Id + TEXT(":") + W.Id;
				ClickedAt = Now;
				if (bTwice) { if (W.State == TEXT("ok") && LiveGrid) { const FGridOpts Op = LiveGrid->O; Act([Op, W](FString& S) { return Op.Buy(W, S); }); } return FReply::Handled(); }
				if (Chosen.FindRef(Id) != W.Id) { Chosen.Add(Id, W.Id); Draw(); }
				return FReply::Handled();
			})
			[
				SNew(SBox).WidthOverride(CellW)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(bSel ? UiLantern : FLinearColor::Transparent).Padding(2)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[ SNew(SBrixThing).Mesh(MeshFor(W.Id, W.Band)).Rules(&BuyMenu).Grey(bOut).Size(FVector2D(CellW - 4, BuyMenu.StagePx)).Yaw(YawFor(O.Id + TEXT(":") + W.Id)) ]
							+ SOverlay::Slot().VAlign(VAlign_Center)[ Flag(bOut) ]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(2, 5, 2, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[ Price(W) ]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[ W.Goes.IsEmpty() ? SNullWidget::NullWidget : (TSharedRef<SWidget>)(SNew(SBox).WidthOverride(14).HeightOverride(14)[ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("Icons.FilledCircle")).BorderBackgroundColor(FLinearColor::FromSRGBColor(Dot.Key)) ]) ]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(2, 1, 2, 0)[ SNew(STextBlock).Font(UiFont(11)).ColorAndOpacity(UiDim).Text(FText::FromString(W.Name)).OverflowPolicy(ETextOverflowPolicy::Ellipsis) ]
				]
			]
		];
	}
	// the showcase
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	auto Line = [&Rows](const FString& A, TSharedRef<SWidget> B)
	{
		Rows->AddSlot().AutoHeight()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
			[ SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(.42f)[ UiText(A, 14, UiDim) ] + SHorizontalBox::Slot().FillWidth(.58f).HAlign(HAlign_Right)[ B ] ]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(UiRule).Padding(0)[ SNew(SBox).HeightOverride(1.f) ] ]
		];
	};
	if (BuyMenu.Goes.Contains(P.Goes)) Line(TEXT("Goes to"), Chip(P.Goes));
	if (P.Left >= 0) Line(O.StockLabel, UiText(O.bMine ? FString::FromInt(P.Left) : LeftWords(P), 14, P.State == TEXT("out") ? UiNeg : P.Left == 1 ? UiGold : UiCream));
	Line(O.PriceLabel, UiText((P.State == TEXT("short") ? Format(P.Short) + TEXT(" short  ") : FString()) + Format(P.Price), 14, P.State == TEXT("short") ? UiNeg : UiCream, true));
	const FGridOpts Op = O;
	const float StageW = BuyMenu.ShowPx - 24.f;
	TSharedRef<SWidget> Show = SNew(SBox).WidthOverride((float)BuyMenu.ShowPx)
	[
		SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(FLinearColor::FromSRGBColor(FColor(217, 207, 182, 15))).Padding(FMargin(12, 10, 12, 12))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[ SNew(SBrixThing).Mesh(MeshFor(P.Id, P.Band)).Rules(&BuyMenu).Spin(true).Drag(true).Grey(P.State == TEXT("out")).Size(FVector2D(StageW, BuyMenu.ShowStagePx)).Yaw(YawFor(O.Id + TEXT(":show:") + P.Id)) ]
				+ SOverlay::Slot().VAlign(VAlign_Center)[ Flag(P.State == TEXT("out")) ]
				+ SOverlay::Slot().VAlign(VAlign_Bottom).HAlign(HAlign_Center).Padding(0, 0, 0, 5)[ SNew(STextBlock).Visibility(EVisibility::HitTestInvisible).Font(UiFont(10)).ColorAndOpacity(FLinearColor::FromSRGBColor(FColor(217, 207, 182, 115))).Text(FText::FromString(TEXT("drag to turn"))) ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 6)[ UiText(P.Name, 21) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[ Rows ]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).IsFocusable(false).IsEnabled(P.State == TEXT("ok")).ContentPadding(0)
				.OnClicked_Lambda([this, Op, P]() { Act([Op, P](FString& S) { return Op.Buy(P, S); }); return FReply::Handled(); })
				[
					SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(P.State == TEXT("ok") ? UiLantern : UiDisabled).Padding(FMargin(12, 9)).HAlign(HAlign_Center)
					[ SNew(STextBlock).Font(UiFont(13, true)).ColorAndOpacity(UiInk).Text(FText::FromString(O.Verb + TEXT(" · ") + Format(P.Price))) ]
				]
			]
		]
	];
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 16, 0)[ SNew(SBox).MaxDesiredHeight((float)BuyMenu.ListPx)[ SNew(SScrollBox) + SScrollBox::Slot()[ List ] ] ]
		+ SHorizontalBox::Slot().AutoWidth()[ Show ];
}

bool ABrixLife::ArrowKey(int32 Dx, int32 Dy)
{
	if (!LiveGrid || !LiveGrid->Things.Num()) return false;
	const FLive& G = *LiveGrid;
	const int32 At = FMath::Max(0, G.Things.IndexOfByPredicate([&](const FWare& W) { return W.Id == Chosen.FindRef(G.Id); }));
	const int32 To = At + Dx + Dy * G.Columns;
	if (G.Things.IsValidIndex(To) && (Dy != 0 || To / G.Columns == At / G.Columns)) { Chosen.Add(G.Id, G.Things[To].Id); Draw(); }
	return true;
}

bool ABrixLife::EnterKey()
{
	if (!LiveGrid) return false;
	const FLive G = *LiveGrid;
	const FWare* W = G.Things.FindByPredicate([&](const FWare& X) { return X.Id == Chosen.FindRef(G.Id); });
	if (W && W->State == TEXT("ok")) { const FWare Thing = *W; Act([G, Thing](FString& S) { return G.O.Buy(Thing, S); }); }
	return true;
}

void ABrixLife::StallPanel(TSharedRef<SVerticalBox> Body)
{
	const FBrixEconomyRules& R = E->R;
	auto Slot = [&](TSharedRef<SWidget> W, float Top = 6) { Body->AddSlot().AutoHeight().Padding(0, Top, 0, 0)[ W ]; };
	Slot(PurseLine(), 0);
	if (!E->Stall)
	{
		Slot(UiText(FString::Printf(TEXT("Rent a stall for today: %s. Set out up to %d things at your own prices; what is left comes home at midnight."), *Format(R.StallRent), R.StallSlots)));
		TSharedRef<SHorizontalBox> B = UiRow();
		UiAdd(B, UiButton(TEXT("Rent a stall for today"), [this]() { Act([this](FString& S) { S = TEXT("The stall is yours until midnight."); return E->RentStall(); }); }));
		Slot(B);
		return;
	}
	Slot(UiHeading(TEXT("On your stall")), 0);
	if (!E->Stall->Slots.Num()) Slot(UiText(TEXT("Nothing yet."), 15, UiDim));
	for (int32 k = 0; k < E->Stall->Slots.Num(); ++k)
	{
		const FBrixStall::FSlot& Sl = E->Stall->Slots[k];
		TSharedRef<SHorizontalBox> B = SNew(SHorizontalBox);
		B->AddSlot().FillWidth(1).VAlign(VAlign_Center)[ UiText(E->Item(Sl.Item)->Name) ];
		B->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)[ UiText(Format(Sl.Price), 15, UiCream, true) ];
		if (Sl.bSold) B->AddSlot().AutoWidth().VAlign(VAlign_Center)[ UiText(TEXT("sold"), 15, UiPos) ];
		else B->AddSlot().AutoWidth()[ UiButton(TEXT("Take back"), [this, k]() { Act([this, k](FString& S) { S.Reset(); return E->TakeBack(k); }); }) ];
		Slot(B, 4);
	}
	TArray<FString> Can;
	for (const auto& It : E->Inv) { const FBrixEconomyRules::FItem* I = E->Item(It.Key); if (I && !I->bQuest && I->Price > 0) Can.Add(It.Key); }
	Can.Sort([](const FString& A, const FString& B) { return FCString::Strcmp(*A, *B) < 0; });
	if (Can.Num() && E->Stall->Slots.Num() < R.StallSlots)
	{
		StallPick = StallPick % Can.Num();
		const FString It = Can[StallPick];
		const FBrixEconomyRules::FItem* I = E->Item(It);
		if (StallPrice.IsEmpty()) StallPrice = FString::Printf(TEXT("%.2f"), I->Price / 100.0);
		TSharedRef<SHorizontalBox> B = UiRow();
		UiAdd(B, UiText(TEXT("Set out")));
		UiAdd(B, UiButton(FString::Printf(TEXT("%s (%d)"), *I->Name, E->Inv.FindRef(It)), [this, N = Can.Num()]() { StallPick = (StallPick + 1) % N; StallPrice.Reset(); Draw(); }, true, true));
		UiAdd(B, UiText(TEXT("at Bx")));
		UiAdd(B, SNew(SBox).WidthOverride(80.f)[ SNew(SEditableTextBox).Text(FText::FromString(StallPrice)).Font(UiFont(14)).OnTextChanged_Lambda([this](const FText& T) { StallPrice = T.ToString(); }) ]);
		UiAdd(B, UiButton(TEXT("Set out"), [this, It]()
		{
			Act([this, It](FString& S) { S.Reset(); const bool bOk = E->SetOut(It, FMath::RoundToInt64(FCString::Atod(*StallPrice) * 100.0)); if (bOk) StallPrice.Reset(); return bOk; });
		}));
		UiAdd(B, UiText(TEXT("usually ") + Format(I->Price), 14, UiDim));
		Slot(B, 10);
		Slot(UiText(TEXT("Cheaper than usual sells fast; dearer sells slowly."), 14, UiDim));
	}
	else if (!Can.Num()) Slot(UiText(TEXT("You have nothing to sell. Buy cheap at a shop, sell dear here."), 14, UiDim));
}
