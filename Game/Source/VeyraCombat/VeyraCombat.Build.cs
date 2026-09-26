// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// Reusable combat truth (ADR-006 §3, PROJECT_STRUCTURE.md "VeyraCombat"): attributes, the canonical
// damage/healing/shield pipeline, statuses and movement primitives. It never depends on a
// Vanguard, item, Flux, world objective or UI. It links the Gameplay Ability System because it owns
// the Attribute Sets and the damage execution (ADR-002).
public class VeyraCombat : ModuleRules
{
	public VeyraCombat(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"VeyraCore",
		});

		// Push-model replication for the attribute sets and the absorption ledger.
		PrivateDependencyModuleNames.Add("NetCore");

		// The Combat domain's tuning ships with every build that runs combat (ADR-006 §6).
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Combat.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Schemas/Combat.schema.json", StagedFileType.UFS);
	}
}
