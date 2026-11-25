// SND-019, SND-020, QST-009: which new ledger transactions are shown as coins, and which achievements a player's facts
// earn, as world.reward_events() and Tools/citysim/achievements.py say (Game/Data/life.json reward_checks, achievements.checks).
#include "BrixRewards.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool LoadRules(FAutomationTestBase& T, FBrixLifeData& Life, FBrixRewardRules& R, FBrixAchievementRules& A)
	{
		FString Error;
		if (!T.TestTrue(TEXT("life.json loads: ") + Error, LoadBrixLife(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/life.json"))), Life, Error))) return false;
		return T.TestTrue(TEXT("the rewards and achievements load: ") + Error, LoadBrixRewards(Life.Json, R, A, Error));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND019_MoneyComingInIsShown, "BrixTown.World.SND019_money_coming_in_is_shown_as_python_says",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND019_MoneyComingInIsShown::RunTest(const FString&)
{
	FBrixLifeData Life;
	FBrixRewardRules R;
	FBrixAchievementRules A;
	if (!LoadRules(*this, Life, R, A)) return false;
	const TSharedPtr<FJsonObject> C = Life.Json->GetObjectField(TEXT("reward_checks"));
	TArray<FBrixTx> Tx;
	for (const auto& V : C->GetArrayField(TEXT("tx")))
	{
		const TArray<TSharedPtr<FJsonValue>>& X = V->AsArray();
		FBrixTx& T = Tx.AddDefaulted_GetRef();
		T.No = (int32)X[0]->AsNumber(); T.Day = (int32)X[1]->AsNumber(); T.Minute = X[2]->AsNumber(); T.Memo = X[3]->AsString();
		for (const auto& L : X[4]->AsArray()) T.Lines.Add({ L->AsArray()[0]->AsString(), (int64)L->AsArray()[1]->AsNumber() });
	}
	const TArray<FBrixRewardEvent> Got = BrixRewardEvents(R, Tx);
	const TArray<TSharedPtr<FJsonValue>>& Want = C->GetArrayField(TEXT("want"));
	if (!TestEqual(TEXT("showings"), Got.Num(), Want.Num())) return false;
	for (int32 k = 0; k < Want.Num(); ++k)
	{
		const TSharedPtr<FJsonObject> W = Want[k]->AsObject();
		TestEqual(FString::Printf(TEXT("showing %d's kind"), k), Got[k].Kind, W->GetStringField(TEXT("kind")));
		TestEqual(FString::Printf(TEXT("showing %d's amount"), k), Got[k].Amount, (int64)W->GetNumberField(TEXT("amount")));
		if (Got[k].Kind == TEXT("reward"))
		{
			TestEqual(FString::Printf(TEXT("showing %d's coins (SND-019)"), k), Got[k].Coins, (int32)W->GetNumberField(TEXT("coins")));
			TestEqual(FString::Printf(TEXT("showing %d's memo"), k), Got[k].Memo, W->GetStringField(TEXT("memo")));
		}
		else
		{
			TArray<FString> Acc;
			for (const auto& V : W->GetArrayField(TEXT("accounts"))) Acc.Add(V->AsString());
			TestEqual(TEXT("interest's accounts (SND-020)"), FString::Join(Got[k].Accounts, TEXT(",")), FString::Join(Acc, TEXT(",")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQST009_AchievementsAreEarned, "BrixTown.World.QST009_achievements_are_earned_as_python_says",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FQST009_AchievementsAreEarned::RunTest(const FString&)
{
	FBrixLifeData Life;
	FBrixRewardRules R;
	FBrixAchievementRules A;
	if (!LoadRules(*this, Life, R, A)) return false;
	const TSharedPtr<FJsonObject> K = Life.Json->GetObjectField(TEXT("achievements"))->GetObjectField(TEXT("checks"));
	const TArray<TSharedPtr<FJsonValue>>& Steps = K->GetArrayField(TEXT("steps")), &Want = K->GetArrayField(TEXT("want"));
	TMap<FString, int32> Have;
	for (int32 Day = 0; Day < Steps.Num(); ++Day)
	{
		TMap<FString, int64> F;
		for (const auto& V : Steps[Day]->AsObject()->Values) F.Add(FString(V.Key.ToView()), (int64)V.Value->AsNumber());
		const TArray<FString> Got = BrixAward(A, F, Have, Day);
		TArray<FString> W;
		for (const auto& V : Want[Day]->AsArray()) W.Add(V->AsString());
		TestEqual(FString::Printf(TEXT("earned on day %d"), Day), FString::Join(Got, TEXT(",")), FString::Join(W, TEXT(",")));
	}
	const TSharedPtr<FJsonObject> WH = K->GetObjectField(TEXT("have"));
	TestEqual(TEXT("achievements had"), Have.Num(), WH->Values.Num());
	for (const auto& V : WH->Values) TestEqual(TEXT("the day of ") + FString(V.Key.ToView()), Have.FindRef(FString(V.Key.ToView())), (int32)V.Value->AsNumber());
	return true;
}

#endif
