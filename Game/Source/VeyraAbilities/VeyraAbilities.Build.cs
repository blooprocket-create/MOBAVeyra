// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// Reusable ability execution (ADR-006 §3, PROJECT_STRUCTURE.md "VeyraAbilities"): Veyra's layer over
// the Gameplay Ability System. Activation validation, the Commit point, costs, cooldowns and
// targeting live here once; a Vanguard's ability composes them. Combat owns the rules they call.
public class VeyraAbilities : ModuleRules
{
	public VeyraAbilities(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"VeyraCore",
			"VeyraCombat",
		});

		// Push-model replication for the cooldown ledger and the loadout.
		PrivateDependencyModuleNames.Add("NetCore");

		// The Abilities domain's tuning ships with every build that runs abilities (ADR-006 §6).
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Abilities.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Schemas/Abilities.schema.json", StagedFileType.UFS);
	}
}
