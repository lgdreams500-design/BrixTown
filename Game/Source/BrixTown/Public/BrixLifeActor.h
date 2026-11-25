#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixBuyMenu.h"
#include "BrixEconomy.h"
#include "BrixEstate.h"
#include "BrixQuests.h"
#include "BrixRewards.h"
#include "BrixRewardsWidget.h"
#include "BrixSettings.h"
#include "BrixTalk.h"
#include "BrixLifeActor.generated.h"

class SBox;
class SVerticalBox;
class SWidget;
struct FBrixCity;
struct FBrixDay;

// The player's life in the town, as the slice's life.js does it (ADR-0012): the money in their purse on screen, the bank's
// counter, the money page (I). The rules are BrixWorld's FBrixEconomy (a port of economy.js); this actor is the screens
// and the hooks. A new game starts when play starts (the character designer is not brought over yet): the starting cash
// is paid in through the ledger, and the economy's days are counted from then. Each frame the economy is brought up to
// the clock, so the bank settles every midnight that has passed (BANK-008).
// B at a counter opens it: the bank's, a shop's or café's (to buy and to sell, the goods as objects in a grid with the
// chosen one large beside it: UI-020, ADR-0044), the market stall's; I the money page, Esc closes. At a shop's counter the
// arrow keys choose a thing and Enter buys it; a click chooses, a second click soon after buys.
// For test shots: -BrixCounter=bank (or market, or a shop's kind or lot id) puts the player at its door a third into the wait and opens its counter, after
// doing -BrixLife=<op>;<op>... (ops as Tools/citysim/economy.py check_script() writes them, arguments by commas, such as
// open,current;move,p:current,p:savings,1000;loan,5000,3).
// A reward's or interest's sound, as sound.js Sound.reward(what, i, n) names them: quest_start, coin_out, coin_land,
// coin_take, quest_end, interest_start, interest_coin, interest_end, achievement (SND-019 to SND-021). Bound by the sound.
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnBrixRewardSound, const FString& /*What*/, int32 /*I*/, int32 /*N*/);

UCLASS(Config = Game)
class BRIXTOWN_API ABrixLife : public AActor
{
	GENERATED_BODY()

public:
	ABrixLife();
	void Start(const FBrixCity& City, TSharedPtr<FBrixDay> InDay);
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	bool IsOpen() const { return !Panel.IsEmpty() || bMenu; }
	bool IsMenu() const { return bMenu; }
	// the game menu (BrixMenu.cpp; UI-005 to UI-019): Esc opens it on its home, a page's key on that page
	void Menu(const FString& Tab);
	void MenuKey(const FString& Action);  // settings.json's action: me, money, belongings, property, journal, wardrobe, map, help
	void CloseMenu();
	void TestMenu(const FString& Page);
	void TickMenuShot();
	FBrixSettings Settings;               // World/rules/settings.json with the device's own values
	void CounterKey();                    // B: the counter of the lot the player is at, if it has one
	void WalletKey();                     // I: the money page
	void QuestsKey();                     // J: the quest log (QST-005)
	// conversations (life.js conversation(), allowed(), chose(), talkEnded(), vars(); ABrixTalk calls these)
	FBrixDialogue::FConversation Conversation(int32 Resident, const FBrixDialogue::FConversation& Base);
	bool Allowed(const FBrixDialogue::FOption& O) const;
	void Chose(int32 Resident, const FBrixDialogue::FOption& O);
	void TalkEnded();
	TMap<FString, FString> TalkVars() const;
	FBrixQuests* Quests() const { return Q.Get(); }
	const FBrixQuestRules& QuestLines() const { return QuestRules; }   // the maps' markers (life.js targets())
	FOnBrixRewardSound OnRewardSound;
	const TMap<FString, int32>& Achievements() const { return Feats; }   // earned, and on which day (QST-009)
	bool ArrowKey(int32 Dx, int32 Dy);    // at a shop's counter: choose the next thing that way
	bool EnterKey();                      // and buy (or sell) it
	bool IsShop() const { return Panel == TEXT("shop"); }
	void Close();
	void Toast(const FString& Text);
	void ToggleSound();                   // N (SND-013): the Sound switch of the settings flipped, applied, kept, and said
	void SetPlayerName(const FString& Name) { PlayerName = Name; bNamed = !Name.IsEmpty(); }   // the designed character's (CHR-001)
	const FString& Name() const { return PlayerName; }
	void ShowHud(bool bShow);
	// QST-008: the money, quests and achievements into a save, and back (the coins of what was before are not shown)
	void SaveTo(struct FBrixSaveGame& S) const;
	bool LoadFrom(const struct FBrixSaveGame& S, FString& OutWhy);             // the purse, toasts and quests on screen; hidden while the character designer is open
	bool RunOps(const FString& Ops);      // the -BrixLife= ops, as economy.run_script() runs them; false if any was refused
	bool OpenCounterAt(const FString& What);   // -BrixCounter=: stand at that counter's door and open it
	FBrixEconomy* Economy() const { return E.Get(); }
	// the real estate market (F-PROP-ESTATE 2; PROP-009 to PROP-023): P opens the menu's Properties page, B at an estate
	// agent's office its counter
	FBrixEstate* Estate() const { return Est.Get(); }
	TArray<int32> MyLots() const { return Est ? Est->MyLots() : TArray<int32>(); }   // owned or rented (the maps' map_mine)
	FString BoardText(int32 Lot) const;   // PROP-023: what the board over a building's door says, or empty
	void ShowOnMap(int32 Lot);            // UI-006: the mark at a building, and the big map
	int32 LotHereForEstate() const { return LotHere(); }
	int32 CounterHere() const;            // the lot whose counter the player is at, or -1

	UPROPERTY(Config) FString LifeFile = TEXT("Data/life.json");
	UPROPERTY(Config) float CounterReach = 3.f;     // m from a lot's door at which its counter can be used from the street
	UPROPERTY(Config) float CoinArcDeg = 220.f;     // SND-019: the coins spring out over this arc on the far side of the player from the camera
	UPROPERTY(Config) float CoinGlow = 1.6f;        // SND-019: the coins' lantern light, times the slice's
	UPROPERTY(Config) float ToastSeconds = 4.5f;    // life.js toast()
	UPROPERTY(Config) float PlusSeconds = 2.2f;     // the "+Bx" beside the purse
	UPROPERTY(Config) int32 BankStatementLines = 12;
	UPROPERTY(Config) int32 WalletStatementLines = 15;
	UPROPERTY(Config) float CheckSeconds = 0.25f;   // how often goals are checked (life.js: four times a second)
	UPROPERTY(Config) int32 TrackedQuests = 3;      // quest lines shown at the right of the screen

private:
	void BuildUI();
	void StartMenu();
	void DrawMenu();
	TSharedRef<SWidget> HomePage();
	TSharedRef<SWidget> MenuPage(const FString& Id);
	void SettingsPage(TSharedRef<SVerticalBox> Body);
	void SavePage(TSharedRef<SVerticalBox> Body);
	void HelpPage(TSharedRef<SVerticalBox> Body);
	FString Headline(const FString& Id) const;
	const FBrixMenuTab* Tab(const FString& Id) const;
	FLinearColor TabColour(const FString& Id, bool bLetters) const;
	FString TabKey(const FString& Id) const;
	FString SettingsPath() const;
	class ABrixDesigner* Designer() const;   // the character designer, whose wardrobe the Wardrobe page is
	void PictureSet();
	void ApplySettings();
	void SaveSettings() const;
	FString CounterWords(int32 Lot) const;
	void Draw();                          // the open panel, drawn again from the state of the game (life.js draw())
	void Act(TFunction<bool(FString&)> Fn);
	void Redraw();   // a button: do it, say what happened or why it was refused, draw again
	void Open(const FString& Kind, int32 Lot);
	void BankPanel(TSharedRef<SVerticalBox> Body);
	void WalletPanel(TSharedRef<SVerticalBox> Body);
	void EstatePanel(TSharedRef<SVerticalBox> Body, const FString& Agent, int32 Here);   // life.js estatePanel(): at an agent's, or Properties
	TSharedRef<SWidget> UnitRow(int32 i, const FString& Agent);
	FString BuyUnits(const TArray<int32>& Units, const FString& Agent, bool& bOk);
	void QuestsPanel(TSharedRef<SVerticalBox> Body);
	TMap<FString, FString> JobVars(const FBrixJob& O) const;
	FString FillJob(const FString& Text, const FBrixJob& O) const;
	FString WhereWords(const FBrixGoal& G) const;   // QST-005: where the person to see is right now
	FBrixQuests::FCtx Ctx() const;
	int32 LotHere() const;                // the lot the player is in, or at the door of
	double Minute() const;
	TArray<TPair<FString, FString>> Tracked() const;   // what the right of the screen shows: quest, then what to do
	TSharedRef<SWidget> TrackCard(int32 K);
	// the showing of money coming in (rewards.js)
	struct FShowing { FString Kind; int32 N = 1, Landed = 0, Taken = 0; };
	struct FCoin { FVector P = FVector::ZeroVector, V = FVector::ZeroVector, From = FVector::ZeroVector; double Floor = 0, Spin = 0, Vs = 0, Ph = 0, Age = 0, T = 0, Side = 1, PullAt = 0, Small = 1; int32 State = 0, Bounces = 0, I = 0; bool bHit = false, bDone = false; FString Purse; int64 Part = 0; TSharedPtr<FShowing> Show; TArray<FVector, TInlineAllocator<6>> Trail; };
	struct FFly { FVector2D A = FVector2D::ZeroVector; FString To; double T = 0, Len = .4, Bend = 0; float R = 6.f; bool bFromPlayer = false, bStarted = false, bDone = false; TFunction<void()> Land; };
	struct FSpark { FVector2D P, V; double T = 0, Life = .6; };
	void Later(double Dt, TFunction<void()> Fn);
	void Sound(const FString& What, int32 I = 0, int32 N = 0);
	void Land(const FString& Purse, int64 Chips, TSharedPtr<FShowing> S);
	void Sparks(FVector2D At, int32 N);
	void ShowReward(const FBrixRewardEvent& Ev);
	void ShowInterest(const FBrixRewardEvent& Ev);
	void TickRewards(float Dt);
	void CheckAchievements();
	FVector2D PurseCashPx() const;
	FVector2D PurseBankPx() const;
	FVector2D SlipPx() const;
	TSharedRef<SWidget> PurseLine() const;
	TSharedRef<SWidget> StatementTable(int32 N) const;
	void ShopPanel(TSharedRef<SVerticalBox> Body);
	void StallPanel(TSharedRef<SVerticalBox> Body);
	// one thing of a buy menu (life.js ware()): whether it can be had now, sold out, or dearer than cash and current account
	struct FWare { FString Id, Name, Goes, Band; int64 Price = 0; int32 Left = -1; FString State = TEXT("ok"); int64 Short = 0; };
	struct FGridOpts { FString Id, Verb = TEXT("Buy"), StockLabel, PriceLabel = TEXT("Price"), Empty = TEXT("Nothing here."); bool bMine = false; TFunction<bool(const FWare&, FString&)> Buy; };
	TSharedRef<SWidget> Grid(const TArray<FWare>& Things, const FGridOpts& O);   // layout B (buymenu.js grid())
	FWare Ware(const FString& Id, const FString& Name, int64 Price, const FString& Goes, int32 Left, const FString& Band) const;
	FString ShopGoes(const FString& Item) const;
	TSharedPtr<double> YawFor(const FString& Key);
	TSharedPtr<const FBrixThingMesh> MeshFor(const FString& Thing, const FString& Band);
	void Frame(double& OutDay, double& OutMinute) const;
	FVector2D PlayerAt() const;

	FBrixLifeData Data;
	FBrixBuyMenuRules BuyMenu;
	TUniquePtr<FBrixEconomy> E;
	FBrixQuestRules QuestRules;
	TArray<FBrixQuestResident> QuestResidents;
	TUniquePtr<FBrixQuests> Q;
	FBrixEstateRules EstateRules;
	TArray<FBrixUnit> EstateUnits;
	TMap<FString, FString> EstateRoles;
	TUniquePtr<FBrixEstate> Est;            // holds the economy: goes before it
	int32 PayTerm = 0;                    // how to pay: 0 outright, else the mortgage term's place + 1 (life.js payChoice())
	FString AgentFilter = TEXT("all");    // the kind of unit an agent shows
	TMap<int32, FString> RentEdit;        // the rent typed for a unit of the player's, in brix
	TSharedPtr<FBrixDay> Day;
	TOptional<FBrixJob> Offered;          // the job offered in the talk under way (life.js _offer)
	int32 PendingCounter = -1;            // a resident whose counter opens when the talk ends (an answer's act "shop")
	float CheckLeft = 0.f;
	FBrixRewardRules RewardRules;
	FBrixAchievementRules AchievementRules;
	TMap<FString, int32> Feats;
	TArray<FString> Banners;
	FString BannerName, BannerLine, SlipWhat;
	double BannerLeft = -5, BannerAge = 0, SlipLeft = 0, Clock = 0;
	int32 SeenTx = 0;
	int64 Shown = 0, HeldCash = 0, HeldBank = 0;
	TArray<TPair<double, TFunction<void()>>> Timeline;
	TArray<FCoin> Coins;
	TArray<FFly> Flies;
	TArray<FSpark> Spark;
	TArray<FBrixCoinSprite> Sprites;
	FVector2D ViewportPx = FVector2D(1600, 900);
	float PxScale = 1.f;
	TArray<FBrixEconLot> Lots;
	TArray<float> LotAngle;
	TArray<FVector2D> LotDoor;
	FString PlayerName = TEXT("You");
	bool bNamed = false;
	int64 LastMoney = 0;
	float ToastLeft = 0.f, DeltaLeft = 0.f;
	FString ToastText, DeltaText;
	bool bDeltaUp = true;

	// the open panel (life.js panel)
	FString Panel, Msg;
	int32 PanelLot = -1;
	int32 MoveFrom = 0, MoveTo = 1, LoanSize = 0, LoanTerm = 0;
	FString Amount = TEXT("5.00");
	FString Side = TEXT("buy");           // a shop's tab: what it sells, or what it buys
	int32 StallPick = 0;
	FString StallPrice;
	TMap<FString, FString> Chosen;        // by grid: the thing chosen (buymenu.js chosen)
	TMap<FString, TSharedPtr<double>> Yaws;   // by grid and thing: how far it has turned
	TMap<FString, TSharedPtr<const FBrixThingMesh>> Meshes;
	struct FLive { FString Id; TArray<FWare> Things; FGridOpts O; int32 Columns = 4; };
	TOptional<FLive> LiveGrid;            // the grid on the panel now, for the keys
	FString ClickedId;
	double ClickedAt = 0;

	TSharedPtr<SWidget> Hud, PanelRoot, MenuRoot;
	TSharedPtr<SBox> MenuBody;
	bool bMenu = false, bFpsShown = false;
	FString MenuTab, CityName;
	int32 SettingsGroup = 0;
	double MenuShotAt = -1;               // -BrixMenu: the real time to take the test shot at (the menu has stopped the game's own timers)
	TSharedPtr<SBox> PanelBody;
};
