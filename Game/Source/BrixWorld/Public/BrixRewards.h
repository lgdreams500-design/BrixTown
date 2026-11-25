#pragma once

#include "CoreMinimal.h"
#include "BrixEconomy.h"

class FJsonObject;
class FBrixQuests;

// Money coming in, shown (SND-019, SND-020; ADR-0043), and achievements (QST-009): the rules of the slice's rewards.js
// and achievements.js, themselves world.reward_events() and Tools/citysim/achievements.py. Nothing here moves money: the
// ledger has already paid; this says which new transactions are shown as coins, and which achievements a player's facts
// have earned. SND019_money_coming_in_is_shown_as_python_says and QST009_achievements_are_earned_as_python_says check them.

struct BRIXWORLD_API FBrixRewardRules
{
	FString RewardFrom = TEXT("sys:rewards"), InterestFrom = TEXT("bank:vault"), InterestMemo = TEXT("Interest");
	TArray<TPair<int64, int32>> Tiers;        // a reward of at least this much springs this many coins
	// how the coins move (sound.json rewards.coins): seconds, metres, metres a second
	double StartS = .38, GapS = .035, FromM = 1.05, RingM[2] = { 1.0, 1.7 }, UpMps[2] = { 4.2, 5.6 }, FallMps2 = 15, Bounce = .42, SizeM = .07, HoverM = .3;
	double PullAfterS = 1.5, PullGapS = .07, LiftS = .14, PullS = .34, FlyS = .45, InterestStartS = .55, InterestGapS = .13, InterestFlyS = .38, SlipS = 2.2;
	int32 InterestCoins = 6;
	int32 CoinsFor(int64 Amount) const;
};

struct BRIXWORLD_API FBrixRewardEvent
{
	FString Kind;                             // reward, interest
	int64 Amount = 0;
	int32 Coins = 0;
	FString Memo;
	TArray<FString> Accounts;                 // interest: the accounts paid, sorted
	TMap<FString, int64> To;                  // what each of the player's accounts got
};

// The showings for these transactions: each reward paid from the rewards account, and all the interest as one.
BRIXWORLD_API TArray<FBrixRewardEvent> BrixRewardEvents(const FBrixRewardRules& R, const TArray<FBrixTx>& Tx, int32 From = 0);

struct BRIXWORLD_API FBrixAchievement { FString Id, Name, Line, Fact; int64 AtLeast = 0; };
struct BRIXWORLD_API FBrixAchievementRules
{
	FString JobMemo = TEXT("Job:");
	double BannerS = 3.6;
	TArray<FBrixAchievement> List;
	const FBrixAchievement* Find(const FString& Id) const { return List.FindByPredicate([&](const FBrixAchievement& A) { return A.Id == Id; }); }
};

// The facts achievements are earned from, worked out from what the game keeps (achievements.py facts()). Without the
// property market (not brought over yet) homes rented and owned are 0.
BRIXWORLD_API TMap<FString, int64> BrixAchievementFacts(const FBrixAchievementRules& R, const FBrixEconomy& E, const FBrixQuests* Q);
// The achievements these facts earn that are not had yet, marked as had on Day; returns them in the list's order.
BRIXWORLD_API TArray<FString> BrixAward(const FBrixAchievementRules& R, const TMap<FString, int64>& Facts, TMap<FString, int32>& Have, int32 Day);

BRIXWORLD_API bool LoadBrixRewards(const TSharedPtr<FJsonObject>& Life, FBrixRewardRules& OutRewards, FBrixAchievementRules& OutAchievements, FString& OutError);
