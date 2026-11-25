#include "BrixLifeActor.h"

#include "BrixCharacter.h"
#include "BrixCity.h"
#include "BrixGameMode.h"
#include "BrixTalkActor.h"
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
		return SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).IsEnabled(bEnabled)
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
		if (n != E->BankLot) continue;   // the bank's counter; shops' and the market's come with them
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
			return !IsOpen() && !bTalk && CounterHere() >= 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[ SNew(STextBlock).Font(UiFont(15)).ColorAndOpacity(UiGold).Text(FText::FromString(TEXT("B"))) ]
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(STextBlock).Font(UiFont(15)).ColorAndOpacity(UiCream).Text_Lambda([this]() { return FText::FromString(CounterHere() == (E ? E->BankLot : -2) ? FString(TEXT("Bank counter")) : FString()); }) ]
		]
	];
	SAssignNew(PanelRoot, SBox)
		.HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(16, 40))
		.Visibility_Lambda([this]() { return IsOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
		[
			SNew(SBox).WidthOverride(620.f)
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

void ABrixLife::CounterKey()
{
	if (Panel == TEXT("bank")) { Close(); return; }
	const int32 n = CounterHere();
	if (n < 0) { Toast(TEXT("No counter here. Shops, cafés, the bank and the market square have one.")); return; }
	if (n == E->BankLot) Open(TEXT("bank"), n);
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
	if (Panel == TEXT("bank")) { Title = Lots[PanelLot].Name; Sub = TEXT("Accounts, savings and loans"); }
	else { Title = TEXT("Money"); Sub = PlayerName + TEXT(" · I"); }
	if (!Msg.IsEmpty())
		Body->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[ SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(UiMsgBack).Padding(FMargin(10, 6))[ UiText(Msg, 15) ] ];
	if (Panel == TEXT("bank")) BankPanel(Body);
	else WalletPanel(Body);
	PanelBody->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(STextBlock).Font(UiFont(24)).ColorAndOpacity(UiCream).Text(FText::FromString(Title)) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(12, 0, 0, 3)[ SNew(STextBlock).Font(UiFont(14)).ColorAndOpacity(UiDim).Text(FText::FromString(Sub)) ]
		]
		+ SVerticalBox::Slot().FillHeight(1)[ SNew(SScrollBox) + SScrollBox::Slot()[ Body ] ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 8, 0, 0)[ UiText(TEXT("Esc  close"), 13, UiDim) ]);
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
	const int32 n = What == TEXT("bank") ? E->BankLot : E->LotIndex.FindRef(What, -1);
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
