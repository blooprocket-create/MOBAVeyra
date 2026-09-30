// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// Player settings (ADR-024, PROJECT_STRUCTURE.md "VeyraSettings"): the registry of every setting the
// Settings screen offers, their two stores (device-local and account-level) and the change event the
// systems that apply them listen to. Presentation only: it never decides a match. It sits in the
// Preferences layer, above Foundation and below every module that applies a setting.
public class VeyraSettings : ModuleRules
{
	public VeyraSettings(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			// Key bindings are keys (ADR-024 §2).
			"InputCore",
			// Content IDs, provenance and the tuning dialect's binder.
			"VeyraCore",
		});

		// The account document is JSON.
		PrivateDependencyModuleNames.Add("Json");

		// The registry ships with every build that shows the Settings screen (ADR-024 §2).
		RuntimeDependencies.Add("$(ProjectDir)/Settings/Settings.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Settings/Settings.schema.json", StagedFileType.UFS);
	}
}
