// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// The Flux domain (ARCHITECTURE.md §3, PROJECT_STRUCTURE.md "VeyraFlux", ADR-011 §2): each team's
// authoritative Team Flux, its permanent total and temporary grants, and the Fluxborn strength it
// gives. It sits in the Battleground layer beside VeyraWorld; the two meet only through Match.
public class VeyraFlux : ModuleRules
{
	public VeyraFlux(ReadOnlyTargetRules Target) : base(Target)
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
			// Push-model replication for the Team Flux state.
			"NetCore",
		});

		// The Flux tuning ships with every build that runs a match (ADR-006 §6).
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Flux.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Schemas/Flux.schema.json", StagedFileType.UFS);
	}
}
