#pragma once

#include "CoreMinimal.h"

class FJsonObject;
struct FBrixCity;

// Money, the bank, shops, a mall's outlets and the market stall (ADR-0012): a port of the slice's economy.js, itself a port
// of Tools/citysim/economy.py, rule for rule and in the same order, so the same data gives the same ledger. The data is
// Game/Data/life.json, written by Tools/worldgen/build_play.py with fixed runs and the results Python gives for them;
// ECO002_the_ledger_matches_the_python and BANK008_the_bank_settles_as_the_python_does run them here.
// Money is whole chips, int64 (ECO-001), and moves only as balanced ledger transactions (ECO-002). Nothing is ticked:
// stock and stall sales are worked out from the residents' routine, and the bank settles each day that has ended (BANK-008).
// A rule that refuses (not enough cash, sold out...) changes nothing and returns false, with the reason in Why (the
// slice's Refused).

namespace BrixMoney
{
	BRIXWORLD_API FString Format(int64 Chips);            // "Bx 12.50", "-Bx 0.05" (economy.js fmt)
	BRIXWORLD_API bool IsSystem(const FString& Account);  // "sys:..." accounts make and take money (ECO-003)
	BRIXWORLD_API const TArray<FString>& Player();        // p:cash, p:current, p:savings
	BRIXWORLD_API int64 FloorDiv(int64 A, int64 B);       // Math.floor(a / b), for negative a too
}

using FBrixLines = TArray<TPair<FString, int64>>;

struct BRIXWORLD_API FBrixTx
{
	int32 No = 0, Day = 0;
	double Minute = 0;
	FString Memo;
	FBrixLines Lines;                                     // account, chips; they sum to zero
};

struct BRIXWORLD_API FBrixLedger
{
	TArray<FBrixTx> Tx;                                   // append-only (ECO-002)
	TMap<FString, int64> Bal;

	int64 Balance(const FString& Account) const { const int64* B = Bal.Find(Account); return B ? *B : 0; }
	// A transaction, or a refusal when the lines do not balance or an account that is not a system account would go below
	// zero; refused, nothing is written.
	bool Post(int32 Day, double Minute, const FString& Memo, const FBrixLines& Lines, FString& Why);
	int64 Supply() const;                                 // every account but the system ones (ECO-003)
	int64 SystemTotal() const;                            // the system accounts: always -Supply()
	struct FEntry { int32 No = 0, Day = 0; double Minute = 0; FString Memo; int64 Change = 0; };
	// The transactions that touched these accounts, newest first, with what they changed in them all (ECO-009).
	TArray<FEntry> Statement(const TArray<FString>& Accounts, int32 N = 0) const;
	int64 MovedBetween(int32 No, const TArray<FString>& Accounts) const;   // what went into these accounts from each other
};

// World/rules/economy.json and World/rules/mall_goods.json, as Game/Data/life.json carries them (ECO-012).
struct BRIXWORLD_API FBrixEconomyRules
{
	struct FItem { FString Name; int64 Price = 0; bool bSell = false, bQuest = false; };
	struct FShop { FString Kind, Name, MatchKind, NameHas; TArray<FString> Sells, Buys; int64 Till = 0; };
	struct FAccount { FString Kind, Name; int64 RateBp = 0; };
	struct FGood { FString Name, Goes, Piece; int64 Price = 0; };
	FString Symbol = TEXT("Bx");
	int64 StartCash = 0, WelcomeDeposit = 0;
	FString BankLot;
	int64 Vault = 0;
	TArray<FAccount> Accounts;                            // current, savings, in the file's order
	int64 LoanMin = 0, LoanMax = 0, LoanStep = 1, LoanRateBp = 0, LateFee = 0;
	TArray<int32> LoanTerms;
	int32 MaxLoans = 1, DefaultAfterMissed = 3;
	TMap<FString, FItem> Items;
	TArray<FShop> Shops;                                  // in the file's order: the first that matches a lot is its kind
	int32 StockTarget = 10, RestockMinute = 360;
	int64 SlopeBp = 0, MinBp = 0, MaxBp = 0, SellShareBp = 0;
	int32 RepMin = -100, RepMax = 100;
	int64 DiscountBpAtMax = 0, MarkupBpAtMin = 0;
	FString StallLot;
	int64 StallRent = 0, StallBuyBp = 0, StallMaxPriceFactorBp = 0;
	int32 StallSlots = 6, StallOpen = 480, StallClose = 1080;
	double StallElasticity = 3;
	// a mall's outlets (ECO-013, ECO-014)
	bool bGoods = false;
	FString GoodsDetail = TEXT("near");                   // mall_goods.json display.detail: which make of each good stands in an outlet (FURN-065)
	int32 PerOutlet = 8, OutletStockTarget = 6;
	TMap<FString, TArray<FString>> OutletSells;           // outlet kind -> the goods that kind deals in
	TMap<FString, FGood> Goods;

	const FAccount* Account(const FString& Kind) const { return Accounts.FindByPredicate([&](const FAccount& A) { return A.Kind == Kind; }); }
	const FShop* Shop(const FString& Kind) const { return Shops.FindByPredicate([&](const FShop& S) { return S.Kind == Kind; }); }
};

// One of the town's lots as the economy sees it (the city's lots in its blocks' order, the slice's LOTS).
struct BRIXWORLD_API FBrixEconLot
{
	FString Id, Kind, Name;
	FVector2D Min = FVector2D::ZeroVector, Max = FVector2D::ZeroVector;
	FVector2D Centre() const { return (Min + Max) * 0.5; }
};

struct BRIXWORLD_API FBrixOutlet { FString Id, Name, Kind; int32 Level = 0; };

// Everything Game/Data/life.json holds for money, shops and quests.
struct BRIXWORLD_API FBrixLifeData
{
	FBrixEconomyRules Rules;
	TArray<FString> LotIds;                               // the lots' ids in the order the visits and outlets are numbered by
	TMap<int32, TArray<TPair<double, int32>>> Visits;     // lot -> (minute, resident index): the day's shop visits, in time order
	TMap<int32, TArray<FString>> Staff;                   // lot -> the ids of the residents who work there
	TMap<int32, TArray<FBrixOutlet>> Outlets;             // a mall's lot -> its outlets, in its plan's order
	TSharedPtr<FJsonObject> Json;                         // the whole file: quests, the buy menu, the fixed runs and what Python gives
};

BRIXWORLD_API bool LoadBrixLife(const FString& Path, FBrixLifeData& Out, FString& OutError);
// The economy's lots from the city, checked to be in the order life.json was written in.
BRIXWORLD_API bool BrixEconLots(const FBrixCity& City, const FBrixLifeData& Life, TArray<FBrixEconLot>& Out, FString& OutError);

struct BRIXWORLD_API FBrixLoan
{
	int32 Id = 0, Day = 0;
	int64 Principal = 0, Total = 0, Paid = 0, Fees = 0, FeesPaid = 0, Taken = 0;
	TArray<int64> Plan;                                   // each day's instalment
	int32 Missed = 0;
	FString Status = TEXT("active");                     // active, repaid, defaulted
	int64 Owed() const { return Total + Fees - Paid - FeesPaid - Taken; }
};

struct BRIXWORLD_API FBrixStall
{
	struct FSlot { FString Item; int64 Price = 0; bool bSold = false; };
	int32 Day = 0;
	double Done = 0;
	TArray<FSlot> Slots;
};

class BRIXWORLD_API FBrixEconomy
{
public:
	FBrixEconomy(const FBrixLifeData& Life, const TArray<FBrixEconLot>& Lots);

	const FBrixEconomyRules& R;
	TArray<FBrixEconLot> Lots;
	TMap<FString, int32> LotIndex;
	FBrixLedger L;
	int32 Day = 0;
	double Minute = 0;
	int32 Settled = 0;
	TMap<FString, int32> Inv;                             // what the player carries
	TArray<FString> Accounts;                             // the kinds opened, in the order opened
	bool bWelcomed = false, bBarred = false;
	TArray<FBrixLoan> Loans;
	TMap<FString, TMap<FString, TPair<int32, int32>>> Trades;   // "lot|restock day(|outlet)" -> item -> (bought, sold) by the player
	TMap<FString, int32> Rep;                             // resident id -> reputation (QST-007)
	TOptional<FBrixStall> Stall;
	int32 StallSoldTotal = 0;
	TMap<FString, int32> Clothes, Homeware;               // bought at a mall's outlets (ECO-014)
	TMap<int32, TArray<FBrixOutlet>> Outlets;
	TArray<FString> Kinds;                                // each lot's shop kind, or empty
	int32 BankLot = -1, MarketLot = -1;
	FString Why;                                          // why the last rule refused
	// more rules settled at each day's end, after the bank (economy.js hooks: the estate market), by id
	TMap<int32, TFunction<void(int32)>> Hooks;
	int32 NextHook = 0;

	// the player
	void NewCharacter();
	int64 TotalMoney() const;
	int64 Funds() const;                                  // cash and current account: what a purchase can be paid from
	bool GiveItem(const FString& Item, int32 N = 1);
	bool TakeItem(const FString& Item, int32 N = 1);
	bool Spend(int64 Amount, const FString& To, const FString& Memo);
	bool HasAccount(const FString& Kind) const { return Accounts.Contains(Kind); }
	const FBrixEconomyRules::FItem* Item(const FString& Id) const { return Items.Find(Id); }
	// the clock and the bank (BANK-001, BANK-002, BANK-007, BANK-008)
	void Advance(int32 ToDay, double ToMinute);
	bool OpenAccount(const FString& Kind);
	bool Move(const FString& Src, const FString& Dst, int64 Amount);
	TPair<int64, TArray<int64>> LoanTerms(int64 Principal, int32 Term) const;
	bool TakeLoan(int64 Principal, int32 Term);
	bool Repay(int32 Id);
	FBrixLoan* ActiveLoan();
	// shops (ECO-005, ECO-010, ECO-011)
	FString ShopKind(int32 N) const { return Kinds.IsValidIndex(N) ? Kinds[N] : FString(); }
	int32 Restock() const { return Restock(Day, Minute); }
	int32 Restock(int32 D, double M) const { return M >= R.RestockMinute ? D : D - 1; }
	int32 ResidentBuys(int32 N, const FString& It) const;
	int32 Stock(int32 N, const FString& It) const;
	int32 RepWithLot(int32 N) const;
	int64 Factor(int32 N, const FString& It) const;
	int64 Price(int32 N, const FString& It) const;
	int64 Offer(int32 N, const FString& It) const;
	bool Buy(int32 N, const FString& It, int64* OutPaid = nullptr);
	bool Sell(int32 N, const FString& It, int64* OutPaid = nullptr);
	// a mall's outlets (ECO-013, ECO-014)
	const FBrixOutlet* Outlet(int32 N, const FString& U) const;
	FBrixEconomyRules::FGood Good(const FString& G) const;
	TArray<FString> OutletSells(int32 N, const FString& U) const;
	TMap<FString, int32> OutletResidentBuys(int32 N, const FString& U) const;
	int32 OutletStock(int32 N, const FString& U, const FString& G) const;
	int64 OutletPrice(int32 N, const FString& U, const FString& G) const;
	bool BuyAt(int32 N, const FString& U, const FString& G, int64* OutPaid = nullptr);
	// the market stall (PROP-008)
	bool RentStall();
	bool SetOut(const FString& Item, int64 Price);
	bool TakeBack(int32 K);
	int64 StallChance(const FString& Item, int64 Price) const;

	static TArray<FString> OutletRange(const FBrixEconomyRules& R, const FString& U, const FString& Kind);

private:
	void Settle(int32 D);
	void LoanDay(FBrixLoan& Loan, int32 D);
	void PayLoan(FBrixLoan& Loan, int64 Amount, int32 D, double M, const FString& Memo);
	void DefaultLoan(FBrixLoan& Loan, int32 D);
	void RunStall(int32 D, double M);
	void Trade(int32 N, const FString& It, int32 K);
	FString KindOf(int32 N) const;
	bool No(const FString& Msg) { Why = Msg; return false; }

	TMap<FString, FBrixEconomyRules::FItem> Items;        // the town's items and the outlets' goods that are carried or eaten
	const TMap<int32, TArray<TPair<double, int32>>>& Visits;
	const TMap<int32, TArray<FString>>& Staff;
	mutable TMap<FString, TArray<FString>> SellsCache;    // "lot|outlet" -> its goods
};
