// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// The Economy domain (ARCHITECTURE.md §3, PROJECT_STRUCTURE.md "VeyraEconomy", ADR-008 §1). M5 brings
// its first owner, in-match Progression: XP, levels, skill points and ability ranks (Economy &
// Progression Bible §16). M7 adds Gold, in its own class so Gold and XP are never mixed, and the
// rewards that pay both (ADR-011 §11). It sits in its own layer above Combat, whose verbs apply
// level-up stat growth and whose deaths the rewards follow, and below Abilities, which reads ranks.
public class VeyraEconomy : ModuleRules
{
	public VeyraEconomy(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"VeyraCore",
			// Rewards follow Combat's deaths.
			"VeyraCombat",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GameplayAbilities",
			// Push-model replication for the progression component.
			"NetCore",
		});

		// The Progression and Economy tuning ship with every build that runs a match (ADR-006 §6).
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Progression.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Schemas/Progression.schema.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Economy.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Schemas/Economy.schema.json", StagedFileType.UFS);
	}
}
