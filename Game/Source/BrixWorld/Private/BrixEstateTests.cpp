// PROP-009 to PROP-021, BANK-002: the real estate market in the Unreal build follows Tools/citysim/estate.py.
// Game/Data/life.json carries estate.check_script() and what Python gives for it (build_play.py estate_checks, the same
// fixed run the slice's estate.js checks itself against); here the C++ runs it on the same data and must give the same
// ledger, owners, tenants, block indices, history, mortgages, listings, moves and furnishing levels.
#include "BrixCity.h"
#include "BrixEconomy.h"
#include "BrixEstate.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FEstateLife
	{
		FBrixCity City;
		FBrixLifeData Data;
		TArray<FBrixEconLot> Lots;
		FBrixEstateRules Rules;
		TArray<FBrixUnit> Units;
		TMap<FString, FString> Roles;
	};
	bool LoadEstateLife(FAutomationTestBase& T, FEstateLife& Out)
	{
		FString Error;
		const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/city_01")));
		const FString Life = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/life.json")));
		if (!T.TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(Dir, Out.City, Error))) return false;
		if (!T.TestTrue(TEXT("life.json loads: ") + Error, LoadBrixLife(Life, Out.Data, Error))) return false;
		if (!T.TestTrue(TEXT("the lots are in life.json's order: ") + Error, BrixEconLots(Out.City, Out.Data, Out.Lots, Error))) return false;
		return T.TestTrue(TEXT("the estate loads: ") + Error, LoadBrixEstate(Out.Data, Out.Rules, Out.Units, Out.Roles, Error));
	}
	int64 Num(const TSharedPtr<FJsonValue>& V) { return (int64)V->AsNumber(); }
	FString Text(const TSharedPtr<FJsonValue>& V) { return V.IsValid() && V->Type == EJson::String ? V->AsString() : FString(); }

	// estate.run_script(): each op ok or refused, then a summary
	int32 Cheapest(const FBrixEstate& M, TFunctionRef<bool(int32)> Ok, TFunctionRef<int64(int32)> Key)
	{
		int32 Best = -1;
		for (int32 j = 0; j < M.Units.Num(); ++j) if (Ok(j) && (Best < 0 || Key(j) < Key(Best))) Best = j;
		return Best;
	}
	TArray<FString> RunScript(FBrixEconomy& E, FBrixEstate& M, const TArray<TSharedPtr<FJsonValue>>& Ops, TArray<int32>& Mine, FAutomationTestBase& T)
	{
		TArray<FString> Results;
		for (const TSharedPtr<FJsonValue>& OpV : Ops)
		{
			const TArray<TSharedPtr<FJsonValue>>& Op = OpV->AsArray();
			const FString Name = Op[0]->AsString();
			bool bOk = true;
			if (Name == TEXT("advance")) E.Advance((int32)Num(Op[1]), Op[2]->AsNumber());
			else if (Name == TEXT("new")) E.NewCharacter();
			else if (Name == TEXT("start")) M.Start();
			else if (Name == TEXT("open")) bOk = E.OpenAccount(Op[1]->AsString());
			else if (Name == TEXT("give")) bOk = E.L.Post(E.Day, E.Minute, TEXT("Test money"), { { TEXT("sys:start"), -Num(Op[1]) }, { TEXT("p:current"), Num(Op[1]) } }, E.Why);
			else if (Name == TEXT("buy_cheapest"))
			{
				const FString Kind = Op[1]->AsString(), Agent = Text(Op[2]);
				const int64 DepPc = Num(Op[3]);
				const int32 Term = (int32)Num(Op[4]);
				const int32 i = Cheapest(M, [&](int32 j) { return M.Units[j].Kind == Kind && M.Ask(j).IsSet(); }, [&](int32 j) { return *M.Ask(j); });
				if (i < 0) bOk = false;
				else
				{
					bOk = DepPc ? M.Buy({ i }, Agent, BrixMoney::FloorDiv(*M.Ask(i) * DepPc, 100), Term) : M.Buy({ i }, Agent);
					if (bOk) Mine.Add(i);
				}
			}
			else if (Name == TEXT("rent_cheapest"))
			{
				const FString Kind = Op[1]->AsString();
				const int32 i = Cheapest(M, [&](int32 j) { return M.Units[j].Kind == Kind && M.Tenant[j].IsEmpty() && !M.IsMine(j); }, [&](int32 j) { return M.MarketRent(j); });
				bOk = i >= 0 && M.RentUnit(i);
			}
			else if (Name == TEXT("setrent")) { const int32 k = (int32)Num(Op[1]); bOk = Mine.IsValidIndex(k) && M.SetRent(Mine[k], BrixMoney::FloorDiv(M.MarketRent(Mine[k]) * Num(Op[2]), 10000)); }
			else if (Name == TEXT("list")) { const int32 k = (int32)Num(Op[1]); bOk = Mine.IsValidIndex(k) && M.ListForSale(Mine[k], BrixMoney::FloorDiv(M.Value(Mine[k]) * Num(Op[2]), 10000), Op[3]->AsString()); }
			else if (Name == TEXT("sell_now")) { const int32 k = (int32)Num(Op[1]); bOk = Mine.IsValidIndex(k) && M.SellNow(Mine[k]); }
			else T.AddError(TEXT("an op the port does not know: ") + Name);
			Results.Add(bOk ? TEXT("ok") : TEXT("refused"));
		}
		return Results;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPROP021_MarketMatchesPython, "BrixTown.World.PROP021_the_market_matches_the_python",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FPROP021_MarketMatchesPython::RunTest(const FString&)
{
	FEstateLife Life;
	if (!LoadEstateLife(*this, Life)) return false;
	FBrixEconomy E(Life.Data, Life.Lots);
	FBrixEstate M(Life.Rules, E, Life.Units, Life.Roles);
	const TSharedPtr<FJsonObject> C = Life.Data.Json->GetObjectField(TEXT("estate_checks")), Want = C->GetObjectField(TEXT("want"));
	TArray<int32> Mine;
	const TArray<FString> Results = RunScript(E, M, C->GetArrayField(TEXT("script")), Mine, *this);
	const TArray<TSharedPtr<FJsonValue>>& WR = Want->GetArrayField(TEXT("results"));
	for (int32 k = 0; k < FMath::Max(WR.Num(), Results.Num()); ++k)
	{
		const FString W = WR.IsValidIndex(k) ? WR[k]->AsString() : TEXT("-"), G = Results.IsValidIndex(k) ? Results[k] : TEXT("-");
		if (W != G) AddError(FString::Printf(TEXT("op %d gives %s, Python %s (%s / %s)"), k, *G, *W, *M.Why, *E.Why));
	}
	TestEqual(TEXT("units (PROP-009)"), (int64)M.Units.Num(), (int64)Want->GetNumberField(TEXT("units")));
	for (const auto& B : Want->GetObjectField(TEXT("balances"))->Values)
		TestEqual(TEXT("the balance of ") + FString(B.Key.ToView()), E.L.Balance(FString(B.Key.ToView())), Num(B.Value));
	TestEqual(TEXT("transactions"), (int64)E.L.Tx.Num(), (int64)Want->GetNumberField(TEXT("tx")));
	TestEqual(TEXT("money in the town (ECO-003, PROP-018)"), E.L.Supply(), (int64)Want->GetNumberField(TEXT("supply")));
	TestEqual(TEXT("the system accounts hold the opposite of the supply"), E.L.SystemTotal(), -E.L.Supply());
	const TArray<TSharedPtr<FJsonValue>>& WO = Want->GetArrayField(TEXT("owner")), & WT = Want->GetArrayField(TEXT("tenant"));
	int32 BadO = 0, BadT = 0;
	for (int32 i = 0; i < WO.Num() && i < M.Units.Num(); ++i)
	{
		if (M.Owner[i] != Text(WO[i]) && BadO++ < 5) AddError(FString::Printf(TEXT("unit %d (%s) is owned by %s, Python %s"), i, *M.Units[i].Id, *M.Owner[i], *Text(WO[i])));
		if (M.Tenant[i] != Text(WT[i]) && BadT++ < 5) AddError(FString::Printf(TEXT("unit %d (%s) is let to '%s', Python '%s'"), i, *M.Units[i].Id, *M.Tenant[i], *Text(WT[i])));
	}
	TestEqual(TEXT("owners that differ (PROP-010)"), BadO, 0);
	TestEqual(TEXT("tenants that differ"), BadT, 0);
	for (const auto& I : Want->GetObjectField(TEXT("idx"))->Values)
		TestEqual(TEXT("the price index of ") + FString(I.Key.ToView()) + TEXT(" (PROP-016)"), M.Idx.FindRef(FString(I.Key.ToView())), Num(I.Value));
	const TArray<TSharedPtr<FJsonValue>>& WH = Want->GetArrayField(TEXT("history"));
	if (TestEqual(TEXT("days in the history"), M.History.Num(), WH.Num()))
		for (int32 k = 0; k < WH.Num(); ++k)
			for (int32 q = 0; q < 6; ++q)
				TestEqual(FString::Printf(TEXT("history day %d, figure %d"), k, q), M.History[k][q], Num(WH[k]->AsArray()[q]));
	const TArray<TSharedPtr<FJsonValue>>& WM = Want->GetArrayField(TEXT("mortgages"));
	if (TestEqual(TEXT("mortgages (PROP-019)"), M.Mortgages.Num(), WM.Num()))
		for (int32 k = 0; k < WM.Num(); ++k)
		{
			const TArray<TSharedPtr<FJsonValue>>& W = WM[k]->AsArray();
			TestEqual(FString::Printf(TEXT("mortgage %d's status"), k + 1), M.Mortgages[k].Status, W[0]->AsString());
			TestEqual(FString::Printf(TEXT("mortgage %d paid"), k + 1), M.Mortgages[k].Paid, Num(W[1]));
			TestEqual(FString::Printf(TEXT("mortgage %d's fees"), k + 1), M.Mortgages[k].Fees, Num(W[2]));
			TestEqual(FString::Printf(TEXT("mortgage %d missed"), k + 1), (int64)M.Mortgages[k].Missed, Num(W[3]));
		}
	const TSharedPtr<FJsonObject> WL = Want->GetObjectField(TEXT("listing"));
	TestEqual(TEXT("listings (PROP-012)"), M.Listing.Num(), WL->Values.Num());
	for (const auto& Ls : WL->Values)
	{
		const FBrixListing* Got = M.Listing.Find(FCString::Atoi(*FString(Ls.Key.ToView())));
		if (TestNotNull(TEXT("listed: unit ") + FString(Ls.Key.ToView()), Got)) TestEqual(TEXT("the asking price of unit ") + FString(Ls.Key.ToView()), Got->Price, Num(Ls.Value));
	}
	const TArray<TSharedPtr<FJsonValue>>& WMine = Want->GetArrayField(TEXT("mine"));
	if (TestEqual(TEXT("units the player bought"), Mine.Num(), WMine.Num()))
		for (int32 k = 0; k < Mine.Num(); ++k) TestEqual(FString::Printf(TEXT("bought unit %d"), k), (int64)Mine[k], Num(WMine[k]));
	const TArray<TSharedPtr<FJsonValue>>& WMv = Want->GetArrayField(TEXT("moves"));
	if (TestEqual(TEXT("households that moved (PROP-022)"), M.Moves.Num(), WMv.Num()))
		for (int32 k = 0; k < WMv.Num(); ++k)
		{
			const TArray<TSharedPtr<FJsonValue>>& W = WMv[k]->AsArray();
			TestEqual(FString::Printf(TEXT("move %d"), k), FString::Printf(TEXT("%d %d %d"), M.Moves[k].X, M.Moves[k].Y, M.Moves[k].Z),
				FString::Printf(TEXT("%lld %lld %lld"), Num(W[0]), Num(W[1]), Num(W[2])));
		}
	int64 Beds = 0;
	for (const FBrixUnit& U : M.Units) Beds += U.Beds;
	TestEqual(TEXT("beds"), Beds, (int64)Want->GetNumberField(TEXT("beds")));
	const TArray<TSharedPtr<FJsonValue>>& WLv = Want->GetArrayField(TEXT("levels"));
	for (int32 k = 1; k <= 4 && k <= WLv.Num(); ++k)
	{
		int64 n = 0;
		for (int32 i = 0; i < M.Units.Num(); ++i) if (M.Tenant[i] == TEXT("hh") && M.LevelOf(i) == k) ++n;
		TestEqual(FString::Printf(TEXT("homes furnished at level %d (FURN-063)"), k), n, Num(WLv[k - 1]));
	}
	for (const FBrixTx& Tx : E.L.Tx)
	{
		int64 Sum = 0;
		for (const auto& Ln : Tx.Lines) Sum += Ln.Value;
		if (Sum != 0) AddError(TEXT("a transaction that does not balance: ") + Tx.Memo);
	}
	return true;
}

// PROP-009, PROP-010: every building that is not public land is carved into units with stable ids and no floor in two;
// every unit has an owner, more than one company holds the town and none holds half of it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPROP009_EveryBuildingIsCarvedAndOwned, "BrixTown.World.PROP009_every_building_is_carved_into_owned_units",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FPROP009_EveryBuildingIsCarvedAndOwned::RunTest(const FString&)
{
	FEstateLife Life;
	if (!LoadEstateLife(*this, Life)) return false;
	FBrixEconomy E(Life.Data, Life.Lots);
	FBrixEstate M(Life.Rules, E, Life.Units, Life.Roles);
	TSet<FString> Ids;
	TMap<FString, int32> Kinds, Cos;
	for (int32 i = 0; i < M.Units.Num(); ++i)
	{
		const FBrixUnit& U = M.Units[i];
		TestFalse(TEXT("a unit id is used once: ") + U.Id, Ids.Contains(U.Id));
		Ids.Add(U.Id);
		TestTrue(TEXT("a unit's id is its lot's: ") + U.Id, U.Id.StartsWith(U.LotId + TEXT(":u")));
		TestEqual(TEXT("a unit's lot is in the town: ") + U.Id, Life.Lots.IsValidIndex(U.Lot) ? Life.Lots[U.Lot].Id : FString(), U.LotId);
		TestTrue(TEXT("a unit has floor area: ") + U.Id, U.Area > 0);
		Kinds.FindOrAdd(U.Kind) += 1;
		if (M.Owner[i].StartsWith(TEXT("co:"))) Cos.FindOrAdd(M.Owner[i]) += 1;
		if (M.Owner[i] == TEXT("hh")) TestEqual(TEXT("a household owns only the home it lives in: ") + U.Id, M.Tenant[i], FString(TEXT("hh")));
	}
	for (const auto& L : M.ByLot)
	{
		TSet<int32> Fl;
		for (const int32 i : L.Value) for (const int32 f : M.Units[i].Floors) { TestFalse(TEXT("no floor in two units: ") + M.Units[i].Id, Fl.Contains(f)); Fl.Add(f); }
	}
	for (const TCHAR* K : { TEXT("house"), TEXT("flat"), TEXT("shop"), TEXT("office"), TEXT("plant"), TEXT("civic") })
		TestTrue(FString(TEXT("there are units of kind ")) + K, Kinds.FindRef(K) > 0);
	for (const FBrixEconLot& L : Life.Lots)
		if (L.Kind == TEXT("garden") || L.Kind == TEXT("stop") || L.Kind == TEXT("market"))
			TestFalse(TEXT("public land is not sold: ") + L.Id, M.ByLot.Contains(Life.Lots.IndexOfByPredicate([&](const FBrixEconLot& x) { return x.Id == L.Id; })));
	TestTrue(TEXT("more than one company holds the town"), Cos.Num() >= 2);
	for (const auto& C : Cos) TestTrue(TEXT("no company holds half the town: ") + C.Key, C.Value < M.Units.Num() / 2);
	for (const auto& O : Life.Rules.Companies) TestTrue(TEXT("a company's office is a lot of the town: ") + O.Office, Life.Lots.ContainsByPredicate([&](const FBrixEconLot& x) { return x.Id == O.Office; }));
	return true;
}

// PROP-018: with no player the money supply holds steady over a game year; balances stay at or above zero; the indices stay
// in their bounds. PROP-021: the market saved and loaded goes on exactly as the one that was not.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPROP018_SupplyHoldsAndASaveGoesOn, "BrixTown.World.PROP018_the_money_supply_holds_and_a_saved_market_goes_on",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FPROP018_SupplyHoldsAndASaveGoesOn::RunTest(const FString&)
{
	FEstateLife Life;
	if (!LoadEstateLife(*this, Life)) return false;
	FBrixEconomy E(Life.Data, Life.Lots);
	FBrixEstate M(Life.Rules, E, Life.Units, Life.Roles);
	E.Advance(0, 440);
	E.NewCharacter();
	M.Start();
	E.Advance(20, 600);
	// the player buys a flat, rents it out, and the game is saved and read into a fresh economy and market
	E.OpenAccount(TEXT("current"));
	E.L.Post(E.Day, E.Minute, TEXT("Test money"), { { TEXT("sys:start"), -900000 }, { TEXT("p:current"), 900000 } }, E.Why);
	int32 Flat = -1;
	for (int32 i = 0; i < M.Units.Num(); ++i) if (M.Units[i].Kind == TEXT("flat") && M.Ask(i) && (Flat < 0 || *M.Ask(i) < *M.Ask(Flat))) Flat = i;
	TestTrue(TEXT("the cheapest flat is bought (PROP-011)"), Flat >= 0 && M.Buy({ Flat }, TEXT("lark")));
	TestTrue(TEXT("it is the player's"), M.IsMine(Flat));
	TestTrue(TEXT("and on their list of lots"), M.MyLots().Contains(M.Units[Flat].Lot));
	FString Text;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(M.Save().ToSharedRef(), W);
	TSharedPtr<FJsonObject> Back;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Back);
	FBrixEconomy E2(Life.Data, Life.Lots);
	FBrixEstate M2(Life.Rules, E2, Life.Units, Life.Roles);
	// the economy is brought back as BrixSave does: its own ledger and state
	E2.L = E.L; E2.Day = E.Day; E2.Minute = E.Minute; E2.Settled = E.Settled; E2.Accounts = E.Accounts; E2.bWelcomed = E.bWelcomed;
	M2.Load(Back);
	TestTrue(TEXT("the flat is still the player's after loading"), M2.IsMine(Flat));
	E.Advance(40, 600);
	E2.Advance(40, 600);
	TestEqual(TEXT("both markets go on alike: the supply"), E2.L.Supply(), E.L.Supply());
	TestEqual(TEXT("both markets go on alike: transactions"), E2.L.Tx.Num(), E.L.Tx.Num());
	TestEqual(TEXT("both markets go on alike: the player's current account"), E2.L.Balance(TEXT("p:current")), E.L.Balance(TEXT("p:current")));
	TestEqual(TEXT("both markets go on alike: the town index"), M2.TownIndex(), M.TownIndex());
	int32 Differ = 0;
	for (int32 i = 0; i < M.Units.Num(); ++i) if (M.Owner[i] != M2.Owner[i] || M.Tenant[i] != M2.Tenant[i]) ++Differ;
	TestEqual(TEXT("both markets go on alike: owners and tenants"), Differ, 0);
	// with no player: a game year (estate.json balance: index_days, supply_drift_bp), as test_estate.py's test_PROP_018
	FBrixEconomy E3(Life.Data, Life.Lots);
	FBrixEstate M3(Life.Rules, E3, Life.Units, Life.Roles);
	E3.Advance(0, 440);
	M3.Start();
	const TSharedPtr<FJsonObject> Bal = Life.Data.Json->GetObjectField(TEXT("estate"))->GetObjectField(TEXT("balance"));
	const int32 Days = (int32)Bal->GetNumberField(TEXT("index_days"));
	E3.Advance(60, 0);
	const int64 A3 = E3.L.Supply();
	E3.Advance(Days, 0);
	const int64 Z3 = E3.L.Supply();
	TestTrue(FString::Printf(TEXT("the supply holds over a year with no player (%lld to %lld)"), A3, Z3), FMath::Abs(Z3 - A3) * 10000 / A3 < (int64)Bal->GetNumberField(TEXT("supply_drift_bp")));
	TestTrue(TEXT("the town's households hold money"), E3.L.Balance(FBrixEstate::Town) > 0);
	for (const auto& C : Life.Rules.Companies) TestTrue(TEXT("a company is not below zero: ") + C.Id, E3.L.Balance(TEXT("co:") + C.Id) >= 0);
	for (const auto& Hr : M3.History) TestTrue(TEXT("the town index stays in its bounds"), Hr[1] >= Life.Rules.IdxMinBp && Hr[1] <= Life.Rules.IdxMaxBp);
	TestEqual(TEXT("the system accounts hold the opposite of the supply"), E3.L.SystemTotal(), -E3.L.Supply());
	return true;
}

#endif
