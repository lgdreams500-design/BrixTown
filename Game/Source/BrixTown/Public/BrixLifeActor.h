#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixEconomy.h"
#include "BrixLifeActor.generated.h"

class SBox;
class SVerticalBox;
class SWidget;
struct FBrixCity;

// The player's life in the town, as the slice's life.js does it (ADR-0012): the money in their purse on screen, the bank's
// counter, the money page (I). The rules are BrixWorld's FBrixEconomy (a port of economy.js); this actor is the screens
// and the hooks. A new game starts when play starts (the character designer is not brought over yet): the starting cash
// is paid in through the ledger, and the economy's days are counted from then. Each frame the economy is brought up to
// the clock, so the bank settles every midnight that has passed (BANK-008).
// B at a counter opens it (the bank's now; shops' and the market stall's to come), I the money page, Esc closes.
// For test shots: -BrixCounter=bank puts the player at the bank's door a third into the wait and opens its counter, after
// doing -BrixLife=<op>;<op>... (ops as Tools/citysim/economy.py check_script() writes them, arguments by commas, such as
// open,current;move,p:current,p:savings,1000;loan,5000,3).
UCLASS(Config = Game)
class BRIXTOWN_API ABrixLife : public AActor
{
	GENERATED_BODY()

public:
	ABrixLife();
	void Start(const FBrixCity& City);
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	bool IsOpen() const { return !Panel.IsEmpty(); }
	void CounterKey();                    // B: the counter of the lot the player is at, if it has one
	void WalletKey();                     // I: the money page
	void Close();
	void Toast(const FString& Text);
	bool RunOps(const FString& Ops);      // the -BrixLife= ops, as economy.run_script() runs them; false if any was refused
	bool OpenCounterAt(const FString& What);   // -BrixCounter=: stand at that counter's door and open it
	FBrixEconomy* Economy() const { return E.Get(); }
	int32 CounterHere() const;            // the lot whose counter the player is at, or -1

	UPROPERTY(Config) FString LifeFile = TEXT("Data/life.json");
	UPROPERTY(Config) float CounterReach = 3.f;     // m from a lot's door at which its counter can be used from the street
	UPROPERTY(Config) float ToastSeconds = 4.5f;    // life.js toast()
	UPROPERTY(Config) float PlusSeconds = 2.2f;     // the "+Bx" beside the purse
	UPROPERTY(Config) int32 BankStatementLines = 12;
	UPROPERTY(Config) int32 WalletStatementLines = 15;

private:
	void BuildUI();
	void Draw();                          // the open panel, drawn again from the state of the game (life.js draw())
	void Act(TFunction<bool(FString&)> Fn);   // a button: do it, say what happened or why it was refused, draw again
	void Open(const FString& Kind, int32 Lot);
	void BankPanel(TSharedRef<SVerticalBox> Body);
	void WalletPanel(TSharedRef<SVerticalBox> Body);
	TSharedRef<SWidget> PurseLine() const;
	TSharedRef<SWidget> StatementTable(int32 N) const;
	void Frame(double& OutDay, double& OutMinute) const;
	FVector2D PlayerAt() const;

	FBrixLifeData Data;
	TUniquePtr<FBrixEconomy> E;
	TArray<FBrixEconLot> Lots;
	TArray<float> LotAngle;
	TArray<FVector2D> LotDoor;
	FString PlayerName = TEXT("You");
	int64 LastMoney = 0;
	float ToastLeft = 0.f, DeltaLeft = 0.f;
	FString ToastText, DeltaText;
	bool bDeltaUp = true;

	// the open panel (life.js panel)
	FString Panel, Msg;
	int32 PanelLot = -1;
	int32 MoveFrom = 0, MoveTo = 1, LoanSize = 0, LoanTerm = 0;
	FString Amount = TEXT("5.00");

	TSharedPtr<SWidget> Hud, PanelRoot;
	TSharedPtr<SBox> PanelBody;
};
