// The game menu (UI-005 to UI-019; F-UI-MENU level 2, F-UI-SETTINGS level 1) in the "Lamplight" look (ADR-0049): Esc opens
// it on its home of cards, one for each page with the page's headline; a page's key opens that page. Down the left the
// pages stand in their groups (You, Town, Game), each with its colour; along the top who is playing, the day, the time,
// the cash and the way back to town. While it is open the clock stands still (settings.json menu.pauses).
#include "BrixLifeActor.h"
#include "BrixMapActor.h"

#include "BrixCharacter.h"
#include "BrixDesignerActor.h"
#include "BrixDay.h"
#include "BrixGameMode.h"
#include "BrixTalkActor.h"
#include "BrixLook.h"
#include "BrixThingWidget.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "Containers/Ticker.h"
#include "Misc/Paths.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"

using namespace BrixLook;
using BrixMoney::Format;

namespace
{
	FString MenuHhmm(double M) { const int32 m = (int32)M; return FString::Printf(TEXT("%02d:%02d"), (m / 60) % 24, m % 60); }
	FLinearColor Lin(const FColor& C) { return FLinearColor::FromSRGBColor(C); }
}

ABrixDesigner* ABrixLife::Designer() const
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	return Mode ? Mode->Designer.Get() : nullptr;
}

FString ABrixLife::SettingsPath() const { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BrixTown"), TEXT("settings.json")); }

void ABrixLife::StartMenu()
{
	FString Error;
	if (!LoadBrixSettings(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/settings.json"))), Settings, Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("Brix Town: no menu settings (%s)"), *Error);
		return;
	}
	FString Text;
	SetPalette(Settings.Lamplight);   // the look's colours are data (ADR-0049)
	if (FFileHelper::LoadFileToString(Text, *SettingsPath())) Settings.Load(Text);   // the device's own settings (UI-009)
	ApplySettings();
	if (!GEngine || !GEngine->GameViewport) return;
	SAssignNew(MenuRoot, SOverlay).Visibility_Lambda([this]() { return IsMenu() ? EVisibility::Visible : EVisibility::Collapsed; })
		+ SOverlay::Slot()[ SNew(SBorder).BorderImage(Round(Veil(), 0.f)).Visibility_Lambda([this]() { return MenuTab == TEXT("wardrobe") ? EVisibility::Collapsed : EVisibility::HitTestInvisible; }) ]
		+ SOverlay::Slot()[ SAssignNew(MenuBody, SBox) ];
	GEngine->GameViewport->AddViewportWidgetContent(MenuRoot.ToSharedRef(), 30);
}

const FBrixMenuTab* ABrixLife::Tab(const FString& Id) const { return Settings.Tabs.FindByPredicate([&](const FBrixMenuTab& T) { return T.Id == Id; }); }

FLinearColor ABrixLife::TabColour(const FString& Id, bool bLetters) const
{
	const FBrixMenuTab* T = Tab(Id);
	const TPair<FColor, FColor>* C = T ? Settings.Colours.Find(T->Colour) : nullptr;
	return C ? Lin(bLetters ? C->Value : C->Key) : (bLetters ? Cream() : StoneLift());
}

FString ABrixLife::TabKey(const FString& Id) const
{
	const FBrixMenuTab* T = Tab(Id);
	const TMap<FString, TArray<FString>>* K = Settings.Platforms.Find(TEXT("keyboard"));
	const TArray<FString>* Keys = T && K ? K->Find(T->Action) : nullptr;
	return Keys && Keys->Num() && T->Action != TEXT("menu") ? (*Keys)[0] : FString();
}

void ABrixLife::Menu(const FString& Id)
{
	if (!E || !Settings.Tabs.Num()) return;
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (Mode && Mode->Talk && Mode->Talk->IsTalking()) return;
	if (!Panel.IsEmpty()) Close();
	if (Id != TEXT("map") && Mode && Mode->Map && Mode->Map->IsOpen()) Mode->Map->Close();   // a page over the map: the map goes first
	if (Id == TEXT("map") && Mode && Mode->Map)   // the map is the town's own big map (life.js panel("map"))
	{
		CloseMenu();
		Mode->Map->Toggle();
		return;
	}
	if (bMenu && MenuTab == Id) { CloseMenu(); return; }   // a page's key again: back to town
	MenuTab = Id;
	Msg.Reset();
	if (!bMenu)
	{
		bMenu = true;
		if (ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0))) Me->BeginPanel();
		if (Settings.bPauses) UGameplayStatics::SetGamePaused(this, true);   // the clock stands still (UI-005)
	}
	DrawMenu();
}

// -BrixMenu=<page> (home, me, wallet, quests, belongings, settings, help...; with -BrixSettingsGroup=<n>): to see a page in a
// test shot. The menu stops the clock and the game's timers with it, so the shot is taken from here, by the real clock.
void ABrixLife::TestMenu(const FString& Page)
{
	int32 G = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixSettingsGroup="), G)) SettingsGroup = G;
	Menu(Page == TEXT("home") ? FString() : Page);
	MenuShotAt = FPlatformTime::Seconds() + 2.5;
}

void ABrixLife::TickMenuShot()
{
	if (MenuShotAt < 0 || FPlatformTime::Seconds() < MenuShotAt) return;
	FString Shot;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixShot="), Shot)) FScreenshotRequest::RequestScreenshot(Shot, true, false);
	UE_LOG(LogTemp, Display, TEXT("Brix Town: the menu at %s, shot"), MenuTab.IsEmpty() ? TEXT("home") : *MenuTab);
	MenuShotAt = -1;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { FPlatformMisc::RequestExit(false); return false; }), 2.f);
}

void ABrixLife::MenuKey(const FString& Action)
{
	for (const FBrixMenuTab& T : Settings.Tabs) if (T.Action == Action) { Menu(T.Id); return; }
}

void ABrixLife::CloseMenu()
{
	if (!bMenu) return;
	bMenu = false;
	if (ABrixDesigner* Dz = Designer()) Dz->WardrobeView(false);
	if (MenuBody) MenuBody->SetContent(SNullWidget::NullWidget);
	UGameplayStatics::SetGamePaused(this, false);
	if (ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0))) Me->EndPanel();
}

FString ABrixLife::Headline(const FString& Id) const   // what a page's card says on the home (life.js headline())
{
	if (Id == TEXT("me")) return FString::Printf(TEXT("Day %d in %s"), E->Day + 1, *CityName);
	if (Id == TEXT("wallet"))
	{
		FString S = Format(E->TotalMoney()) + TEXT(" in all");
		if (const FBrixLoan* L = E->Loans.FindByPredicate([](const FBrixLoan& X) { return X.Status == TEXT("active"); })) S += TEXT(" · owe ") + Format(L->Owed());
		return S;
	}
	if (Id == TEXT("quests"))
	{
		int32 Offers = 0;
		if (Q) for (const FBrixQuestLine& L : QuestRules.Quests) Offers += !L.Giver.IsEmpty() && Q->Offers(L.Giver).Contains(L.Id);
		return Q ? FString::Printf(TEXT("%d under way · %d on offer"), Q->Active.Num() + (Q->Job ? 1 : 0), Offers) : FString();
	}
	if (Id == TEXT("belongings")) { int32 n = 0; for (const auto& I : E->Inv) n += I.Value; return n ? FString::Printf(TEXT("%d things carried"), n) : TEXT("Nothing carried"); }
	if (Id == TEXT("property"))   // life.js headline(): the buildings owned and their worth, or the places rented
	{
		if (!Est) return TEXT("Nothing owned yet");
		const TArray<int32> Mine = Est->Owned();
		TSet<int32> LotsOwned;
		int64 Worth = 0;
		for (const int32 i : Mine) { LotsOwned.Add(Est->Units[i].Lot); Worth += Est->Value(i); }
		const int32 Rents = Est->Tenancy.Num();
		if (LotsOwned.Num()) return FString::Printf(TEXT("%d building%s · worth about %s"), LotsOwned.Num(), LotsOwned.Num() == 1 ? TEXT("") : TEXT("s"), *Format(Worth));
		return Rents ? FString::Printf(TEXT("%d place%s rented"), Rents, Rents == 1 ? TEXT("") : TEXT("s")) : FString(TEXT("None owned yet"));
	}
	if (Id == TEXT("wardrobe")) return Designer() ? Designer()->WardrobeHeadline() : FString(TEXT("What you wear"));
	if (Id == TEXT("map"))   // life.js headline(): the mark and how far it is
	{
		const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
		return Mode && Mode->Map && Mode->Map->Mark.bSet ? TEXT("Mark: ") + Mode->Map->Mark.Name + FString::Printf(TEXT(", %d m"), (int32)FMath::RoundHalfFromZero(FVector2D::Distance(Mode->Map->Mark.Pos, Mode->Map->Player))) : FString(TEXT("No mark: click the map to set one"));
	}
	if (Id == TEXT("storage")) return TEXT("Not yet");
	if (Id == TEXT("settings")) return TEXT("Picture, sound, controls");
	if (Id == TEXT("save")) return Designer() ? Designer()->SaveHeadline() : FString(TEXT("Three slots"));
	if (Id == TEXT("help")) return TEXT("The controls");
	return FString();
}

void ABrixLife::DrawMenu()
{
	if (!bMenu || !MenuBody) return;
	const bool bHome = MenuTab.IsEmpty();
	double D0 = 0, M0 = 0;
	Frame(D0, M0);
	// the bar along the top: who, the day, the time, the cash, back to town (UI-015)
	TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)[ Tile(Lantern(), PlayerName.Left(1).ToUpper(), Ink(), 30.f) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 22, 0)[ Text(PlayerName, 17, Cream(), true, false) ]   // the designed character's name (CHR-001)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 22, 0)[ Text(FString::Printf(TEXT("Day %d · %s · %s"), E->Day + 1, Day ? *Day->Weekday : TEXT(""), Day ? *Day->Season : TEXT("")), 14, Dim(), false, false) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 22, 0)[ Text(MenuHhmm(M0) + (Settings.bPauses ? TEXT("  (the clock stands still)") : TEXT("")), 14, Dim(), false, false) ]
		+ SHorizontalBox::Slot().FillWidth(1)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 18, 0)[ Tag(Format(E->L.Balance(TEXT("p:cash")) - HeldCash), false, 14) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Button(TEXT("Back to town  ·  Esc"), [this]() { CloseMenu(); }, false) ];
	// the pages down the left, in their groups (UI-015, UI-016)
	TSharedRef<SVerticalBox> Rail = SNew(SVerticalBox);
	auto RailItem = [this, &Rail](const FString& Id, const FString& Name, const FLinearColor& Col, const FLinearColor& Letters, const FString& Key)
	{
		const bool bOn = MenuTab == Id;
		Rail->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).IsFocusable(false).ContentPadding(0)
			.OnClicked_Lambda([this, Id]() { MenuTab = Id; DrawMenu(); return FReply::Handled(); })
			[
				SNew(SBorder).BorderImage(Round(bOn ? Col : FLinearColor::Transparent, 11.f)).Padding(FMargin(8, 6))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)[ Tile(bOn ? Letters * FLinearColor(1, 1, 1, .25f) : Col, Name.Left(1), bOn ? Col : Letters, 24.f) ]
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[ Text(Name, 14, bOn ? Letters : Cream(), bOn, false) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Text(Key, 11, bOn ? Letters : Dim(), true, false) ]
				]
			]
		];
	};
	RailItem(FString(), TEXT("Home"), Lantern(), Ink(), TEXT("Esc"));
	for (const auto& G : Settings.MenuGroups)
	{
		Rail->AddSlot().AutoHeight().Padding(8, 12, 0, 6)[ Caps(G.Value) ];
		for (const FBrixMenuTab& T : Settings.Tabs) if (T.Group == G.Key) RailItem(T.Id, T.Name, TabColour(T.Id, false), TabColour(T.Id, true), TabKey(T.Id));
	}
	TSharedRef<SWidget> Page = bHome ? HomePage() : MenuPage(MenuTab);
	const bool bWardrobe = MenuTab == TEXT("wardrobe") && Designer();   // the page to one side, so all of the character shows beside it (life.js side)
	TSharedPtr<SWidget> PageBox;
	if (bWardrobe) Page = SAssignNew(PageBox, SBox).WidthOverride(Designer()->WardrobeWidth)[ Page ];
	if (ABrixDesigner* Dz = Designer()) Dz->WardrobeView(bWardrobe, PageBox);
	MenuBody->SetContent(
		SNew(SBox).Padding(FMargin(28, 22))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)[ Card(Bar, FMargin(16, 10)) ]
			+ SVerticalBox::Slot().FillHeight(1)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 18, 0)[ SNew(SBox).WidthOverride(212.f)[ Card(SNew(SScrollBox) + SScrollBox::Slot()[ Rail ], FMargin(10, 12)) ] ]
				+ SHorizontalBox::Slot().FillWidth(1)[ bWardrobe ? (TSharedRef<SWidget>)(SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[ Page ] + SHorizontalBox::Slot().FillWidth(1)) : Page ]
			]
		]);
}

// the home: a card for every page, its colour as a lintel over it, its headline, its key (UI-015)
TSharedRef<SWidget> ABrixLife::HomePage()
{
	TSharedRef<SWrapBox> W = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(16, 16));
	for (const auto& G : Settings.MenuGroups)
		for (const FBrixMenuTab& T : Settings.Tabs)
		{
			if (T.Group != G.Key) continue;
			const bool bSmall = T.Board == TEXT("small"), bTall = T.Board == TEXT("tall");
			const FLinearColor Col = TabColour(T.Id, false), Let = TabColour(T.Id, true);
			const FString Id = T.Id;
			W->AddSlot()
			[
				SNew(SBox).WidthOverride(bSmall ? 196.f : 290.f).HeightOverride(bSmall ? 104.f : bTall ? 200.f : 150.f)
				[
					SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).IsFocusable(false).ContentPadding(0)
					.OnClicked_Lambda([this, Id]() { MenuTab = Id; DrawMenu(); return FReply::Handled(); })
					[
						SNew(SBorder).BorderImage(Round(StoneDeep(), 16.f)).Padding(FMargin(0, 0, 0, 5))
						[
							SNew(SBorder).BorderImage(Round(Stone(), 16.f, Srgb(255, 236, 200, 18), 1.f)).Padding(0)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()   // the lintel in the page's colour
								[
									SNew(SBorder).BorderImage(Round(Col, 15.f)).Padding(FMargin(14, bSmall ? 8 : 12))
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[ Text(T.Name, bSmall ? 16 : 21, Let, true, false) ]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Text(TabKey(Id), 12, Let, true, false) ]
									]
								]
								+ SVerticalBox::Slot().FillHeight(1).Padding(FMargin(14, 10)).VAlign(VAlign_Center)[ Text(Headline(Id), bSmall ? 12 : 15, Cream()) ]
							]
						]
					]
				]
			];
		}
	return SNew(SScrollBox) + SScrollBox::Slot()
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4, 0, 0, 14)[ Text(TEXT("Good to see you."), 26, Cream(), true, false) ]
		+ SVerticalBox::Slot().AutoHeight()[ W ]
	];
}

// a page: its header in its colour, its sections as cards
TSharedRef<SWidget> ABrixLife::MenuPage(const FString& Id)
{
	const FBrixMenuTab* T = Tab(Id);
	const FLinearColor Col = TabColour(Id, false), Let = TabColour(Id, true);
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	auto Section = [&Body](const FString& Title, TSharedRef<SWidget> Content)
	{
		Body->AddSlot().AutoHeight().Padding(0, 0, 0, 14)
		[ Card(SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, Title.IsEmpty() ? 0 : 8)[ Title.IsEmpty() ? SNullWidget::NullWidget : Caps(Title) ] + SVerticalBox::Slot().AutoHeight()[ Content ]) ];
	};
	auto Lines = [](const TArray<TPair<FString, FString>>& Rows) -> TSharedRef<SWidget>
	{
		TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
		for (int32 k = 0; k < Rows.Num(); ++k)
		{
			if (k) V->AddSlot().AutoHeight()[ Rule() ];
			V->AddSlot().AutoHeight().Padding(0, 6)
			[ SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1)[ Text(Rows[k].Key, 14) ] + SHorizontalBox::Slot().AutoWidth().Padding(12, 0, 0, 0)[ Text(Rows[k].Value, 14, Cream(), true, false) ] ];
		}
		return V;
	};
	auto Soon = [&Section](const FString& Words) { Section(FString(), Text(Words, 15, Dim())); };
	FString Sub;
	if (Id == TEXT("me"))   // UI-011, UI-019
	{
		Sub = TEXT("Who you are and how the town sees you");
		TArray<TPair<FString, FString>> Have = { { TEXT("Money in all"), Format(E->TotalMoney()) }, { TEXT("Things carried"), Headline(TEXT("belongings")) } };
		if (FBrixLoan* L = E->ActiveLoan()) Have.Add({ TEXT("Owed to the bank"), Format(L->Owed()) });
		if (Q) { Have.Add({ TEXT("Quests finished"), FString::FromInt(Q->Done.Num()) }); Have.Add({ TEXT("Under way"), FString::FromInt(Q->Active.Num() + (Q->Job ? 1 : 0)) }); }
		Section(TEXT("What you have"), Lines(Have));
		TSharedRef<SVerticalBox> A = SNew(SVerticalBox);
		int32 Earned = 0;
		for (int32 Pass = 0; Pass < 2; ++Pass)   // those earned first, with the day; then those still to earn, dimmed
			for (const FBrixAchievement& X : AchievementRules.List)
			{
				const int32* D = Feats.Find(X.Id);
				if ((Pass == 0) != (D != nullptr)) continue;
				Earned += D != nullptr;
				A->AddSlot().AutoHeight().Padding(0, 4)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)[ Tile(D ? Srgb(201, 162, 74) : StoneLift(), D ? TEXT("★") : TEXT("·"), D ? Ink() : Dim(), 22.f) ]
					+ SHorizontalBox::Slot().FillWidth(1)[ SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[ Text(X.Name, 14, D ? Cream() : Dim(), true) ] + SVerticalBox::Slot().AutoHeight()[ Text(X.Line, 12, Dim()) ] ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Text(D ? FString::Printf(TEXT("Day %d"), *D + 1) : FString(), 12, Dim(), false, false) ]
				];
			}
		Section(FString::Printf(TEXT("Achievements · %d of %d"), Earned, AchievementRules.List.Num()), A);
		TArray<TPair<FString, FString>> People;
		for (const auto& R : E->Rep) if (R.Value && Q) { const int32 i = Q->ResidentIndex(R.Key); People.Add({ Day && Day->Names.IsValidIndex(i) ? Day->Names[i] : R.Key, (R.Value > 0 ? TEXT("+") : TEXT("")) + FString::FromInt(R.Value) }); }
		if (People.Num()) Section(TEXT("People you know"), Lines(People));
	}
	else if (Id == TEXT("wallet"))   // ECO-009
	{
		Sub = TEXT("Cash, accounts, loans and every line of the statement");
		TArray<TPair<FString, FString>> Acc = { { TEXT("Cash"), Format(E->L.Balance(TEXT("p:cash"))) } };
		for (const FString& K : E->Accounts) Acc.Add({ E->R.Account(K)->Name, Format(E->L.Balance(TEXT("p:") + K)) });
		if (FBrixLoan* L = E->ActiveLoan()) Acc.Add({ TEXT("Loan owed"), Format(L->Owed()) });
		Section(TEXT("Accounts"), Lines(Acc));
		Section(TEXT("Statement"), StatementTable(WalletStatementLines));
	}
	else if (Id == TEXT("quests")) { Sub = TEXT("Quests and work under way, on offer and finished"); TSharedRef<SVerticalBox> V = SNew(SVerticalBox); QuestsPanel(V); Section(FString(), V); }
	else if (Id == TEXT("belongings"))   // INV-001: what is carried, each drawn as it is in a buy menu
	{
		Sub = TEXT("What you carry");
		TArray<FString> Keys;
		E->Inv.GetKeys(Keys);
		Keys.Sort([](const FString& A, const FString& B) { return FCString::Strcmp(*A, *B) < 0; });
		if (!Keys.Num()) Soon(TEXT("You carry nothing yet. Shops and cafés sell things (B at their counter); some quests give things too."));
		else
		{
			TSharedRef<SWrapBox> W = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12, 12));
			for (const FString& K : Keys)
				W->AddSlot()
				[
					SNew(SBox).WidthOverride(132.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[ SNew(SBorder).BorderImage(Round(StoneDeep(), 10.f)).Padding(0)[ SNew(SBrixThing).Mesh(MeshFor(K, TEXT("ochre"))).Rules(&BuyMenu).Size(FVector2D(132, 96)).Yaw(YawFor(TEXT("belongings:") + K)) ] ]
						+ SVerticalBox::Slot().AutoHeight().Padding(2, 6, 2, 0)[ SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1)[ Text(E->Item(K) ? E->Item(K)->Name : K, 12) ] + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[ Tag(FString::Printf(TEXT("×%d"), E->Inv[K]), false, 11) ] ]
					]
				];
			Section(FString::Printf(TEXT("Carried · %d kinds"), Keys.Num()), W);
		}
		if (FurnHeld.Num())   // INV-001: the furniture taken away in the designer, to place again in any home of yours (life.js belongingsTab())
		{
			TArray<TPair<FString, FString>> Fl;
			for (const FBrixLayoutPiece& P : FurnHeld) Fl.Add({ P.Name, FString::Printf(TEXT("%.2f × %.2f m"), P.W, P.D) });
			Section(FString::Printf(TEXT("Furniture · %d pieces · place them from the building designer's Furnish tab"), FurnHeld.Num()), Lines(Fl));
		}
		// CITY-100: the skateboards owned (life.js belongingsTab()), each with its top speed and how it goes, the one in use
		// marked (in the hand or ridden); any other made the one in use
		if (ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
		{
			TSharedRef<SVerticalBox> Sk = SNew(SVerticalBox);
			for (int32 k = 0; k < Me->Boards.Own.Num(); ++k)
			{
				const FString Kind = Me->Boards.Own[k];
				const FBrixBoardKind* K = Me->BoardRules.Kind(Kind);
				if (!K) continue;
				const bool bMine = Me->Boards.InUse == Kind;
				if (k) Sk->AddSlot().AutoHeight()[ Rule() ];
				Sk->AddSlot().AutoHeight().Padding(0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)[ SNew(SBorder).BorderImage(Round(StoneDeep(), 10.f)).Padding(0)[ SNew(SBrixThing).Mesh(MeshFor(TEXT("board:") + Kind, FString())).Rules(&BuyMenu).Size(FVector2D(110, 62)).Yaw(YawFor(TEXT("belongings:board:") + Kind)) ] ]
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[ Text(K->Name, 14, Cream(), true) ]
						+ SVerticalBox::Slot().AutoHeight()[ Text(FString::Printf(TEXT("up to %d km/h"), (int32)K->TopKmh) + (K->bPush ? TEXT(", pushed with a foot") : TEXT("")) + (bMine ? (Me->Boards.Riding() ? TEXT(" · riding it") : TEXT(" · in your hand")) : TEXT("")), 12, Dim()) ]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						bMine ? (TSharedRef<SWidget>)Text(TEXT("In use"), 13, Dim(), false, false)
						: Button(TEXT("Use this"), [this, Me, Kind, K]() { Me->Boards.Use(Kind); Toast(TEXT("Now using the ") + K->Name.ToLower() + TEXT(".")); DrawMenu(); }, false)
					]
				];
			}
			if (Me->Boards.Own.Num()) Section(TEXT("Skateboards · Y gets on the one in use"), Sk);
			else Soon(TEXT("You have no skateboard. ") + BoardsSoldWhere() + TEXT(" sell three kinds, from a board you push with a foot to the Neon Boost."));
		}
	}
	else if (Id == TEXT("property"))   // PROP-011 to PROP-019, UI-006 (life.js estatePanel(), kind "property": P anywhere)
	{
		Sub = TEXT("What you own and rent · P");
		TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
		if (!Msg.IsEmpty()) V->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[ SNew(SBorder).BorderImage(Round(FLinearColor::FromSRGBColor(FColor(240, 195, 90, 31)), 10.f)).Padding(FMargin(12, 7))[ Text(Msg, 15) ] ];
		if (Est) EstatePanel(V, FString(), LotHere());
		else V->AddSlot().AutoHeight()[ Text(TEXT("The property market is not in this game's data (run Tools/worldgen/build_play.py)."), 15, Dim()) ];
		Section(FString(), V);
	}
	else if (Id == TEXT("wardrobe"))   // CHR-014 to CHR-016: the designer's own choices, outfits and the clothes bought
	{
		Sub = PlayerName + TEXT(" · changing is free");
		if (ABrixDesigner* Dz = Designer()) Dz->WardrobePage(Body, E->Clothes, [this]() { DrawMenu(); });
	}
	else if (Id == TEXT("storage")) { Sub = TEXT("Lockers and storage units"); Soon(TEXT("Storage comes when F-PROP-STORAGE is picked (UI-014). Until then what you have is in Belongings.")); }
	else if (Id == TEXT("map")) { Sub = TEXT("The town"); Soon(TEXT("The big map with its filters and a mark (UI-012, UI-018) is not in this build yet. Its filters are already in Settings, under The map.")); }
	else if (Id == TEXT("settings")) { Sub = TEXT("Kept on this device, not in your save"); SettingsPage(Body); }
	else if (Id == TEXT("save")) { Sub = TEXT("Your games"); SavePage(Body); }
	else if (Id == TEXT("help")) { Sub = TEXT("The controls of keyboard and mouse, by where you are"); HelpPage(Body); }
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
		[
			SNew(SBorder).BorderImage(Round(Col * .72f + FLinearColor(0, 0, 0, .28f), 16.f)).Padding(FMargin(0, 0, 0, 5))
			[
				SNew(SBorder).BorderImage(Round(Col, 16.f)).Padding(FMargin(22, 14))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1)
					[ SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[ Text(T ? T->Name : Id, 28, Let, true, false) ] + SVerticalBox::Slot().AutoHeight()[ Text(Sub, 13, Let * FLinearColor(1, 1, 1, .8f)) ] ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Text(TabKey(Id), 16, Let, true, false) ]
				]
			]
		]
		+ SVerticalBox::Slot().FillHeight(1)[ SNew(SScrollBox) + SScrollBox::Slot()[ Body ] ];
}

// UI-009: one group at a time; a switch flips, a choice goes round, a range steps; each takes effect at once and is kept
void ABrixLife::SettingsPage(TSharedRef<SVerticalBox> Body)
{
	if (!Settings.Groups.IsValidIndex(SettingsGroup)) SettingsGroup = 0;
	const FLinearColor Col = TabColour(TEXT("settings"), false), Let = TabColour(TEXT("settings"), true);
	TSharedRef<SHorizontalBox> Pills = SNew(SHorizontalBox);
	for (int32 g = 0; g < Settings.Groups.Num(); ++g)
	{
		const bool bOn = g == SettingsGroup;
		Pills->AddSlot().AutoWidth().Padding(0, 0, 8, 0)[ Button(Settings.Groups[g].Value, [this, g]() { SettingsGroup = g; DrawMenu(); }, bOn, true, Col, Let) ];
	}
	Body->AddSlot().AutoHeight().Padding(0, 0, 0, 14)[ Pills ];
	const FString G = Settings.Groups[SettingsGroup].Key;
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	bool bFirst = true;
	for (const FBrixSetting& S : Settings.Settings)
	{
		if (S.Group != G) continue;
		if (!bFirst) Rows->AddSlot().AutoHeight()[ Rule() ];
		bFirst = false;
		const FString Key = S.Key;
		auto Change = [this, G, Key](int32 Steps) { Settings.Step(G, Key, Steps); if (G == TEXT("graphics") && Key == TEXT("picture")) PictureSet(); ApplySettings(); SaveSettings(); DrawMenu(); };
		TSharedRef<SWidget> Control = SNullWidget::NullWidget;
		if (S.Type == TEXT("switch"))
		{
			const bool bOn = S.Value > .5;
			Control = SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).IsFocusable(false).ContentPadding(0).OnClicked_Lambda([Change]() { Change(1); return FReply::Handled(); })
			[
				SNew(SBox).WidthOverride(54.f).HeightOverride(28.f)
				[
					SNew(SBorder).BorderImage(Round(bOn ? Lantern() : StoneLift(), 14.f)).Padding(FMargin(3)).HAlign(bOn ? HAlign_Right : HAlign_Left)
					[ SNew(SBox).WidthOverride(22.f)[ SNew(SBorder).BorderImage(Round(bOn ? Ink() : Dim(), 11.f)) ] ]
				]
			];
		}
		else
		{
			FString Words;
			if (S.Type == TEXT("choice")) { for (const auto& O : S.Options) if (O.Key == S.Choice) Words = O.Value; }
			else Words = FMath::IsNearlyEqual(S.Step, FMath::RoundToDouble(S.Step)) ? FString::Printf(TEXT("%d"), (int32)FMath::RoundToDouble(S.Value)) : FString::Printf(TEXT("%.1f"), S.Value);
			Control = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[ Button(TEXT("‹"), [Change]() { Change(-1); }, false) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0)[ SNew(SBox).MinDesiredWidth(150.f).HAlign(HAlign_Center)[ Text(Words, 14, Cream(), true, false) ] ]
				+ SHorizontalBox::Slot().AutoWidth()[ Button(TEXT("›"), [Change]() { Change(1); }, false) ];
		}
		Rows->AddSlot().AutoHeight().Padding(0, 8)
		[ SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[ Text(S.Name, 15) ] + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Control ] ];
	}
	Body->AddSlot().AutoHeight().Padding(0, 0, 0, 14)[ Card(Rows) ];
	Body->AddSlot().AutoHeight()[ SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[ Button(TEXT("Back to the defaults"), [this]() { Settings.Reset(); ApplySettings(); SaveSettings(); DrawMenu(); }, false) ] ];
}

void ABrixLife::PictureSet()   // "Picture" sets the five below it at once (settings.json)
{
	const FBrixSetting* P = Settings.Find(TEXT("graphics"), TEXT("picture"));
	const bool bBetter = P && P->Choice == TEXT("better");
	for (const TCHAR* K : { TEXT("shadows"), TEXT("ambient_occlusion"), TEXT("bounce") }) if (FBrixSetting* S = Settings.Find(TEXT("graphics"), K)) S->Value = bBetter ? 1 : 0;
	if (FBrixSetting* R = Settings.Find(TEXT("graphics"), TEXT("resolution"))) R->Choice = bBetter ? TEXT("auto") : TEXT("soft");
}

void ABrixLife::ToggleSound()
{
	Settings.Step(TEXT("audio"), TEXT("sound"));
	ApplySettings();
	SaveSettings();
	const bool bNow = Settings.On(TEXT("audio"), TEXT("sound"));
	Toast(bNow ? TEXT("Sound on") : TEXT("Sound off"));   // life.js setting(): "Sound on" / "Sound off"
	if (bMenu) DrawMenu();
	UE_LOG(LogTemp, Display, TEXT("Brix Town: sound %s (N turns it %s)"), bNow ? TEXT("on") : TEXT("off"), bNow ? TEXT("off") : TEXT("on"));
}

void ABrixLife::SaveSettings() const
{
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(SettingsPath()), true);
	FFileHelper::SaveStringToFile(Settings.Save(), *SettingsPath());
}

// What each setting does here, at once (UI-009). What this build does not have yet (names over residents, the minimap,
// first person, the separate sound levels the sound session owns) is kept and waits for it.
void ABrixLife::ApplySettings()
{
	if (!Settings.Settings.Num()) return;
	auto Cmd = [this](const FString& C) { if (GEngine) GEngine->Exec(GetWorld(), *C); };
	Cmd(FString::Printf(TEXT("r.ShadowQuality %d"), Settings.On(TEXT("graphics"), TEXT("shadows")) ? 5 : 0));
	Cmd(FString::Printf(TEXT("r.AmbientOcclusionLevels %d"), Settings.On(TEXT("graphics"), TEXT("ambient_occlusion")) ? -1 : 0));
	Cmd(FString::Printf(TEXT("r.DynamicGlobalIlluminationMethod %d"), Settings.On(TEXT("graphics"), TEXT("bounce")) ? 1 : 0));
	const FBrixSetting* Res = Settings.Find(TEXT("graphics"), TEXT("resolution"));
	Cmd(FString::Printf(TEXT("r.ScreenPercentage %d"), Res && Res->Choice == TEXT("soft") ? 50 : 100));
	if (Settings.On(TEXT("graphics"), TEXT("frame_counter")) != bFpsShown) { Cmd(TEXT("stat fps")); bFpsShown = !bFpsShown; }
	if (GEngine) if (FAudioDeviceHandle Audio = GEngine->GetMainAudioDevice())
		Audio->SetTransientPrimaryVolume(Settings.On(TEXT("audio"), TEXT("sound")) ? (float)Settings.Number(TEXT("audio"), TEXT("master")) / 100.f : 0.f);
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetApplicationScale((float)Settings.Number(TEXT("display"), TEXT("text_size")) / 100.f);
	if (ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
		Me->SetView((float)Settings.Number(TEXT("controls"), TEXT("camera_distance")), (float)Settings.Number(TEXT("graphics"), TEXT("field_of_view")),
			(float)Settings.Number(TEXT("controls"), TEXT("sensitivity")) / 100.f, Settings.On(TEXT("controls"), TEXT("invert")));
}

// UI-007, UI-013: the slots; saving a game (QST-008) is not in this build yet, so they say so
void ABrixLife::SavePage(TSharedRef<SVerticalBox> Body)   // QST-008: the slots, as the character designer keeps them
{
	if (ABrixDesigner* Dz = Designer()) Dz->SavePage(Body, [this]() { DrawMenu(); }, [this]() { CloseMenu(); });
}

// UI-008: the controls of the platform played on, by where they work, from settings.json
void ABrixLife::HelpPage(TSharedRef<SVerticalBox> Body)
{
	const TMap<FString, TArray<FString>>* K = Settings.Platforms.Find(TEXT("keyboard"));
	if (!K) return;
	TSharedRef<SWrapBox> W = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(14, 14));
	for (const auto& C : Settings.Contexts)
	{
		if (C.Key == TEXT("test")) continue;   // the slice's own test keys
		TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
		Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[ Caps(C.Value) ];
		for (const auto& A : Settings.Actions)
		{
			if (Settings.ActionContext.FindRef(A.Key) != C.Key) continue;
			const TArray<FString>* Keys = K->Find(A.Key);
			TSharedRef<SHorizontalBox> Ks = SNew(SHorizontalBox);
			if (Keys) for (const FString& Key : *Keys) Ks->AddSlot().AutoWidth().Padding(0, 0, 4, 0)[ Tag(Key, false, 11) ];
			Rows->AddSlot().AutoHeight().Padding(0, 3)[ SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[ Text(A.Value, 13) ] + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0, 0, 0)[ Ks ] ];
		}
		W->AddSlot()[ SNew(SBox).WidthOverride(400.f)[ Card(Rows) ] ];
	}
	Body->AddSlot().AutoHeight()[ W ];
}
