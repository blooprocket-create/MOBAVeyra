using UnrealBuildTool;

public class VeyraWorldTools : ModuleRules
{
    public VeyraWorldTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "Landscape", "LandscapeEditor",
            "Json", "Projects", "AssetRegistry", "MeshDescription", "StaticMeshDescription", "PCG", "NavigationSystem", "ImageCore",
            "VeyraCore", "VeyraCombat", "VeyraWorld", "VeyraMatch"
        });
        // Editor review generation shares the client fog presentation; servers never load it.
        if (Target.Type != TargetType.Server)
        {
            PrivateDependencyModuleNames.Add("VeyraUI");
        }
    }
}
