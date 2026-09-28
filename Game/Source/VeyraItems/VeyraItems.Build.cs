// Copyright © 2026 Wayfinder Studios. All rights reserved.

using UnrealBuildTool;

// The Items domain (ARCHITECTURE.md §3, PROJECT_STRUCTURE.md "VeyraItems", ADR-012 §2): item
// definitions and their tier rules, each Vanguard's inventory and pending purchases, and the shop's
// transactions, which spend and refund Gold through Economy and apply equipment through Combat. It
// sits in its own Items layer above Abilities, whose archetypes run item Actives, and below
// Battleground; Match routes the fountain and the player's requests to it.
public class VeyraItems : ModuleRules
{
	public VeyraItems(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"VeyraCore",
			// Equipment is applied as Combat's equipment stats.
			"VeyraCombat",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GameplayAbilities",
			// Push-model replication for the inventory.
			"NetCore",
			// Item slots hold their items' Actives, which Abilities' archetypes run (ADR-012 §1).
			"VeyraAbilities",
			// Purchases spend and refund Gold; bonus Attack Speed is a fraction of the base progression keeps.
			"VeyraEconomy",
		});

		// The Items tuning ships with every build that runs a match (ADR-006 §6).
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Items.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Tuning/Schemas/Items.schema.json", StagedFileType.UFS);
	}
}
