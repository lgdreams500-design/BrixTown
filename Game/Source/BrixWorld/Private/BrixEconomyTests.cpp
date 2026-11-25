// ECO-001, ECO-002, ECO-003, ECO-011, ECO-012, BANK-001, BANK-002, BANK-007, BANK-008: money, the bank and the shops in
// the Unreal build follow Tools/citysim/economy.py. Game/Data/life.json carries two fixed runs of the rules and what
// Python gives for them (build_play.py: economy.check_script() with outlet_script(), and bank_script()); here the C++
// runs them on the same data and must give the same ledger, balances, things carried, loans, prices and outlet stock.
#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixEconomy.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FLife
	{
		FBrixCity City;
		FBrixLifeData Data;
		TArray<FBrixEconLot> Lots;
	};
	bool LoadLife(FAutomationTestBase& T, FLife& Out)
	{
		FString Error;
		const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/city_01")));
		const FString Life = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/life.json")));
		if (!T.TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(Dir, Out.City, Error))) return false;
		if (!T.TestTrue(TEXT("life.json loads: ") + Error, LoadBrixLife(Life, Out.Data, Error))) return false;
		return T.TestTrue(TEXT("the lots are in life.json's order: ") + Error, BrixEconLots(Out.City, Out.Data, Out.Lots, Error));
	}
	FString Str(const TSharedPtr<FJsonValue>& V) { return V->Type == EJson::String ? V->AsString() : FString::Printf(TEXT("%lld"), (int64)V->AsNumber()); }
	int64 Num(const TSharedPtr<FJsonValue>& V) { return (int64)V->AsNumber(); }

	// economy.run_script(): each op ok or refused, then a summary of where everything stands
	void RunAndCompare(FAutomationTestBase& T, const FLife& Life, const TSharedPtr<FJsonObject>& Checks, const FString& Label)
	{
		FBrixEconomy E(Life.Data, Life.Lots);
		const TSharedPtr<FJsonObject> Shops = Checks->GetObjectField(TEXT("shops")), Want = Checks->GetObjectField(TEXT("want"));
		auto Shop = [&](const TSharedPtr<FJsonValue>& V) { return (int32)Shops->GetNumberField(V->AsString()); };
		TArray<FString> Results;
		for (const TSharedPtr<FJsonValue>& OpV : Checks->GetArrayField(TEXT("script")))
		{
			const TArray<TSharedPtr<FJsonValue>>& Op = OpV->AsArray();
			const FString Name = Op[0]->AsString();
			bool bOk = true;
			if (Name == TEXT("new")) E.NewCharacter();
			else if (Name == TEXT("buy")) bOk = E.Buy(Shop(Op[1]), Op[2]->AsString());
			else if (Name == TEXT("sell")) bOk = E.Sell(Shop(Op[1]), Op[2]->AsString());
			else if (Name == TEXT("open")) bOk = E.OpenAccount(Op[1]->AsString());
			else if (Name == TEXT("move")) bOk = E.Move(Op[1]->AsString(), Op[2]->AsString(), Num(Op[3]));
			else if (Name == TEXT("advance")) E.Advance((int32)Num(Op[1]), Op[2]->AsNumber());
			else if (Name == TEXT("loan")) bOk = E.TakeLoan(Num(Op[1]), (int32)Num(Op[2]));
			else if (Name == TEXT("rent")) bOk = E.RentStall();
			else if (Name == TEXT("setout")) bOk = E.SetOut(Op[1]->AsString(), Num(Op[2]));
			else if (Name == TEXT("buyat")) bOk = E.BuyAt((int32)Num(Op[1]), Op[2]->AsString(), Op[3]->AsString());
			else T.AddError(Label + TEXT(": an op the port does not know: ") + Name);
			Results.Add(bOk ? TEXT("ok") : TEXT("refused"));
		}
		const TArray<TSharedPtr<FJsonValue>>& WR = Want->GetArrayField(TEXT("results"));
		for (int32 k = 0; k < FMath::Max(WR.Num(), Results.Num()); ++k)
		{
			const FString W = WR.IsValidIndex(k) ? WR[k]->AsString() : TEXT("-"), G = Results.IsValidIndex(k) ? Results[k] : TEXT("-");
			if (W != G) T.AddError(FString::Printf(TEXT("%s: op %d gives %s, Python %s (%s)"), *Label, k, *G, *W, *E.Why));
		}
		for (const auto& B : Want->GetObjectField(TEXT("balances"))->Values)
			T.TestEqual(Label + TEXT(": the balance of ") + FString(B.Key.ToView()), E.L.Balance(FString(B.Key.ToView())), Num(B.Value));
		T.TestEqual(Label + TEXT(": transactions"), (int64)E.L.Tx.Num(), (int64)Want->GetNumberField(TEXT("tx")));
		T.TestEqual(Label + TEXT(": money in the town (ECO-003)"), E.L.Supply(), (int64)Want->GetNumberField(TEXT("supply")));
		T.TestEqual(Label + TEXT(": the system accounts hold the opposite of the supply (ECO-003)"), E.L.SystemTotal(), -E.L.Supply());
		auto SameMap = [&](const TCHAR* Field, const TMap<FString, int32>& Got)
		{
			const TSharedPtr<FJsonObject> W = Want->GetObjectField(Field);
			T.TestEqual(Label + TEXT(": how many kinds of ") + Field, Got.Num(), W->Values.Num());
			for (const auto& V : W->Values) T.TestEqual(Label + TEXT(": ") + Field + TEXT(" ") + FString(V.Key.ToView()), (int64)Got.FindRef(FString(V.Key.ToView())), Num(V.Value));
		};
		SameMap(TEXT("inv"), E.Inv);
		SameMap(TEXT("clothes"), E.Clothes);
		SameMap(TEXT("homeware"), E.Homeware);
		T.TestEqual(Label + TEXT(": things sold on the stall"), (int64)E.StallSoldTotal, (int64)Want->GetNumberField(TEXT("sold")));
		const TArray<TSharedPtr<FJsonValue>>& WL = Want->GetArrayField(TEXT("loans"));
		if (T.TestEqual(Label + TEXT(": loans"), E.Loans.Num(), WL.Num()))
			for (int32 k = 0; k < WL.Num(); ++k)
			{
				const TArray<TSharedPtr<FJsonValue>>& W = WL[k]->AsArray();
				const FBrixLoan& Ln = E.Loans[k];
				T.TestEqual(Label + FString::Printf(TEXT(": loan %d's status"), k + 1), Ln.Status, W[0]->AsString());
				T.TestEqual(Label + FString::Printf(TEXT(": loan %d paid"), k + 1), Ln.Paid, Num(W[1]));
				T.TestEqual(Label + FString::Printf(TEXT(": loan %d's fees"), k + 1), Ln.Fees, Num(W[2]));
				T.TestEqual(Label + FString::Printf(TEXT(": loan %d's missed instalments"), k + 1), (int64)Ln.Missed, Num(W[3]));
			}
		for (const auto& P : Want->GetObjectField(TEXT("prices"))->Values)
		{
			const int32 n = (int32)Shops->GetNumberField(FString(P.Key.ToView()));
			T.TestEqual(Label + TEXT(": the price at the ") + FString(P.Key.ToView()), E.Price(n, E.R.Shop(FString(P.Key.ToView()))->Sells[0]), Num(P.Value));
		}
		int32 Outlets = 0;
		for (const TSharedPtr<FJsonValue>& OV : Want->GetArrayField(TEXT("outlets")))
		{
			const TArray<TSharedPtr<FJsonValue>>& O = OV->AsArray();
			const int32 n = (int32)Num(O[0]);
			const FString U = O[1]->AsString();
			const TArray<FString> Sells = E.OutletSells(n, U);
			const TArray<TSharedPtr<FJsonValue>>& WS = O[2]->AsArray();
			FString Got, Wanted;
			for (const FString& G : Sells) Got += G + TEXT(" ");
			for (const auto& G : WS) Wanted += G->AsString() + TEXT(" ");
			if (!T.TestEqual(Label + TEXT(": what outlet ") + U + TEXT(" sells (ECO-013)"), Got, Wanted)) continue;
			for (int32 k = 0; k < Sells.Num(); ++k)
			{
				T.TestEqual(Label + TEXT(": stock of ") + Sells[k] + TEXT(" at ") + U, (int64)E.OutletStock(n, U, Sells[k]), Num(O[3]->AsArray()[k]));
				T.TestEqual(Label + TEXT(": price of ") + Sells[k] + TEXT(" at ") + U, E.OutletPrice(n, U, Sells[k]), Num(O[4]->AsArray()[k]));
			}
			++Outlets;
		}
		T.TestTrue(Label + TEXT(": outlets checked"), Outlets > 0);
		const TArray<TSharedPtr<FJsonValue>>& WH = Want->GetArrayField(TEXT("hashes"));
		const double Hashes[4] = { BrixSliceHash({ TEXT("a") }), BrixSliceHash({ TEXT("1"), TEXT("2"), TEXT("b") }), BrixSliceHash({ TEXT("r0504"), TEXT("16"), TEXT("job") }), BrixSliceHash({ TEXT("523"), TEXT("3"), TEXT("117"), TEXT("buy") }) };
		for (int32 k = 0; k < 4 && k < WH.Num(); ++k) T.TestTrue(Label + FString::Printf(TEXT(": hash %d"), k), FMath::Abs(Hashes[k] - WH[k]->AsNumber()) < 1e-11);
		// every transaction balances and is whole chips (ECO-001, ECO-002)
		for (const FBrixTx& Tx : E.L.Tx)
		{
			int64 Sum = 0;
			for (const auto& Ln : Tx.Lines) Sum += Ln.Value;
			if (Sum != 0) T.AddError(Label + TEXT(": a transaction that does not balance: ") + Tx.Memo);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FECO002_LedgerMatchesPython, "BrixTown.World.ECO002_the_ledger_matches_the_python",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FECO002_LedgerMatchesPython::RunTest(const FString&)
{
	FLife Life;
	if (!LoadLife(*this, Life)) return false;
	RunAndCompare(*this, Life, Life.Data.Json->GetObjectField(TEXT("checks")), TEXT("economy.check_script"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBANK008_BankSettlesAsPython, "BrixTown.World.BANK008_the_bank_settles_as_the_python_does",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FBANK008_BankSettlesAsPython::RunTest(const FString&)
{
	FLife Life;
	if (!LoadLife(*this, Life)) return false;
	RunAndCompare(*this, Life, Life.Data.Json->GetObjectField(TEXT("bank_checks")), TEXT("bank_script"));
	return true;
}

// ECO-001, ECO-002, ECO-003, BANK-007: whole chips, balanced lines, nothing below zero but the system accounts, a refusal
// writes nothing; money comes only from system accounts.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FECO001_MoneyIsWholeChipsInBalancedLines, "BrixTown.World.ECO001_money_is_whole_chips_in_balanced_lines",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FECO001_MoneyIsWholeChipsInBalancedLines::RunTest(const FString&)
{
	TestEqual(TEXT("a sum is written in brix and chips"), BrixMoney::Format(1250), FString(TEXT("Bx 12.50")));
	TestEqual(TEXT("a debt has its sign"), BrixMoney::Format(-5), FString(TEXT("-Bx 0.05")));
	TestEqual(TEXT("floor division as JS and Python do it"), BrixMoney::FloorDiv(-7, 2), (int64)-4);
	FBrixLedger L;
	FString Why;
	TestTrue(TEXT("money made by a system account"), L.Post(0, 0, TEXT("start"), { { TEXT("sys:start"), -500 }, { TEXT("p:cash"), 500 } }, Why));
	TestFalse(TEXT("lines that do not balance are refused (ECO-002)"), L.Post(0, 0, TEXT("bad"), { { TEXT("p:cash"), -100 }, { TEXT("p:current"), 90 } }, Why));
	TestFalse(TEXT("an account other than a system one may not go below zero (BANK-007)"), L.Post(0, 0, TEXT("bad"), { { TEXT("p:cash"), -600 }, { TEXT("p:current"), 600 } }, Why));
	TestEqual(TEXT("a refused transaction writes nothing"), L.Tx.Num(), 1);
	TestEqual(TEXT("the cash is as it was"), L.Balance(TEXT("p:cash")), (int64)500);
	TestEqual(TEXT("the supply is what the system accounts made (ECO-003)"), L.Supply(), -L.SystemTotal());
	FLife Life;
	if (!LoadLife(*this, Life)) return false;
	FBrixEconomy E(Life.Data, Life.Lots);
	TestTrue(TEXT("the bank is a lot of the town"), E.BankLot >= 0);
	TestTrue(TEXT("the market is a lot of the town"), E.MarketLot >= 0);
	TestEqual(TEXT("at the start only the bank and the shops hold money"), E.L.Supply(), -E.L.Balance(TEXT("sys:founding")));
	E.Advance(0, 440);
	E.NewCharacter();
	TestEqual(TEXT("a new character's cash (ECO-012)"), E.L.Balance(TEXT("p:cash")), Life.Data.Rules.StartCash);
	TestFalse(TEXT("no savings before a current account (BANK-001)"), E.OpenAccount(TEXT("savings")));
	TestFalse(TEXT("no moving to an account not open"), E.Move(TEXT("p:cash"), TEXT("p:current"), 100));
	TestTrue(TEXT("a current account opens"), E.OpenAccount(TEXT("current")));
	TestFalse(TEXT("moving more than there is is refused, not partly made (BANK-007)"), E.Move(TEXT("p:cash"), TEXT("p:current"), E.L.Balance(TEXT("p:cash")) + 1));
	TestEqual(TEXT("after which the cash is untouched"), E.L.Balance(TEXT("p:cash")), Life.Data.Rules.StartCash);
	return true;
}

#endif
