// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Engine/Engine.h"
#include "Tests/Items/VeyraItemsTestCatalog.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraItemsTests
{
	bool HasProblem(const TArray<FString>& Problems, const TCHAR* Prefix)
	{
		return Problems.ContainsByPredicate([Prefix](const FString& Problem) { return Problem.StartsWith(Prefix); });
	}

	// Veyra.Items.Catalog.*: the tier rules and the costs (Item Bible §2, §11; ADR-012 §3).
	TEST_CLASS(Catalog, "Veyra.Items")
	{
		TEST_METHOD(AConsistentCatalogPasses)
		{
			const TArray<FString> Problems = VeyraItems::Validate(TestCatalog());
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), FString::Join(Problems, TEXT(" | "))));
		}

		TEST_METHOD(TheTierRulesAreEnforced)
		{
			FVeyraItemsTuning Broken = TestCatalog();
			Broken.Items[ItemId(TEXT("test_grip"))].Components = { ItemId(TEXT("test_plate")) };
			Broken.Items[ItemId(TEXT("test_harness"))].Attunement = { ItemId(TEXT("test_weight")) };
			Broken.Items[ItemId(TEXT("test_temper"))].Attunement.Reset();
			const TArray<FString> Problems = VeyraItems::Validate(Broken);
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/items/test_grip/components")), FString::Join(Problems, TEXT(" | "))));
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/items/test_harness/attunement")), TEXT("no Attunement below a Masterwork")));
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/items/test_temper/attunement")), TEXT("a Masterwork has one")));
		}

		TEST_METHOD(AMythicalCarriesExactlyTwoAttunements)
		{
			FVeyraItemsTuning Mythicals = WithMythicals(TestCatalog());
			const TArray<FString> Problems = VeyraItems::Validate(Mythicals);
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), FString::Join(Problems, TEXT(" | "))));
			Mythicals.Items[ItemId(TEXT("test_harbor"))].Attunement.Pop();
			ASSERT_THAT(IsTrue(HasProblem(VeyraItems::Validate(Mythicals), TEXT("/items/test_harbor/attunement")), TEXT("one is too few (ADR-025 §2)")));
			Mythicals.Items[ItemId(TEXT("test_harbor"))].Attunement = { ItemId(TEXT("test_weight")), ItemId(TEXT("test_charge")), ItemId(TEXT("test_weight")) };
			ASSERT_THAT(IsTrue(HasProblem(VeyraItems::Validate(Mythicals), TEXT("/items/test_harbor/attunement")), TEXT("and three too many")));
		}

		TEST_METHOD(AQuestEvolvesItsItemIntoAnotherTheShopNeverSells)
		{
			const FVeyraContentId Reclaimer = ItemId(TEXT("test_reclaimer"));
			const FVeyraContentId Reservoir = ItemId(TEXT("test_reservoir"));
			const FVeyraItemsTuning Quest = WithQuest(TestCatalog());
			const TArray<FString> Problems = VeyraItems::Validate(Quest);
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), FString::Join(Problems, TEXT(" | "))));
			ASSERT_THAT(IsTrue(VeyraItems::IsEvolutionOnly(Quest, Reservoir) && !VeyraItems::IsEvolutionOnly(Quest, Reclaimer)));
			ASSERT_THAT(IsTrue(VeyraItems::QuestLine(Quest, Reservoir) == Reclaimer && VeyraItems::QuestLine(Quest, Reclaimer) == Reclaimer));
			ASSERT_THAT(IsFalse(VeyraItems::QuestLine(Quest, ItemId(TEXT("test_grip"))).IsValid()));
			ASSERT_THAT(IsTrue(VeyraItems::TotalCost(Quest, Reservoir) == 450.0, TEXT("its Gold is its base form's (ADR-025 §3)")));
			ASSERT_THAT(IsTrue(VeyraItems::TotalCost(Quest, ItemId(TEXT("test_haven"))) == 450.0 + 400.0 + 500.0));

			FVeyraItemsTuning Priced = Quest;
			Priced.Items[Reservoir].Cost = 100.0;
			ASSERT_THAT(IsTrue(HasProblem(VeyraItems::Validate(Priced), TEXT("/items/test_reservoir/cost")), TEXT("an evolution costs nothing")));
			FVeyraItemsTuning TwoPassives = Quest;
			TwoPassives.Items[Reservoir].Attunement.Add(ItemId(TEXT("test_weight")));
			ASSERT_THAT(IsTrue(HasProblem(VeyraItems::Validate(TwoPassives), TEXT("/items/test_reservoir/attunement")), TEXT("one passive at most")));
			FVeyraItemsTuning IntoEquipment = Quest;
			IntoEquipment.Quests[Reclaimer].EvolvesInto = ItemId(TEXT("test_grip"));
			ASSERT_THAT(IsTrue(HasProblem(VeyraItems::Validate(IntoEquipment), TEXT("/quests/test_reclaimer/evolvesInto")), TEXT("into another Quest Item")));
			FVeyraItemsTuning NoQuest = Quest;
			NoQuest.Quests.Reset();
			const TArray<FString> Orphaned = VeyraItems::Validate(NoQuest);
			ASSERT_THAT(IsTrue(HasProblem(Orphaned, TEXT("/items/test_reclaimer/category")), FString::Join(Orphaned, TEXT(" | "))));
			ASSERT_THAT(IsTrue(HasProblem(Orphaned, TEXT("/items/test_reservoir/components")), TEXT("with no quest, the reservoir needs a recipe")));
		}

		TEST_METHOD(RecipesNeverLoopAndBootsStopAtTierTwo)
		{
			FVeyraItemsTuning Broken = TestCatalog();
			Broken.Items[ItemId(TEXT("test_harness"))].Components.Add(ItemId(TEXT("test_temper")));
			Broken.Items[ItemId(TEXT("test_temper"))].Category = EVeyraItemCategory::Boots;
			const TArray<FString> Problems = VeyraItems::Validate(Broken);
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/items/test_harness/components/2")), FString::Join(Problems, TEXT(" | "))));
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/items/test_temper/tier"))));
		}

		TEST_METHOD(OnlyConsumablesStackAndEachHasItsEntry)
		{
			FVeyraItemsTuning Broken = TestCatalog();
			Broken.Items[ItemId(TEXT("test_grip"))].StackLimit = 2;
			Broken.Consumables.Remove(ItemId(TEXT("test_tonic")));
			const TArray<FString> Problems = VeyraItems::Validate(Broken);
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/items/test_grip/stackLimit")), FString::Join(Problems, TEXT(" | "))));
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/items/test_tonic/category"))));
		}

		TEST_METHOD(ARefillableConsumableDoesNotStack)
		{
			FVeyraItemsTuning Broken = TestCatalog();
			Broken.Consumables[ItemId(TEXT("test_tonic"))].Charges = 2;
			const TArray<FString> Problems = VeyraItems::Validate(Broken);
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/items/test_tonic/stackLimit")), FString::Join(Problems, TEXT(" | "))));
			Broken.Items[ItemId(TEXT("test_tonic"))].StackLimit = 1;
			ASSERT_THAT(IsTrue(VeyraItems::Validate(Broken).IsEmpty(), TEXT("held one at a time, it is valid")));
		}

		TEST_METHOD(FractureNeverRemovesAllMagicResistance)
		{
			FVeyraItemsTuning Broken = TestCatalog();
			const FVeyraContentId Fracture = ItemId(TEXT("test_fracture"));
			Broken.Items[ItemId(TEXT("test_temper"))].Attunement = { Fracture };
			Broken.WeightOfWar.Reset();
			FVeyraStackingAttunementTuning& Stacks = Broken.Fracture.Add(Fracture);
			Stacks.PerStack = 0.25;
			Stacks.MaxStacks = 4;
			Stacks.DurationSeconds = 1.0;
			const TArray<FString> Problems = VeyraItems::Validate(Broken);
			ASSERT_THAT(IsTrue(HasProblem(Problems, TEXT("/fracture/test_fracture/perStack")), FString::Join(Problems, TEXT(" | "))));
			Stacks.MaxStacks = 3;
			ASSERT_THAT(IsTrue(VeyraItems::Validate(Broken).IsEmpty()));
		}

		TEST_METHOD(TotalCostCountsEveryComponentAllTheWayDown)
		{
			const FVeyraItemsTuning Tuning = TestCatalog();
			ASSERT_THAT(IsTrue(VeyraItems::TotalCost(Tuning, ItemId(TEXT("test_grip"))) == 350.0));
			ASSERT_THAT(IsTrue(VeyraItems::TotalCost(Tuning, ItemId(TEXT("test_harness"))) == 350.0 + 350.0 + 400.0));
			ASSERT_THAT(IsTrue(VeyraItems::TotalCost(Tuning, ItemId(TEXT("test_temper"))) == 600.0 + 1100.0 + 400.0));
		}

		TEST_METHOD(TheRevisionsItemsKeepTheBiblesRecipesAndStats)
		{
			// Item Bible §4, §6 and §8 (2026-09-30): each new item's recipe and the stats its identity names. Their
			// amounts are provisional tuning (ADR-025 §8); which stats they are is canon.
			struct FExpected
			{
				const TCHAR* Item;
				TArray<FString> Components;
				TArray<FString> Stats;
			};
			const FExpected Revision[] = {
				{ TEXT("warforged_grip"), {}, { TEXT("PhysicalPower") } },
				{ TEXT("titansteel_grip"), {}, { TEXT("PhysicalPower") } },
				{ TEXT("marchplate"), {}, { TEXT("Armor") } },
				{ TEXT("shatterdeep_crystal"), {}, { TEXT("MagicResist") } },
				{ TEXT("picket_plating"), { TEXT("marchplate"), TEXT("vital_plate") }, { TEXT("Armor"), TEXT("Health") } },
				{ TEXT("canyonward"), { TEXT("shatterdeep_crystal"), TEXT("vital_plate") }, { TEXT("Health"), TEXT("MagicResist") } },
				{ TEXT("breaker_aegis"), { TEXT("marchplate"), TEXT("shatterdeep_crystal"), TEXT("timing_coil") }, { TEXT("AbilityHaste"), TEXT("Armor"), TEXT("MagicResist") } },
				{ TEXT("resonant_wardstone"), { TEXT("shatterdeep_crystal"), TEXT("shatterdeep_crystal") }, { TEXT("MagicResist") } },
				{ TEXT("foundation_plate"), { TEXT("marchplate"), TEXT("marchplate") }, { TEXT("Armor") } },
				{ TEXT("waymark_weave"), { TEXT("renewal_mesh"), TEXT("renewal_mesh") }, { TEXT("HealthRegeneration") } },
				{ TEXT("rescue_rig"), { TEXT("quickcoil"), TEXT("vital_plate") }, { TEXT("AttackSpeed"), TEXT("Health") } },
				{ TEXT("killstring_assembly"), { TEXT("keensteel"), TEXT("quickcoil") }, { TEXT("AttackSpeed"), TEXT("CritChance") } },
				{ TEXT("riverhold_bastion"), { TEXT("foundation_plate"), TEXT("marchplate"), TEXT("picket_plating") }, { TEXT("Armor"), TEXT("Health") } },
				{ TEXT("blackreef_bell"), { TEXT("canyonward"), TEXT("resonant_wardstone") }, { TEXT("Health"), TEXT("MagicResist") } },
				{ TEXT("harborline_harness"), { TEXT("rescue_rig"), TEXT("warforged_grip"), TEXT("waymark_weave") },
					{ TEXT("AttackSpeed"), TEXT("Health"), TEXT("HealthRegeneration"), TEXT("PhysicalPower") } },
				{ TEXT("doombringer_bow"), { TEXT("killstring_assembly"), TEXT("titansteel_grip") }, { TEXT("AttackSpeed"), TEXT("CritChance"), TEXT("PhysicalPower") } },
			};
			for (const FExpected& Expected : Revision)
			{
				const FVeyraItemDefinition* Item = UVeyraItemsTuningSubsystem::FindItem(ItemId(Expected.Item));
				ASSERT_THAT(IsNotNull(Item, Expected.Item));
				TArray<FString> Components;
				for (const FVeyraContentId& Component : Item->Components)
				{
					Components.Add(Component.ToString());
				}
				Components.Sort();
				ASSERT_THAT(AreEqual(FString::Join(Expected.Components, TEXT(" + ")), FString::Join(Components, TEXT(" + ")), Expected.Item));
				// The stats it has are the ones the bible names, read by name from the tuning struct.
				TArray<FString> Stats;
				for (TFieldIterator<FDoubleProperty> Stat(FVeyraItemStatsTuning::StaticStruct()); Stat; ++Stat)
				{
					if (Stat->GetPropertyValue_InContainer(&Item->Stats) != 0.0)
					{
						Stats.Add(Stat->GetName());
					}
				}
				Stats.Sort();
				ASSERT_THAT(AreEqual(FString::Join(Expected.Stats, TEXT(", ")), FString::Join(Stats, TEXT(", ")), Expected.Item));
			}

			// The bible's comparisons: Warforged beats Iron Grip and Titansteel beats both (§4); the two-of-a-kind
			// assemblies give at least what their two components do (§6).
			const auto Stats = [](const TCHAR* Item) { return UVeyraItemsTuningSubsystem::FindItem(ItemId(Item))->Stats; };
			ASSERT_THAT(IsTrue(Stats(TEXT("warforged_grip")).PhysicalPower > Stats(TEXT("iron_grip")).PhysicalPower));
			ASSERT_THAT(IsTrue(Stats(TEXT("titansteel_grip")).PhysicalPower > Stats(TEXT("warforged_grip")).PhysicalPower));
			ASSERT_THAT(IsTrue(Stats(TEXT("resonant_wardstone")).MagicResist >= 2 * Stats(TEXT("shatterdeep_crystal")).MagicResist));
			ASSERT_THAT(IsTrue(Stats(TEXT("foundation_plate")).Armor >= 2 * Stats(TEXT("marchplate")).Armor));
			ASSERT_THAT(IsTrue(Stats(TEXT("waymark_weave")).HealthRegeneration >= 2 * Stats(TEXT("renewal_mesh")).HealthRegeneration));

			// The Masterworks' Attunements (§8).
			const FVeyraItemsTuning& Catalog = UVeyraItemsTuningSubsystem::Get();
			ASSERT_THAT(IsTrue(Catalog.DragTheTempo.Contains(UVeyraItemsTuningSubsystem::FindItem(ItemId(TEXT("riverhold_bastion")))->Attunement[0])));
			ASSERT_THAT(IsTrue(Catalog.QuietingChime.Contains(UVeyraItemsTuningSubsystem::FindItem(ItemId(TEXT("blackreef_bell")))->Attunement[0])));
			ASSERT_THAT(IsTrue(Catalog.SafeHarbor.Contains(UVeyraItemsTuningSubsystem::FindItem(ItemId(TEXT("harborline_harness")))->Attunement[0])));
			ASSERT_THAT(IsTrue(Catalog.MarkedForDoom.Contains(UVeyraItemsTuningSubsystem::FindItem(ItemId(TEXT("doombringer_bow")))->Attunement[0])));
		}

		TEST_METHOD(TheCommittedQuestEvolvesFluxReclaimerIntoWaylineReservoir)
		{
			// Item Bible §10: Reclamation's last hits evolve Flux Reclaimer; Wayline Reservoir improves both its stats.
			const FVeyraItemsTuning& Catalog = UVeyraItemsTuningSubsystem::Get();
			const FVeyraQuestTuning* Reclamation = Catalog.Quests.Find(ItemId(TEXT("flux_reclaimer")));
			ASSERT_THAT(IsNotNull(Reclamation));
			ASSERT_THAT(IsTrue(Reclamation->Objective == EVeyraQuestObjective::LaneFluxbornLastHits && Reclamation->EvolvesInto == ItemId(TEXT("wayline_reservoir"))));
			ASSERT_THAT(IsTrue(VeyraItems::IsEvolutionOnly(Catalog, ItemId(TEXT("wayline_reservoir")))));
			const FVeyraItemStatsTuning& Base = UVeyraItemsTuningSubsystem::FindItem(ItemId(TEXT("flux_reclaimer")))->Stats;
			const FVeyraItemDefinition* Evolved = UVeyraItemsTuningSubsystem::FindItem(ItemId(TEXT("wayline_reservoir")));
			ASSERT_THAT(IsTrue(Base.Health > 0.0 && Base.HealthRegeneration > 0.0));
			ASSERT_THAT(IsTrue(Evolved->Stats.Health > Base.Health && Evolved->Stats.HealthRegeneration > Base.HealthRegeneration));
			ASSERT_THAT(IsTrue(Evolved->Attunement.Num() == 1 && Catalog.ResidualCurrent.Contains(Evolved->Attunement[0]), TEXT("Residual Current")));
		}

		TEST_METHOD(TheCommittedCatalogLoads)
		{
			const UVeyraItemsTuningSubsystem* Subsystem = GEngine->GetEngineSubsystem<UVeyraItemsTuningSubsystem>();
			ASSERT_THAT(IsTrue(Subsystem && Subsystem->IsLoaded()));
			ASSERT_THAT(IsTrue(VeyraItems::Validate(UVeyraItemsTuningSubsystem::Get()).IsEmpty()));
			// Starting Gold buys a component, or a component and consumables (Economy & Progression Bible §10).
			ASSERT_THAT(IsTrue(UVeyraItemsTuningSubsystem::FindItem(ItemId(TEXT("iron_grip"))) != nullptr));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
