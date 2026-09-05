using UnrealBuildTool;
public class ThreeHearths : ModuleRules
{
    public ThreeHearths(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "Json", "HTTP" });
    }
}
