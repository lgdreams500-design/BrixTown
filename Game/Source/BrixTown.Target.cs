using UnrealBuildTool;

public class BrixTownTarget : TargetRules
{
	public BrixTownTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "BrixWorld", "BrixTown" });
	}
}
