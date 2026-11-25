#include "BrixRewards.h"

#include "BrixQuests.h"
#include "Dom/JsonObject.h"

int32 FBrixRewardRules::CoinsFor(int64 Amount) const
{
	int32 n = 0;
	for (const TPair<int64, int32>& T : Tiers) if (Amount >= T.Key) n = T.Value;
	return n;
}

TArray<FBrixRewardEvent> BrixRewardEvents(const FBrixRewardRules& R, const TArray<FBrixTx>& Tx, int32 From)
{
	TArray<FBrixRewardEvent> Out;
	TMap<FString, int64> Interest;
	for (int32 k = FMath::Max(0, From); k < Tx.Num(); ++k)
	{
		const FBrixTx& T = Tx[k];
		TMap<FString, int64> Got;
		int64 Paid = 0;
		bool bFromRewards = false, bFromBank = false;
		for (const auto& L : T.Lines)
		{
			if (L.Key.StartsWith(TEXT("p:")) && L.Value > 0) { Got.FindOrAdd(L.Key) += L.Value; Paid += L.Value; }
			bFromRewards |= L.Key == R.RewardFrom && L.Value < 0;
			bFromBank |= L.Key == R.InterestFrom && L.Value < 0;
		}
		if (!Paid) continue;
		if (bFromRewards) { FBrixRewardEvent E; E.Kind = TEXT("reward"); E.Amount = Paid; E.Coins = R.CoinsFor(Paid); E.Memo = T.Memo; E.To = Got; Out.Add(E); }
		else if (T.Memo.StartsWith(R.InterestMemo, ESearchCase::CaseSensitive) && bFromBank) for (const auto& G : Got) Interest.FindOrAdd(G.Key) += G.Value;
	}
	if (Interest.Num())
	{
		FBrixRewardEvent E;
		E.Kind = TEXT("interest");
		Interest.GetKeys(E.Accounts);
		E.Accounts.Sort([](const FString& A, const FString& B) { return FCString::Strcmp(*A, *B) < 0; });
		for (const auto& I : Interest) E.Amount += I.Value;
		E.To = Interest;
		Out.Add(E);
	}
	return Out;
}

TMap<FString, int64> BrixAchievementFacts(const FBrixAchievementRules& R, const FBrixEconomy& E, const FBrixQuests* Q)
{
	int64 Jobs = 0, Repaid = 0;
	for (const FBrixTx& T : E.L.Tx)
		if (T.Memo.StartsWith(R.JobMemo, ESearchCase::CaseSensitive) && T.Lines.ContainsByPredicate([](const TPair<FString, int64>& L) { return L.Key.StartsWith(TEXT("p:")) && L.Value > 0; })) ++Jobs;
	for (const FBrixLoan& L : E.Loans) Repaid += L.Status == TEXT("repaid");
	return { { TEXT("quests_done"), Q ? Q->Done.Num() : 0 }, { TEXT("jobs_paid"), Jobs }, { TEXT("savings"), E.L.Balance(TEXT("p:savings")) }, { TEXT("loans_repaid"), Repaid },
		{ TEXT("stall_sold"), E.StallSoldTotal }, { TEXT("homes_rented"), 0 }, { TEXT("homes_owned"), 0 }, { TEXT("money"), E.TotalMoney() } };
}

TArray<FString> BrixAward(const FBrixAchievementRules& R, const TMap<FString, int64>& F, TMap<FString, int32>& Have, int32 Day)
{
	TArray<FString> Got;
	for (const FBrixAchievement& A : R.List) if (!Have.Contains(A.Id) && F.FindRef(A.Fact) >= A.AtLeast) Got.Add(A.Id);
	for (const FString& Id : Got) Have.Add(Id, Day);
	return Got;
}

bool LoadBrixRewards(const TSharedPtr<FJsonObject>& J, FBrixRewardRules& R, FBrixAchievementRules& A, FString& OutError)
{
	R = FBrixRewardRules();
	A = FBrixAchievementRules();
	const TSharedPtr<FJsonObject>* Rw = nullptr;
	if (!J || !J->TryGetObjectField(TEXT("rewards"), Rw)) { OutError = TEXT("no rewards in life.json"); return false; }
	const TSharedPtr<FJsonObject> L = (*Rw)->GetObjectField(TEXT("ledger")), C = (*Rw)->GetObjectField(TEXT("coins"));
	R.RewardFrom = L->GetStringField(TEXT("reward_from"));
	R.InterestFrom = L->GetStringField(TEXT("interest_from"));
	R.InterestMemo = L->GetStringField(TEXT("interest_memo"));
	for (const auto& T : C->GetArrayField(TEXT("reward_tiers"))) R.Tiers.Add({ (int64)T->AsArray()[0]->AsNumber(), (int32)T->AsArray()[1]->AsNumber() });
	auto N = [&C](const TCHAR* F, double& V) { C->TryGetNumberField(F, V); };
	auto Pair = [&C](const TCHAR* F, double* V) { const TArray<TSharedPtr<FJsonValue>>* P = nullptr; if (C->TryGetArrayField(F, P) && P->Num() == 2) { V[0] = (*P)[0]->AsNumber(); V[1] = (*P)[1]->AsNumber(); } };
	N(TEXT("start_s"), R.StartS); N(TEXT("gap_s"), R.GapS); N(TEXT("from_m"), R.FromM); Pair(TEXT("ring_m"), R.RingM); Pair(TEXT("up_mps"), R.UpMps);
	N(TEXT("fall_mps2"), R.FallMps2); N(TEXT("bounce"), R.Bounce); N(TEXT("size_m"), R.SizeM); N(TEXT("hover_m"), R.HoverM); N(TEXT("pull_after_s"), R.PullAfterS);
	N(TEXT("pull_gap_s"), R.PullGapS); N(TEXT("lift_s"), R.LiftS); N(TEXT("pull_s"), R.PullS); N(TEXT("fly_s"), R.FlyS); N(TEXT("interest_start_s"), R.InterestStartS);
	N(TEXT("interest_gap_s"), R.InterestGapS); N(TEXT("interest_fly_s"), R.InterestFlyS); N(TEXT("slip_s"), R.SlipS);
	double Ic = R.InterestCoins; N(TEXT("interest_coins"), Ic); R.InterestCoins = (int32)Ic;
	const TSharedPtr<FJsonObject> Ach = J->GetObjectField(TEXT("achievements"));
	A.JobMemo = Ach->GetStringField(TEXT("job_memo"));
	Ach->TryGetNumberField(TEXT("banner_s"), A.BannerS);
	for (const auto& V : Ach->GetArrayField(TEXT("list")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject(), W = O->GetObjectField(TEXT("when"));
		A.List.Add({ O->GetStringField(TEXT("id")), O->GetStringField(TEXT("name")), O->GetStringField(TEXT("line")), W->GetStringField(TEXT("fact")), (int64)W->GetNumberField(TEXT("at_least")) });
	}
	if (!R.Tiers.Num() || !A.List.Num()) { OutError = TEXT("no reward tiers or achievements"); return false; }
	return true;
}
