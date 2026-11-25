#pragma once

#include "CoreMinimal.h"

class FJsonObject;

// The game menu's pages and the settings and controls (UI-005, UI-008 to UI-010, UI-013, UI-015, UI-016): World/rules/settings.json
// read as it is, checked by the rules of world.settings_problems() (Tools/worldgen/world.py), line for line. The values the
// player picks are kept on the device (Saved/BrixTown/settings.json), not in a save (UI-009).

struct BRIXWORLD_API FBrixSetting
{
	FString Group, Key, Name, Type;           // type: switch, choice, range
	TArray<TPair<FString, FString>> Options;  // a choice's: value, words
	double Min = 0, Max = 0, Step = 1;
	// the default and the value now: a switch 0/1, a choice its value, a range its number
	FString DefaultChoice, Choice;
	double Default = 0, Value = 0;
	bool bDefaultIsBool = false, bDefaultIsNumber = false;
};

struct BRIXWORLD_API FBrixMenuTab { FString Id, Name, Action, Group, Colour, Board = TEXT("big"); };

struct BRIXWORLD_API FBrixSettings
{
	TSharedPtr<FJsonObject> Json;             // the file as read; Problems() reads this, so a broken copy can be checked
	TArray<TPair<FString, FString>> Groups;   // id, name, in the file's order
	TArray<FBrixSetting> Settings;            // in the file's order
	TArray<FBrixMenuTab> Tabs;
	TArray<TPair<FString, FString>> MenuGroups;   // you, town, game
	TMap<FString, TPair<FColor, FColor>> Colours;
	TMap<FString, FColor> Lamplight;          // menu.lamplight: the cards' colours (ADR-0049); the veil's alpha in Lamplight["veil"].A
	TArray<TPair<FString, FString>> Contexts, Actions;   // id, words; an action's context in ActionContext
	TMap<FString, FString> ActionContext;
	TMap<FString, TMap<FString, TArray<FString>>> Platforms;   // platform -> action -> controls
	TMap<FString, FString> PlatformNames;
	bool bPauses = true;
	int32 SaveSlots = 1;

	FBrixSetting* Find(const FString& Group, const FString& Key);
	const FBrixSetting* Find(const FString& Group, const FString& Key) const;
	double Number(const FString& Group, const FString& Key) const;      // a range's value, a switch's 0 or 1
	bool On(const FString& Group, const FString& Key) const { return Number(Group, Key) > .5; }
	// Change one setting: a switch flips, a choice goes to the next, a range moves by Steps steps within its bounds.
	void Step(const FString& Group, const FString& Key, int32 Steps = 1);
	void Reset();                             // every setting to its default
	FString Save() const;                     // the device's file
	void Load(const FString& Text);           // unknown or bad values are left at their default
};

BRIXWORLD_API bool LoadBrixSettings(const FString& Path, FBrixSettings& Out, FString& OutError);
BRIXWORLD_API bool ParseBrixSettings(const TSharedPtr<FJsonObject>& Json, FBrixSettings& Out, FString& OutError);
// world.settings_problems(): every rule settings.json must keep. Empty when sound.
BRIXWORLD_API TArray<FString> BrixSettingsProblems(const TSharedPtr<FJsonObject>& Json);
