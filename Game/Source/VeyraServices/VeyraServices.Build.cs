// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// The trusted-services client (ADR-006 §3, ADR-007): the only Veyra module that talks to the
// backend. A client reads its launch code from standard input, signs in and joins its assigned
// match; a match server reads its assignment from standard input and reports that it is ready and
// how its match ended. It plugs into VeyraMatch's contracts, so no gameplay module knows HTTP.
public class VeyraServices : ModuleRules
{
	public VeyraServices(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"VeyraMatch",
		});
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EngineSettings",
			"HTTP",
			"Json",
			"VeyraCore",
		});

		// A match server validates its assignment against this schema (ADR-007 §5).
		RuntimeDependencies.Add("$(ModuleDir)/Schemas/MatchAssignment.schema.json", StagedFileType.UFS);
	}
}
