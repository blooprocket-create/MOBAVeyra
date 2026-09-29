// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Components/ActorTestSpawner.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "CQTest.h"
#include "Engine/World.h"
#include "Gold/VeyraGoldComponent.h"
#include "TimerManager.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Life/VeyraLifeComponent.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/Items/VeyraItemsTestCatalog.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "VeyraAbilitiesVerbs.h"
#include "VeyraPlayerState.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraItemsTests
{
	// Veyra.Items.Shop.*: the shop's transactions on a participant, its Gold, items and stats (Economy &
	// Progression Bible §10–§12; ADR-012 §5–§6, §9).
	TEST_CLASS(Shop, "Veyra.Items")
	{
		// Fixture values.
		static constexpr double Purse = 10000.0;
		static constexpr double GripPower = 10.0;

		FActorTestSpawner Spawner;
		FVeyraItemsTuning Tuning = TestCatalog();
		UVeyraShopSubsystem* Subsystem = nullptr;
		AVeyraPlayerState* Participant = nullptr;
		UVeyraInventoryComponent* Inventory = nullptr;
		UVeyraGoldComponent* Gold = nullptr;

		BEFORE_EACH()
		{
			Tuning.Items[ItemId(TEXT("test_grip"))].Stats.PhysicalPower = GripPower;
			UVeyraItemsTuningSubsystem::SetTestOverride(&Tuning);
			Subsystem = Spawner.GetWorld().GetSubsystem<UVeyraShopSubsystem>();
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Participant = World.Spawn(EVeyraTeam::A, FVector::ZeroVector).GetPlayerState<AVeyraPlayerState>();
			Inventory = Participant ? Participant->FindComponentByClass<UVeyraInventoryComponent>() : nullptr;
			Gold = Participant ? Participant->FindComponentByClass<UVeyraGoldComponent>() : nullptr;
			ASSERT_THAT(IsTrue(Subsystem && Inventory && Gold));
			UVeyraShopSubsystem::InitializeInventory(*Participant);
			ASSERT_THAT(IsTrue(Gold->Grant(Purse, EVeyraGoldReason::Developer)));
		}

		AFTER_EACH()
		{
			UVeyraItemsTuningSubsystem::SetTestOverride(nullptr);
		}

		int32 CountOf(const TCHAR* Item) const
		{
			int32 Count = 0;
			for (const FVeyraInventorySlot& Slot : Inventory->GetSlots())
			{
				Count += !Slot.IsEmpty() && Slot.Item == ItemId(Item) ? Slot.Count : 0;
			}
			return Count;
		}

		double PhysicalPower() const
		{
			return Participant->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPowerAttribute());
		}

		void Die() const
		{
			Participant->FindComponentByClass<UVeyraLifeComponent>()->SetState(EVeyraLifeState::Dead);
		}

		TEST_METHOD(AFluxSpellSwapsOnlyAtTheFountainForGold)
		{
			const TArray<FVeyraContentId>& Roster = UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster;
			ASSERT_THAT(IsTrue(Roster.Num() >= 4));
			const double Cost = UVeyraEconomyTuningSubsystem::Get().FluxSpells.SwapCost;
			UVeyraAbilityLoadoutComponent& Loadout = *Participant->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			ASSERT_THAT(IsTrue(Loadout.Grant(*Participant->GetAbilitySystemComponent(), EVeyraAbilitySlot::Spell1, Roster[0])));

			// Never remotely: a queued purchase cannot change a spell (Battleground Bible §14).
			ASSERT_THAT(IsTrue(Subsystem->SwapFluxSpell(*Participant, 0, Roster[1]) == EVeyraShopRefusal::NotAtFountain));
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Subsystem->SwapFluxSpell(*Participant, 1, Roster[0]) == EVeyraShopRefusal::AlreadyEquipped, TEXT("one spell, one slot")));
			ASSERT_THAT(IsTrue(Subsystem->SwapFluxSpell(*Participant, 2, Roster[1]) == EVeyraShopRefusal::NoSuchSpellSlot));
			ASSERT_THAT(IsTrue(Subsystem->SwapFluxSpell(*Participant, 0, ItemId(TEXT("test_grip"))) == EVeyraShopRefusal::UnknownSpell));
			ASSERT_THAT(IsTrue(Gold->GetGold() == Purse, TEXT("a refusal costs nothing")));

			// A locked slot takes the swap and stays locked: its threshold is the slot's (§14).
			ASSERT_THAT(IsTrue(Loadout.IsLocked(EVeyraAbilitySlot::Spell1)));
			ASSERT_THAT(IsTrue(Subsystem->SwapFluxSpell(*Participant, 0, Roster[1]) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Loadout.FindSlot(EVeyraAbilitySlot::Spell1)->Ability == Roster[1] && Loadout.IsLocked(EVeyraAbilitySlot::Spell1)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Gold->GetGold(), Purse - Cost)));
			// Filling an empty slot is a swap too.
			ASSERT_THAT(IsTrue(Subsystem->SwapFluxSpell(*Participant, 1, Roster[0]) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Gold->GetGold(), Purse - Cost * 2.0)));

			// A spell swapped in while the old one cools starts on its own full cooldown, so a swap never resets one.
			UVeyraCooldownComponent& Cooldowns = *Participant->FindComponentByClass<UVeyraCooldownComponent>();
			Cooldowns.StartCooldown(Roster[1], VeyraAbilityRules::CooldownSeconds(UVeyraAbilitiesTuningSubsystem::Get(), Roster[1], 1), EVeyraCooldownHaste::Fixed);
			ASSERT_THAT(IsTrue(Subsystem->SwapFluxSpell(*Participant, 0, Roster[2]) == EVeyraShopRefusal::None));
			const double Full = VeyraAbilityRules::CooldownSeconds(UVeyraAbilitiesTuningSubsystem::Get(), Roster[2], 1);
			ASSERT_THAT(IsTrue(Full > 0.0 && FMath::IsNearlyEqual(Cooldowns.GetRemainingSecondsNow(Roster[2]), Full)));
			// A spell swapped in for a ready one is ready.
			ASSERT_THAT(IsTrue(Subsystem->SwapFluxSpell(*Participant, 1, Roster[3]) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Cooldowns.GetRemainingSecondsNow(Roster[3]) == 0.0));

			Gold->Spend(Gold->GetGold());
			ASSERT_THAT(IsTrue(Subsystem->SwapFluxSpell(*Participant, 1, Roster[0]) == EVeyraShopRefusal::NotEnoughGold));
		}

		TEST_METHOD(AtTheFountainAPurchaseArrivesAtOnceWithItsStats)
		{
			const double Before = PhysicalPower();
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_grip"))) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_grip")) == 1 && Gold->GetGold() == Purse - 350.0));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(PhysicalPower(), Before + GripPower), TEXT("its stats apply")));
		}

		TEST_METHOD(AwayAPurchaseWaitsAndGivesNothingUntilTheFountain)
		{
			const double Before = PhysicalPower();
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_grip"))) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_grip")) == 0 && Inventory->GetQueue().Num() == 1));
			ASSERT_THAT(IsTrue(Gold->GetGold() == Purse - 350.0 && Gold->GetHolds().Num() == 1, TEXT("spent now, and held")));
			ASSERT_THAT(IsTrue(PhysicalPower() == Before, TEXT("no benefit before delivery (§11.1)")));

			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_grip")) == 1 && Inventory->GetQueue().IsEmpty() && Gold->GetHolds().IsEmpty()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(PhysicalPower(), Before + GripPower)));
		}

		TEST_METHOD(CancellingRefundsInFullWithWhatDependedOnIt)
		{
			Subsystem->Buy(*Participant, ItemId(TEXT("test_grip")));
			Subsystem->Buy(*Participant, ItemId(TEXT("test_grip")));
			Subsystem->Buy(*Participant, ItemId(TEXT("test_plate")));
			Subsystem->Buy(*Participant, ItemId(TEXT("test_wheel")));
			ASSERT_THAT(IsTrue(Subsystem->Cancel(*Participant, 0) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Inventory->GetQueue().Num() == 2, TEXT("the wheel needed the cancelled grip")));
			ASSERT_THAT(IsTrue(Gold->GetGold() == Purse - 350.0 - 400.0, TEXT("the other grip and the plate stay paid; the rest came back")));
		}

		TEST_METHOD(DeathDeliversTheQueue)
		{
			Subsystem->Buy(*Participant, ItemId(TEXT("test_plate")));
			Die();
			Subsystem->DeliverOnDeath(*Participant);
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_plate")) == 1 && Inventory->GetQueue().IsEmpty()));
		}

		TEST_METHOD(SellingHappensAtTheFountainForTheResaleValue)
		{
			Subsystem->SetAtFountain(*Participant, true);
			Subsystem->Buy(*Participant, ItemId(TEXT("test_harness")));
			Subsystem->SetAtFountain(*Participant, false);
			ASSERT_THAT(IsTrue(Subsystem->Sell(*Participant, 0) == EVeyraShopRefusal::NotAtFountain));
			Subsystem->SetAtFountain(*Participant, true);
			const double Before = Gold->GetGold();
			ASSERT_THAT(IsTrue(Subsystem->Sell(*Participant, 0) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Gold->GetGold() - Before, 0.7 * VeyraItems::TotalCost(Tuning, ItemId(TEXT("test_harness"))))));
		}

		TEST_METHOD(UndoTakesBackSeveralStepsForEveryCoin)
		{
			Subsystem->SetAtFountain(*Participant, true);
			Subsystem->Buy(*Participant, ItemId(TEXT("test_grip")));
			Subsystem->Buy(*Participant, ItemId(TEXT("test_grip")));
			Subsystem->Buy(*Participant, ItemId(TEXT("test_wheel")));
			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_grip")) == 2, TEXT("the recipe's components come back")));
			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Gold->GetGold() == Purse && CountOf(TEXT("test_grip")) == 0, TEXT("no Gold gained or lost")));
			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::NothingToUndo));
		}

		TEST_METHOD(UndoEndsOnLeavingTheFountain)
		{
			Subsystem->SetAtFountain(*Participant, true);
			Subsystem->Buy(*Participant, ItemId(TEXT("test_grip")));
			Subsystem->SetAtFountain(*Participant, false);
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::NothingToUndo));
		}

		TEST_METHOD(AnItemsActiveSitsInItsSlotAndCastsAtRankOne)
		{
			// The committed Abilities.json's Cleave, on the test wheel.
			const FVeyraContentId Cleave = ItemId(TEXT("razorwheel_cleave"));
			Tuning.Items[ItemId(TEXT("test_wheel"))].Active = { Cleave };
			Subsystem->SetAtFountain(*Participant, true);
			Subsystem->Buy(*Participant, ItemId(TEXT("test_wheel")));
			ASSERT_THAT(IsTrue(UVeyraShopSubsystem::GetUse(*Participant, 0) == EVeyraItemUse::Active));
			const UVeyraAbilityLoadoutComponent* Loadout = Participant->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(EVeyraAbilitySlot::Item1);
			ASSERT_THAT(IsTrue(Entry && Entry->Ability == Cleave, TEXT("slot 1's key casts it (ADR-012 §1)")));

			// Ability Haste leaves an item's cooldown alone (Combat Bible §21).
			UAbilitySystemComponent& Abilities = *Participant->GetAbilitySystemComponent();
			Abilities.SetNumericAttributeBase(UVeyraOffenceSet::GetAbilityHasteAttribute(), 100.0f);
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(Abilities, EVeyraAbilitySlot::Item1, FVeyraCastTarget()) == EVeyraCastRejection::None,
				TEXT("no ranks needed")));
			const double Cooldown = Participant->FindComponentByClass<UVeyraCooldownComponent>()->GetRemainingSecondsNow(Cleave);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Cooldown, UVeyraAbilitiesTuningSubsystem::FindArea(Cleave)->Cast.CooldownSecondsByRank[0]),
				FString::SanitizeFloat(Cooldown)));

			ASSERT_THAT(IsTrue(Subsystem->Sell(*Participant, 0) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsNull(Loadout->FindSlot(EVeyraAbilitySlot::Item1), TEXT("the Active leaves with its item")));
		}

		TEST_METHOD(ATonicRestoresHealthOverTimeOneAtATime)
		{
			// Fixture values: 100 Health over 1 second.
			constexpr double Restored = 100.0;
			constexpr double Duration = 1.0;
			FVeyraConsumableTuning& Tonic = Tuning.Consumables[ItemId(TEXT("test_tonic"))];
			Tonic.HealthRestored = Restored;
			Tonic.DurationSeconds = Duration;
			Subsystem->SetAtFountain(*Participant, true);
			Subsystem->Buy(*Participant, ItemId(TEXT("test_tonic")));
			Subsystem->Buy(*Participant, ItemId(TEXT("test_tonic")));
			UAbilitySystemComponent& Abilities = *Participant->GetAbilitySystemComponent();
			const double Max = Abilities.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
			Abilities.SetNumericAttributeBase(UVeyraVitalsSet::GetHealthAttribute(), static_cast<float>(Max - 2.0 * Restored));

			ASSERT_THAT(IsTrue(UVeyraShopSubsystem::GetUse(*Participant, 0) == EVeyraItemUse::Consumable));
			ASSERT_THAT(IsTrue(Subsystem->UseConsumable(*Participant, 0) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_tonic")) == 1, TEXT("one is used up")));
			ASSERT_THAT(IsTrue(Subsystem->UseConsumable(*Participant, 0) == EVeyraShopRefusal::StillRestoring));
			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::NothingToUndo, TEXT("a used consumable ends undo")));

			// Past its duration, on world time. The timer manager ticks at most once per engine frame, and
			// a test runs inside one; its first tick only activates the timers set before it, so two frames
			// pass. A looping timer fires once for each interval the second covers.
			constexpr float Margin = 0.1f;
			FTimerManager& Timers = Spawner.GetWorld().GetTimerManager();
			++GFrameCounter;
			Timers.Tick(0.0f);
			++GFrameCounter;
			Timers.Tick(static_cast<float>(Duration) + Margin);
			const double Health = Abilities.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Health, Max - Restored, 1.0),
				FString::Printf(TEXT("all of it, and no more: Health %.1f of %.1f, expected %.1f"), Health, Max, Max - Restored)));
			ASSERT_THAT(IsTrue(Subsystem->UseConsumable(*Participant, 0) == EVeyraShopRefusal::None, TEXT("the next may start")));
		}

		TEST_METHOD(BasicAttacksOnVanguardsStackSpoolUpUntilItLapses)
		{
			// Fixture values: the Masterwork gains a Spool Up of 10% of base Attack Speed a stack, three at most, for 2 s.
			constexpr double PerStack = 0.1;
			constexpr int32 MaxStacks = 3;
			constexpr double Lasts = 2.0;
			const FVeyraContentId Spool = ItemId(TEXT("test_spool"));
			Tuning.Items[ItemId(TEXT("test_temper"))].Attunement = { Spool };
			FVeyraStackingAttunementTuning& SpoolUp = Tuning.SpoolUp.Add(Spool);
			SpoolUp.PerStack = PerStack;
			SpoolUp.MaxStacks = MaxStacks;
			SpoolUp.DurationSeconds = Lasts;
			Tuning.WeightOfWar.Reset();
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_temper"))) == EVeyraShopRefusal::None));

			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(200.0, 0.0, 0.0));
			UAbilitySystemComponent& Abilities = *Participant->GetAbilitySystemComponent();
			const double Before = Abilities.GetNumericAttribute(UVeyraOffenceSet::GetAttackSpeedAttribute());
			// Bonus Attack Speed is a fraction of the base progression keeps.
			UVeyraProgressionComponent* Progression = Participant->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->Initialize(FVeyraStatGrowth(), Before);
			const double BaseAttackSpeed = Progression->GetBaseAttackSpeed();
			ASSERT_THAT(IsTrue(BaseAttackSpeed > 0.0));
			FVeyraHostileDamageEvent Hit;
			Hit.Source = &Abilities;
			Hit.Target = Enemy.GetAbilitySystemComponent();
			Hit.Delivery = EVeyraDamageDelivery::Ability;
			Subsystem->OnHostileDamage(Hit);
			ASSERT_THAT(IsTrue(Abilities.GetNumericAttribute(UVeyraOffenceSet::GetAttackSpeedAttribute()) == Before, TEXT("an ability is no basic attack")));

			Hit.Delivery = EVeyraDamageDelivery::BasicAttack;
			for (int32 Attack = 0; Attack < MaxStacks + 2; ++Attack)
			{
				Subsystem->OnHostileDamage(Hit);
			}
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Abilities.GetNumericAttribute(UVeyraOffenceSet::GetAttackSpeedAttribute()), Before + BaseAttackSpeed * PerStack * MaxStacks, 1e-4),
				TEXT("stacks to its cap")));

			// Past its duration, the stacks fall away.
			UWorld& WorldTime = Spawner.GetWorld();
			const double Until = WorldTime.GetTimeSeconds() + Lasts + 0.1;
			while (WorldTime.GetTimeSeconds() < Until)
			{
				WorldTime.Tick(LEVELTICK_TimeOnly, 0.1f);
			}
			Subsystem->ExpireStacks();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Abilities.GetNumericAttribute(UVeyraOffenceSet::GetAttackSpeedAttribute()), Before, 1e-4)));
		}

		TEST_METHOD(AnEmptySlotOrAPlainItemHasNothingToUse)
		{
			ASSERT_THAT(IsTrue(UVeyraShopSubsystem::GetUse(*Participant, 0) == EVeyraItemUse::None));
			Subsystem->SetAtFountain(*Participant, true);
			Subsystem->Buy(*Participant, ItemId(TEXT("test_grip")));
			ASSERT_THAT(IsTrue(UVeyraShopSubsystem::GetUse(*Participant, 0) == EVeyraItemUse::None));
			ASSERT_THAT(IsTrue(Subsystem->UseConsumable(*Participant, 0) == EVeyraShopRefusal::EmptySlot));
		}

		TEST_METHOD(TheDeadShopAsAtTheFountain)
		{
			const double Before = PhysicalPower();
			Die();
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_grip"))) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_grip")) == 1, TEXT("assigned at the fountain (§10)")));
			ASSERT_THAT(IsTrue(PhysicalPower() == Before, TEXT("and giving nothing until respawn (ADR-012 §9)")));
			ASSERT_THAT(IsTrue(Subsystem->Sell(*Participant, 0) == EVeyraShopRefusal::None));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
