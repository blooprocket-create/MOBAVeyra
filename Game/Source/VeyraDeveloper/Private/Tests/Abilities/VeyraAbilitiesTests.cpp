// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Components/ActorTestSpawner.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "CQTest.h"
#include "Engine/Engine.h"
#include "Life/VeyraLifeComponent.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Tests/Abilities/VeyraTestClockGameState.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesVerbs.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	FVeyraContentId Id(const TCHAR* Text)
	{
		return FVeyraContentId::FromText(Text).GetValue();
	}

	// Veyra.Abilities.CooldownLedger.*: the cooldown ledger's arithmetic (ADR-006 §4 amendment).
	TEST_CLASS(CooldownLedger, "Veyra.Abilities")
	{
		static constexpr double Start = 10.0;
		static constexpr double Duration = 5.0;

		TEST_METHOD(AnAbilityWithNoCooldownIsReady)
		{
			TArray<FVeyraCooldownEntry> Entries;
			ASSERT_THAT(IsTrue(VeyraCooldowns::RemainingSeconds(Entries, Id(TEXT("test_bolt")), Start) == 0.0));
		}

		TEST_METHOD(CountsDownInGameplayTime)
		{
			TArray<FVeyraCooldownEntry> Entries;
			VeyraCooldowns::Start(Entries, Id(TEXT("test_bolt")), Duration, Start);
			ASSERT_THAT(IsTrue(VeyraCooldowns::RemainingSeconds(Entries, Id(TEXT("test_bolt")), Start) == Duration));
			ASSERT_THAT(IsTrue(VeyraCooldowns::RemainingSeconds(Entries, Id(TEXT("test_bolt")), Start + 2.0) == Duration - 2.0));
			ASSERT_THAT(IsTrue(VeyraCooldowns::RemainingSeconds(Entries, Id(TEXT("test_bolt")), Start + Duration) == 0.0));
			ASSERT_THAT(IsTrue(VeyraCooldowns::RemainingSeconds(Entries, Id(TEXT("test_bolt")), Start + 2.0 * Duration) == 0.0));
		}

		TEST_METHOD(RestartingReplacesTheCooldown)
		{
			TArray<FVeyraCooldownEntry> Entries;
			VeyraCooldowns::Start(Entries, Id(TEXT("test_bolt")), Duration, Start);
			VeyraCooldowns::Start(Entries, Id(TEXT("test_bolt")), Duration / 2.0, Start);
			ASSERT_THAT(AreEqual(Entries.Num(), 1));
			ASSERT_THAT(IsTrue(VeyraCooldowns::RemainingSeconds(Entries, Id(TEXT("test_bolt")), Start) == Duration / 2.0));
		}

		TEST_METHOD(AbilitiesCoolDownIndependently)
		{
			TArray<FVeyraCooldownEntry> Entries;
			VeyraCooldowns::Start(Entries, Id(TEXT("first")), Duration, Start);
			ASSERT_THAT(IsTrue(VeyraCooldowns::RemainingSeconds(Entries, Id(TEXT("second")), Start) == 0.0));
		}
	};

	// Veyra.Abilities.CooldownClock.*: the ledger counts in the server's world time. A client, whose
	// world clock started when it loaded the map, reads it through its game state's estimate of the
	// server's clock, so a late joiner sees the true remaining time.
	TEST_CLASS(CooldownClock, "Veyra.Abilities")
	{
		static constexpr double Duration = 5.0;
		// How far the server's clock runs ahead of a client that loaded the map later.
		static constexpr float ServerClockAhead = 2.0f;

		FActorTestSpawner Spawner;

		TEST_METHOD(RemainingTimeFollowsTheServerClock)
		{
			AVeyraTestClockGameState& GameState = Spawner.SpawnActor<AVeyraTestClockGameState>();
			UVeyraCooldownComponent& Cooldowns = *Spawner.SpawnActor<AVeyraPlayerState>().FindComponentByClass<UVeyraCooldownComponent>();
			Cooldowns.StartCooldown(Id(TEXT("test_bolt")), Duration);
			ASSERT_THAT(IsTrue(Cooldowns.GetRemainingSecondsNow(Id(TEXT("test_bolt"))) == Duration));

			// The same ledger, read where the server's clock is ahead of the local world clock.
			GameState.SetServerClockOffset(ServerClockAhead);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Cooldowns.GetRemainingSecondsNow(Id(TEXT("test_bolt"))), Duration - ServerClockAhead)));
		}
	};

	// Veyra.Abilities.TargetedCasting.*: a targeted damage ability validates its cast, commits its
	// cost and cooldown and deals its damage (Combat Bible §26, §27, §29, §30).
	TEST_CLASS(TargetedCasting, "Veyra.Abilities")
	{
		// Fixture values, independent of the committed Abilities.json.
		static constexpr double CastRange = 500.0;
		static constexpr double CooldownSeconds = 5.0;
		static constexpr double ResourceCost = 10.0;
		static constexpr double DamageAmount = 30.0;
		static constexpr double StartingMaxHealth = 100.0;
		static constexpr double StartingMaxResource = 25.0;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;

		BEFORE_EACH()
		{
			FVeyraTargetedDamageAbilityTuning Bolt;
			Bolt.CastRange = CastRange;
			Bolt.CooldownSeconds = CooldownSeconds;
			Bolt.ResourceCost = ResourceCost;
			Bolt.DamageType = EVeyraDamageType::TrueDamage;
			Bolt.DamageAmount = DamageAmount;
			Tuning.TargetedDamage.Add(Id(TEXT("test_bolt")), Bolt);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			Caster = &SpawnVanguard(EVeyraTeam::A, FVector::ZeroVector);
			Enemy = &SpawnVanguard(EVeyraTeam::B, FVector(CastRange / 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(LoadoutOf(*Caster).Grant(AbilitiesOf(*Caster), EVeyraAbilitySlot::Q, Id(TEXT("test_bolt")))));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		AVeyraVanguardCharacter& SpawnVanguard(EVeyraTeam Team, const FVector& Location)
		{
			AVeyraPlayerState& PlayerState = Spawner.SpawnActor<AVeyraPlayerState>();
			PlayerState.SetVeyraTeam(Team);
			VeyraCombat::InitializeVitals(*PlayerState.GetAbilitySystemComponent(), StartingMaxHealth);
			VeyraCombat::InitializeResource(*PlayerState.GetAbilitySystemComponent(), StartingMaxResource);
			AVeyraVanguardCharacter& Vanguard = Spawner.SpawnActorAt<AVeyraVanguardCharacter>(Location, FRotator::ZeroRotator);
			Vanguard.SetPlayerState(&PlayerState);
			return Vanguard;
		}

		static UAbilitySystemComponent& AbilitiesOf(const AVeyraVanguardCharacter& Vanguard)
		{
			return *Vanguard.GetAbilitySystemComponent();
		}

		static UVeyraAbilityLoadoutComponent& LoadoutOf(const AVeyraVanguardCharacter& Vanguard)
		{
			return *Vanguard.GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
		}

		EVeyraCastRejection CastAt(AActor* Target, EVeyraAbilitySlot Slot = EVeyraAbilitySlot::Q)
		{
			FVeyraCastTarget CastTarget;
			CastTarget.Actor = Target;
			return VeyraAbilities::TryCast(AbilitiesOf(*Caster), Slot, CastTarget);
		}

		static double Attribute(const AVeyraVanguardCharacter& Vanguard, const FGameplayAttribute& Which)
		{
			return AbilitiesOf(Vanguard).GetNumericAttribute(Which);
		}

		TEST_METHOD(AValidCastCommitsAndDealsItsDamage)
		{
			ASSERT_THAT(IsTrue(CastAt(Enemy) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Attribute(*Enemy, UVeyraVitalsSet::GetHealthAttribute()) == StartingMaxHealth - DamageAmount));
			ASSERT_THAT(IsTrue(Attribute(*Caster, UVeyraResourceSet::GetResourceAttribute()) == StartingMaxResource - ResourceCost));
			const UVeyraCooldownComponent* Cooldowns = Caster->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>();
			ASSERT_THAT(IsTrue(Cooldowns->GetRemainingSecondsNow(Id(TEXT("test_bolt"))) == CooldownSeconds));
		}

		TEST_METHOD(ARecastDuringTheCooldownIsRefused)
		{
			ASSERT_THAT(IsTrue(CastAt(Enemy) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(CastAt(Enemy) == EVeyraCastRejection::OnCooldown));
			ASSERT_THAT(IsTrue(Attribute(*Enemy, UVeyraVitalsSet::GetHealthAttribute()) == StartingMaxHealth - DamageAmount));
		}

		TEST_METHOD(RefusedCastsPayNothing)
		{
			AVeyraVanguardCharacter& Ally = SpawnVanguard(EVeyraTeam::A, FVector(0.0, CastRange / 2.0, 0.0));
			AVeyraVanguardCharacter& FarEnemy = SpawnVanguard(EVeyraTeam::B, FVector(CastRange * 4.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(CastAt(&Ally) == EVeyraCastRejection::NotHostile));
			ASSERT_THAT(IsTrue(CastAt(Caster) == EVeyraCastRejection::InvalidTarget));
			ASSERT_THAT(IsTrue(CastAt(nullptr) == EVeyraCastRejection::InvalidTarget));
			ASSERT_THAT(IsTrue(CastAt(&FarEnemy) == EVeyraCastRejection::OutOfRange));
			ASSERT_THAT(IsTrue(CastAt(Enemy, EVeyraAbilitySlot::W) == EVeyraCastRejection::UnknownAbility));
			ASSERT_THAT(IsTrue(Attribute(*Caster, UVeyraResourceSet::GetResourceAttribute()) == StartingMaxResource));
			const UVeyraCooldownComponent* Cooldowns = Caster->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>();
			ASSERT_THAT(IsTrue(Cooldowns->GetRemainingSecondsNow(Id(TEXT("test_bolt"))) == 0.0));
		}

		TEST_METHOD(ACastNeedsItsCost)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::SpendResource(AbilitiesOf(*Caster), StartingMaxResource - ResourceCost / 2.0)));
			ASSERT_THAT(IsTrue(CastAt(Enemy) == EVeyraCastRejection::InsufficientResource));
		}

		TEST_METHOD(TheDeadCannotCastOrBeTargeted)
		{
			Enemy->GetPlayerState()->FindComponentByClass<UVeyraLifeComponent>()->SetState(EVeyraLifeState::Dead);
			ASSERT_THAT(IsTrue(CastAt(Enemy) == EVeyraCastRejection::TargetDead));
			Caster->GetPlayerState()->FindComponentByClass<UVeyraLifeComponent>()->SetState(EVeyraLifeState::Dead);
			ASSERT_THAT(IsTrue(CastAt(Enemy) == EVeyraCastRejection::CasterDead));
		}

		TEST_METHOD(GrantingUnknownContentIsRefused)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("defines no ability with that ID"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			ASSERT_THAT(IsFalse(LoadoutOf(*Caster).Grant(AbilitiesOf(*Caster), EVeyraAbilitySlot::W, Id(TEXT("no_such_ability")))));
		}
	};

	// Veyra.Abilities.AbilitiesTuning.*: the committed Abilities.json loads.
	TEST_CLASS(AbilitiesTuning, "Veyra.Abilities")
	{
		TEST_METHOD(CommittedFileLoads)
		{
			ASSERT_THAT(IsTrue(GEngine->GetEngineSubsystem<UVeyraAbilitiesTuningSubsystem>()->IsLoaded()));
			ASSERT_THAT(IsTrue(UVeyraAbilitiesTuningSubsystem::Defines(Id(TEXT("test_bolt")))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
