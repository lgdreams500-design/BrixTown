#include "BrixEstate.h"

#include "BrixDay.h"
#include "BrixEconomy.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

using BrixMoney::FloorDiv;

const TCHAR* FBrixEstate::Town = TEXT("npc:town");

namespace
{
	int64 Idiv(int64 A, int64 B) { return FloorDiv(A, B); }   // Python's // on whole numbers
	int64 Clamp(int64 V, int64 A, int64 B) { return V < A ? A : V > B ? B : V; }
	FString S(int64 V) { return FString::Printf(TEXT("%lld"), V); }
	int64 Int(const TSharedPtr<FJsonObject>& O, const TCHAR* K, int64 Or = 0)
	{
		double V = 0;
		return O.IsValid() && O->TryGetNumberField(K, V) ? (int64)V : Or;
	}
	TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		const TSharedPtr<FJsonObject>* P = nullptr;
		return O.IsValid() && O->TryGetObjectField(K, P) ? *P : MakeShared<FJsonObject>();
	}
	TArray<FString> Strs(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		TArray<FString> Out;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(K, A)) for (const auto& V : *A) Out.Add(V->AsString());
		return Out;
	}
	TPair<int64, int64> Range(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(K, A) && A->Num() == 2) return { (int64)(*A)[0]->AsNumber(), (int64)(*A)[1]->AsNumber() };
		return { 0, 0 };
	}
	TMap<FString, int64> IntMap(const TSharedPtr<FJsonObject>& O)
	{
		TMap<FString, int64> Out;
		for (const auto& V : O->Values) Out.Add(FString(V.Key.ToView()), (int64)V.Value->AsNumber());
		return Out;
	}
	double H(std::initializer_list<FString> P) { return BrixSliceHash(P); }
	const FString PCash = TEXT("p:cash"), PCurrent = TEXT("p:current"), Player = TEXT("player"), Hh = TEXT("hh"), Firm = TEXT("firm"),
		City = TEXT("city"), Lodger = TEXT("lodger"), SysTax = TEXT("sys:tax"), Vault = TEXT("bank:vault");
	bool IsHouseOrFlat(const FString& K) { return K == TEXT("house") || K == TEXT("flat"); }
}

// ---------------------------------------------------------------- the rules

bool FBrixEstateRules::Load(const TSharedPtr<FJsonObject>& J, FString& OutError)
{
	if (!J.IsValid()) { OutError = TEXT("no estate rules"); return false; }
	for (const auto& K : Obj(J, TEXT("kinds"))->Values)
	{
		const TSharedPtr<FJsonObject> O = K.Value->AsObject();
		FKind& Kd = Kinds.Add(FString(K.Key.ToView()));
		Kd.Name = O->GetStringField(TEXT("name"));
		Kd.PerM2 = Int(O, TEXT("per_m2"));
		Kd.YieldBp = Int(O, TEXT("yield_bp"));
		for (const auto& W : Obj(O, TEXT("owners"))->Values) Kd.Owners.Add({ FString(W.Key.ToView()), (int64)W.Value->AsNumber() });
	}
	for (const auto& V : J->GetArrayField(TEXT("companies")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FCompany& C = Companies.AddDefaulted_GetRef();
		C.Id = O->GetStringField(TEXT("id")); C.Name = O->GetStringField(TEXT("name")); C.Office = O->GetStringField(TEXT("office"));
		C.Focus = Strs(O, TEXT("focus"));
		C.Cash = Int(O, TEXT("cash")); C.Reserve = Int(O, TEXT("reserve")); C.Cap = Int(O, TEXT("cap"));
		C.DividendBp = Int(O, TEXT("dividend_bp")); C.Weight = Int(O, TEXT("weight"));
	}
	const TSharedPtr<FJsonObject> I = Obj(J, TEXT("index"));
	IdxMinBp = Int(I, TEXT("min_bp")); IdxMaxBp = Int(I, TEXT("max_bp")); WaveBp = Int(I, TEXT("wave_bp")); WaveDays = (int32)Int(I, TEXT("wave_days"), 7);
	OccTargetPermille = Int(I, TEXT("occupancy_target_permille")); OccBpPerPermille = Int(I, TEXT("occupancy_bp_per_permille"));
	PlayerBuyBp = Int(I, TEXT("player_buy_bp")); PlayerBuyDays = (int32)Int(I, TEXT("player_buy_days"), 7); PlayerBuyCapBp = Int(I, TEXT("player_buy_cap_bp"));
	const TSharedPtr<FJsonObject> C = Obj(J, TEXT("costs"));
	UpkeepBp = Int(C, TEXT("upkeep_bp")); TaxBp = Int(C, TEXT("tax_bp")); PortfolioFree = (int32)Int(C, TEXT("portfolio_free"));
	PortfolioBpPerUnit = Int(C, TEXT("portfolio_bp_per_unit")); PortfolioCapBp = Int(C, TEXT("portfolio_cap_bp"));
	MissedBeforeSeizure = (int32)Int(C, TEXT("missed_before_seizure"), 3); SeizureFee = Int(C, TEXT("seizure_fee"));
	const TSharedPtr<FJsonObject> T = Obj(J, TEXT("trade"));
	TransferTaxBp = Int(T, TEXT("transfer_tax_bp")); AgentFeeBp = Int(T, TEXT("agent_fee_bp")); BidBp = Int(T, TEXT("bid_bp"));
	ForcedBidBp = Int(T, TEXT("forced_bid_bp")); WholeDiscountBp = Int(T, TEXT("whole_building_discount_bp"));
	OfferBp = IntMap(Obj(T, TEXT("offer_bp"))); ListBp = IntMap(Obj(T, TEXT("list_bp"))); CitySells = Strs(T, TEXT("city_sells"));
	const TPair<int64, int64> Ask = Range(T, TEXT("ask_bp")); AskLoBp = Ask.Key; AskHiBp = Ask.Value;
	ListHighIndexBp = Int(T, TEXT("list_high_index_bp")); ListLowCashBp = Int(T, TEXT("list_low_cash_bp"));
	ListingDays = (int32)Int(T, TEXT("listing_days"), 10); PriceCutBp = Int(T, TEXT("price_cut_bp"));
	SaleBp = Int(T, TEXT("sale_bp")); SaleSlopeBp = Int(T, TEXT("sale_slope_bp")); SaleMaxBp = Int(T, TEXT("sale_max_bp"));
	HouseholdBuyBp = Int(T, TEXT("household_buy_bp")); HouseholdMinPoolDays = Int(T, TEXT("household_min_pool_days"));
	const TPair<int64, int64> PA = Range(T, TEXT("player_ask_range_bp")); PlayerAskLoBp = PA.Key; PlayerAskHiBp = PA.Value;
	const TSharedPtr<FJsonObject> Le = Obj(J, TEXT("letting"));
	ArriveBp = Int(Le, TEXT("arrive_bp")); ArriveSlopeBp = Int(Le, TEXT("arrive_slope_bp")); LeaveBp = Int(Le, TEXT("leave_bp"));
	LeaveSlopeBp = Int(Le, TEXT("leave_slope_bp")); LodgerOccupiedPermille = Int(Le, TEXT("lodger_occupied_permille"));
	const TPair<int64, int64> RR = Range(Le, TEXT("rent_range_bp")); RentLoBp = RR.Key; RentHiBp = RR.Value;
	MaxTenancies = (int32)Int(Le, TEXT("max_tenancies"), 2); MissedBeforeEviction = (int32)Int(Le, TEXT("missed_before_eviction"), 3);
	const TSharedPtr<FJsonObject> M = Obj(J, TEXT("mortgage"));
	DepositBp = Int(M, TEXT("deposit_bp")); MortgageRateBp = Int(M, TEXT("rate_bp")); MortgageLateFee = Int(M, TEXT("late_fee"));
	for (const FString& Tm : Strs(M, TEXT("terms"))) MortgageTerms.Add(FCString::Atoi(*Tm));
	MaxMortgages = (int32)Int(M, TEXT("max"), 2); MortgageDefaultAfter = (int32)Int(M, TEXT("default_after_missed"), 3);
	const TSharedPtr<FJsonObject> Tw = Obj(J, TEXT("town"));
	Wages = IntMap(Obj(Tw, TEXT("wages")));
	FirmCoverBp = Int(Tw, TEXT("firm_cover_bp")); LodgerCoverBp = Int(Tw, TEXT("lodger_cover_bp")); PoolDays = Int(Tw, TEXT("pool_days")); SpendBp = Int(Tw, TEXT("spend_bp"));
	const TSharedPtr<FJsonObject> Mv = Obj(J, TEXT("moving"));
	MoveBp = Int(Mv, TEXT("move_bp")); OverBp = Int(Mv, TEXT("over_bp")); MoveRentShareBp = Int(Mv, TEXT("rent_share_bp")); MoveBuyBp = Int(Mv, TEXT("buy_bp"));
	HistoryDays = (int32)Int(J, TEXT("history_days"), 30);
	const TSharedPtr<FJsonObject>* F = nullptr;
	if (J->TryGetObjectField(TEXT("furnishing"), F))
	{
		bFurnishing = true;
		Levels = Strs(*F, TEXT("levels"));
		for (const auto& B : (*F)->GetArrayField(TEXT("start_bands"))) StartBands.Add({ Int(B->AsObject(), TEXT("from_wage")), (int32)Int(B->AsObject(), TEXT("level")) });
		SaveBp = Int(*F, TEXT("save_bp"));
		for (const auto& V : (*F)->GetArrayField(TEXT("step_cost"))) StepCost.Add((int64)V->AsNumber());
		LodgerLevel = (int32)Int(*F, TEXT("lodger_level"), 2); EmptyLevel = (int32)Int(*F, TEXT("empty_level"), 1);
	}
	if (Companies.Num() < 2) { OutError = TEXT("the estate needs at least two companies (PROP-010)"); return false; }
	return true;
}

bool LoadBrixEstate(const FBrixLifeData& Life, FBrixEstateRules& OutRules, TArray<FBrixUnit>& OutUnits, TMap<FString, FString>& OutRoles, FString& OutError)
{
	const TSharedPtr<FJsonObject>& J = Life.Json;
	if (!J.IsValid() || !J->HasField(TEXT("estate")) || !J->HasField(TEXT("estate_units"))) { OutError = TEXT("life.json has no estate: run Tools/worldgen/build_play.py"); return false; }
	OutRules = FBrixEstateRules();
	if (!OutRules.Load(J->GetObjectField(TEXT("estate")), OutError)) return false;
	OutUnits.Reset();
	for (const auto& V : J->GetArrayField(TEXT("estate_units")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FBrixUnit& U = OutUnits.AddDefaulted_GetRef();
		U.Id = O->GetStringField(TEXT("id")); U.LotId = O->GetStringField(TEXT("lot_id")); U.Block = O->GetStringField(TEXT("block"));
		U.Kind = O->GetStringField(TEXT("kind"));
		O->TryGetStringField(TEXT("tenant0"), U.Tenant0);   // null: nobody
		U.Lot = (int32)Int(O, TEXT("lot")); U.Beds = (int32)Int(O, TEXT("beds"));
		for (const auto& F : O->GetArrayField(TEXT("floors"))) U.Floors.Add((int32)F->AsNumber());
		U.Area = Int(O, TEXT("area")); U.LocBp = Int(O, TEXT("loc_bp")); U.FloorBp = Int(O, TEXT("floor_bp"));
		U.Members = Strs(O, TEXT("members"));
		if (!OutRules.Kinds.Contains(U.Kind)) { OutError = TEXT("a unit of a kind the rules do not know: ") + U.Kind; return false; }
	}
	OutRoles.Reset();
	for (const auto& V : J->GetArrayField(TEXT("residents")))
		OutRoles.Add(V->AsObject()->GetStringField(TEXT("id")), V->AsObject()->GetStringField(TEXT("role")));
	return true;
}

// ---------------------------------------------------------------- the market

FBrixEstate::FBrixEstate(const FBrixEstateRules& Rules, FBrixEconomy& Econ, const TArray<FBrixUnit>& InUnits, const TMap<FString, FString>& InRoles)
	: R(Rules), E(Econ), Units(InUnits), Roles(InRoles)
{
	for (int32 i = 0; i < Units.Num(); ++i)
	{
		ById.Add(Units[i].Id, i);
		ByLot.FindOrAdd(Units[i].Lot).Add(i);
		Blocks.AddUnique(Units[i].Block);
		Members.Add(Units[i].Members);
	}
	Homes();
	for (const FBrixEstateRules::FCompany& C : R.Companies) Office.Add(C.Office, C.Id);
	for (const FBrixUnit& U : Units)
	{
		Base.Add(Idiv(Idiv(U.Area * R.Kinds[U.Kind].PerM2 * U.LocBp, 10000) * U.FloorBp, 10000));
		BaseTotal += Base.Last();
	}
	for (int32 i = 0; i < Units.Num(); ++i) Owner.Add(FirstOwner(i));
	for (const FBrixUnit& U : Units) { Tenant.Add(U.Tenant0); Rent.Add(0); ToLet.Add(false); }
	for (const FString& B : Blocks) Idx.Add(B, 10000);
	HookId = E.NextHook++;
	E.Hooks.Add(HookId, [this](int32 D) { Settle(D); });
}

FBrixEstate::~FBrixEstate() { E.Hooks.Remove(HookId); }

FBrixLedger& FBrixEstate::L() const { return E.L; }

void FBrixEstate::Homes()
{
	Wage.Reset();
	Home.Reset();
	for (int32 i = 0; i < Members.Num(); ++i)
	{
		int64 W = 0;
		for (const FString& M : Members[i]) { if (const FString* Role = Roles.Find(M)) W += R.Wages.FindRef(*Role); Home.Add(M, i); }
		Wage.Add(W);
	}
}

FString FBrixEstate::FirstOwner(int32 i) const                // PROP-010
{
	const FBrixUnit& U = Units[i];
	TArray<TPair<FString, int64>> Opts;
	for (const auto& W : R.Kinds[U.Kind].Owners)
		if (W.Value > 0 && !(W.Key == TEXT("household") && U.Tenant0 != Hh) && !(W.Key == TEXT("firm") && U.Tenant0 != Firm)) Opts.Add(W);
	if (!Opts.Num()) Opts.Add({ City, 1 });
	int64 Sum = 0;
	for (const auto& O : Opts) Sum += O.Value;
	double t = H({ U.Id, TEXT("owner") }) * (double)Sum;
	FString Pick = Opts.Last().Key;
	for (const auto& O : Opts) { if (t < (double)O.Value) { Pick = O.Key; break; } t -= (double)O.Value; }
	if (Pick == TEXT("household")) return Hh;
	if (Pick == Firm) return Firm;
	if (Pick == City) return City;
	return TEXT("co:") + CompanyFor(U.Kind, { U.Id, TEXT("first") });
}

FString FBrixEstate::CompanyFor(const FString& Kind, const TArray<FString>& Seed) const
{
	TArray<const FBrixEstateRules::FCompany*> Cs;
	for (const auto& C : R.Companies) if (C.Focus.Contains(Kind)) Cs.Add(&C);
	if (!Cs.Num()) for (const auto& C : R.Companies) Cs.Add(&C);
	FString Best;
	double Bs = -1;
	for (const auto* C : Cs)
	{
		// hash01(c.id, ...seed)
		const double Hs = Seed.Num() == 3 ? H({ C->Id, Seed[0], Seed[1], Seed[2] }) : Seed.Num() == 2 ? H({ C->Id, Seed[0], Seed[1] }) : H({ C->Id, Seed[0] });
		const double Sc = Hs * (double)C->Weight;
		if (Sc > Bs) { Best = C->Id; Bs = Sc; }
	}
	return Best;
}

void FBrixEstate::Start()
{
	if (bStarted) return;
	bStarted = true;
	TArray<TPair<FString, int64>> Lines;
	for (const auto& C : R.Companies) Lines.Add({ TEXT("co:") + C.Id, C.Cash });
	Lines.Add({ Town, R.PoolDays * DayIncome() });
	int64 Sum = 0;
	for (const auto& Ln : Lines) Sum += Ln.Value;
	Lines.Add({ TEXT("sys:founding"), -Sum });
	L().Post(E.Day, E.Minute, TEXT("The estate agents open; the town's households and businesses begin"), Lines, Why);
}

// ---- values (PROP-011)
int64 FBrixEstate::Value(int32 i) const { return Idiv(Base[i] * Idx[Units[i].Block], 10000); }
int64 FBrixEstate::MarketRent(int32 i) const { return FMath::Max<int64>(1, Idiv(Value(i) * R.Kinds[Units[i].Kind].YieldBp, 10000)); }
int64 FBrixEstate::RentOf(int32 i) const                     // PROP-014
{
	if (Tenant[i] == Player) return Tenancy[i].Rent;
	if (Owner[i] == Player)
	{
		const int64 Rt = Rent[i] ? Rent[i] : MarketRent(i);
		return (Tenant[i] == Hh || Tenant[i] == Firm) ? FMath::Min(Rt, MarketRent(i)) : Rt;
	}
	return MarketRent(i);
}
int64 FBrixEstate::TownIndex() const
{
	int64 s = 0;
	for (int32 i = 0; i < Units.Num(); ++i) s += Value(i);
	return Idiv(s * 10000, BaseTotal);
}
int64 FBrixEstate::DayIncome() const
{
	int64 n = 0;
	for (int32 i = 0; i < Units.Num(); ++i)
	{
		const FString& t = Tenant[i];
		if (t == Hh) n += Wage[i];
		else if (t == Firm) n += Owner[i] != Firm ? Idiv(RentOf(i) * R.FirmCoverBp, 10000) : 0;
		else if (t == Lodger) n += Idiv(RentOf(i) * R.LodgerCoverBp, 10000);
	}
	return n;
}
TArray<int32> FBrixEstate::Owned() const
{
	TArray<int32> O;
	for (int32 i = 0; i < Owner.Num(); ++i) if (Owner[i] == Player) O.Add(i);
	return O;
}
int64 FBrixEstate::CostBp(int32 i) const
{
	int64 Bp = R.UpkeepBp + R.TaxBp;
	if (Owner[i] == Player) Bp += FMath::Min<int64>(R.PortfolioCapBp, R.PortfolioBpPerUnit * FMath::Max(0, Owned().Num() - R.PortfolioFree));
	return Bp;
}
int64 FBrixEstate::CostOf(int32 i) const { return Idiv(Value(i) * CostBp(i), 10000); }
FString FBrixEstate::Acct(const FString& Who) const
{
	if (Who == City) return TEXT("sys:city");
	if (Who == Hh || Who == Firm) return Town;
	if (Who == Player) return E.HasAccount(TEXT("current")) ? PCurrent : PCash;
	return Who;
}

// ---- the day (PROP-012, PROP-015, PROP-016)
int64 FBrixEstate::WaveOf(const FString& B, int32 D) const
{
	const int64 n = R.WaveDays, w = Idiv(D, n), t = D - w * n;
	const int64 h0 = (int64)FMath::FloorToDouble(H({ B, S(w), TEXT("wave") }) * 2001) - 1000;
	const int64 h1 = (int64)FMath::FloorToDouble(H({ B, S(w + 1), TEXT("wave") }) * 2001) - 1000;
	return Idiv((h0 * (n - t) + h1 * t) * R.WaveBp, n * 1000);
}

void FBrixEstate::UpdateIndex(int32 D)
{
	TMap<FString, int64> Occ, Tot, Bump;
	for (int32 i = 0; i < Units.Num(); ++i)
	{
		Tot.FindOrAdd(Units[i].Block) += 1;
		if (!Tenant[i].IsEmpty()) Occ.FindOrAdd(Units[i].Block) += 1;
	}
	for (const auto& Bu : Buys)
		if (D - Bu.Key < R.PlayerBuyDays) { int64& b = Bump.FindOrAdd(Bu.Value); b = FMath::Min<int64>(R.PlayerBuyCapBp, b + R.PlayerBuyBp); }
	for (const FString& B : Blocks)
	{
		const int64 o = Idiv(Occ.FindRef(B) * 1000, Tot[B]);
		Idx.Add(B, Clamp(10000 + WaveOf(B, D) + (o - R.OccTargetPermille) * R.OccBpPerPermille + Bump.FindRef(B), R.IdxMinBp, R.IdxMaxBp));
	}
	Buys.RemoveAll([&](const TPair<int32, FString>& x) { return !(D - x.Key < R.PlayerBuyDays); });
}

void FBrixEstate::Settle(int32 D)
{
	if (!bStarted) return;
	UpdateIndex(D);
	const int64 Earned = RentsAndCosts(D);
	int64 Spent = 0;
	Moving(D);
	Letting(D);
	const int32 Sold = Market(D);
	MortgageDays(D);
	Companies(D);
	Spent += Furnishing(D);                                   // FURN-063, before the town spends what is left over
	Spent += TownSpend(D);
	History.Add({ D, TownIndex(), L().Supply(), Sold, Earned, Spent });
	while (History.Num() > R.HistoryDays) History.RemoveAt(0);
}

int64 FBrixEstate::RentsAndCosts(int32 D)
{
	TArray<TPair<FString, int64>> Npc;                        // in the order first touched (JS Map)
	auto Add = [&](const FString& A, int64 V)
	{
		if (!V) return;
		for (auto& P : Npc) if (P.Key == A) { P.Value += V; return; }
		Npc.Add({ A, V });
	};
	auto Get = [&](const FString& A) -> int64 { for (auto& P : Npc) if (P.Key == A) return P.Value; return 0; };
	auto Set = [&](const FString& A, int64 V) { for (auto& P : Npc) if (P.Key == A) { P.Value = V; return; } Npc.Add({ A, V }); };
	TArray<TPair<int32, int64>> PCosts, PRentOut;
	int64 PRentIn = 0, Inc = 0;
	for (int32 i = 0; i < Units.Num(); ++i)
	{
		const FString& o = Owner[i];
		const FString& t = Tenant[i];
		if (o != City)
		{
			const int64 c = CostOf(i);
			if (o == Player) PCosts.Add({ i, c }); else { Add(Acct(o), -c); Add(SysTax, c); }
		}
		if (!t.IsEmpty() && !(t == Hh && o == Hh) && !(t == Firm && o == Firm))
		{
			const int64 r = RentOf(i);
			if (t == Player) PRentOut.Add({ i, r });
			else if (o == Player) PRentIn += r;
			else { Add(Town, -r); Add(Acct(o), r); }
		}
		int64 w = 0;
		if (t == Hh) w = Wage[i];
		else if (t == Firm && o != Firm) w = Idiv(RentOf(i) * R.FirmCoverBp, 10000);
		else if (t == Lodger) w = Idiv(RentOf(i) * R.LodgerCoverBp, 10000);
		Add(Town, w); Add(TEXT("sys:wages"), -w); Inc += w;
	}
	Short.Reset();
	const TArray<TPair<FString, int64>> Keys = Npc;
	for (const auto& K : Keys)
	{
		const FString& a = K.Key;
		if (a.StartsWith(TEXT("co:")) && L().Balance(a) + Get(a) < 0)
		{
			const int64 Gap = -(L().Balance(a) + Get(a));
			Set(a, Get(a) + Gap); Set(SysTax, Get(SysTax) - Gap); Short.Add(a.Mid(3));
		}
	}
	if (L().Balance(Town) + Get(Town) - PRentIn < 0)
	{
		const int64 Gap = -(L().Balance(Town) + Get(Town) - PRentIn);
		Set(Town, Get(Town) + Gap); Set(TEXT("sys:wages"), Get(TEXT("sys:wages")) - Gap); Inc += Gap;
	}
	TArray<TPair<FString, int64>> Lines;
	for (const auto& P : Npc) if (P.Value) Lines.Add(P);
	if (Lines.Num()) L().Post(D, 1440, TEXT("Rents, upkeep, tax and earnings across town"), Lines, Why);
	Income = Inc;
	if (PRentIn) L().Post(D, 1440, TEXT("Rent from your tenants"), { { Town, -PRentIn }, { Acct(Player), PRentIn } }, Why);
	for (const auto& Ro : PRentOut)                           // PROP-013
	{
		const int32 i = Ro.Key;
		FBrixTenancy& Ten = Tenancy[i];
		if (Pay(D, Ro.Value, Acct(Owner[i]), TEXT("Rent, ") + Name(i))) Ten.Missed = 0;
		else if (++Ten.Missed >= R.MissedBeforeEviction) { Tenant[i].Reset(); Tenancy.Remove(i); }
	}
	for (const auto& Co : PCosts)                             // PROP-006, PROP-017
	{
		const int32 i = Co.Key;
		const int64 Due = Co.Value + Owed.FindRef(i);
		if (Due && Pay(D, Due, SysTax, TEXT("Upkeep and tax, ") + Name(i))) { Owed.Remove(i); Missed.Remove(i); }
		else if (Due)
		{
			Owed.Add(i, Due);
			if (++Missed.FindOrAdd(i) >= R.MissedBeforeSeizure) Seize(i, D);
		}
	}
	return Inc;
}

bool FBrixEstate::Pay(int32 D, int64 Amount, const FString& To, const FString& Memo)
{
	if (Amount <= 0) return true;
	const int64 Cash = L().Balance(PCash), Cur = E.HasAccount(TEXT("current")) ? L().Balance(PCurrent) : 0;
	if (Cash + Cur < Amount) return false;
	const int64 a = FMath::Min(Cash, Amount);
	TArray<TPair<FString, int64>> Lines;
	if (a) Lines.Add({ PCash, -a });
	if (Amount > a) Lines.Add({ PCurrent, -(Amount - a) });
	Lines.Add({ To, Amount });
	L().Post(D, D == E.Day && E.Minute < 1440 ? E.Minute : 1440, Memo, Lines, Why);
	return true;
}

void FBrixEstate::Moving(int32 D)                             // PROP-022
{
	TArray<int32> Cands;
	for (int32 j = 0; j < Units.Num(); ++j)
		if (IsHouseOrFlat(Units[j].Kind) && Tenant[j].IsEmpty() && Units[j].Beds > 0 && (Owner[j] != Player || ToLet[j])) Cands.Add(j);
	if (!Cands.Num()) return;
	for (int32 i = 0; i < Units.Num(); ++i)
	{
		const TArray<FString> Ms = Members[i];
		if (Tenant[i] != Hh || Owner[i] == Hh || !Ms.Num()) continue;
		const int64 Budget = Idiv(Wage[i] * R.MoveRentShareBp, 10000);
		const int64 Ch = R.MoveBp + (RentOf(i) > Budget ? R.OverBp : 0);
		if (H({ Units[i].Id, S(D), TEXT("move") }) * 10000 >= (double)Ch) continue;
		int32 Best = -1;
		double Bs = -1;
		for (const int32 j : Cands)
		{
			if (!Tenant[j].IsEmpty() || Units[j].Beds < Ms.Num() || RentOf(j) > Budget) continue;
			const double Sc = H({ Units[i].Id, Units[j].Id, S(D), TEXT("pick") });
			if (Sc > Bs) { Best = j; Bs = Sc; }
		}
		if (Best < 0) continue;
		const int32 j = Best;
		if (const FBrixListing* Ls = Listing.Find(j); Ls && Owner[j] != Player)
		{
			const int64 Tax = Idiv(Ls->Price * R.TransferTaxBp, 10000);
			if (H({ Units[i].Id, S(D), TEXT("buyhome") }) * 10000 < (double)R.MoveBuyBp && L().Balance(Town) >= Ls->Price + Tax + R.HouseholdMinPoolDays * Income)
			{
				const FBrixListing Copy = *Ls;
				Transfer(j, Hh, Copy.Price, D, Copy.Agent, TEXT("Sold: ") + Name(j));
			}
		}
		Members[j] = Ms; Members[i].Reset();
		Tenant[j] = Hh; Tenant[i].Reset();
		Homes();
		Moves.Add(FIntVector(D, i, j));
		while (Moves.Num() > 40) Moves.RemoveAt(0);
	}
}

void FBrixEstate::Letting(int32 D)                            // PROP-015
{
	for (int32 i = 0; i < Units.Num(); ++i)
	{
		const bool bMine = Owner[i] == Player;
		const FString& t = Tenant[i];
		const int64 Ratio = Idiv((bMine ? RentOf(i) : MarketRent(i)) * 10000, MarketRent(i));
		if (t == Lodger)
		{
			const int64 Ch = R.LeaveBp + (bMine ? Idiv(R.LeaveSlopeBp * FMath::Max<int64>(0, Ratio - 10000), 10000) : 0);
			if (H({ Units[i].Id, S(D), TEXT("leave") }) * 10000 < (double)Ch) Tenant[i].Reset();
		}
		else if (t.IsEmpty() && (!bMine || ToLet[i]))
		{
			const int64 Ch = bMine ? Clamp(R.ArriveBp - Idiv(R.ArriveSlopeBp * (Ratio - 10000), 10000), 0, 10000) : R.ArriveBp;
			if (H({ Units[i].Id, S(D), TEXT("arrive") }) * 10000 < (double)Ch) Tenant[i] = Lodger;
		}
	}
}

int32 FBrixEstate::Market(int32 D)                            // PROP-012, PROP-016
{
	int32 Sold = 0;
	for (int32 i = 0; i < Units.Num(); ++i)
	{
		if (FBrixListing* Ls = Listing.Find(i))
		{
			const int64 Ratio = Idiv(Value(i) * 10000, Ls->Price);
			const int64 Ch = Clamp(R.SaleBp + Idiv(R.SaleSlopeBp * (Ratio - 10000), 10000), 0, R.SaleMaxBp);
			if (H({ Units[i].Id, S(D), TEXT("sale") }) * 10000 < (double)Ch)
			{
				const FString B = Buyer(i, Ls->Price, D);
				if (!B.IsEmpty())
				{
					const FBrixListing Copy = *Ls;
					if (Transfer(i, B, Copy.Price, D, Copy.Agent, TEXT("Sold: ") + Name(i))) { ++Sold; continue; }
					Ls = Listing.Find(i);
					if (!Ls) continue;
				}
			}
			if (Owner[i] != Player && D - Ls->Since > 0 && (D - Ls->Since) % R.ListingDays == 0) Ls->Price = Idiv(Ls->Price * (10000 - R.PriceCutBp), 10000);
		}
		else if (Owner[i] != Player && Owner[i] != City)
		{
			const FString& o = Owner[i];
			const FString Typ = o.StartsWith(TEXT("co:")) ? TEXT("company") : o == Hh ? TEXT("household") : TEXT("firm");
			int64 Ch = R.ListBp.FindRef(Typ) + Idiv(FMath::Max<int64>(0, Idx[Units[i].Block] - 10000) * R.ListHighIndexBp, 100);
			if (Typ == TEXT("company") && Short.Contains(o.Mid(3))) Ch += R.ListLowCashBp;
			if (Typ == TEXT("company") && L().Balance(o) < R.Company(o.Mid(3))->Reserve) Ch += R.ListLowCashBp;
			if (H({ Units[i].Id, S(D), TEXT("list") }) * 10000 < (double)Ch)
			{
				const int64 Ask = R.AskLoBp + (int64)FMath::FloorToDouble(H({ Units[i].Id, S(D), TEXT("ask") }) * (double)(R.AskHiBp - R.AskLoBp + 1));
				const FString Agent = Typ == TEXT("company") ? o.Mid(3) : CompanyFor(Units[i].Kind, { Units[i].Id, S(D), TEXT("agent") });
				Listing.Add(i, { Idiv(Value(i) * Ask, 10000), D, Agent, o });
			}
		}
	}
	return Sold;
}

FString FBrixEstate::Buyer(int32 i, int64 Price, int32 D) const
{
	const FBrixUnit& U = Units[i];
	const int64 Tax = Idiv(Price * R.TransferTaxBp, 10000);
	if (Tenant[i] == Hh && Owner[i] != Hh)
		if (H({ U.Id, S(D), TEXT("hh") }) * 10000 < (double)R.HouseholdBuyBp && L().Balance(Town) >= Price + Tax + R.HouseholdMinPoolDays * Income) return Hh;
	FString Best;
	double Bs = -1;
	for (const auto& C : R.Companies)
	{
		const FString a = TEXT("co:") + C.Id;
		if (a == Owner[i] || !C.Focus.Contains(U.Kind)) continue;
		if (L().Balance(a) < Price + Tax + C.Reserve) continue;
		const double Sc = H({ C.Id, U.Id, S(D), TEXT("buyer") }) * (double)C.Weight;
		if (Sc > Bs) { Best = a; Bs = Sc; }
	}
	return Best;
}

bool FBrixEstate::Transfer(int32 i, const FString& BuyerWho, int64 Price, int32 D, const FString& Agent, const FString& Memo, const TArray<TPair<FString, int64>>* Extra, double Minute)
{
	const FString Seller = Owner[i];
	const int64 Tax = Idiv(Price * R.TransferTaxBp, 10000);
	const int64 Fee = (Agent.IsEmpty() || Seller == TEXT("co:") + Agent) ? 0 : Idiv(Price * R.AgentFeeBp, 10000);
	TArray<TPair<FString, int64>> Lines = Extra ? *Extra : TArray<TPair<FString, int64>>{ { Acct(BuyerWho), -(Price + Tax) } };
	Lines.Add({ SysTax, Tax });
	int64 Debt = 0;
	FBrixMortgage* M = Seller == Player ? MortgageOn(i) : nullptr;
	if (M)
	{
		Debt = M->Owed();
		if (Price - Fee < Debt) return No(TEXT("the price would not pay off the mortgage"));
		Lines.Add({ Vault, Debt });
	}
	Lines.Add({ Acct(Seller), Price - Fee - Debt });
	if (Fee) Lines.Add({ TEXT("co:") + Agent, Fee });
	if (!L().Post(D, Minute, Memo, Lines, Why)) return false;
	if (M && Debt) { M->Paid += Debt; M->Status = TEXT("repaid"); }
	Owner[i] = BuyerWho;
	if (BuyerWho == Player) { Rent[i] = MarketRent(i); ToLet[i] = true; Buys.Add({ D, Units[i].Block }); }
	else ToLet[i] = false;
	Listing.Remove(i); Owed.Remove(i); Missed.Remove(i);
	Sales.Add({ D, i, Price, Seller, BuyerWho });
	while (Sales.Num() > 40) Sales.RemoveAt(0);
	return true;
}

void FBrixEstate::Seize(int32 i, int32 D)                     // PROP-017
{
	ForcedSale(i, D, Owed.FindRef(i) + R.SeizureFee, SysTax, TEXT("Seized for unpaid tax: ") + Name(i));
}

void FBrixEstate::ForcedSale(int32 i, int32 D, int64 OwedNow, const FString& To, const FString& Memo)
{
	const int64 Price = Idiv(Value(i) * R.ForcedBidBp, 10000), Tax = Idiv(Price * R.TransferTaxBp, 10000);
	FString BuyerWho;
	for (const auto& C : R.Companies) if (L().Balance(TEXT("co:") + C.Id) >= Price + Tax) { BuyerWho = TEXT("co:") + C.Id; break; }
	if (BuyerWho.IsEmpty()) BuyerWho = City;
	FBrixMortgage* M = MortgageOn(i);
	const int64 Debt = M ? M->Owed() : 0, Take = FMath::Min(Price, OwedNow);
	TArray<TPair<FString, int64>> Lines = { { Acct(BuyerWho), -(Price + Tax) }, { SysTax, Tax }, { To, Take } };
	const int64 Back = FMath::Min(Price - Take, Debt);
	if (Back) Lines.Add({ Vault, Back });
	const int64 Rest = Price - Take - Back;
	if (Rest) Lines.Add({ Acct(Player), Rest });
	L().Post(D, 1440, Memo, Lines, Why);
	if (M) { M->Paid += Back; M->Status = Back >= Debt ? TEXT("repaid") : TEXT("defaulted"); }
	Owner[i] = BuyerWho; ToLet[i] = false;
	Listing.Remove(i); Owed.Remove(i); Missed.Remove(i);
	Sales.Add({ D, i, Price, Player, BuyerWho });
	while (Sales.Num() > 40) Sales.RemoveAt(0);
}

void FBrixEstate::Companies(int32 D)                          // PROP-018
{
	for (const auto& C : R.Companies)
	{
		const FString a = TEXT("co:") + C.Id;
		const int64 Over = L().Balance(a) - C.Cap;
		if (Over > 0)
		{
			const int64 P = Idiv(Over * C.DividendBp, 10000);
			if (P) L().Post(D, 1440, C.Name + TEXT(" pays a dividend"), { { a, -P }, { TEXT("sys:dividends"), P } }, Why);
		}
	}
}

// ---- furnishing levels (FURN-062, FURN-063): estate.js furnish
int32 FBrixEstate::FurnishLevel(int64 W, int32 Days) const
{
	int32 Lv = 1;
	for (const auto& B : R.StartBands) if (W >= B.Key) Lv = FMath::Max(Lv, B.Value);
	const int64 Saved = Idiv(W * R.SaveBp, 10000) * FMath::Max(0, Days);
	int64 Spent = 0;
	while (Lv < R.Levels.Num() && Saved >= Spent + R.StepCost[Lv - 1]) { Spent += R.StepCost[Lv - 1]; ++Lv; }
	return Lv;
}

int32 FBrixEstate::LevelOf(int32 i, int32 D) const
{
	if (D < 0) D = E.Day;
	if (!R.bFurnishing) return 1;
	const FString& t = Tenant[i];
	if (t == Hh && Members[i].Num()) return FurnishLevel(Wage[i], D);
	return t == Lodger ? R.LodgerLevel : R.EmptyLevel;
}

int64 FBrixEstate::Furnishing(int32 D)                        // FURN-063: a rise's cost, residents' account to sys:spend
{
	if (!R.bFurnishing) return 0;
	int64 Paid = 0;
	for (int32 i = 0; i < Units.Num(); ++i)
	{
		if (Tenant[i] != Hh || !Members[i].Num()) continue;
		const int32 a = FurnishLevel(Wage[i], D - 1), b = FurnishLevel(Wage[i], D);
		int64 Cost = 0;
		for (int32 k = a; k < b; ++k) Cost += R.StepCost[k - 1];
		const int64 c = FMath::Min(Cost, L().Balance(Town));
		if (c > 0)
		{
			L().Post(D, 1440, TEXT("Furniture for ") + Name(i) + TEXT(" (") + R.Levels[b - 1] + TEXT(")"), { { Town, -c }, { TEXT("sys:spend"), c } }, Why);
			Paid += c;
		}
	}
	return Paid;
}

int64 FBrixEstate::TownSpend(int32 D)
{
	const int64 Over = L().Balance(Town) - R.PoolDays * Income;
	if (Over <= 0) return 0;
	const int64 P = Idiv(Over * R.SpendBp, 10000);
	if (P) L().Post(D, 1440, TEXT("What the town's households and businesses spend"), { { Town, -P }, { TEXT("sys:spend"), P } }, Why);
	return P;
}

// ---- mortgages (BANK-002, PROP-019)
TPair<int64, TArray<int64>> FBrixEstate::MortgageTerms(int64 Principal, int32 Term) const
{
	const int64 Total = Principal + Idiv(Principal * R.MortgageRateBp * Term, 10000), Inst = Idiv(Total, Term);
	TArray<int64> Plan;
	for (int32 k = 0; k < Term - 1; ++k) Plan.Add(Inst);
	Plan.Add(Total - Inst * (Term - 1));
	return { Total, Plan };
}
FBrixMortgage* FBrixEstate::MortgageOn(int32 i)
{
	return Mortgages.FindByPredicate([&](const FBrixMortgage& M) { return M.Status == TEXT("active") && M.Units.Contains(i); });
}
const FBrixMortgage* FBrixEstate::MortgageOn(int32 i) const
{
	return Mortgages.FindByPredicate([&](const FBrixMortgage& M) { return M.Status == TEXT("active") && M.Units.Contains(i); });
}

void FBrixEstate::MortgageDays(int32 D)
{
	for (int32 n = 0; n < Mortgages.Num(); ++n)
	{
		FBrixMortgage& M = Mortgages[n];
		if (M.Status != TEXT("active")) continue;
		const int32 k = D - M.Day;
		if (k < 1) continue;
		int64 Plan = 0;
		for (int32 q = 0; q < FMath::Min(k, M.Plan.Num()); ++q) Plan += M.Plan[q];
		int64 Due = FMath::Max<int64>(0, Plan - M.Paid) + M.Fees - M.FeesPaid;
		Due = FMath::Min(Due, M.Owed());
		if (Due <= 0) continue;
		if (E.HasAccount(TEXT("current")) && L().Balance(PCurrent) >= Due)
		{
			const int64 Fee = M.Fees - M.FeesPaid;
			TArray<TPair<FString, int64>> Lines = { { PCurrent, -Due } };
			if (Fee) Lines.Add({ TEXT("sys:fees"), Fee });
			if (Due - Fee) Lines.Add({ Vault, Due - Fee });
			L().Post(D, 1440, FString::Printf(TEXT("Mortgage %d repayment"), M.Id), Lines, Why);
			M.FeesPaid += Fee; M.Paid += Due - Fee; M.Missed = 0;
			if (M.Owed() <= 0) M.Status = TEXT("repaid");
			continue;
		}
		++M.Missed; M.Fees += R.MortgageLateFee;
		if (M.Missed >= R.MortgageDefaultAfter)
		{
			const TArray<int32> Us = M.Units;
			for (const int32 i : Us) if (Owner[i] == Player && Mortgages[n].Status == TEXT("active")) ForcedSale(i, D, 0, TEXT("sys:fees"), TEXT("Repossessed by the bank: ") + Name(i));
			if (Mortgages[n].Status == TEXT("active")) Mortgages[n].Status = TEXT("defaulted");
			E.bBarred = true;
		}
	}
}

// ---- what the player sees
FString FBrixEstate::Name(int32 i) const
{
	const FBrixUnit& U = Units[i];
	const FString LotName = E.Lots.IsValidIndex(U.Lot) ? E.Lots[U.Lot].Name : U.LotId;
	if (ByLot[U.Lot].Num() == 1) return LotName;
	const int32 f = U.Floors[0];
	return LotName + TEXT(", ") + R.Kinds[U.Kind].Name.ToLower() + (f == 0 && U.Floors.Num() > 1 ? FString() : f ? FString::Printf(TEXT(" on floor %d"), f) : FString(TEXT(" on the ground floor")));
}

FString FBrixEstate::OwnerName(int32 i) const
{
	const FString& o = Owner[i];
	if (o == Player) return TEXT("you");
	if (o == City) return TEXT("the town");
	if (o == Hh) return TEXT("the household living there");
	if (o == Firm) return TEXT("the business there");
	const FBrixEstateRules::FCompany* C = R.Company(o.Mid(3));
	return C ? C->Name : o;
}

TOptional<int64> FBrixEstate::Ask(int32 i) const              // PROP-011
{
	if (Owner[i] == Player) return {};
	if (const FBrixListing* Ls = Listing.Find(i)) return Ls->Price;
	const FString& o = Owner[i];
	if (o == City && !R.CitySells.Contains(Units[i].Kind)) return {};
	const FString Typ = o.StartsWith(TEXT("co:")) ? TEXT("company") : o == Hh ? TEXT("household") : o;
	return Idiv(Value(i) * R.OfferBp.FindRef(Typ), 10000);
}

TPair<int32, int32> FBrixEstate::Board(int32 Lot) const      // PROP-023
{
	int32 Sale = 0, Let = 0;
	if (const TArray<int32>* Ix = ByLot.Find(Lot))
		for (const int32 i : *Ix)
		{
			if (Listing.Contains(i) && Owner[i] != Player) ++Sale;
			if (Tenant[i].IsEmpty() && Owner[i] != Player) ++Let;
		}
	return { Sale, Let };
}

int64 FBrixEstate::Bid(int32 i) const { return Idiv(Value(i) * R.BidBp, 10000); }

TArray<int32> FBrixEstate::MyLots() const
{
	TArray<int32> Out;
	for (int32 i = 0; i < Units.Num(); ++i) if (Owner[i] == Player || Tenant[i] == Player) Out.AddUnique(Units[i].Lot);
	Out.Sort();
	return Out;
}

bool FBrixEstate::OwnsWhole(int32 Lot) const
{
	const TArray<int32>* Ix = ByLot.Find(Lot);
	if (!Ix || !Ix->Num()) return false;
	for (const int32 i : *Ix) if (Owner[i] != Player) return false;
	return true;
}

// ---- what the player does
bool FBrixEstate::NeedOffice(const FString& Agent)
{
	if (!Agent.IsEmpty() && !R.Company(Agent)) return No(TEXT("no such estate agent"));
	return true;
}

bool FBrixEstate::Buy(const TArray<int32>& Which, const FString& Agent, TOptional<int64> Deposit, TOptional<int32> Term, int64* OutPaid)   // PROP-011, PROP-019
{
	if (!NeedOffice(Agent)) return false;
	if (!Which.Num()) return No(TEXT("nothing to buy"));
	TArray<int64> Prices;
	for (const int32 i : Which)
	{
		if (!Units.IsValidIndex(i)) return No(TEXT("no such unit"));
		const TOptional<int64> P = Ask(i);
		if (!P) return No(Name(i) + TEXT(" is not for sale"));
		Prices.Add(*P);
	}
	if (Which.Num() > 1)
	{
		const int32 Lot = Units[Which[0]].Lot;
		TArray<int32> Rest, Got = Which;
		for (const int32 i : ByLot[Lot]) if (Owner[i] != Player) Rest.Add(i);
		Rest.Sort(); Got.Sort();
		bool bSameLot = true;
		for (const int32 i : Which) if (Units[i].Lot != Lot) bSameLot = false;
		if (!bSameLot || Rest != Got) return No(TEXT("a whole building is bought together"));
		for (int64& P : Prices) P = Idiv(P * (10000 - R.WholeDiscountBp), 10000);
	}
	int64 Total = 0, Tax = 0;
	for (const int64 P : Prices) { Total += P; Tax += Idiv(P * R.TransferTaxBp, 10000); }
	int64 Loan = 0;
	if (Deposit.IsSet() || Term.IsSet())
	{
		if (!E.HasAccount(TEXT("current"))) return No(TEXT("a mortgage needs a current account"));
		if (E.bBarred) return No(TEXT("the bank will not lend to you again"));
		int32 Active = 0;
		for (const auto& M : Mortgages) if (M.Status == TEXT("active")) ++Active;
		if (Active >= R.MaxMortgages) return No(FString::Printf(TEXT("you already have %d mortgages"), R.MaxMortgages));
		if (!Term.IsSet() || !R.MortgageTerms.Contains(*Term) || !Deposit.IsSet() || *Deposit * 10000 < Total * R.DepositBp || *Deposit > Total)
			return No(TEXT("the bank does not offer that mortgage"));
		Loan = Total - *Deposit;
		if (L().Balance(Vault) < Loan) return No(TEXT("the bank cannot lend that today"));
	}
	const int64 Need = Total + Tax - Loan;
	if (E.Funds() < Need) return No(TEXT("you cannot afford it"));
	FBrixMortgage M;
	if (Loan)
	{
		const TPair<int64, TArray<int64>> Tm = MortgageTerms(Loan, *Term);
		M.Id = Mortgages.Num() + 1; M.Day = E.Day; M.Principal = Loan; M.Total = Tm.Key; M.Plan = Tm.Value; M.Units = Which;
	}
	int64 LeftLoan = Loan;
	for (int32 n = 0; n < Which.Num(); ++n)
	{
		const int32 i = Which[n];
		const int64 p = Prices[n], t = Idiv(p * R.TransferTaxBp, 10000), FromBank = FMath::Min(LeftLoan, p);
		LeftLoan -= FromBank;
		const int64 Own = p + t - FromBank, a = FMath::Min(L().Balance(PCash), Own);
		TArray<TPair<FString, int64>> Lines;
		if (a) Lines.Add({ PCash, -a });
		if (Own > a) Lines.Add({ PCurrent, -(Own - a) });
		if (FromBank) Lines.Add({ Vault, -FromBank });
		const FBrixListing* Ls = Listing.Find(i);
		const FString Ag = Ls ? Ls->Agent : (Owner[i] == Hh || Owner[i] == Firm) ? Agent : FString();
		Transfer(i, Player, p, E.Day, Ag, TEXT("Bought: ") + Name(i), &Lines, E.Minute);
	}
	if (Loan) Mortgages.Add(M);
	if (OutPaid) *OutPaid = Total + Tax;
	return true;
}

bool FBrixEstate::SellNow(int32 i, const FString& Agent, int64* OutPrice)   // PROP-012
{
	if (!NeedOffice(Agent)) return false;
	if (!IsMine(i)) return No(TEXT("that is not yours"));
	const int64 Price = Bid(i), Tax = Idiv(Price * R.TransferTaxBp, 10000);
	FString Best;
	for (const auto& C : R.Companies)
	{
		const FString a = TEXT("co:") + C.Id;
		if (C.Focus.Contains(Units[i].Kind) && L().Balance(a) >= Price + Tax + C.Reserve)
			if (Best.IsEmpty() || L().Balance(a) > L().Balance(Best)) Best = a;
	}
	if (Best.IsEmpty()) return No(TEXT("no company will buy it today"));
	if (!Transfer(i, Best, Price, E.Day, FString(), TEXT("Sold: ") + Name(i), nullptr, E.Minute)) return false;
	if (OutPrice) *OutPrice = Price;
	return true;
}

bool FBrixEstate::ListForSale(int32 i, int64 Price, const FString& Agent)
{
	if (!NeedOffice(Agent)) return false;
	if (Agent.IsEmpty()) return No(TEXT("list it with an estate agent"));
	if (!IsMine(i)) return No(TEXT("that is not yours"));
	const int64 v = Value(i);
	if (!(Idiv(v * R.PlayerAskLoBp, 10000) <= Price && Price <= Idiv(v * R.PlayerAskHiBp, 10000))) return No(TEXT("the agent will not list it at that price"));
	Listing.Add(i, { Price, E.Day, Agent, Player });
	return true;
}

bool FBrixEstate::Unlist(int32 i)
{
	if (!IsMine(i) || !Listing.Contains(i)) return No(TEXT("it is not listed"));
	Listing.Remove(i);
	return true;
}

bool FBrixEstate::SetRent(int32 i, int64 NewRent, bool bToLet)   // PROP-014
{
	if (!IsMine(i)) return No(TEXT("that is not yours"));
	const int64 m = MarketRent(i);
	if (!(Idiv(m * R.RentLoBp, 10000) <= NewRent && NewRent <= Idiv(m * R.RentHiBp, 10000))) return No(TEXT("that rent is out of bounds"));
	Rent[i] = NewRent; ToLet[i] = bToLet;
	return true;
}

bool FBrixEstate::RentUnit(int32 i, const FString& Agent)    // PROP-013
{
	if (!NeedOffice(Agent)) return false;
	if (IsMine(i)) return No(TEXT("it is yours already"));
	if (!Tenant[i].IsEmpty()) return No(TEXT("it is let"));
	if (Tenancy.Num() >= R.MaxTenancies) return No(TEXT("you rent as many places as you may"));
	const int64 r = MarketRent(i);
	if (!Pay(E.Day, r, Acct(Owner[i]), TEXT("Rent, ") + Name(i))) return No(TEXT("you cannot afford the first day's rent"));
	Tenant[i] = Player;
	Tenancy.Add(i, { r, E.Day, 0 });
	return true;
}

bool FBrixEstate::Leave(int32 i)
{
	if (!Rents(i)) return No(TEXT("you do not rent it"));
	Tenant[i].Reset();
	Tenancy.Remove(i);
	return true;
}

bool FBrixEstate::Repay(int32 Id)
{
	FBrixMortgage* M = Mortgages.FindByPredicate([&](const FBrixMortgage& x) { return x.Id == Id && x.Status == TEXT("active"); });
	if (!M) return No(TEXT("no such mortgage"));
	const int64 Ow = M->Owed();
	if (!E.HasAccount(TEXT("current")) || L().Balance(PCurrent) < Ow) return No(TEXT("not enough in the current account"));
	const int64 Fee = M->Fees - M->FeesPaid;
	TArray<TPair<FString, int64>> Lines = { { PCurrent, -Ow } };
	if (Fee) Lines.Add({ TEXT("sys:fees"), Fee });
	Lines.Add({ Vault, Ow - Fee });
	L().Post(E.Day, E.Minute, FString::Printf(TEXT("Mortgage %d repaid early"), M->Id), Lines, Why);
	M->FeesPaid += Fee; M->Paid += Ow - Fee; M->Status = TEXT("repaid");
	return true;
}

// ---- saving (estate.js save() and load())
namespace
{
	TSharedRef<FJsonValue> NumV(int64 V) { return MakeShared<FJsonValueNumber>((double)V); }
	TSharedRef<FJsonValue> StrOrNull(const FString& V) { return V.IsEmpty() ? StaticCastSharedRef<FJsonValue>(MakeShared<FJsonValueNull>()) : StaticCastSharedRef<FJsonValue>(MakeShared<FJsonValueString>(V)); }
	TArray<TSharedPtr<FJsonValue>> NumArr(const TArray<int64>& A) { TArray<TSharedPtr<FJsonValue>> Out; for (const int64 V : A) Out.Add(NumV(V)); return Out; }
	TArray<TSharedPtr<FJsonValue>> IntArr(const TArray<int32>& A) { TArray<TSharedPtr<FJsonValue>> Out; for (const int32 V : A) Out.Add(NumV(V)); return Out; }
	TArray<TSharedPtr<FJsonValue>> StrArr(const TArray<FString>& A) { TArray<TSharedPtr<FJsonValue>> Out; for (const FString& V : A) Out.Add(MakeShared<FJsonValueString>(V)); return Out; }
	FString OptStr(const TSharedPtr<FJsonValue>& V) { return V.IsValid() && V->Type == EJson::String ? V->AsString() : FString(); }
}

TSharedPtr<FJsonObject> FBrixEstate::Save() const
{
	TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Ids, Ow, Te, Rt, Tl, Mb;
	for (int32 i = 0; i < Units.Num(); ++i)
	{
		Ids.Add(MakeShared<FJsonValueString>(Units[i].Id));
		Ow.Add(MakeShared<FJsonValueString>(Owner[i]));
		Te.Add(StrOrNull(Tenant[i]));
		Rt.Add(NumV(Rent[i]));
		Tl.Add(MakeShared<FJsonValueBoolean>(ToLet[i]));
		Mb.Add(MakeShared<FJsonValueArray>(StrArr(Members[i])));
	}
	O->SetArrayField(TEXT("ids"), Ids); O->SetBoolField(TEXT("started"), bStarted);
	O->SetArrayField(TEXT("owner"), Ow); O->SetArrayField(TEXT("tenant"), Te); O->SetArrayField(TEXT("rent"), Rt); O->SetArrayField(TEXT("to_let"), Tl);
	O->SetArrayField(TEXT("members"), Mb);
	TSharedPtr<FJsonObject> Tn = MakeShared<FJsonObject>(), Ls = MakeShared<FJsonObject>(), Ms = MakeShared<FJsonObject>(), Od = MakeShared<FJsonObject>();
	for (const auto& T : Tenancy) { TSharedPtr<FJsonObject> X = MakeShared<FJsonObject>(); X->SetNumberField(TEXT("rent"), (double)T.Value.Rent); X->SetNumberField(TEXT("since"), T.Value.Since); X->SetNumberField(TEXT("missed"), T.Value.Missed); Tn->SetObjectField(S(T.Key), X); }
	for (const auto& T : Listing)
	{
		TSharedPtr<FJsonObject> X = MakeShared<FJsonObject>();
		X->SetNumberField(TEXT("price"), (double)T.Value.Price); X->SetNumberField(TEXT("since"), T.Value.Since);
		X->SetField(TEXT("agent"), StrOrNull(T.Value.Agent)); X->SetStringField(TEXT("by"), T.Value.By);
		Ls->SetObjectField(S(T.Key), X);
	}
	for (const auto& T : Missed) Ms->SetNumberField(S(T.Key), T.Value);
	for (const auto& T : Owed) Od->SetNumberField(S(T.Key), (double)T.Value);
	O->SetObjectField(TEXT("tenancy"), Tn); O->SetObjectField(TEXT("listing"), Ls); O->SetObjectField(TEXT("missed"), Ms); O->SetObjectField(TEXT("owed"), Od);
	TArray<TSharedPtr<FJsonValue>> Mo;
	for (const FBrixMortgage& M : Mortgages)
	{
		TSharedPtr<FJsonObject> X = MakeShared<FJsonObject>();
		X->SetNumberField(TEXT("id"), M.Id); X->SetNumberField(TEXT("day"), M.Day); X->SetNumberField(TEXT("principal"), (double)M.Principal);
		X->SetNumberField(TEXT("total"), (double)M.Total); X->SetArrayField(TEXT("plan"), NumArr(M.Plan)); X->SetNumberField(TEXT("paid"), (double)M.Paid);
		X->SetNumberField(TEXT("fees"), (double)M.Fees); X->SetNumberField(TEXT("fees_paid"), (double)M.FeesPaid); X->SetNumberField(TEXT("missed"), M.Missed);
		X->SetStringField(TEXT("status"), M.Status); X->SetArrayField(TEXT("units"), IntArr(M.Units));
		Mo.Add(MakeShared<FJsonValueObject>(X));
	}
	O->SetArrayField(TEXT("mortgages"), Mo);
	TArray<TSharedPtr<FJsonValue>> Bu, Hi, Sa, Mv;
	for (const auto& B : Buys) Bu.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ NumV(B.Key), MakeShared<FJsonValueString>(B.Value) }));
	for (const auto& Hr : History) Hi.Add(MakeShared<FJsonValueArray>(NumArr(Hr)));
	for (const FBrixSale& Sl : Sales) Sa.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ NumV(Sl.Day), NumV(Sl.Unit), NumV(Sl.Price), MakeShared<FJsonValueString>(Sl.Seller), MakeShared<FJsonValueString>(Sl.Buyer) }));
	for (const FIntVector& M : Moves) Mv.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ NumV(M.X), NumV(M.Y), NumV(M.Z) }));
	O->SetArrayField(TEXT("buys"), Bu); O->SetArrayField(TEXT("history"), Hi); O->SetArrayField(TEXT("sales"), Sa); O->SetArrayField(TEXT("moves"), Mv);
	TSharedPtr<FJsonObject> Ix = MakeShared<FJsonObject>(), Sh = MakeShared<FJsonObject>();
	for (const auto& I : Idx) Ix->SetNumberField(I.Key, (double)I.Value);
	for (const FString& Sx : Short) Sh->SetBoolField(Sx, true);
	O->SetObjectField(TEXT("idx"), Ix); O->SetObjectField(TEXT("short"), Sh);
	O->SetNumberField(TEXT("income"), (double)Income);
	return O;
}

void FBrixEstate::Load(const TSharedPtr<FJsonObject>& S0)
{
	if (!S0.IsValid()) return;
	// units matched by id; a save from before ids were kept is matched by position; units gone are dropped, new ones keep
	// how the town starts them (estate.js load())
	const TArray<TSharedPtr<FJsonValue>>* IdsP = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* OwnP = nullptr;
	S0->TryGetArrayField(TEXT("owner"), OwnP);
	TArray<int32> Map;
	if (S0->TryGetArrayField(TEXT("ids"), IdsP)) for (const auto& V : *IdsP) Map.Add(ById.FindRef(V->AsString(), -1));
	else if (OwnP) for (int32 o = 0; o < OwnP->Num(); ++o) Map.Add(o < Units.Num() ? o : -1);
	auto At = [&](int32 o) { return Map.IsValidIndex(o) ? Map[o] : -1; };
	for (int32 i = 0; i < Units.Num(); ++i) { Owner[i] = FirstOwner(i); Tenant[i] = Units[i].Tenant0; Rent[i] = 0; ToLet[i] = false; Members[i] = Units[i].Members; }
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (OwnP) for (int32 o = 0; o < OwnP->Num(); ++o) if (At(o) >= 0) Owner[At(o)] = (*OwnP)[o]->AsString();
	if (S0->TryGetArrayField(TEXT("tenant"), A)) for (int32 o = 0; o < A->Num(); ++o) if (At(o) >= 0) Tenant[At(o)] = OptStr((*A)[o]);
	if (S0->TryGetArrayField(TEXT("rent"), A)) for (int32 o = 0; o < A->Num(); ++o) if (At(o) >= 0) Rent[At(o)] = (int64)(*A)[o]->AsNumber();
	if (S0->TryGetArrayField(TEXT("to_let"), A)) for (int32 o = 0; o < A->Num(); ++o) if (At(o) >= 0) ToLet[At(o)] = (*A)[o]->AsBool();
	if (S0->TryGetArrayField(TEXT("members"), A))
		for (int32 o = 0; o < A->Num(); ++o) if (At(o) >= 0) { TArray<FString> Ms; for (const auto& V : (*A)[o]->AsArray()) Ms.Add(V->AsString()); Members[At(o)] = Ms; }
	S0->TryGetBoolField(TEXT("started"), bStarted);
	Tenancy.Reset(); Listing.Reset(); Missed.Reset(); Owed.Reset();
	for (const auto& V : Obj(S0, TEXT("tenancy"))->Values)
		if (const int32 i = At(FCString::Atoi(*FString(V.Key.ToView()))); i >= 0)
		{ const TSharedPtr<FJsonObject> X = V.Value->AsObject(); Tenancy.Add(i, { Int(X, TEXT("rent")), (int32)Int(X, TEXT("since")), (int32)Int(X, TEXT("missed")) }); }
	for (const auto& V : Obj(S0, TEXT("listing"))->Values)
		if (const int32 i = At(FCString::Atoi(*FString(V.Key.ToView()))); i >= 0)
		{
			const TSharedPtr<FJsonObject> X = V.Value->AsObject();
			FBrixListing Ls{ Int(X, TEXT("price")), (int32)Int(X, TEXT("since")), OptStr(X->TryGetField(TEXT("agent"))), FString() };
			X->TryGetStringField(TEXT("by"), Ls.By);
			Listing.Add(i, Ls);
		}
	for (const auto& V : Obj(S0, TEXT("missed"))->Values) if (const int32 i = At(FCString::Atoi(*FString(V.Key.ToView()))); i >= 0) Missed.Add(i, (int32)V.Value->AsNumber());
	for (const auto& V : Obj(S0, TEXT("owed"))->Values) if (const int32 i = At(FCString::Atoi(*FString(V.Key.ToView()))); i >= 0) Owed.Add(i, (int64)V.Value->AsNumber());
	Mortgages.Reset();
	if (S0->TryGetArrayField(TEXT("mortgages"), A))
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> X = V->AsObject();
			FBrixMortgage& M = Mortgages.AddDefaulted_GetRef();
			M.Id = (int32)Int(X, TEXT("id")); M.Day = (int32)Int(X, TEXT("day")); M.Principal = Int(X, TEXT("principal")); M.Total = Int(X, TEXT("total"));
			for (const auto& P : X->GetArrayField(TEXT("plan"))) M.Plan.Add((int64)P->AsNumber());
			M.Paid = Int(X, TEXT("paid")); M.Fees = Int(X, TEXT("fees")); M.FeesPaid = Int(X, TEXT("fees_paid")); M.Missed = (int32)Int(X, TEXT("missed"));
			X->TryGetStringField(TEXT("status"), M.Status);
			for (const auto& U : X->GetArrayField(TEXT("units"))) if (At((int32)U->AsNumber()) >= 0) M.Units.Add(At((int32)U->AsNumber()));
		}
	Buys.Reset(); History.Reset(); Sales.Reset(); Moves.Reset();
	if (S0->TryGetArrayField(TEXT("buys"), A)) for (const auto& V : *A) Buys.Add({ (int32)V->AsArray()[0]->AsNumber(), V->AsArray()[1]->AsString() });
	if (S0->TryGetArrayField(TEXT("history"), A)) for (const auto& V : *A) { TArray<int64> Hr; for (const auto& x : V->AsArray()) Hr.Add((int64)x->AsNumber()); History.Add(Hr); }
	if (S0->TryGetArrayField(TEXT("sales"), A))
		for (const auto& V : *A)
		{
			const TArray<TSharedPtr<FJsonValue>>& X = V->AsArray();
			if (X.Num() >= 5 && At((int32)X[1]->AsNumber()) >= 0) Sales.Add({ (int32)X[0]->AsNumber(), At((int32)X[1]->AsNumber()), (int64)X[2]->AsNumber(), X[3]->AsString(), X[4]->AsString() });
		}
	if (S0->TryGetArrayField(TEXT("moves"), A))
		for (const auto& V : *A)
		{
			const TArray<TSharedPtr<FJsonValue>>& X = V->AsArray();
			const int32 a = At((int32)X[1]->AsNumber()), b = At((int32)X[2]->AsNumber());
			if (a >= 0 && b >= 0) Moves.Add(FIntVector((int32)X[0]->AsNumber(), a, b));
		}
	for (const auto& V : Obj(S0, TEXT("idx"))->Values) Idx.Add(FString(V.Key.ToView()), (int64)V.Value->AsNumber());
	Short.Reset();
	for (const auto& V : Obj(S0, TEXT("short"))->Values) Short.Add(FString(V.Key.ToView()));
	Income = Int(S0, TEXT("income"));
	Homes();
}
