#include "BrixEconomy.h"

#include "BrixCity.h"
#include "BrixDay.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace BrixMoney
{
	FString Format(int64 C)
	{
		const int64 A = FMath::Abs(C);
		return FString::Printf(TEXT("%sBx %lld.%02lld"), C < 0 ? TEXT("-") : TEXT(""), A / 100, A % 100);
	}
	bool IsSystem(const FString& A) { return A.StartsWith(TEXT("sys:"), ESearchCase::CaseSensitive); }
	const TArray<FString>& Player()
	{
		static const TArray<FString> P = { TEXT("p:cash"), TEXT("p:current"), TEXT("p:savings") };
		return P;
	}
	int64 FloorDiv(int64 A, int64 B)
	{
		int64 Q = A / B;
		if ((A % B != 0) && ((A < 0) != (B < 0))) --Q;
		return Q;
	}
}
using namespace BrixMoney;

namespace
{
	// the slice's hash01 (economy.js, play.js hash()): numbers as JS writes them, whole numbers here
	FString S(int32 V) { return FString::FromInt(V); }
	bool Less(const FString& A, const FString& B) { return FCString::Strcmp(*A, *B) < 0; }   // JS's sort(): by code unit
}

// ---------------------------------------------------------------- the ledger (ECO-002, ECO-003)

bool FBrixLedger::Post(int32 Day, double Minute, const FString& Memo, const FBrixLines& Lines, FString& Why)
{
	if (!Lines.Num()) { Why = TEXT("nothing to post"); return false; }
	int64 Total = 0;
	TMap<FString, int64> After;
	TArray<FString> Order;
	for (const TPair<FString, int64>& Ln : Lines)
	{
		Total += Ln.Value;
		int64* A = After.Find(Ln.Key);
		if (!A) { A = &After.Add(Ln.Key, Balance(Ln.Key)); Order.Add(Ln.Key); }
		*A += Ln.Value;
	}
	if (Total != 0) { Why = TEXT("lines do not balance"); return false; }
	for (const FString& A : Order) if (After[A] < 0 && !IsSystem(A)) { Why = A + TEXT(" would go below zero"); return false; }
	FBrixTx& T = Tx.AddDefaulted_GetRef();
	T.No = Tx.Num() - 1;
	T.Day = Day;
	T.Minute = Minute;
	T.Memo = Memo;
	for (const TPair<FString, int64>& Ln : Lines) if (Ln.Value != 0) T.Lines.Add(Ln);
	for (const FString& A : Order) Bal.Add(A, After[A]);
	return true;
}

int64 FBrixLedger::Supply() const
{
	int64 s = 0;
	for (const auto& B : Bal) if (!IsSystem(B.Key)) s += B.Value;
	return s;
}

int64 FBrixLedger::SystemTotal() const
{
	int64 s = 0;
	for (const auto& B : Bal) if (IsSystem(B.Key)) s += B.Value;
	return s;
}

TArray<FBrixLedger::FEntry> FBrixLedger::Statement(const TArray<FString>& Accts, int32 N) const
{
	TArray<FEntry> Out;
	for (int32 k = Tx.Num() - 1; k >= 0; --k)
	{
		const FBrixTx& T = Tx[k];
		bool bAny = false;
		int64 Ch = 0;
		for (const auto& Ln : T.Lines) if (Accts.Contains(Ln.Key)) { bAny = true; Ch += Ln.Value; }
		if (bAny) Out.Add({ T.No, T.Day, T.Minute, T.Memo, Ch });
		if (N && Out.Num() >= N) break;
	}
	return Out;
}

int64 FBrixLedger::MovedBetween(int32 No, const TArray<FString>& Accts) const
{
	int64 m = 0;
	if (Tx.IsValidIndex(No)) for (const auto& Ln : Tx[No].Lines) if (Accts.Contains(Ln.Key) && Ln.Value > 0) m += Ln.Value;
	return m;
}

// ---------------------------------------------------------------- loading Game/Data/life.json

namespace
{
	TArray<FString> Strings(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		TArray<FString> Out;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O && O->TryGetArrayField(Field, A)) for (const auto& V : *A) Out.Add(V->AsString());
		return Out;
	}
	int64 Int(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, int64 Default = 0)
	{
		double V = 0;
		return O && O->TryGetNumberField(Field, V) ? (int64)V : Default;
	}
}

bool LoadBrixLife(const FString& Path, FBrixLifeData& Out, FString& OutError)
{
	Out = FBrixLifeData();
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = TEXT("cannot read ") + Path; return false; }
	TSharedPtr<FJsonObject> J;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J) { OutError = Path + TEXT(" is not JSON"); return false; }
	Out.Json = J;
	FBrixEconomyRules& R = Out.Rules;
	const TSharedPtr<FJsonObject>* E = nullptr;
	if (!J->TryGetObjectField(TEXT("economy"), E)) { OutError = TEXT("no economy in ") + Path; return false; }
	const TSharedPtr<FJsonObject> Ec = *E;
	R.Symbol = Ec->GetObjectField(TEXT("currency"))->GetStringField(TEXT("symbol"));
	R.StartCash = Int(Ec->GetObjectField(TEXT("start")), TEXT("cash"));
	R.WelcomeDeposit = Int(Ec->GetObjectField(TEXT("start")), TEXT("welcome_deposit"));
	const TSharedPtr<FJsonObject> B = Ec->GetObjectField(TEXT("bank"));
	R.BankLot = B->GetStringField(TEXT("lot"));
	R.Vault = Int(B, TEXT("vault"));
	for (const auto& A : B->GetObjectField(TEXT("accounts"))->Values)
		R.Accounts.Add({ FString(A.Key.ToView()), A.Value->AsObject()->GetStringField(TEXT("name")), Int(A.Value->AsObject(), TEXT("rate_bp")) });
	const TSharedPtr<FJsonObject> Ln = B->GetObjectField(TEXT("loan"));
	R.LoanMin = Int(Ln, TEXT("min")); R.LoanMax = Int(Ln, TEXT("max")); R.LoanStep = Int(Ln, TEXT("step"), 1); R.LoanRateBp = Int(Ln, TEXT("rate_bp"));
	R.MaxLoans = (int32)Int(Ln, TEXT("max_loans"), 1); R.LateFee = Int(Ln, TEXT("late_fee")); R.DefaultAfterMissed = (int32)Int(Ln, TEXT("default_after_missed"), 3);
	for (const auto& V : Ln->GetArrayField(TEXT("terms"))) R.LoanTerms.Add((int32)V->AsNumber());
	for (const auto& I : Ec->GetObjectField(TEXT("items"))->Values)
	{
		const TSharedPtr<FJsonObject> O = I.Value->AsObject();
		FBrixEconomyRules::FItem It;
		It.Name = O->GetStringField(TEXT("name"));
		It.Price = Int(O, TEXT("price"));
		O->TryGetBoolField(TEXT("sell"), It.bSell);
		O->TryGetBoolField(TEXT("quest"), It.bQuest);
		R.Items.Add(FString(I.Key.ToView()), It);
	}
	for (const auto& Sh : Ec->GetObjectField(TEXT("shops"))->Values)
	{
		const TSharedPtr<FJsonObject> O = Sh.Value->AsObject();
		FBrixEconomyRules::FShop P;
		P.Kind = FString(Sh.Key.ToView());
		P.Name = O->GetStringField(TEXT("name"));
		const TSharedPtr<FJsonObject> M = O->GetObjectField(TEXT("match"));
		P.MatchKind = M->GetStringField(TEXT("kind"));
		M->TryGetStringField(TEXT("name_has"), P.NameHas);
		P.Sells = Strings(O, TEXT("sells"));
		P.Buys = Strings(O, TEXT("buys"));
		P.Till = Int(O, TEXT("till"));
		R.Shops.Add(P);
	}
	const TSharedPtr<FJsonObject> St = Ec->GetObjectField(TEXT("stock"));
	R.StockTarget = (int32)Int(St, TEXT("target"), 10);
	R.RestockMinute = (int32)Int(St, TEXT("restock_minute"), 360);
	const TSharedPtr<FJsonObject> Pr = Ec->GetObjectField(TEXT("price"));
	R.SlopeBp = Int(Pr, TEXT("slope_bp")); R.MinBp = Int(Pr, TEXT("min_bp")); R.MaxBp = Int(Pr, TEXT("max_bp")); R.SellShareBp = Int(Pr, TEXT("sell_share_bp"));
	const TSharedPtr<FJsonObject> Rp = Ec->GetObjectField(TEXT("reputation"));
	R.RepMin = (int32)Int(Rp, TEXT("min"), -100); R.RepMax = (int32)Int(Rp, TEXT("max"), 100);
	R.DiscountBpAtMax = Int(Rp, TEXT("discount_bp_at_max")); R.MarkupBpAtMin = Int(Rp, TEXT("markup_bp_at_min"));
	const TSharedPtr<FJsonObject> Sl = Ec->GetObjectField(TEXT("stall"));
	R.StallLot = Sl->GetStringField(TEXT("lot"));
	R.StallRent = Int(Sl, TEXT("rent")); R.StallSlots = (int32)Int(Sl, TEXT("slots"), 6); R.StallBuyBp = Int(Sl, TEXT("buy_bp"));
	R.StallElasticity = Sl->GetNumberField(TEXT("elasticity")); R.StallMaxPriceFactorBp = Int(Sl, TEXT("max_price_factor_bp"));
	const TArray<TSharedPtr<FJsonValue>>& Open = Sl->GetArrayField(TEXT("open"));
	if (Open.Num() == 2) { R.StallOpen = (int32)Open[0]->AsNumber(); R.StallClose = (int32)Open[1]->AsNumber(); }
	const TSharedPtr<FJsonObject>* G = nullptr;
	if (J->TryGetObjectField(TEXT("goods"), G))
	{
		R.bGoods = true;
		R.PerOutlet = (int32)Int(*G, TEXT("per_outlet"), 8);
		R.OutletStockTarget = (int32)Int((*G)->GetObjectField(TEXT("stock")), TEXT("target"), 6);
		for (const auto& K : (*G)->GetObjectField(TEXT("sells"))->Values)
		{
			TArray<FString>& L = R.OutletSells.Add(FString(K.Key.ToView()));
			for (const auto& V : K.Value->AsArray()) L.Add(V->AsString());
		}
		for (const auto& K : (*G)->GetObjectField(TEXT("goods"))->Values)
		{
			const TSharedPtr<FJsonObject> O = K.Value->AsObject();
			FBrixEconomyRules::FGood Gd;
			Gd.Name = O->GetStringField(TEXT("name"));
			Gd.Price = Int(O, TEXT("price"));
			Gd.Goes = O->GetStringField(TEXT("goes"));
			O->TryGetStringField(TEXT("piece"), Gd.Piece);
			R.Goods.Add(FString(K.Key.ToView()), Gd);
		}
	}
	Out.LotIds = Strings(J, TEXT("lots"));
	for (const auto& V : J->GetObjectField(TEXT("visits"))->Values)
	{
		TArray<TPair<double, int32>>& L = Out.Visits.Add(FCString::Atoi(*FString(V.Key.ToView())));
		for (const auto& P : V.Value->AsArray()) { const auto& A = P->AsArray(); L.Add({ A[0]->AsNumber(), (int32)A[1]->AsNumber() }); }
	}
	for (const auto& V : J->GetObjectField(TEXT("staff"))->Values)
	{
		TArray<FString>& L = Out.Staff.Add(FCString::Atoi(*FString(V.Key.ToView())));
		for (const auto& P : V.Value->AsArray()) L.Add(P->AsString());
	}
	for (const auto& V : J->GetObjectField(TEXT("outlets"))->Values)
	{
		TArray<FBrixOutlet>& L = Out.Outlets.Add(FCString::Atoi(*FString(V.Key.ToView())));
		for (const auto& P : V.Value->AsArray())
		{
			const TSharedPtr<FJsonObject> O = P->AsObject();
			L.Add({ O->GetStringField(TEXT("id")), O->GetStringField(TEXT("name")), O->GetStringField(TEXT("kind")), (int32)O->GetNumberField(TEXT("level")) });
		}
	}
	// ECO-012: the numbers the rules lean on must be there and sensible
	if (R.StartCash <= 0 || R.Vault <= 0 || !R.Account(TEXT("current")) || !R.Account(TEXT("savings")) || !R.LoanTerms.Num() || R.LoanStep <= 0 || R.StockTarget <= 0
		|| R.MinBp > R.MaxBp || R.RepMin >= 0 || R.RepMax <= 0 || !R.Shops.Num())
	{
		OutError = TEXT("the economy's numbers are missing or out of range");
		return false;
	}
	for (const FBrixEconomyRules::FShop& P : R.Shops)
		for (const FString& It : P.Sells) if (!R.Items.Contains(It)) { OutError = P.Kind + TEXT(" sells an item there is not: ") + It; return false; }
	return true;
}

bool BrixEconLots(const FBrixCity& City, const FBrixLifeData& Life, TArray<FBrixEconLot>& Out, FString& OutError)
{
	Out.Reset();
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) Out.Add({ L.Id, L.Kind, L.Name, L.Min, L.Max });
	if (Out.Num() != Life.LotIds.Num()) { OutError = FString::Printf(TEXT("the city has %d lots, life.json %d: run Tools/worldgen/build_play.py"), Out.Num(), Life.LotIds.Num()); return false; }
	for (int32 n = 0; n < Out.Num(); ++n)
		if (Out[n].Id != Life.LotIds[n]) { OutError = FString::Printf(TEXT("lot %d is %s in the city, %s in life.json: run Tools/worldgen/build_play.py"), n, *Out[n].Id, *Life.LotIds[n]); return false; }
	return true;
}

// ---------------------------------------------------------------- the economy

FBrixEconomy::FBrixEconomy(const FBrixLifeData& Life, const TArray<FBrixEconLot>& InLots)
	: R(Life.Rules), Lots(InLots), Visits(Life.Visits), Staff(Life.Staff)
{
	for (int32 n = 0; n < Lots.Num(); ++n) LotIndex.Add(Lots[n].Id, n);
	Items = R.Items;
	if (R.bGoods)
	{
		for (const auto& G : R.Goods)
			if (G.Value.Goes == TEXT("carry") || G.Value.Goes == TEXT("eat")) Items.Add(G.Key, { G.Value.Name, G.Value.Price, G.Value.Goes == TEXT("carry"), false });
		Outlets = Life.Outlets;
	}
	BankLot = LotIndex.FindRef(R.BankLot, -1);
	MarketLot = LotIndex.FindRef(R.StallLot, -1);
	FBrixLines Lines = { { TEXT("bank:vault"), R.Vault } };
	for (int32 n = 0; n < Lots.Num(); ++n)
	{
		Kinds.Add(KindOf(n));
		if (!Kinds[n].IsEmpty()) Lines.Add({ TEXT("till:") + Lots[n].Id, R.Shop(Kinds[n])->Till });
	}
	int64 Sum = 0;
	for (const auto& Ln : Lines) Sum += Ln.Value;
	Lines.Add({ TEXT("sys:founding"), -Sum });
	L.Post(0, 0, TEXT("The town's bank and shops open"), Lines, Why);
}

void FBrixEconomy::NewCharacter()
{
	L.Post(Day, Minute, TEXT("Starting cash"), { { TEXT("sys:start"), -R.StartCash }, { TEXT("p:cash"), R.StartCash } }, Why);
}

int64 FBrixEconomy::TotalMoney() const
{
	int64 s = 0;
	for (const FString& A : Player()) s += L.Balance(A);
	return s;
}

int64 FBrixEconomy::Funds() const { return L.Balance(TEXT("p:cash")) + (HasAccount(TEXT("current")) ? L.Balance(TEXT("p:current")) : 0); }

bool FBrixEconomy::GiveItem(const FString& It, int32 N)
{
	if (!Items.Contains(It)) return No(TEXT("no such item ") + It);
	Inv.FindOrAdd(It) += N;
	return true;
}

bool FBrixEconomy::TakeItem(const FString& It, int32 N)
{
	if (Inv.FindRef(It) < N) return No(TEXT("you have no ") + It);
	if ((Inv[It] -= N) == 0) Inv.Remove(It);
	return true;
}

bool FBrixEconomy::Spend(int64 Amount, const FString& To, const FString& Memo)
{
	if (Amount <= 0) return No(TEXT("nothing to pay"));
	const int64 a = FMath::Min(L.Balance(TEXT("p:cash")), Amount);
	FBrixLines Lines;
	if (a) Lines.Add({ TEXT("p:cash"), -a });
	if (Amount > a)
	{
		if (!HasAccount(TEXT("current"))) return No(TEXT("not enough cash"));
		Lines.Add({ TEXT("p:current"), -(Amount - a) });
	}
	Lines.Add({ To, Amount });
	return L.Post(Day, Minute, Memo, Lines, Why);
}

// ---- the clock and the bank

void FBrixEconomy::Advance(int32 ToDay, double ToMinute)
{
	if (ToDay < Day || (ToDay == Day && ToMinute < Minute)) return;   // the clock only goes forward
	if (Stall && Stall->Day < ToDay) RunStall(Stall->Day, 1440);
	while (Settled < ToDay) { Day = Settled; Minute = 1440; Settle(Settled); ++Settled; }
	Day = ToDay;
	Minute = ToMinute;
	if (Stall && Stall->Day == ToDay) RunStall(ToDay, ToMinute);
}

void FBrixEconomy::Settle(int32 D)
{
	for (const FString& Kind : Accounts)
	{
		const FBrixEconomyRules::FAccount* A = R.Account(Kind);
		const FString Acct = TEXT("p:") + Kind;
		const int64 Gain = FloorDiv(L.Balance(Acct) * A->RateBp, 10000);
		if (Gain > 0 && L.Balance(TEXT("bank:vault")) >= Gain)
			L.Post(D, 1440, TEXT("Interest, ") + A->Name.ToLower(), { { TEXT("bank:vault"), -Gain }, { Acct, Gain } }, Why);
	}
	for (FBrixLoan& Loan : Loans) if (Loan.Status == TEXT("active")) LoanDay(Loan, D);
	if (Stall && Stall->Day == D)
	{
		for (const FBrixStall::FSlot& Sl : Stall->Slots) if (!Sl.bSold) GiveItem(Sl.Item);
		Stall.Reset();
	}
	TArray<int32> Ids;
	Hooks.GetKeys(Ids);
	Ids.Sort();
	for (const int32 Id : Ids) if (const TFunction<void(int32)>* H = Hooks.Find(Id)) (*H)(D);
}

bool FBrixEconomy::OpenAccount(const FString& Kind)
{
	if (!R.Account(Kind)) return No(TEXT("no such account"));
	if (HasAccount(Kind)) return No(TEXT("already open"));
	if (Kind == TEXT("savings") && !HasAccount(TEXT("current"))) return No(TEXT("open a current account first"));
	Accounts.Add(Kind);
	if (Kind == TEXT("current") && !bWelcomed)
	{
		bWelcomed = true;
		L.Post(Day, Minute, TEXT("Linden Bank welcome deposit"), { { TEXT("sys:start"), -R.WelcomeDeposit }, { TEXT("p:current"), R.WelcomeDeposit } }, Why);
	}
	return true;
}

bool FBrixEconomy::Move(const FString& Src, const FString& Dst, int64 Amount)
{
	for (const FString& A : { Src, Dst })
		if (!Player().Contains(A) || (A != TEXT("p:cash") && !HasAccount(A.Mid(2)))) return No(TEXT("no such account open: ") + A);
	if (Src == Dst || Amount <= 0) return No(TEXT("nothing to move"));
	return L.Post(Day, Minute, TEXT("Transfer"), { { Src, -Amount }, { Dst, Amount } }, Why);
}

TPair<int64, TArray<int64>> FBrixEconomy::LoanTerms(int64 Principal, int32 Term) const
{
	const int64 Total = Principal + FloorDiv(Principal * R.LoanRateBp * Term, 10000), Inst = FloorDiv(Total, Term);
	TArray<int64> Plan;
	for (int32 k = 0; k < Term - 1; ++k) Plan.Add(Inst);
	Plan.Add(Total - Inst * (Term - 1));
	return { Total, Plan };
}

FBrixLoan* FBrixEconomy::ActiveLoan() { return Loans.FindByPredicate([](const FBrixLoan& Ln) { return Ln.Status == TEXT("active"); }); }

bool FBrixEconomy::TakeLoan(int64 Principal, int32 Term)
{
	if (!HasAccount(TEXT("current"))) return No(TEXT("a loan needs a current account"));
	if (bBarred) return No(TEXT("the bank will not lend to you again"));
	int32 Active = 0;
	for (const FBrixLoan& Ln : Loans) Active += Ln.Status == TEXT("active");
	if (Active >= R.MaxLoans) return No(TEXT("you already have a loan"));
	if (!(R.LoanMin <= Principal && Principal <= R.LoanMax) || Principal % R.LoanStep || !R.LoanTerms.Contains(Term)) return No(TEXT("the bank does not offer that loan"));
	if (L.Balance(TEXT("bank:vault")) < Principal) return No(TEXT("the bank cannot lend that today"));
	const TPair<int64, TArray<int64>> T = LoanTerms(Principal, Term);
	FBrixLoan Loan;
	Loan.Id = Loans.Num() + 1;
	Loan.Day = Day;
	Loan.Principal = Principal;
	Loan.Total = T.Key;
	Loan.Plan = T.Value;
	if (!L.Post(Day, Minute, FString::Printf(TEXT("Loan %d paid out"), Loan.Id), { { TEXT("bank:vault"), -Principal }, { TEXT("p:current"), Principal } }, Why)) return false;
	Loans.Add(Loan);
	return true;
}

void FBrixEconomy::PayLoan(FBrixLoan& Loan, int64 Amount, int32 D, double M, const FString& Memo)
{
	const int64 Fee = FMath::Min(Amount, Loan.Fees - Loan.FeesPaid);
	FBrixLines Lines = { { TEXT("p:current"), -Amount } };
	if (Fee) Lines.Add({ TEXT("sys:fees"), Fee });
	if (Amount - Fee) Lines.Add({ TEXT("bank:vault"), Amount - Fee });
	L.Post(D, M, Memo, Lines, Why);
	Loan.FeesPaid += Fee;
	Loan.Paid += Amount - Fee;
	if (Loan.Owed() <= 0) Loan.Status = TEXT("repaid");
}

void FBrixEconomy::LoanDay(FBrixLoan& Loan, int32 D)
{
	const int32 k = D - Loan.Day;
	if (k < 1) return;
	int64 Sched = 0;
	for (int32 j = 0; j < FMath::Min(k, Loan.Plan.Num()); ++j) Sched += Loan.Plan[j];
	Sched -= Loan.Paid + Loan.Taken;
	const int64 Due = FMath::Max<int64>(0, Sched) + Loan.Fees - Loan.FeesPaid;
	if (Due <= 0) return;
	if (HasAccount(TEXT("current")) && L.Balance(TEXT("p:current")) >= Due)
	{
		PayLoan(Loan, Due, D, 1440, FString::Printf(TEXT("Loan %d repayment"), Loan.Id));
		Loan.Missed = 0;
		return;
	}
	++Loan.Missed;
	Loan.Fees += R.LateFee;
	if (Loan.Missed >= R.DefaultAfterMissed) DefaultLoan(Loan, D);
}

void FBrixEconomy::DefaultLoan(FBrixLoan& Loan, int32 D)
{
	const int64 Cur = L.Balance(TEXT("p:current"));
	if (Cur > 0) PayLoan(Loan, FMath::Min(Cur, Loan.Owed()), D, 1440, FString::Printf(TEXT("Loan %d: account seized"), Loan.Id));
	TArray<FString> Keys;
	Inv.GetKeys(Keys);
	Keys.Sort(Less);
	for (const FString& It : Keys)
	{
		const FBrixEconomyRules::FItem& I = Items[It];
		if (!I.bSell || I.Price <= 0) continue;
		while (Inv.FindRef(It) && Loan.Owed() > 0) { TakeItem(It); Loan.Taken += FloorDiv(I.Price * R.SellShareBp, 10000); }
	}
	Loan.Status = TEXT("defaulted");
	bBarred = true;
}

bool FBrixEconomy::Repay(int32 Id)
{
	FBrixLoan* Loan = Loans.FindByPredicate([Id](const FBrixLoan& Ln) { return Ln.Id == Id && Ln.Status == TEXT("active"); });
	if (!Loan) return No(TEXT("no such loan"));
	const int64 Owed = Loan->Owed();
	if (L.Balance(TEXT("p:current")) < Owed) return No(TEXT("not enough in the current account"));
	PayLoan(*Loan, Owed, Day, Minute, FString::Printf(TEXT("Loan %d repaid early"), Loan->Id));
	return true;
}

// ---- shops

FString FBrixEconomy::KindOf(int32 N) const
{
	const FBrixEconLot& Lt = Lots[N];
	for (const FBrixEconomyRules::FShop& P : R.Shops) if (Lt.Kind == P.MatchKind && (P.NameHas.IsEmpty() || Lt.Name.Contains(P.NameHas, ESearchCase::CaseSensitive))) return P.Kind;
	return FString();
}

int32 FBrixEconomy::ResidentBuys(int32 N, const FString& It) const
{
	const TArray<FString>& Sells = R.Shop(Kinds[N])->Sells;
	const int32 Rd = Restock(Day, Minute);
	struct FSpan { int32 D; double A, B; };
	TArray<FSpan> Spans = { { Rd, (double)R.RestockMinute, Rd == Day ? Minute : 1440.0 } };
	if (Rd < Day) Spans.Add({ Day, 0.0, Minute });
	int32 c = 0;
	const TArray<TPair<double, int32>>* V = Visits.Find(N);
	if (!V) return 0;
	for (const FSpan& Sp : Spans)
		for (const TPair<double, int32>& T : *V)
			if (Sp.A <= T.Key && T.Key <= Sp.B && Sells[(int32)FMath::Floor(BrixSliceHash({ S(T.Value), S(Sp.D), S(N), TEXT("buy") }) * Sells.Num())] == It) ++c;
	return c;
}

int32 FBrixEconomy::Stock(int32 N, const FString& It) const
{
	const FString K = ShopKind(N);
	if (K.IsEmpty() || !R.Shop(K)->Sells.Contains(It)) return 0;
	const TMap<FString, TPair<int32, int32>>* T = Trades.Find(S(N) + TEXT("|") + S(Restock()));
	const TPair<int32, int32> BS = T && T->Contains(It) ? (*T)[It] : TPair<int32, int32>(0, 0);
	return FMath::Max(0, R.StockTarget - ResidentBuys(N, It) - BS.Key + BS.Value);
}

int32 FBrixEconomy::RepWithLot(int32 N) const
{
	const TArray<FString>* W = Staff.Find(N);
	bool bAny = false;
	int32 Best = 0;
	if (W) for (const FString& Rid : *W) if (const int32* V = Rep.Find(Rid)) { Best = bAny ? FMath::Max(Best, *V) : *V; bAny = true; }
	return Best;
}

int64 FBrixEconomy::Factor(int32 N, const FString& It) const
{
	const int64 T = R.StockTarget;
	return FMath::Clamp<int64>(10000 + FloorDiv(R.SlopeBp * (T - Stock(N, It)), T), R.MinBp, R.MaxBp);
}

namespace
{
	int64 RepMod(const FBrixEconomyRules& R, int32 r)
	{
		return r >= 0 ? 10000 - FloorDiv(R.DiscountBpAtMax * r, R.RepMax) : 10000 + FloorDiv(R.MarkupBpAtMin * r, R.RepMin);
	}
}

int64 FBrixEconomy::Price(int32 N, const FString& It) const
{
	return FMath::Max<int64>(1, FloorDiv(Items[It].Price * Factor(N, It) * RepMod(R, RepWithLot(N)) + 50000000, 100000000));
}

int64 FBrixEconomy::Offer(int32 N, const FString& It) const
{
	const FString K = ShopKind(N);
	const FBrixEconomyRules::FItem* I = Items.Find(It);
	if (K.IsEmpty() || !I || !I->bSell || !R.Shop(K)->Buys.Contains(It)) return 0;
	const int64 F = R.Shop(K)->Sells.Contains(It) ? Factor(N, It) : 10000;
	return FloorDiv(I->Price * F * R.SellShareBp, 100000000);
}

void FBrixEconomy::Trade(int32 N, const FString& It, int32 K)
{
	TPair<int32, int32>& T = Trades.FindOrAdd(S(N) + TEXT("|") + S(Restock())).FindOrAdd(It);
	(K == 0 ? T.Key : T.Value)++;
}

bool FBrixEconomy::Buy(int32 N, const FString& It, int64* OutPaid)
{
	if (Outlets.Contains(N)) return No(TEXT("buy at one of its outlets"));
	const FString K = ShopKind(N);
	if (K.IsEmpty() || !R.Shop(K)->Sells.Contains(It)) return No(TEXT("this shop does not sell that"));
	if (Stock(N, It) < 1) return No(TEXT("sold out"));
	const int64 P = Price(N, It);
	if (!Spend(P, TEXT("till:") + Lots[N].Id, Items[It].Name + TEXT(", ") + Lots[N].Name)) return false;
	Trade(N, It, 0);
	GiveItem(It);
	if (OutPaid) *OutPaid = P;
	return true;
}

bool FBrixEconomy::Sell(int32 N, const FString& It, int64* OutPaid)
{
	const int64 P = Offer(N, It);
	if (P <= 0) return No(TEXT("this shop does not buy that"));
	if (Inv.FindRef(It) < 1) return No(TEXT("you have none"));
	const FString Till = TEXT("till:") + Lots[N].Id;
	if (L.Balance(Till) < P) return No(TEXT("the till is empty"));
	if (!L.Post(Day, Minute, TEXT("Sold ") + Items[It].Name.ToLower() + TEXT(", ") + Lots[N].Name, { { Till, -P }, { TEXT("p:cash"), P } }, Why)) return false;
	TakeItem(It);
	if (R.Shop(ShopKind(N))->Sells.Contains(It)) Trade(N, It, 1);
	if (OutPaid) *OutPaid = P;
	return true;
}

// ---- a mall's outlets

const FBrixOutlet* FBrixEconomy::Outlet(int32 N, const FString& U) const
{
	const TArray<FBrixOutlet>* Us = Outlets.Find(N);
	return Us ? Us->FindByPredicate([&](const FBrixOutlet& O) { return O.Id == U; }) : nullptr;
}

FBrixEconomyRules::FGood FBrixEconomy::Good(const FString& G) const
{
	if (const FBrixEconomyRules::FGood* Gd = R.Goods.Find(G)) return *Gd;
	FBrixEconomyRules::FGood Out;
	if (const FBrixEconomyRules::FItem* I = R.Items.Find(G)) { Out.Name = I->Name; Out.Price = I->Price; Out.Goes = I->bSell ? TEXT("carry") : TEXT("eat"); }
	return Out;
}

TArray<FString> FBrixEconomy::OutletRange(const FBrixEconomyRules& R, const FString& U, const FString& Kind)
{
	const TArray<FString>* L = R.OutletSells.Find(Kind);
	if (!L) return {};
	if (L->Num() <= R.PerOutlet) return *L;
	TArray<TPair<double, FString>> H;
	for (const FString& G : *L) H.Add({ BrixSliceHash({ U, G, TEXT("range") }), G });
	H.Sort([](const TPair<double, FString>& A, const TPair<double, FString>& B) { return A.Key != B.Key ? A.Key < B.Key : Less(A.Value, B.Value); });
	TSet<FString> Keep;
	for (int32 k = 0; k < R.PerOutlet; ++k) Keep.Add(H[k].Value);
	TArray<FString> Out;
	for (const FString& G : *L) if (Keep.Contains(G)) Out.Add(G);
	return Out;
}

TArray<FString> FBrixEconomy::OutletSells(int32 N, const FString& U) const
{
	const FBrixOutlet* O = Outlet(N, U);
	if (!O) return {};
	const FString Key = S(N) + TEXT("|") + U;
	if (const TArray<FString>* C = SellsCache.Find(Key)) return *C;
	return SellsCache.Add(Key, OutletRange(R, U, O->Kind));
}

TMap<FString, int32> FBrixEconomy::OutletResidentBuys(int32 N, const FString& U) const
{
	TMap<FString, int32> c;
	const TArray<FBrixOutlet>& Us = Outlets[N];
	const TArray<FString> Sells = OutletSells(N, U);
	if (!Sells.Num()) return c;
	const int32 Rd = Restock(Day, Minute);
	struct FSpan { int32 D; double A, B; };
	TArray<FSpan> Spans = { { Rd, (double)R.RestockMinute, Rd == Day ? Minute : 1440.0 } };
	if (Rd < Day) Spans.Add({ Day, 0.0, Minute });
	const TArray<TPair<double, int32>>* V = Visits.Find(N);
	if (!V) return c;
	for (const FSpan& Sp : Spans)
		for (const TPair<double, int32>& T : *V)
			if (Sp.A <= T.Key && T.Key <= Sp.B && Us[(int32)FMath::Floor(BrixSliceHash({ S(T.Value), S(Sp.D), S(N), TEXT("outlet") }) * Us.Num())].Id == U)
				c.FindOrAdd(Sells[(int32)FMath::Floor(BrixSliceHash({ S(T.Value), S(Sp.D), U, TEXT("buy") }) * Sells.Num())])++;
	return c;
}

int32 FBrixEconomy::OutletStock(int32 N, const FString& U, const FString& G) const
{
	if (!Outlet(N, U) || !OutletSells(N, U).Contains(G)) return 0;
	const TMap<FString, TPair<int32, int32>>* T = Trades.Find(S(N) + TEXT("|") + S(Restock()) + TEXT("|") + U);
	const int32 b = T && T->Contains(G) ? (*T)[G].Key : 0;
	return FMath::Max(0, R.OutletStockTarget - OutletResidentBuys(N, U).FindRef(G) - b);
}

int64 FBrixEconomy::OutletPrice(int32 N, const FString& U, const FString& G) const
{
	const int64 T = R.OutletStockTarget;
	const int64 F = FMath::Clamp<int64>(10000 + FloorDiv(R.SlopeBp * (T - OutletStock(N, U, G)), T), R.MinBp, R.MaxBp);
	return FMath::Max<int64>(1, FloorDiv(Good(G).Price * F * RepMod(R, RepWithLot(N)) + 50000000, 100000000));
}

bool FBrixEconomy::BuyAt(int32 N, const FString& U, const FString& G, int64* OutPaid)
{
	const FBrixOutlet* O = Outlet(N, U);
	if (!O) return No(TEXT("no such outlet"));
	if (!OutletSells(N, U).Contains(G)) return No(TEXT("this outlet does not sell that"));
	if (OutletStock(N, U, G) < 1) return No(TEXT("sold out"));
	const FBrixEconomyRules::FGood Gd = Good(G);
	const int64 P = OutletPrice(N, U, G);
	if (!Spend(P, TEXT("till:") + Lots[N].Id, Gd.Name + TEXT(", ") + O->Name)) return false;
	Trades.FindOrAdd(S(N) + TEXT("|") + S(Restock()) + TEXT("|") + U).FindOrAdd(G).Key++;
	if (Gd.Goes == TEXT("wear")) Clothes.FindOrAdd(G)++;
	else if (Gd.Goes == TEXT("home")) Homeware.FindOrAdd(G)++;
	else GiveItem(G);
	if (OutPaid) *OutPaid = P;
	return true;
}

// ---- the market stall

bool FBrixEconomy::RentStall()
{
	if (Stall) return No(TEXT("you already have a stall today"));
	if (!Spend(R.StallRent, TEXT("sys:fees"), TEXT("Market stall rent"))) return false;
	FBrixStall St;
	St.Day = Day;
	St.Done = Minute;
	Stall = St;
	return true;
}

bool FBrixEconomy::SetOut(const FString& It, int64 P)
{
	const FBrixEconomyRules::FItem* I = Items.Find(It);
	if (!Stall) return No(TEXT("rent a stall first"));
	if (!I || I->bQuest || I->Price <= 0) return No(TEXT("that cannot be sold here"));
	if (Stall->Slots.Num() >= R.StallSlots) return No(TEXT("the stall is full"));
	if (P < 1 || P > FloorDiv(I->Price * R.StallMaxPriceFactorBp, 10000)) return No(TEXT("that price is not allowed"));
	if (!TakeItem(It)) return false;
	Stall->Slots.Add({ It, P, false });
	return true;
}

bool FBrixEconomy::TakeBack(int32 K)
{
	if (!Stall || !Stall->Slots.IsValidIndex(K) || Stall->Slots[K].bSold) return No(TEXT("nothing there"));
	const FString It = Stall->Slots[K].Item;
	Stall->Slots.RemoveAt(K);
	return GiveItem(It);
}

int64 FBrixEconomy::StallChance(const FString& It, int64 P) const
{
	const double Base = (double)Items[It].Price, E = R.StallElasticity;
	return FMath::Min<int64>(9500, (int64)FMath::Floor((double)R.StallBuyBp * FMath::Pow(Base, E) / FMath::Pow((double)P, E)));
}

void FBrixEconomy::RunStall(int32 D, double M)
{
	FBrixStall& St = *Stall;
	const double a = FMath::Max(St.Done, (double)R.StallOpen), b = FMath::Min(M, (double)R.StallClose);
	if (const TArray<TPair<double, int32>>* V = Visits.Find(MarketLot))
		for (const TPair<double, int32>& T : *V)
		{
			if (!(a < T.Key && T.Key <= b)) continue;
			for (int32 k = 0; k < St.Slots.Num(); ++k)
			{
				FBrixStall::FSlot& Sl = St.Slots[k];
				if (Sl.bSold) continue;
				if (BrixSliceHash({ S(T.Value), S(D), TEXT("stall"), S(k), Sl.Item }) * 10000 < StallChance(Sl.Item, Sl.Price))
				{
					Sl.bSold = true;
					++StallSoldTotal;
					L.Post(D, T.Key, TEXT("Stall: ") + Items[Sl.Item].Name.ToLower() + TEXT(" sold"), { { TEXT("sys:residents"), -Sl.Price }, { TEXT("p:cash"), Sl.Price } }, Why);
					break;
				}
			}
		}
	St.Done = FMath::Max(St.Done, M);
}
