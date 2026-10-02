// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemComponent.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Movement/VeyraMovementComponent.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Abilities/VeyraTestFluxborn.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	namespace StasisFixture
	{
		// Fixture values.
		constexpr double LongSeconds = 60.0;
		constexpr double Hit = 40.0;
		constexpr double Mend = 25.0;
		constexpr double Shove = 300.0;
		constexpr double ShoveSpeed = 1000.0;

		FVeyraStatusSpec Spec(const TCHAR* Id, EVeyraStatusKind Kind, double Magnitude = 0.0)
		{
			FVeyraStatusSpec Status;
			Status.Id = FVeyraContentId::FromText(Id).GetValue();
			Status.Kind = Kind;
			Status.Magnitude = Magnitude;
			Status.DurationSeconds = LongSeconds;
			return Status;
		}

		FVeyraRawDamageEvent TrueHit(EVeyraDamageDelivery Delivery = EVeyraDamageDelivery::Ability)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Hit });
			Damage.Delivery = Delivery;
			return Damage;
		}

		UVeyraStatusComponent& StatusesOf(const UAbilitySystemComponent& Unit)
		{
			return *Unit.GetOwner()->FindComponentByClass<UVeyraStatusComponent>();
		}
	}

	// Veyra.Combat.Stasis.*: a unit in Stasis takes no action, cannot be targeted or damaged, and nothing new
	// lands on it but its own side's statuses; only its effect ends it (Combat Bible §10; ADR-050 §1).
	TEST_CLASS(Stasis, "Veyra.Combat")
	{
		FActorTestSpawner Spawner;

		UAbilitySystemComponent& Unit(EVeyraTeam Team, double X)
		{
			AVeyraTestFluxborn& Body = Spawner.SpawnActorAt<AVeyraTestFluxborn>(FVector(X, 0.0, 0.0), FRotator::ZeroRotator);
			Body.SetVeyraTeam(Team);
			Body.GetVeyraMovement()->SetMovementMode(MOVE_Walking);
			UAbilitySystemComponent& AbilitySystem = *Body.GetAbilitySystemComponent();
			VeyraCombat::InitializeStats(AbilitySystem, ExampleStats());
			return AbilitySystem;
		}

		static FVeyraStatusSpec StasisSpec()
		{
			return StasisFixture::Spec(TEXT("test_stasis"), EVeyraStatusKind::Stasis);
		}

		TEST_METHOD(ItBlocksEveryActionAndEntryInterrupts)
		{
			UAbilitySystemComponent& Held = Unit(EVeyraTeam::A, 0.0);
			int32 Interruptions = 0;
			StasisFixture::StatusesOf(Held).OnInterrupted.AddLambda([&Interruptions] { ++Interruptions; });
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Held, Held, StasisSpec())));
			const EVeyraActionBlocks Blocks = VeyraCombat::GetActionBlocks(Held);
			ASSERT_THAT(IsTrue(EnumHasAllFlags(Blocks, EVeyraActionBlocks::Move | EVeyraActionBlocks::Attack | EVeyraActionBlocks::Cast)));
			ASSERT_THAT(IsTrue(Interruptions == 1, TEXT("a cast under way ends as it enters Stasis")));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(Held, StasisSpec().Id)));
			ASSERT_THAT(IsTrue(VeyraCombat::GetActionBlocks(Held) == EVeyraActionBlocks::None));
		}

		TEST_METHOD(ItIsUntargetableAndTakesNoDamageNotEvenTrueDamage)
		{
			UAbilitySystemComponent& Held = Unit(EVeyraTeam::A, 0.0);
			UAbilitySystemComponent& Enemy = Unit(EVeyraTeam::B, 300.0);
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Held, Held, StasisSpec())));
			ASSERT_THAT(IsTrue(VeyraTargeting::IsUntargetable(*Held.GetOwner())));
			ASSERT_THAT(IsFalse(VeyraTargeting::CanHitEnemy(Enemy.GetOwner(), *Held.GetOwner())));
			ASSERT_THAT(IsTrue(VeyraCombat::IsInvulnerable(Held)));
			for (const EVeyraDamageDelivery Delivery : { EVeyraDamageDelivery::Ability, EVeyraDamageDelivery::BasicAttack, EVeyraDamageDelivery::Periodic })
			{
				VeyraCombat::DealDamage(Enemy, Held, StasisFixture::TrueHit(Delivery));
			}
			ASSERT_THAT(IsTrue(VeyraCombat::GetMissingHealth(Held) == 0.0, TEXT("an effect over time ticks for 0, as every hit lands for 0")));

			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(Held, StasisSpec().Id)));
			ASSERT_THAT(IsFalse(VeyraTargeting::IsUntargetable(*Held.GetOwner())));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Enemy, Held, StasisFixture::TrueHit())));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCombat::GetMissingHealth(Held), StasisFixture::Hit), TEXT("out of Stasis, damage lands again")));
		}

		TEST_METHOD(ItKeepsWhatItHeldAndRefusesWhatAnEnemyAdds)
		{
			UAbilitySystemComponent& Held = Unit(EVeyraTeam::A, 0.0);
			UAbilitySystemComponent& Ally = Unit(EVeyraTeam::A, 200.0);
			UAbilitySystemComponent& Enemy = Unit(EVeyraTeam::B, 300.0);
			const FVeyraStatusSpec Weaken = StasisFixture::Spec(TEXT("test_weaken"), EVeyraStatusKind::Weaken, 0.2);
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Enemy, Held, Weaken)));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Held, Held, StasisSpec())));
			ASSERT_THAT(IsTrue(StasisFixture::StatusesOf(Held).Has(EVeyraStatusKind::Weaken), TEXT("what it held stays, its time running")));

			ASSERT_THAT(IsFalse(VeyraCombat::ApplyStatus(Enemy, Held, StasisFixture::Spec(TEXT("test_stun"), EVeyraStatusKind::Stun))));
			ASSERT_THAT(IsFalse(VeyraCombat::ApplyStatus(Enemy, Held, StasisFixture::Spec(TEXT("test_mark"), EVeyraStatusKind::Counter)),
				TEXT("an enemy's status of any kind")));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Ally, Held, StasisFixture::Spec(TEXT("test_haste"), EVeyraStatusKind::MoveSpeed, 0.1)),
				TEXT("its own side's statuses still land")));
		}

		TEST_METHOD(NothingDisplacesHealsOrShieldsIt)
		{
			UAbilitySystemComponent& Held = Unit(EVeyraTeam::A, 0.0);
			UAbilitySystemComponent& Ally = Unit(EVeyraTeam::A, 200.0);
			UAbilitySystemComponent& Enemy = Unit(EVeyraTeam::B, 300.0);
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Enemy, Held, StasisFixture::TrueHit())));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Held, Held, StasisSpec())));

			const FVeyraDisplacement Shove{ FVector::ForwardVector, StasisFixture::Shove, StasisFixture::ShoveSpeed };
			ASSERT_THAT(IsFalse(VeyraCombat::Displace(Enemy, Held, Shove)));
			ASSERT_THAT(IsFalse(VeyraCombat::Displace(Ally, Held, Shove), TEXT("nor an ally's displacement")));
			ASSERT_THAT(IsTrue(VeyraCombat::RestoreHealthFrom(Ally, Held, StasisFixture::Mend) == 0.0));
			ASSERT_THAT(IsFalse(VeyraCombat::GrantShield(Ally, Held, EVeyraShieldCategory::Universal, StasisFixture::Mend, StasisFixture::LongSeconds).IsValid()));
			ASSERT_THAT(IsFalse(VeyraCombat::GrantTemporaryHealth(Ally, Held, StasisFixture::Mend, StasisFixture::LongSeconds).IsValid()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCombat::GetMissingHealth(Held), StasisFixture::Hit)));
			// Health Regeneration is no new heal: it goes on in Stasis (ADR-050 §1).
			ASSERT_THAT(IsTrue(VeyraCombat::RegenerateHealth(Held, StasisFixture::Mend / 5.0)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCombat::GetMissingHealth(Held), StasisFixture::Hit - StasisFixture::Mend / 5.0)));

			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(Held, StasisSpec().Id)));
			ASSERT_THAT(IsTrue(VeyraCombat::RestoreHealthFrom(Ally, Held, StasisFixture::Mend) > 0.0, TEXT("out of Stasis, heals land again")));
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(Enemy, Held, Shove), TEXT("and displacement")));
		}

		TEST_METHOD(ItIsNotCrowdControlAndHasNoMagnitude)
		{
			ASSERT_THAT(IsFalse(VeyraStatuses::IsTenacityReducible(EVeyraStatusKind::Stasis)));
			ASSERT_THAT(IsFalse(VeyraStatuses::IsCrowdControl(EVeyraStatusKind::Stasis)));
			ASSERT_THAT(IsTrue(VeyraStatuses::Validate(StasisSpec()).IsEmpty()));
			FVeyraStatusSpec Strong = StasisSpec();
			Strong.Magnitude = 1.0;
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(Strong).IsEmpty()));
		}
	};

	// Veyra.Combat.SealedHealth.*: a sealed Health is a meter its owner sets, as an Echo's Integrity (ADR-050 §3).
	TEST_CLASS(SealedHealth, "Veyra.Combat")
	{
		FActorTestSpawner Spawner;
		int32 HostileHits = 0;

		BEFORE_EACH()
		{
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnHostileDamage.AddLambda(
				[this](const FVeyraHostileDamageEvent&) { ++HostileHits; });
		}

		UAbilitySystemComponent& Unit(EVeyraTeam Team, double X)
		{
			AVeyraTestFluxborn& Body = Spawner.SpawnActorAt<AVeyraTestFluxborn>(FVector(X, 0.0, 0.0), FRotator::ZeroRotator);
			Body.SetVeyraTeam(Team);
			UAbilitySystemComponent& AbilitySystem = *Body.GetAbilitySystemComponent();
			VeyraCombat::InitializeStats(AbilitySystem, ExampleStats());
			return AbilitySystem;
		}

		TEST_METHOD(DamageIsAnnouncedButTakesNothing)
		{
			UAbilitySystemComponent& Sealed = Unit(EVeyraTeam::A, 0.0);
			UAbilitySystemComponent& Enemy = Unit(EVeyraTeam::B, 300.0);
			VeyraCombat::SealHealth(Sealed);
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Enemy, Sealed, StasisFixture::TrueHit(EVeyraDamageDelivery::BasicAttack))));
			ASSERT_THAT(IsTrue(VeyraCombat::GetMissingHealth(Sealed) == 0.0));
			ASSERT_THAT(IsTrue(HostileHits == 1, TEXT("its owner reads each hit")));
			ASSERT_THAT(IsFalse(VeyraTargeting::IsUntargetable(*Sealed.GetOwner()), TEXT("a sealed unit may still be attacked")));
		}

		TEST_METHOD(OnlyItsOwnerMovesIt)
		{
			UAbilitySystemComponent& Sealed = Unit(EVeyraTeam::A, 0.0);
			UAbilitySystemComponent& Ally = Unit(EVeyraTeam::A, 200.0);
			const double Full = ExampleStats().MaxHealth;
			TestRunner->AddExpectedMessagePlain(TEXT("it needs sealed Health"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			ASSERT_THAT(IsFalse(VeyraCombat::SetSealedHealth(Sealed, Full / 2.0), TEXT("only a sealed Health is set")));
			VeyraCombat::SealHealth(Sealed);
			ASSERT_THAT(IsTrue(VeyraCombat::SetSealedHealth(Sealed, Full / 2.0)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCombat::GetMissingHealth(Sealed), Full / 2.0)));
			ASSERT_THAT(IsTrue(VeyraCombat::RestoreHealthFrom(Ally, Sealed, StasisFixture::Mend) == 0.0));
			ASSERT_THAT(IsFalse(VeyraCombat::RestoreHealth(Sealed, StasisFixture::Mend), TEXT("nor any restoration")));
			ASSERT_THAT(IsFalse(VeyraCombat::RegenerateHealth(Sealed, StasisFixture::Mend), TEXT("nor regeneration")));
			ASSERT_THAT(IsFalse(VeyraCombat::GrantShield(Ally, Sealed, EVeyraShieldCategory::Universal, StasisFixture::Mend, StasisFixture::LongSeconds).IsValid()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCombat::GetMissingHealth(Sealed), Full / 2.0)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
