using UnrealBuildTool;

public class BrixTownEditorTarget : TargetRules
{
	public BrixTownEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "BrixWorld", "BrixTown" });
	}
}
