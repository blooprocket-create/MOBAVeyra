// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// The Vision domain (ARCHITECTURE.md §3, PROJECT_STRUCTURE.md "VeyraVision", ADR-016): what each team
// and each player can see, Dense Fog, stealth against True Sight, and the fog gate that keeps what a
// player cannot see off their client. It sits in the Battleground layer beside VeyraFlux and
// VeyraWorld; peers meet only through Match. Everything below reads it through the visibility
// contract beside Combat's targeting.
public class VeyraVision : ModuleRules
{
	public VeyraVision(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"VeyraCore",
		});

		PublicDependencyModuleNames.Add("VeyraCombat");

		// A ward is a unit with its own Ability System Component (ADR-016 §6).
		PublicDependencyModuleNames.Add("GameplayAbilities");

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Projectiles and telegraphed areas are gated like units.
			"VeyraAbilities",
			// The fog gate's groups (ADR-006 §5).
			"NetCore",
			// A destroyed ward pays its destroyer (Vision Bible §8; ADR-016 §6).
			"VeyraEconomy",
		});

		SetupIrisSupport(Target);

		// The Vision tuning ships with every build that runs a match (ADR-006 §6).
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Vision.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Schemas/Vision.schema.json", StagedFileType.UFS);
	}
}
