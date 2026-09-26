// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// Match orchestration (ADR-006 §3, PROJECT_STRUCTURE.md "VeyraMatch"): teams, phases, spawn and
// respawn, score and victory. It arrives in M2 holding only the PlayerState, which owns each
// Vanguard's Ability System Component (ADR-006 §4); the GameMode and GameState follow in M3.
public class VeyraMatch : ModuleRules
{
	public VeyraMatch(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GameplayAbilities" });
		PrivateDependencyModuleNames.Add("VeyraCombat");
	}
}
