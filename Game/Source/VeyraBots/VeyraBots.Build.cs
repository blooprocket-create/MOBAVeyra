// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// AI Vanguards (ADR-013, PROJECT_STRUCTURE.md "VeyraBots"): the brains that play bots' Vanguards
// through the players' order paths, and the data that says how. It sits in its own Autonomy layer
// above Match, which announces bots and never names their brain; nothing depends on it. Bots think
// only on the server.
public class VeyraBots : ModuleRules
{
	public VeyraBots(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"VeyraCore",
			// Bots are Match's participants, seated and announced by it, and order through it.
			"VeyraMatch",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GameplayAbilities",
			// What a bot sees and does: vitals and targeting, abilities and attacks, Gold and ranks,
			// the shop, the lanes and structures, and the Vanguards it plays.
			"VeyraCombat",
			"VeyraAbilities",
			"VeyraEconomy",
			"VeyraItems",
			"VeyraWorld",
			"VeyraVanguards",
		});

		// The Bots tuning ships with every build that runs a match (ADR-006 §6).
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Bots.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Schemas/Bots.schema.json", StagedFileType.UFS);
	}
}
