// UI-005, UI-008, UI-009, UI-010: the game menu's pages, the settings and the controls, as World/rules/settings.json has them,
// checked in the Unreal build by the same rules as Tools/worldgen/test_menu.py (world.settings_problems()).
#include "BrixSettings.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TSharedPtr<FJsonObject> ReadRules()   // a fresh copy each time, so a test can break it
	{
		FString Text;
		TSharedPtr<FJsonObject> J;
		FFileHelper::LoadFileToString(Text, *FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/settings.json"))));
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J);
		return J;
	}
	bool Has(const TArray<FString>& Problems, const FString& Start, const FString& Also = FString())
	{
		return Problems.ContainsByPredicate([&](const FString& P) { return P.StartsWith(Start) && (Also.IsEmpty() || P.Contains(Also)); });
	}
	const EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUI009_SettingsAreValid, "BrixTown.World.UI009_settings_are_valid", Flags)
bool FUI009_SettingsAreValid::RunTest(const FString&)
{
	FBrixSettings S;
	FString Error;
	if (!TestTrue(TEXT("settings.json is sound: ") + Error, ParseBrixSettings(ReadRules(), S, Error))) return false;
	for (const TCHAR* G : { TEXT("graphics"), TEXT("audio"), TEXT("display"), TEXT("controls") })
		TestTrue(FString(TEXT("a group of settings: ")) + G, S.Groups.ContainsByPredicate([&](const TPair<FString, FString>& X) { return X.Key == G; }));
	// kept on the device: what is saved comes back, and a value out of range is left at its default
	S.Step(TEXT("graphics"), TEXT("field_of_view"), 3);
	S.Step(TEXT("audio"), TEXT("sound"));
	S.Step(TEXT("graphics"), TEXT("picture"));
	const FString Saved = S.Save();
	FBrixSettings T;
	ParseBrixSettings(ReadRules(), T, Error);
	T.Load(Saved);
	TestEqual(TEXT("a range comes back"), T.Number(TEXT("graphics"), TEXT("field_of_view")), S.Number(TEXT("graphics"), TEXT("field_of_view")));
	TestFalse(TEXT("a switch comes back"), T.On(TEXT("audio"), TEXT("sound")));
	TestEqual(TEXT("a choice comes back"), T.Find(TEXT("graphics"), TEXT("picture"))->Choice, FString(TEXT("simple")));
	T.Load(TEXT("{\"graphics\":{\"field_of_view\":500}}"));
	TestEqual(TEXT("a value out of range is not taken"), T.Number(TEXT("graphics"), TEXT("field_of_view")), S.Number(TEXT("graphics"), TEXT("field_of_view")));
	T.Step(TEXT("graphics"), TEXT("field_of_view"), 1000);
	TestEqual(TEXT("a range stops at its end"), T.Number(TEXT("graphics"), TEXT("field_of_view")), T.Find(TEXT("graphics"), TEXT("field_of_view"))->Max);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUI009_BadSettingIsCaught, "BrixTown.World.UI009_a_bad_setting_is_caught", Flags)
bool FUI009_BadSettingIsCaught::RunTest(const FString&)
{
	TSharedPtr<FJsonObject> J = ReadRules();
	J->GetObjectField(TEXT("groups"))->GetObjectField(TEXT("graphics"))->GetObjectField(TEXT("settings"))->GetObjectField(TEXT("field_of_view"))->SetNumberField(TEXT("default"), 200);
	J->GetObjectField(TEXT("groups"))->GetObjectField(TEXT("audio"))->GetObjectField(TEXT("settings"))->GetObjectField(TEXT("sound"))->SetStringField(TEXT("default"), TEXT("loud"));
	const TArray<FString> P = BrixSettingsProblems(J);
	TestTrue(TEXT("a range's default out of range is caught"), Has(P, TEXT("setting graphics.field_of_view")));
	TestTrue(TEXT("a switch's default that is not yes or no is caught"), Has(P, TEXT("setting audio.sound")));
	FBrixSettings S;
	FString Error;
	TestFalse(TEXT("broken settings are not used"), ParseBrixSettings(J, S, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUI008_EveryPlatformMapsEveryAction, "BrixTown.World.UI008_every_platform_maps_every_action", Flags)
bool FUI008_EveryPlatformMapsEveryAction::RunTest(const FString&)
{
	FBrixSettings S;
	FString Error;
	if (!TestTrue(TEXT("settings.json is sound: ") + Error, ParseBrixSettings(ReadRules(), S, Error))) return false;
	TArray<FString> Ps;
	S.Platforms.GetKeys(Ps);
	Ps.Sort();
	TestEqual(TEXT("the platforms"), FString::Join(Ps, TEXT(",")), FString(TEXT("gamepad,keyboard,touch")));
	for (const auto& P : S.Platforms)
		for (const auto& A : S.Actions)
			if (!P.Value.Contains(A.Key) || !P.Value[A.Key].Num()) AddError(P.Key + TEXT(" has nothing for ") + A.Key);
	TSharedPtr<FJsonObject> J = ReadRules();
	J->GetObjectField(TEXT("platforms"))->GetObjectField(TEXT("touch"))->GetObjectField(TEXT("controls"))->RemoveField(TEXT("help"));
	TestTrue(TEXT("an action with no control is caught"), Has(BrixSettingsProblems(J), TEXT("touch: nothing does help")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUI010_NoControlDoesTwoThings, "BrixTown.World.UI010_no_control_does_two_things", Flags)
bool FUI010_NoControlDoesTwoThings::RunTest(const FString&)
{
	TestEqual(TEXT("settings.json as it is"), BrixSettingsProblems(ReadRules()).Num(), 0);
	TSharedPtr<FJsonObject> J = ReadRules();
	J->GetObjectField(TEXT("platforms"))->GetObjectField(TEXT("keyboard"))->GetObjectField(TEXT("controls"))->SetArrayField(TEXT("map"), { MakeShared<FJsonValueString>(TEXT("E")) });
	TestTrue(TEXT("E on foot and the map anywhere clash"), Has(BrixSettingsProblems(J), TEXT("keyboard: E does"), TEXT("map")));
	J = ReadRules();
	J->GetObjectField(TEXT("platforms"))->GetObjectField(TEXT("gamepad"))->GetObjectField(TEXT("controls"))->SetArrayField(TEXT("drive"), { MakeShared<FJsonValueString>(TEXT("A")) });
	TestTrue(TEXT("A drives and brakes in a car: a clash"), Has(BrixSettingsProblems(J), TEXT("gamepad: A does")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUI005_EveryTabOpensFromAControl, "BrixTown.World.UI005_every_tab_opens_from_a_control_on_every_platform", Flags)
bool FUI005_EveryTabOpensFromAControl::RunTest(const FString&)
{
	FBrixSettings S;
	FString Error;
	if (!TestTrue(TEXT("settings.json is sound: ") + Error, ParseBrixSettings(ReadRules(), S, Error))) return false;
	for (const TCHAR* Want : { TEXT("me"), TEXT("wallet"), TEXT("belongings"), TEXT("property"), TEXT("quests"), TEXT("wardrobe"), TEXT("storage"), TEXT("map"), TEXT("settings"), TEXT("save"), TEXT("help") })
		TestTrue(FString(TEXT("a page: ")) + Want, S.Tabs.ContainsByPredicate([&](const FBrixMenuTab& T) { return T.Id == Want; }));
	for (const auto& P : S.Platforms)
		for (const FBrixMenuTab& T : S.Tabs)
			if (!P.Value.Contains(T.Action) || !P.Value[T.Action].Num()) AddError(P.Key + TEXT(": no control opens ") + T.Name);
	TestTrue(TEXT("the menu stops the clock (owner, 2026-11-23)"), S.bPauses);
	TestTrue(TEXT("more than one save slot (UI-013)"), S.SaveSlots >= 3);
	for (const FBrixMenuTab& T : S.Tabs) TestTrue(TEXT("every page is in a group (UI-015): ") + T.Id, S.MenuGroups.ContainsByPredicate([&](const TPair<FString, FString>& G) { return G.Key == T.Group; }));
	return true;
}

#endif
