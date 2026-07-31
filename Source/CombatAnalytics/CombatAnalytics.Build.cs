// YourGame.Build.cs
// Add "Json" and "JsonUtilities" to your existing module's PublicDependencyModuleNames
// (or PrivateDependencyModuleNames) so FJsonObject / FJsonSerializer resolve.
// This is a snippet to merge into your project's real Build.cs - not a standalone file.

using UnrealBuildTool;

public class CombatAnalytics : ModuleRules
{
	public CombatAnalytics(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
            "EnhancedInput",
            "Json",          // Required: FJsonObject, FJsonValue
			"JsonUtilities"  // Required: FJsonSerializer, TJsonWriter
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
