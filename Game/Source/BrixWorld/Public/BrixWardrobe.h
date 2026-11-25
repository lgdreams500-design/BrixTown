#pragma once

#include "CoreMinimal.h"
#include "BrixDesigner.h"

class FJsonObject;

// The wardrobe (CHR-014 to CHR-016; F-CHR-WARDROBE level 1, with the owned clothes of level 2 the owner asked for), as the
// slice's life.js wardrobeTab(): what the player wears changes for free, from the designer's choices; outfits are saved by
// name, up to settings.json wardrobe.max_outfits; clothes bought at the mall's outlets are owned (the economy's Clothes)
// and put on, each setting only the parts of the look it names (World/rules/mall_goods.json "parts").
struct BRIXWORLD_API FBrixWardrobe
{
	TArray<TSharedRef<FJsonObject>> Outfits;     // each the wardrobe's parts and a name (BrixOutfitOf), in the order saved

	// CHR-015: keep what V wears as Name, in place of one of the same name; false, and why, when it cannot
	bool Save(const FBrixDesignerRules& D, const FBrixDesign& V, const FString& Name, FString& OutWhy);
	bool Forget(const FString& Name);
	const FJsonObject* Find(const FString& Name) const;
	TArray<FString> Names() const;
	// kept in the save as a list of outfits; Load keeps only the sound ones
	TArray<TSharedPtr<class FJsonValue>> ToJson() const;
	void Load(const FBrixDesignerRules& D, const TArray<TSharedPtr<class FJsonValue>>& List);
};

// A good the outlets sell to wear (ECO-014: "goes": "wear")
struct BRIXWORLD_API FBrixGarment
{
	FString Id, Name;
	TSharedPtr<FJsonObject> Parts;               // the parts of the look it sets
};

// The goods to wear of Game/Data/life.json (its goods.goods with goes "wear"), by id
BRIXWORLD_API bool LoadBrixGarments(const FString& LifePath, TMap<FString, FBrixGarment>& Out, FString& OutError);

// CHR-016: what is wrong with a garment's parts: only the wardrobe's parts, each a value the designer offers
BRIXWORLD_API TArray<FString> BrixGarmentProblems(const FBrixDesignerRules& D, const FBrixGarment& G);
// whether V wears it now (every part it names is as it says)
BRIXWORLD_API bool BrixIsWearing(const FBrixDesign& V, const FBrixGarment& G);
// what it is, in the designer's words (life.js partWords()): its garment, bottom and bag, as "T-shirt", "Skirt"
BRIXWORLD_API TArray<FString> BrixGarmentWords(const FBrixDesignerRules& D, const FBrixGarment& G);
// put it on: its parts and nothing else
BRIXWORLD_API void BrixPutOn(const FBrixGarment& G, FBrixDesign& V);
