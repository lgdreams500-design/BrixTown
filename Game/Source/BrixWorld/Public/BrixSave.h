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

// boards.js save() / load() (CITY-100): the boards owned, the one in use and whether it is ridden; the kind in use of a save
// (empty when it has none, as a save from before the skate store)
BRIXWORLD_API TSharedRef<FJsonObject> BrixBoardsSave(const TArray<FString>& Own, const FString& InUse, bool bRiding);
BRIXWORLD_API FString BrixBoardsInUse(const TSharedPtr<FJsonObject>& Boards);

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
	TSharedPtr<FJsonObject> Plans;               // life.js plans: lot id -> storey -> its inner walls as replanned and paid for (BUILD-021)
	TSharedPtr<FJsonObject> Store;               // store.js save(): the furniture outlet's bookings, what has arrived, its catalogue (FURN-025 to FURN-029)
	TSharedPtr<FJsonObject> Furnishings;         // life.js furnishings: lot id -> storey -> its pieces as placed and paid for (FURN-051)
	TArray<TSharedPtr<FJsonValue>> Furniture;    // life.js belongings: the pieces taken away, to place again (INV-001)
	TSharedPtr<FJsonObject> Remodels;            // life.js remodels: lot id -> the building's design as remodelled and paid for (BUILD-005)
	TArray<TSharedPtr<FJsonValue>> Outfits;
	TMap<FString, int32> Feats;                  // achievement -> the day it was earned (QST-009)
	TSharedPtr<FJsonObject> Boards;              // boards.js save(): own, kind (the one in use), riding (CITY-100); the economy keeps who owns what
	FDateTime At;                                // when it was saved (UTC)
	FString ToString() const;
	// false, and why, when the text is not a save this game can carry on from (life.js lookOk() of its character too)
	bool FromString(const FString& Text, const FBrixDesignerRules& D, FString& OutWhy);
};
