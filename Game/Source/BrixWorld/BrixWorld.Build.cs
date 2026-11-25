using UnrealBuildTool;

// The map as data (WLD-020): reads World/ into plain structs. Engine-light on purpose (TECH-003, TECH-004): it needs
// only Core and Json, so it can be tested headless and knows nothing about meshes, actors or rendering.
public class BrixWorld : ModuleRules
{
	public BrixWorld(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;   // each file on its own, so files' short names (a colour, a helper) cannot clash (as BrixTown)
		PublicDependencyModuleNames.AddRange(new string[] { "Core" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Json" });
	}
}
