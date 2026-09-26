// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// Match orchestration (ADR-006 §3, PROJECT_STRUCTURE.md "VeyraMatch"): teams, phases, spawn and
// respawn, score and victory, and the network entry points for player intent. The PlayerState owns
// each Vanguard's Ability System Component (ADR-006 §4); a server-side controller moves the Vanguard
// (ADR-006 §7).
public class VeyraMatch : ModuleRules
{
	public VeyraMatch(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AIModule",
			"GameplayAbilities",
			"VeyraCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"NavigationSystem",
			"NetCore",
			"VeyraCombat",
		});

		// The Match domain's tuning ships with every build that runs a match (ADR-006 §6).
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Match.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Schemas/Match.schema.json", StagedFileType.UFS);
	}
}
