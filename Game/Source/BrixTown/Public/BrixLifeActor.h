#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixBuyMenu.h"
#include "BrixEconomy.h"
#include "BrixEstate.h"
#include "BrixBuilder.h"
#include "BrixRoomPlan.h"
#include "BrixFurnishPlan.h"
#include "BrixDesignPick.h"
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
	TSharedPtr<const struct FBrixGoodsModels> GoodsModels;   // the mall's goods as made in Blender, for the buy menus (set before Start; FURN-065)
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
	bool IsShop() const { return Panel == TEXT("shop") || Panel == TEXT("outlet"); }
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
	// the building designer (F-BLD-DESIGN; BUILD-004 to BUILD-007, BUILD-016 to BUILD-020): remodel a building or unit you own
	bool OpenRemodel(int32 Lot);
	bool IsDesigningInside() const { return CutLot >= 0; }   // the designer is looking into a storey cut open (the haze is the room's)
	const TMap<FString, TSharedPtr<FJsonObject>>& Remodels() const { return RemodelsDone; }   // lot id -> its design as paid for
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
	void RemodelPanel(TSharedRef<SVerticalBox> Body);
	void RemodelSet(const FBrixDesignChanges& Changes);
	void EndRemodel();                    // the building as it was (or as just paid for), the player's own view again
	void DesignView();                    // BUILD-020: the camera over the building, three-quarters from its front
	bool Holding(int32 Lot, bool& bWhole, TArray<int32>& Floors) const;   // life.js holding()
	TSharedPtr<FJsonObject> DesignOf(int32 Lot) const;   // as remodelled and paid for, else inside.json's
	bool RemodelPay(FString& Said);
	void ApplyRemodel(int32 Lot, const TSharedPtr<FJsonObject>& Design);
	// the room plan (BUILD-021 to BUILD-023)
	const struct FBrixLot* CityLot(int32 Lot) const;
	FBrixRoomPlanRules PlanRules() const;
	FBrixPlan PlanNow(int32 Lot, int32 Floor) const;
	bool RoomFor(int32 Lot, int32 Floor, FBrixRoomSpec& Out) const;
	void ShowPlan(int32 Lot, int32 Floor, const FBrixPlan& Plan);
	void PushUndo();
	void EditPlan(TFunction<void(FBrixPlan&)> Fn);
	void AddWall(const FString& Ax);
	void RoomsSection(TSharedRef<SVerticalBox> Body);
	int64 WorkOf(TArray<FBrixCostLine>& Lines, TArray<FString>& Probs, int64& DesignTotal) const;
	// furnishing a storey (FURN-051, FURN-052, FURN-055)
	FBrixFurnishRulesD FurnRules() const;
	bool CanFurnish(int32 Lot) const;
	FBrixLayout LayNow(int32 Lot, int32 Floor) const;
	void ShowLayout(int32 Lot, int32 Floor, const FBrixLayout& Lay);
	FBrixLayout HeldNow() const;
	FBrixLayout PoolOf() const;
	void EditLayout(TFunction<void(FBrixLayout&)> Fn);
	void FurnishSection(TSharedRef<SVerticalBox> Body);   // drawn and kept (inside.json's insides too)
	// the mouse over the picture while the designer is open (BUILD-020, BUILD-021, FURN-055; life.js pointer(), designMarks(),
	// designKey()): a click picks the piece or inner wall under it, a drag moves it on the grid; on open ground a drag turns
	// the view, the right or middle button or Shift slides it, the wheel comes nearer, a double click looks at that spot
	enum class EPoint : uint8 { Down, Move, Up, Wheel, Twice };
	void DesignPointer(EPoint Type, const FVector2D& Px, int32 Button, bool bShift, const FVector2D& DeltaPx, float Wheel, bool bHeld);
	bool DesignRay(const FVector2D& Px, FBrixRay& Out) const;   // the camera's ray through a point of the picture (pixels)
	bool SlatePoint(EPoint Type, const FVector2D& Px);    // for a test shot: the same through the interface, as a mouse would
	double DesignFloorM() const;          // the storey in view: its floor over the ground
	double DesignDist0() const;           // how far the view stands off before the wheel
	double DesignPitch0() const;          // and how steeply it looks down, radians
	void DesignMarks();                   // the outlines round what is picked, what is under the mouse and what cannot stand
	TSet<FString> StoreyBad() const;      // the pieces of the storey in view its check finds wrong (life.js storeyBad())
	void DesignKeys();                    // R turns the piece picked (Shift+R the other way), Delete takes it away
	void SyncCut();                       // the building cut open at the storey looked into, whole from outside (BUILD-020)
	float CutNow(int32 Lot) const;        // the height a lot is cut open at now, or below 0
	struct FGrab { int32 Kind = 0; FString Id; FVector2D Off = FVector2D::ZeroVector; double OffA = 0; bool bMoved = false; };   // 1 turns the view, 2 slides it, 3 a piece, 4 a wall
	FGrab Grab;
	FString PieceHover, WallHover;
	bool bPieceList = false;              // the storey's pieces listed by name as well
	double ViewYawAdd = 0, ViewPitchAdd = 0, ViewZoom = 1;   // the view as dragged: turned and tipped (radians), nearer or further
	FVector2D ViewPan = FVector2D::ZeroVector;             // and slid across the ground from the building's middle (metres)
	int32 CutLot = -1, CutFloor = -1;     // the building drawn cut open, and at which storey
	TSharedPtr<SWidget> DesignPad;
	float CardWide() const { return RemodelPanelPx - 44.f - 12.f; }   // the designer's card inside its edges, less the room kept for its scroll bar
	UPROPERTY(Config) float DesignCutM = 1.25f;      // the outer walls are cut this far above the storey's floor (play.js: F + 1.25)
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
	bool BoardBuy(int32 N, const FString& U, const FString& Kind, FString& Said);
public:
	FString BoardsSoldWhere() const;                   // where boards are sold: the mall's sports and toy outlets (ADR-0054)
private:
	void StallPanel(TSharedRef<SVerticalBox> Body);
	// one thing of a buy menu (life.js ware()): whether it can be had now, sold out, or dearer than cash and current account
	// State: ok, short (dearer than cash and the current account), out (sold out) or have (a board already owned: CITY-103);
	// More: rows the showcase adds under its name (a board's top speed)
	// Line: a piece of furniture's line (its own mesh in that line is its picture); Size: its width, depth and height, metres
	struct FWare { FString Id, Name, Goes, Band, Line; int64 Price = 0; int32 Left = -1; FString State = TEXT("ok"); int64 Short = 0; TArray<TPair<FString, FString>> More; TOptional<FVector3f> Size; };
	TArray<FWare> BoardWares(const FString& Band) const;   // a sports or toy outlet's three boards, in its grid (CITY-102, CITY-103)
	// bNarrow: layout D, small cells and a strip under them, Width pixels wide (a panel narrower than the rules' narrow_px)
	struct FGridOpts { FString Id, Verb = TEXT("Buy"), StockLabel, PriceLabel = TEXT("Price"), Empty = TEXT("Nothing here."); bool bMine = false, bNarrow = false; float Width = 0.f; TFunction<bool(const FWare&, FString&)> Buy; };
	TSharedRef<SWidget> Grid(const TArray<FWare>& Things, const FGridOpts& O);   // layout B, or D when narrow (buymenu.js grid())
	FWare Ware(const FString& Id, const FString& Name, int64 Price, const FString& Goes, int32 Left, const FString& Band) const;
	FString ShopGoes(const FString& Item) const;
	TSharedPtr<double> YawFor(const FString& Key);
	TSharedPtr<const FBrixThingMesh> MeshFor(const FString& Thing, const FString& Band, const FString& Line = FString());
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
	TArray<FBrixDesignLot> DesignLots;    // the town's lots as the designer sees them (BrixBuilder::LotOf)
	TMap<FString, TSharedPtr<FJsonObject>> RemodelsDone;
	TSharedPtr<FJsonObject> Draft, DraftBase;   // the remodel being tried, and what it started from
	TArray<TSharedPtr<FJsonObject>> DraftUndo;
	TMap<int32, FBrixPlan> PlanDraft, PlanBase;   // the storeys' room plans being tried, and as they were
	TArray<TMap<int32, FBrixPlan>> PlanUndo;
	FString WallSel;                      // the inner wall picked
	TMap<int32, FBrixLayout> LayDraft, LayBase;   // the storeys' furniture being tried, and as it was
	TArray<TMap<int32, FBrixLayout>> LayUndo;
	FString PieceSel, AddRoom, AddLine;   // the piece picked; the kind of room and the line new pieces come from
	FBrixLayout FurnHeld;                 // the furniture among the belongings (INV-001; life.js belongings)
	TMap<FString, TMap<int32, FBrixLayout>> LaysDone;   // lot id -> storey -> its furniture as paid for (life.js furnishings)
	TMap<FString, TMap<int32, FBrixPlan>> PlansDone;   // lot id -> storey -> its plan as paid for (life.js plans)
	FString DesignTab = TEXT("outside"), DesignFacade = TEXT("*");
	int32 DesignStorey = 0, ViewTurn = 0;
	UPROPERTY() TObjectPtr<class ACameraActor> ViewCam;
	UPROPERTY(Config) float DesignPitchDeg = 35.f;   // BUILD-020: the designer's view at the angle of an isometric drawing
	UPROPERTY(Config) float RemodelPanelPx = 470.f;  // the designer's card, at the left of the screen
	UPROPERTY(Config) float DesignTurnDeg = 36.f;    // and turned that far from the front (builder: designCam's .62 rad)
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
	FString PanelUnit;                    // the outlet of a mall whose counter is open (ECO-013)
	bool OutletHere(int32& OutLot, FString& OutUnit) const;   // the outlet the player stands in, on whichever floor (life.js outletHere())
	void OutletPanel(TSharedRef<SVerticalBox> Body);
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
	TSharedPtr<SBox> PanelBody, PanelBox;   // the card's body; the box that places the card (PanelRoot)
};
