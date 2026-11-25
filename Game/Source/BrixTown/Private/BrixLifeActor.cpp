#include "BrixLifeActor.h"

#include "BrixSave.h"
#include "BrixBody.h"

#include "BrixCharacter.h"
#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixGameMode.h"
#include "BrixMallActor.h"
#include "BrixLook.h"
#include "BrixMapActor.h"
#include "BrixInside.h"
#include "BrixRooms.h"
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
#include "Camera/PlayerCameraManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SLeafWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScrollBar.h"
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
	// A row of buttons that goes on to the next line at a width in the interface's own units. (Wrapped at whatever width the
	// row was given, the designer's rows were measured for one width and laid out at another: they lay over one another.)
	TSharedRef<SWrapBox> UiWrap(float Width, float Gap = 6.f) { return SNew(SWrapBox).UseAllottedSize(false).PreferredSize(Width).InnerSlotPadding(FVector2D(Gap, 4)); }
	// .pbtn: the lantern's orange with dark letters; .off: quiet; disabled: grey
	TSharedRef<SWidget> UiButton(const FString& Label, TFunction<void()> OnClick, bool bEnabled = true, bool bQuiet = false)
	{
		return BrixLook::Button(Label, OnClick, !bQuiet, bEnabled);
	}
	// The picture behind the building designer's card (life.js pointer()): the mouse over it picks and drags the pieces and
	// inner walls and turns the view. Kind: 0 down, 1 move, 2 up, 3 the wheel, 4 a double click; Px in the picture's pixels.
	class SBrixDesignPad : public SLeafWidget
	{
	public:
		using FPoint = TFunction<void(int32 Kind, const FVector2D& Px, int32 Button, bool bShift, const FVector2D& Delta, float Wheel, bool bHeld)>;
		SLATE_BEGIN_ARGS(SBrixDesignPad) {}
			SLATE_ARGUMENT(FPoint, OnPoint)
			SLATE_ARGUMENT(TFunction<int32()>, Hand)   // 0 the arrow, 1 a hand over what can be moved, 2 a hand holding it
		SLATE_END_ARGS()
		void Construct(const FArguments& A) { OnPoint = A._OnPoint; Hand = A._Hand; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32 Layer, const FWidgetStyle&, bool) const override { return Layer; }
		virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override
		{
			if (OnPoint) OnPoint(0, Px(G, E), ButtonOf(E), E.IsShiftDown(), FVector2D::ZeroVector, 0.f, true);
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
		virtual FReply OnMouseButtonUp(const FGeometry& G, const FPointerEvent& E) override
		{
			if (!HasMouseCapture()) return FReply::Unhandled();
			if (OnPoint) OnPoint(2, Px(G, E), ButtonOf(E), E.IsShiftDown(), FVector2D::ZeroVector, 0.f, false);
			return FReply::Handled().ReleaseMouseCapture();
		}
		virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override
		{
			const bool bHeld = HasMouseCapture();
			const FVector2f D = E.GetCursorDelta();
			if (OnPoint) OnPoint(1, Px(G, E), 0, E.IsShiftDown(), FVector2D(D.X, D.Y), 0.f, bHeld);
			return bHeld ? FReply::Handled() : FReply::Unhandled();
		}
		virtual FReply OnMouseWheel(const FGeometry& G, const FPointerEvent& E) override
		{
			if (OnPoint) OnPoint(3, Px(G, E), 0, E.IsShiftDown(), FVector2D::ZeroVector, E.GetWheelDelta(), false);
			return FReply::Handled();
		}
		virtual FReply OnMouseButtonDoubleClick(const FGeometry& G, const FPointerEvent& E) override
		{
			if (OnPoint) OnPoint(4, Px(G, E), ButtonOf(E), E.IsShiftDown(), FVector2D::ZeroVector, 0.f, false);
			return FReply::Handled();
		}
		virtual FCursorReply OnCursorQuery(const FGeometry&, const FPointerEvent&) const override
		{
			const int32 H = Hand ? Hand() : 0;
			return FCursorReply::Cursor(H == 2 ? EMouseCursor::GrabHandClosed : H == 1 ? EMouseCursor::GrabHand : EMouseCursor::Default);
		}
	private:
		static int32 ButtonOf(const FPointerEvent& E) { const FKey K = E.GetEffectingButton(); return K == EKeys::RightMouseButton ? 2 : K == EKeys::MiddleMouseButton ? 1 : 0; }
		static FVector2D Px(const FGeometry& G, const FPointerEvent& E) { const FVector2f L = G.AbsoluteToLocal(E.GetScreenSpacePosition()); return FVector2D(L.X, L.Y) * G.Scale; }
		FPoint OnPoint;
		TFunction<int32()> Hand;
	};
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
	BuyMenu.Models = GoodsModels;   // a good with a model shows its model (FURN-065); without them, its stand-in
	double D0 = 0, M0 = 0;
	Frame(D0, M0);
	E->Advance((int32)D0, M0);
	if (LoadBrixEstate(Data, EstateRules, EstateUnits, EstateRoles, Error)) Est = MakeUnique<FBrixEstate>(EstateRules, *E, EstateUnits, EstateRoles);   // PROP-009
	else UE_LOG(LogTemp, Warning, TEXT("Brix Town: no property market (%s)"), *Error);
	if (LoadBrixQuests(Data.Json, QuestRules, QuestResidents, Error))
	{
		Q = MakeUnique<FBrixQuests>(QuestRules, *E, QuestResidents);
		Q->OnNote = [this](const FString& T) { Toast(T); };
		Q->OwnsHome = [this]() { if (Est) for (const int32 u : Est->Owned()) if (Est->Units[u].Kind == TEXT("house") || Est->Units[u].Kind == TEXT("flat")) return true; return false; };   // QST-015: a home of the player's own (PROP-009)
		Q->NewGame();   // a new game (life.js newGame()): the starting cash, and the first starter quest line
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Brix Town: no quests (%s)"), *Error);
		E->NewCharacter();
	}
	if (Est) Est->Start();   // the estate agents open (life.js newGame(): M.start())
	StartStore(nullptr);     // and the furniture outlet's tills (FURN-022 to FURN-029)
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
		if (n != E->BankLot && n != E->MarketLot && n != OutletLotIx && E->ShopKind(n).IsEmpty() && !(Est && Est->Office.Contains(Lots[n].Id))) continue;   // the bank, the market, the shops and the estate agents have a counter
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
	if (Panel == TEXT("remodel")) DesignKeys();
	double D0 = 0, M0 = 0;
	Frame(D0, M0);
	E->Advance((int32)D0, M0);   // the bank settles each midnight passed (BANK-008)
	if (Q && (CheckLeft -= Dt) <= 0.f) { CheckLeft = CheckSeconds; StoreFrame(); Q->Check(Ctx()); PlayScene(); CheckAchievements(); }
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
	if (Store) S.Store = Store->Save();   // the furniture outlet's bookings (FURN-025 to FURN-029)
	S.Remodels = MakeShared<FJsonObject>();   // QST-008: the buildings remodelled, each its design as paid for (life.js remodels)
	for (const auto& R : RemodelsDone) S.Remodels->SetObjectField(R.Key, R.Value);
	S.Plans = MakeShared<FJsonObject>();   // QST-008: the storeys replanned (life.js plans: lot id -> storey -> walls)
	for (const auto& P : PlansDone)
	{
		TSharedPtr<FJsonObject> ByFloor = MakeShared<FJsonObject>();
		for (const auto& F : P.Value)
		{
			TArray<TSharedPtr<FJsonValue>> Ws;
			for (const FBrixPlanWall& W : F.Value) Ws.Add(MakeShared<FJsonValueObject>(BrixRoomPlan::WallToJson(W)));
			ByFloor->SetArrayField(FString::FromInt(F.Key), Ws);
		}
		S.Plans->SetObjectField(P.Key, ByFloor);
	}
	S.Furnishings = MakeShared<FJsonObject>();   // QST-008: the storeys furnished (life.js furnishings) and the furniture among the belongings
	for (const auto& P : LaysDone)
	{
		TSharedPtr<FJsonObject> ByFloor = MakeShared<FJsonObject>();
		for (const auto& F : P.Value)
		{
			TArray<TSharedPtr<FJsonValue>> Ps;
			for (const FBrixLayoutPiece& X : F.Value) Ps.Add(MakeShared<FJsonValueObject>(BrixFurnishPlan::ToJson(X)));
			ByFloor->SetArrayField(FString::FromInt(F.Key), Ps);
		}
		S.Furnishings->SetObjectField(P.Key, ByFloor);
	}
	S.Furniture.Reset();
	for (const FBrixLayoutPiece& X : FurnHeld) S.Furniture.Add(MakeShared<FJsonValueObject>(BrixFurnishPlan::ToJson(X, false)));
	S.Feats = Feats;
	if (const ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))   // CITY-100
		S.Boards = BrixBoardsSave(E->Boards, Me->Boards.InUse, Me->Boards.Riding());
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
		NewQ->OwnsHome = [this]() { if (Est) for (const int32 u : Est->Owned()) if (Est->Units[u].Kind == TEXT("house") || Est->Units[u].Kind == TEXT("flat")) return true; return false; };   // QST-015: a home of the player's own (PROP-009)
	}
	TUniquePtr<FBrixEstate> NewM;
	if (Est)
	{
		NewM = MakeUnique<FBrixEstate>(EstateRules, *NewE, EstateUnits, EstateRoles);
		if (S.Estate) NewM->Load(S.Estate); else NewM->Start();   // a save from before the market: it opens now (life.js loadSave())
	}
	if (!Panel.IsEmpty()) Close();
	Store.Reset();        // the store holds the economy too
	Q = MoveTemp(NewQ);   // the quests and the market hold the economy: the old ones go first
	Est = MoveTemp(NewM);
	for (const auto& R : RemodelsDone) if (const int32* n = E->LotIndex.Find(R.Key)) ApplyRemodel(*n, nullptr);   // the old game's remodels undone
	E = MoveTemp(NewE);
	StartStore(S.Store);
	RentEdit.Reset();
	TMap<FString, TMap<int32, FBrixPlan>> OldPlans = MoveTemp(PlansDone);
	PlansDone.Reset();
	for (const auto& P : OldPlans) if (const int32* n = E->LotIndex.Find(P.Key)) for (const auto& F : P.Value) ShowPlan(*n, F.Key, PlanNow(*n, F.Key));   // the old game's room plans undone
	if (S.Plans)
		for (const auto& KV : S.Plans->Values)
		{
			if (!KV.Value.IsValid() || KV.Value->Type != EJson::Object) continue;
			for (const auto& F : KV.Value->AsObject()->Values)
			{
				FBrixPlan Pl;
				for (const auto& W : F.Value->AsArray()) Pl.Add(BrixRoomPlan::WallFromJson(W->AsObject()));
				const int32 f = FCString::Atoi(*FString(F.Key));
				PlansDone.FindOrAdd(FString(KV.Key)).Add(f, Pl);
				if (const int32* n = E->LotIndex.Find(FString(KV.Key))) ShowPlan(*n, f, Pl);
			}
		}
	TMap<FString, TMap<int32, FBrixLayout>> OldLays = MoveTemp(LaysDone);
	LaysDone.Reset();
	for (const auto& P : OldLays) if (const int32* n = E->LotIndex.Find(P.Key)) for (const auto& F : P.Value) ShowLayout(*n, F.Key, LayNow(*n, F.Key));   // the old game's furniture as the town had it
	if (S.Furnishings)
		for (const auto& KV : S.Furnishings->Values)
		{
			if (!KV.Value.IsValid() || KV.Value->Type != EJson::Object) continue;
			for (const auto& F : KV.Value->AsObject()->Values)
			{
				FBrixLayout Ly;
				for (const auto& X : F.Value->AsArray()) Ly.Add(BrixFurnishPlan::FromJson(X->AsObject()));
				const int32 f = FCString::Atoi(*FString(F.Key));
				LaysDone.FindOrAdd(FString(KV.Key)).Add(f, Ly);
				if (const int32* n = E->LotIndex.Find(FString(KV.Key))) ShowLayout(*n, f, Ly);
			}
		}
	FurnHeld.Reset();
	for (const auto& X : S.Furniture) if (X.IsValid() && X->Type == EJson::Object) FurnHeld.Add(BrixFurnishPlan::FromJson(X->AsObject()));
	RemodelsDone.Reset();
	if (S.Remodels)
		for (const auto& KV : S.Remodels->Values)
			if (KV.Value.IsValid() && KV.Value->Type == EJson::Object)
			{
				RemodelsDone.Add(FString(KV.Key), KV.Value->AsObject());
				if (const int32* n = E->LotIndex.Find(FString(KV.Key))) ApplyRemodel(*n, KV.Value->AsObject());
			}
	Feats = S.Feats;
	if (ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))   // CITY-100: the boards owned, the one in use in the hand
		Me->SetBoards(E->Boards, BrixBoardsInUse(S.Boards));
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
	SAssignNew(PanelBox, SBox)
		.HAlign(HAlign_Center).VAlign(VAlign_Center)
		// the building designer's card stands to the left, the building in view beside it (BUILD-020)
		.Padding_Lambda([this]() { return Panel == TEXT("remodel") ? FMargin(16, 24) : FMargin(16, 40); })   // (the designer's card is put at the left in Draw())
		.Visibility_Lambda([this]() { return Panel.IsEmpty() ? EVisibility::Collapsed : Panel == TEXT("remodel") ? EVisibility::SelfHitTestInvisible : EVisibility::Visible; })   // beside the designer's card the mouse is on the picture
		[
			SNew(SBox).WidthOverride_Lambda([this]() { return IsShop() || Panel == TEXT("store") ? (float)BuyMenu.WidePanelPx : Panel == TEXT("remodel") ? RemodelPanelPx : 620.f; })
			[
				BrixLook::Card(SAssignNew(PanelBody, SBox), FMargin(22, 18, 22, 14), BrixLook::Stone(), 18.f)
			]
		];
	GEngine->GameViewport->AddViewportWidgetContent(Hud.ToSharedRef(), 15);
	DesignPad = SNew(SBrixDesignPad)
		.Visibility_Lambda([this]() { return Panel == TEXT("remodel") && !bMenu ? EVisibility::Visible : EVisibility::HitTestInvisible; })
		.OnPoint([this](int32 Kind, const FVector2D& Px, int32 Button, bool bShift, const FVector2D& Delta, float Wheel, bool bHeld) { DesignPointer((EPoint)Kind, Px, Button, bShift, Delta, Wheel, bHeld); })
		.Hand([this]() { return Grab.Kind >= 3 ? 2 : (!PieceHover.IsEmpty() || !WallHover.IsEmpty()) ? 1 : 0; });
	GEngine->GameViewport->AddViewportWidgetContent(DesignPad.ToSharedRef(), 20);
	PanelRoot = PanelBox;
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
	if (n == OutletLotIx && Store) return TEXT("Buy at the ") + Lots[n].Name;   // the furniture outlet's tills (FURN-025)
	if (Est) if (const FString* Co = Est->Office.Find(Lots[n].Id)) return TEXT("Estate agent: ") + EstateRules.Company(*Co)->Name;
	if (!E->ShopKind(n).IsEmpty() && !E->Outlets.Contains(n)) return TEXT("Buy and sell at ") + Lots[n].Name;
	return FString();
}

void ABrixLife::CounterKey()
{
	if (Panel == TEXT("bank") || Panel == TEXT("shop") || Panel == TEXT("stall") || Panel == TEXT("outlet") || Panel == TEXT("agent") || Panel == TEXT("store")) { Close(); return; }
	int32 On = -1;
	FString U;
	if (OutletHere(On, U)) { PanelUnit = U; Open(TEXT("outlet"), On); return; }   // each outlet has its own counter (WLD-066, ECO-013)
	const int32 n = CounterHere();
	if (n >= 0 && E->Outlets.Contains(n)) { Toast(TEXT("Walk into one of ") + Lots[n].Name + TEXT("'s outlets to buy there: each has its own goods.")); return; }
	if (n < 0 || (n != OutletLotIx && n != E->BankLot && n != E->MarketLot))   // FURN-023: with the outlet's catalogue, B at a home of the player's orders from it
	{
		bool bWhole = false;
		TArray<int32> Fl;
		const int32 Here = LotHere();
		if (Store && Store->bCatalogue && Here >= 0 && E->ShopKind(Here).IsEmpty() && Holding(Here, bWhole, Fl)) { bStoreHome = true; StoreTab = TEXT("show"); Open(TEXT("store"), OutletLotIx); return; }
	}
	if (n < 0) { Toast(TEXT("No counter here. Shops, cafés, the bank and the market square have one.")); return; }
	if (n == OutletLotIx && Store) { bStoreHome = false; StoreTab = TEXT("show"); Open(TEXT("store"), n); return; }   // the furniture outlet's tills (FURN-025)
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
	if (PanelBox) PanelBox->SetHAlign(Panel == TEXT("remodel") ? HAlign_Left : HAlign_Center);   // the designer's card stands to the left, the building in view beside it (BUILD-020)
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
	else if (Panel == TEXT("store"))
	{
		Title = bStoreHome ? FString(TEXT("The outlet's catalogue")) : Lots[PanelLot].Name;
		Sub = bStoreHome ? TEXT("Ordered from home · delivery only") : TEXT("Furniture · take it now or have it delivered");
		if (StoreTab == TEXT("show")) Hint = TEXT("← → ↑ ↓  choose    Enter  add to basket    Esc  close");
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
	else if (Panel == TEXT("remodel")) { RemodelPanel(Body); DesignMarks(); }
	else if (Panel == TEXT("store")) StorePanel(Body);
	else if (Panel == TEXT("agent") && Est) EstatePanel(Body, Est->Office[Lots[PanelLot].Id], -1);
	else WalletPanel(Body);
	// The designer's card keeps room for its scroll bar whether it shows or not: its rows of buttons wrap by the width they
	// are given, and a bar that came and went with their height left them over one another.
	TSharedRef<SWidget> Scrolled = SNullWidget::NullWidget;
	if (Panel == TEXT("remodel"))
	{
		TSharedRef<SScrollBar> Bar = SNew(SScrollBar);
		Scrolled = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1)[ SNew(SScrollBox).ExternalScrollbar(Bar) + SScrollBox::Slot()[ Body ] ]
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(12.f)[ Bar ] ];
	}
	else Scrolled = SNew(SScrollBox) + SScrollBox::Slot()[ Body ];
	PanelBody->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0, 0, 12, 0)[ SNew(STextBlock).Font(UiFont(24)).ColorAndOpacity(UiCream).Text(FText::FromString(Title)) ]
			+ SWrapBox::Slot().VAlign(VAlign_Bottom).Padding(0, 0, 0, 3)[ SNew(STextBlock).Font(UiFont(14)).ColorAndOpacity(UiDim).Text(FText::FromString(Sub)) ]
		]
		+ SVerticalBox::Slot().FillHeight(1)[ Scrolled ]
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
	if (Mode->Town()) Mode->Town()->Repaint(Lot, Design, CutNow(Lot));   // cut open at the storey the designer looks into (BUILD-020)
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
	PlanUndo.Reset(); PlanDraft.Reset(); PlanBase.Reset(); WallSel.Reset();
	LayUndo.Reset(); LayDraft.Reset(); LayBase.Reset(); PieceSel.Reset();
	DesignTab = bWhole ? TEXT("outside") : TEXT("inside");
	DesignFacade = TEXT("*");
	DesignStorey = Floors.Num() ? Floors[0] : 0;
	ViewTurn = 0;
	ViewYawAdd = ViewPitchAdd = 0; ViewZoom = 1; ViewPan = FVector2D::ZeroVector;
	Grab = FGrab(); PieceHover.Reset(); WallHover.Reset(); bPieceList = false;
	Open(TEXT("remodel"), Lot);
	DesignView();
	return Panel == TEXT("remodel");
}

void ABrixLife::RemodelSet(const FBrixDesignChanges& Changes)
{
	PushUndo();
	Draft = BrixBuilder::Remodel(Draft, Changes);
	ApplyRemodel(PanelLot, Draft);
	Msg.Reset();
	Draw();
}

void ABrixLife::EndRemodel()
{
	const bool bWasCut = CutLot >= 0;   // cut open for the designer: whole again
	CutLot = CutFloor = -1;
	Grab = FGrab(); PieceHover.Reset(); WallHover.Reset();
	if (const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this))) if (Mode->RoomsActor()) { Mode->RoomsActor()->SetDesign(-1, -1); Mode->RoomsActor()->SetMarks({}, {}); }
	if (PanelLot >= 0 && (Draft != DraftBase || bWasCut)) ApplyRemodel(PanelLot, RemodelsDone.Contains(Lots[PanelLot].Id) ? DraftBase : TSharedPtr<FJsonObject>());   // not paid for: as it was
	for (const auto& P : PlanDraft) if (!(P.Value == PlanBase.FindRef(P.Key))) ShowPlan(PanelLot, P.Key, PlanNow(PanelLot, P.Key));   // not paid for: as they were
	Draft.Reset();
	DraftBase.Reset();
	DraftUndo.Reset();
	PlanUndo.Reset(); PlanDraft.Reset(); PlanBase.Reset();
	for (const auto& L : LayDraft) if (!(L.Value == LayBase.FindRef(L.Key))) ShowLayout(PanelLot, L.Key, LayNow(PanelLot, L.Key));   // as it was
	LayUndo.Reset(); LayDraft.Reset(); LayBase.Reset();
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC && PC->GetPawn()) PC->SetViewTargetWithBlend(PC->GetPawn(), 0.5f);
	if (ViewCam) { ViewCam->Destroy(); ViewCam = nullptr; }
}

double ABrixLife::DesignFloorM() const
{
	const FBrixLot* L = CityLot(PanelLot);
	return L ? BrixFloorUp(*L, DesignStorey, FBrixDoorRules::FromConfig()) : 3.0 * DesignStorey;
}

double ABrixLife::DesignPitch0() const { return FMath::DegreesToRadians(DesignTab != TEXT("outside") ? DesignPitchDeg + 20.0 : (double)DesignPitchDeg); }

double ABrixLife::DesignDist0() const
{
	if (!Draft || !Lots.IsValidIndex(PanelLot)) return 10.0;
	const FVector2D Size = Lots[PanelLot].Max - Lots[PanelLot].Min;
	const bool bInside = DesignTab != TEXT("outside");
	return (FMath::Max(Size.X, Size.Y) * (bInside ? 1.1 : 1.35) + (bInside ? 2.0 : Draft->GetObjectField(TEXT("shell"))->GetNumberField(TEXT("height")))) * 1.15;
}

float ABrixLife::CutNow(int32 Lot) const
{
	if (Lot != CutLot || CutFloor < 0) return -1.f;
	const FBrixLot* L = CityLot(Lot);
	return L ? BrixFloorUp(*L, CutFloor, FBrixDoorRules::FromConfig()) + DesignCutM : -1.f;
}

// BUILD-020 (life.js syncView(); play.js: the designer's cut): looking inside, the building is drawn cut open a little above
// the storey's floor and only that storey's rooms are drawn; from outside it is whole
void ABrixLife::SyncCut()
{
	const bool bIn = Panel == TEXT("remodel") && Draft && DesignTab != TEXT("outside");
	const int32 WantLot = bIn ? PanelLot : -1, WantFloor = bIn ? DesignStorey : -1;
	if (WantLot == CutLot && WantFloor == CutFloor) return;
	CutLot = WantLot;
	CutFloor = WantFloor;
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (Mode && Mode->RoomsActor()) Mode->RoomsActor()->SetDesign(CutLot, CutFloor);
	if (PanelLot >= 0 && Draft) ApplyRemodel(PanelLot, Draft);
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
	const double A = FMath::Atan2(Out.Y, Out.X) + FMath::DegreesToRadians(DesignTurnDeg + 45.0 * ViewTurn) + ViewYawAdd;
	const bool bInside = DesignTab != TEXT("outside");
	const double Look = bInside ? DesignFloorM() : Hgt * 0.5;
	const double Dist = FMath::Clamp(DesignDist0() * ViewZoom, 2.0, 160.0);   // (play.js: from 2 m to 160 m)
	const double P = FMath::Clamp(DesignPitch0() + ViewPitchAdd, .04, 1.55);
	const FVector2D Mid = L.Centre() + ViewPan;
	const FVector Target(Mid.X * CmPerM, Mid.Y * CmPerM, (Look + 0.3) * CmPerM);
	const FVector Eye = Target + FVector(FMath::Cos(A) * FMath::Cos(P), FMath::Sin(A) * FMath::Cos(P), FMath::Sin(P)) * Dist * CmPerM;
	if (!ViewCam) ViewCam = GetWorld()->SpawnActor<ACameraActor>();
	if (!ViewCam) return;
	const FRotator Rot = (Target - Eye).Rotation();
	const FVector Left = -FRotationMatrix(Rot).GetUnitAxis(EAxis::Y) * Dist * CmPerM * 0.22;   // the card is on the left: the building to the right of the middle
	ViewCam->SetActorLocationAndRotation(Eye + Left, Rot);
	if (PC->GetViewTarget() != ViewCam) PC->SetViewTargetWithBlend(ViewCam, 0.6f);
	SyncCut();
}

bool ABrixLife::RemodelPay(FString& Said)
{
	TArray<FBrixCostLine> Lines;
	TArray<FString> Probs;
	int64 DesignTotal = 0;
	const int64 Total = WorkOf(Lines, Probs, DesignTotal);   // checked again, as the server will (BUILD-006)
	if (!Lines.Num()) { E->Why = TEXT("nothing has changed"); return false; }
	if (Probs.Num()) { E->Why = Probs[0]; return false; }
	const FString Name = Lots[PanelLot].Name;
	if (DesignTotal > 0 && !E->Spend(DesignTotal, TEXT("sys:builders"), TEXT("Remodel: ") + Name)) return false;
	if (Total - DesignTotal > 0 && !E->Spend(Total - DesignTotal, TEXT("sys:builders"), TEXT("Rooms and furniture: ") + Name)) return false;   // BUILD-023, FURN-052
	if (Draft != DraftBase) { RemodelsDone.Add(Lots[PanelLot].Id, Draft); DraftBase = Draft; ApplyRemodel(PanelLot, Draft); }
	for (const auto& P : PlanDraft)
		if (!(P.Value == PlanBase.FindRef(P.Key))) { PlansDone.FindOrAdd(Lots[PanelLot].Id).Add(P.Key, P.Value); PlanBase.Add(P.Key, P.Value); }
	FurnHeld = HeldNow();   // what was taken away is with the belongings now; what was placed is not
	for (const auto& L : LayDraft)
		if (!(L.Value == LayBase.FindRef(L.Key))) { LaysDone.FindOrAdd(Lots[PanelLot].Id).Add(L.Key, L.Value); LayBase.Add(L.Key, L.Value); }
	DraftUndo.Reset();
	PlanUndo.Reset();
	LayUndo.Reset();
	Said = TEXT("Done: ") + Name + (Total ? TEXT(", for ") + Format(Total) : FString()) + TEXT(".");
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
		TSharedRef<SWrapBox> W = UiWrap(CardWide() - 128.f, 4.f);
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
	if (CanFurnish(PanelLot)) UiAdd(Tabs, UiButton(TEXT("Furnish"), [this]() { DesignTab = TEXT("furnish"); DesignView(); Draw(); }, true, DesignTab != TEXT("furnish")));
	UiAdd(Tabs, UiButton(TEXT("Turn the view"), [this]() { ViewTurn = (ViewTurn + 1) % 8; DesignView(); }, true, true));
	Slot(Tabs);
	if (!bWhole) Slot(UiText(TEXT("You own part of this building: the outside needs all of it."), 13, UiDim));
	const TSharedPtr<FJsonObject> Sh = Draft->GetObjectField(TEXT("shell")), Op = Draft->GetObjectField(TEXT("openings"));
	FString Front;
	Sh->TryGetStringField(TEXT("side"), Front);
	if (DesignTab == TEXT("outside"))
	{
		TSharedRef<SWrapBox> Fs = UiWrap(CardWide(), 6.f);
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
		if (DesignTab == TEXT("furnish")) FurnishSection(Body);
		else {
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
		RoomsSection(Body);
		}
	}
	// the work, its price line by line, and what would stop it (BUILD-005, BUILD-006)
	TArray<FBrixCostLine> Lines;
	TArray<FString> Probs;
	int64 DesignTotal = 0;
	const int64 Total = WorkOf(Lines, Probs, DesignTotal);
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
	UiAdd(Bs, UiButton(TEXT("Undo"), [this]()
	{
		if (!DraftUndo.Num()) return;
		Draft = DraftUndo.Pop();
		const TMap<int32, FBrixPlan> Was = PlanUndo.Num() ? PlanUndo.Pop() : PlanDraft;
		for (const auto& P : PlanDraft) if (!(P.Value == Was.FindRef(P.Key))) ShowPlan(PanelLot, P.Key, Was.Contains(P.Key) ? Was[P.Key] : PlanBase.FindRef(P.Key));
		PlanDraft = Was;
		const TMap<int32, FBrixLayout> LWas = LayUndo.Num() ? LayUndo.Pop() : LayDraft;
		for (const auto& L : LayDraft) if (!(L.Value == LWas.FindRef(L.Key))) ShowLayout(PanelLot, L.Key, LWas.Contains(L.Key) ? LWas[L.Key] : LayBase.FindRef(L.Key));
		LayDraft = LWas;
		ApplyRemodel(PanelLot, Draft);
		Draw();
	}, DraftUndo.Num() > 0, true));
	bool bPlanned = false;
	for (const auto& P : PlanDraft) bPlanned |= !(P.Value == PlanBase.FindRef(P.Key));
	for (const auto& Ld : LayDraft) bPlanned |= !(Ld.Value == LayBase.FindRef(Ld.Key));
	UiAdd(Bs, UiButton(TEXT("Start again"), [this]()
	{
		PushUndo();
		Draft = DraftBase;
		for (const auto& P : PlanDraft) if (!(P.Value == PlanBase.FindRef(P.Key))) ShowPlan(PanelLot, P.Key, PlanBase.FindRef(P.Key));
		PlanDraft = PlanBase;
		for (const auto& L : LayDraft) if (!(L.Value == LayBase.FindRef(L.Key))) ShowLayout(PanelLot, L.Key, LayBase.FindRef(L.Key));
		LayDraft = LayBase;
		ApplyRemodel(PanelLot, Draft);
		Draw();
	}, Draft != DraftBase || bPlanned, true));
	Slot(Bs, 10);
	Slot(UiText(TEXT("Drag the view to look round; right drag, middle drag or Shift+drag slides it; the wheel comes nearer where the mouse is; double click looks at a spot."), 12, UiDim), 8);
}

// ---- the room plan (BUILD-021 to BUILD-023; life.js planNow(), editWall(), addWall(), fitWall(), grow(); play.js roomFor())

const FBrixLot* ABrixLife::CityLot(int32 Lot) const
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const FBrixCity* C = Mode ? Mode->CityData() : nullptr;
	if (!C) return nullptr;
	int32 k = 0;
	for (const FBrixBlock& B : C->Blocks) for (const FBrixLot& L : B.Lots) if (k++ == Lot) return &L;
	return nullptr;
}

FBrixRoomPlanRules ABrixLife::PlanRules() const
{
	FBrixRoomPlanRules R;
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (Mode && Mode->InsidesData() && Mode->InsidesData()->Builder.RoomPlan) R.Load(Mode->InsidesData()->Builder.RoomPlan->GetObjectField(TEXT("rules")));
	return R;
}

FBrixPlan ABrixLife::PlanNow(int32 Lot, int32 f) const   // a storey's walls as they are now: planned and paid for, else as the town built them
{
	if (Lots.IsValidIndex(Lot)) if (const TMap<int32, FBrixPlan>* P = PlansDone.Find(Lots[Lot].Id)) if (const FBrixPlan* Pq = P->Find(f)) return *Pq;
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const FBrixInsides* Ins = Mode ? Mode->InsidesData() : nullptr;
	const FBrixLot* L = CityLot(Lot);
	if (!Ins || !L || !Ins->Lots.IsValidIndex(Lot) || !Ins->Lots[Lot].IsSet() || !Ins->Lots[Lot]->Flats.Num()) return FBrixPlan();
	return BrixRoomPlan::PlanOf(BrixFlatWalls(*L, Ins->Lots[Lot].GetValue(), f, Ins->Rules));
}

bool ABrixLife::RoomFor(int32 Lot, int32 f, FBrixRoomSpec& S) const   // play.js roomFor()
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const FBrixInsides* Ins = Mode ? Mode->InsidesData() : nullptr;
	const FBrixLot* L = CityLot(Lot);
	if (!Ins || !L || !Ins->Lots.IsValidIndex(Lot) || !Ins->Lots[Lot].IsSet()) return false;
	const FBrixInside& In = Ins->Lots[Lot].GetValue();
	const double t = .35;
	S = FBrixRoomSpec();
	S.Room = FVector4f(L->Min.X + t, L->Min.Y + t, L->Max.X - t, L->Max.Y - t);
	FString Front = TEXT("s");
	const TSharedPtr<FJsonObject> D = DesignOf(Lot);
	if (D) D->GetObjectField(TEXT("shell"))->TryGetStringField(TEXT("side"), Front);
	const FVector2D Dir = Front == TEXT("n") ? FVector2D(0, -1) : Front == TEXT("s") ? FVector2D(0, 1) : Front == TEXT("w") ? FVector2D(-1, 0) : FVector2D(1, 0);   // play.js DOOR_DIR: out of the door
	if (f == 0)
	{
		const double w = In.DoorWidth / 2 + .2, Ln = 2.2;
		const FVector2D A = L->Door - Dir * Ln;
		S.Keep.Add(FVector4f(FMath::Min(L->Door.X, A.X) - (Dir.X ? 0 : w), FMath::Min(L->Door.Y, A.Y) - (Dir.Y ? 0 : w), FMath::Max(L->Door.X, A.X) + (Dir.X ? 0 : w), FMath::Max(L->Door.Y, A.Y) + (Dir.Y ? 0 : w)));
		S.Entries.Add(L->Door - Dir * .6);
	}
	if (In.Stair)
	{
		const FBrixStairGeo G = BrixStairOf(*In.Stair, Ins->Rules);
		const FVector2D A = G.P(G.U0 - .3f, FMath::Min(G.V0, G.V1) - .3f), B = G.P(G.U1 + .3f, FMath::Max(G.V0, G.V1) + .3f);
		S.Keep.Add(FVector4f(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)));
		if (f > 0) { const FBrixStairGeo::FFlight Fl = G.Flight(f - 1); S.Entries.Add(G.P(Fl.Ut + Fl.Dir * .5f, (Fl.Lane.X + Fl.Lane.Y) / 2)); }
		if (f < L->Floors - 1) S.Entries.Add(G.FootAt(f));
	}
	if (!S.Entries.Num()) S.Entries.Add(FVector2D((S.Room.X + S.Room.Z) / 2, (S.Room.Y + S.Room.W) / 2));
	const FBrixDoorRules DR = FBrixDoorRules::FromConfig();
	S.Ceiling = BrixCeilingUp(*L, f, DR) - BrixFloorUp(*L, f, DR);
	return true;
}

void ABrixLife::ShowPlan(int32 Lot, int32 f, const FBrixPlan& Plan)   // play.js showPlan(): the storey's walls drawn from it
{
	ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode || !Mode->RoomsActor() || !Mode->InsidesData()) return;
	const FBrixRoomPlanRules R = PlanRules();
	const TSharedPtr<FJsonObject> Kit = Mode->InsidesData()->Builder.Kit;
	TArray<FBrixPartition> Segs;
	TArray<TOptional<FVector3f>> Cols;
	for (const FBrixPlanWall& W : Plan)
	{
		TOptional<FVector3f> C;
		const TSharedPtr<FJsonObject>* Cl = nullptr;
		if (!W.Colour.IsEmpty() && Kit && Kit->GetObjectField(TEXT("colours"))->TryGetObjectField(W.Colour, Cl))   // mixed as the inside walls are (storeys.js wallColour())
		{
			const TArray<TSharedPtr<FJsonValue>>& P = (*Cl)->GetArrayField(TEXT("rgb"));
			const TSharedPtr<FJsonObject> Mix = Kit->GetObjectField(TEXT("inner_mix"));
			const TArray<TSharedPtr<FJsonValue>>& With = Mix->GetArrayField(TEXT("with"));
			const float Sh = (float)Mix->GetNumberField(TEXT("share"));
			FVector3f V;
			for (int32 k = 0; k < 3; ++k) V[k] = (float)P[k]->AsNumber() + ((float)With[k]->AsNumber() - (float)P[k]->AsNumber()) * Sh;
			C = V;
		}
		for (const FBrixPartition& S : BrixRoomPlan::SegsOf(R, { W })) { Segs.Add(S); Cols.Add(C); }
	}
	const bool bDrawn = Mode->RoomsActor()->SetPlan(Lot, f, Segs, Cols);
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %s floor %d drawn from its plan: %d walls, %d pieces%s"), Lots.IsValidIndex(Lot) ? *Lots[Lot].Id : TEXT("?"), f, Plan.Num(), Segs.Num(), bDrawn ? TEXT("") : TEXT(" (its rooms are not drawn)"));
}

void ABrixLife::PushUndo() { DraftUndo.Add(Draft); PlanUndo.Add(PlanDraft); LayUndo.Add(LayDraft); }

void ABrixLife::EditPlan(TFunction<void(FBrixPlan&)> Fn)
{
	FBrixPlan& P = PlanDraft.FindOrAdd(DesignStorey);
	PushUndo();
	Fn(P);
	ShowPlan(PanelLot, DesignStorey, P);
	Msg.Reset();
	Draw();
}

void ABrixLife::AddWall(const FString& Ax)   // a new 3 m wall across the middle of the storey (life.js addWall(): where the view looks)
{
	FBrixRoomSpec S;
	if (!RoomFor(PanelLot, DesignStorey, S)) return;
	const FBrixRoomPlanRules R = PlanRules();
	const bool bX = Ax == TEXT("x");
	const double Lo = bX ? S.Room.X : S.Room.Y, Hi = bX ? S.Room.Z : S.Room.W, Flo = bX ? S.Room.Y : S.Room.X, Fhi = bX ? S.Room.W : S.Room.Z;
	const double C = (Lo + Hi) / 2, Fix = BrixRoomPlan::Gridded(R, (Flo + Fhi) / 2), A0 = BrixRoomPlan::Gridded(R, FMath::Clamp(C - 1.5, Lo, Hi - 3)), A1 = BrixRoomPlan::Gridded(R, FMath::Min(Hi, A0 + 3));
	EditPlan([&](FBrixPlan& P)
	{
		int32 n = 1;
		for (const FBrixPlanWall& W : P) if (W.Id.StartsWith(TEXT("w"))) n = FMath::Max(n, FCString::Atoi(*W.Id.Mid(1)) + 1);
		FBrixPlanWall W;
		W.Id = FString::Printf(TEXT("w%d"), n); W.Ax = Ax; W.Fix = Fix; W.A0 = A0; W.A1 = A1;
		P.Add(W);
		WallSel = W.Id;
	});
}

void ABrixLife::RoomsSection(TSharedRef<SVerticalBox> Body)   // life.js remodelPanel(), the inside tab's Rooms
{
	auto Slot = [&](TSharedRef<SWidget> W, float Top = 6) { Body->AddSlot().AutoHeight().Padding(0, Top, 0, 0)[ W ]; };
	FBrixRoomSpec S;
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!RoomFor(PanelLot, DesignStorey, S) || !Mode || !Mode->RoomsActor() || !Mode->RoomsActor()->HasRooms(PanelLot))
	{
		Slot(UiText(TEXT("This building's rooms are not drawn inside yet, so its walls cannot be planned."), 12, UiDim));
		return;
	}
	if (!PlanDraft.Contains(DesignStorey)) { const FBrixPlan P = PlanNow(PanelLot, DesignStorey); PlanBase.Add(DesignStorey, P); PlanDraft.Add(DesignStorey, P); }
	const FBrixRoomPlanRules R = PlanRules();
	const FBrixPlan& Plan = PlanDraft[DesignStorey];
	Slot(UiHeading(TEXT("Rooms")), 0);
	Slot(UiText(TEXT("Click an inner wall to pick it, drag it to move it either way: its ends catch on the walls they meet. Fit between walls stretches it from wall to wall. Every room must be reached from the way in, and the stairs kept clear."), 12, UiDim));
	TSharedRef<SWrapBox> Ws = UiWrap(CardWide(), 6.f);
	for (const FBrixPlanWall& W : Plan)
	{
		const FString Id = W.Id;
		Ws->AddSlot()[ UiButton(FString::Printf(TEXT("%s %s, %.2f m"), W.Ax == TEXT("x") ? TEXT("E-W") : TEXT("N-S"), *W.Id, W.A1 - W.A0), [this, Id]() { WallSel = WallSel == Id ? FString() : Id; Draw(); }, true, WallSel != Id) ];
	}
	if (!Plan.Num()) Ws->AddSlot()[ UiText(TEXT("No inner walls on this storey."), 13, UiDim) ];
	Slot(Ws);
	if (const FBrixPlanWall* W = Plan.FindByPredicate([this](const FBrixPlanWall& X) { return X.Id == WallSel; }))
	{
		const FString Id = W->Id;
		const bool bX = W->Ax == TEXT("x");
		const double Lo = bX ? S.Room.X : S.Room.Y, Hi = bX ? S.Room.Z : S.Room.W;
		auto Edit = [this, Id](TFunction<void(FBrixPlanWall&)> Fn) { EditPlan([Id, Fn](FBrixPlan& P) { if (FBrixPlanWall* X = P.FindByPredicate([&](const FBrixPlanWall& Y) { return Y.Id == Id; })) Fn(*X); }); };
		TSharedRef<SWrapBox> A = UiWrap(CardWide(), 6.f);
		A->AddSlot()[ UiButton(W->Door ? TEXT("Close the doorway") : TEXT("Add a doorway"), [Edit, R]() { Edit([R](FBrixPlanWall& X) { if (X.Door) X.Door.Reset(); else X.Door = BrixRoomPlan::Gridded(R, (X.A0 + X.A1) / 2); }); }, true, true) ];
		A->AddSlot()[ UiButton(TEXT("Fit between walls"), [this, Edit, R, Lo, Hi, Id]()   // life.js fitWall(): to the nearest wall across, or the room's side, at each end
		{
			const FBrixPlan Now = PlanDraft[DesignStorey];
			Edit([&Now, R, Lo, Hi, Id](FBrixPlanWall& X)
			{
				TArray<double> St = { Lo, Hi };
				for (const FBrixPlanWall& Qw : Now) if (Qw.Id != Id && Qw.Ax != X.Ax && Qw.A0 - .01 <= X.Fix && X.Fix <= Qw.A1 + .01) St.Add(Qw.Fix);
				const double C = (X.A0 + X.A1) / 2;
				double A0 = -1e9, A1 = 1e9;
				for (const double v : St) { if (v < C - .3) A0 = FMath::Max(A0, v); if (v > C + .3) A1 = FMath::Min(A1, v); }
				X.A0 = FCString::Atod(*FString::Printf(TEXT("%.2f"), FMath::CeilToDouble(A0 / R.Grid - 1e-6) * R.Grid));
				X.A1 = FCString::Atod(*FString::Printf(TEXT("%.2f"), FMath::FloorToDouble(A1 / R.Grid + 1e-6) * R.Grid));
				if (X.Door) X.Door = BrixRoomPlan::Gridded(R, FMath::Clamp(*X.Door, X.A0 + R.DoorW / 2 + R.DoorMargin, X.A1 - R.DoorW / 2 - R.DoorMargin));
			});
		}, true, true) ];
		auto Grow = [Edit, R, Lo, Hi](double d) { Edit([R, Lo, Hi, d](FBrixPlanWall& X) { X.A0 = BrixRoomPlan::Gridded(R, FMath::Max(Lo, X.A0 - d / 2)); X.A1 = BrixRoomPlan::Gridded(R, FMath::Min(Hi, X.A1 + d / 2)); }); };
		A->AddSlot()[ UiButton(TEXT("Longer"), [Grow]() { Grow(.5); }, true, true) ];
		A->AddSlot()[ UiButton(TEXT("Shorter"), [Grow]() { Grow(-.5); }, W->A1 - W->A0 >= 1.1, true) ];
		auto Move = [Edit, R](double d) { Edit([R, d](FBrixPlanWall& X) { X.Fix = BrixRoomPlan::Gridded(R, X.Fix + d); if (X.Door) X.Door = BrixRoomPlan::Gridded(R, *X.Door); }); };   // life.js: dragging it either way
		A->AddSlot()[ UiButton(bX ? TEXT("Move north") : TEXT("Move west"), [Move]() { Move(-.25); }, true, true) ];
		A->AddSlot()[ UiButton(bX ? TEXT("Move south") : TEXT("Move east"), [Move]() { Move(.25); }, true, true) ];
		A->AddSlot()[ UiButton(TEXT("Take down"), [this, Id]() { EditPlan([Id](FBrixPlan& P) { P.RemoveAll([&](const FBrixPlanWall& X) { return X.Id == Id; }); }); WallSel.Reset(); Draw(); }, true, true) ];
		Slot(A);
		// its own colour (BUILD-022): the district's inside colours, or the storey's
		if (const TSharedPtr<FJsonObject> Kit = Mode->InsidesData()->Builder.Kit)
		{
			TSharedRef<SWrapBox> C = UiWrap(CardWide() - 128.f, 4.f);
			FBrixDesignOption None;
			None.Id = MakeShared<FJsonValueNull>(); None.Name = TEXT("As the storey");
			C->AddSlot()[ SwatchButton(None, W->Colour.IsEmpty(), [Edit]() { Edit([](FBrixPlanWall& X) { X.Colour.Reset(); }); }) ];
			const TSharedPtr<FJsonObject> Mix = Kit->GetObjectField(TEXT("inner_mix"));
			const TArray<TSharedPtr<FJsonValue>>& With = Mix->GetArrayField(TEXT("with"));
			const float Sh = (float)Mix->GetNumberField(TEXT("share"));
			for (const TSharedPtr<FJsonValue>& V : Kit->GetArrayField(TEXT("plaster")))
			{
				const FString Cid = V->AsString();
				const TArray<TSharedPtr<FJsonValue>>& P = Kit->GetObjectField(TEXT("colours"))->GetObjectField(Cid)->GetArrayField(TEXT("rgb"));
				FBrixDesignOption O;
				O.Id = V; O.Name = Kit->GetObjectField(TEXT("colours"))->GetObjectField(Cid)->GetStringField(TEXT("name"));
				FVector3f Rgb;
				for (int32 k = 0; k < 3; ++k) Rgb[k] = (float)P[k]->AsNumber() + ((float)With[k]->AsNumber() - (float)P[k]->AsNumber()) * Sh;
				O.Rgb = Rgb;
				C->AddSlot()[ SwatchButton(O, W->Colour == Cid, [Edit, Cid]() { Edit([Cid](FBrixPlanWall& X) { X.Colour = Cid; }); }) ];
			}
			Slot(SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth().Padding(0, 6, 10, 0)[ SNew(SBox).WidthOverride(118.f)[ UiText(TEXT("Its colour"), 13, UiDim) ] ] + SHorizontalBox::Slot().FillWidth(1)[ C ], 4);
		}
	}
	TSharedRef<SHorizontalBox> Add = UiRow();
	UiAdd(Add, UiButton(TEXT("Add a wall east to west"), [this]() { AddWall(TEXT("x")); }, true, true));
	UiAdd(Add, UiButton(TEXT("Add a wall north to south"), [this]() { AddWall(TEXT("z")); }, true, true));
	Slot(Add);
}

// The work of the whole remodel: the design's (outside and finishes) and each storey's room plan, line by line, and what stops it
int64 ABrixLife::WorkOf(TArray<FBrixCostLine>& Lines, TArray<FString>& Probs, int64& DesignTotal) const
{
	Lines.Reset();
	Probs.Reset();
	DesignTotal = 0;
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Draft || !Mode || !Mode->InsidesData() || !DesignLots.IsValidIndex(PanelLot)) return 0;
	const FBrixBuilderRules& R = Mode->InsidesData()->Builder;
	const FBrixDesignLot& L = DesignLots[PanelLot];
	bool bWhole = false;
	TArray<int32> Mine;
	Holding(PanelLot, bWhole, Mine);
	if (Draft != DraftBase)
	{
		DesignTotal = BrixBuilder::Cost(R, L, DraftBase, Draft, Lines);
		Probs = BrixBuilder::Check(R, L, DraftBase, Draft, bWhole, Mine, {});
		if (!Lines.Num()) Probs.Reset();
	}
	int64 Total = DesignTotal;
	const FBrixRoomPlanRules PR = PlanRules();
	TArray<FString> Colours;
	if (R.Kit) for (const auto& V : R.Kit->GetArrayField(TEXT("plaster"))) Colours.Add(V->AsString());
	TArray<int32> Fl;
	PlanDraft.GetKeys(Fl);
	Fl.Sort();
	for (const int32 f : Fl)
	{
		const FBrixPlan& A = PlanDraft[f];
		const FBrixPlan& B = PlanBase.FindRef(f);
		if (A == B) continue;
		FBrixRoomSpec S;
		if (!RoomFor(PanelLot, f, S)) continue;
		const FString Tag = Mine.Num() > 1 ? (f ? FString::Printf(TEXT(" (floor %d)"), f) : FString(TEXT(" (ground)"))) : FString();
		TArray<FBrixPlanLine> Pl;
		Total += BrixRoomPlan::Cost(PR, B, A, S.Ceiling, Pl);
		for (const FBrixPlanLine& X : Pl) Lines.Add({ X.What + Tag, X.Qty, X.Chips });
		for (const FString& X : BrixRoomPlan::Check(PR, S, Colours, B, A, {})) Probs.Add(X + Tag);
		if (!bWhole && !Mine.Contains(f)) Probs.Add(FString::Printf(TEXT("floor %d is not yours to change"), f));
	}
	// the storeys' furniture (FURN-051, FURN-052): bought pieces, placed belongings, moved and taken away
	const FBrixFurnishRulesD FR = FurnRules();
	const FBrixLayout Pool = PoolOf();
	TArray<int32> Lf;
	LayDraft.GetKeys(Lf);
	Lf.Sort();
	for (const int32 f : Lf)
	{
		const FBrixLayout& A = LayDraft[f];
		const FBrixLayout& B = LayBase.FindRef(f);
		if (A == B) continue;
		FBrixRoomSpec S;
		if (!RoomFor(PanelLot, f, S)) continue;
		const FString Tag = Mine.Num() > 1 ? (f ? FString::Printf(TEXT(" (floor %d)"), f) : FString(TEXT(" (ground)"))) : FString();
		TArray<FBrixFurnLine> Fln;
		Total += BrixFurnishPlan::Cost(FR, B, A, Pool, Fln);
		for (const FBrixFurnLine& X : Fln) Lines.Add({ X.What + Tag, X.Qty, X.Chips });
		for (const FString& X : BrixFurnishPlan::Check(FR, S.Room, S.Keep, S.Ceiling, B, A, {}, Pool)) Probs.Add(X + Tag);
		if (!bWhole && !Mine.Contains(f)) Probs.Add(FString::Printf(TEXT("floor %d is not yours to change"), f));
	}
	if (Lines.Num() && Total > E->Funds()) Probs.Add(TEXT("it costs ") + Format(Total) + TEXT(" and you have ") + Format(E->Funds()));
	return Total;
}

// ---- furnishing a storey in the designer (FURN-051, FURN-052, FURN-055; life.js remodelPanel()'s Furnish tab, editPiece(),
// addPiece(), placeHeld(), heldNow(), poolOf(); INV-001: what is taken away goes to the belongings, and is placed from there)

FBrixFurnishRulesD ABrixLife::FurnRules() const
{
	FBrixFurnishRulesD F;
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (Mode && Mode->InsidesData() && Mode->InsidesData()->Builder.Furnish) F.Load(Mode->InsidesData()->Builder.Furnish->GetObjectField(TEXT("rules")));
	return F;
}

bool ABrixLife::CanFurnish(int32 Lot) const   // life.js canFurnish(): furnished at its places (not a planned home, not the outlet)
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	FBrixLayout L;
	return Mode && Mode->RoomsActor() && Mode->RoomsActor()->TownLayout(Lot, 0, L);
}

FBrixLayout ABrixLife::LayNow(int32 Lot, int32 f) const   // a storey as it is now: as furnished and paid for, else as the town furnished it
{
	if (Lots.IsValidIndex(Lot)) if (const TMap<int32, FBrixLayout>* P = LaysDone.Find(Lots[Lot].Id)) if (const FBrixLayout* Lq = P->Find(f)) return *Lq;
	FBrixLayout L;
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (Mode && Mode->RoomsActor()) Mode->RoomsActor()->TownLayout(Lot, f, L);
	return L;
}

void ABrixLife::ShowLayout(int32 Lot, int32 f, const FBrixLayout& Lay)
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (Mode && Mode->RoomsActor()) Mode->RoomsActor()->SetLayout(Lot, f, Lay);
}

FBrixLayout ABrixLife::HeldNow() const   // the belongings as the drafts leave them: less what was placed, with what was taken away
{
	FBrixLayout Held = FurnHeld;
	TArray<int32> Fl;
	LayDraft.GetKeys(Fl);
	Fl.Sort();
	for (const int32 f : Fl) Held = BrixFurnishPlan::Stow(LayBase.FindRef(f), LayDraft[f], Held, Lots.IsValidIndex(PanelLot) ? Lots[PanelLot].Id : FString(), f);
	return Held;
}

FBrixLayout ABrixLife::PoolOf() const   // what may be placed: the belongings, and whatever the drafts took away (life.js poolOf())
{
	FBrixLayout Pool = FurnHeld;
	for (const FBrixLayoutPiece& P : HeldNow()) if (!Pool.ContainsByPredicate([&](const FBrixLayoutPiece& X) { return X.Id == P.Id; })) Pool.Add(P);
	return Pool;
}

void ABrixLife::EditLayout(TFunction<void(FBrixLayout&)> Fn)
{
	if (!LayDraft.Contains(DesignStorey)) { const FBrixLayout L = LayNow(PanelLot, DesignStorey); LayBase.Add(DesignStorey, L); LayDraft.Add(DesignStorey, L); }
	PushUndo();
	Fn(LayDraft[DesignStorey]);
	ShowLayout(PanelLot, DesignStorey, LayDraft[DesignStorey]);
	Msg.Reset();
	Draw();
}

void ABrixLife::FurnishSection(TSharedRef<SVerticalBox> Body)
{
	auto Slot = [&](TSharedRef<SWidget> W, float Top = 6) { Body->AddSlot().AutoHeight().Padding(0, Top, 0, 0)[ W ]; };
	FBrixRoomSpec S;
	if (!CanFurnish(PanelLot) || !RoomFor(PanelLot, DesignStorey, S))
	{
		Slot(UiText(TEXT("This kind of building is not furnished in the designer: the planned homes keep their plans, and the outlet its hall."), 12, UiDim));
		return;
	}
	if (!LayDraft.Contains(DesignStorey)) { const FBrixLayout L = LayNow(PanelLot, DesignStorey); LayBase.Add(DesignStorey, L); LayDraft.Add(DesignStorey, L); }
	const FBrixFurnishRulesD F = FurnRules();
	const FBrixLayout& Lay = LayDraft[DesignStorey];
	Slot(UiText(TEXT("Click a piece to pick it, drag it across the floor to move it. R turns it (Shift+R the other way), Delete puts it with your belongings."), 12, UiDim), 0);
	if (const FBrixLayoutPiece* Sel = Lay.FindByPredicate([this](const FBrixLayoutPiece& X) { return X.Id == PieceSel; }))
	{
		const FString Id = Sel->Id;
		auto Edit = [this, Id](TFunction<void(FBrixLayoutPiece&)> Fn) { EditLayout([Id, Fn](FBrixLayout& L) { if (FBrixLayoutPiece* X = L.FindByPredicate([&](const FBrixLayoutPiece& Y) { return Y.Id == Id; })) Fn(*X); }); };
		const double Turn = F.Turn;
		TSharedRef<SWrapBox> A = UiWrap(CardWide(), 6.f);
		A->AddSlot()[ UiButton(TEXT("Turn left"), [Edit, Turn]() { Edit([Turn](FBrixLayoutPiece& X) { X.Yaw = FMath::Fmod(FMath::Fmod(X.Yaw + Turn, 360.0) + 360.0, 360.0); }); }, true, true) ];
		A->AddSlot()[ UiButton(TEXT("Turn right"), [Edit, Turn]() { Edit([Turn](FBrixLayoutPiece& X) { X.Yaw = FMath::Fmod(FMath::Fmod(X.Yaw - Turn, 360.0) + 360.0, 360.0); }); }, true, true) ];
		A->AddSlot()[ UiButton(TEXT("To your belongings"), [this, Id]() { PieceSel.Reset(); EditLayout([Id](FBrixLayout& L) { L.RemoveAll([&](const FBrixLayoutPiece& X) { return X.Id == Id; }); }); }, true, true) ];
		A->AddSlot()[ UiButton(TEXT("Put down"), [this]() { PieceSel.Reset(); Draw(); }, true, true) ];
		Slot(UiText(Sel->Name + (StoreyBad().Contains(Id) ? TEXT(" · can't stand here") : TEXT("")), 14, UiCream, true), 8);
		Slot(A, 2);
	}
	// the pieces by name too, for a piece that stands behind another (the slice has only the picture)
	Slot(UiButton(bPieceList ? FString(TEXT("Hide the list of pieces")) : FString::Printf(TEXT("List the %d pieces by name"), Lay.Num()), [this]() { bPieceList = !bPieceList; Draw(); }, Lay.Num() > 0, true));
	if (bPieceList)
	{
		TSharedRef<SWrapBox> Ps = UiWrap(CardWide(), 6.f);
		TMap<FString, int32> Count, Seen;   // the town's pieces have no names of their own: numbered where several share one
		for (const FBrixLayoutPiece& P : Lay) ++Count.FindOrAdd(P.Name);
		for (const FBrixLayoutPiece& P : Lay)
		{
			const FString Id = P.Id;
			const int32 n = ++Seen.FindOrAdd(P.Name);
			Ps->AddSlot()[ UiButton(Count[P.Name] > 1 ? FString::Printf(TEXT("%s %d"), *P.Name, n) : P.Name, [this, Id]() { PieceSel = PieceSel == Id ? FString() : Id; Draw(); }, true, PieceSel != Id) ];
		}
		Slot(Ps);
	}
	// a new piece, or one from the belongings, goes where the view looks, on the grid, inside the room (life.js addPiece(), placeHeld())
	const FVector2D Looks = Lots[PanelLot].Centre() + ViewPan;
	auto Where = [F, S, Looks](double W, double D)
	{
		return FVector2D(BrixFurnishPlan::Snap(F, FMath::Clamp(Looks.X, FMath::Min<double>(S.Room.X + W / 2, S.Room.Z - W / 2), FMath::Max<double>(S.Room.X + W / 2, S.Room.Z - W / 2))),
			BrixFurnishPlan::Snap(F, FMath::Clamp(Looks.Y, FMath::Min<double>(S.Room.Y + D / 2, S.Room.W - D / 2), FMath::Max<double>(S.Room.Y + D / 2, S.Room.W - D / 2))));
	};
	const FBrixLayout Held = HeldNow();
	if (Held.Num())
	{
		Slot(UiHeading(TEXT("From your belongings")), 0);
		for (const FBrixLayoutPiece& B : Held)
		{
			TSharedRef<SHorizontalBox> R = UiRow();
			R->AddSlot().FillWidth(1).VAlign(VAlign_Center)[ UiText(FString::Printf(TEXT("%s  ·  %.2f × %.2f m"), *B.Name, B.W, B.D), 13) ];
			const FBrixLayoutPiece Bc = B;
			const FVector2D At = Where(B.W, B.D);
			UiAdd(R, UiButton(TEXT("Place"), [this, Bc, At]() { PieceSel = Bc.Id; EditLayout([&](FBrixLayout& L) { FBrixLayoutPiece Q = Bc; Q.X = At.X; Q.Z = At.Y; Q.Yaw = 0; L.Add(Q); }); }, true, true));
			Slot(R, 2);
		}
	}
	Slot(UiHeading(TEXT("Add a piece")), 0);
	if (AddRoom.IsEmpty() && F.Rooms.Num()) AddRoom = F.Rooms[0].Key;
	if (AddLine.IsEmpty() && F.Lines.Num()) AddLine = F.Lines[0];
	TSharedRef<SWrapBox> Rk = UiWrap(CardWide(), 6.f);
	for (const auto& R : F.Rooms) { const FString K = R.Key; Rk->AddSlot()[ UiButton(K.Left(1).ToUpper() + K.Mid(1), [this, K]() { AddRoom = K; Draw(); }, true, AddRoom != K) ]; }
	Slot(Rk);
	TSharedRef<SWrapBox> Lk = UiWrap(CardWide(), 6.f);
	Lk->AddSlot().VAlign(VAlign_Center)[ UiText(TEXT("Line"), 13, UiDim) ];
	for (const FString& Ln : F.Lines) Lk->AddSlot()[ UiButton(Ln == TEXT("oldtown") ? FString(TEXT("Old Town")) : Ln.Left(1).ToUpper() + Ln.Mid(1), [this, Ln]() { AddLine = Ln; Draw(); }, true, AddLine != Ln) ];
	Slot(Lk, 4);
	// the pieces as objects in the chosen line, small cells (UI-020, layout D: the designer's card is narrow)
	TArray<FWare> Things;
	if (const auto* Room = F.Rooms.FindByPredicate([this](const TPair<FString, TArray<FString>>& R) { return R.Key == AddRoom; }))
		for (const FString& K : Room->Value)
		{
			const FBrixFurnishRulesD::FPiece* Pc = F.Pieces.Find(K);
			if (!Pc || !Pc->From.IsEmpty()) continue;   // sold at the mall's outlets, not here (FURN-064)
			FWare W;
			W.Id = K; W.Name = Pc->Name; W.Price = Pc->Price; W.Goes = TEXT("place"); W.Line = AddLine; W.Size = Pc->Size;   // paid with the rest of the work: never too dear to add
			Things.Add(W);
		}
	FGridOpts O;
	O.Id = TEXT("furnish");
	O.Verb = TEXT("Add");
	O.Width = CardWide() - 2.f;
	O.bNarrow = O.Width < BuyMenu.NarrowPx;
	O.Buy = [this, F, Where](const FWare& W, FString& Said)
	{
		const FBrixFurnishRulesD::FPiece* Pc = F.Pieces.Find(W.Id);
		if (!Pc) { E->Why = TEXT("there is no such piece"); return false; }
		const FVector2D At = Where(Pc->Size.X, Pc->Size.Y);
		FString NewId;
		const FBrixLayout Pool = PoolOf();
		const FString Here = FString::Printf(TEXT("b:%s:%d:"), *Lots[PanelLot].Id, DesignStorey);
		EditLayout([&](FBrixLayout& L)
		{
			int32 n = 1;   // (never an id a belonging from this storey has)
			for (const FBrixLayoutPiece& X : L) if (X.Id.StartsWith(TEXT("k"))) n = FMath::Max(n, FCString::Atoi(*X.Id.Mid(1)) + 1);
			for (const FBrixLayoutPiece& X : Pool) if (X.Id.StartsWith(Here + TEXT("k"))) n = FMath::Max(n, FCString::Atoi(*X.Id.Mid(Here.Len() + 1)) + 1);
			NewId = FString::Printf(TEXT("k%d"), n);
			L.Add(BrixFurnishPlan::KitPiece(F, W.Id, NewId, At.X, At.Y, 0, AddLine));
			PieceSel = NewId;
		});
		Said.Reset();
		return true;
	};
	Slot(Grid(Things, O), 4);
}

// ---- the mouse in the designer (BUILD-020, BUILD-021, FURN-055; life.js pointer(), designMarks(), storeyBad(), designKey();
// the picking and the dragging themselves are BrixWorld's BrixDesignPick)

bool ABrixLife::DesignRay(const FVector2D& Px, FBrixRay& Out) const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	FVector O, D;
	if (!PC || !UGameplayStatics::DeprojectScreenToWorld(PC, Px, O, D)) return false;
	Out.O = FVector3d(O.X / CmPerM, O.Z / CmPerM, O.Y / CmPerM);   // the data's frame: x east, y up, z south
	Out.D = FVector3d(D.X, D.Z, D.Y);
	return true;
}

TSet<FString> ABrixLife::StoreyBad() const
{
	TSet<FString> Bad;
	const FBrixLayout* A = LayDraft.Find(DesignStorey);
	const FBrixLayout* B = LayBase.Find(DesignStorey);
	FBrixRoomSpec S;
	if (!A || !B || *A == *B || !RoomFor(PanelLot, DesignStorey, S)) return Bad;
	const FBrixFurnishRulesD F = FurnRules();
	TSet<FString> Old;
	for (const TPair<FString, FString>& Pr : BrixFurnishPlan::Problems(F, S.Room, S.Keep, S.Ceiling, *B)) Old.Add(Pr.Key);
	for (const TPair<FString, FString>& Pr : BrixFurnishPlan::Problems(F, S.Room, S.Keep, S.Ceiling, *A))
	{
		if (Old.Contains(Pr.Key)) continue;
		TArray<FString> Ids;
		Pr.Key.ParseIntoArray(Ids, TEXT("|"), false);
		for (int32 k = 0; k + 1 < Ids.Num(); ++k) Bad.Add(Ids[k]);   // the key is the pieces' ids, then what is wrong
	}
	return Bad;
}

void ABrixLife::DesignMarks()
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode || !Mode->RoomsActor()) return;
	TArray<FTransform> At;
	TArray<FLinearColor> Col;
	// lit in its own colour, as the slice's signal lamp: what is picked the lantern's orange, what cannot stand there red,
	// what is under the mouse pale (deeper than the slice's, to read on a floor the sun is on)
	const FLinearColor Picked(.9f, .3f, .03f), Wrong(.9f, .05f, .03f), Under(.5f, .5f, .45f);
	auto Put = [&](const FBrixLayoutPiece& P, double F, bool bSel, bool bBad)
	{
		for (const BrixDesignPick::FMarkBox& M : BrixDesignPick::Marks(P, F, bSel))
		{
			// the slice turns a box by M4.ry(yaw): its x to (cos, -sin) on the plan, which is Unreal's yaw -yaw
			At.Add(FTransform(FQuat(FVector::UpVector, -FMath::DegreesToRadians(M.YawDeg)), FVector(M.At.X, M.At.Z, M.At.Y + M.Size.Y / 2) * CmPerM, FVector(M.Size.X, M.Size.Z, M.Size.Y)));
			Col.Add(bBad ? Wrong : bSel ? Picked : Under);
		}
	};
	if (Panel == TEXT("remodel") && Draft && DesignTab != TEXT("outside"))
	{
		const double F = DesignFloorM();
		if (DesignTab == TEXT("inside"))
		{
			const FBrixRoomPlanRules PR = PlanRules();
			if (const FBrixPlan* Plan = PlanDraft.Find(DesignStorey))
				for (const FBrixPlanWall& W : *Plan)
					if (W.Id == WallSel || (W.Id == WallHover && Grab.Kind == 0)) Put(BrixDesignPick::WallAsPiece(PR, W), F, W.Id == WallSel, false);
		}
		else if (const FBrixLayout* Lay = LayDraft.Find(DesignStorey))
		{
			const TSet<FString> Bad = StoreyBad();
			for (const FBrixLayoutPiece& P : *Lay) if (P.Id == PieceSel || (P.Id == PieceHover && Grab.Kind == 0)) Put(P, F, P.Id == PieceSel, Bad.Contains(P.Id));
			for (const FBrixLayoutPiece& P : *Lay) if (P.Id != PieceSel && Bad.Contains(P.Id)) Put(P, F, false, true);
		}
	}
	Mode->RoomsActor()->SetMarks(At, Col);
}

void ABrixLife::DesignPointer(EPoint Type, const FVector2D& Px, int32 Button, bool bShift, const FVector2D& DeltaPx, float Wheel, bool bHeld)
{
	if (Panel != TEXT("remodel") || bMenu || !Draft || !Lots.IsValidIndex(PanelLot)) return;
	const FBrixEconLot& L = Lots[PanelLot];
	const double Beyond = 20;   // the view goes 20 m beyond the building at most
	const bool bInside = DesignTab != TEXT("outside");
	const double Ground = bInside ? DesignFloorM() : 0.0;
	FBrixRay Ray;
	const bool bRay = DesignRay(Px, Ray);
	const TOptional<FVector2D> Hit = bRay ? BrixDesignPick::FloorHit(Ray, Ground) : TOptional<FVector2D>();
	const FVector2D Mid = L.Centre() + ViewPan;
	auto Aim = [&](const FVector2D& To)
	{
		ViewPan = FVector2D(FMath::Clamp(To.X, L.Min.X - Beyond, L.Max.X + Beyond), FMath::Clamp(To.Y, L.Min.Y - Beyond, L.Max.Y + Beyond)) - L.Centre();
		DesignView();
	};
	const bool bWalls = DesignTab == TEXT("inside") && PlanDraft.Contains(DesignStorey);
	const bool bPieces = DesignTab == TEXT("furnish") && LayDraft.Contains(DesignStorey);
	auto WallUnder = [&]() { return bRay ? BrixDesignPick::PickWall(PlanRules(), PlanDraft[DesignStorey], Ground, DesignCutM, Ray) : (int32)INDEX_NONE; };
	auto PieceUnder = [&]() { return bRay ? BrixDesignPick::PickPiece(LayDraft[DesignStorey], Ground, Ray) : (int32)INDEX_NONE; };
	if (Type == EPoint::Wheel)   // nearer or further, towards the point under the mouse
	{
		const double Base = FMath::Max(DesignDist0(), .1), Was = FMath::Clamp(Base * ViewZoom, 2.0, 160.0), Now = FMath::Clamp(Was * (Wheel < 0 ? 1.15 : 1 / 1.15), 2.0, 160.0), k = Now / Was;
		ViewZoom = Now / Base;
		if (Hit && Hit->X > L.Min.X - Beyond && Hit->X < L.Max.X + Beyond && Hit->Y > L.Min.Y - Beyond && Hit->Y < L.Max.Y + Beyond) Aim(*Hit + (Mid - *Hit) * k);   // (not towards the far distance)
		else DesignView();
		return;
	}
	if (Type == EPoint::Twice) { if (Hit) Aim(*Hit); return; }   // a double click looks at that spot
	if (Type == EPoint::Down)
	{
		Grab = FGrab();
		if (Button == 2 || Button == 1 || (Button == 0 && bShift)) { Grab.Kind = 2; return; }   // the right or middle button, or Shift, slides the view
		Grab.Kind = 1;   // on open ground a drag turns the view
		if (Button != 0) return;
		if (bWalls)   // an inner wall: pick it, and drag it across the floor
		{
			const FBrixPlan& Plan = PlanDraft[DesignStorey];
			const int32 i = WallUnder();
			const FString Was = WallSel;
			WallSel = Plan.IsValidIndex(i) ? Plan[i].Id : FString();
			if (Plan.IsValidIndex(i) && Hit) { const BrixDesignPick::FWallGrab G = BrixDesignPick::GrabWall(Plan[i], *Hit); Grab.Kind = 4; Grab.Id = Plan[i].Id; Grab.Off = FVector2D(G.Off, 0); Grab.OffA = G.OffA; }
			if (Was != WallSel) Draw(); else DesignMarks();
		}
		else if (bPieces)   // a piece: pick it, and drag it across the floor
		{
			const FBrixLayout& Lay = LayDraft[DesignStorey];
			const int32 i = PieceUnder();
			const FString Was = PieceSel;
			PieceSel = Lay.IsValidIndex(i) ? Lay[i].Id : FString();
			if (Lay.IsValidIndex(i) && Hit) { Grab.Kind = 3; Grab.Id = Lay[i].Id; Grab.Off = BrixDesignPick::GrabPiece(Lay[i], *Hit); }
			if (Was != PieceSel) Draw(); else DesignMarks();
		}
		return;
	}
	if (Type == EPoint::Move)
	{
		if (!bHeld || Grab.Kind == 0)   // hovering: what is under the mouse is outlined, and the mouse says it can be moved
		{
			FString Wall, Piece;
			if (bWalls) { const int32 i = WallUnder(); if (PlanDraft[DesignStorey].IsValidIndex(i)) Wall = PlanDraft[DesignStorey][i].Id; }
			else if (bPieces) { const int32 i = PieceUnder(); if (LayDraft[DesignStorey].IsValidIndex(i)) Piece = LayDraft[DesignStorey][i].Id; }
			if (Wall != WallHover || Piece != PieceHover) { WallHover = Wall; PieceHover = Piece; DesignMarks(); }
			return;
		}
		if (Grab.Kind == 4)   // a wall follows the mouse, across and along, on the grid, inside the room; its ends catch on walls
		{
			FBrixRoomSpec S;
			FBrixPlan* Plan = PlanDraft.Find(DesignStorey);
			FBrixPlanWall* W = Plan ? Plan->FindByPredicate([&](const FBrixPlanWall& X) { return X.Id == Grab.Id; }) : nullptr;
			if (!Hit || !W || !RoomFor(PanelLot, DesignStorey, S)) return;
			const FBrixPlanWall To = BrixDesignPick::DragWall(PlanRules(), *Plan, *W, S.Room, *Hit, { Grab.Off.X, Grab.OffA });
			if (To.Fix == W->Fix && To.A0 == W->A0) return;
			if (!Grab.bMoved) { PushUndo(); Grab.bMoved = true; }
			*W = To;
			ShowPlan(PanelLot, DesignStorey, *Plan);
			Msg.Reset();
			DesignMarks();
		}
		else if (Grab.Kind == 3)   // a piece follows the mouse across the floor, on the grid
		{
			FBrixLayout* Lay = LayDraft.Find(DesignStorey);
			FBrixLayoutPiece* P = Lay ? Lay->FindByPredicate([&](const FBrixLayoutPiece& X) { return X.Id == Grab.Id; }) : nullptr;
			if (!Hit || !P) return;
			const FVector2D To = BrixDesignPick::DragPiece(FurnRules(), *Hit, Grab.Off);
			if (To.X == P->X && To.Y == P->Z) return;
			if (!Grab.bMoved) { PushUndo(); Grab.bMoved = true; }
			P->X = To.X; P->Z = To.Y;
			ShowLayout(PanelLot, DesignStorey, *Lay);
			Msg.Reset();
			DesignMarks();
		}
		else if (Grab.Kind == 2 && ViewCam)   // the view slides: the ground goes with the mouse
		{
			const FRotationMatrix M(ViewCam->GetActorRotation());
			const FVector2D Fwd = FVector2D(M.GetUnitAxis(EAxis::X)).GetSafeNormal(), Right = FVector2D(M.GetUnitAxis(EAxis::Y)).GetSafeNormal();
			const double k = FMath::Clamp(DesignDist0() * ViewZoom, 2.0, 160.0) * .0016;
			Aim(Mid - Right * DeltaPx.X * k + Fwd * DeltaPx.Y * k);
		}
		else if (Grab.Kind == 1)   // the view turns round the building, and tips
		{
			ViewYawAdd += DeltaPx.X * .006;
			const double P0 = DesignPitch0();
			ViewPitchAdd = FMath::Clamp(P0 + ViewPitchAdd + DeltaPx.Y * .005, .04, 1.55) - P0;
			DesignView();
		}
		return;
	}
	if (Type == EPoint::Up)
	{
		const bool bMoved = Grab.bMoved;
		Grab = FGrab();
		if (bMoved) Draw(); else DesignMarks();   // the work and what stops it, now that it is put down
	}
}

void ABrixLife::DesignKeys()   // life.js designKey()
{
	if (DesignTab != TEXT("furnish") || PieceSel.IsEmpty() || bMenu || Grab.Kind >= 3) return;
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const FBrixLayout* Lay = LayDraft.Find(DesignStorey);
	if (!PC || !Lay || !Lay->ContainsByPredicate([this](const FBrixLayoutPiece& X) { return X.Id == PieceSel; })) return;
	const FString Id = PieceSel;
	if (PC->WasInputKeyJustPressed(EKeys::R))
	{
		const double Turn = FurnRules().Turn * (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift) ? -1 : 1);
		EditLayout([&](FBrixLayout& L) { if (FBrixLayoutPiece* X = L.FindByPredicate([&](const FBrixLayoutPiece& Y) { return Y.Id == Id; })) X->Yaw = FMath::Fmod(FMath::Fmod(X->Yaw + Turn, 360.0) + 360.0, 360.0); });
	}
	else if (PC->WasInputKeyJustPressed(EKeys::Delete) || PC->WasInputKeyJustPressed(EKeys::BackSpace))
	{
		PieceSel.Reset();
		EditLayout([&](FBrixLayout& L) { L.RemoveAll([&](const FBrixLayoutPiece& X) { return X.Id == Id; }); });
	}
}

// For a test shot: a mouse's press, move or release at a point of the picture, sent through the interface as the mouse's own
// would be, so the pad behind the card is what takes it. False when the interface cannot be reached.
bool ABrixLife::SlatePoint(EPoint Type, const FVector2D& Px)
{
	if (!FSlateApplication::IsInitialized() || !DesignPad) return false;
	const FGeometry G = DesignPad->GetTickSpaceGeometry();
	if (G.GetLocalSize().X < 1.f) return false;
	const FVector2f Abs2 = G.LocalToAbsolute(FVector2f(Px / FMath::Max(G.Scale, .01f)));
	const FVector2D Abs(Abs2.X, Abs2.Y);
	static FVector2D Last = Abs;
	TSet<FKey> Down;
	if (Type != EPoint::Up) Down.Add(EKeys::LeftMouseButton);
	const FPointerEvent Ev(0, Abs, Last, Down, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
	Last = Abs;
	FSlateApplication& App = FSlateApplication::Get();
	if (Type == EPoint::Down) App.ProcessMouseButtonDownEvent(nullptr, Ev);
	else if (Type == EPoint::Move) App.ProcessMouseMoveEvent(Ev);
	else if (Type == EPoint::Up) App.ProcessMouseButtonUpEvent(Ev);
	else return false;
	return true;
}

// ---- the furniture outlet's tills (FURN-022 to FURN-029; life.js storePanel(), storeFrame(), standIn(), unbox(); the rules are
// BrixWorld's BrixStore). B in the furniture outlet opens its till: the showroom's pieces by room and line as objects (each
// with what its tag says), a basket where each piece is taken now (free, to the belongings) or delivered to a home of the
// player's in a slot (in its boxes, or put together by a fitter), and the deliveries: where each van is, and the flat packs
// waiting to be put together. With the outlet's catalogue, B at a home of the player's orders from there, for delivery only.

void ABrixLife::StartStore(const TSharedPtr<FJsonObject>& Saved)
{
	Store.Reset();
	OutletLotIx = Lots.IndexOfByPredicate([](const FBrixEconLot& L) { return L.Kind == TEXT("furniture"); });
	const TSharedPtr<FJsonObject>* Fu = nullptr;
	const TSharedPtr<FJsonObject>* Fg = nullptr;
	FBrixStoreRules R;
	FBrixFurnishRulesD F;
	if (!E || !Data.Json || !Data.Json->TryGetObjectField(TEXT("furniture"), Fu) || !Data.Json->TryGetObjectField(TEXT("furnishing"), Fg) || !R.Load((*Fu)->GetObjectField(TEXT("rules"))) || !F.Load(*Fg))
	{
		UE_LOG(LogTemp, Warning, TEXT("Brix Town: no furniture store (life.json has no furniture rules: run Tools/worldgen/build_play.py)"));
		return;
	}
	Store = MakeUnique<FBrixStore>(R, F, *E);
	Store->Holds = [this](const FString& U) { return UnitHeld(U); };
	Store->RouteM = [this](const FString& U) { return RouteTo(U); };
	if (Saved) Store->Load(Saved);
	StoreNotes = Store->Notes.Num();
	Store->Found(E->Day, E->Minute);   // (a save from before the store gets the company its float)
	E->Hooks.Add(StoreHookId, [this](int32 D) { if (Store) Store->Settle(D); });   // FURN-024: its takings go on to wages and stock at each day's end
	Basket.Reset();
}

bool ABrixLife::UnitHeld(const FString& Unit) const
{
	const int32* i = Est ? Est->ById.Find(Unit) : nullptr;
	return i && (Est->IsMine(*i) || Est->Rents(*i));
}

double ABrixLife::RouteTo(const FString& Unit) const   // along the streets' grid from the outlet's door to the building's [ASSUMPTION: until the van drives the roads, FURN-027]
{
	const int32* i = Est ? Est->ById.Find(Unit) : nullptr;
	if (!i || !LotDoor.IsValidIndex(OutletLotIx) || !LotDoor.IsValidIndex(Est->Units[*i].Lot)) return 600.0;
	const FVector2D A = LotDoor[OutletLotIx], B = LotDoor[Est->Units[*i].Lot];
	return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
}

namespace
{
	FBrixLayoutPiece StoreAsBelonging(const FBrixStorePiece& P)   // what the store hands over is with the belongings (INV-001), whole
	{
		FBrixLayoutPiece Q;
		Q.Id = P.Id; Q.Kind = P.Kind; Q.Name = P.Name; Q.W = P.W; Q.D = P.D; Q.H = P.H; Q.bRug = P.bRug; Q.Line = P.Line;
		return Q;
	}
}

void ABrixLife::StoreFrame()   // bring the deliveries up to the clock (life.js storeFrame())
{
	if (!Store || !Est) return;
	Store->Advance(E->Day, E->Minute);
	for (; StoreNotes < Store->Notes.Num(); ++StoreNotes) Toast(Store->Notes[StoreNotes].Words);
	for (const FBrixStorePiece& P : Store->Owned) FurnHeld.Add(StoreAsBelonging(P));
	Store->Owned.Reset();
	TArray<TPair<FString, FBrixStorePiece>> Here;
	for (const TPair<FString, TArray<FBrixStorePiece>>& A : Store->Arrived) for (const FBrixStorePiece& P : A.Value) Here.Add({ A.Key, P });
	bool bChanged = false;
	for (const TPair<FString, FBrixStorePiece>& H : Here)
	{
		const FString U = H.Key, Id = H.Value.Id, Name = H.Value.Name.ToLower();
		const int32* i = Est->ById.Find(U);
		auto ToBelongings = [&](const FString& Why)
		{
			FurnHeld.Add(StoreAsBelonging(H.Value));
			Store->RemoveArrived(U, Id);
			Toast(TEXT("Your ") + Name + TEXT(" ") + Why + TEXT(": it is with your belongings."));
		};
		if (!i) { ToBelongings(TEXT("has no home to stand in")); bChanged = true; continue; }
		const int32 Lot = Est->Units[*i].Lot, f = Est->Units[*i].Floors.Num() ? Est->Units[*i].Floors[0] : 0;
		const FBrixFurnishRulesD F = FurnRules();
		if (H.Value.StoodLot.IsEmpty())   // FURN-028: carried in, it is set down on its home's floor
		{
			FBrixRoomSpec S;
			FBrixLayoutPiece As, Put;
			bool bBoxed = false;
			if (!CanFurnish(Lot) || !RoomFor(Lot, f, S)) { ToBelongings(TEXT("was delivered to ") + Est->Name(*i) + TEXT(", whose rooms are not arranged in the designer")); bChanged = true; continue; }
			FBrixLayout Lay = LayNow(Lot, f);
			Store->AsPlaced(Id, E->Day, E->Minute, As, bBoxed);
			if (!BrixStoreSetDown(F, S.Room, S.Keep, S.Ceiling, Lay, As, H.Value.At, S.Entries.Num() ? S.Entries[0] : FVector2D((S.Room.X + S.Room.Z) / 2, (S.Room.Y + S.Room.W) / 2), Put))
			{
				ToBelongings(TEXT("found no room on the floor of ") + Est->Name(*i)); bChanged = true; continue;
			}
			Lay.Add(Put);
			LaysDone.FindOrAdd(Lots[Lot].Id).Add(f, Lay);
			ShowLayout(Lot, f, Lay);
			if (FBrixStorePiece* P = Store->Find(Id)) { P->StoodLot = Lots[Lot].Id; P->StoodFloor = f; }
			Toast(TEXT("Your ") + Name + TEXT(" has been delivered to ") + Est->Name(*i) + (bBoxed ? TEXT(", in its boxes: put it together from the outlet's Deliveries, or book a fitter.") : TEXT(".")));
			UE_LOG(LogTemp, Display, TEXT("Brix Town: %s (%s) set down in %s floor %d at %.2f, %.2f%s"), *Id, *H.Value.Kind, *Lots[Lot].Id, f, Put.X, Put.Z, bBoxed ? TEXT(", in its boxes") : TEXT(""));
			if (!bBoxed) Store->RemoveArrived(U, Id);   // whole: it is the home's furniture now
			bChanged = true;
		}
		else if (!Store->IsBoxed(Id, E->Day, E->Minute))   // FURN-029: put together, it stands as itself where its boxes stood
		{
			if (TMap<int32, FBrixLayout>* ByF = LaysDone.Find(H.Value.StoodLot))
				if (FBrixLayout* Lay = ByF->Find(H.Value.StoodFloor))
					if (FBrixLayoutPiece* Whole = Lay->FindByPredicate([&](const FBrixLayoutPiece& X) { return X.Id == Id; }))
					{
						Whole->Name = H.Value.Name; Whole->W = H.Value.W; Whole->D = H.Value.D; Whole->H = H.Value.H; Whole->bBoxed = false;
						if (const int32* n = E->LotIndex.Find(H.Value.StoodLot)) ShowLayout(*n, H.Value.StoodFloor, *Lay);
					}
			Store->RemoveArrived(U, Id);
			Toast(TEXT("Your ") + Name + TEXT(" is put together."));
			bChanged = true;
		}
	}
	if (bChanged && Panel == TEXT("store")) Draw();
}

FString ABrixLife::SlotWords(int32 OnDay, int32 Slot) const
{
	const FIntPoint S = Store && Store->R.Slots.IsValidIndex(Slot) ? Store->R.Slots[Slot] : FIntPoint(0, 0);
	return (OnDay == E->Day + 1 ? FString(TEXT("tomorrow")) : FString::Printf(TEXT("day %d"), OnDay + 1)) + TEXT(", ") + UiHhmm(S.X) + TEXT(" to ") + UiHhmm(S.Y);
}

TArray<FBrixBasketItem> ABrixLife::BasketFilled() const   // the basket with where and when its deliveries go
{
	TArray<FBrixBasketItem> Out = Basket;
	const TArray<FIntPoint> Sl = Store->SlotsNow();
	const FIntPoint When = Sl.Num() ? Sl[FMath::Clamp(StoreWhen, 0, Sl.Num() - 1)] : FIntPoint(0, 0);
	for (FBrixBasketItem& It : Out) if (It.How == TEXT("delivery")) { It.Unit = StoreUnit; It.Day = When.X; It.Slot = When.Y; }
	return Out;
}

bool ABrixLife::StoreCheckout(FString& Said)
{
	const TArray<FBrixBasketItem> Filled = BasketFilled();
	int64 Total = 0;
	TArray<FString> Got;
	if (!Store->Checkout(Filled, bStoreHome, Total, Got)) { E->Why = Store->Why; return false; }
	int32 Sent = 0;
	for (const FBrixBasketItem& It : Filled) Sent += It.How == TEXT("delivery");
	const FBrixBasketItem* First = Filled.FindByPredicate([](const FBrixBasketItem& It) { return It.How == TEXT("delivery"); });
	Said = TEXT("Paid ") + Format(Total) + TEXT(".")
		+ (Got.Num() - Sent ? FString::Printf(TEXT(" %d with your belongings (G): place them from the Furnish tab of the building designer."), Got.Num() - Sent) : FString())
		+ (First ? FString::Printf(TEXT(" %d booked for "), Sent) + SlotWords(First->Day, First->Slot) + TEXT(".") : FString());
	Basket.Reset();
	StoreTab = Sent ? TEXT("book") : TEXT("show");
	StoreFrame();   // what was taken now is with the belongings at once
	return true;
}

void ABrixLife::StorePanel(TSharedRef<SVerticalBox> Body)
{
	if (!Store || !Est) return;
	auto Slot = [&](TSharedRef<SWidget> W, float Top = 6) { Body->AddSlot().AutoHeight().Padding(0, Top, 0, 0)[ W ]; };
	const FBrixFurnishRulesD& F = Store->F;
	TArray<int32> Homes;
	for (int32 i = 0; i < Est->Units.Num(); ++i) if (Est->IsMine(i) || Est->Rents(i)) Homes.Add(i);
	if (!Homes.ContainsByPredicate([this](int32 i) { return Est->Units[i].Id == StoreUnit; })) StoreUnit = Homes.Num() ? Est->Units[Homes[0]].Id : FString();
	if (StoreRoom.IsEmpty() && F.Rooms.Num()) StoreRoom = F.Rooms[0].Key;
	if (StoreLine.IsEmpty() && F.Lines.Num()) StoreLine = F.Lines[0];
	const TArray<FIntPoint> Sl = Store->SlotsNow();
	StoreWhen = FMath::Clamp(StoreWhen, 0, FMath::Max(0, Sl.Num() - 1));
	int32 Due = 0, Packs = 0;
	for (const FBrixStoreOrder& O : Store->Orders) Due += O.Landed < O.Pieces.Num();
	for (const TPair<FString, TArray<FBrixStorePiece>>& A : Store->Arrived) Packs += A.Value.Num();
	auto Go = [this](const TCHAR* T) { const FString Tab = T; return [this, Tab]() { StoreTab = Tab; Msg.Reset(); Draw(); }; };
	auto LineName = [](const FString& Ln) { return Ln == TEXT("oldtown") ? FString(TEXT("Old Town")) : Ln.Left(1).ToUpper() + Ln.Mid(1); };
	Slot(PurseLine(), 0);
	TSharedRef<SHorizontalBox> Tabs = UiRow();
	UiAdd(Tabs, UiButton(bStoreHome ? TEXT("Catalogue") : TEXT("Showroom"), Go(TEXT("show")), true, StoreTab != TEXT("show")));
	UiAdd(Tabs, UiButton(FString::Printf(TEXT("Basket (%d)"), Basket.Num()), Go(TEXT("basket")), true, StoreTab != TEXT("basket")));
	UiAdd(Tabs, UiButton(Due + Packs ? FString::Printf(TEXT("Deliveries (%d)"), Due + Packs) : FString(TEXT("Deliveries")), Go(TEXT("book")), true, StoreTab != TEXT("book")));
	if (!Store->bCatalogue && !bStoreHome) UiAdd(Tabs, UiButton(TEXT("Take a catalogue (free)"), [this]() { Act([this](FString& S) { Store->bCatalogue = true; S = TEXT("You have the outlet's catalogue: B at a home of yours orders from it, for delivery."); return true; }); }, true, true));
	Slot(Tabs);
	if (StoreTab == TEXT("show"))
	{
		TSharedRef<SWrapBox> Rk = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6, 4));
		for (const auto& R : F.Rooms) { const FString K = R.Key; Rk->AddSlot()[ UiButton(K.Left(1).ToUpper() + K.Mid(1), [this, K]() { StoreRoom = K; Draw(); }, true, StoreRoom != K) ]; }
		Slot(Rk);
		TSharedRef<SHorizontalBox> Lk = UiRow();
		UiAdd(Lk, UiText(TEXT("Line"), 13, UiDim));
		for (const FString& Ln : F.Lines) UiAdd(Lk, UiButton(LineName(Ln), [this, Ln]() { StoreLine = Ln; Draw(); }, true, StoreLine != Ln));
		Slot(Lk, 4);
		TArray<FWare> Things;   // each piece as an object, with what its tag says (FURN-022)
		if (const auto* Room = F.Rooms.FindByPredicate([this](const TPair<FString, TArray<FString>>& R) { return R.Key == StoreRoom; }))
			for (const FString& K : Room->Value)
			{
				FBrixStoreTag T;
				if (!Store->Tag(K, StoreLine, T)) continue;
				FWare W;
				W.Id = K; W.Name = T.Name; W.Price = T.Price; W.Goes = TEXT("home"); W.Band = BuyMenu.Bands.FindRef(TEXT("mall")); W.Line = StoreLine; W.Size = T.Size;
				W.More.Add({ TEXT("Size"), FString::Printf(TEXT("%d × %d × %d cm"), FMath::RoundToInt(T.Size.X * 100), FMath::RoundToInt(T.Size.Y * 100), FMath::RoundToInt(T.Size.Z * 100)) });
				W.More.Add({ TEXT("It comes"), T.bFlatPacked ? FString::Printf(TEXT("flat-packed, %d %s"), T.Boxes, T.Boxes == 1 ? TEXT("box") : TEXT("boxes")) : FString(TEXT("whole")) });
				if (T.bFlatPacked) W.More.Add({ TEXT("To put together"), FString::Printf(TEXT("%d min, or %s"), T.AssemblyMin, *Format(T.AssemblyFee)) });
				Things.Add(W);
			}
		FGridOpts O;
		O.Id = TEXT("store");
		O.Verb = TEXT("Add to basket");
		O.Buy = [this](const FWare& W, FString& Said)
		{
			FBrixBasketItem It;
			It.Kind = W.Id; It.Line = W.Line; It.How = bStoreHome ? TEXT("delivery") : TEXT("instant");
			Basket.Add(It);
			Said = W.Name + TEXT(" is in your basket.");
			return true;
		};
		Slot(Grid(Things, O), 6);
		Slot(UiText(FString(TEXT("Every piece comes in the four lines. ")) + (bStoreHome ? TEXT("From home everything is delivered.") : TEXT("At the till each piece is taken now, free, to your belongings (G), or delivered to a home of yours."))
			+ TEXT(" A flat-packed piece is delivered in its boxes: put it together yourself, or pay a fitter."), 13, UiDim));
	}
	else if (StoreTab == TEXT("basket"))
	{
		if (!Basket.Num()) { Slot(UiText(TEXT("Nothing in your basket yet."), 14, UiDim)); return; }
		bool bAnySent = false;
		for (int32 k = 0; k < Basket.Num(); ++k)
		{
			const FBrixBasketItem It = Basket[k];
			const bool bPack = Store->FlatPacked(It.Kind), bSend = It.How == TEXT("delivery");
			bAnySent |= bSend;
			TSharedRef<SHorizontalBox> Rw = UiRow();
			Rw->AddSlot().FillWidth(1).VAlign(VAlign_Center)[ UiText(F.Pieces[It.Kind].Name + TEXT("  ·  ") + LineName(It.Line), 14) ];
			UiAdd(Rw, UiText(Format(Store->Price(It.Kind)), 14, UiGold));
			UiAdd(Rw, UiButton(TEXT("Take now"), [this, k]() { Basket[k].How = TEXT("instant"); Draw(); }, !bStoreHome, bSend));
			UiAdd(Rw, UiButton(TEXT("Deliver"), [this, k]() { Basket[k].How = TEXT("delivery"); Draw(); }, Homes.Num() > 0, !bSend));
			if (bSend && bPack)
			{
				UiAdd(Rw, UiButton(TEXT("In its boxes"), [this, k]() { Basket[k].bAssembled = false; Draw(); }, true, It.bAssembled));
				UiAdd(Rw, UiButton(TEXT("Put together, ") + Format(Store->AssemblyFee(It.Kind)), [this, k]() { Basket[k].bAssembled = true; Draw(); }, true, !It.bAssembled));
			}
			else if (bSend) UiAdd(Rw, UiText(TEXT("comes whole"), 12, UiDim));
			UiAdd(Rw, UiButton(TEXT("Take out"), [this, k]() { if (Basket.IsValidIndex(k)) Basket.RemoveAt(k); Draw(); }, true, true));
			Slot(Rw, 3);
		}
		if (bAnySent)
		{
			TSharedRef<SWrapBox> To = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6, 4));
			To->AddSlot().VAlign(VAlign_Center)[ UiText(TEXT("Deliver to"), 13, UiDim) ];
			for (const int32 i : Homes) { const FString U = Est->Units[i].Id; To->AddSlot()[ UiButton(Est->Name(i), [this, U]() { StoreUnit = U; Draw(); }, true, StoreUnit != U) ]; }
			Slot(To, 8);
			TSharedRef<SHorizontalBox> Wn = UiRow();
			UiAdd(Wn, UiText(TEXT("When"), 13, UiDim));
			UiAdd(Wn, UiButton(TEXT("Sooner"), [this]() { --StoreWhen; Draw(); }, StoreWhen > 0, true));
			if (Sl.Num()) UiAdd(Wn, UiText(SlotWords(Sl[StoreWhen].X, Sl[StoreWhen].Y), 14, UiCream, true));
			UiAdd(Wn, UiButton(TEXT("Later"), [this]() { ++StoreWhen; Draw(); }, StoreWhen < Sl.Num() - 1, true));
			Slot(Wn, 4);
		}
		int64 Total = 0;
		TArray<FBrixFurnLine> Lines;
		FString Why;
		const bool bCan = Store->Quote(BasketFilled(), bStoreHome, Total, Lines);
		if (!bCan) Why = Store->Why;
		else if (Total > E->Funds()) Why = TEXT("it costs ") + Format(Total) + TEXT(" and you have ") + Format(E->Funds());
		if (bCan)
		{
			Slot(UiHeading(TEXT("The bill")), 0);
			for (const FBrixFurnLine& Ln : Lines)
			{
				TSharedRef<SHorizontalBox> B = UiRow();
				B->AddSlot().FillWidth(1).VAlign(VAlign_Center)[ UiText(Ln.What, 14) ];
				UiAdd(B, UiText(Ln.Qty == 1 ? FString() : FString::Printf(TEXT("× %d"), (int32)Ln.Qty), 12, UiDim));
				UiAdd(B, UiText(Format(Ln.Chips), 14, UiGold));
				Slot(B, 2);
			}
			TSharedRef<SHorizontalBox> B = UiRow();
			B->AddSlot().FillWidth(1)[ UiText(TEXT("Total"), 14, UiCream, true) ];
			UiAdd(B, UiText(Format(Total), 15, UiGold, true));
			Slot(B, 4);
		}
		if (!Why.IsEmpty()) Slot(UiText(Why.Left(1).ToUpper() + Why.Mid(1) + TEXT("."), 13, UiNeg));
		TSharedRef<SHorizontalBox> Pay = UiRow();
		UiAdd(Pay, UiButton(bCan ? TEXT("Pay ") + Format(Total) : FString(TEXT("Pay")), [this]() { Act([this](FString& S) { return StoreCheckout(S); }); }, Why.IsEmpty()));
		Slot(Pay, 10);
	}
	else
	{
		Slot(UiHeading(TEXT("Deliveries")), 0);
		int32 Open = 0;
		for (const FBrixStoreOrder& O : Store->Orders)
		{
			const FBrixVan V = Store->Van(O, E->Day, E->Minute);
			if (V.Phase == TEXT("done")) continue;
			++Open;
			FString Names;
			for (const FBrixStorePiece& P : O.Pieces) Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + P.Name;
			const int32* i = Est->ById.Find(O.Unit);
			const FString How = V.Phase == TEXT("booked") ? FString(TEXT("booked")) : V.Phase == TEXT("out") ? FString::Printf(TEXT("the van is on its way, %d %%"), FMath::RoundToInt(V.F * 100))
				: V.Phase == TEXT("at the door") ? FString::Printf(TEXT("the crew is carrying it in, %d of %d in"), V.Landed, O.Pieces.Num()) : FString::Printf(TEXT("delivered; the van is driving back, %d %%"), FMath::RoundToInt(V.F * 100));
			Slot(UiText(SlotWords(O.Day, O.Slot) + TEXT("  ·  ") + (i ? Est->Name(*i) : O.Unit) + TEXT("  ·  ") + Names + TEXT("  ·  ") + How, 14), 3);
		}
		if (!Open) Slot(UiText(TEXT("Nothing is on its way."), 14, UiDim));
		Slot(UiHeading(TEXT("In their boxes")), 0);
		if (!Packs) Slot(UiText(TEXT("No flat packs are waiting to be put together."), 14, UiDim));
		for (const TPair<FString, TArray<FBrixStorePiece>>& A : Store->Arrived)
			for (const FBrixStorePiece& P : A.Value)
			{
				const int32* i = Est->ById.Find(A.Key);
				const FString Id = P.Id, Name = P.Name.ToLower();
				TSharedRef<SHorizontalBox> Rw = UiRow();
				Rw->AddSlot().FillWidth(1).VAlign(VAlign_Center)[ UiText(P.Name + TEXT("  ·  ") + (i ? Est->Name(*i) : A.Key), 14) ];
				if (P.Ready) UiAdd(Rw, UiText((FMath::FloorToInt(*P.Ready / 1440) == E->Day ? FString(TEXT("ready at ")) : FString::Printf(TEXT("ready on day %d at "), FMath::FloorToInt(*P.Ready / 1440) + 1)) + UiHhmm(FMath::Fmod(*P.Ready, 1440.0)), 13, UiDim));
				else
				{
					UiAdd(Rw, UiButton(FString::Printf(TEXT("Put it together (%d min)"), Store->AssemblyMin(P.Kind)), [this, Id, Name]() { Act([this, Id, Name](FString& S)
					{
						int32 M = 0;
						if (!Store->Assemble(Id, E->Day, E->Minute, M)) { E->Why = Store->Why; return false; }
						S = FString::Printf(TEXT("You are putting the %s together: %d minutes."), *Name, M);
						return true;
					}); }, true, true));
					if (Sl.Num()) UiAdd(Rw, UiButton(TEXT("A fitter, ") + Format(Store->AssemblyFee(P.Kind)) + TEXT(", ") + SlotWords(Sl[0].X, Sl[0].Y), [this, Id, When = Sl[0]]() { Act([this, Id, When](FString& S)
					{
						int64 Fee = 0;
						if (!Store->Fitter(Id, When.X, When.Y, Fee)) { E->Why = Store->Why; return false; }
						S = TEXT("A fitter is booked for ") + Format(Fee) + TEXT(".");
						return true;
					}); }, true, true));
				}
				Slot(Rw, 3);
			}
	}
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
		if (A[0] == TEXT("later"))   // later,<s>: the ops after it are done that much later (a picture has to be drawn before a mouse can be on it)
		{
			FString Rest;
			for (int32 k = (int32)(&Op - List.GetData()) + 1; k < List.Num(); ++k) Rest += (Rest.IsEmpty() ? TEXT("") : TEXT(";")) + List[k];
			Later(A.IsValidIndex(1) ? FCString::Atod(*A[1]) : 1.0, [this, Rest]() { RunOps(Rest); });
			break;
		}
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
		else if (A[0] == TEXT("tab") && Panel == TEXT("remodel")) { DesignTab = Sv(1); DesignView(); Draw(); }   // tab,<outside|inside|furnish>   // tab,<outside|inside>
		else if (A[0] == TEXT("storey") && Panel == TEXT("remodel")) { DesignStorey = (int32)I(1); WallSel.Reset(); DesignView(); Draw(); }
		else if (A[0] == TEXT("addwall") && Panel == TEXT("remodel")) { Draw(); AddWall(Sv(1)); }   // addwall,<x|z>: a new wall across the storey's middle
		else if (A[0] == TEXT("wall") && Panel == TEXT("remodel")) { WallSel = Sv(1); Draw(); }   // wall,<id>: pick it
		else if (A[0] == TEXT("door") && Panel == TEXT("remodel") && !WallSel.IsEmpty())   // door: open or close the picked wall's doorway
		{
			const FBrixRoomPlanRules PR = PlanRules();
			const FString Id = WallSel;
			EditPlan([&](FBrixPlan& Pl) { if (FBrixPlanWall* X = Pl.FindByPredicate([&](const FBrixPlanWall& Y) { return Y.Id == Id; })) { if (X->Door) X->Door.Reset(); else X->Door = BrixRoomPlan::Gridded(PR, (X->A0 + X->A1) / 2); } });
		}
		else if (A[0] == TEXT("close")) Close();
		// furnishing (FURN-051): piece,<id> picks one; turnpiece,<deg>; movepiece,<dx>,<dz> (metres); takeaway; addpiece,<kind>,<line>; placeheld,<n>
		else if (A[0] == TEXT("piece") && Panel == TEXT("remodel")) { PieceSel = Sv(1); Draw(); }
		else if ((A[0] == TEXT("turnpiece") || A[0] == TEXT("movepiece")) && Panel == TEXT("remodel") && !PieceSel.IsEmpty())
		{
			const FBrixFurnishRulesD FR = FurnRules();
			const FString Id = PieceSel;
			const bool bTurn = A[0] == TEXT("turnpiece");
			const double Dx = FCString::Atod(*Sv(1)), Dz = FCString::Atod(*Sv(2));
			EditLayout([&](FBrixLayout& L) { if (FBrixLayoutPiece* X = L.FindByPredicate([&](const FBrixLayoutPiece& Y) { return Y.Id == Id; })) { if (bTurn) X->Yaw = FMath::Fmod(FMath::Fmod(X->Yaw + Dx, 360.0) + 360.0, 360.0); else { X->X = BrixFurnishPlan::Snap(FR, X->X + Dx); X->Z = BrixFurnishPlan::Snap(FR, X->Z + Dz); } } });
		}
		else if (A[0] == TEXT("takeaway") && Panel == TEXT("remodel") && !PieceSel.IsEmpty()) { const FString Id = PieceSel; EditLayout([&](FBrixLayout& L) { L.RemoveAll([&](const FBrixLayoutPiece& X) { return X.Id == Id; }); }); PieceSel.Reset(); }
		else if ((A[0] == TEXT("addpiece") || A[0] == TEXT("placeheld")) && Panel == TEXT("remodel"))
		{
			FBrixRoomSpec Sp;
			bOk = RoomFor(PanelLot, DesignStorey, Sp);
			const FBrixFurnishRulesD FR = FurnRules();
			const double Cx = BrixFurnishPlan::Snap(FR, (Sp.Room.X + Sp.Room.Z) / 2), Cz = BrixFurnishPlan::Snap(FR, (Sp.Room.Y + Sp.Room.W) / 2);
			const FBrixLayout Held = HeldNow();
			const int32 k = (int32)I(1);
			if (A[0] == TEXT("placeheld") && !Held.IsValidIndex(k)) bOk = false;
			if (bOk) EditLayout([&](FBrixLayout& L)
			{
				if (A[0] == TEXT("placeheld")) { FBrixLayoutPiece Q = Held[k]; Q.X = Cx; Q.Z = Cz; Q.Yaw = 0; L.Add(Q); PieceSel = Q.Id; return; }
				int32 n = 1;
				for (const FBrixLayoutPiece& X : L) if (X.Id.StartsWith(TEXT("k"))) n = FMath::Max(n, FCString::Atoi(*X.Id.Mid(1)) + 1);
				PieceSel = FString::Printf(TEXT("k%d"), n);
				L.Add(BrixFurnishPlan::KitPiece(FR, Sv(1), PieceSel, Cx, Cz, 0, Sv(2).IsEmpty() ? FString(TEXT("plain")) : Sv(2)));
			});
		}   // close: the panel shut, the player's own view again
		// the mouse (FURN-055, BUILD-021, BUILD-020): dragpiece,<id>,<x>,<z> presses on that piece in the picture and drags its middle
		// to there (metres, the data's frame); dragwall,<id>,<x>,<z> the same for an inner wall; view,<turn px>,<tip px>,<wheel clicks>
		// drags the view round and turns the wheel; listpieces shows the storey's pieces by name; a fifth word "by" makes x and z a distance
		else if ((A[0] == TEXT("dragpiece") || A[0] == TEXT("dragwall")) && Panel == TEXT("remodel"))
		{
			APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
			const bool bWall = A[0] == TEXT("dragwall");
			const FString Id = Sv(1);
			const double F = DesignFloorM();
			FVector2D To(FCString::Atod(*Sv(2)), FCString::Atod(*Sv(3)));
			FVector2D From = FVector2D::ZeroVector;
			bOk = false;
			if (bWall) { if (const FBrixPlan* Pl = PlanDraft.Find(DesignStorey)) if (const FBrixPlanWall* W = Pl->FindByPredicate([&](const FBrixPlanWall& X) { return X.Id == Id; })) { const FBrixLayoutPiece Wp = BrixDesignPick::WallAsPiece(PlanRules(), *W); From = FVector2D(Wp.X, Wp.Z); bOk = true; } }
			else if (const FBrixLayout* Ly = LayDraft.Find(DesignStorey)) if (const FBrixLayoutPiece* Pc = Ly->FindByPredicate([&](const FBrixLayoutPiece& X) { return X.Id == Id; })) { From = FVector2D(Pc->X, Pc->Z); bOk = true; }
			if (Sv(4) == TEXT("by")) To += From;   // dragpiece,<id>,<dx>,<dz>,by: that far from where it stands
			FVector2D S0, S1;
			E->Why.Reset();
			double Top = 0;   // the press is on its top, where nothing nearer is in the way
			if (bWall) Top = DesignCutM - .05;
			else if (const FBrixLayout* Ly = LayDraft.Find(DesignStorey)) if (const FBrixLayoutPiece* Pc = Ly->FindByPredicate([&](const FBrixLayoutPiece& X) { return X.Id == Id; })) Top = FMath::Max(Pc->H, .05) - .01;
			if (bOk && PC && ViewCam)
			{
				PC->SetViewTarget(ViewCam);   // the view is there at once, so the picture's points are this camera's
				if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
				FBrixRay Ray;
				bOk = PC->ProjectWorldLocationToScreen(FVector(From.X, From.Y, F + Top) * CmPerM, S0) && DesignRay(S0, Ray);
				const TOptional<FVector2D> Hit0 = bOk ? BrixDesignPick::FloorHit(Ray, F) : TOptional<FVector2D>();   // where the mouse is on the floor: the piece keeps its distance from it
				bOk = Hit0.IsSet() && PC->ProjectWorldLocationToScreen(FVector(Hit0->X + To.X - From.X, Hit0->Y + To.Y - From.Y, F) * CmPerM, S1);
			}
			else if (bOk) { bOk = false; E->Why = TEXT("the designer has no view yet"); }
			else E->Why = TEXT("no ") + Id + TEXT(" on the storey in view");
			if (!bOk && E->Why.IsEmpty()) E->Why = Id + TEXT(" is not in the picture");
			if (bOk)
			{
				Grab = FGrab();
				const bool bClear = S0.X > (RemodelPanelPx + 24.f) * PxScale;   // not under the card: a press there would be on its buttons
				const bool bSent = bClear && SlatePoint(EPoint::Down, S0);   // through the interface, as the mouse's own press would come
				const bool bPad = bSent && Grab.Kind != 0;
				FString Took = TEXT("nothing");
				if (bPad) { if (Grab.Kind >= 3) Took = Grab.Id; SlatePoint(EPoint::Move, (S0 + S1) / 2); SlatePoint(EPoint::Move, S1); SlatePoint(EPoint::Up, S1); }
				else
				{
					if (bSent) SlatePoint(EPoint::Up, S0);
					DesignPointer(EPoint::Down, S0, 0, false, FVector2D::ZeroVector, 0.f, true);
					if (Grab.Kind >= 3) Took = Grab.Id;
					DesignPointer(EPoint::Move, S1, 0, false, S1 - S0, 0.f, true);
					DesignPointer(EPoint::Up, S1, 0, false, FVector2D::ZeroVector, 0.f, false);
				}
				FVector2D Now = FVector2D::ZeroVector;
				if (bWall) { if (const FBrixPlanWall* W = PlanDraft[DesignStorey].FindByPredicate([&](const FBrixPlanWall& X) { return X.Id == Id; })) { const FBrixLayoutPiece Wp = BrixDesignPick::WallAsPiece(PlanRules(), *W); Now = FVector2D(Wp.X, Wp.Z); } }
				else if (const FBrixLayoutPiece* Pc = LayDraft[DesignStorey].FindByPredicate([&](const FBrixLayoutPiece& X) { return X.Id == Id; })) Now = FVector2D(Pc->X, Pc->Z);
				bOk = (bWall ? WallSel : PieceSel) == Id && !Now.Equals(From, 1e-6);
				UE_LOG(LogTemp, Display, TEXT("Brix Town: the mouse %s %s %s from %.2f, %.2f to %.2f, %.2f (asked to %.2f, %.2f; the picture's %.0f, %.0f to %.0f, %.0f; its press took hold of %s), %s"),
					bOk ? TEXT("dragged") : TEXT("did not drag"), bWall ? TEXT("wall") : TEXT("piece"), *Id, From.X, From.Y, Now.X, Now.Y, To.X, To.Y, S0.X, S0.Y, S1.X, S1.Y, *Took,
					bPad ? TEXT("through the pad behind the card") : TEXT("handed straight to the designer (the pad did not take the press)"));
				if (!bOk) E->Why = TEXT("the mouse did not pick ") + Id;
			}
		}
		else if (A[0] == TEXT("view") && Panel == TEXT("remodel"))
		{
			Grab = FGrab();
			Grab.Kind = 1;
			DesignPointer(EPoint::Move, ViewportPx / 2, 0, false, FVector2D(FCString::Atod(*Sv(1)), FCString::Atod(*Sv(2))), 0.f, true);
			Grab = FGrab();
			const int32 Clicks = FCString::Atoi(*Sv(3));
			for (int32 k = 0; k < FMath::Abs(Clicks); ++k) DesignPointer(EPoint::Wheel, FVector2D(-1, -1), 0, false, FVector2D::ZeroVector, Clicks > 0 ? 1.f : -1.f, false);
		}
		else if (A[0] == TEXT("listpieces") && Panel == TEXT("remodel")) { bPieceList = true; Draw(); }
		// the furniture outlet's till (FURN-022 to FURN-029): store opens it (store,home: from the catalogue); storetab,<show|basket|book>;
		// basket,<kind>,<line>,<instant|delivery>[,assembled]; storeunit,<unit id>; checkout pays; assemble,<piece id>; catalogue takes one
		else if (A[0] == TEXT("store") && Store) { bStoreHome = Sv(1) == TEXT("home"); StoreTab = TEXT("show"); if (bStoreHome) Store->bCatalogue = true; Open(TEXT("store"), OutletLotIx); bOk = Panel == TEXT("store"); }
		else if (A[0] == TEXT("storetab") && Store) { StoreTab = Sv(1); Draw(); }
		else if (A[0] == TEXT("catalogue") && Store) Store->bCatalogue = true;
		else if (A[0] == TEXT("basket") && Store) { FBrixBasketItem It; It.Kind = Sv(1); It.Line = Sv(2).IsEmpty() ? FString(TEXT("plain")) : Sv(2); It.How = Sv(3).IsEmpty() ? FString(TEXT("instant")) : Sv(3); It.bAssembled = Sv(4) == TEXT("assembled"); Basket.Add(It); Draw(); }
		else if (A[0] == TEXT("storeunit") && Store) { StoreUnit = Sv(1); Draw(); }
		else if (A[0] == TEXT("checkout") && Store)
		{
			if (StoreUnit.IsEmpty() && Est) for (int32 j = 0; j < Est->Units.Num(); ++j) if (Est->IsMine(j) || Est->Rents(j)) { StoreUnit = Est->Units[j].Id; break; }
			FString Said;
			bOk = StoreCheckout(Said);
			Msg = bOk ? Said : TEXT("Refused: ") + E->Why + TEXT(".");
			if (bOk) UE_LOG(LogTemp, Display, TEXT("Brix Town: at the furniture outlet's till: %s"), *Said);
			Draw();
		}
		else if (A[0] == TEXT("assemble") && Store) { int32 Mn = 0; bOk = Store->Assemble(Sv(1), E->Day, E->Minute, Mn); if (!bOk) E->Why = Store->Why; Draw(); }
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
		else if (A[0] == TEXT("board"))   // board,<kind>: bought at the first of the mall's outlets that sells boards (CITY-103)
		{
			bOk = false;
			FString Said;
			for (const auto& M : E->Outlets)
			{
				for (const FBrixOutlet& U : M.Value) if (E->SellsBoards(M.Key, U.Id)) { bOk = BoardBuy(M.Key, U.Id, Sv(1), Said); break; }
				if (bOk) break;
			}
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

TSharedPtr<const FBrixThingMesh> ABrixLife::MeshFor(const FString& Thing, const FString& Band, const FString& Line)
{
	const FString Key = Thing + TEXT("|") + Band + TEXT("|") + Line;
	if (const TSharedPtr<const FBrixThingMesh>* M = Meshes.Find(Key)) return *M;
	if (Thing.StartsWith(TEXT("board:")))   // a skateboard on show (CITY-102): its own model, as people.js board() draws it
	{
		const ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
		const FBrixBoardKind* K = Me ? Me->BoardRules.Kind(Thing.RightChop(6)) : nullptr;
		TSharedPtr<const FBrixThingMesh> Got;
		if (K) { FBrixBodyMesh Body; DrawBrixBoard(*K, 0, Body); Got = MakeShared<const FBrixThingMesh>(BrixThingFromBody(Body)); }
		return Meshes.Add(Key, Got);
	}
	TOptional<FBrixThingMesh> M = BuyMenu.MeshFor(Thing, Band);
	if (const FBrixShows Sh = BuyMenu.Shows(Thing); !M && Sh.Kind == TEXT("piece"))   // a piece of furniture: the kit's own mesh in its line (ADR-0050), as it will stand in the room
	{
		const FString Name = TEXT("SM_") + Sh.Name.Replace(TEXT(":"), TEXT("_")) + TEXT("_") + (Line.IsEmpty() ? FString(TEXT("plain")) : Line);
		M = BrixThingFromStaticMesh(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Furniture/%s.%s"), *Name, *Name)));
		const FBrixFurnishRulesD F = FurnRules();
		if (const FBrixFurnishRulesD::FPiece* Pc = M ? nullptr : F.Pieces.Find(Sh.Name)) M = BrixThingBox(Pc->Size, FVector3f(.45f, .38f, .3f));   // no mesh of its own yet (or one that cannot be read): a box its size, as in the room
		UE_LOG(LogTemp, Display, TEXT("Brix Town: the picture of %s is %s, %d triangles"), *Name, M && !M->bStandIn ? TEXT("its own mesh") : TEXT("a box its size"), M ? M->Num() : 0);
	}
	TSharedPtr<const FBrixThingMesh> Got; if (M) Got = MakeShared<const FBrixThingMesh>(*M); return Meshes.Add(Key, Got);
}

FString ABrixLife::BoardsSoldWhere() const
{
	for (const auto& M : E ? E->Outlets : TMap<int32, TArray<FBrixOutlet>>())
		if (Lots.IsValidIndex(M.Key)) return TEXT("The sports and toy shops in ") + Lots[M.Key].Name;
	return TEXT("The mall's sports and toy shops");
}

// buy a board at outlet U of the mall at lot n (CITY-103): through the ledger; the character owns it, and uses it when it is faster
bool ABrixLife::BoardBuy(int32 n, const FString& U, const FString& K, FString& Said)
{
	const FBrixEconomyRules::FBoard* B = E->R.Boards.Find(K);
	ABrixCharacter* Rider = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	const FString Was = Rider ? Rider->Boards.InUse : FString();
	int64 P = 0;
	if (!E->BuyBoard(n, U, K, &P)) return false;
	const FString Use = FBrixEconomy::BoardInUse(E->R, Was, K);
	if (Rider)   // the board joins the others; the one in use changes only when the new one is faster (a ride goes on)
	{
		TArray<FString> Own;
		for (const FString& X : Rider->BoardRules.Order) if (E->Boards.Contains(X) || Rider->Boards.Own.Contains(X)) Own.Add(X);
		Rider->Boards.Own = Own;
		Rider->Boards.Use(Use);
	}
	const FBrixEconomyRules::FBoard* WasB = E->R.Boards.Find(Was);
	Said = TEXT("Bought the ") + (B ? B->Name.ToLower() : K) + TEXT(" for ") + Format(P) + TEXT(".") + (Use == K ? FString(TEXT(" It is the one in use: Y gets on it, or put it on show at home."))
		: TEXT(" Your ") + (WasB ? WasB->Name.ToLower() : FString(TEXT("board"))) + TEXT(" stays the one in use; Belongings (G) changes it."));
	return true;
}

// a sports or toy outlet's skateboards (CITY-102, CITY-103; ADR-0054): the three boards in the buy menu, each its own model
// turning in the showcase, at boards.json's prices; one owned shows as yours and is not sold again. A board bought goes to the
// belongings and is the one in use when it is faster than the one in use (economy.board_in_use); none are bought back (Q-40).
TArray<ABrixLife::FWare> ABrixLife::BoardWares(const FString& Band) const
{
	const ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	TArray<FWare> Things;
	for (const FString& K : E->R.BoardOrder)
	{
		const FBrixEconomyRules::FBoard* B = E->R.Boards.Find(K);
		if (!B) continue;
		FWare W = Ware(TEXT("board:") + K, B->Name, B->Price, TEXT("carry"), -1, Band);
		if (E->Boards.Contains(K) || (Me && Me->Boards.Own.Contains(K))) { W.State = TEXT("have"); W.Short = 0; }
		W.More.Add({ TEXT("Top speed"), FString::Printf(TEXT("%d km/h"), (int32)B->TopKmh) });
		if (const FBrixBoardKind* BK = Me ? Me->BoardRules.Kind(K) : nullptr) W.More.Add({ TEXT("It goes"), BK->bPush ? TEXT("pushed with a foot") : TEXT("by itself, W held") });
		Things.Add(W);
	}
	return Things;
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
	const bool bBoards = E->SellsBoards(n, U);   // a sports or toy outlet sells the three skateboards too (ADR-0054)
	if (bBoards) Things.Append(BoardWares(Band));
	FGridOpts Op;
	Op.Id = FString::Printf(TEXT("outlet:%d:%s"), n, *U);
	Op.StockLabel = TEXT("In stock");
	Op.Buy = [this, n, U](const FWare& W, FString& Said)
	{
		if (W.Id.StartsWith(TEXT("board:"))) return BoardBuy(n, U, W.Id.RightChop(6), Said);
		int64 P = 0;
		if (!E->BuyAt(n, U, W.Id, &P)) return false;
		const FBrixEconomyRules::FGood Gd = E->Good(W.Id);
		Said = TEXT("Bought ") + Gd.Name.ToLower() + TEXT(" for ") + Format(P) + (Gd.Goes == TEXT("home") ? TEXT(". It is with your belongings (G), to place at home.") : Gd.Goes == TEXT("wear") ? TEXT(". It is in your wardrobe (U).") : TEXT("."));
		return true;
	};
	Body->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[ Grid(Things, Op) ];
	Body->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[ UiText(FString(TEXT("Things for the home go to your belongings (G): place them from the Furnish tab of the building designer, in any home of yours. Clothes go to your wardrobe (U). "))
		+ (bBoards ? TEXT("A skateboard goes with you among your belongings, to ride (Y) or to put on show at home. ") : TEXT("")) + TEXT("Outlets do not buy things back; what you carry can be sold on your market stall."), 15, UiDim) ];
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
	auto Flag = [](const FString& State) -> TSharedRef<SWidget>
	{
		const bool bOut = State == TEXT("out"), bHave = State == TEXT("have");
		if (!bOut && !bHave) return SNullWidget::NullWidget;
		return SNew(SBox).VAlign(VAlign_Center)
			[ SNew(SBorder).BorderImage(UiWhite()).BorderBackgroundColor(FLinearColor::FromSRGBColor(FColor(122, 66, 52))).Padding(FMargin(0, 3)).HAlign(HAlign_Center).RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(-4.f))))
				[ SNew(STextBlock).Font(UiFont(10, true)).ColorAndOpacity(FLinearColor::FromSRGBColor(FColor(251, 243, 224))).Text(FText::FromString(bHave ? TEXT("YOURS") : TEXT("SOLD OUT"))) ] ];
	};
	// layout D (buymenu.js grid(), narrow): small cells a few across, the chosen one turning; under them a strip with its name,
	// where it goes, what is left, its size, its price and the one button. For a panel narrower than the rules' narrow_px
	// (the building designer's card).
	if (O.bNarrow)
	{
		const int32 NCols = FMath::Max(1, BuyMenu.NarrowColumns);
		const float NGap = 6.f, NCell = FMath::Max((float)BuyMenu.NarrowCellMinPx, (O.Width - NGap * (NCols - 1)) / NCols);
		FLive NLive;
		NLive.Id = O.Id; NLive.Things = Things; NLive.O = O; NLive.Columns = NCols;
		LiveGrid = NLive;
		auto NBoard = [this](const FString& Goes) { const FBrixBuyMenuRules::FGoes* G = BuyMenu.Goes.Find(Goes); const TPair<FColor, FColor>* C = G ? BuyMenu.BoardColours.Find(G->Colour) : nullptr; return C ? *C : TPair<FColor, FColor>(FColor(59, 60, 74), FColor(232, 223, 201)); };
		TSharedRef<SGridPanel> NList = SNew(SGridPanel);
		for (int32 k = 0; k < Things.Num(); ++k)
		{
			const FWare W = Things[k];
			const bool bSel = W.Id == P.Id, bOut = W.State == TEXT("out") || W.State == TEXT("have");
			const TPair<FColor, FColor> Dot = NBoard(W.Goes);
			TSharedRef<SWidget> CountMark = SNullWidget::NullWidget, DotMark = SNullWidget::NullWidget;
			if (W.Left > 0) CountMark = SNew(STextBlock).Visibility(EVisibility::HitTestInvisible).Font(UiFont(9, true)).ColorAndOpacity(W.Left == 1 && !O.bMine ? UiGold : UiDim).Text(FText::FromString(FString::Printf(TEXT("×%d"), W.Left)));
			if (BuyMenu.Goes.Contains(W.Goes))   // .bm-narrow .bm-dot: where it goes
				DotMark = SNew(SBox).Visibility(EVisibility::HitTestInvisible).WidthOverride(15).HeightOverride(15)
				[
					SNew(SBorder).BorderImage(BrixLook::Round(FLinearColor::FromSRGBColor(Dot.Key), 7.5f)).Padding(0).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[ UiMark(BuyMenu, W.Goes, 9.f, FLinearColor::FromSRGBColor(Dot.Value)) ]
				];
			NList->AddSlot(k % NCols, k / NCols).Padding(k % NCols ? NGap : 0.f, k / NCols ? NGap : 0.f, 0.f, 0.f)
			[
				SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).IsFocusable(false).ContentPadding(0).ToolTipText(FText::FromString(W.Name))
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
					SNew(SBox).WidthOverride(NCell)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBorder).BorderImage(BrixLook::Round(bSel ? UiLantern : BrixLook::StoneDeep(), 8.f)).Padding(bSel ? 2 : 1)
							[
								SNew(SOverlay)   // .bm-narrow .bm-stage: square
								+ SOverlay::Slot()[ SNew(SBrixThing).Mesh(MeshFor(W.Id, W.Band, W.Line)).Rules(&BuyMenu).Spin(bSel && !bOut).Grey(bOut).Size(FVector2D(NCell - (bSel ? 4 : 2), NCell - (bSel ? 4 : 2))).Yaw(YawFor(O.Id + TEXT(":") + W.Id + TEXT(":") + W.Line)) ]
								+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(4, 3, 0, 0)[ CountMark ]
								+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0, 3, 3, 0)[ DotMark ]
								+ SOverlay::Slot().VAlign(VAlign_Center)[ Flag(W.State) ]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 3, 0, 0)[ BrixLook::Tag(Format(W.Price), W.State == TEXT("short"), 10) ]
					]
				]
			];
		}
		const FGridOpts NOp = O;
		TSharedRef<SWrapBox> Facts = UiWrap(FMath::Max(120.f, O.Width - 20.f - 170.f), 10.f);
		if (BuyMenu.Goes.Contains(P.Goes)) Facts->AddSlot().VAlign(VAlign_Center)[ Chip(P.Goes) ];
		if (P.Left >= 0) Facts->AddSlot().VAlign(VAlign_Center)[ UiText(LeftWords(P), 11, P.State == TEXT("out") ? UiNeg : P.Left == 1 && !O.bMine ? UiGold : UiDim) ];
		if (P.Size) Facts->AddSlot().VAlign(VAlign_Center)[ UiText(FString::Printf(TEXT("%d × %d × %d cm"), FMath::RoundToInt(P.Size->X * 100), FMath::RoundToInt(P.Size->Y * 100), FMath::RoundToInt(P.Size->Z * 100)), 10, UiDim) ];
		TSharedRef<SHorizontalBox> End = SNew(SHorizontalBox);
		if (P.State == TEXT("short")) End->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[ UiText(Format(P.Short) + TEXT(" short"), 10, UiNeg) ];
		End->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[ Price(P) ];
		End->AddSlot().AutoWidth().VAlign(VAlign_Center)[ BrixLook::Button(O.Verb, [this, NOp, P]() { Act([NOp, P](FString& S) { return NOp.Buy(P, S); }); }, true, P.State == TEXT("ok"), UiLantern, UiInk, 13) ];
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ SNew(SBox).MaxDesiredHeight((float)BuyMenu.NarrowListPx)[ SNew(SScrollBox) + SScrollBox::Slot()[ NList ] ] ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
			[
				SNew(SBorder).BorderImage(BrixLook::Round(FLinearColor::FromSRGBColor(FColor(217, 207, 182, 18)), 6.f)).Padding(FMargin(10, 8))   // .bm-strip
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ UiText(P.Name, 14) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[ Facts ]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)[ End ]
					]
				]
			];
	}
	TSharedRef<SGridPanel> List = SNew(SGridPanel);
	for (int32 k = 0; k < Things.Num(); ++k)
	{
		const FWare W = Things[k];
		const bool bSel = W.Id == P.Id, bOut = W.State == TEXT("out") || W.State == TEXT("have");
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
							+ SOverlay::Slot()[ SNew(SBrixThing).Mesh(MeshFor(W.Id, W.Band, W.Line)).Rules(&BuyMenu).Grey(bOut).Size(FVector2D(CellW - 4, BuyMenu.StagePx)).Yaw(YawFor(O.Id + TEXT(":") + W.Id)) ]
							+ SOverlay::Slot().VAlign(VAlign_Center)[ Flag(W.State) ]
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
	for (const TPair<FString, FString>& Mo : P.More) Line(Mo.Key, UiText(Mo.Value, 14));
	if (BuyMenu.Goes.Contains(P.Goes)) Line(TEXT("Goes to"), Chip(P.Goes));
	if (P.State == TEXT("have")) Line(TEXT("Yours"), UiText(TEXT("you have this one"), 14, UiPos));
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
				+ SOverlay::Slot()[ SNew(SBrixThing).Mesh(MeshFor(P.Id, P.Band, P.Line)).Rules(&BuyMenu).Spin(true).Drag(true).Grey(P.State == TEXT("out") || P.State == TEXT("have")).Size(FVector2D(StageW, BuyMenu.ShowStagePx)).Yaw(YawFor(O.Id + TEXT(":show:") + P.Id)) ]
				+ SOverlay::Slot().VAlign(VAlign_Center)[ Flag(P.State) ]
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
	if (Q->TalkingTo(Rid).IsEmpty())   // QST-011: a story line has no offer talk: it starts when its giver is spoken to
		for (const FString& Qid : Q->Offers(Rid))
			if (QuestRules.Find(Qid)->Opens()) { Q->Start(Qid); break; }
	const TArray<FString> Talks = Q->TalkingTo(Rid);
	if (Talks.Num())   // this resident is the one to see for a quest stage: that talk, nothing else
		return StageTalk(Talks[0]);
	FBrixDialogue::FConversation C = Base;
	C.Vars.Reset();
	TArray<FBrixDialogue::FOption> Extra;
	for (const FString& Qid : Q->Offers(Rid))   // quest lines this resident offers
	{
		const FBrixQuestLine* L = QuestRules.Find(Qid);
		if (L->Opens()) continue;
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

FBrixDialogue::FConversation ABrixLife::StageTalk(const FString& Qid) const
{
	FBrixDialogue::FConversation C;
	const FBrixQuestStage* St = Q ? Q->Stage(Qid) : nullptr;
	if (!St) return C;
	C.Title = QuestRules.Find(Qid)->Title;
	C.Start = TEXT("q:") + Qid + TEXT(":") + Q->FirstLine(Qid);   // the first line whose needs hold (QST-011)
	for (const auto& N : St->Nodes)
	{
		FBrixDialogue::FNode X = N.Value;
		for (FBrixDialogue::FOption& O : X.Options) if (!O.Next.IsEmpty()) O.Next = TEXT("q:") + Qid + TEXT(":") + O.Next;
		if (!X.Who.IsEmpty() && X.Who != TEXT("scene"))   // QST-012: somebody else says it: their name goes before the line
		{
			const int32 w = Q->ResidentIndex(X.Who);
			if (Day && Day->Names.IsValidIndex(w)) X.Say = Day->Names[w] + TEXT(": ") + X.Say;
		}
		if (!X.Dir.IsEmpty()) X.Say = TEXT("(") + X.Dir + TEXT(")\n") + X.Say;   // what is seen, above what is said
		C.Nodes.Add(TEXT("q:") + Qid + TEXT(":") + N.Key, X);
	}
	return C;
}

void ABrixLife::PlayScene()
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Q || !Mode || !Mode->Talk || Mode->Talk->IsTalking() || IsOpen()) return;
	const TArray<FString> Here = Q->Scenes(Ctx());
	if (Here.IsEmpty()) { SceneSeen.Reset(); return; }
	const FString Key = Here[0] + TEXT(":") + Q->ActiveOf(Here[0])->Stage;
	if (Key == SceneSeen) return;   // left unanswered: it opens again when the player has gone and come back
	SceneSeen = Key;
	Mode->Talk->OpenScene(StageTalk(Here[0]));
}

bool ABrixLife::Allowed(const FBrixDialogue::FOption& O) const { return !Q || Q->NeedOk(O.Need); }

void ABrixLife::Chose(int32 i, const FBrixDialogue::FOption& O)
{
	if (!Q || (!QuestResidents.IsValidIndex(i) && O.Quest.IsEmpty())) return;   // i is -1 in a scene: nobody is spoken to (QST-012)
	bool bOk = true;
	if (!O.Quest.IsEmpty())
	{
		TArray<FString> Acts = O.Act;
		Acts.RemoveAll([](const FString& A) { return A == TEXT("shop") || A == TEXT("bank"); });
		bOk = Q->Act(O.Quest, Acts, QuestResidents.IsValidIndex(i) ? QuestResidents[i].Id : FString());
	}
	if (bOk && O.Job == TEXT("take") && Offered) bOk = Q->TakeJob(*Offered);
	if (bOk && O.Job == TEXT("hand")) bOk = Q->HandOver();
	if (!bOk) Toast(E->Why);
	if (O.Act.Contains(TEXT("shop")) && QuestResidents.IsValidIndex(i)) PendingCounter = i;
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
