#pragma once

#include "CoreMinimal.h"
#include "BrixDesigner.h"

class FJsonObject;
class FJsonValue;
class FBrixEconomy;
class FBrixQuests;

// Saving the game (QST-008), as the slice's life.js save() and loadSave(): the character (their designer values), where
// they stand and the time of day, the economy as economy.py save() keeps it (the ledger's transactions, from which the
// balances are worked out again on loading, and the rest of its state), the quests as quests.py save() (lines under way and
// finished, the journal, a job, the jobs taken), the wardrobe's outfits and the achievements earned. A save is one JSON
// file per slot. Checked by QST008_a_save_brings_the_game_back.

// economy.py save() / load(): load posts the transactions again, so every balance comes from the ledger itself
BRIXWORLD_API TSharedRef<FJsonObject> BrixEconomySave(const FBrixEconomy& E);
BRIXWORLD_API bool BrixEconomyLoad(FBrixEconomy& E, const FJsonObject& S, FString& OutWhy);
// quests.py save() / load()
BRIXWORLD_API TSharedRef<FJsonObject> BrixQuestsSave(const FBrixQuests& Q);
BRIXWORLD_API bool BrixQuestsLoad(FBrixQuests& Q, const FJsonObject& S, FString& OutWhy);

// One slot's save (life.js save(): v, look, pl, clock, econ, quests, estate, remodels, outfits, feats, at)
struct BRIXWORLD_API FBrixSaveGame
{
	int32 Version = 1;
	FBrixDesign Look;
	FVector Pos = FVector::ZeroVector;          // the slice's frame: metres, x east, y up, z south
	double Yaw = 0;                              // radians, the slice's
	int32 Day = 0;                               // the economy's day since the game began
	double Minute = 0;                           // the time of day
	TSharedPtr<FJsonObject> Econ, Quests;
	TSharedPtr<FJsonObject> Estate;              // estate.js save(): the property market (PROP-021); none in a save from before it
	TSharedPtr<FJsonObject> Remodels;            // life.js remodels: lot id -> the building's design as remodelled and paid for (BUILD-005)
	TArray<TSharedPtr<FJsonValue>> Outfits;
	TMap<FString, int32> Feats;                  // achievement -> the day it was earned (QST-009)
	FDateTime At;                                // when it was saved (UTC)
	FString ToString() const;
	// false, and why, when the text is not a save this game can carry on from (life.js lookOk() of its character too)
	bool FromString(const FString& Text, const FBrixDesignerRules& D, FString& OutWhy);
};
