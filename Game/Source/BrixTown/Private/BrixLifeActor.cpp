#include "BrixLifeActor.h"

#include "BrixSave.h"

#include "BrixCharacter.h"
#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixGameMode.h"
#include "BrixMallActor.h"
#include "BrixLook.h"
#include "BrixMapActor.h"
#include "BrixInside.h"
#include "BrixTownActor.h"
#include "Camera/CameraActor.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "BrixTalkActor.h"
#include "BrixThingWidget.h"
#include "BrixUnits.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
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
		return BrixLook::Button(Label, OnClick, !bQuiet, bEnabled);
	}
	// UI-020: the mark of where a thing goes, for a buy menu's dots and chips (buymenu.js ICON): painted once from its paths
	// into a white picture whose alpha is the mark, drawn in the board colour's lettering
	const FSlateBrush* MarkBrush(const FBrixBuyMenuRules& B, const FString& Goes)
	{
		static TMap<FString, TUniquePtr<FSlateBrush>> Made;
		if (const TUniquePtr<FSlateBrush>* Have = Made.Find(Goes)) return Have->Get();
		TUniquePtr<FSlateBrush>& Br = Made.Add(Goes);
		const TArray<FBrixIconPath>* Paths = B.Icons.Find(Goes);
		const int32 Px = 64;
		UTexture2D* T = Paths && Paths->Num() ? UTexture2D::CreateTransient(Px, Px, PF_B8G8R8A8) : nullptr;
		if (!T) return nullptr;
		const TArray<float> Cover = BrixIconCoverage(*Paths, Px);
		T->SRGB = true;
		T->Filter = TF_Trilinear;
		FColor* Dst = static_cast<FColor*>(T->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		for (int32 i = 0; i < Px * Px; ++i) Dst[i] = FColor(255, 255, 255, (uint8)FMath::RoundToInt(Cover[i] * 255.f));
		T->GetPlatformData()->Mips[0].BulkData.Unlock();
		T->UpdateResource();
		T->AddToRoot();   // painted once, kept for the whole visit
		Br = MakeUnique<FSlateBrush>();
		Br->SetResourceObject(T);
		Br->ImageSize = FVector2D(Px, Px);
		Br->DrawAs = ESlateBrushDrawType::Image;
		return Br.Get();
	}
	TSharedRef<SWidget> UiMark(const FBrixBuyMenuRules& B, const FString& Goes, float Size, const FLinearColor& Ink)
	{
		const FSlateBrush* Br = MarkBrush(B, Goes);
		if (!Br) return SNullWidget::NullWidget;
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)[ SNew(SImage).Image(Br).ColorAndOpacity(Ink) ];
	}
	TSharedRef<SHorizontalBox> UiRow() { return SNew(SHorizontalBox); }
	void UiAdd(TSharedRef<SHorizontalBox> R, TSharedRef<SWidget> W) { R->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[ W ]; }
}

ABrixLife::ABrixLife()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ABrixLife::Start(const FBrixCity& City, TSharedPtr<FBrixDay> InDay)
{
	Day = InDay;
	CityName = City.Name;
	FString Error;
	if (!LoadBrixLife(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), LifeFile)), Data, Error) || !BrixEconLots(City, Data, Lots, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Brix Town: no money or shops (%s)"), *Error);
		return;
	}
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) { LotAngle.Add(L.AngleDeg); LotDoor.Add(L.Door); DesignLots.Add(BrixBuilder::LotOf(L, B)); }
	E = MakeUnique<FBrixEconomy>(Data, Lots);
	if (!LoadBrixBuyMenu(Data.Json, BuyMenu, Error)) UE_LOG(LogTemp, Warning, TEXT("Brix Town: buy menus without pictures (%s)"), *Error);
	double D0 = 0, M0 = 0;
	Frame(D0, M0);
	E->Advance((int32)D0, M0);
	if (LoadBrixEstate(Data, EstateRules, EstateUnits, EstateRoles, Error)) Est = MakeUnique<FBrixEstate>(EstateRules, *E, EstateUnits, EstateRoles);   // PROP-009
	else UE_LOG(LogTemp, Warning, TEXT("Brix Town: no property market (%s)"), *Error);
	if (LoadBrixQuests(Data.Json, QuestRules, QuestResidents, Error))
	{
		Q = MakeUnique<FBrixQuests>(QuestRules, *E, QuestResidents);
		Q->OnNote = [this](const FString& T) { Toast(T); };
		Q->NewGame();   // a new game (life.js newGame()): the starting cash, and the first starter quest line
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Brix Town: no quests (%s)"), *Error);
		E->NewCharacter();
	}
	if (Est) Est->Start();   // the estate agents open (life.js newGame(): M.start())
	LastMoney = E->TotalMoney();
	if (!LoadBrixRewards(Data.Json, RewardRules, AchievementRules, Error)) UE_LOG(LogTemp, Warning, TEXT("Brix Town: no reward showings (%s)"), *Error);
	SeenTx = E->L.Tx.Num();   // what a new game starts with is not shown (rewards.js reset())
	UE_LOG(LogTemp, Display, TEXT("Brix Town: a new game with %s in cash; the bank is %s, the market %s"), *Format(E->L.Balance(TEXT("p:cash"))),
		E->BankLot >= 0 ? *Lots[E->BankLot].Name : TEXT("missing"), E->MarketLot >= 0 ? *Lots[E->MarketLot].Name : TEXT("missing"));
	BuildUI();
	StartMenu();
}

void ABrixLife::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GEngine && GEngine->GameViewport)
	{
		if (MenuRoot) GEngine->GameViewport->RemoveViewportWidgetContent(MenuRoot.ToSharedRef());
		if (Hud) GEngine->GameViewport->RemoveViewportWidgetContent(Hud.ToSharedRef());
		if (PanelRoot) GEngine->GameViewport->RemoveViewportWidgetContent(PanelRoot.ToSharedRef());
	}
	Super::EndPlay(Reason);
}

// the economy's day and minute: days since the new game, by the game's clock
double ABrixLife::Minute() const { double D1 = 0, M1 = 0; Frame(D1, M1); return M1; }

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
		if (n != E->BankLot && n != E->MarketLot && E->ShopKind(n).IsEmpty() && !(Est && Est->Office.Contains(Lots[n].Id))) continue;   // the bank, the market, the shops and the estate agents have a counter
		const FBrixEconLot& L = Lots[n];
		const FVector2D C = L.Centre(), D = P - C;
		const double A = FMath::DegreesToRadians(-LotAngle[n]);
		const FVector2D U(D.X * FMath::Cos(A) - D.Y * FMath::Sin(A), D.X * FMath::Sin(A) + D.Y * FMath::Cos(A));
		const FVector2D Q2 = U + C;
		if (Q2.X >= L.Min.X && Q2.X <= L.Max.X && Q2.Y >= L.Min.Y && Q2.Y <= L.Max.Y) return n;
		const double Dd = FVector2D::Distance(P, LotDoor[n]);
		if (Dd < BestD) { BestD = Dd; Best = n; }
	}
	return Best;
}

void ABrixLife::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!E) return;
	TickMenuShot();
	double D0 = 0, M0 = 0;
	Frame(D0, M0);
	E->Advance((int32)D0, M0);   // the bank settles each midnight passed (BANK-008)
	if (Q && (CheckLeft -= Dt) <= 0.f) { CheckLeft = CheckSeconds; Q->Check(Ctx()); CheckAchievements(); }
	Shown = 0;
	TickRewards(Dt);   // coins first; their "+" is the purse counting up as they land (SND-019, SND-020)
	SlipLeft = FMath::Max(0.0, SlipLeft - Dt);   // goals met where the player is (QST-001, QST-006)
	const int64 Tm = E->TotalMoney();
	if (Tm != LastMoney)
	{
		const int64 D = Tm - LastMoney - Shown;
		if (D) DeltaText = (D > 0 ? TEXT("+") : TEXT("")) + Format(D);
		if (D) { bDeltaUp = D > 0; DeltaLeft = PlusSeconds; }
		if (IsOpen()) Draw();
	}
	LastMoney = Tm;
	ToastLeft = FMath::Max(0.f, ToastLeft - Dt);
	DeltaLeft = FMath::Max(0.f, DeltaLeft - Dt);
}

void ABrixLife::SaveTo(FBrixSaveGame& S) const
{
	if (!E) return;
	S.Day = E->Day;
	S.Minute = E->Minute;
	S.Econ = BrixEconomySave(*E);
	if (Q) S.Quests = BrixQuestsSave(*Q);
	if (Est) S.Estate = Est->Save();   // PROP-021: the market comes back as it was
	S.Remodels = MakeShared<FJsonObject>();   // QST-008: the buildings remodelled, each its design as paid for (life.js remodels)
	for (const auto& R : RemodelsDone) S.Remodels->SetObjectField(R.Key, R.Value);
	S.Feats = Feats;
}

bool ABrixLife::LoadFrom(const FBrixSaveGame& S, FString& OutWhy)   // life.js loadSave()
{
	if (!E || !S.Econ || !S.Quests) { OutWhy = TEXT("nothing to load"); return false; }
	TUniquePtr<FBrixEconomy> NewE = MakeUnique<FBrixEconomy>(Data, Lots);
	if (!BrixEconomyLoad(*NewE, *S.Econ, OutWhy)) return false;
	TUniquePtr<FBrixQuests> NewQ;
	if (Q)
	{
		NewQ = MakeUnique<FBrixQuests>(QuestRules, *NewE, QuestResidents);
		if (!BrixQuestsLoad(*NewQ, *S.Quests, OutWhy)) return false;
		NewQ->OnNote = [this](const FString& T) { Toast(T); };
	}
	TUniquePtr<FBrixEstate> NewM;
	if (Est)
	{
		NewM = MakeUnique<FBrixEstate>(EstateRules, *NewE, EstateUnits, EstateRoles);
		if (S.Estate) NewM->Load(S.Estate); else NewM->Start();   // a save from before the market: it opens now (life.js loadSave())
	}
	if (!Panel.IsEmpty()) Close();
	Q = MoveTemp(NewQ);   // the quests and the market hold the economy: the old ones go first
	Est = MoveTemp(NewM);
	for (const auto& R : RemodelsDone) if (const int32* n = E->LotIndex.Find(R.Key)) ApplyRemodel(*n, nullptr);   // the old game's remodels undone
	E = MoveTemp(NewE);
	RentEdit.Reset();
	RemodelsDone.Reset();
	if (S.Remodels)
		for (const auto& KV : S.Remodels->Values)
			if (KV.Value.IsValid() && KV.Value->Type == EJson::Object)
			{
				RemodelsDone.Add(FString(KV.Key), KV.Value->AsObject());
				if (const int32* n = E->LotIndex.Find(FString(KV.Key))) ApplyRemodel(*n, KV.Value->AsObject());
			}
	Feats = S.Feats;
	Offered.Reset();
	PendingCounter = -1;
	Coins.Reset(); Flies.Reset(); Spark.Reset(); Timeline.Reset(); Banners.Reset();
	SeenTx = E->L.Tx.Num();   // what was before the load is not shown again (rewards.js reset())
	Shown = HeldCash = HeldBank = 0;
	LastMoney = E->TotalMoney();
	return true;
}

void ABrixLife::ShowHud(bool bShow)
{
	if (Hud) Hud->SetVisibility(bShow ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
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
	+ SOverlay::Slot()[ SNew(SBrixCoins).Sprites(&Sprites).ViewportPx(&ViewportPx).GlowBoost(CoinGlow) ]
	// the slip from the bank under the purse while interest is paid in (index.html #slip)
	+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(22, 178, 0, 0))
	[
		SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(UiCream).Padding(FMargin(12, 6)).Visibility_Lambda([this]() { return SlipLeft > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Font(UiFont(16)).ColorAndOpacity(UiInk).Text(FText::FromString(TEXT("Interest paid"))) ]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Font(UiFont(13)).ColorAndOpacity(UiInk).Text_Lambda([this]() { return FText::FromString(TEXT("into your ") + SlipWhat); }) ]
		]
	]
	// an achievement's painted banner, dropping in at the top (index.html #feat; SND-021)
	+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(16, 18, 16, 0))
	[
		SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(FLinearColor::FromSRGBColor(FColor(201, 162, 74))).Padding(FMargin(22, 12))
		.Visibility_Lambda([this]() { return BannerLeft > -0.6 && !BannerName.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		.RenderTransform_Lambda([this]()
		{
			const double k = FMath::Min(1.0, BannerAge / .55), Out = BannerLeft > 0 ? 0.0 : FMath::Min(1.0, -BannerLeft / .5);
			const double In = 1 - FMath::Pow(1 - k, 3) * (1 - 2.2 * k);   // drops in with a little overshoot
			return FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(-1.5f)), FVector2D(0, (1 - In) * -140 - Out * 160));
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Font(UiFont(10, true)).ColorAndOpacity(UiInk).Text(FText::FromString(TEXT("ACHIEVEMENT"))) ]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Font(UiFont(22)).ColorAndOpacity(UiInk).Text_Lambda([this]() { return FText::FromString(BannerName); }) ]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Font(UiFont(12)).ColorAndOpacity(UiInk).Text_Lambda([this]() { return FText::FromString(BannerLine); }) ]
		]
	]
	+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(22, 150, 0, 0))
	[
		SNew(SHorizontalBox).Visibility_Lambda([this]() { return bMenu ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(STextBlock).Font(UiFont(16, true)).ColorAndOpacity(UiCream).ShadowOffset(FVector2D(0, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, .75f))
			.Text_Lambda([this]() { return FText::FromString(E ? Format(E->L.Balance(TEXT("p:cash")) - HeldCash) : FString()); }) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(STextBlock).Font(UiFont(16)).ColorAndOpacity(UiDim).ShadowOffset(FVector2D(0, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, .75f))
			.Text_Lambda([this]() { return FText::FromString(E && E->Accounts.Num() ? TEXT(" · bank ") + Format(E->L.Balance(TEXT("p:current")) + E->L.Balance(TEXT("p:savings")) - HeldBank) : FString()); }) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(10, 0, 0, 0)
		[ SNew(STextBlock).Font(UiFont(16, true)).ShadowOffset(FVector2D(0, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, .75f))
			.ColorAndOpacity_Lambda([this]() { FLinearColor C = bDeltaUp ? UiPos : UiNeg; C.A = FMath::Clamp(DeltaLeft / .6f, 0.f, 1.f); return FSlateColor(C); })
			.Text_Lambda([this]() { return FText::FromString(DeltaText); }) ]
	]
	+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0, 270, 18, 0))
	[
		SNew(SBox).WidthOverride(260.f).Visibility_Lambda([this]()
		{
			const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
			return !IsOpen() && !(Mode && Mode->Talk && Mode->Talk->IsTalking()) && Tracked().Num() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ TrackCard(0) ]
			+ SVerticalBox::Slot().AutoHeight()[ TrackCard(1) ]
			+ SVerticalBox::Slot().AutoHeight()[ TrackCard(2) ]
			+ SVerticalBox::Slot().AutoHeight()[ TrackCard(3) ]
		]
	]
	+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(16, 200, 16, 0))
	[
		SNew(SBox).MaxDesiredWidth(560.f).Visibility_Lambda([this]() { return ToastLeft > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SBorder).BorderImage(UiWhite()).Padding(FMargin(16, 8))
			.BorderBackgroundColor_Lambda([this]() { FLinearColor C = UiPanel78; C.A *= FMath::Clamp(ToastLeft / .5f, 0.f, 1.f); return FSlateColor(C); })
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
		.HAlign(HAlign_Center).VAlign(VAlign_Center)
		// the building designer's card stands to the left, the building in view beside it (BUILD-020)
		.Padding_Lambda([this]() { return Panel == TEXT("remodel") ? FMargin(16, 24, FMath::Max(16.f, (float)(ViewportPx.X / FMath::Max(PxScale, .1f)) - RemodelPanelPx - 32.f), 24) : FMargin(16, 40); })
		.Visibility_Lambda([this]() { return !Panel.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
		[
			SNew(SBox).WidthOverride_Lambda([this]() { return IsShop() ? (float)BuyMenu.WidePanelPx : Panel == TEXT("remodel") ? RemodelPanelPx : 620.f; })
			[
				BrixLook::Card(SAssignNew(PanelBody, SBox), FMargin(22, 18, 22, 14), BrixLook::Stone(), 18.f)
			]
		];
	GEngine->GameViewport->AddViewportWidgetContent(Hud.ToSharedRef(), 15);
	GEngine->GameViewport->AddViewportWidgetContent(PanelRoot.ToSharedRef(), 25);
}

// ---- panels (life.js open(), close(), act(), draw())

// what the offer over a counter says (life.js hud()): nothing for a mall, whose outlets each have their own
FString ABrixLife::CounterWords(int32 n) const
{
	if (!E) return FString();
	int32 On = -1;
	FString U;
	if (OutletHere(On, U)) { const FBrixOutlet* O = E->Outlet(On, U); return TEXT("Buy at ") + (O ? O->Name : U); }   // each outlet has its own counter (WLD-066)
	if (n < 0) return FString();
	if (n == E->BankLot) return TEXT("Bank counter");
	if (n == E->MarketLot) return TEXT("Your market stall");
	if (Est) if (const FString* Co = Est->Office.Find(Lots[n].Id)) return TEXT("Estate agent: ") + EstateRules.Company(*Co)->Name;
	if (!E->ShopKind(n).IsEmpty() && !E->Outlets.Contains(n)) return TEXT("Buy and sell at ") + Lots[n].Name;
	return FString();
}

void ABrixLife::CounterKey()
{
	if (Panel == TEXT("bank") || Panel == TEXT("shop") || Panel == TEXT("stall") || Panel == TEXT("outlet") || Panel == TEXT("agent")) { Close(); return; }
	int32 On = -1;
	FString U;
	if (OutletHere(On, U)) { PanelUnit = U; Open(TEXT("outlet"), On); return; }   // each outlet has its own counter (WLD-066, ECO-013)
	const int32 n = CounterHere();
	if (n >= 0 && E->Outlets.Contains(n)) { Toast(TEXT("Walk into one of ") + Lots[n].Name + TEXT("'s outlets to buy there: each has its own goods.")); return; }
	if (n < 0) { Toast(TEXT("No counter here. Shops, cafés, the bank and the market square have one.")); return; }
	if (n == E->BankLot) Open(TEXT("bank"), n);
	else if (Est && Est->Office.Contains(Lots[n].Id)) Open(TEXT("agent"), n);   // PROP-011 to PROP-019 (life.js: B at an estate agent)
	else if (n == E->MarketLot) Open(TEXT("stall"), n);
	else { Side = TEXT("buy"); Open(TEXT("shop"), n); }
}

void ABrixLife::QuestsKey() { Menu(TEXT("quests")); }   // J: the Journal page (life.js: I, J and the rest open the menu at their page)

void ABrixLife::WalletKey() { Menu(TEXT("wallet")); }

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
	if (bMenu) { CloseMenu(); return; }
	if (!IsOpen()) return;
	if (Panel == TEXT("remodel")) EndRemodel();
	Panel.Reset();
	LiveGrid.Reset();
	if (PanelBody) PanelBody->SetContent(SNullWidget::NullWidget);
	if (ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0))) Me->EndPanel();
}

void ABrixLife::Redraw() { if (bMenu && Panel.IsEmpty()) DrawMenu(); else Draw(); }   // the counter's panel, or the menu's page under it

void ABrixLife::Act(TFunction<bool(FString&)> Fn)
{
	FString Said;
	Msg = Fn(Said) ? Said : TEXT("Refused: ") + E->Why + TEXT(".");
	Redraw();
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
	else if (Panel == TEXT("outlet"))   // one outlet of a mall (life.js draw(), kind "outlet")
	{
		const FBrixOutlet* O = E->Outlet(PanelLot, PanelUnit);
		const ABrixGameMode* GM = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
		const FBrixMall* MD = GM && GM->Mall ? GM->Mall->Data() : nullptr;
		const FString* Label = MD && O ? MD->KindLabel.Find(O->Kind) : nullptr;
		Title = O ? O->Name : PanelUnit;
		Sub = (Label && !Label->IsEmpty() ? *Label : FString(TEXT("Outlet"))) + TEXT(" · ") + Lots[PanelLot].Name + (O && O->Level ? FString::Printf(TEXT(", floor %d"), O->Level) : FString(TEXT(", ground floor")))
			+ TEXT(" · restocks at ") + UiHhmm(E->R.RestockMinute);
		Hint = TEXT("← → ↑ ↓  choose    Enter  buy    Esc  close");
	}
	else if (Panel == TEXT("quests")) { Title = TEXT("Quests"); Sub = FString::Printf(TEXT("Day %d · J"), E->Day + 1); }
	else if (Panel == TEXT("remodel"))
	{
		bool bWhole = false; TArray<int32> Fl;
		Holding(PanelLot, bWhole, Fl);
		Title = Lots[PanelLot].Name;
		Sub = bWhole ? TEXT("Remodel · Esc to leave it as it was") : TEXT("Your rooms · Esc to leave them as they were");
	}
	else if (Panel == TEXT("agent") && Est) { Title = EstateRules.Company(Est->Office[Lots[PanelLot].Id])->Name; Sub = TEXT("Estate agents · buy, sell and rent anywhere in town"); }
	else if (Panel == TEXT("stall")) { Title = Lots[PanelLot].Name; Sub = TEXT("Your market stall · residents shop here ") + UiHhmm(E->R.StallOpen) + TEXT(" to ") + UiHhmm(E->R.StallClose); }
	else { Title = TEXT("Money"); Sub = PlayerName + TEXT(" · I"); }
	if (!Msg.IsEmpty())
		Body->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[ SNew(SBorder).BorderImage(BrixLook::Round(UiMsgBack, 10.f)).Padding(FMargin(12, 7))[ UiText(Msg, 15) ] ];
	if (Panel == TEXT("bank")) BankPanel(Body);
	else if (Panel == TEXT("shop")) ShopPanel(Body);
	else if (Panel == TEXT("outlet")) OutletPanel(Body);
	else if (Panel == TEXT("stall")) StallPanel(Body);
	else if (Panel == TEXT("quests")) QuestsPanel(Body);
	else if (Panel == TEXT("remodel")) RemodelPanel(Body);
	else if (Panel == TEXT("agent") && Est) EstatePanel(Body, Est->Office[Lots[PanelLot].Id], -1);
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

// ---- the real estate market (PROP-011 to PROP-019, PROP-023; life.js estatePanel(), unitRow(), buyUnits(), buildingsTab())

namespace
{
	FString TenantText(const FBrixEstate& M, int32 i)
	{
		const FString& t = M.Tenant[i];
		return t == TEXT("player") ? TEXT("you rent it") : t == TEXT("hh") ? TEXT("lived in") : t == TEXT("firm") ? TEXT("in business") : t == TEXT("lodger") ? TEXT("let") : TEXT("empty");
	}
	TSharedRef<SWidget> UiCell(TSharedRef<SWidget> W, float Width) { return SNew(SBox).WidthOverride(Width).VAlign(VAlign_Center)[ W ]; }
}

FString ABrixLife::BoardText(int32 Lot) const               // PROP-023 (life.js boardText())
{
	if (!Est || !Est->ByLot.Contains(Lot)) return FString();
	const TPair<int32, int32> B = Est->Board(Lot);
	const int32 k = Est->ByLot[Lot].Num();
	if (!B.Key && !B.Value) return FString();
	auto Part = [k](int32 c, const TCHAR* W) { return c ? (k > 1 ? FString::Printf(TEXT("%d %s"), c, W) : FString(W)) : FString(); };
	FString S = Part(B.Key, TEXT("for sale"));
	const FString L2 = Part(B.Value, TEXT("to let"));
	if (!L2.IsEmpty()) S += (S.IsEmpty() ? TEXT("") : TEXT(" · ")) + L2;
	return S.Left(1).ToUpper() + S.Mid(1);
}

void ABrixLife::ShowOnMap(int32 Lot)                         // UI-006: a waypoint at a building, and the big map
{
	if (!Lots.IsValidIndex(Lot)) return;
	ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode || !Mode->Map) return;
	Mode->Map->MarkAt(Lots[Lot].Centre(), Lots[Lot].Name);
	Close();
	Mode->Map->Open();
	Toast(TEXT("Marked on the map: ") + Lots[Lot].Name + TEXT(". Esc or M closes the map."));
}

FString ABrixLife::BuyUnits(const TArray<int32>& Units, const FString& Agent, bool& bOk)   // outright, or a mortgage at the least deposit
{
	const FBrixEstateRules& R = EstateRules;
	if (!PayTerm)
	{
		int64 Paid = 0;
		bOk = Est->Buy(Units, Agent, {}, {}, &Paid);
		return TEXT("Bought for ") + Format(Paid) + TEXT(" with the transfer tax.");
	}
	const int32 Term = R.MortgageTerms[FMath::Clamp(PayTerm - 1, 0, R.MortgageTerms.Num() - 1)];
	int64 Total = 0;
	for (const int32 i : Units) { const int64 a = Est->Ask(i).Get(0); Total += Units.Num() > 1 ? BrixMoney::FloorDiv(a * (10000 - R.WholeDiscountBp), 10000) : a; }
	const int64 Dep = (Total * R.DepositBp + 9999) / 10000;   // Math.ceil
	bOk = Est->Buy(Units, Agent, Dep, Term);
	if (!bOk) return FString();
	const FBrixMortgage& Mg = Est->Mortgages.Last();
	return FString::Printf(TEXT("Bought with a deposit of %s; the bank lends %s, repaid at about %s a day for %d days."), *Format(Dep), *Format(Mg.Principal), *Format(Mg.Plan[0]), Term);
}

TSharedRef<SWidget> ABrixLife::UnitRow(int32 i, const FString& Agent)
{
	const TOptional<int64> A = Est->Ask(i);
	TSharedRef<SHorizontalBox> R = UiRow();
	R->AddSlot().FillWidth(1).VAlign(VAlign_Center).Padding(0, 0, 8, 0)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ UiText(Est->Name(i), 14) ]
		+ SVerticalBox::Slot().AutoHeight()[ UiText(FString::Printf(TEXT("%lld m² · %s · %s"), Est->Units[i].Area, *Est->OwnerName(i), *TenantText(*Est, i)), 12, UiDim) ]
	];
	R->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[ UiCell(UiText(A ? Format(*A) : Est->IsMine(i) ? FString(TEXT("yours")) : FString(TEXT("not for sale")), 14, A ? UiGold : UiDim, A.IsSet()), 104.f) ];
	R->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[ UiCell(UiText(Format(Est->MarketRent(i)) + TEXT("/day"), 12, UiDim), 92.f) ];
	if (A) UiAdd(R, UiButton(TEXT("Buy"), [this, i, Agent]() { Act([this, i, Agent](FString& S) { bool bOk = false; S = BuyUnits({ i }, Agent, bOk); if (!bOk) E->Why = Est->Why; return bOk; }); }));
	if (Est->Tenant[i].IsEmpty() && !Est->IsMine(i))
		UiAdd(R, UiButton(TEXT("Rent"), [this, i, Agent]()
		{
			Act([this, i, Agent](FString& S)
			{
				if (!Est->RentUnit(i, Agent)) { E->Why = Est->Why; return false; }
				S = TEXT("You rent ") + Est->Name(i) + TEXT(" for ") + Format(Est->Tenancy[i].Rent) + TEXT(" a day, paid each midnight.");
				return true;
			});
		}, true, true));
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[ R ]
		+ SVerticalBox::Slot().AutoHeight()[ SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(UiRule).Padding(0)[ SNew(SBox).HeightOverride(1.f) ] ];
}

void ABrixLife::EstatePanel(TSharedRef<SVerticalBox> Body, const FString& Agent, int32 Here)
{
	if (!Est) return;
	const FBrixEstateRules& R = EstateRules;
	auto Slot = [&](TSharedRef<SWidget> W, float Top = 6) { Body->AddSlot().AutoHeight().Padding(0, Top, 0, 0)[ W ]; };
	const TArray<int64>* Hist = Est->History.Num() ? &Est->History.Last() : nullptr;
	Slot(PurseLine(), 0);
	Slot(UiText(FString::Printf(TEXT("Prices in town %.1f%% of usual · money in town %s"), (Hist ? (*Hist)[1] : Est->TownIndex()) / 100.0, *Format(E->L.Supply()))
		+ (Hist ? FString::Printf(TEXT(" · %lld sold yesterday"), (*Hist)[3]) : FString()), 13, UiDim));
	auto PayChoice = [this, &R]()   // outright, or a mortgage (BANK-002, PROP-019)
	{
		TSharedRef<SHorizontalBox> P = UiRow();
		UiAdd(P, UiText(TEXT("Pay"), 14));
		const FString Now = PayTerm ? FString::Printf(TEXT("mortgage, %d days"), R.MortgageTerms[FMath::Clamp(PayTerm - 1, 0, R.MortgageTerms.Num() - 1)]) : FString(TEXT("outright"));
		UiAdd(P, UiButton(Now, [this, N = R.MortgageTerms.Num() + 1]() { PayTerm = (PayTerm + 1) % N; Redraw(); }, true, true));
		return P;
	};
	if (Here >= 0 && Est->ByLot.Contains(Here))   // the building the player stands in
	{
		const TArray<int32>& Ix = Est->ByLot[Here];
		TArray<int32> Rest;
		for (const int32 i : Ix) if (!Est->IsMine(i)) Rest.Add(i);
		Slot(UiHeading(TEXT("This building: ") + Lots[Here].Name), 0);
		Slot(PayChoice());
		for (const int32 i : Ix) Slot(UnitRow(i, FString()), 2);
		bool bAll = Rest.Num() > 0;
		for (const int32 i : Rest) bAll &= Est->Ask(i).IsSet();
		if (Ix.Num() > 1 && bAll)
		{
			int64 Tot = 0;
			for (const int32 i : Rest) Tot += BrixMoney::FloorDiv(*Est->Ask(i) * (10000 - R.WholeDiscountBp), 10000);
			TSharedRef<SHorizontalBox> B = UiRow();
			UiAdd(B, UiButton(TEXT("Buy the whole building for ") + Format(Tot), [this, Rest]() { Act([this, Rest](FString& S) { bool bOk = false; S = BuyUnits(Rest, FString(), bOk); if (!bOk) E->Why = Est->Why; return bOk; }); }));
			Slot(B);
		}
	}
	if (!Agent.IsEmpty())
	{
		TArray<FString> Kinds = { TEXT("all") };
		for (const auto& K : R.Kinds) Kinds.Add(K.Key);
		auto Want = [this](int32 i) { return AgentFilter == TEXT("all") || Est->Units[i].Kind == AgentFilter; };
		TArray<int32> Listed;
		for (const auto& Ls : Est->Listing) if (!Est->IsMine(Ls.Key) && Want(Ls.Key)) Listed.Add(Ls.Key);
		Listed.Sort([this](int32 a, int32 b) { const int64 pa = Est->Listing[a].Price, pb = Est->Listing[b].Price; return pa != pb ? pa < pb : a < b; });
		if (Listed.Num() > 12) Listed.SetNum(12);
		Slot(UiHeading(TEXT("For sale")), 0);
		TSharedRef<SHorizontalBox> F = UiRow();
		UiAdd(F, UiText(TEXT("Show"), 14));
		UiAdd(F, UiButton(AgentFilter == TEXT("all") ? FString(TEXT("everything")) : R.Kinds[AgentFilter].Name.ToLower(), [this, Kinds]() { AgentFilter = Kinds[(Kinds.IndexOfByKey(AgentFilter) + 1) % Kinds.Num()]; Redraw(); }, true, true));
		Slot(F);
		Slot(PayChoice());
		if (Listed.Num()) for (const int32 i : Listed) Slot(UnitRow(i, Agent), 2);
		else Slot(UiText(TEXT("Nothing of that kind on the market today."), 14, UiDim));
		Slot(UiText(TEXT("Anything else in town can be bought too: stand in the building and press P to see what its owners would take."), 13, UiDim));
		TArray<int32> Free;
		for (int32 i = 0; i < Est->Units.Num(); ++i) if (Est->Tenant[i].IsEmpty() && !Est->IsMine(i) && Want(i)) Free.Add(i);
		Free.Sort([this](int32 a, int32 b) { const int64 ra = Est->MarketRent(a), rb = Est->MarketRent(b); return ra != rb ? ra < rb : a < b; });
		if (Free.Num() > 8) Free.SetNum(8);
		Slot(UiHeading(TEXT("To let")), 0);
		if (Free.Num()) for (const int32 i : Free) Slot(UnitRow(i, Agent), 2);
		else Slot(UiText(TEXT("Nothing empty just now."), 14, UiDim));
	}
	const TArray<int32> Mine = Est->Owned();
	if (Agent.IsEmpty())   // UI-006: each building you own once, with Show on map (life.js buildingsTab())
	{
		Slot(UiHeading(TEXT("Your buildings")), 0);
		TArray<int32> LotsOwned;
		for (const int32 i : Mine) LotsOwned.AddUnique(Est->Units[i].Lot);
		LotsOwned.Sort();
		if (!LotsOwned.Num()) Slot(UiText(TEXT("You own no building yet. Buy one: stand in it and press P, or see an estate agent."), 14, UiDim));
		for (const int32 n : LotsOwned)
		{
			TArray<int32> Floors;
			for (const int32 i : Est->ByLot[n]) if (Est->IsMine(i)) for (const int32 f : Est->Units[i].Floors) Floors.AddUnique(f);
			Floors.Sort();
			FString Fl;
			for (const int32 f : Floors) Fl += (Fl.IsEmpty() ? TEXT("") : TEXT(", ")) + (f ? FString::Printf(TEXT("floor %d"), f) : FString(TEXT("the ground floor")));
			TSharedRef<SHorizontalBox> B = UiRow();
			B->AddSlot().FillWidth(1).VAlign(VAlign_Center)[ SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[ UiText(Lots[n].Name, 15, UiCream, true) ]
				+ SVerticalBox::Slot().AutoHeight()[ UiText(Est->OwnsWhole(n) ? FString(TEXT("The whole building")) : TEXT("Your rooms on ") + Fl, 12, UiDim) ] ];
			UiAdd(B, UiButton(Est->OwnsWhole(n) ? TEXT("Remodel") : TEXT("Redecorate your rooms"), [this, n]() { OpenRemodel(n); }, DesignOf(n).IsValid()));
			UiAdd(B, UiButton(TEXT("Show on map"), [this, n]() { ShowOnMap(n); }, true, true));
			Slot(B, 4);
		}
	}
	Slot(UiHeading(TEXT("Your property")), 0);
	if (!Mine.Num()) Slot(UiText(TEXT("You own nothing yet."), 14, UiDim));
	for (const int32 i : Mine)
	{
		const FBrixListing* Ls = Est->Listing.Find(i);
		const int64* Ow = Est->Owed.Find(i);
		FString& Typed = RentEdit.FindOrAdd(i);
		if (Typed.IsEmpty()) Typed = FString::Printf(TEXT("%.2f"), Est->RentOf(i) / 100.0);
		Slot(UiText(Est->Name(i), 15, UiCream, true), 8);
		Slot(UiText(FString::Printf(TEXT("%lld m² · worth about %s · %s · upkeep and tax %s/day"), Est->Units[i].Area, *Format(Est->Value(i)), *TenantText(*Est, i), *Format(Est->CostOf(i)))
			+ (Ls ? TEXT(" · listed at ") + Format(Ls->Price) : FString()) + (Ow ? TEXT(" · ") + Format(*Ow) + TEXT(" unpaid!") : FString()), 12, Ow ? UiNeg : UiDim), 2);
		TSharedRef<SHorizontalBox> Rt = UiRow();
		UiAdd(Rt, UiText(TEXT("Rent Bx"), 14));
		UiAdd(Rt, SNew(SBox).WidthOverride(80.f)[ SNew(SEditableTextBox).Text(FText::FromString(Typed)).Font(UiFont(14)).OnTextChanged_Lambda([this, i](const FText& T) { RentEdit.Add(i, T.ToString()); }) ]);
		UiAdd(Rt, UiText(TEXT("a day (market ") + Format(Est->MarketRent(i)) + TEXT(")"), 12, UiDim));
		UiAdd(Rt, UiButton(TEXT("Set"), [this, i]()
		{
			Act([this, i](FString& S)
			{
				S = TEXT("Rent set; offered to tenants.");
				if (Est->SetRent(i, FMath::RoundToInt64(FCString::Atod(*RentEdit.FindRef(i)) * 100.0), true)) return true;
				E->Why = Est->Why;
				return false;
			});
		}, true, true));
		UiAdd(Rt, UiButton(Est->ToLet[i] ? TEXT("Stop letting") : TEXT("Offer to let"), [this, i]() { Est->ToLet[i] = !Est->ToLet[i]; Redraw(); }, true, true));
		Slot(Rt, 2);
		TSharedRef<SHorizontalBox> Rw = UiRow();
		UiAdd(Rw, UiButton(TEXT("Sell now for ") + Format(Est->Bid(i)), [this, i, Agent]()
		{
			Act([this, i, Agent](FString& S) { int64 P = 0; if (!Est->SellNow(i, Agent, &P)) { E->Why = Est->Why; return false; } RentEdit.Remove(i); S = TEXT("Sold for ") + Format(P) + TEXT("."); return true; });
		}, true, true));
		if (!Agent.IsEmpty())
		{
			if (Ls) UiAdd(Rw, UiButton(TEXT("Take it off the market"), [this, i]() { Act([this, i](FString& S) { S = TEXT("Taken off the market."); if (Est->Unlist(i)) return true; E->Why = Est->Why; return false; }); }, true, true));
			else UiAdd(Rw, UiButton(TEXT("List at ") + Format(Est->Value(i)), [this, i, Agent]()
			{
				Act([this, i, Agent](FString& S)
				{
					if (!Est->ListForSale(i, Est->Value(i), Agent)) { E->Why = Est->Why; return false; }
					S = TEXT("Listed with ") + EstateRules.Company(Agent)->Name + FString::Printf(TEXT("; the agent takes %s%% when it sells."), *FString::SanitizeFloat(EstateRules.AgentFeeBp / 100.0));
					return true;
				});
			}, true, true));
		}
		Slot(Rw, 4);
	}
	TArray<int32> Ten;
	Est->Tenancy.GetKeys(Ten);
	Ten.Sort();
	if (Ten.Num())
	{
		Slot(UiHeading(TEXT("Places you rent")), 0);
		for (const int32 i : Ten)
		{
			const FBrixTenancy& T = Est->Tenancy[i];
			TSharedRef<SHorizontalBox> B = UiRow();
			B->AddSlot().FillWidth(1).VAlign(VAlign_Center)[ UiText(Est->Name(i), 14) ];
			UiAdd(B, UiText(Format(T.Rent) + TEXT(" a day") + (T.Missed ? FString::Printf(TEXT(", %d unpaid"), T.Missed) : FString()), 13, T.Missed ? UiNeg : UiDim));
			UiAdd(B, UiButton(TEXT("Move out"), [this, i]() { Act([this, i](FString& S) { S = TEXT("You have moved out."); if (Est->Leave(i)) return true; E->Why = Est->Why; return false; }); }, true, true));
			Slot(B, 2);
		}
	}
	TArray<const FBrixMortgage*> Ms;
	for (const FBrixMortgage& Mg : Est->Mortgages) if (Mg.Status == TEXT("active")) Ms.Add(&Mg);
	if (Ms.Num())
	{
		Slot(UiHeading(TEXT("Mortgages")), 0);
		for (const FBrixMortgage* Mg : Ms)
		{
			TSharedRef<SHorizontalBox> B = UiRow();
			B->AddSlot().FillWidth(1).VAlign(VAlign_Center)[ UiText(FString::Printf(TEXT("Mortgage %d: %s owed, about %s a day"), Mg->Id, *Format(Mg->Owed()), *Format(Mg->Plan[0])) + (Mg->Missed ? FString::Printf(TEXT("; %d missed"), Mg->Missed) : FString()), 14) ];
			const int32 Id = Mg->Id;
			UiAdd(B, UiButton(TEXT("Repay now"), [this, Id]() { Act([this, Id](FString& S) { S = TEXT("Mortgage repaid."); if (Est->Repay(Id)) return true; E->Why = Est->Why; return false; }); }, true, true));
			Slot(B, 2);
		}
	}
	if (Est->Sales.Num())
	{
		Slot(UiHeading(TEXT("Lately sold")), 0);
		for (int32 k = Est->Sales.Num() - 1; k >= FMath::Max(0, Est->Sales.Num() - 5); --k)
		{
			const FBrixSale& Sl = Est->Sales[k];
			const FString& b = Sl.Buyer;
			const FBrixEstateRules::FCompany* Co = b.StartsWith(TEXT("co:")) ? EstateRules.Company(b.Mid(3)) : nullptr;
			const FString Who = b == TEXT("player") ? FString(TEXT("you")) : b == TEXT("hh") ? FString(TEXT("the household")) : b == TEXT("firm") ? FString(TEXT("the business")) : b == TEXT("city") ? FString(TEXT("the town")) : Co ? Co->Name : b;
			TSharedRef<SHorizontalBox> B = UiRow();
			B->AddSlot().FillWidth(1).VAlign(VAlign_Center)[ UiText(Est->Name(Sl.Unit), 13) ];
			UiAdd(B, UiText(Format(Sl.Price), 13, UiGold));
			UiAdd(B, UiText(FString::Printf(TEXT("day %d to %s"), Sl.Day + 1, *Who), 12, UiDim));
			Slot(B, 2);
		}
	}
}

// ---- the building designer: remodel (BUILD-004 to BUILD-007, BUILD-016 to BUILD-020; life.js remodelPanel())
// From Properties (Remodel, or Redecorate your rooms) at a building you own. Every choice comes from BrixBuilder::Options()
// (the kit and the district's palette); each is shown on the building at once (the town draws it again from the draft) with
// its price; Pay checks it as builder.py does and pays through the ledger. Outside needs the whole building; inside, the
// storeys of the units you own. A remodel paid for is there at once (F-BLD-CONSTRUCT level 0: instant) and kept in the save.

namespace
{
	FLinearColor Swatch(const FVector3f& C)   // life.js css(): the colour a little lighter, as the slice shows a swatch
	{
		return FLinearColor::FromSRGBColor(FColor((uint8)FMath::RoundToInt(FMath::Min(1.f, C.X * 1.25f) * 255), (uint8)FMath::RoundToInt(FMath::Min(1.f, C.Y * 1.25f) * 255), (uint8)FMath::RoundToInt(FMath::Min(1.f, C.Z * 1.25f) * 255)));
	}
	TSharedRef<SWidget> SwatchButton(const FBrixDesignOption& O, bool bOn, TFunction<void()> OnClick)
	{
		if (!O.Rgb) return UiButton(O.Name, OnClick, true, !bOn);   // a word (With shutters, Ashlar, ...)
		return SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).ToolTipText(FText::FromString(O.Name)).ContentPadding(0)
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(SBorder).BorderImage(BrixLook::Round(bOn ? UiLantern : UiRule, 7.f)).Padding(bOn ? 3.f : 2.f)
				[ SNew(SBorder).BorderImage(BrixLook::Round(Swatch(*O.Rgb), 5.f)).Padding(0)[ SNew(SBox).WidthOverride(26.f).HeightOverride(26.f) ] ]
			];
	}
	TSharedPtr<FJsonValue> JStr(const FString& S) { return MakeShared<FJsonValueString>(S); }
}

bool ABrixLife::Holding(int32 Lot, bool& bWhole, TArray<int32>& Floors) const
{
	bWhole = false;
	Floors.Reset();
	if (!Est || !Est->ByLot.Contains(Lot)) return false;
	const TArray<int32>& Ix = Est->ByLot[Lot];
	int32 Mine = 0;
	for (const int32 i : Ix) if (Est->IsMine(i)) { ++Mine; for (const int32 f : Est->Units[i].Floors) Floors.AddUnique(f); }
	Floors.Sort();
	bWhole = Mine == Ix.Num();
	return Mine > 0;
}

TSharedPtr<FJsonObject> ABrixLife::DesignOf(int32 Lot) const
{
	if (!Lots.IsValidIndex(Lot)) return nullptr;
	if (const TSharedPtr<FJsonObject>* R = RemodelsDone.Find(Lots[Lot].Id)) return *R;
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode || !Mode->InsidesData() || !Mode->InsidesData()->Lots.IsValidIndex(Lot) || !Mode->InsidesData()->Lots[Lot].IsSet()) return nullptr;
	return Mode->InsidesData()->Lots[Lot]->Design;
}

void ABrixLife::ApplyRemodel(int32 Lot, const TSharedPtr<FJsonObject>& Design)
{
	ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode) return;
	if (Mode->Town()) Mode->Town()->Repaint(Lot, Design);
	if (Mode->InsidesData() && Mode->InsidesData()->Lots.IsValidIndex(Lot) && Mode->InsidesData()->Lots[Lot].IsSet() && (Design || Mode->InsidesData()->Lots[Lot]->Design))   // the rooms' furnishing reads the finishes there
	{
		FBrixInside& In = Mode->InsidesData()->Lots[Lot].GetValue();
		const FBrixBuiltLook B = BrixBuilder::Built(Mode->InsidesData()->Builder, Design ? Design : In.Design);
		if (B.WallColour.Num() == In.WallColour.Num()) { In.WallColour = B.WallColour; In.FloorColour = B.FloorColour; In.FloorFinish = B.FloorFinish; }
	}
}

bool ABrixLife::OpenRemodel(int32 Lot)
{
	if (!E || !Est || !Lots.IsValidIndex(Lot)) return false;
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode || !Mode->InsidesData() || !Mode->InsidesData()->Builder.IsSet() || !DesignLots.IsValidIndex(Lot)) { Toast(TEXT("The building designer's rules are not in this game's data.")); return false; }
	bool bWhole = false;
	TArray<int32> Floors;
	if (!Holding(Lot, bWhole, Floors)) { Toast(TEXT("You own nothing in ") + Lots[Lot].Name + TEXT(".")); return false; }
	const TSharedPtr<FJsonObject> D = DesignOf(Lot);
	if (!D) { Toast(Lots[Lot].Name + TEXT(" has no design yet.")); return false; }
	if (bMenu) CloseMenu();
	DraftBase = D;
	Draft = D;
	DraftUndo.Reset();
	DesignTab = bWhole ? TEXT("outside") : TEXT("inside");
	DesignFacade = TEXT("*");
	DesignStorey = Floors.Num() ? Floors[0] : 0;
	ViewTurn = 0;
	Open(TEXT("remodel"), Lot);
	DesignView();
	return Panel == TEXT("remodel");
}

void ABrixLife::RemodelSet(const FBrixDesignChanges& Changes)
{
	DraftUndo.Add(Draft);
	Draft = BrixBuilder::Remodel(Draft, Changes);
	ApplyRemodel(PanelLot, Draft);
	Msg.Reset();
	Draw();
}

void ABrixLife::EndRemodel()
{
	if (PanelLot >= 0 && Draft != DraftBase) ApplyRemodel(PanelLot, RemodelsDone.Contains(Lots[PanelLot].Id) ? DraftBase : TSharedPtr<FJsonObject>());   // not paid for: as it was
	Draft.Reset();
	DraftBase.Reset();
	DraftUndo.Reset();
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC && PC->GetPawn()) PC->SetViewTargetWithBlend(PC->GetPawn(), 0.5f);
	if (ViewCam) { ViewCam->Destroy(); ViewCam = nullptr; }
}

void ABrixLife::DesignView()   // BUILD-020 (life.js startView(), frame3()): over the building, three-quarters from its front
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !Draft || !Lots.IsValidIndex(PanelLot)) return;
	const FBrixEconLot& L = Lots[PanelLot];
	const TSharedPtr<FJsonObject> Sh = Draft->GetObjectField(TEXT("shell"));
	FString Front;
	Sh->TryGetStringField(TEXT("side"), Front);
	const double Hgt = Sh->GetNumberField(TEXT("height"));
	const FVector2D Out = Front == TEXT("n") ? FVector2D(0, -1) : Front == TEXT("s") ? FVector2D(0, 1) : Front == TEXT("w") ? FVector2D(-1, 0) : FVector2D(1, 0);
	const double A = FMath::Atan2(Out.Y, Out.X) + FMath::DegreesToRadians(DesignTurnDeg + 45.0 * ViewTurn);
	const FVector2D Size = L.Max - L.Min;
	const bool bInside = DesignTab == TEXT("inside");
	const double Look = bInside ? 3.0 * DesignStorey + 1.0 : Hgt * 0.5;
	const double Dist = (FMath::Max(Size.X, Size.Y) * (bInside ? 1.1 : 1.35) + (bInside ? 2.0 : Hgt)) * 1.15;
	const double P = FMath::DegreesToRadians(bInside ? DesignPitchDeg + 20.0 : (double)DesignPitchDeg);
	const FVector Target(L.Centre().X * CmPerM, L.Centre().Y * CmPerM, (Look + 0.3) * CmPerM);
	const FVector Eye = Target + FVector(FMath::Cos(A) * FMath::Cos(P), FMath::Sin(A) * FMath::Cos(P), FMath::Sin(P)) * Dist * CmPerM;
	if (!ViewCam) ViewCam = GetWorld()->SpawnActor<ACameraActor>();
	if (!ViewCam) return;
	const FRotator Rot = (Target - Eye).Rotation();
	const FVector Left = -FRotationMatrix(Rot).GetUnitAxis(EAxis::Y) * Dist * CmPerM * 0.22;   // the card is on the left: the building to the right of the middle
	ViewCam->SetActorLocationAndRotation(Eye + Left, Rot);
	if (PC->GetViewTarget() != ViewCam) PC->SetViewTargetWithBlend(ViewCam, 0.6f);
}

bool ABrixLife::RemodelPay(FString& Said)
{
	const FBrixDesignLot& L = DesignLots[PanelLot];
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const FBrixBuilderRules& R = Mode->InsidesData()->Builder;
	bool bWhole = false;
	TArray<int32> Floors;
	Holding(PanelLot, bWhole, Floors);
	TArray<FBrixCostLine> Lines;
	const int64 Total = BrixBuilder::Cost(R, L, DraftBase, Draft, Lines);
	const TArray<FString> Probs = BrixBuilder::Check(R, L, DraftBase, Draft, bWhole, Floors, E->Funds());   // as the server will (BUILD-006)
	if (Probs.Num()) { E->Why = Probs[0]; return false; }
	if (Total > 0 && !E->Spend(Total, TEXT("sys:builders"), TEXT("Remodel: ") + Lots[PanelLot].Name)) return false;
	RemodelsDone.Add(Lots[PanelLot].Id, Draft);
	DraftBase = Draft;
	DraftUndo.Reset();
	ApplyRemodel(PanelLot, Draft);
	Said = TEXT("Done: ") + Lots[PanelLot].Name + (Total ? TEXT(", for ") + Format(Total) : FString()) + TEXT(".");
	return true;
}

void ABrixLife::RemodelPanel(TSharedRef<SVerticalBox> Body)
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Draft || !Mode || !Mode->InsidesData() || !DesignLots.IsValidIndex(PanelLot)) return;
	const FBrixBuilderRules& R = Mode->InsidesData()->Builder;
	const FBrixDesignLot& L = DesignLots[PanelLot];
	bool bWhole = false;
	TArray<int32> Mine;
	Holding(PanelLot, bWhole, Mine);
	if (bWhole) { Mine.Reset(); for (int32 f = 0; f < L.Floors; ++f) Mine.Add(f); }
	auto Slot = [&](TSharedRef<SWidget> W, float Top = 6) { Body->AddSlot().AutoHeight().Padding(0, Top, 0, 0)[ W ]; };
	const TArray<TPair<FString, TArray<FBrixDesignOption>>> Menu0 = BrixBuilder::Options(R, L, DraftBase);
	auto MenuOf = [&](const FString& Part) -> const TArray<FBrixDesignOption>* { for (const auto& P : Menu0) if (P.Key == Part) return &P.Value; return nullptr; };
	auto Row = [&](const FString& Label, const TArray<FBrixDesignOption>& Opts, const TSharedPtr<FJsonValue>& Cur, TFunction<FBrixDesignChanges(const FBrixDesignOption&)> Path)
	{
		TSharedRef<SWrapBox> W = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4, 4));
		for (const FBrixDesignOption& O : Opts)
		{
			const FBrixDesignChanges Ch = Path(O);
			W->AddSlot()[ SwatchButton(O, BrixBuilder::Same(O.Id, Cur), [this, Ch]() { RemodelSet(Ch); }) ];
		}
		Slot(SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0, 6, 10, 0)[ SNew(SBox).WidthOverride(118.f)[ UiText(Label, 13, UiDim) ] ]
			+ SHorizontalBox::Slot().FillWidth(1)[ W ], 4);
	};
	auto At = [](const FString& Path) { return [Path](const FBrixDesignOption& O) { return FBrixDesignChanges{ { Path, O.Id } }; }; };
	Slot(PurseLine(), 0);
	TSharedRef<SHorizontalBox> Tabs = UiRow();
	UiAdd(Tabs, UiButton(TEXT("Outside"), [this]() { DesignTab = TEXT("outside"); DesignView(); Draw(); }, bWhole, DesignTab != TEXT("outside")));
	UiAdd(Tabs, UiButton(TEXT("Inside"), [this]() { DesignTab = TEXT("inside"); DesignView(); Draw(); }, true, DesignTab != TEXT("inside")));
	UiAdd(Tabs, UiButton(TEXT("Turn the view"), [this]() { ViewTurn = (ViewTurn + 1) % 8; DesignView(); }, true, true));
	Slot(Tabs);
	if (!bWhole) Slot(UiText(TEXT("You own part of this building: the outside needs all of it."), 13, UiDim));
	const TSharedPtr<FJsonObject> Sh = Draft->GetObjectField(TEXT("shell")), Op = Draft->GetObjectField(TEXT("openings"));
	FString Front;
	Sh->TryGetStringField(TEXT("side"), Front);
	if (DesignTab == TEXT("outside"))
	{
		TSharedRef<SWrapBox> Fs = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6, 4));
		Fs->AddSlot().VAlign(VAlign_Center)[ UiText(TEXT("Walls"), 13, UiDim) ];
		for (const FString F : { FString(TEXT("*")), FString(TEXT("n")), FString(TEXT("s")), FString(TEXT("e")), FString(TEXT("w")) })
		{
			FString Name = F == TEXT("*") ? FString(TEXT("All sides")) : BrixBuilder::FacadeName(F);
			if (F != TEXT("*")) Name = Name.Left(1).ToUpper() + Name.Mid(1) + (F == Front ? TEXT(" (front)") : TEXT(""));
			Fs->AddSlot()[ UiButton(Name, [this, F]() { DesignFacade = F; Draw(); }, true, DesignFacade != F) ];
		}
		Slot(Fs);
		const FString Fc = DesignFacade == TEXT("*") ? Front : DesignFacade, Wp = TEXT("walls.") + DesignFacade;
		const TSharedPtr<FJsonObject> Fa = Draft->GetObjectField(TEXT("walls"))->GetObjectField(Fc);
		if (const auto* Pl = MenuOf(TEXT("walls.*.plaster"))) Row(TEXT("Plaster"), *Pl, Fa->TryGetField(TEXT("plaster")), At(Wp + TEXT(".plaster")));
		const TSharedPtr<FJsonObject> St = Fa->GetObjectField(TEXT("stone"));
		if (const auto* Ss = MenuOf(TEXT("walls.*.stone")))
		{
			Row(TEXT("Stone or brick"), *Ss, St->TryGetField(TEXT("pattern")), [Wp](const FBrixDesignOption& O)
			{
				TSharedPtr<FJsonValue> First = MakeShared<FJsonValueNull>();
				for (const auto& C : O.Colors) if (C.IsValid() && !C->IsNull()) { First = C; break; }
				TSharedPtr<FJsonObject> S = MakeShared<FJsonObject>();
				S->SetField(TEXT("pattern"), O.Id);
				S->SetField(TEXT("color"), First);
				FBrixDesignChanges Ch = { { Wp + TEXT(".stone"), MakeShared<FJsonValueObject>(S) } };
				if (!O.bBrickToEaves) Ch.Add({ Wp + TEXT(".brick_to_eaves"), MakeShared<FJsonValueBoolean>(false) });
				return Ch;
			});
			FString Pat;
			St->TryGetStringField(TEXT("pattern"), Pat);
			if (const FBrixDesignOption* So = Ss->FindByPredicate([&](const FBrixDesignOption& O) { return O.Id->Type == EJson::String && O.Id->AsString() == Pat; }))
			{
				if (So->Colors.Num() > 1)
				{
					TArray<FBrixDesignOption> Cs;
					const TArray<TSharedPtr<FJsonValue>> PatCols = R.Patterns->GetArrayField(Pat);
					for (int32 k = 0; k < So->Colors.Num(); ++k)
					{
						FBrixDesignOption C;
						C.Id = So->Colors[k];
						C.Name = So->Colors[k]->IsNull() ? FString(TEXT("The town's stone")) : FString::Printf(TEXT("%s %d"), *So->Name, k + 1);
						C.Rgb = So->Colors[k]->IsNull() || !PatCols.IsValidIndex(k) || PatCols[k]->Type != EJson::Array ? FVector3f(.55f, .52f, .47f)
							: FVector3f(PatCols[k]->AsArray()[0]->AsNumber(), PatCols[k]->AsArray()[1]->AsNumber(), PatCols[k]->AsArray()[2]->AsNumber());
						Cs.Add(C);
					}
					Row(TEXT("Its colour"), Cs, St->TryGetField(TEXT("color")), At(Wp + TEXT(".stone.color")));
				}
				if (So->bBrickToEaves)
				{
					FBrixDesignOption Y, N;
					Y.Id = MakeShared<FJsonValueBoolean>(true); Y.Name = TEXT("Yes");
					N.Id = MakeShared<FJsonValueBoolean>(false); N.Name = TEXT("No");
					Row(TEXT("Brick to the eaves"), { Y, N }, Fa->TryGetField(TEXT("brick_to_eaves")), At(Wp + TEXT(".brick_to_eaves")));
				}
			}
		}
		if (const auto* Rt = MenuOf(TEXT("roof.tiles"))) Row(TEXT("Roof tiles"), *Rt, Draft->GetObjectField(TEXT("roof"))->TryGetField(TEXT("tiles")), At(TEXT("roof.tiles")));
		if (const auto* Fr = MenuOf(TEXT("openings.frame"))) Row(TEXT("Window frames"), *Fr, Op->TryGetField(TEXT("frame")), At(TEXT("openings.frame")));
		if (const auto* Su = MenuOf(TEXT("openings.shutters")))
		{
			Row(TEXT("Shutters"), *Su, Op->TryGetField(TEXT("shutters")), At(TEXT("openings.shutters")));
			bool bSh = false;
			Op->TryGetBoolField(TEXT("shutters"), bSh);
			if (const auto* Sc = MenuOf(TEXT("openings.shutter_color")); Sc && bSh) Row(TEXT("Shutter colour"), *Sc, Op->TryGetField(TEXT("shutter_color")), At(TEXT("openings.shutter_color")));
		}
		const TPair<const TCHAR*, const TCHAR*> Extra[4] = { { TEXT("openings.balcony"), TEXT("Balcony") }, { TEXT("openings.keystones"), TEXT("Keystones") }, { TEXT("ornament.quoins"), TEXT("Corner stones") }, { TEXT("ornament.lantern"), TEXT("Wall lantern") } };
		for (const auto& X : Extra) if (const auto* Ox = MenuOf(X.Key)) Row(X.Value, *Ox, BrixBuilder::Get(Draft, X.Key), At(X.Key));
	}
	else
	{
		TSharedRef<SHorizontalBox> Ss = UiRow();
		UiAdd(Ss, UiText(TEXT("Storey"), 13, UiDim));
		for (const int32 f : Mine) UiAdd(Ss, UiButton(f == 0 ? FString(TEXT("Ground")) : FString::Printf(TEXT("Floor %d"), f), [this, f]() { DesignStorey = f; DesignView(); Draw(); }, true, DesignStorey != f));
		Slot(Ss);
		if (!Mine.Contains(DesignStorey) && Mine.Num()) DesignStorey = Mine[0];
		const int32 f = DesignStorey;
		const TSharedPtr<FJsonObject> In = Draft->GetObjectField(TEXT("inside"));
		auto Put = [this, f](const FString& Key) { return [this, f, Key](const FBrixDesignOption& O)
		{
			TArray<TSharedPtr<FJsonValue>> A = Draft->GetObjectField(TEXT("inside"))->GetArrayField(Key);
			if (A.IsValidIndex(f)) A[f] = O.Id;
			return FBrixDesignChanges{ { TEXT("inside.") + Key, MakeShared<FJsonValueArray>(A) } };
		}; };
		const TArray<TSharedPtr<FJsonValue>> Ws = In->GetArrayField(TEXT("walls")), Fl = In->GetArrayField(TEXT("floors"));
		if (const auto* Iw = MenuOf(TEXT("inside.walls")))
		{
			TArray<FBrixDesignOption> Mixed = *Iw;   // shown as they look inside (the kit's inner_mix)
			const TSharedPtr<FJsonObject> Mix = R.Kit->GetObjectField(TEXT("inner_mix"));
			const TArray<TSharedPtr<FJsonValue>>& With = Mix->GetArrayField(TEXT("with"));
			const float Share = (float)Mix->GetNumberField(TEXT("share"));
			for (FBrixDesignOption& O : Mixed) if (O.Rgb) O.Rgb = *O.Rgb + (FVector3f(With[0]->AsNumber(), With[1]->AsNumber(), With[2]->AsNumber()) - *O.Rgb) * Share;
			Row(TEXT("Walls"), Mixed, Ws.IsValidIndex(f) ? Ws[f] : nullptr, Put(TEXT("walls")));
		}
		if (const auto* Ifl = MenuOf(TEXT("inside.floors"))) Row(TEXT("Floor"), *Ifl, Fl.IsValidIndex(f) ? Fl[f] : nullptr, Put(TEXT("floors")));
		Slot(UiText(TEXT("The room plan and the furniture of a storey are not in this build's designer yet."), 12, UiDim));
	}
	// the work, its price line by line, and what would stop it (BUILD-005, BUILD-006)
	TArray<FBrixCostLine> Lines;
	const int64 Total = BrixBuilder::Cost(R, L, DraftBase, Draft, Lines);
	TArray<FString> Probs = Draft == DraftBase ? TArray<FString>() : BrixBuilder::Check(R, L, DraftBase, Draft, bWhole, Mine, {});
	if (Lines.Num() && Total > E->Funds()) Probs.Add(TEXT("it costs ") + Format(Total) + TEXT(" and you have ") + Format(E->Funds()));
	Slot(UiHeading(TEXT("The work")), 0);
	if (!Lines.Num()) Slot(UiText(TEXT("Pick something to change: it shows on the building at once. Nothing is paid until you build."), 13, UiDim));
	for (const FBrixCostLine& Ln : Lines)
	{
		TSharedRef<SHorizontalBox> B = UiRow();
		B->AddSlot().FillWidth(1).VAlign(VAlign_Center)[ UiText(Ln.What, 14) ];
		const bool bCount = Ln.Qty == FMath::FloorToDouble(Ln.Qty) && !Ln.What.Contains(TEXT("wall"));
		UiAdd(B, UiText(Ln.Qty == 1 ? FString() : bCount ? FString::Printf(TEXT("× %d"), (int32)Ln.Qty) : FString::Printf(TEXT("%s m²"), *FString::SanitizeFloat(Ln.Qty)), 12, UiDim));
		UiAdd(B, UiText(Format(Ln.Chips), 14, UiGold));
		Slot(B, 2);
	}
	if (Lines.Num()) { TSharedRef<SHorizontalBox> B = UiRow(); B->AddSlot().FillWidth(1)[ UiText(TEXT("Total"), 14, UiCream, true) ]; UiAdd(B, UiText(Format(Total), 15, UiGold, true)); Slot(B, 4); }
	if (Probs.Num())
	{
		FString P;
		for (const FString& X : Probs) P += (P.IsEmpty() ? TEXT("") : TEXT(". ")) + X.Left(1).ToUpper() + X.Mid(1);
		Slot(UiText(P + TEXT("."), 13, UiNeg));
	}
	TSharedRef<SHorizontalBox> Bs = UiRow();
	UiAdd(Bs, UiButton(Lines.Num() ? (Total ? TEXT("Pay ") + Format(Total) + TEXT(" and build") : FString(TEXT("Keep it like this"))) : FString(TEXT("Pay and build")),
		[this]() { Act([this](FString& S) { return RemodelPay(S); }); }, Lines.Num() > 0 && !Probs.Num()));
	UiAdd(Bs, UiButton(TEXT("Undo"), [this]() { if (DraftUndo.Num()) { Draft = DraftUndo.Pop(); ApplyRemodel(PanelLot, Draft); Draw(); } }, DraftUndo.Num() > 0, true));
	UiAdd(Bs, UiButton(TEXT("Start again"), [this]() { DraftUndo.Add(Draft); Draft = DraftBase; ApplyRemodel(PanelLot, Draft); Draw(); }, Draft != DraftBase, true));
	Slot(Bs, 10);
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
		else if (A[0] == TEXT("act") && Q)   // act,<quest>,<resident id>,<act>|<act>...: what an answer in a talk does
		{
			TArray<FString> Acts;
			Sv(3).ParseIntoArray(Acts, TEXT("|"));
			bOk = Q->Act(Sv(1), Acts, Sv(2));
		}
		else if (A[0] == TEXT("check") && Q) Q->Check(Ctx());
		else if (A[0] == TEXT("buy") || A[0] == TEXT("sell"))
		{
			int32 n = E->LotIndex.FindRef(Sv(1), -1);
			for (int32 k = 0; n < 0 && k < Lots.Num(); ++k) if (E->ShopKind(k) == Sv(1) && !E->Outlets.Contains(k)) n = k;
			bOk = n >= 0 && (A[0] == TEXT("buy") ? E->Buy(n, Sv(2)) : E->Sell(n, Sv(2)));
		}
		else if (A[0] == TEXT("setout")) bOk = E->SetOut(Sv(1), I(2));
		// the property market (estate.run_script()'s ops): give,<chips> into the current account (a test shot's money);
		// buy_cheapest,<kind>,<agent>[,<deposit %>,<term>]; rent_cheapest,<kind>; buy_lot,<lot id> (every unit not yours)
		else if (A[0] == TEXT("days"))   // days,<n>: the clock moved on that many days, the bank and the market settled for each (a test shot's)
		{
			ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
			bOk = Mode != nullptr;
			if (Mode)
			{
				Mode->SetClock(Mode->ClockMinutes() + I(1) * 1440.0);
				double D1 = 0, M1 = 0;
				Frame(D1, M1);
				E->Advance((int32)D1, M1);
			}
		}
		// the designer: remodel,<lot id> opens it; set,<path>=<value> (a kit id, true, false, a number or JSON) tries a change; pay builds it
		else if (A[0] == TEXT("remodel")) bOk = OpenRemodel(E->LotIndex.FindRef(Sv(1), -1));
		else if (A[0] == TEXT("set") && Panel == TEXT("remodel"))
		{
			FString Path, Val;
			bOk = Sv(1).Split(TEXT("="), &Path, &Val);
			TSharedPtr<FJsonValue> V;
			const TSharedRef<TJsonReader<>> Rd = TJsonReaderFactory<>::Create(Val);
			if (!FJsonSerializer::Deserialize(Rd, V) || !V.IsValid()) V = MakeShared<FJsonValueString>(Val);
			if (bOk) RemodelSet({ { Path, V } });
		}
		else if (A[0] == TEXT("pay") && Panel == TEXT("remodel")) { FString Said; bOk = RemodelPay(Said); Msg = bOk ? Said : TEXT("Refused: ") + E->Why + TEXT("."); Draw(); }
		else if (A[0] == TEXT("give")) bOk = E->HasAccount(TEXT("current")) && E->L.Post(E->Day, E->Minute, TEXT("Test money"), { { TEXT("sys:start"), -I(1) }, { TEXT("p:current"), I(1) } }, E->Why);
		else if (Est && (A[0] == TEXT("buy_cheapest") || A[0] == TEXT("rent_cheapest")))
		{
			int32 Best = -1;
			const bool bBuy = A[0] == TEXT("buy_cheapest");
			for (int32 j = 0; j < Est->Units.Num(); ++j)
			{
				if (Est->Units[j].Kind != Sv(1)) continue;
				if (bBuy ? !Est->Ask(j).IsSet() : (!Est->Tenant[j].IsEmpty() || Est->IsMine(j))) continue;
				const int64 K = bBuy ? *Est->Ask(j) : Est->MarketRent(j);
				if (Best < 0 || K < (bBuy ? *Est->Ask(Best) : Est->MarketRent(Best))) Best = j;
			}
			if (Best < 0) bOk = false;
			else if (!bBuy) bOk = Est->RentUnit(Best);
			else if (I(3)) bOk = Est->Buy({ Best }, Sv(2), BrixMoney::FloorDiv(*Est->Ask(Best) * I(3), 100), (int32)I(4));
			else bOk = Est->Buy({ Best }, Sv(2));
			if (!bOk) E->Why = Est->Why;
		}
		else if (Est && A[0] == TEXT("buy_lot"))
		{
			const int32 n = E->LotIndex.FindRef(Sv(1), -1);
			TArray<int32> Rest;
			if (const TArray<int32>* Ix = Est->ByLot.Find(n)) for (const int32 i : *Ix) if (!Est->IsMine(i)) Rest.Add(i);
			bOk = Rest.Num() && Est->Buy(Rest);
			if (!bOk) E->Why = Est->Why;
		}
		else if (A[0] == TEXT("outlet"))   // outlet,<good>: bought at the first of a mall's outlets that sells it (ECO-013; to wear, CHR-016)
		{
			bOk = false;
			for (const auto& M : E->Outlets)
			{
				for (const FBrixOutlet& U : M.Value) if (E->OutletSells(M.Key, U.Id).Contains(Sv(1))) { bOk = E->BuyAt(M.Key, U.Id, Sv(1)); break; }
				if (bOk) break;
			}
		}
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
	const ABrixGameMode* GM = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (GM && GM->Mall && GM->Mall->Data())   // an outlet of Eastgate, by its unit's id: stood a way into its room, facing its back wall
		for (const FBrixOutletRoom& R : GM->Mall->Data()->Rooms)
			if (R.Id == What)
			{
				APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0);
				if (!Me) return false;
				const double a = (R.A0 + R.A1) / 2, v = FMath::Min(R.D * .5, 1.6);
				const FVector2D At(R.X + R.T.X * a - R.N.X * v, R.Z + R.T.Y * a - R.N.Y * v);
				Me->SetActorLocation(FVector(At.X * CmPerM, At.Y * CmPerM, (R.Fy + 1.0) * CmPerM), false, nullptr, ETeleportType::TeleportPhysics);
				if (APlayerController* PC = Cast<APlayerController>(Me->GetController())) PC->SetControlRotation(FRotator(-10.0, FMath::RadiansToDegrees(FMath::Atan2(-R.N.Y, -R.N.X)), 0));
				CounterKey();
				return IsOpen();
			}
	int32 n = What == TEXT("bank") ? E->BankLot : What == TEXT("market") ? E->MarketLot : E->LotIndex.FindRef(What, -1);
	if (n < 0 && Est && EstateRules.Company(What)) n = E->LotIndex.FindRef(EstateRules.Company(What)->Office, -1);   // an estate agent by its id (lark, harrow, cobble)
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

// The outlet of a mall the player stands in, on whichever floor (life.js outletHere(); outlets.js at(), WLD-066)
bool ABrixLife::OutletHere(int32& OutLot, FString& OutUnit) const
{
	if (!E) return false;
	const ABrixGameMode* GM = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!GM || !GM->Mall || !GM->Mall->Data() || !Me) return false;
	const FVector Feet = Me->GetActorLocation() - FVector(0, 0, 90.f);
	const FBrixOutletRoom* R = GM->Mall->OutletAt(Feet);
	const int32 n = E->LotIndex.FindRef(GM->Mall->Data()->LotId, -1);
	if (!R || n < 0 || !E->Outlet(n, R->Id)) return false;
	OutLot = n;
	OutUnit = R->Id;
	return true;
}

// ECO-013, ECO-014: one outlet of a mall, its own goods, stock and prices (life.js draw(), kind "outlet"; buyAt())
void ABrixLife::OutletPanel(TSharedRef<SVerticalBox> Body)
{
	const int32 n = PanelLot;
	const FString U = PanelUnit;
	const FBrixOutlet* O = E->Outlet(n, U);
	if (!O) return;
	const ABrixGameMode* GM = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const FBrixMall* MD = GM && GM->Mall ? GM->Mall->Data() : nullptr;
	const FString Sign = MD ? MD->KindSign.FindRef(O->Kind) : FString();
	const FString Band = BuyMenu.Colours.Contains(Sign) ? Sign : BuyMenu.Bands.FindRef(TEXT("mall"));
	Body->AddSlot().AutoHeight()[ PurseLine() ];
	Body->AddSlot().AutoHeight()[ UiHeading(TEXT("Buy")) ];
	TArray<FWare> Things;
	for (const FString& G : E->OutletSells(n, U)) { const FBrixEconomyRules::FGood Gd = E->Good(G); Things.Add(Ware(G, Gd.Name, E->OutletPrice(n, U, G), Gd.Goes, E->OutletStock(n, U, G), Band)); }
	FGridOpts Op;
	Op.Id = FString::Printf(TEXT("outlet:%d:%s"), n, *U);
	Op.StockLabel = TEXT("In stock");
	Op.Buy = [this, n, U](const FWare& W, FString& Said)
	{
		int64 P = 0;
		if (!E->BuyAt(n, U, W.Id, &P)) return false;
		const FBrixEconomyRules::FGood Gd = E->Good(W.Id);
		Said = TEXT("Bought ") + Gd.Name.ToLower() + TEXT(" for ") + Format(P) + (Gd.Goes == TEXT("home") ? TEXT(". It is with your belongings (G), to place at home.") : Gd.Goes == TEXT("wear") ? TEXT(". It is in your wardrobe (U).") : TEXT("."));
		return true;
	};
	Body->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[ Grid(Things, Op) ];
	Body->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[ UiText(TEXT("Things for the home go to your belongings (G): place them from the Furnish tab of the building designer, in any home of yours. Clothes go to your wardrobe (U). Outlets do not buy things back; what you carry can be sold on your market stall."), 15, UiDim) ];
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
		return SNew(SBorder).BorderImage(BrixLook::Round(FLinearColor::FromSRGBColor(C.Key), 9.f)).Padding(FMargin(6, 2, 9, 3))   // .bm-chip: the mark, then the words
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 5, 0)[ UiMark(BuyMenu, Goes, 13.f, FLinearColor::FromSRGBColor(C.Value)) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(STextBlock).Font(UiFont(10, true)).ColorAndOpacity(FLinearColor::FromSRGBColor(C.Value)).Text(FText::FromString(G->Name)) ]
			];
	};
	auto Price = [](const FWare& W) { return BrixLook::Tag(Format(W.Price), W.State == TEXT("short"), 12); };
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
						SNew(SBorder).BorderImage(BrixLook::Round(bSel ? UiLantern : BrixLook::StoneDeep(), 12.f)).Padding(bSel ? 3 : 2)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[ SNew(SBrixThing).Mesh(MeshFor(W.Id, W.Band)).Rules(&BuyMenu).Grey(bOut).Size(FVector2D(CellW - 4, BuyMenu.StagePx)).Yaw(YawFor(O.Id + TEXT(":") + W.Id)) ]
							+ SOverlay::Slot().VAlign(VAlign_Center)[ Flag(bOut) ]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(2, 5, 2, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).HAlign(HAlign_Left)[ Price(W) ]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[   // .bm-dot: a round of the board colour with the mark in its lettering
							W.Goes.IsEmpty() ? SNullWidget::NullWidget : (TSharedRef<SWidget>)(SNew(SBox).WidthOverride(18).HeightOverride(18)
							[
								SNew(SBorder).BorderImage(BrixLook::Round(FLinearColor::FromSRGBColor(Dot.Key), 9.f)).Padding(0).HAlign(HAlign_Center).VAlign(VAlign_Center)
								[ UiMark(BuyMenu, W.Goes, 11.f, FLinearColor::FromSRGBColor(Dot.Value)) ]
							])
						]
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
		SNew(SBorder).BorderImage(BrixLook::Round(BrixLook::StoneDeep(), 14.f)).Padding(FMargin(12, 10, 12, 12))
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
				BrixLook::Button(O.Verb + TEXT(" · ") + Format(P.Price), [this, Op, P]() { Act([Op, P](FString& S) { return Op.Buy(P, S); }); }, true, P.State == TEXT("ok"), UiLantern, UiInk, 14)
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

// ---- quests and jobs in conversation (QST-001 to QST-007; life.js conversation(), chose(), talkEnded())

int32 ABrixLife::LotHere() const
{
	const FVector2D P = PlayerAt();
	int32 Best = -1;
	double BestD = CounterReach;
	for (int32 n = 0; n < Lots.Num(); ++n)
	{
		const FBrixEconLot& L = Lots[n];
		const FVector2D C = L.Centre(), D = P - C;
		const double A = FMath::DegreesToRadians(-LotAngle[n]);
		const FVector2D Q2 = FVector2D(D.X * FMath::Cos(A) - D.Y * FMath::Sin(A), D.X * FMath::Sin(A) + D.Y * FMath::Cos(A)) + C;
		if (Q2.X >= L.Min.X && Q2.X <= L.Max.X && Q2.Y >= L.Min.Y && Q2.Y <= L.Max.Y) return n;
		const double Dd = FVector2D::Distance(P, LotDoor[n]);
		if (Dd < BestD) { BestD = Dd; Best = n; }
	}
	return Best;
}

FBrixQuests::FCtx ABrixLife::Ctx() const
{
	FBrixQuests::FCtx C;
	const int32 n = LotHere();
	if (n >= 0) C.Lot = Lots[n].Id;
	C.Pos = PlayerAt();
	C.Minute = E ? E->Minute : 0;
	return C;
}

TMap<FString, FString> ABrixLife::TalkVars() const
{
	return { { TEXT("player"), bNamed ? PlayerName : FString(TEXT("you")) } };   // life.js vars(): the designed character's name (CHR-001)
}

TMap<FString, FString> ABrixLife::JobVars(const FBrixJob& O) const
{
	TMap<FString, FString> V;
	const FBrixJobKind* K = QuestRules.Job(O.Kind);
	const double Dl = O.Deadline > 0 ? O.Deadline : E->Day * 1440.0 + E->Minute + O.Hours * 60.0;
	V.Add(TEXT("pay"), Format(O.Pay));
	V.Add(TEXT("hours"), FString::FromInt(O.Hours));
	V.Add(TEXT("by"), UiHhmm(FMath::Fmod(Dl, 1440.0)));
	V.Add(TEXT("minutes"), FString::FromInt(O.Minutes ? O.Minutes : K ? K->Minutes : 0));
	V.Add(TEXT("item"), O.Item.IsEmpty() ? FString() : E->Item(O.Item)->Name.ToLower());
	V.Add(TEXT("from"), O.Kind == TEXT("parcel") ? Lots[O.Frm].Name : Day && Day->Names.IsValidIndex(O.Giver) ? Day->Names[O.Giver] : FString());
	V.Add(TEXT("to"), O.To >= 0 ? Lots[O.To].Name : FString());
	return V;
}

FString ABrixLife::FillJob(const FString& Text, const FBrixJob& O) const { return FBrixDialogue::Fill(Text, JobVars(O)); }

FBrixDialogue::FConversation ABrixLife::Conversation(int32 i, const FBrixDialogue::FConversation& Base)
{
	Offered.Reset();
	if (!Q || !QuestResidents.IsValidIndex(i)) return Base;
	const FString Rid = QuestResidents[i].Id;
	auto Prefix = [](const FString& Qid, const TMap<FString, FBrixDialogue::FNode>& Nodes, TMap<FString, FBrixDialogue::FNode>& Into)
	{
		for (const auto& N : Nodes)
		{
			FBrixDialogue::FNode X = N.Value;
			for (FBrixDialogue::FOption& O : X.Options) if (!O.Next.IsEmpty()) O.Next = TEXT("q:") + Qid + TEXT(":") + O.Next;
			Into.Add(TEXT("q:") + Qid + TEXT(":") + N.Key, X);
		}
	};
	const TArray<FString> Talks = Q->TalkingTo(Rid);
	if (Talks.Num())   // this resident is the one to see for a quest stage: that talk, nothing else
	{
		const FBrixQuestStage* St = Q->Stage(Talks[0]);
		FBrixDialogue::FConversation C;
		C.Title = QuestRules.Find(Talks[0])->Title;
		C.Start = TEXT("q:") + Talks[0] + TEXT(":") + St->Start;
		Prefix(Talks[0], St->Nodes, C.Nodes);
		return C;
	}
	FBrixDialogue::FConversation C = Base;
	C.Vars.Reset();
	TArray<FBrixDialogue::FOption> Extra;
	for (const FString& Qid : Q->Offers(Rid))   // quest lines this resident offers
	{
		const FBrixQuestLine* L = QuestRules.Find(Qid);
		Prefix(Qid, L->OfferNodes, C.Nodes);
		FBrixDialogue::FOption O;
		O.Text = L->OfferLine;
		O.Next = TEXT("q:") + Qid + TEXT(":") + L->OfferStart;
		Extra.Add(O);
	}
	const FString RoleName = QuestResidents[i].Role;
	const bool bHasKind = QuestRules.Jobs.ContainsByPredicate([&](const FBrixJobKind& K) { return K.Roles.Contains(RoleName); });
	auto Line = [this](const TCHAR* K) { return QuestRules.Lines.FindRef(K); };
	auto Node = [](const FString& Say, const FString& Mood, const TArray<FBrixDialogue::FOption>& Opts) { FBrixDialogue::FNode N; N.Say = Say; N.Mood = Mood; N.Options = Opts; return N; };
	auto Opt = [](const FString& Text, const FString& Job) { FBrixDialogue::FOption O; O.Text = Text; O.Job = Job; return O; };
	if (Q->Job && Q->Job->Kind == TEXT("shopping") && Q->Job->Giver == i)   // bringing an elder their shopping
	{
		C.Vars = JobVars(*Q->Job);
		C.Nodes.Add(TEXT("job:thanks"), Node(Line(TEXT("thanks")), TEXT("happy"), { Opt(TEXT("Glad to help."), FString()) }));
		FBrixDialogue::FOption O = Opt(Line(TEXT("hand_over")), TEXT("hand"));
		O.Next = TEXT("job:thanks");
		O.Need = { TEXT("have:") + Q->Job->Item };
		Extra.Add(O);
	}
	else if (bHasKind)   // asking for work (QST-006)
	{
		if (const TOptional<FBrixJob> O = Q->JobOffer(i, E->Day))
		{
			C.Vars = JobVars(*O);
			if (Q->Job) C.Nodes.Add(TEXT("job:offer"), Node(Line(TEXT("busy")), TEXT("thinking"), { Opt(TEXT("Right, I will finish that first."), FString()) }));
			else
			{
				Offered = *O;
				C.Nodes.Add(TEXT("job:offer"), Node(QuestRules.Job(O->Kind)->Offer, TEXT("thinking"), { Opt(Line(TEXT("accept")), TEXT("take")), Opt(Line(TEXT("decline")), FString()) }));
			}
		}
		else C.Nodes.Add(TEXT("job:offer"), Node(Line(TEXT("none")), TEXT("neutral"), { Opt(TEXT("Fair enough."), FString()) }));
		FBrixDialogue::FOption Ask = Opt(Line(TEXT("ask")), FString());
		Ask.Next = TEXT("job:offer");
		Extra.Add(Ask);
	}
	if (Extra.Num())
		if (FBrixDialogue::FNode* N = C.Nodes.Find(C.Start))
		{
			Extra.Append(N->Options);
			if (Extra.Num() > 9) Extra.SetNum(9);
			N->Options = Extra;
		}
	return C;
}

bool ABrixLife::Allowed(const FBrixDialogue::FOption& O) const { return !Q || Q->NeedOk(O.Need); }

void ABrixLife::Chose(int32 i, const FBrixDialogue::FOption& O)
{
	if (!Q || !QuestResidents.IsValidIndex(i)) return;
	bool bOk = true;
	if (!O.Quest.IsEmpty())
	{
		TArray<FString> Acts = O.Act;
		Acts.RemoveAll([](const FString& A) { return A == TEXT("shop") || A == TEXT("bank"); });
		bOk = Q->Act(O.Quest, Acts, QuestResidents[i].Id);
	}
	if (bOk && O.Job == TEXT("take") && Offered) bOk = Q->TakeJob(*Offered);
	if (bOk && O.Job == TEXT("hand")) bOk = Q->HandOver();
	if (!bOk) Toast(E->Why);
	if (O.Act.Contains(TEXT("shop"))) PendingCounter = i;
}

void ABrixLife::TalkEnded()
{
	Offered.Reset();
	if (PendingCounter < 0 || !Day) return;
	const TOptional<FBrixWhere> W = Day->Where(PendingCounter, Minute());
	PendingCounter = -1;
	if (W && W->Lot >= 0 && !CounterWords(W->Lot).IsEmpty())
	{
		if (W->Lot == E->BankLot) Open(TEXT("bank"), W->Lot);
		else if (W->Lot == E->MarketLot) Open(TEXT("stall"), W->Lot);
		else { Side = TEXT("buy"); Open(TEXT("shop"), W->Lot); }
	}
	else CounterKey();
}

// ---- the quest log (QST-005) and the tracker

FString ABrixLife::WhereWords(const FBrixGoal& G) const
{
	if (G.Kind == TEXT("talk"))
	{
		const int32 i = Q->ResidentIndex(G.Value);
		const FString N = Day && Day->Names.IsValidIndex(i) ? Day->Names[i] : G.Value;
		const TOptional<FBrixWhere> W = Day ? Day->Where(i, Minute()) : TOptional<FBrixWhere>();
		if (!W) return N + TEXT(" is away from the streets just now (by tram or car).");
		if (W->Lot >= 0 && !W->bWalking) return N + TEXT(" is at ") + Lots[W->Lot].Name + TEXT(" now.");
		return N + TEXT(" is out and about") + (W->bWalking ? TEXT(", walking.") : TEXT("."));
	}
	if (G.Kind == TEXT("go")) { const int32 n = E->LotIndex.FindRef(G.Value, -1); return n >= 0 ? TEXT("Go to ") + Lots[n].Name + TEXT(".") : FString(); }
	if (G.Kind == TEXT("near")) return TEXT("Between ") + UiHhmm(G.From) + TEXT(" and ") + UiHhmm(G.To) + TEXT(". It is ") + UiHhmm(Minute()) + TEXT(".");
	return FString();
}

TArray<TPair<FString, FString>> ABrixLife::Tracked() const
{
	TArray<TPair<FString, FString>> Out;
	if (!Q) return Out;
	for (int32 k = 0; k < Q->Active.Num() && k < TrackedQuests; ++k)
		if (const FBrixQuestStage* St = Q->Stage(Q->Active[k].Key)) Out.Add({ QuestRules.Find(Q->Active[k].Key)->Title, St->Log });
	if (Q->Job)
	{
		const FBrixJobKind* K = QuestRules.Job(Q->Job->Kind);
		const double Left = FMath::Max(0.0, Q->Job->Deadline - (E->Day * 1440.0 + E->Minute));
		Out.Add({ FString::Printf(TEXT("%s · %dh %02dm left"), *K->Title, (int32)(Left / 60), (int32)FMath::Fmod(Left, 60.0)), FillJob(K->Log, *Q->Job) });
	}
	return Out;
}

TSharedRef<SWidget> ABrixLife::TrackCard(int32 K)
{
	return SNew(SBox).Padding(FMargin(0, 0, 0, 6)).Visibility_Lambda([this, K]() { return Tracked().IsValidIndex(K) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(UiGold).Padding(0)[ SNew(SBox).WidthOverride(3.f) ] ]
		+ SHorizontalBox::Slot().FillWidth(1)
		[
			SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(UiPanel62).Padding(FMargin(10, 7))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)[ SNew(STextBlock).Font(UiFont(14)).ColorAndOpacity(UiCream).AutoWrapText(true).Text_Lambda([this, K]() { const auto T = Tracked(); return FText::FromString(T.IsValidIndex(K) ? T[K].Key : FString()); }) ]
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Font(UiFont(11)).ColorAndOpacity(UiCream).AutoWrapText(true).Text_Lambda([this, K]() { const auto T = Tracked(); return FText::FromString(T.IsValidIndex(K) ? T[K].Value : FString()); }) ]
			]
		]
	];
}

void ABrixLife::QuestsPanel(TSharedRef<SVerticalBox> Body)
{
	auto Slot = [&](TSharedRef<SWidget> W, float Top) { Body->AddSlot().AutoHeight().Padding(0, Top, 0, 0)[ W ]; };
	auto Entry = [&](const FString& Title, const FString& Text, const FString& Where, bool bDone)
	{
		TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
		V->AddSlot().AutoHeight()[ UiText(Title, 17, bDone ? UiDim : UiCream) ];
		if (!Text.IsEmpty()) V->AddSlot().AutoHeight().Padding(0, 2, 0, 0)[ UiText(Text, 14) ];
		if (!Where.IsEmpty()) V->AddSlot().AutoHeight().Padding(0, 2, 0, 0)[ UiText(Where, 14, UiDim) ];
		Slot(V, 8);
	};
	if (!Q) { Slot(UiText(TEXT("No quests in this build."), 15, UiDim), 0); return; }
	Slot(UiHeading(TEXT("Under way")), 0);
	if (!Q->Active.Num() && !Q->Job) Slot(UiText(TEXT("Nothing under way. Talk to people: some have things for you to do, and some have work (ask them)."), 15, UiDim), 6);
	for (const auto& A : Q->Active)
		if (const FBrixQuestStage* St = Q->Stage(A.Key)) Entry(QuestRules.Find(A.Key)->Title, St->Log, WhereWords(St->Goal), false);
	if (Q->Job)
	{
		const FBrixJob& J = *Q->Job;
		const FBrixJobKind* K = QuestRules.Job(J.Kind);
		FString T = FillJob(K->Log, J);
		if (J.Kind == TEXT("shift")) T += FString::Printf(TEXT(" (%d of %d minutes done)"), (int32)J.Worked, J.Minutes);
		if (J.Kind == TEXT("parcel")) T += J.Step == TEXT("pickup") ? TEXT(" Collect it first.") : TEXT(" You have the parcel.");
		Entry(K->Title, T, FString(), false);
	}
	bool bOffers = false;
	for (const FBrixQuestLine& L : QuestRules.Quests)
	{
		if (L.Giver.IsEmpty() || !Q->Offers(L.Giver).Contains(L.Id)) continue;
		if (!bOffers) { Slot(UiHeading(TEXT("On offer")), 0); bOffers = true; }
		FBrixGoal G;
		G.Kind = TEXT("talk");
		G.Value = L.Giver;
		Entry(L.Title, L.Summary, WhereWords(G), false);
	}
	if (Q->Done.Num())
	{
		Slot(UiHeading(TEXT("Finished")), 0);
		for (const auto& D : Q->Done) { const FBrixQuestLine* L = QuestRules.Find(D.Key); Entry(L->Title, L->Endings.FindRef(D.Value).Log, FString(), true); }
	}
	TArray<TPair<FString, int32>> Reps;
	for (const auto& R : E->Rep) if (R.Value) Reps.Add({ R.Key, R.Value });
	Reps.StableSort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) { return A.Value > B.Value; });
	if (Reps.Num())
	{
		Slot(UiHeading(TEXT("People you know")), 0);
		for (const auto& R : Reps)
		{
			const int32 i = Q->ResidentIndex(R.Key);
			TSharedRef<SHorizontalBox> B = SNew(SHorizontalBox);
			B->AddSlot().FillWidth(1)[ UiText(Day && Day->Names.IsValidIndex(i) ? Day->Names[i] : R.Key) ];
			B->AddSlot().AutoWidth().Padding(12, 0)[ UiText(Day && Day->Careers.IsValidIndex(i) && !Day->Careers[i].IsEmpty() ? Day->Careers[i] : QuestResidents.IsValidIndex(i) ? QuestResidents[i].Role : FString(), 15, UiDim) ];
			B->AddSlot().AutoWidth()[ UiText((R.Value > 0 ? TEXT("+") : TEXT("")) + FString::FromInt(R.Value), 15, R.Value > 0 ? UiPos : UiNeg) ];
			Slot(B, 4);
		}
	}
}

// ---- money coming in, shown (SND-019, SND-020; rewards.js) and achievements (QST-009, SND-021)
// The ledger has already paid: these only show it. While coins are on their way the purse shows that much less (Held),
// so it counts up as they land.

void ABrixLife::Later(double Dt, TFunction<void()> Fn) { Timeline.Add({ Clock + Dt, MoveTemp(Fn) }); }

void ABrixLife::Sound(const FString& What, int32 I, int32 N)
{
	OnRewardSound.Broadcast(What, I, N);   // the sound session plays sound.js's Sound.reward(what, i, n) here
}

void ABrixLife::Land(const FString& Purse, int64 Chips, TSharedPtr<FShowing> S)
{
	(Purse == TEXT("cash") ? HeldCash : HeldBank) -= Chips;
	Sparks(Purse == TEXT("cash") ? PurseCashPx() : PurseBankPx(), 10);
	if (++S->Landed < S->N) return;
	if (S->Kind == TEXT("reward")) Sound(TEXT("quest_end"));
	else { Later(.12, [this]() { Sound(TEXT("interest_end")); }); Later(RewardRules.SlipS, [this]() { SlipLeft = 0; }); }
}

void ABrixLife::Sparks(FVector2D At, int32 N)
{
	for (int32 k = 0; k < N; ++k)
	{
		const double A = FMath::FRandRange(0.0, UE_DOUBLE_TWO_PI), V = FMath::FRandRange(.3, 1.0) * 150 * PxScale;
		Spark.Add({ At, FVector2D(FMath::Cos(A) * V, FMath::Sin(A) * V - 45 * PxScale), 0, FMath::FRandRange(.4, .9) });
	}
}

void ABrixLife::ShowReward(const FBrixRewardEvent& Ev)
{
	const FBrixRewardRules& C = RewardRules;
	TSharedPtr<FShowing> S = MakeShared<FShowing>();
	S->Kind = Ev.Kind;
	S->N = FMath::Max(1, Ev.Coins);
	FString Main = TEXT("p:cash");
	for (const auto& T : Ev.To) if (T.Value > Ev.To.FindRef(Main)) Main = T.Key;
	const FString Purse = Main == TEXT("p:cash") ? TEXT("cash") : TEXT("bank");
	const int64 Each = Ev.Amount / S->N;
	(Purse == TEXT("cash") ? HeldCash : HeldBank) += Ev.Amount;
	Sound(TEXT("quest_start"));
	const APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector At = Me ? Me->GetActorLocation() / CmPerM - FVector(0, 0, .85) : FVector::ZeroVector;   // the player's feet, metres
	const bool bTwo = S->N > 10;
	// The slice spreads them all round the player; here the camera is close behind, so they spring out over an arc on the
	// far side and to the sides (CoinArcDeg), never between the player and the eye where they would loom large.
	double Away = FMath::FRandRange(0.0, 6.2832);
	if (const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		if (PC->PlayerCameraManager)
		{
			const FVector Eye = PC->PlayerCameraManager->GetCameraLocation() / CmPerM;
			if (FVector2D(At.X - Eye.X, At.Y - Eye.Y).Size() > .2) Away = FMath::Atan2(At.Y - Eye.Y, At.X - Eye.X);
		}
	const double Arc = FMath::DegreesToRadians(FMath::Clamp((double)CoinArcDeg, 30.0, 360.0)), Turn = FMath::RandBool() ? -1.0 : 1.0;
	const double Phase = Away - Turn * Arc / 2;
	for (int32 i = 0; i < S->N; ++i)
	{
		const int64 Part = i == S->N - 1 ? Ev.Amount - Each * (S->N - 1) : Each;
		Later(C.StartS + i * C.GapS, [this, i, S, Purse, Part, Phase, Turn, Arc, bTwo, At]()
		{
			const FBrixRewardRules& K = RewardRules;
			if (i % 2 == 0) Sound(TEXT("coin_out"), i, S->N);
			const double A = Phase + Turn * (Arc * (i + .5) / S->N + FMath::FRandRange(-.08, .08));
			const double R = (bTwo ? (i % 3 == 0 ? K.RingM[0] : K.RingM[1]) : (K.RingM[0] + K.RingM[1]) / 2) * FMath::FRandRange(.92, 1.08);
			const double Vy = FMath::FRandRange(K.UpMps[0], K.UpMps[1]), T = (Vy + FMath::Sqrt(Vy * Vy + 2 * K.FallMps2 * (K.FromM - K.SizeM))) / K.FallMps2;
			FCoin Cn;
			Cn.P = At + FVector(0, 0, K.FromM);
			Cn.V = FVector(FMath::Cos(A) * R / T, FMath::Sin(A) * R / T, Vy);
			Cn.Floor = At.Z + K.SizeM;
			Cn.Spin = FMath::FRandRange(0.0, 6.0); Cn.Vs = FMath::FRandRange(9.0, 15.0); Cn.Ph = FMath::FRandRange(0.0, 6.0);
			Cn.Side = Turn;
			Cn.PullAt = K.PullAfterS + i * K.PullGapS;
			Cn.Purse = Purse; Cn.Part = Part; Cn.Show = S; Cn.I = i;
			Coins.Add(Cn);
		});
	}
}

void ABrixLife::ShowInterest(const FBrixRewardEvent& Ev)
{
	const FBrixRewardRules& C = RewardRules;
	TSharedPtr<FShowing> S = MakeShared<FShowing>();
	S->Kind = Ev.Kind;
	S->N = FMath::Max(1, C.InterestCoins);
	const int64 Each = Ev.Amount / S->N;
	HeldBank += Ev.Amount;
	TArray<FString> Names;
	for (const FString& A : Ev.Accounts) Names.Add(A == TEXT("p:current") ? TEXT("current account") : A == TEXT("p:savings") ? TEXT("savings account") : A.Mid(2));
	SlipWhat = FString::Join(Names, TEXT(" and "));
	SlipLeft = 1e9;
	Later(.3, [this]() { Sound(TEXT("interest_start")); });
	for (int32 i = 0; i < S->N; ++i)
	{
		const int64 Part = i == S->N - 1 ? Ev.Amount - Each * (S->N - 1) : Each;
		Later(C.InterestStartS + i * C.InterestGapS, [this, i, S, Part]()
		{
			FFly F;
			F.A = SlipPx() + FVector2D((-40 + i * 12) * PxScale, 0);
			F.To = TEXT("bank"); F.Len = RewardRules.InterestFlyS; F.Bend = -34; F.R = 7;
			F.Land = [this, i, S, Part]() { Sound(TEXT("interest_coin"), i, S->N); Land(TEXT("bank"), Part, S); };
			Flies.Add(F);
		});
	}
}

FVector2D ABrixLife::PurseCashPx() const { return FVector2D(22 + 36, 150 + 10) * PxScale; }
FVector2D ABrixLife::PurseBankPx() const { return FVector2D(22 + 120, 150 + 10) * PxScale; }
FVector2D ABrixLife::SlipPx() const { return FVector2D(22 + 90, 150 + 50) * PxScale; }

void ABrixLife::TickRewards(float FrameDt)
{
	const float Dt = FMath::Min(FrameDt, .05f);   // a long frame does not throw the coins across the street
	const FBrixRewardRules& C = RewardRules;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Vp;
		GEngine->GameViewport->GetViewportSize(Vp);
		if (Vp.X > 0) { ViewportPx = Vp; PxScale = GEngine->GameViewport->GetDPIScale(); }
	}
	if (E->L.Tx.Num() > SeenTx)
	{
		for (const FBrixRewardEvent& Ev : BrixRewardEvents(RewardRules, E->L.Tx, SeenTx)) { if (Ev.Kind == TEXT("reward")) ShowReward(Ev); else ShowInterest(Ev); Shown += Ev.Amount; }
		SeenTx = E->L.Tx.Num();
	}
	Clock += Dt;
	for (int32 k = 0; k < Timeline.Num();) if (Timeline[k].Key <= Clock) { TFunction<void()> Fn = MoveTemp(Timeline[k].Value); Timeline.RemoveAt(k); Fn(); } else ++k;
	// the banner: one at a time, none while a panel covers the screen
	BannerLeft = FMath::Max(-5.0, BannerLeft - Dt);
	if (BannerLeft <= -.7 && Banners.Num() && !IsOpen())
	{
		const FBrixAchievement* A = AchievementRules.Find(Banners[0]);
		Banners.RemoveAt(0);
		if (A) { BannerName = A->Name; BannerLine = A->Line; BannerLeft = AchievementRules.BannerS; BannerAge = 0; Sound(TEXT("achievement")); }
	}
	BannerAge += Dt;
	// the coins (rewards.js frame())
	const APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector Chest = Me ? Me->GetActorLocation() / CmPerM - FVector(0, 0, .85) + FVector(0, 0, C.FromM) : FVector::ZeroVector;
	for (FCoin& Cn : Coins)
	{
		Cn.Age += Dt;
		Cn.Spin += Cn.Vs * Dt;
		if (Cn.State == 0)   // thrown out: falls to its spot and bounces twice
		{
			Cn.V.Z -= C.FallMps2 * Dt;
			Cn.P += Cn.V * Dt;
			if (Cn.P.Z < Cn.Floor)
			{
				Cn.P.Z = Cn.Floor;
				if (!Cn.bHit) { Cn.bHit = true; Sound(TEXT("coin_land"), Cn.I, Cn.Show->N); }
				Cn.V.Z *= -C.Bounce; Cn.V.X *= .3; Cn.V.Y *= .3; Cn.Vs = FMath::Max(4.0, Cn.Vs * .6);
				if (++Cn.Bounces >= 2 || Cn.V.Z < 1) { Cn.State = 1; Cn.T = 0; }
			}
		}
		else if (Cn.State == 1)   // waiting: it stands up off the ground, turning slowly and bobbing
		{
			Cn.T += Dt;
			const double k = FMath::Min(1.0, Cn.T / .4), e = 1 - (1 - k) * (1 - k) * (1 - k);
			Cn.P.Z = Cn.Floor + e * C.HoverM + FMath::Sin(Cn.Age * 3.6 + Cn.Ph) * .03 * e;
			Cn.Vs += (3 - Cn.Vs) * FMath::Min(1.0, (double)Dt * 5);
		}
		if (Cn.State < 2 && Cn.Age >= Cn.PullAt) { Cn.State = 2; Cn.T = 0; Cn.From = Cn.P; }
		if (Cn.State == 2)   // taken: a small lift, then a curving swoop in to the player, wherever they have walked to
		{
			Cn.T += Dt;
			Cn.Vs = 18;
			Cn.Trail.Add(Cn.P);   // a short trail of light behind it (rewards.js c.trail)
			if (Cn.Trail.Num() > 6) Cn.Trail.RemoveAt(0);
			if (Cn.T < C.LiftS) Cn.P.Z = Cn.From.Z + Cn.T / C.LiftS * .16;
			else
			{
				const double u = FMath::Min(1.0, (Cn.T - C.LiftS) / C.PullS), e = u * u;
				const FVector S0 = Cn.From + FVector(0, 0, .16);
				const FVector2D D2(Chest.X - S0.X, Chest.Y - S0.Y);
				const double d = FMath::Max(1e-3, D2.Size());
				const FVector M((S0.X + Chest.X) / 2 - D2.Y / d * .55 * Cn.Side, (S0.Y + Chest.Y) / 2 + D2.X / d * .55 * Cn.Side, FMath::Max(S0.Z, Chest.Z) + .45);
				Cn.P = (1 - e) * (1 - e) * S0 + 2 * e * (1 - e) * M + e * e * Chest;
				Cn.Small = 1 - .45 * e;
				if (u >= 1)
				{
					Cn.bDone = true;
					Sound(TEXT("coin_take"), Cn.Show->Taken++, Cn.Show->N);
					FFly F;
					F.bFromPlayer = true;
					F.To = Cn.Purse; F.Len = C.FlyS; F.Bend = FMath::FRandRange(-30.0, 30.0); F.R = 6.5f;
					const FString Purse = Cn.Purse;
					const int64 Part = Cn.Part;
					TSharedPtr<FShowing> S = Cn.Show;
					F.Land = [this, Purse, Part, S]() { Land(Purse, Part, S); };
					Flies.Add(F);
				}
			}
		}
	}
	Coins.RemoveAll([](const FCoin& Cn) { return Cn.bDone; });
	// what is drawn: coins where they are on the screen, coins flying to the purse, sparks
	Sprites.Reset();
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	FVector2D PlayerPx = ViewportPx * FVector2D(.5, .92);
	if (PC && Me) { FVector2D Px; if (PC->ProjectWorldLocationToScreen(Chest * CmPerM, Px)) PlayerPx = Px; }
	// The coins, far ones first (rewards.js draw()). One behind the player's body is not drawn, as this layer lies over the
	// whole picture; each has its shadow under it, and while it hovers a pool of its light on the ground.
	const FVector Eye = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() / CmPerM : Chest;
	FBox2D Body(ForceInit);
	double BodyD = 0;
	if (PC && Me)
	{
		FVector2D F, Hd;
		const FVector Feet = Chest - FVector(0, 0, C.FromM);
		if (PC->ProjectWorldLocationToScreen(Feet * CmPerM, F) && PC->ProjectWorldLocationToScreen((Feet + FVector(0, 0, 1.75)) * CmPerM, Hd) && F.Y > Hd.Y)
		{
			const double Half = .3 * (F.Y - Hd.Y) / 1.75;
			Body = FBox2D(FVector2D(F.X - Half, Hd.Y), FVector2D(F.X + Half, F.Y));
			BodyD = FVector::Distance(Eye, Chest);
		}
	}
	TArray<const FCoin*> Order;
	for (const FCoin& Cn : Coins) Order.Add(&Cn);
	Order.Sort([&](const FCoin& A, const FCoin& B) { return FVector::DistSquared(A.P, Eye) > FVector::DistSquared(B.P, Eye); });
	for (const FCoin* Cp : Order)
	{
		const FCoin& Cn = *Cp;
		FVector2D Px, Up;
		if (!PC || !PC->ProjectWorldLocationToScreen(Cn.P * CmPerM, Px) || !PC->ProjectWorldLocationToScreen((Cn.P + FVector(0, 0, C.SizeM)) * CmPerM, Up)) continue;
		if (Body.bIsValid && FVector::Distance(Eye, Cn.P) > BodyD * 1.03 && Body.IsInside(Px)) continue;
		const float Pop = Cn.Age < .14 ? (float)(1 + 2.2 * FMath::Pow(Cn.Age / .14 - 1, 3) + 1.2 * FMath::Pow(Cn.Age / .14 - 1, 2)) : 1.f;
		const float Lift = Cn.State == 2 && Cn.T < C.LiftS ? (float)(1 + .18 * Cn.T / C.LiftS) : 1.f;
		const float R = FMath::Max(3.f * PxScale, (float)FVector2D::Distance(Px, Up)) * Pop * (float)Cn.Small * Lift;
		for (int32 k = 0; k < Cn.Trail.Num(); ++k)   // the trail, faint at its tail
		{
			FVector2D Tp;
			if (!PC->ProjectWorldLocationToScreen(Cn.Trail[k] * CmPerM, Tp)) continue;
			FBrixCoinSprite Tr;
			Tr.Px = Tp; Tr.R = R * (1 + k * .22f); Tr.Glow = (k + 1.f) / Cn.Trail.Num() * .55f; Tr.bHaloOnly = true;
			Sprites.Add(Tr);
		}
		FBrixCoinSprite Sp;
		Sp.Px = Px;
		Sp.R = R;
		Sp.Spin = (float)Cn.Spin;
		Sp.Glow = Cn.State == 1 ? .75f + .25f * FMath::Sin(Cn.Age * 3.6 + Cn.Ph) : Cn.State == 2 ? 1.f : .45f;
		const double Tw = FMath::Sin(Cn.Age * 2.3 + Cn.Ph * 5);
		if (Cn.State == 1 && Tw > .93) Sp.Glint = (float)((Tw - .93) / .07);
		FVector2D Fp, Fu;
		const FVector Floor(Cn.P.X, Cn.P.Y, Cn.Floor - C.SizeM + .01);
		if (Cn.State < 2 && PC->ProjectWorldLocationToScreen(Floor * CmPerM, Fp) && PC->ProjectWorldLocationToScreen((Floor + FVector(0, 0, C.SizeM)) * CmPerM, Fu))
		{
			const double UpM = FMath::Min(1.0, (Cn.P.Z - Cn.Floor) / 1.2);
			Sp.bFloor = true;
			Sp.FloorPx = Fp;
			Sp.FloorR = (float)(FVector2D::Distance(Fp, Fu) * (1.1 - UpM * .4));
			Sp.Shadow = (float)(1 - UpM);
			Sp.Pool = Cn.State == 1 ? 1.f : 0.f;
		}
		Sprites.Add(Sp);
	}
	for (FFly& F : Flies)
	{
		F.T += Dt / F.Len;
		if (F.bFromPlayer && !F.bStarted) { F.A = PlayerPx; F.bStarted = true; }
		const FVector2D B = F.To == TEXT("cash") ? PurseCashPx() : PurseBankPx();
		const double k = FMath::Min(1.0, F.T), e = k * k * (3 - 2 * k);
		const FVector2D M((F.A.X + B.X) / 2 + F.Bend * PxScale, FMath::Min(F.A.Y, B.Y) - 50 * PxScale);
		for (int32 j = 3; j >= 1; --j)   // a fading streak of light behind it
		{
			const double kk = FMath::Max(0.0, k - j * .07), ee = kk * kk * (3 - 2 * kk);
			FBrixCoinSprite Tr;
			Tr.Px = (1 - ee) * (1 - ee) * F.A + 2 * ee * (1 - ee) * M + ee * ee * B;
			Tr.R = (11 - j * 2) * PxScale / 2.8f; Tr.Glow = .5f - j * .12f; Tr.bHaloOnly = true;
			Sprites.Add(Tr);
		}
		FBrixCoinSprite Sp;
		Sp.Px = (1 - e) * (1 - e) * F.A + 2 * e * (1 - e) * M + e * e * B;
		Sp.R = (F.R - 1.5f * (float)e) * PxScale;
		Sp.Spin = (float)(Clock * 13 + F.A.X);
		Sp.Glow = .8f;
		Sprites.Add(Sp);
		if (F.T >= 1) { F.bDone = true; F.Land(); }
	}
	Flies.RemoveAll([](const FFly& F) { return F.bDone; });
	for (FSpark& S : Spark)
	{
		S.T += Dt;
		S.V.Y += 300 * PxScale * Dt;
		S.P += S.V * Dt;
		const double a = 1 - S.T / S.Life;
		if (a > 0) { FBrixCoinSprite Sp; Sp.bSpark = true; Sp.Px = S.P; Sp.R = 4.5f * PxScale * (float)(.5 + a * .5); Sp.Alpha = (float)a; Sprites.Add(Sp); }
	}
	Spark.RemoveAll([](const FSpark& S) { return S.T >= S.Life; });
}

void ABrixLife::CheckAchievements()
{
	if (!AchievementRules.List.Num()) return;
	for (const FString& Id : BrixAward(AchievementRules, BrixAchievementFacts(AchievementRules, *E, Q.Get()), Feats, E->Day)) Banners.Add(Id);
}
