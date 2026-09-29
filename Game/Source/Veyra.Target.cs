// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class VeyraTarget : TargetRules
{
	public VeyraTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		ApplySharedVeyraTargetSettings(this);
		ApplyMonolithicVeyraTargetSettings(this);
	}

	/// <summary>
	/// The only settings every Veyra target shares (ADR-006 §2). Add nothing here that would
	/// force VeyraEditor into a unique build environment: on a source engine that rebuilds the
	/// whole engine.
	/// </summary>
	internal static void ApplySharedVeyraTargetSettings(TargetRules Target)
	{
		Target.DefaultBuildSettings = BuildSettingsVersion.V7;
		Target.IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		Target.ExtraModuleNames.Add("Veyra");
	}

	/// <summary>
	/// Settings for the Game, Client and Server targets only. Each is monolithic and already has
	/// its own build environment, so these cannot affect VeyraEditor, which must never receive
	/// them.
	/// </summary>
	internal static void ApplyMonolithicVeyraTargetSettings(TargetRules Target)
	{
		// Push-model replication (ADR-006 §5). The engine compiles it only into editor builds
		// unless a target asks for it.
		Target.bWithPushModel = true;

		// A monolithic Win64 link outgrew the linker's default 4 KB PDB pages (LNK1318, "LIMIT (12)"),
		// as UnrealBuildTool warns monolithic game builds may. Larger pages allow a larger PDB; the
		// option is the linker's alone, so nothing recompiles.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			Target.WindowsPlatform.PdbPageSize = 8192;
		}
	}
}
