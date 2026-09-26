// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class VeyraClientTarget : TargetRules
{
	public VeyraClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Client;
		VeyraTarget.ApplySharedVeyraTargetSettings(this);
		VeyraTarget.ApplyMonolithicVeyraTargetSettings(this);
	}
}
