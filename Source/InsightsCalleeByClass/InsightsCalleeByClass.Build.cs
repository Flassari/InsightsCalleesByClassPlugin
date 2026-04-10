using UnrealBuildTool;

public class InsightsCalleeByClass : ModuleRules
{
	public InsightsCalleeByClass(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"InputCore",
				"Slate",
				"SlateCore",
				"TraceInsights",
				"TraceInsightsCore",
				"TraceServices",
			}
		);
	}
}
