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
			"GameplayTags",
			"Json",
			"CQTest",
			"VeyraCore",
			"VeyraCombat",
		});
	}
}
