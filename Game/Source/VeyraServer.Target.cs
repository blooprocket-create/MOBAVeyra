// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// Linux is the shipping server platform; the Win64 server is a local debugging convenience
// only (ADR-006 §2).
[SupportedPlatforms("Win64", "Linux")]
public class VeyraServerTarget : TargetRules
{
	public VeyraServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		VeyraTarget.ApplySharedVeyraTargetSettings(this);
		VeyraTarget.ApplyMonolithicVeyraTargetSettings(this);
	}
}
