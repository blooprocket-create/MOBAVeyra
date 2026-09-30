// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Attunements/VeyraAttunementSubsystem.h"
#include "CombatState/VeyraCombatStateComponent.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Engine/World.h"
#include "Gold/VeyraGoldComponent.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/Abilities/VeyraTestFluxborn.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "Tests/Items/VeyraItemsTestCatalog.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraItemsTests
{
	// Veyra.Items.Attunements.*: the Attunements a hit on an enemy Vanguard sets off, through Combat's
	// dealt-damage event and verbs (Item Bible §8–§9; ADR-023 §3–§5). Each test gives the fixture
	// catalog's Masterwork the Attunement under test.
	TEST_CLASS(Attunements, "Veyra.Items")
	{
		// Fixture values.
		static constexpr double Purse = 10000.0;
		static constexpr double Blow = 50.0;
		static constexpr float WorldStep = 0.1f;

		FActorTestSpawner Spawner;
		FVeyraItemsTuning Tuning = TestCatalog();
		UVeyraShopSubsystem* Shop = nullptr;
		AVeyraPlayerState* Participant = nullptr;
		AVeyraVanguardCharacter* HolderBody = nullptr;
		UAbilitySystemComponent* Enemy = nullptr;
		TArray<FVeyraDamageDealtEvent> Dealt;

		BEFORE_EACH()
		{
			UVeyraItemsTuningSubsystem::SetTestOverride(&Tuning);
			Shop = Spawner.GetWorld().GetSubsystem<UVeyraShopSubsystem>();
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			HolderBody = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Participant = HolderBody->GetPlayerState<AVeyraPlayerState>();
			Enemy = World.Spawn(EVeyraTeam::B, FVector(200.0, 0.0, 0.0)).GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(Shop && Participant && Enemy));
			UVeyraShopSubsystem::InitializeInventory(*Participant);
			ASSERT_THAT(IsTrue(Participant->FindComponentByClass<UVeyraGoldComponent>()->Grant(Purse, EVeyraGoldReason::Developer)));
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDamageDealt.AddLambda([this](const FVeyraDamageDealtEvent& Event) { Dealt.Add(Event); });
		}

		AFTER_EACH()
		{
			UVeyraItemsTuningSubsystem::SetTestOverride(nullptr);
		}

		UAbilitySystemComponent& Holder() const
		{
			return *Participant->GetAbilitySystemComponent();
		}

		/** Gives the Masterwork Attunement and buys it at the fountain. */
		void Hold(const TCHAR* Attunement)
		{
			Tuning.Items[ItemId(TEXT("test_temper"))].Attunement = { ItemId(Attunement) };
			Tuning.WeightOfWar.Reset();
			Shop->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Shop->Buy(*Participant, ItemId(TEXT("test_temper"))) == EVeyraShopRefusal::None));
		}

		void Hit(EVeyraDamageType Type, double Amount, EVeyraDamageDelivery Delivery)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ Type, Amount });
			Damage.Delivery = Delivery;
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Holder(), *Enemy, Damage)));
		}

		double HolderShield() const
		{
			double Sum = 0.0;
			for (const FVeyraShieldEntry& Shield : Participant->FindComponentByClass<UVeyraDamageAbsorptionComponent>()->GetLedger().Shields)
			{
				Sum += Shield.Remaining;
			}
			return Sum;
		}

		const UVeyraStatusComponent& EnemyStatuses() const
		{
			return *Enemy->GetOwner()->FindComponentByClass<UVeyraStatusComponent>();
		}

		int32 Procs() const
		{
			return Dealt.FilterByPredicate([](const FVeyraDamageDealtEvent& Event) { return Event.Delivery == EVeyraDamageDelivery::Proc; }).Num();
		}

		void RunFor(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, WorldStep);
			}
		}

		void KillEnemyFluxborn()
		{
			constexpr double Lethal = 1.0e6;
			AVeyraTestFluxborn& Minion = Spawner.SpawnActorAt<AVeyraTestFluxborn>(FVector(300.0, 0.0, 0.0), FRotator::ZeroRotator);
			Minion.SetVeyraTeam(EVeyraTeam::B);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Minion.GetAbilitySystemComponent(), VeyraCombatTests::ExampleStats())));
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Lethal });
			Damage.Delivery = EVeyraDamageDelivery::BasicAttack;
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Holder(), *Minion.GetAbilitySystemComponent(), Damage)));
		}

		double StoredCurrent() const
		{
			const FVeyraInventorySlot* Slot = Participant->FindComponentByClass<UVeyraInventoryComponent>()->GetSlots().FindByPredicate(
				[](const FVeyraInventorySlot& Each) { return Each.Item == ItemId(TEXT("test_reservoir")); });
			return Slot ? Slot->Current : -1.0;
		}

		bool Amplified() const
		{
			return Participant->FindComponentByClass<UVeyraStatusComponent>()->Has(EVeyraStatusKind::HealthRegeneration);
		}

		TEST_METHOD(ResidualCurrentStoresLastHitsAndAmplifiesRegenerationOnceQuiet)
		{
			Tuning = WithQuest(Tuning);
			const FVeyraResidualCurrentTuning& Residual = Tuning.ResidualCurrent[ItemId(TEXT("test_current"))];
			UVeyraAttunementSubsystem& Attunements = *Spawner.GetWorld().GetSubsystem<UVeyraAttunementSubsystem>();
			ASSERT_THAT(IsTrue(Shop->GrantItem(*Participant, ItemId(TEXT("test_reservoir"))) == EVeyraShopRefusal::None));

			// Each last hit stores Current, up to the cap (Item Bible §10).
			KillEnemyFluxborn();
			ASSERT_THAT(IsTrue(StoredCurrent() == Residual.CurrentPerLastHit));
			KillEnemyFluxborn();
			KillEnemyFluxborn();
			ASSERT_THAT(IsTrue(StoredCurrent() == Residual.CurrentCap, FString::SanitizeFloat(StoredCurrent())));

			// At full Health it keeps its Current (ADR-025 §8).
			Attunements.UpdateHeld();
			ASSERT_THAT(IsTrue(StoredCurrent() == Residual.CurrentCap && !Amplified()));

			// Missing Health and clear of enemy Vanguards, it spends a tick's Current to amplify regeneration.
			const FGameplayAttribute Health = UVeyraVitalsSet::GetHealthAttribute();
			Holder().SetNumericAttributeBase(Health, Holder().GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) / 2.0f);
			Attunements.UpdateHeld();
			const double Tick = UVeyraCombatTuningSubsystem::Get().Regeneration.TickSeconds;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(StoredCurrent(), Residual.CurrentCap - Residual.CurrentPerSecond * Tick), FString::SanitizeFloat(StoredCurrent())));
			ASSERT_THAT(IsTrue(Amplified()));

			// An enemy Vanguard's damage suspends it and keeps the Current.
			FVeyraRawDamageEvent Poke;
			Poke.Components.Add({ EVeyraDamageType::TrueDamage, 1.0 });
			Poke.Delivery = EVeyraDamageDelivery::BasicAttack;
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy, Holder(), Poke)));
			ASSERT_THAT(IsTrue(Attunements.GetVanguardDamageTakenAt(Holder()).IsSet()));
			const double Kept = StoredCurrent();
			Attunements.UpdateHeld();
			ASSERT_THAT(IsTrue(StoredCurrent() == Kept && !Amplified()));

			// After its quiet time it spends again.
			RunFor(Residual.QuietSeconds + WorldStep);
			Holder().SetNumericAttributeBase(Health, Holder().GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) / 2.0f);
			Attunements.UpdateHeld();
			ASSERT_THAT(IsTrue(StoredCurrent() < Kept && Amplified()));
		}

		/** An enemy Vanguard's hit on the holder. */
		void StrikeHolder(EVeyraDamageDelivery Delivery)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, 1.0 });
			Damage.Delivery = Delivery;
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy, Holder(), Damage)));
		}

		bool HolderShielded() const
		{
			return Participant->FindComponentByClass<UVeyraStatusComponent>()->Has(EVeyraStatusKind::SpellShield);
		}

		TEST_METHOD(QuietingChimeFormsBlocksAnAbilityAndFormsAgainOnceQuiet)
		{
			// Fixture value: a short wait to form again.
			constexpr double Reform = 2.0;
			Tuning.QuietingChime.Add(ItemId(TEXT("test_chime"))).ReformSeconds = Reform;
			Hold(TEXT("test_chime"));
			UVeyraAttunementSubsystem& Attunements = *Spawner.GetWorld().GetSubsystem<UVeyraAttunementSubsystem>();
			Attunements.UpdateHeld();
			ASSERT_THAT(IsTrue(HolderShielded(), TEXT("it forms at once when nothing has hurt the holder")));
			StrikeHolder(EVeyraDamageDelivery::BasicAttack);
			Attunements.UpdateHeld();
			ASSERT_THAT(IsTrue(HolderShielded(), TEXT("a formed shield keeps through damage")));

			ASSERT_THAT(IsTrue(VeyraCombat::BlockAbilityHit(Holder(), *Enemy)));
			Attunements.UpdateHeld();
			ASSERT_THAT(IsFalse(HolderShielded(), TEXT("consumed, it waits")));
			RunFor(Reform * 0.6);
			StrikeHolder(EVeyraDamageDelivery::Ability);
			RunFor(Reform * 0.6);
			Attunements.UpdateHeld();
			ASSERT_THAT(IsFalse(HolderShielded(), TEXT("enemy-Vanguard damage starts the wait again (ADR-025 §7)")));
			RunFor(Reform * 0.6);
			Attunements.UpdateHeld();
			ASSERT_THAT(IsTrue(HolderShielded()));
		}

		TEST_METHOD(DragTheTempoSlowsAnEnemyVanguardsAttacksOnTheHolder)
		{
			// Fixture values: a fifth of the attacker's Attack Speed for 3 s.
			constexpr double Reduction = 0.2;
			constexpr double Seconds = 3.0;
			FVeyraDragTheTempoTuning& Tempo = Tuning.DragTheTempo.Add(ItemId(TEXT("test_tempo")));
			Tempo.AttackSpeedReduction = Reduction;
			Tempo.Seconds = Seconds;
			Hold(TEXT("test_tempo"));
			StrikeHolder(EVeyraDamageDelivery::Ability);
			ASSERT_THAT(IsFalse(EnemyStatuses().Has(EVeyraStatusKind::AttackSpeed), TEXT("only a basic attack")));
			StrikeHolder(EVeyraDamageDelivery::BasicAttack);
			StrikeHolder(EVeyraDamageDelivery::BasicAttack);
			const TArray<FVeyraStatusEntry> Slows = EnemyStatuses().GetLedger().Entries.FilterByPredicate(
				[](const FVeyraStatusEntry& Entry) { return Entry.Id == ItemId(TEXT("test_tempo")); });
			ASSERT_THAT(IsTrue(Slows.Num() == 1 && Slows[0].Stacks == 1 && FMath::IsNearlyEqual(Slows[0].Magnitude, -Reduction),
				TEXT("refreshed, never stacked (ADR-025 §7)")));
		}

		/** The holder's basic attack on the enemy, a crit or not. */
		void Attack(bool bCritical)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Blow });
			Damage.Delivery = EVeyraDamageDelivery::BasicAttack;
			FVeyraPreparedDamage Prepared = VeyraCombat::PrepareDamage(Holder(), Damage);
			Prepared.bCritical = bCritical;
			ASSERT_THAT(IsTrue(VeyraCombat::DealPreparedDamage(Prepared, *Enemy)));
		}

		TEST_METHOD(MarkedForDoomBuildsOnAttacksThenTheNextAttackOnTheDoomedConsumesIt)
		{
			// Fixture values: 1 Doom a hit, 2 a crit, Doomed at 3; a fifth of missing Health; 5 s to lapse.
			constexpr double Ratio = 0.2;
			constexpr double Expiry = 5.0;
			FVeyraMarkedForDoomTuning& Doom = Tuning.MarkedForDoom.Add(ItemId(TEXT("test_doom")));
			Doom.DoomPerHit = 1.0;
			Doom.DoomPerCrit = 2.0;
			Doom.DoomedAt = 3.0;
			Doom.ExpirySeconds = Expiry;
			Doom.MissingHealthRatio = Ratio;
			Hold(TEXT("test_doom"));
			// No Armor, so the Physical Proc lands whole.
			Enemy->SetNumericAttributeBase(UVeyraDefenceSet::GetArmorAttribute(), 0.0f);
			UVeyraAttunementSubsystem& Attunements = *Spawner.GetWorld().GetSubsystem<UVeyraAttunementSubsystem>();

			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(IsTrue(Attunements.GetDoom(Holder(), *Enemy) == 0.0, TEXT("only basic attacks build it")));
			Attack(false);
			Attack(true);
			ASSERT_THAT(IsTrue(Attunements.GetDoom(Holder(), *Enemy) == 3.0 && Procs() == 0, TEXT("a hit and a crit: Doomed, nothing dealt yet")));
			const FVeyraStatusEntry* Mark = EnemyStatuses().GetLedger().Entries.FindByPredicate([](const FVeyraStatusEntry& Entry) { return Entry.Id == ItemId(TEXT("test_doom")); });
			ASSERT_THAT(IsTrue(Mark && Mark->Kind == EVeyraStatusKind::Counter && Mark->Stacks == 3, TEXT("every machine sees the Doom as a mark")));
			const double MissingBefore = VeyraCombat::GetMissingHealth(*Enemy);
			Attack(false);
			ASSERT_THAT(AreEqual(1, Procs(), TEXT("the next attack consumes it")));
			const double Expected = MissingBefore + Blow + Ratio * (MissingBefore + Blow);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCombat::GetMissingHealth(*Enemy), Expected, 1e-3),
				FString::Printf(TEXT("missing %g, expected %g: the share of the missing Health after the hit"), VeyraCombat::GetMissingHealth(*Enemy), Expected)));
			ASSERT_THAT(IsTrue(Attunements.GetDoom(Holder(), *Enemy) == 0.0, TEXT("the consuming hit adds none (ADR-025 §8.5)")));
			ASSERT_THAT(IsFalse(EnemyStatuses().GetLedger().Entries.ContainsByPredicate([](const FVeyraStatusEntry& Entry) { return Entry.Id == ItemId(TEXT("test_doom")); }),
				TEXT("and the mark goes with it")));

			Attack(true);
			RunFor(Expiry + WorldStep);
			ASSERT_THAT(IsTrue(Attunements.GetDoom(Holder(), *Enemy) == 0.0, TEXT("it lapses")));
		}

		double StoredReserve() const
		{
			const FVeyraInventorySlot* Slot = Participant->FindComponentByClass<UVeyraInventoryComponent>()->GetSlots().FindByPredicate(
				[](const FVeyraInventorySlot& Each) { return Each.Item == ItemId(TEXT("test_temper")); });
			return Slot ? Slot->Reserve : -1.0;
		}

		TEST_METHOD(SafeHarborBanksReserveFromHitsAndHealsWithItOutOfCombat)
		{
			// Fixture values: half of each hit banks, up to a tenth of Max Health; 5% of Max Health a second.
			constexpr double Fraction = 0.5;
			constexpr double CapFraction = 0.1;
			FVeyraSafeHarborTuning& Harbor = Tuning.SafeHarbor.Add(ItemId(TEXT("test_reserve")));
			Harbor.ReserveFraction = Fraction;
			Harbor.CapMaxHealthFraction = CapFraction;
			Harbor.ConversionMaxHealthFractionPerSecond = 0.05;
			Hold(TEXT("test_reserve"));
			UVeyraAttunementSubsystem& Attunements = *Spawner.GetWorld().GetSubsystem<UVeyraAttunementSubsystem>();
			const double MaxHealth = Holder().GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());

			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Proc);
			ASSERT_THAT(IsTrue(StoredReserve() == 0.0, TEXT("item damage banks nothing (ADR-025 §8.7)")));
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::BasicAttack);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(StoredReserve(), Blow * Fraction), FString::SanitizeFloat(StoredReserve())));
			for (int32 Index = 0; Index < 3; ++Index)
			{
				Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Ability);
			}
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(StoredReserve(), MaxHealth * CapFraction), TEXT("up to its cap")));

			// In Vanguard combat it keeps; out of it, it heals what is missing.
			const FGameplayAttribute Health = UVeyraVitalsSet::GetHealthAttribute();
			Holder().SetNumericAttributeBase(Health, static_cast<float>(MaxHealth / 2.0));
			UVeyraCombatStateComponent& CombatState = *Participant->FindComponentByClass<UVeyraCombatStateComponent>();
			CombatState.NoteCombat();
			Attunements.UpdateHeld();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(StoredReserve(), MaxHealth * CapFraction), TEXT("none in combat")));
			CombatState.Clear();
			Attunements.UpdateHeld();
			const double Healed = Holder().GetNumericAttribute(Health) - MaxHealth / 2.0;
			ASSERT_THAT(IsTrue(Healed > 0.0 && FMath::IsNearlyEqual(StoredReserve(), MaxHealth * CapFraction - Healed, 1e-3), TEXT("what heals is spent")));
		}

		double HolderTemporaryHealth() const
		{
			return VeyraAbsorption::TotalTemporaryHealth(Participant->FindComponentByClass<UVeyraDamageAbsorptionComponent>()->GetLedger());
		}

		TEST_METHOD(HighTideSpeedsSafeHarborAndTurnsItsOverflowIntoTemporaryHealth)
		{
			// Fixture values: the Masterwork carries Safe Harbor and High Tide, as The Last Harbor does.
			constexpr double Conversion = 0.05;
			constexpr double Acceleration = 1.0;
			constexpr double Overflow = 0.5;
			constexpr double TemporaryCap = 0.02;
			FVeyraSafeHarborTuning& Harbor = Tuning.SafeHarbor.Add(ItemId(TEXT("test_reserve")));
			Harbor.ReserveFraction = 1.0;
			Harbor.CapMaxHealthFraction = 0.5;
			Harbor.ConversionMaxHealthFractionPerSecond = Conversion;
			FVeyraHighTideTuning& Tide = Tuning.HighTide.Add(ItemId(TEXT("test_tide")));
			Tide.CurrentPerLastHit = 2.0;
			Tide.CurrentCap = 5.0;
			Tide.CurrentPerSecond = 1.0;
			Tide.RegenerationAmplification = 3.0;
			Tide.ReserveConversionAcceleration = Acceleration;
			Tide.OverflowToTemporaryHealth = Overflow;
			Tide.TemporaryHealthCapMaxHealthFraction = TemporaryCap;
			Tide.TemporaryHealthSeconds = 60.0;
			Tuning.Items[ItemId(TEXT("test_temper"))].Attunement = { ItemId(TEXT("test_reserve")), ItemId(TEXT("test_tide")) };
			Tuning.WeightOfWar.Reset();
			Shop->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Shop->Buy(*Participant, ItemId(TEXT("test_temper"))) == EVeyraShopRefusal::None));
			UVeyraAttunementSubsystem& Attunements = *Spawner.GetWorld().GetSubsystem<UVeyraAttunementSubsystem>();
			const double MaxHealth = Holder().GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
			const double Tick = UVeyraCombatTuningSubsystem::Get().Regeneration.TickSeconds;

			// Reserve from a hit; Current from a last hit, as Residual Current stores it.
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::BasicAttack);
			KillEnemyFluxborn();
			// A copy each time: the shop replaces the slots as it stores.
			const auto Held = [this] {
				return *Participant->FindComponentByClass<UVeyraInventoryComponent>()->GetSlots().FindByPredicate(
					[](const FVeyraInventorySlot& Each) { return Each.Item == ItemId(TEXT("test_temper")); });
			};
			ASSERT_THAT(IsTrue(Held().Reserve == Blow && Held().Current == Tide.CurrentPerLastHit));

			// In Vanguard combat nothing moves.
			UVeyraCombatStateComponent& CombatState = *Participant->FindComponentByClass<UVeyraCombatStateComponent>();
			CombatState.NoteCombat();
			Attunements.UpdateHeld();
			ASSERT_THAT(IsTrue(Held().Reserve == Blow && Held().Current == Tide.CurrentPerLastHit && HolderTemporaryHealth() == 0.0));

			// Out of it, at full Health, the tide doubles Safe Harbor's conversion and turns it into Temporary Health.
			CombatState.Clear();
			Attunements.UpdateHeld();
			const double Converted = FMath::Min(Blow, Conversion * (1.0 + Acceleration) * MaxHealth * Tick);
			const double Expected = FMath::Min(Converted * Overflow, TemporaryCap * MaxHealth);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HolderTemporaryHealth(), Expected, 1e-3), FString::Printf(TEXT("%g, expected %g"), HolderTemporaryHealth(), Expected)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Held().Reserve, Blow - Converted, 1e-3) && FMath::IsNearlyEqual(Held().Current, Tide.CurrentPerLastHit - Tide.CurrentPerSecond * Tick),
				FString::Printf(TEXT("Reserve %g, Current %g"), Held().Reserve, Held().Current)));
			ASSERT_THAT(IsTrue(Participant->FindComponentByClass<UVeyraStatusComponent>()->Has(EVeyraStatusKind::HealthRegeneration)));

			// Its Temporary Health stops at the cap, one grant.
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Attunements.UpdateHeld();
			}
			ASSERT_THAT(IsTrue(HolderTemporaryHealth() <= TemporaryCap * MaxHealth + 1e-3));
			ASSERT_THAT(IsTrue(Participant->FindComponentByClass<UVeyraDamageAbsorptionComponent>()->GetLedger().TemporaryHealth.Num() == 1));
		}

		TEST_METHOD(ReprisalGuardShieldsAShareOfTheHitThenWaits)
		{
			// Fixture values: a fifth of the hit, at most 15, for a long while; 5 s of cooldown.
			constexpr double Fraction = 0.2;
			constexpr double Cap = 15.0;
			constexpr double Cooldown = 5.0;
			FVeyraReprisalGuardTuning& Guard = Tuning.ReprisalGuard.Add(ItemId(TEXT("test_guard")));
			Guard.DamageFraction = Fraction;
			Guard.MaxShield = Cap;
			Guard.ShieldSeconds = 60.0;
			Guard.CooldownSeconds = Cooldown;
			Hold(TEXT("test_guard"));

			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Proc);
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Periodic);
			ASSERT_THAT(IsTrue(HolderShield() == 0.0, TEXT("a proc or a tick is no triggering attack or ability")));
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HolderShield(), Blow * Fraction), FString::SanitizeFloat(HolderShield())));
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::BasicAttack);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HolderShield(), Blow * Fraction), TEXT("then it waits")));

			// Past its cooldown, a heavy hit's shield stops at the cap, in place of the last.
			RunFor(Cooldown + WorldStep);
			Hit(EVeyraDamageType::TrueDamage, Blow * 4.0, EVeyraDamageDelivery::BasicAttack);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HolderShield(), Cap), FString::SanitizeFloat(HolderShield())));
		}

		TEST_METHOD(DragSlowsOnlyOnDamagingAbilities)
		{
			// Fixture values: 30% for a second.
			constexpr double Slow = 0.3;
			FVeyraDragTuning& Drag = Tuning.Drag.Add(ItemId(TEXT("test_drag")));
			Drag.Slow = Slow;
			Drag.DurationSeconds = 1.0;
			Hold(TEXT("test_drag"));

			Hit(EVeyraDamageType::Physical, Blow, EVeyraDamageDelivery::BasicAttack);
			Hit(EVeyraDamageType::Magic, Blow, EVeyraDamageDelivery::Proc);
			ASSERT_THAT(IsTrue(EnemyStatuses().GetStrongestSlow() == 0.0, TEXT("a basic attack or a proc does not")));
			Hit(EVeyraDamageType::Magic, Blow, EVeyraDamageDelivery::Periodic);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(EnemyStatuses().GetStrongestSlow(), Slow), TEXT("an ability's damage over time does")));
			VeyraCombat::RemoveStatus(*Enemy, ItemId(TEXT("test_drag")));
			Hit(EVeyraDamageType::Magic, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(EnemyStatuses().GetStrongestSlow(), Slow), TEXT("and so does its hit")));
		}

		TEST_METHOD(ConvergencePrimesAndTheNextAbilityHitConsumesIt)
		{
			// Fixture values: a 4 s window; 40 magic damage and a fifth of Magic Power. No Magic
			// Resistance, so the bonus lands whole.
			constexpr double Window = 4.0;
			constexpr double Base = 40.0;
			constexpr double Ratio = 0.2;
			FVeyraConvergenceTuning& Convergence = Tuning.Convergence.Add(ItemId(TEXT("test_converge")));
			Convergence.WindowSeconds = Window;
			Convergence.BaseDamage = Base;
			Convergence.MagicPowerRatio = Ratio;
			Hold(TEXT("test_converge"));
			Enemy->SetNumericAttributeBase(UVeyraDefenceSet::GetMagicResistAttribute(), 0.0f);

			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::BasicAttack);
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::BasicAttack);
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(AreEqual(0, Procs(), TEXT("basic attacks neither prime nor consume; the ability primes")));
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(AreEqual(1, Procs(), TEXT("the next ability consumes the prime")));
			const FVeyraDamageDealtEvent& Bonus = *Dealt.FindByPredicate([](const FVeyraDamageDealtEvent& Event) { return Event.Delivery == EVeyraDamageDelivery::Proc; });
			const double Expected = Base + Ratio * Holder().GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute());
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Bonus.Of(EVeyraDamageType::Magic), Expected, 1e-3), FString::SanitizeFloat(Bonus.Of(EVeyraDamageType::Magic))));

			// Consuming does not prime again, and a prime lapses after its window.
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(AreEqual(1, Procs()));
			RunFor(Window + WorldStep);
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(AreEqual(1, Procs(), TEXT("the lapsed prime is gone; this one primes afresh")));
			Hit(EVeyraDamageType::TrueDamage, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(AreEqual(2, Procs()));
		}

		/** Proc damage the holder dealt Unit. */
		double ProcDamageTo(const UAbilitySystemComponent& Unit, EVeyraDamageType Type) const
		{
			double Sum = 0.0;
			for (const FVeyraDamageDealtEvent& Event : Dealt)
			{
				Sum += Event.Delivery == EVeyraDamageDelivery::Proc && Event.Target.Get() == &Unit ? Event.Of(Type) : 0.0;
			}
			return Sum;
		}

		TEST_METHOD(EndlessCleaveSplashesTheAttacksBaseDamageAroundItsTarget)
		{
			// Fixture values: 40% from melee, 20% from range, 300 around the target; an attack of all its
			// Physical Power. The splashed have no Armor, so the share lands whole.
			constexpr double Melee = 0.4;
			constexpr double Ranged = 0.2;
			FVeyraEndlessCleaveTuning& Cleave = Tuning.EndlessCleave.Add(ItemId(TEXT("test_endless")));
			Cleave.MeleeFraction = Melee;
			Cleave.RangedFraction = Ranged;
			Cleave.Radius = 300.0;
			Hold(TEXT("test_endless"));
			FVeyraBasicAttackProfile Profile;
			Profile.Range = 150.0;
			Profile.PhysicalPowerRatio = 1.0;
			Profile.WindupFraction = 0.3;
			Profile.AcquisitionRadius = 400.0;
			UVeyraBasicAttackComponent* Attacks = Participant->FindComponentByClass<UVeyraBasicAttackComponent>();
			ASSERT_THAT(IsTrue(Attacks && Attacks->SetProfile(Profile)));

			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			UAbilitySystemComponent& Near = *World.Spawn(EVeyraTeam::B, FVector(350.0, 0.0, 0.0)).GetAbilitySystemComponent();
			UAbilitySystemComponent& Far = *World.Spawn(EVeyraTeam::B, FVector(900.0, 0.0, 0.0)).GetAbilitySystemComponent();
			UAbilitySystemComponent& Ally = *World.Spawn(EVeyraTeam::A, FVector(250.0, 100.0, 0.0)).GetAbilitySystemComponent();
			UAbilitySystemComponent& Tower = *World.SpawnStructure(EVeyraTeam::B, FVector(200.0, 200.0, 0.0)).GetAbilitySystemComponent();
			Near.SetNumericAttributeBase(UVeyraDefenceSet::GetArmorAttribute(), 0.0f);
			const double Power = Holder().GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPowerAttribute());

			Hit(EVeyraDamageType::Physical, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(IsTrue(ProcDamageTo(Near, EVeyraDamageType::Physical) == 0.0, TEXT("an ability does not cleave")));
			Hit(EVeyraDamageType::Physical, Blow, EVeyraDamageDelivery::BasicAttack);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ProcDamageTo(Near, EVeyraDamageType::Physical), Melee * Power, 1e-3),
				FString::SanitizeFloat(ProcDamageTo(Near, EVeyraDamageType::Physical))));
			ASSERT_THAT(IsTrue(ProcDamageTo(*Enemy, EVeyraDamageType::Physical) == 0.0 && ProcDamageTo(Far, EVeyraDamageType::Physical) == 0.0,
				TEXT("not the target itself, nor one beyond the radius")));
			ASSERT_THAT(IsFalse(Dealt.ContainsByPredicate([&Ally, &Tower](const FVeyraDamageDealtEvent& Event) {
				return Event.Target.Get() == &Ally || Event.Target.Get() == &Tower;
			}), TEXT("never an ally, never a structure")));

			// A ranged holder splashes its smaller share.
			FVeyraAttackProjectileTuning& Projectile = Profile.Projectile.AddDefaulted_GetRef();
			Projectile.Speed = 1200.0;
			Projectile.Radius = 10.0;
			ASSERT_THAT(IsTrue(Attacks->SetProfile(Profile)));
			Hit(EVeyraDamageType::Physical, Blow, EVeyraDamageDelivery::BasicAttack);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ProcDamageTo(Near, EVeyraDamageType::Physical), (Melee + Ranged) * Power, 1e-3)));
		}

		TEST_METHOD(TemperedByConflictChargesNearAnEnemyAndItsHitGrowsTheItem)
		{
			// Fixture values: 3 s within 700; 50 and a tenth of Max Health; a fifth of it kept; 30 s per enemy.
			constexpr double Charge = 3.0;
			constexpr double Base = 50.0;
			constexpr double Share = 0.1;
			constexpr double Kept = 0.2;
			FVeyraTemperedByConflictTuning& Tempered = Tuning.TemperedByConflict.Add(ItemId(TEXT("test_tempered")));
			Tempered.Radius = 700.0;
			Tempered.ChargeSeconds = Charge;
			Tempered.CheckSeconds = 0.25;
			Tempered.BaseDamage = Base;
			Tempered.MaxHealthFraction = Share;
			Tempered.HealthGainFraction = Kept;
			Tempered.CooldownSeconds = 30.0;
			Hold(TEXT("test_tempered"));
			Enemy->SetNumericAttributeBase(UVeyraDefenceSet::GetArmorAttribute(), 0.0f);
			UVeyraAttunementSubsystem& Attunements = *Spawner.GetWorld().GetSubsystem<UVeyraAttunementSubsystem>();

			Attunements.UpdateTempering();
			ASSERT_THAT(IsFalse(Attunements.IsTempered(Holder(), *Enemy), TEXT("not yet")));
			RunFor(Charge + WorldStep);
			Attunements.UpdateTempering();
			ASSERT_THAT(IsTrue(Attunements.IsTempered(Holder(), *Enemy), TEXT("after staying near")));

			Hit(EVeyraDamageType::Physical, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(AreEqual(0, Procs(), TEXT("an ability does not consume it")));
			const double MaxHealth = Holder().GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
			const double Bonus = Base + Share * MaxHealth;
			Hit(EVeyraDamageType::Physical, Blow, EVeyraDamageDelivery::BasicAttack);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ProcDamageTo(*Enemy, EVeyraDamageType::Physical), Bonus, 1e-3), TEXT("the basic attack does")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Holder().GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()), MaxHealth + Kept * Bonus, 1e-3),
				TEXT("and a share becomes Max Health")));
			ASSERT_THAT(IsFalse(Attunements.IsTempered(Holder(), *Enemy)));

			// That enemy's cooldown holds the next charge back.
			RunFor(Charge + WorldStep);
			Attunements.UpdateTempering();
			ASSERT_THAT(IsFalse(Attunements.IsTempered(Holder(), *Enemy), TEXT("its cooldown runs")));

			// Sold, the grown Max Health leaves with the item.
			const int32 Slot = Participant->FindComponentByClass<UVeyraInventoryComponent>()->GetSlots().IndexOfByPredicate(
				[](const FVeyraInventorySlot& Held) { return Held.Item == ItemId(TEXT("test_temper")); });
			ASSERT_THAT(IsTrue(Shop->Sell(*Participant, Slot) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Holder().GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()), MaxHealth, 1e-3)));
		}

		TEST_METHOD(FractureStacksOnMagicDamageUpToItsCap)
		{
			// Fixture values: 5% a stack, three at most.
			constexpr double PerStack = 0.05;
			constexpr int32 MaxStacks = 3;
			FVeyraStackingAttunementTuning& Fracture = Tuning.Fracture.Add(ItemId(TEXT("test_fracture")));
			Fracture.PerStack = PerStack;
			Fracture.MaxStacks = MaxStacks;
			Fracture.DurationSeconds = 4.0;
			Hold(TEXT("test_fracture"));
			const auto Retained = [this] { return Enemy->GetNumericAttribute(UVeyraDefenceSet::GetMagicResistReductionRetainedAttribute()); };

			Hit(EVeyraDamageType::Physical, Blow, EVeyraDamageDelivery::BasicAttack);
			ASSERT_THAT(IsTrue(Retained() == 1.0, TEXT("physical damage does not")));
			Hit(EVeyraDamageType::Magic, Blow, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Retained(), 1.0 - PerStack, 1e-5)));
			for (int32 More = 0; More < MaxStacks; ++More)
			{
				Hit(EVeyraDamageType::Magic, Blow, EVeyraDamageDelivery::BasicAttack);
			}
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Retained(), 1.0 - PerStack * MaxStacks, 1e-5), TEXT("any magic damage adds one, up to the cap")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
