// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// The Vanguards content domain (ADR-008 §1, §2, §5; PROJECT_STRUCTURE.md "Vanguards"): Vanguard
// definitions from Vanguards.json, the passives the shared systems cannot express, and preparing a
// participant as its Vanguard. It sits in the Content layer, above Abilities, whose archetypes run
// its kits, and below Match, which orchestrates it.
public class VeyraVanguards : ModuleRules
{
	public VeyraVanguards(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"VeyraAbilities",
			"VeyraCombat",
			"VeyraCore",
			"VeyraEconomy",
			"VeyraWorld",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GameplayAbilities",
		});

		// The Vanguards tuning ships with every build that runs a match (ADR-006 §6).
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Vanguards.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Schemas/Vanguards.schema.json", StagedFileType.UFS);
	}
}
