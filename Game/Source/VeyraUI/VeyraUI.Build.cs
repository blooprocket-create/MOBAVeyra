// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// Presentation (ADR-008 §1, ADR-010 §4; PROJECT_STRUCTURE.md "VeyraUI"): the grey-box presentation,
// the HUD, the shell's screens and the in-match menu. It observes replicated gameplay state and the
// client-state coordinator's snapshot, asks through the coordinator's intents, and decides nothing.
// Its menus are UMG widgets built in C++, with no widget Blueprints. It sits in the Presentation
// layer, above every gameplay module and VeyraServices, and no gameplay module depends on it. It is
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
			"InputCore",
			"UMG",
			"VeyraAbilities",
			"VeyraCombat",
			"VeyraCore",
			"VeyraEconomy",
			"VeyraMatch",
			"VeyraServices",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// The in-match menu's key, as an input action built at runtime.
			"EnhancedInput",
			"GameplayAbilities",
			"RenderCore",
			"Slate",
			"SlateCore",
			// The HUD names each Vanguard's resource, and the text checks cover each released Vanguard.
			"VeyraVanguards",
			// The grey-box draws the battleground's lanes, river and bases from its layout.
			"VeyraWorld",
		});

		// What players read about Vanguards, abilities and passives (VeyraContentText).
		RuntimeDependencies.Add("$(ProjectDir)/Text/VeyraText.csv", StagedFileType.UFS);
	}
}
