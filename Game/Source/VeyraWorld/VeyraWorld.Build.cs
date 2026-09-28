// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// The World domain (ARCHITECTURE.md §3, PROJECT_STRUCTURE.md "VeyraWorld", ADR-011 §2): the
// battleground's layout, its lanes, structures and Fluxborn. It sits in the Battleground layer
// beside VeyraFlux, above the systems it composes; world actors report outcomes to their owners.
public class VeyraWorld : ModuleRules
{
	public VeyraWorld(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"VeyraCore",
			"VeyraCombat",
			// Fluxborn attack with the basic attack component and profile Vanguards use, and tower
			// shots fly as the homing projectiles abilities use (ADR-011 §2, §7, §8).
			"VeyraAbilities",
			// The Fluxborn's server controllers walk the navigation mesh (ADR-011 §7).
			"AIModule",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Push-model replication for the structures and Fluxborn.
			"NetCore",
			"NavigationSystem",
		});

		// The World tuning, the battleground's layout among it, ships with every build that runs a
		// match (ADR-006 §6).
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/World.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Schemas/World.schema.json", StagedFileType.UFS);
	}
}
