// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// The Economy domain (ARCHITECTURE.md §3, PROJECT_STRUCTURE.md "VeyraEconomy", ADR-008 §1). M5 brings
// its first owner, in-match Progression: XP, levels, skill points and ability ranks (Economy &
// Progression Bible §16). Gold arrives with its first feature, in its own class: Gold and XP are
// never mixed. It sits in its own layer above Combat, whose verbs apply level-up stat growth, and
// below Abilities, which reads ranks.
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
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GameplayAbilities",
			// Push-model replication for the progression component.
			"NetCore",
			"VeyraCombat",
		});

		// The Progression tuning ships with every build that runs a match (ADR-006 §6).
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Progression.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Schemas/Progression.schema.json", StagedFileType.UFS);
	}
}
