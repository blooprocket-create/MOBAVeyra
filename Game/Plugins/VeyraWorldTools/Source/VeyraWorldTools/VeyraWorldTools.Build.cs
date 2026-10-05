using UnrealBuildTool;

public class VeyraWorldTools : ModuleRules
{
    public VeyraWorldTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "Landscape", "LandscapeEditor",
            "Json", "Projects", "AssetRegistry", "MeshDescription", "StaticMeshDescription", "PCG", "NavigationSystem",
            "VeyraCore", "VeyraCombat", "VeyraWorld", "VeyraMatch"
        });
    }
}
