// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraMobilitySet.h"
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
#include "Statuses/VeyraStatusTypes.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/Abilities/VeyraTestFluxborn.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
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
			// Swapped back in for a ready spell, a spell forgets the cooldown it left with: the ledger keys
			// cooldowns by spell, so it would otherwise still be cooling (PR #29 review).
			ASSERT_THAT(IsTrue(Cooldowns.GetRemainingSecondsNow(Roster[1]) > 0.0, TEXT("the spell swapped out earlier still has its cooldown")));
			ASSERT_THAT(IsTrue(Subsystem->SwapFluxSpell(*Participant, 1, Roster[1]) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Cooldowns.GetRemainingSecondsNow(Roster[1]) == 0.0));

			Gold->Spend(Gold->GetGold());
			ASSERT_THAT(IsTrue(Subsystem->SwapFluxSpell(*Participant, 1, Roster[0]) == EVeyraShopRefusal::NotEnoughGold));
		}

		TEST_METHOD(AFountainChargeTakesGoldOnlyAtTheFountain)
		{
			// What the vision-tool swap pays with (ADR-016 §6): the shop takes the Gold, Match makes the change.
			constexpr double Charge = 50.0;
			const TCHAR* ForWhat = TEXT("a test charge");
			ASSERT_THAT(IsTrue(Subsystem->ChargeAtFountain(*Participant, Charge, ForWhat) == EVeyraShopRefusal::NotAtFountain));
			ASSERT_THAT(IsTrue(Gold->GetGold() == Purse, TEXT("a refusal costs nothing")));
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Subsystem->ChargeAtFountain(*Participant, Charge, ForWhat) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Gold->GetGold(), Purse - Charge)));
			Gold->Spend(Gold->GetGold());
			ASSERT_THAT(IsTrue(Subsystem->ChargeAtFountain(*Participant, Charge, ForWhat) == EVeyraShopRefusal::NotEnoughGold));
			// The dead shop as if at the fountain (ADR-012 §9).
			Subsystem->SetAtFountain(*Participant, false);
			Gold->Grant(Charge, EVeyraGoldReason::Developer);
			Die();
			ASSERT_THAT(IsTrue(Subsystem->ChargeAtFountain(*Participant, Charge, ForWhat) == EVeyraShopRefusal::None));
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

		int32 SlotOf(const TCHAR* Item) const
		{
			return Inventory->GetSlots().IndexOfByPredicate([Id = ItemId(Item)](const FVeyraInventorySlot& Slot) { return !Slot.IsEmpty() && Slot.Item == Id; });
		}

		TEST_METHOD(BuyingAMythicalChoosesItUntilThatPurchaseIsUndone)
		{
			Tuning = WithMythicals(Tuning);
			const FVeyraContentId Harbor = ItemId(TEXT("test_harbor"));
			const FVeyraContentId Rival = ItemId(TEXT("test_rival"));
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsFalse(Inventory->GetMythical().IsValid()));
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, Harbor) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Inventory->GetMythical() == Harbor));
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, Rival) == EVeyraShopRefusal::MythicalTaken, TEXT("one per match (Item Bible §11)")));
			// Undo takes the purchase back whole, the choice with it (ADR-025 §2).
			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsFalse(Inventory->GetMythical().IsValid()));
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, Rival) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Inventory->GetMythical() == Rival));
		}

		TEST_METHOD(SellingAMythicalKeepsTheChoiceAndItMayBeBoughtAgain)
		{
			Tuning = WithMythicals(Tuning);
			const FVeyraContentId Harbor = ItemId(TEXT("test_harbor"));
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, Harbor) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Subsystem->Sell(*Participant, SlotOf(TEXT("test_harbor"))) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Inventory->GetMythical() == Harbor, TEXT("selling does not release it (ADR-025 §8.1)")));
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_rival"))) == EVeyraShopRefusal::MythicalTaken));
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, Harbor) == EVeyraShopRefusal::None, TEXT("the same one, again")));
			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Inventory->GetMythical() == Harbor, TEXT("undoing the purchase again leaves the choice that selling kept")));
		}

		TEST_METHOD(AQueuedMythicalChoosesItAndCancellingReleasesIt)
		{
			Tuning = WithMythicals(Tuning);
			const FVeyraContentId Harbor = ItemId(TEXT("test_harbor"));
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, Harbor) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Inventory->GetQueue().Num() == 1 && Inventory->GetMythical() == Harbor, TEXT("queuing chooses it")));
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_rival"))) == EVeyraShopRefusal::MythicalTaken));
			ASSERT_THAT(IsTrue(Subsystem->Cancel(*Participant, 0) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsFalse(Inventory->GetMythical().IsValid()));

			// A queued Mythical dropped because a part it needed went away releases the choice too.
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_temper"))) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, Harbor) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Subsystem->Cancel(*Participant, 0) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Inventory->GetQueue().IsEmpty() && !Inventory->GetMythical().IsValid()));
		}

		TEST_METHOD(UndoingDeliveredPurchasesReleasesTheMythicalOnlyWithItsOwn)
		{
			Tuning = WithMythicals(Tuning);
			const FVeyraContentId Harbor = ItemId(TEXT("test_harbor"));
			// Away, a plate and then the Mythical wait; the fountain delivers both as two undo steps.
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_plate"))) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, Harbor) == EVeyraShopRefusal::None));
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_harbor")) == 1 && Inventory->GetUndoStepCount() == 2));
			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsFalse(Inventory->GetMythical().IsValid()));
			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsFalse(Inventory->GetMythical().IsValid(), TEXT("the plate's step came before the choice")));
			ASSERT_THAT(IsTrue(Gold->GetGold() == Purse));
		}

		/** Kills a fixture unit of Team with a lethal hit from the participant. */
		template <typename TUnit>
		void KillOne(EVeyraTeam Team)
		{
			constexpr double Lethal = 1.0e6;
			TUnit& Unit = Spawner.SpawnActorAt<TUnit>(FVector(300.0, 0.0, 0.0), FRotator::ZeroRotator);
			if constexpr (std::is_same_v<TUnit, AVeyraTestFluxborn>)
			{
				Unit.SetVeyraTeam(Team);
			}
			UAbilitySystemComponent& Target = *Unit.GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(Target, VeyraCombatTests::ExampleStats())));
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Lethal });
			Damage.Delivery = EVeyraDamageDelivery::BasicAttack;
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Participant->GetAbilitySystemComponent(), Target, Damage)));
		}

		int32 ProgressOf(const TCHAR* Item) const
		{
			const int32 Slot = SlotOf(Item);
			return Slot == INDEX_NONE ? INDEX_NONE : Inventory->GetSlots()[Slot].QuestProgress;
		}

		TEST_METHOD(LastHitsOnEnemyLaneFluxbornAdvanceTheQuestUntilItEvolves)
		{
			Tuning = WithQuest(Tuning);
			// Fixture value: the evolution's stats show it arrived.
			constexpr double ReservoirHealth = 250.0;
			Tuning.Items[ItemId(TEXT("test_reservoir"))].Stats.Health = ReservoirHealth;
			const FVeyraContentId Reclaimer = ItemId(TEXT("test_reclaimer"));
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, Reclaimer) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, Reclaimer) == EVeyraShopRefusal::QuestLineHeld));
			const double MaxHealth = Participant->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());

			// Only a last hit on an enemy lane Fluxborn counts (Item Bible §10).
			KillOne<AVeyraTestFluxborn>(EVeyraTeam::A);
			KillOne<AVeyraTestWildlife>(EVeyraTeam::None);
			ASSERT_THAT(AreEqual(0, ProgressOf(TEXT("test_reclaimer")), TEXT("not an ally, nor wildlife")));
			KillOne<AVeyraTestFluxborn>(EVeyraTeam::B);
			ASSERT_THAT(AreEqual(1, ProgressOf(TEXT("test_reclaimer"))));
			KillOne<AVeyraTestFluxborn>(EVeyraTeam::B);
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_reclaimer")) == 0 && CountOf(TEXT("test_reservoir")) == 1, TEXT("it evolved in place")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Participant->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()),
				MaxHealth + ReservoirHealth), TEXT("with its stats")));

			ASSERT_THAT(IsTrue(Subsystem->Undo(*Participant) == EVeyraShopRefusal::AlreadyUsed, TEXT("the evolution was benefit")));
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, Reclaimer) == EVeyraShopRefusal::QuestLineHeld, TEXT("the line is held")));
			const double Before = Gold->GetGold();
			ASSERT_THAT(IsTrue(Subsystem->Sell(*Participant, SlotOf(TEXT("test_reservoir"))) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Gold->GetGold() - Before, 450.0 * Tuning.Shop.ResaleFraction), TEXT("it sells as its base form did")));
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

		TEST_METHOD(SeizeMomentumSlowsWhatItHitsAndSpeedsTheUserForEachVanguard)
		{
			// The committed Abilities.json's Seize Momentum, on the test wheel, among two enemy Vanguards and a Fluxborn.
			const FVeyraContentId Seize = ItemId(TEXT("seize_momentum"));
			Tuning.Items[ItemId(TEXT("test_wheel"))].Active = { Seize };
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(TEXT("test_wheel"))) == EVeyraShopRefusal::None));
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& First = World.Spawn(EVeyraTeam::B, FVector(200.0, 0.0, 0.0));
			World.Spawn(EVeyraTeam::B, FVector(-200.0, 0.0, 0.0));
			World.SpawnFluxborn(EVeyraTeam::B, FVector(0.0, 200.0, 0.0));
			UAbilitySystemComponent& Abilities = *Participant->GetAbilitySystemComponent();
			const double Speed = Abilities.GetNumericAttribute(UVeyraMobilitySet::GetMoveSpeedAttribute());

			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(Abilities, EVeyraAbilitySlot::Item1, FVeyraCastTarget()) == EVeyraCastRejection::None));
			const FVeyraAbilitiesTuning& AbilityTuning = UVeyraAbilitiesTuningSubsystem::Get();
			const double Slow = AbilityTuning.Statuses.FindChecked(ItemId(TEXT("seize_momentum_slow"))).Magnitude;
			const double Haste = AbilityTuning.Statuses.FindChecked(ItemId(TEXT("seize_momentum_haste"))).Magnitude;
			const UVeyraStatusComponent* Struck = First.GetPlayerState()->FindComponentByClass<UVeyraStatusComponent>();
			ASSERT_THAT(IsTrue(Struck && FMath::IsNearlyEqual(Struck->GetStrongestSlow(), Slow), TEXT("what it hits is slowed")));
			const double Hasted = Abilities.GetNumericAttribute(UVeyraMobilitySet::GetMoveSpeedAttribute());
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Hasted, Speed * (1.0 + 2.0 * Haste), 1e-2),
				FString::Printf(TEXT("a stack for each Vanguard, none for the Fluxborn: %g from %g"), Hasted, Speed)));
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

		TEST_METHOD(NothingIsDrunkInStasis)
		{
			Subsystem->SetAtFountain(*Participant, true);
			Subsystem->Buy(*Participant, ItemId(TEXT("test_tonic")));
			UAbilitySystemComponent& Abilities = *Participant->GetAbilitySystemComponent();
			FVeyraStatusSpec Stasis;
			Stasis.Id = ItemId(TEXT("test_stasis"));
			Stasis.Kind = EVeyraStatusKind::Stasis;
			Stasis.DurationSeconds = 60.0;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Abilities, Abilities, Stasis)));
			// It takes no action, so it cannot start a restoration that would outlast the Stasis (ADR-050 §1), as
			// when it holds still while commanding its Echo.
			ASSERT_THAT(IsTrue(Subsystem->UseConsumable(*Participant, 0) == EVeyraShopRefusal::NotNow));
			ASSERT_THAT(IsTrue(CountOf(TEXT("test_tonic")) == 1, TEXT("nothing is used up")));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(Abilities, Stasis.Id)));
			ASSERT_THAT(IsTrue(Subsystem->UseConsumable(*Participant, 0) == EVeyraShopRefusal::None, TEXT("out of it, it drinks")));
		}

		/** Runs world time past Seconds, so a running restoration ends (see the tonic's test). */
		void RunPast(double Seconds)
		{
			constexpr float Margin = 0.1f;
			FTimerManager& Timers = Spawner.GetWorld().GetTimerManager();
			++GFrameCounter;
			Timers.Tick(0.0f);
			++GFrameCounter;
			Timers.Tick(static_cast<float>(Seconds) + Margin);
		}

		int32 ChargesLeft(const TCHAR* Item) const
		{
			const FVeyraInventorySlot* Held = Inventory->GetSlots().FindByPredicate([Item](const FVeyraInventorySlot& Slot) { return Slot.Item == ItemId(Item); });
			return Held ? Held->Charges : INDEX_NONE;
		}

		TEST_METHOD(ARefillableConsumableSpendsChargesAndRefills)
		{
			// Fixture values: the tonic made refillable, with two charges of 10 Health over 1 second
			// (Item Bible §12; ADR-023 §6).
			constexpr int32 Charges = 2;
			constexpr double Duration = 1.0;
			const TCHAR* Flask = TEXT("test_tonic");
			Tuning.Items[ItemId(Flask)].StackLimit = 1;
			FVeyraConsumableTuning& Refillable = Tuning.Consumables[ItemId(Flask)];
			Refillable.Charges = Charges;
			Refillable.HealthRestored = 10.0;
			Refillable.DurationSeconds = Duration;
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(Flask)) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(Subsystem->Buy(*Participant, ItemId(Flask)) == EVeyraShopRefusal::Unique, TEXT("held once")));
			ASSERT_THAT(AreEqual(Charges, ChargesLeft(Flask), TEXT("it arrives full")));

			for (int32 Used = 1; Used <= Charges; ++Used)
			{
				ASSERT_THAT(IsTrue(Subsystem->UseConsumable(*Participant, 0) == EVeyraShopRefusal::None));
				ASSERT_THAT(AreEqual(Charges - Used, ChargesLeft(Flask), TEXT("a use spends a charge")));
				ASSERT_THAT(AreEqual(1, CountOf(Flask), TEXT("and the flask stays")));
				RunPast(Duration);
			}
			ASSERT_THAT(IsTrue(Subsystem->UseConsumable(*Participant, 0) == EVeyraShopRefusal::NoCharges));

			// Arriving at the fountain refills it; so does Match, when its side secures a Flux Well.
			Subsystem->SetAtFountain(*Participant, false);
			Subsystem->SetAtFountain(*Participant, true);
			ASSERT_THAT(AreEqual(Charges, ChargesLeft(Flask), TEXT("the fountain")));
			ASSERT_THAT(IsTrue(Subsystem->UseConsumable(*Participant, 0) == EVeyraShopRefusal::None));
			Subsystem->RefillCharges(*Participant);
			ASSERT_THAT(AreEqual(Charges, ChargesLeft(Flask), TEXT("a secured Well")));
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
