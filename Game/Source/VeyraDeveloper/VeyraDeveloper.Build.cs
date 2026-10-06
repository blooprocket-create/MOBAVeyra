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
			// The front-end map test reads the default maps.
			"EngineSettings",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"InputCore",
			"Json",
			"NavigationSystem",
			"NetCore",
			"PhysicsCore",
			"Projects",
			"RenderCore",
			"RHI",
			"CQTest",
			"VeyraCore",
			"VeyraSettings",
			"VeyraCombat",
			"VeyraEconomy",
			"VeyraAbilities",
			"VeyraItems",
			"VeyraBots",
			"VeyraFlux",
			"VeyraWorld",
			"VeyraVision",
			"VeyraVanguards",
			"VeyraMatch",
			"VeyraServices",
		});

		// CQTest's networked PIE tests start play sessions from the level editor, so they exist
		// only in editor builds. This module also builds into the Development Client and Server.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				// The Vanguard art commandlet decodes the hero illustrations.
				"ImageCore",
				"ImageWrapper",
				"LevelEditor",
				"UnrealEd",
				// The effects commandlet builds Niagara systems from engine templates (ADR-063 §4); an editor is
				// never a server, so Niagara itself comes with the presentation tests below.
				"JsonUtilities",
				"NiagaraCore",
				"NiagaraEditor",
			});
		}

		// VeyraUI is client only (ADR-008 §1): servers neither build nor load it, so its tests exist
		// only in client and editor builds.
		if (Target.Type != TargetType.Server)
		{
			// The shell tests build the screens and menus, which are UMG widgets.
			PrivateDependencyModuleNames.Add("UMG");
			PrivateDependencyModuleNames.Add("VeyraUI");
			// The presentation tests check the fight's effects, which are Niagara systems (ADR-063 §4).
			PrivateDependencyModuleNames.Add("Niagara");
			PrivateDefinitions.Add("WITH_VEYRA_UI=1");
		}
		else
		{
			PrivateDefinitions.Add("WITH_VEYRA_UI=0");
		}

		SetupIrisSupport(Target);
	}
}
