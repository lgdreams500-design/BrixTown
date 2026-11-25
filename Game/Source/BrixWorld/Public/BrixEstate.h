#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FBrixEconomy;
struct FBrixLifeData;
struct FBrixLedger;

// The real estate market (F-PROP-ESTATE level 2, ADR-0022; PROP-009 to PROP-023, BANK-002): a port of the slice's
// estate.js, itself a port of Tools/citysim/estate.py, rule for rule and in the same order, so the same data and the same
// game give the same market. Every building is carved into units (estate.carve(), written into Game/Data/life.json as
// estate_units by build_play.py), each with an owner and maybe a tenant; money moves only through the economy's ledger,
// settled one day at a time at each day's end (a hook of FBrixEconomy's day), never ticked (PROP-021).
// PROP021_the_market_matches_the_python runs estate.check_script() and compares with what Python gives.
// A rule that refuses changes nothing and returns false, with the reason in Why (the slice's Refused).

struct BRIXWORLD_API FBrixEstateRules
{
	struct FKind { FString Name; int64 PerM2 = 0, YieldBp = 0; TArray<TPair<FString, int64>> Owners; };
	struct FCompany { FString Id, Name, Office; TArray<FString> Focus; int64 Cash = 0, Reserve = 0, Cap = 0, DividendBp = 0, Weight = 0; };
	TMap<FString, FKind> Kinds;
	TArray<FCompany> Companies;                           // in the file's order
	// index
	int64 IdxMinBp = 0, IdxMaxBp = 0, WaveBp = 0, OccTargetPermille = 0, OccBpPerPermille = 0, PlayerBuyBp = 0, PlayerBuyCapBp = 0;
	int32 WaveDays = 7, PlayerBuyDays = 7;
	// costs
	int64 UpkeepBp = 0, TaxBp = 0, PortfolioBpPerUnit = 0, PortfolioCapBp = 0, SeizureFee = 0;
	int32 PortfolioFree = 0, MissedBeforeSeizure = 3;
	// trade
	int64 TransferTaxBp = 0, AgentFeeBp = 0, BidBp = 0, ForcedBidBp = 0, WholeDiscountBp = 0, AskLoBp = 0, AskHiBp = 0;
	TMap<FString, int64> OfferBp, ListBp;
	TArray<FString> CitySells;
	int64 ListHighIndexBp = 0, ListLowCashBp = 0, PriceCutBp = 0, SaleBp = 0, SaleSlopeBp = 0, SaleMaxBp = 0, HouseholdBuyBp = 0, HouseholdMinPoolDays = 0;
	int64 PlayerAskLoBp = 0, PlayerAskHiBp = 0;
	int32 ListingDays = 10;
	// letting
	int64 ArriveBp = 0, ArriveSlopeBp = 0, LeaveBp = 0, LeaveSlopeBp = 0, LodgerOccupiedPermille = 0, RentLoBp = 0, RentHiBp = 0;
	int32 MaxTenancies = 2, MissedBeforeEviction = 3;
	// mortgage
	int64 DepositBp = 0, MortgageRateBp = 0, MortgageLateFee = 0;
	TArray<int32> MortgageTerms;
	int32 MaxMortgages = 2, MortgageDefaultAfter = 3;
	// town
	TMap<FString, int64> Wages;
	int64 FirmCoverBp = 0, LodgerCoverBp = 0, PoolDays = 0, SpendBp = 0;
	// moving (PROP-022)
	int64 MoveBp = 0, OverBp = 0, MoveRentShareBp = 0, MoveBuyBp = 0;
	int32 HistoryDays = 30;
	// furnishing levels (FURN-062, FURN-063): World/rules/furnishing_levels.json
	bool bFurnishing = false;
	TArray<FString> Levels;
	TArray<TPair<int64, int32>> StartBands;               // from wage, level
	int64 SaveBp = 0;
	TArray<int64> StepCost;
	int32 LodgerLevel = 2, EmptyLevel = 1;

	bool Load(const TSharedPtr<FJsonObject>& J, FString& OutError);
	const FCompany* Company(const FString& Id) const { return Companies.FindByPredicate([&](const FCompany& C) { return C.Id == Id; }); }
};

// One unit of a building (PROP-009): estate.carve()'s {id, lot, lot_id, block, kind, floors, area, loc_bp, floor_bp,
// members, beds, tenant0}.
struct BRIXWORLD_API FBrixUnit
{
	FString Id, LotId, Block, Kind, Tenant0;
	int32 Lot = 0, Beds = 0;
	TArray<int32> Floors;
	int64 Area = 0, LocBp = 0, FloorBp = 0;
	TArray<FString> Members;
};

struct BRIXWORLD_API FBrixListing { int64 Price = 0; int32 Since = 0; FString Agent, By; };   // Agent empty: none
struct BRIXWORLD_API FBrixTenancy { int64 Rent = 0; int32 Since = 0, Missed = 0; };
struct BRIXWORLD_API FBrixMortgage
{
	int32 Id = 0, Day = 0, Missed = 0;
	int64 Principal = 0, Total = 0, Paid = 0, Fees = 0, FeesPaid = 0;
	TArray<int64> Plan;
	FString Status = TEXT("active");
	TArray<int32> Units;
	int64 Owed() const { return Total + Fees - Paid - FeesPaid; }
};
struct BRIXWORLD_API FBrixSale { int32 Day = 0, Unit = 0; int64 Price = 0; FString Seller, Buyer; };

class BRIXWORLD_API FBrixEstate
{
public:
	static const TCHAR* Town;                             // npc:town

	// The rules and units from life.json ("estate", "estate_units"), the residents' roles from its "residents".
	FBrixEstate(const FBrixEstateRules& Rules, FBrixEconomy& Econ, const TArray<FBrixUnit>& Units, const TMap<FString, FString>& Roles);
	~FBrixEstate();
	FBrixEstate(const FBrixEstate&) = delete;
	FBrixEstate& operator=(const FBrixEstate&) = delete;

	const FBrixEstateRules& R;
	FBrixEconomy& E;
	TArray<FBrixUnit> Units;
	TMap<FString, int32> ById;
	TMap<int32, TArray<int32>> ByLot;                     // lot -> its units
	TArray<FString> Blocks;
	TMap<FString, FString> Roles;
	TArray<TArray<FString>> Members;                      // who lives in each unit now (PROP-022)
	TArray<FIntVector> Moves;                             // the last households to move: day, from unit, to unit
	TArray<int64> Wage;
	TMap<FString, int32> Home;
	TMap<FString, FString> Office;                        // the lot of a company's office -> the company
	TArray<int64> Base;
	int64 BaseTotal = 0;
	bool bStarted = false;
	TArray<FString> Owner, Tenant;                        // Tenant empty: nobody
	TArray<int64> Rent;
	TArray<bool> ToLet;
	TMap<int32, FBrixTenancy> Tenancy;
	TMap<int32, FBrixListing> Listing;
	TMap<int32, int32> Missed;
	TMap<int32, int64> Owed;
	TArray<FBrixMortgage> Mortgages;
	TArray<TPair<int32, FString>> Buys;                   // day, block
	TMap<FString, int64> Idx;
	TArray<TArray<int64>> History;                        // day, town index, money supply, sales, earned, spent
	TArray<FBrixSale> Sales;
	TSet<FString> Short;
	int64 Income = 0;
	FString Why;

	FBrixLedger& L() const;
	void Start();
	// values (PROP-011, PROP-014, PROP-016)
	int64 Value(int32 i) const;
	int64 MarketRent(int32 i) const;
	int64 RentOf(int32 i) const;
	int64 TownIndex() const;
	int64 DayIncome() const;
	TArray<int32> Owned() const;
	int64 CostBp(int32 i) const;
	int64 CostOf(int32 i) const;
	FString Acct(const FString& Who) const;
	// the day (PROP-012, PROP-015 to PROP-019, PROP-022)
	void Settle(int32 D);
	// furnishing levels (FURN-062, FURN-063)
	int32 LevelOf(int32 i, int32 D = -1) const;
	int32 FurnishLevel(int64 Wage, int32 Days) const;
	// mortgages (BANK-002, PROP-019)
	TPair<int64, TArray<int64>> MortgageTerms(int64 Principal, int32 Term) const;
	FBrixMortgage* MortgageOn(int32 i);
	const FBrixMortgage* MortgageOn(int32 i) const;
	// what the player sees
	FString Name(int32 i) const;
	FString OwnerName(int32 i) const;
	TOptional<int64> Ask(int32 i) const;                  // the price to buy it at, unset when it is not for sale
	TPair<int32, int32> Board(int32 Lot) const;           // PROP-023: units for sale, units to let
	int64 Bid(int32 i) const;
	bool IsMine(int32 i) const { return Owner.IsValidIndex(i) && Owner[i] == TEXT("player"); }
	bool Rents(int32 i) const { return Tenant.IsValidIndex(i) && Tenant[i] == TEXT("player"); }
	TArray<int32> MyLots() const;                         // the lots where the player owns or rents a unit, in lot order
	bool OwnsWhole(int32 Lot) const;
	// what the player does (PROP-011 to PROP-014, PROP-019)
	bool Buy(const TArray<int32>& Which, const FString& Agent = FString(), TOptional<int64> Deposit = {}, TOptional<int32> Term = {}, int64* OutPaid = nullptr);
	bool SellNow(int32 i, const FString& Agent = FString(), int64* OutPrice = nullptr);
	bool ListForSale(int32 i, int64 Price, const FString& Agent);
	bool Unlist(int32 i);
	bool SetRent(int32 i, int64 NewRent, bool bToLet = true);
	bool RentUnit(int32 i, const FString& Agent = FString());
	bool Leave(int32 i);
	bool Repay(int32 Id);
	// saving, as estate.js save() and load() keep it (QST-008): units matched by id
	TSharedPtr<FJsonObject> Save() const;
	void Load(const TSharedPtr<FJsonObject>& S);

private:
	void Homes();
	FString FirstOwner(int32 i) const;
	FString CompanyFor(const FString& Kind, const TArray<FString>& Seed) const;
	int64 WaveOf(const FString& B, int32 D) const;
	void UpdateIndex(int32 D);
	int64 RentsAndCosts(int32 D);
	bool Pay(int32 D, int64 Amount, const FString& To, const FString& Memo);
	void Moving(int32 D);
	void Letting(int32 D);
	int32 Market(int32 D);
	FString Buyer(int32 i, int64 Price, int32 D) const;
	bool Transfer(int32 i, const FString& BuyerWho, int64 Price, int32 D, const FString& Agent, const FString& Memo, const TArray<TPair<FString, int64>>* Extra = nullptr, double Minute = 1440);
	void Seize(int32 i, int32 D);
	void ForcedSale(int32 i, int32 D, int64 OwedNow, const FString& To, const FString& Memo);
	void Companies(int32 D);
	int64 Furnishing(int32 D);
	int64 TownSpend(int32 D);
	void MortgageDays(int32 D);
	bool NeedOffice(const FString& Agent);
	bool No(const FString& Msg) { Why = Msg; return false; }
	int32 HookId = -1;
};

// The estate's rules and units from Game/Data/life.json (its "estate", "estate_units" and "residents").
BRIXWORLD_API bool LoadBrixEstate(const FBrixLifeData& Life, FBrixEstateRules& OutRules, TArray<FBrixUnit>& OutUnits, TMap<FString, FString>& OutRoles, FString& OutError);
