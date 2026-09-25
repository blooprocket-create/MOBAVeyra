// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// Composition root (ADR-006 §3): module startup and default-class wiring only. Nothing depends
// on this module.
public class Veyra : ModuleRules
{
	public Veyra(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Core");
	}
}
