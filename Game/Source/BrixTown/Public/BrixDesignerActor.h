#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixDesigner.h"
#include "BrixWardrobe.h"
#include "BrixSave.h"
#include "BrixDesignerActor.generated.h"

class SBox;
class SScrollBox;
class SVerticalBox;
class SWrapBox;
class SWidget;

// The character designer (CHR-001, CHR-002, CHR-013; F-CHR-DESIGN level 2), as the slice's life.js openDesigner() and
// buildDesigner(): a new game opens it, a card down the left in the Lamplight look (ADR-0049) with the name, the body
// (height, build, skin), the face (width, eyes, brows, nose, mouth, eye colour), the hair (style, colour), the clothes
// (a top, a bottom, shoes, each in a colour of the palette), the extras (scarf, cap, apron, bag, moustache) and the walk.
// Every choice comes from World/rules/designer.json, and the character is shown live beside it, the camera close round
// them (drag to turn them). Begin starts the game as that character. The clock stands still while it is open.
// The character is the designer's values and nothing else (BrixWorld/BrixDesigner): BrixDesignLook makes their look.
// For test shots: -BrixDesigner opens it (a test shot otherwise starts as the default character, as the slice's ?auto=1),
// -BrixDesignSet=<key>:<value>;... sets values first (hair:afro;skin:7;height:1.8;cap:terra), -BrixDesignSpin=<deg> turns them,
// -BrixDesignBegin presses Begin after 3 s (the game's own -BrixShot is then taken in play), -BrixOutfit=<name>,... saves
// what is worn as outfits (then -BrixMenu=wardrobe shows them), -BrixSaveAs=<slot> saves the shot's game there 6 s in
// (a test shot saves nothing else), -BrixLoad=<slot> carries on from a slot at once, -BrixNoSave plays without saving.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixDesigner : public AActor
{
	GENERATED_BODY()

public:
	ABrixDesigner();
	void Start(const FString& InCityName);
	virtual void Tick(float DeltaSeconds) override;
	bool IsOpen() const { return bOpen; }
	const FBrixDesign& Design() const { return Look; }
	const FBrixDesignerRules& Rules() const { return D; }
	// what the player wears now changes (the wardrobe, CHR-014): the character is drawn so at once
	void SetDesign(const FBrixDesign& V);
	// the designer closed and the game begins, as this character (its name for the talks, CHR-001)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBegin, const FBrixDesign&);
	FOnBegin OnBegin;
	bool SetValue(const FString& Key, const FString& Value);
	// the game menu's Wardrobe page (CHR-014 to CHR-016; life.js wardrobeTab()): outfits, the clothes owned (the economy's
	// Clothes, bought at the mall's outlets) and what is worn, each change at once and free; Redraw draws the menu again
	void WardrobePage(TSharedRef<SVerticalBox> Page, const TMap<FString, int32>& Owned, TFunction<void()> Redraw);
	FString WardrobeHeadline() const;
	// while the Wardrobe page is open the character stands in full beside it (Page: the page, the character to its right)
	void WardrobeView(bool bOn, TSharedPtr<SWidget> Page = nullptr);
	FBrixWardrobe Wardrobe;                          // the outfits saved (CHR-015), kept in the save

	// Saving (QST-008; life.js save(), loadSave(), saveTab()): a save file a slot in Saved/BrixTown, the game saving itself
	// in the slot being played as it goes and on leaving; a new game starts clean; the start offers to carry on.
	void SavePage(TSharedRef<SVerticalBox> Page, TFunction<void()> Redraw, TFunction<void()> CloseMenu);
	FString SaveHeadline() const;
	bool SaveSlot(int32 Slot, FString& OutWhy);
	bool LoadSlot(int32 Slot, FString& OutWhy);
	TOptional<FBrixSaveGame> ReadSlot(int32 Slot) const;
	int32 PlayingSlot() const { return Slot; }
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;	const TMap<FString, FBrixGarment>& GarmentList() const { return Garments; }   // one of -BrixDesignSet's key:value, as the designer would set it

	UPROPERTY(Config) float PanelWidth = 440.f;      // px at 1600 x 900
	UPROPERTY(Config) float LabelWidth = 106.f;      // a row's label
	UPROPERTY(Config) float WardrobeWidth = 560.f;   // the Wardrobe page, the character beside it
	UPROPERTY(Config) float WardrobeCameraM = 3.6f;  // m off on the Wardrobe page, times height / 1.72: all of you shows
	UPROPERTY(Config) float WardrobeSpin = -18.f;    // degrees the character is turned on the Wardrobe page
	UPROPERTY(Config) float SpinPerPixel = .3f;      // degrees the character turns for a pixel dragged
	UPROPERTY(Config) float AutosaveSeconds = 15.f;  // life.js: the game saves itself this often while played
	UPROPERTY(Config) float ShotSeconds = 8.f;       // -BrixDesigner: real seconds after it opens that the test shot is taken

private:
	void Open();
	void Close();
	void Build();
	void Apply();
	void Begun();
	void Surprise();
	TSharedRef<SWidget> Foot();
	TSharedRef<SWidget> Row(const FString& Label, TSharedRef<SWidget> Content) const;
	TSharedRef<SWidget> Swatch(const FLinearColor& Fill, bool bOn, TFunction<void()> Fn, const FString& Mark = FString()) const;
	TSharedRef<SWidget> Chip(const FString& Label, bool bOn, TFunction<void()> Fn) const;
	TSharedRef<SWrapBox> Wrap(float Wide) const;
	TSharedRef<SWidget> Swatches(int32& Field, const TArray<FVector3f>& List, float Wide, TFunction<void()> Changed);
	TSharedRef<SWidget> Colours(FString& Field, bool bNone, float Wide, TFunction<void()> Changed);
	TSharedRef<SWidget> Chips(FString& Field, const TArray<TPair<FString, FString>>& Opts, float Wide, TFunction<void()> Changed);
	FString ExtraLabel(const TCHAR* Key) const;
	void WearRows(TSharedRef<SVerticalBox> V, float Wide, TFunction<void()> Changed, bool bHeads);
	float ShareLeftOf(const TSharedPtr<SWidget>& W) const;
	class ABrixCharacter* Me() const;

	FBrixDesignerRules D;
	FBrixDesign Look;
	bool bOpen = false, bStarted = false, bReady = false;
	double Spin = 0;
	bool bDrag = false;
	FVector2D DragAt = FVector2D::ZeroVector;
	double ShotAt = -1, BeginAt = -1;
	FString CityName, Msg;
	TSharedPtr<SWidget> Root;
	TSharedPtr<SBox> Body, FootBox;
	TSharedPtr<SScrollBox> Scroll;
	TMap<FString, FBrixGarment> Garments;
	FString OutfitName, WardMsg;
	bool bWardrobe = false;
	TSharedPtr<SWidget> WardrobeBox;
	FString SlotPath(int32 K) const;
	void UseSlot(int32 K);
	void CarryOn();                                  // the start: the playing slot's save, loaded
	bool CanSave() const;                            // a game is being played (not in a test shot, not while designing)
	int32 Slot = 1, Confirm = 0;
	bool bBegun = false, bOffer = false, bTransient = false;
	double SavedAt = 0, BegunAt = 0;
	bool bSavedAs = false;
	FString SaveMsg;
};
