// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class VeyraEditorTarget : TargetRules
{
	public VeyraEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;

		// ADR-006 §2: the editor shares the installed engine's build environment. Do not add
		// settings that differ from UnrealEditor; UBT rejects the target instead of rebuilding.
		BuildEnvironment = TargetBuildEnvironment.Shared;

		VeyraTarget.ApplySharedVeyraTargetSettings(this);
	}
}
