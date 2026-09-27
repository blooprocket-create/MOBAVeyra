// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// Presentation (ADR-008 §1; PROJECT_STRUCTURE.md "VeyraUI"): the grey-box presentation and the HUD.
// It observes replicated gameplay state and draws it, and decides nothing. It sits in the
// Presentation layer, above every gameplay module, and no gameplay module depends on it. It is
// ClientOnly in Veyra.uproject, so servers neither build nor load it.
public class VeyraUI : ModuleRules
{
	public VeyraUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
			"VeyraAbilities",
			"VeyraCombat",
			"VeyraCore",
			"VeyraEconomy",
			"VeyraMatch",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GameplayAbilities",
			// The HUD names the player's ability keys.
			"InputCore",
			"RenderCore",
			// The HUD names each Vanguard's resource.
			"VeyraVanguards",
		});
	}
}
