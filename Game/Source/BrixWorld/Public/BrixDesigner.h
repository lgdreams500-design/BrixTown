#pragma once

#include "CoreMinimal.h"
#include "BrixBody.h"

class FJsonObject;

// The character designer (CHR-001, CHR-002, CHR-013; F-CHR-DESIGN level 2) and the wardrobe (CHR-014, CHR-015; F-CHR-WARDROBE
// level 1) as data. A character is the values of World/rules/designer.json and nothing else: the same body and head as every
// resident, changed by numbers (ART_DIRECTION rule C7), stored as a small JSON object, never as a mesh. BrixDesignLook is
// people.js design(): it turns those values into the look BrixBody draws. BrixLookProblems is world.look_problems(),
// BrixOutfitProblems world.outfit_problems(). Checked against the slice by CHR001_the_designer_makes_the_slices_look.

struct BRIXWORLD_API FBrixDesignerSlider
{
	FString Key, Label, Unit;
	double Min = 0, Max = 1, Step = .01, Default = 0;
};

// One character's designer values (designer.json "default" is one). An extra is empty when not worn.
struct BRIXWORLD_API FBrixDesign
{
	FString Name;
	TMap<FString, double> Sliders;               // height, build, face_w, eye_size, eye_open, brow, nose, lips
	int32 Skin = 0, HairColor = 0, Eyes = 0;     // swatches
	FString Hair, Garment, Top, BottomType, Bottom, Shoes, Walk;
	FString Scarf, Cap, Apron;                   // palette names, or empty for none
	bool bBag = false, bTash = false;
	double Slider(const FString& Key) const { const double* V = Sliders.Find(Key); return V ? *V : 0.0; }
	bool operator==(const FBrixDesign& O) const;
};

struct BRIXWORLD_API FBrixDesignerRules
{
	TArray<FBrixDesignerSlider> Sliders;         // in the file's order
	TArray<FVector3f> Skin, HairColor, Eyes;
	TArray<FString> Hair;
	TArray<TPair<FString, FString>> Garment, BottomType, Extras, Walk;   // value, label
	TArray<TPair<FString, FVector3f>> Palette;   // name, colour, in the file's order
	int32 NameMax = 16;
	FBrixDesign Default;
	TArray<FString> WardrobeParts;               // settings.json wardrobe.parts: what an outfit may change
	int32 MaxOutfits = 12;                       // settings.json wardrobe.max_outfits
	const FVector3f* Colour(const FString& Name) const;
	const FBrixDesignerSlider* SliderRule(const FString& Key) const;
	static bool Has(const TArray<TPair<FString, FString>>& List, const FString& Key);
};

// Reads World/rules/designer.json and the wardrobe of World/rules/settings.json. On failure false, and why.
BRIXWORLD_API bool LoadBrixDesigner(const FString& DesignerPath, const FString& SettingsPath, FBrixDesignerRules& Out, FString& OutError);

// people.js design(v, d): the look of a designed character
BRIXWORLD_API FBrixLook BrixDesignLook(const FBrixDesign& V, const FBrixDesignerRules& D);

// world.look_problems(d, v): what is wrong with one character's values, in its words; none when it can be played
BRIXWORLD_API TArray<FString> BrixLookProblems(const FBrixDesignerRules& D, const FBrixDesign& V);

// The values as designer.json writes them, and back. FromJson lays the fields the object has over InOut (an outfit has
// only some); false if a field has the wrong type.
BRIXWORLD_API TSharedRef<FJsonObject> BrixDesignToJson(const FBrixDesign& V);
BRIXWORLD_API bool BrixDesignFromJson(const FJsonObject& J, FBrixDesign& InOut);

// An outfit (CHR-014, CHR-015): the wardrobe's parts of a design, and a name
BRIXWORLD_API TSharedRef<FJsonObject> BrixOutfitOf(const FBrixDesignerRules& D, const FBrixDesign& V, const FString& Name);
// world.outfit_problems(w, outfit)
BRIXWORLD_API TArray<FString> BrixOutfitProblems(const FBrixDesignerRules& D, const FJsonObject& Outfit);
// put an outfit on: only its parts change (never the body or the name)
BRIXWORLD_API void BrixWearOutfit(const FBrixDesignerRules& D, const FJsonObject& Outfit, FBrixDesign& InOut);
