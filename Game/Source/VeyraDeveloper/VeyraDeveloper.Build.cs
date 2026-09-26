// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// Automation tests, debug commands and test harnesses (ADR-006 §3). A leaf: it may depend on
// any production module, and no production module may depend on it.
public class VeyraDeveloper : ModuleRules
{
	public VeyraDeveloper(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"InputCore",
			"Json",
			"NavigationSystem",
			"NetCore",
			"PhysicsCore",
			"CQTest",
			"VeyraCore",
			"VeyraCombat",
			"VeyraAbilities",
			"VeyraMatch",
		});

		// CQTest's networked PIE tests start play sessions from the level editor, so they exist
		// only in editor builds. This module also builds into the Development Client and Server.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"EngineSettings",
				"LevelEditor",
				"UnrealEd",
			});
		}

		SetupIrisSupport(Target);
	}
}
