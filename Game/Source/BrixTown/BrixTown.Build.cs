using UnrealBuildTool;

// The game: the town built from BrixWorld's data, the light, the player.
public class BrixTown : ModuleRules
{
	public BrixTown(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// each file on its own: with unity builds a file's own short names (a grid's G, a colour's Cream) clash with another
		// file's as soon as a new file changes which files are joined
		bUseUnity = false;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ProceduralMeshComponent", "BrixWorld" });
		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI", "Slate", "SlateCore", "VorbisAudioDecoder", "MeshDescription", "StaticMeshDescription" });   // the talk box, and the voices' Ogg Vorbis
		PrivateIncludePathModuleNames.AddRange(new string[] { "TargetPlatform" });   // FSoundQualityInfo   // the frame's thread and GPU times, logged with a test shot
	}
}
