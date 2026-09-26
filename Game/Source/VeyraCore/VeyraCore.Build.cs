// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// Lowest Veyra-owned foundation (ADR-006 §3): log categories, the native Gameplay Tag
// vocabulary, the tuning-data framework (§6) and domain-neutral contracts. It depends on no
// other Veyra module.
public class VeyraCore : ModuleRules
{
	public VeyraCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "GameplayTags" });

		// RapidJSON parses tuning files; the Json module exposes it.
		PrivateDependencyModuleNames.Add("Json");
	}
}
