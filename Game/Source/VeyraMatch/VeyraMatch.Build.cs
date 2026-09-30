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
			"DeveloperSettings",
			"EnhancedInput",
			"GameplayAbilities",
			"InputCore",
			"VeyraAbilities",
			"VeyraCore",
			// Its PlayerController reports rank-up refusals.
			"VeyraEconomy",
			// ...and shop refusals; the PlayerState holds the inventory (ADR-012 §7).
			"VeyraItems",
			"VeyraVanguards",
			// ...and asks for vision-tool swaps; the PlayerState holds the tool (ADR-016 §6).
			"VeyraVision",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"NavigationSystem",
			"NetCore",
			"VeyraCombat",
			// The player's camera settings (ADR-024 §3); the camera reads them, never the server.
			"VeyraSettings",
			// The battleground's peers, which Match connects (ADR-011 §3).
			"VeyraFlux",
			"VeyraWorld",
		});

		// The Match domain's tuning ships with every build that runs a match (ADR-006 §6).
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Match.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Schemas/Match.schema.json", StagedFileType.UFS);
	}
}
