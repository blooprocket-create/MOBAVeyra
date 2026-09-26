// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// Reusable combat truth (ADR-006 §3, PROJECT_STRUCTURE.md "VeyraCombat"): attributes, the canonical
// damage/healing/shield pipeline, statuses and movement primitives. It never depends on a
// Vanguard, item, Flux, world objective or UI.
public class VeyraCombat : ModuleRules
{
	public VeyraCombat(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject" });
	}
}
