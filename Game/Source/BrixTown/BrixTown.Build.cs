using UnrealBuildTool;

// The game: the town built from BrixWorld's data, the light, the player.
public class BrixTown : ModuleRules
{
	public BrixTown(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ProceduralMeshComponent", "BrixWorld" });
		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI" });   // the frame's thread and GPU times, logged with a test shot
	}
}
